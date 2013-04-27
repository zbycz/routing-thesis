/***************************************
 A simple o5m/o5c parser.

 Part of the Routino routing software.
 ******************/ /******************
 This file Copyright 2012-2013 Andrew M. Bishop

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ***************************************/


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>

#include "osmparser.h"
#include "tagging.h"
#include "logging.h"


/* At the top level */

#define O5M_FILE_NODE           0x10
#define O5M_FILE_WAY            0x11
#define O5M_FILE_RELATION       0x12
#define O5M_FILE_BOUNDING_BOX   0xdb
#define O5M_FILE_TIMESTAMP      0xdc
#define O5M_FILE_HEADER         0xe0
#define O5M_FILE_SYNC           0xee
#define O5M_FILE_JUMP           0xef
#define O5M_FILE_END            0xfe
#define O5M_FILE_RESET          0xff

/* Errors */

#define O5M_EOF                     0

#define O5M_ERROR_UNEXP_EOF        100
#define O5M_ERROR_RESET_NOT_FIRST  101
#define O5M_ERROR_HEADER_NOT_FIRST 102
#define O5M_ERROR_EXPECTED_O5M     103
#define O5M_ERROR_EXPECTED_O5C     104
#define O5M_ERROR_FILE_LEVEL       105



/* Parsing variables and functions */

static uint64_t byteno=0;
static uint64_t nnodes=0,nways=0,nrelations=0;

static int64_t id=0;
static int32_t lat=0;
static int32_t lon=0;
static int64_t timestamp=0;
static int64_t node_refid=0,way_refid=0,relation_refid=0;

static int mode_change=MODE_NORMAL;

static int buffer_allocated;
static unsigned char *buffer=NULL;
static unsigned char *buffer_ptr,*buffer_end;

static int string_table_start=0;
static unsigned char **string_table=NULL;

#define STRING_TABLE_ALLOCATED 15000


/*++++++++++++++++++++++++++++++++++++++
  Refill the data buffer and set the pointers.

  int buffer_refill Return 0 if everything is OK or 1 for EOF.

  int fd The file descriptor to read from.

  uint32_t bytes The number of bytes to read.
  ++++++++++++++++++++++++++++++++++++++*/

static inline int buffer_refill(int fd,uint32_t bytes)
{
 ssize_t n,m;
 uint32_t totalbytes;

 m=buffer_end-buffer_ptr;

 if(m)
    memmove(buffer,buffer_ptr,m);

 totalbytes=bytes+m;

 if(totalbytes>buffer_allocated)
    buffer=(unsigned char *)realloc(buffer,buffer_allocated=totalbytes);

 byteno+=bytes;

 buffer_ptr=buffer;
 buffer_end=buffer+m;

 do
   {
    n=read(fd,buffer_end,bytes);

    if(n<=0)
       return(1);

    buffer_end+=n;
    bytes-=n;
   }
 while(bytes>0);

 return(0);
}

static void process_node(void);
static void process_way(void);
static void process_relation(void);
static void process_info(void);
static unsigned char *process_string(int pair,unsigned char **buf_ptr,unsigned char **string1,unsigned char **string2);


/* Macros to simplify the parser (and make it look more like the XML parser) */

#define BEGIN(xx)            do{ state=(xx); goto finish_parsing; } while(0)

#define BUFFER_CHARS(xx)     do{ if(buffer_refill(fd,(xx))) BEGIN(O5M_ERROR_UNEXP_EOF); } while(0)


/* O5M decoding */

#define O5M_LATITUDE(xx)  (double)(1E-7*(xx))
#define O5M_LONGITUDE(xx) (double)(1E-7*(xx))


/*++++++++++++++++++++++++++++++++++++++
  Parse an O5M int32 data value.

  uint32_t o5m_int32 Returns the integer value.

  unsigned char **ptr The pointer to read the data from.
  ++++++++++++++++++++++++++++++++++++++*/

static inline uint32_t o5m_int32(unsigned char **ptr)
{
 uint32_t result=(**ptr)&0x7F;

 if((**ptr)&0x80) result+=((*++(*ptr))&0x7F)<<7;
 if((**ptr)&0x80) result+=((*++(*ptr))&0x7F)<<14;
 if((**ptr)&0x80) result+=((*++(*ptr))&0x7F)<<21;
 if((**ptr)&0x80) result+=((*++(*ptr))&0x7F)<<28;

 (*ptr)++;

 return(result);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse an O5M int32 data value.

  int32_t o5m_sint32 Returns the integer value.

  unsigned char **ptr The pointer to read the data from.
  ++++++++++++++++++++++++++++++++++++++*/

static inline int32_t o5m_sint32(unsigned char **ptr)
{
 int64_t result=((**ptr)&0x7E)>>1;
 int sign=(**ptr)&0x01;

 if((**ptr)&0x80) result+=(int64_t)((*++(*ptr))&0x7F)<<6;
 if((**ptr)&0x80) result+=(int64_t)((*++(*ptr))&0x7F)<<13;
 if((**ptr)&0x80) result+=(int64_t)((*++(*ptr))&0x7F)<<20;
 if((**ptr)&0x80) result+=(int64_t)((*++(*ptr))&0x7F)<<27;

 (*ptr)++;

 if(sign)
    result=-result-1;

 return(result);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse an O5M int64 data value.

  int64_t o5m_int64 Returns the integer value.

  unsigned char **ptr The pointer to read the data from.
  ++++++++++++++++++++++++++++++++++++++*/

static inline int64_t o5m_int64(unsigned char **ptr)
{
 uint64_t result=(**ptr)&0x7F;

 if((**ptr)&0x80) result+=(uint64_t)((*++(*ptr))&0x7F)<<7;
 if((**ptr)&0x80) result+=(uint64_t)((*++(*ptr))&0x7F)<<14;
 if((**ptr)&0x80) result+=(uint64_t)((*++(*ptr))&0x7F)<<21;
 if((**ptr)&0x80) result+=(uint64_t)((*++(*ptr))&0x7F)<<28;
 if((**ptr)&0x80) result+=(uint64_t)((*++(*ptr))&0x7F)<<35;
 if((**ptr)&0x80) result+=(uint64_t)((*++(*ptr))&0x7F)<<42;
 if((**ptr)&0x80) result+=(uint64_t)((*++(*ptr))&0x7F)<<49;
 if((**ptr)&0x80) result+=(uint64_t)((*++(*ptr))&0x7F)<<56;
 if((**ptr)&0x80) result+=(uint64_t)((*++(*ptr))&0x7F)<<63;

 (*ptr)++;

 return(result);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse an O5M sint64 data value.

  int64_t o5m_sint64 Returns the integer value.

  unsigned char **ptr The pointer to read the data from.
  ++++++++++++++++++++++++++++++++++++++*/

static inline int64_t o5m_sint64(unsigned char **ptr)
{
 int64_t result=((**ptr)&0x7E)>>1;
 int sign=(**ptr)&0x01;

 if((**ptr)&0x80) result+=(int64_t)((*++(*ptr))&0x7F)<<6;
 if((**ptr)&0x80) result+=(int64_t)((*++(*ptr))&0x7F)<<13;
 if((**ptr)&0x80) result+=(int64_t)((*++(*ptr))&0x7F)<<20;
 if((**ptr)&0x80) result+=(int64_t)((*++(*ptr))&0x7F)<<27;
 if((**ptr)&0x80) result+=(int64_t)((*++(*ptr))&0x7F)<<34;
 if((**ptr)&0x80) result+=(int64_t)((*++(*ptr))&0x7F)<<41;
 if((**ptr)&0x80) result+=(int64_t)((*++(*ptr))&0x7F)<<48;
 if((**ptr)&0x80) result+=(int64_t)((*++(*ptr))&0x7F)<<55;
 if((**ptr)&0x80) result+=(int64_t)((*++(*ptr))&0x7F)<<62;

 (*ptr)++;

 if(sign)
    result=-result-1;

 return(result);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse the O5M and call the functions for each OSM item as seen.

  int ParseO5M Returns 0 if OK or something else in case of an error.

  int fd The file descriptor of the file to parse.

  int changes Set to 1 if this is expected to be a changes file, otherwise zero.
  ++++++++++++++++++++++++++++++++++++++*/

int ParseO5M(int fd,int changes)
{
 int i;
 int state;
 int number_reset=0;
 int error;

 /* Print the initial message */

 nnodes=0,nways=0,nrelations=0;

 printf_first("Reading: Bytes=0 Nodes=0 Ways=0 Relations=0");

 /* The actual parser. */

 if(changes)
    mode_change=MODE_MODIFY;

 string_table_start=0;
 string_table=(unsigned char **)malloc(STRING_TABLE_ALLOCATED*sizeof(unsigned char *));
 for(i=0;i<STRING_TABLE_ALLOCATED;i++)
    string_table[i]=(unsigned char*)malloc(252);

 buffer_allocated=4096;
 buffer=(unsigned char*)malloc(buffer_allocated);

 buffer_ptr=buffer_end=buffer;

 while(1)
   {
    uint32_t dataset_length=0;

    /* ================ Parsing states ================ */

    BUFFER_CHARS(1);

    state=*buffer_ptr++;

    if(state!=O5M_FILE_END && state!=O5M_FILE_RESET)
      {
       uint32_t length;
       unsigned char *ptr;

       if(number_reset==0)
          BEGIN(O5M_ERROR_RESET_NOT_FIRST);

       BUFFER_CHARS(4);

       ptr=buffer_ptr;
       dataset_length=o5m_int32(&buffer_ptr);

       length=dataset_length-4+(buffer_ptr-ptr);

       BUFFER_CHARS(length);
      }
    else if(state==O5M_FILE_END)
       ;
    else if(state==O5M_FILE_RESET)
       number_reset++;

    switch(state)
      {
      case O5M_FILE_NODE:

       process_node();

       break;

      case O5M_FILE_WAY:

       process_way();

       break;

      case O5M_FILE_RELATION:

       process_relation();

       break;

      case O5M_FILE_BOUNDING_BOX:

       buffer_ptr+=dataset_length;

       break;

      case O5M_FILE_TIMESTAMP:

       buffer_ptr+=dataset_length;

       break;

      case O5M_FILE_HEADER:

       if(number_reset!=1)
          BEGIN(O5M_ERROR_HEADER_NOT_FIRST);

       if(!changes && strncmp((char*)buffer_ptr,"o5m2",4))
          BEGIN(O5M_ERROR_EXPECTED_O5M);

       if( changes && strncmp((char*)buffer_ptr,"o5c2",4))
          BEGIN(O5M_ERROR_EXPECTED_O5C);

       buffer_ptr+=dataset_length;

       break;

      case O5M_FILE_SYNC:

       buffer_ptr+=dataset_length;

       break;

      case O5M_FILE_JUMP:

       buffer_ptr+=dataset_length;

       break;

      case O5M_FILE_END:

       BEGIN(O5M_EOF);

       break;

      case O5M_FILE_RESET:

       string_table_start=0;
       id=0;
       lat=0;
       lon=0;
       timestamp=0;
       node_refid=0,way_refid=0,relation_refid=0;

       break;

      default:

       error=state;
       BEGIN(O5M_ERROR_FILE_LEVEL);
      }
   }


 finish_parsing:

 switch(state)
   {
    /* End of file */

   case O5M_EOF:
    break;


    /* ================ Error states ================ */


   case O5M_ERROR_UNEXP_EOF:
    fprintf(stderr,"O5M Parser: Error at byte %llu: unexpected end of file seen.\n",byteno);
    break;

   case O5M_ERROR_RESET_NOT_FIRST:
    fprintf(stderr,"O5M Parser: Error at byte %llu: Reset was not the first byte.\n",byteno);
    break;

   case O5M_ERROR_HEADER_NOT_FIRST:
    fprintf(stderr,"O5M Parser: Error at byte %llu: Header was not the first section.\n",byteno);
    break;

   case O5M_ERROR_EXPECTED_O5M:
    fprintf(stderr,"O5M Parser: Error at byte %llu: Expected O5M format but header disagrees.\n",byteno);
    break;

   case O5M_ERROR_EXPECTED_O5C:
    fprintf(stderr,"O5M Parser: Error at byte %llu: Expected O5C format but header disagrees.\n",byteno);
    break;

   case O5M_ERROR_FILE_LEVEL:
    fprintf(stderr,"O5M Parser: Error at byte %llu: Unexpected dataset type %02x.\n",byteno,error);
    break;
   }

 /* Free the parser variables */

 for(i=0;i<STRING_TABLE_ALLOCATED;i++)
    free(string_table[i]);
 free(string_table);

 free(buffer);

 /* Print the final message */

 printf_last("Read: Bytes=%"PRIu64" Nodes=%"PRIu64" Ways=%"PRIu64" Relations=%"PRIu64,byteno,nnodes,nways,nrelations);

 return(state);
}


/*++++++++++++++++++++++++++++++++++++++
  Process an O5M Node dataset.
  ++++++++++++++++++++++++++++++++++++++*/

static void process_node(void)
{
 int64_t delta_id;
 int32_t delta_lat;
 int32_t delta_lon;
 TagList *tags=NULL,*result=NULL;
 int mode=mode_change;

 delta_id=o5m_sint64(&buffer_ptr);
 id+=delta_id;

 if(buffer_ptr<buffer_end)
   {
    if(*buffer_ptr!=0)
       process_info();
    else
       buffer_ptr++;
   }

 if(buffer_ptr<buffer_end)
   {
    delta_lon=o5m_sint32(&buffer_ptr);
    lon+=delta_lon;

    delta_lat=o5m_sint32(&buffer_ptr);
    lat+=delta_lat;
   }
 else
    mode=MODE_DELETE;

 /* Mangle the data and send it to the OSM parser */

 nnodes++;

 if(!(nnodes%10000))
    printf_middle("Reading: Bytes=%"PRIu64" Nodes=%"PRIu64" Ways=%"PRIu64" Relations=%"PRIu64,byteno,nnodes,nways,nrelations);

 tags=NewTagList();

 while(buffer_ptr<buffer_end)
   {
    unsigned char *key,*val;

    process_string(2,&buffer_ptr,&key,&val);

    AppendTag(tags,(char*)key,(char*)val);
   }

 result=ApplyNodeTaggingRules(tags,id);

 ProcessNodeTags(result,id,O5M_LATITUDE(lat),O5M_LONGITUDE(lon),mode);

 DeleteTagList(tags);
 DeleteTagList(result);
}


/*++++++++++++++++++++++++++++++++++++++
  Process an O5M Way dataset.
  ++++++++++++++++++++++++++++++++++++++*/

static void process_way(void)
{
 int64_t delta_id;
 TagList *tags=NULL,*result=NULL;
 int mode=mode_change;
 unsigned char *refs=NULL,*refs_end;

 delta_id=o5m_sint64(&buffer_ptr);
 id+=delta_id;

 if(buffer_ptr<buffer_end)
   {
    if(*buffer_ptr!=0)
       process_info();
    else
       buffer_ptr++;
   }

 if(buffer_ptr<buffer_end)
   {
    uint32_t length;

    length=o5m_int32(&buffer_ptr);

    if(length)
      {
       refs=buffer_ptr;
       refs_end=buffer_ptr+length;

       buffer_ptr=refs_end;
      }
   }
 else
    mode=MODE_DELETE;

 /* Mangle the data and send it to the OSM parser */

 nways++;

 if(!(nways%1000))
    printf_middle("Reading: Bytes=%"PRIu64" Nodes=%"PRIu64" Ways=%"PRIu64" Relations=%"PRIu64,byteno,nnodes,nways,nrelations);

 AddWayRefs(0);

 if(refs)
    while(refs<refs_end)
      {
       int64_t delta_ref;

       delta_ref=o5m_sint64(&refs);
       node_refid+=delta_ref;

       AddWayRefs(node_refid);
      }

 tags=NewTagList();

 while(buffer_ptr<buffer_end)
   {
    unsigned char *key,*val;

    process_string(2,&buffer_ptr,&key,&val);

    AppendTag(tags,(char*)key,(char*)val);
   }

 result=ApplyWayTaggingRules(tags,id);

 ProcessWayTags(result,id,mode);

 DeleteTagList(tags);
 DeleteTagList(result);
}


/*++++++++++++++++++++++++++++++++++++++
  Process an O5M Relation dataset.
  ++++++++++++++++++++++++++++++++++++++*/

static void process_relation()
{
 int64_t delta_id;
 TagList *tags=NULL,*result=NULL;
 int mode=mode_change;
 unsigned char *refs=NULL,*refs_end;

 delta_id=o5m_sint64(&buffer_ptr);
 id+=delta_id;

 if(buffer_ptr<buffer_end)
   {
    if(*buffer_ptr!=0)
       process_info();
    else
       buffer_ptr++;
   }

 if(buffer_ptr<buffer_end)
   {
    uint32_t length;

    length=o5m_int32(&buffer_ptr);

    if(length)
      {
       refs=buffer_ptr;
       refs_end=buffer_ptr+length;

       buffer_ptr=refs_end;
      }
   }
 else
    mode=MODE_DELETE;

 /* Mangle the data and send it to the OSM parser */

 nrelations++;

 if(!(nrelations%1000))
    printf_middle("Reading: Bytes=%"PRIu64" Nodes=%"PRIu64" Ways=%"PRIu64" Relations=%"PRIu64,byteno,nnodes,nways,nrelations);

 AddRelationRefs(0,0,0,NULL);

 if(refs)
    while(refs<refs_end)
      {
       int64_t delta_ref;
       unsigned char *typerole=NULL;

       delta_ref=o5m_sint64(&refs);

       typerole=process_string(1,&refs,NULL,NULL);

       if(*typerole=='0')
         {
          node_refid+=delta_ref;

          AddRelationRefs(node_refid,0,0,(char*)(typerole+1));
         }
       else if(*typerole=='1')
         {
          way_refid+=delta_ref;

          AddRelationRefs(0,way_refid,0,(char*)(typerole+1));
         }
       else if(*typerole=='2')
         {
          relation_refid+=delta_ref;

          AddRelationRefs(0,0,relation_refid,(char*)(typerole+1));
         }
      }

 tags=NewTagList();

 while(buffer_ptr<buffer_end)
   {
    unsigned char *key,*val;

    process_string(2,&buffer_ptr,&key,&val);

    AppendTag(tags,(char*)key,(char*)val);
   }

 result=ApplyRelationTaggingRules(tags,id);

 ProcessRelationTags(result,id,mode);

 DeleteTagList(tags);
 DeleteTagList(result);
}


/*++++++++++++++++++++++++++++++++++++++
  Process an O5M info message.
  ++++++++++++++++++++++++++++++++++++++*/

static void process_info(void)
{
 int64_t timestamp_delta;

 o5m_int32(&buffer_ptr);        /* version */

 timestamp_delta=o5m_sint64(&buffer_ptr);
 timestamp+=timestamp_delta;

 if(timestamp!=0)
   {
    o5m_sint32(&buffer_ptr);     /* changeset */

    process_string(2,&buffer_ptr,NULL,NULL);  /* user */
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Process an O5M string and take care of maintaining the string table.

  unsigned char *process_string_pair Return a pointer to the concatenation of the two strings.

  int pair Set to 2 for a pair or 1 for a single string.

  unsigned char **buf_ptr The pointer to the buffer that is to be updated.

  unsigned char **string1 Returns a pointer to the first of the strings in the pair.

  unsigned char **string2 Returns a pointer to the second of the strings in the pair.
  ++++++++++++++++++++++++++++++++++++++*/

static unsigned char *process_string(int pair,unsigned char **buf_ptr,unsigned char **string1,unsigned char **string2)
{
 int lookup=0;
 unsigned char *string;
 unsigned char *p;

 if(**buf_ptr==0)
    string=*buf_ptr+1;
 else
   {
    uint32_t position=o5m_int32(buf_ptr);

    string=string_table[(string_table_start-position+STRING_TABLE_ALLOCATED)%STRING_TABLE_ALLOCATED];

    lookup=1;
   }

 p=string;

 if(pair==2)
   {
    if(string1)
       *string1=p;

    while(*p) p++;

    p++;

    if(string2)
       *string2=p;
   }

 while(*p) p++;

 if(!lookup)
   {
    if((p-string)<252)
      {
       memcpy(string_table[string_table_start],string,p-string+1);

       string_table_start=(string_table_start+1)%STRING_TABLE_ALLOCATED;
      }

    *buf_ptr=p+1;
   }

 return(string);
}
