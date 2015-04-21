/***************************************
 A simple osm-specific PBF parser where the structure is hard-coded.

 Part of the Routino routing software.
 ******************/ /******************
 This file Copyright 2012 Andrew M. Bishop

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
#include <stdint.h>
#include <string.h>

#if defined(USE_GZIP) && USE_GZIP
#include <zlib.h>
#endif

#include "osmparser.h"
#include "tagging.h"
#include "logging.h"


/* Inside a BlobHeader message */

#define PBF_VAL_BLOBHEADER_TYPE    1
#define PBF_VAL_BLOBHEADER_SIZE    3

/* Inside a Blob message */

#define PBF_VAL_BLOB_RAW_DATA      1
#define PBF_VAL_BLOB_RAW_SIZE      2
#define PBF_VAL_BLOB_ZLIB_DATA     3

/* Inside a HeaderBlock message */

#define PBF_VAL_REQUIRED_FEATURES  4
#define PBF_VAL_OPTIONAL_FEATURES  5

/* Inside a PrimitiveBlock message */

#define PBF_VAL_STRING_TABLE       1
#define PBF_VAL_PRIMITIVE_GROUP    2
#define PBF_VAL_GRANULARITY       17
#define PBF_VAL_LAT_OFFSET        19
#define PBF_VAL_LON_OFFSET        20

/* Inside a PrimitiveGroup message */

#define PBF_VAL_NODES              1
#define PBF_VAL_DENSE_NODES        2
#define PBF_VAL_WAYS               3
#define PBF_VAL_RELATIONS          4

/* Inside a StringTable message */

#define PBF_VAL_STRING             1

/* Inside a Node message */

#define PBF_VAL_NODE_ID            1
#define PBF_VAL_NODE_KEYS          2
#define PBF_VAL_NODE_VALS          3
#define PBF_VAL_NODE_LAT           8
#define PBF_VAL_NODE_LON           9

/* Inside a DenseNode message */

#define PBF_VAL_DENSE_NODE_ID         1
#define PBF_VAL_DENSE_NODE_LAT        8
#define PBF_VAL_DENSE_NODE_LON        9
#define PBF_VAL_DENSE_NODE_KEYS_VALS 10

/* Inside a Way message */

#define PBF_VAL_WAY_ID             1
#define PBF_VAL_WAY_KEYS           2
#define PBF_VAL_WAY_VALS           3
#define PBF_VAL_WAY_REFS           8

/* Inside a Relation message */

#define PBF_VAL_RELATION_ID        1
#define PBF_VAL_RELATION_KEYS      2
#define PBF_VAL_RELATION_VALS      3
#define PBF_VAL_RELATION_ROLES     8
#define PBF_VAL_RELATION_MEMIDS    9
#define PBF_VAL_RELATION_TYPES    10

/* Errors */

#define PBF_EOF                     0

#define PBF_ERROR_UNEXP_EOF       100
#define PBF_ERROR_BLOB_HEADER_LEN 101
#define PBF_ERROR_BLOB_LEN        102
#define PBF_ERROR_NOT_OSM         103
#define PBF_ERROR_BLOB_BOTH       104
#define PBF_ERROR_BLOB_NEITHER    105
#define PBF_ERROR_NO_GZIP         106
#define PBF_ERROR_GZIP_INIT       107
#define PBF_ERROR_GZIP_INFLATE    108
#define PBF_ERROR_GZIP_WRONG_LEN  109
#define PBF_ERROR_GZIP_END        110
#define PBF_ERROR_UNSUPPORTED     111
#define PBF_ERROR_TOO_MANY_GROUPS 112


/* Parsing variables and functions */

static uint64_t byteno=0;
static uint64_t nnodes=0,nways=0,nrelations=0;

static int buffer_allocated,zbuffer_allocated;
static unsigned char *buffer=NULL,*zbuffer=NULL;
static unsigned char *buffer_ptr,*buffer_end;

static int string_table_length=0,string_table_allocated=0;
static unsigned char **string_table=NULL;
static uint32_t *string_table_string_lengths=NULL;

static int32_t granularity=100;
static int64_t lat_offset=0,lon_offset=0;

#define LENGTH_32M (32*1024*1024)


/*++++++++++++++++++++++++++++++++++++++
  Refill the data buffer and set the pointers.

  int buffer_refill Return 0 if everything is OK or 1 for EOF.

  int fd The file descriptor to read from.

  uint32_t bytes The number of bytes to read.
  ++++++++++++++++++++++++++++++++++++++*/

static inline int buffer_refill(int fd,uint32_t bytes)
{
 ssize_t n;

 if(bytes>buffer_allocated)
    buffer=(unsigned char *)realloc(buffer,buffer_allocated=bytes);

 byteno+=bytes;

 buffer_ptr=buffer;
 buffer_end=buffer;

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

#if defined(USE_GZIP) && USE_GZIP
static int uncompress_pbf(unsigned char *data,uint32_t compressed,uint32_t uncompressed);
#endif /* USE_GZIP */

static void process_string_table(unsigned char *data,uint32_t length);
static void process_primitive_group(unsigned char *data,uint32_t length);
static void process_nodes(unsigned char *data,uint32_t length);
static void process_dense_nodes(unsigned char *data,uint32_t length);
static void process_ways(unsigned char *data,uint32_t length);
static void process_relations(unsigned char *data,uint32_t length);


/* Macros to simplify the parser (and make it look more like the XML parser) */

#define BEGIN(xx)            do{ state=(xx); goto finish_parsing; } while(0)

#define BUFFER_CHARS_EOF(xx) do{ if(buffer_refill(fd,(xx))) BEGIN(PBF_EOF); } while(0)

#define BUFFER_CHARS(xx)     do{ if(buffer_refill(fd,(xx))) BEGIN(PBF_ERROR_UNEXP_EOF); } while(0)


/* PBF decoding */

#define PBF_FIELD(xx)   (int)(((xx)&0xFFF8)>>3)
#define PBF_TYPE(xx)    (int)((xx)&0x0007)

#define PBF_LATITUDE(xx)  (double)(1E-9*(granularity*(xx)+lat_offset))
#define PBF_LONGITUDE(xx) (double)(1E-9*(granularity*(xx)+lon_offset))


/*++++++++++++++++++++++++++++++++++++++
  Parse a PBF int32 data value.

  uint32_t pbf_int32 Returns the integer value.

  unsigned char **ptr The pointer to read the data from.
  ++++++++++++++++++++++++++++++++++++++*/

static inline uint32_t pbf_int32(unsigned char **ptr)
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
  Parse a PBF int64 data value.

  int64_t pbf_int64 Returns the integer value.

  unsigned char **ptr The pointer to read the data from.
  ++++++++++++++++++++++++++++++++++++++*/

static inline int64_t pbf_int64(unsigned char **ptr)
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
  Parse a PBF sint64 data value.

  int64_t pbf_sint64 Returns the integer value.

  unsigned char **ptr The pointer to read the data from.
  ++++++++++++++++++++++++++++++++++++++*/

static inline int64_t pbf_sint64(unsigned char **ptr)
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
  Parse a PBF length delimited data value.

  unsigned char *pbf_length_delimited Returns a pointer to the start of the data.

  unsigned char **ptr The pointer to read the data from.

  uint32_t *length Returns the length of the data.
  ++++++++++++++++++++++++++++++++++++++*/

static inline unsigned char *pbf_length_delimited(unsigned char **ptr,uint32_t *length)
{
 uint32_t len=pbf_int32(ptr);

 if(length)
    *length=len;

 *ptr+=len;

 return(*ptr-len);
}


/*++++++++++++++++++++++++++++++++++++++
  Skip any pbf field from a message.

  unsigned char **ptr The pointer to read the data from.

  int type The type of the data.
  ++++++++++++++++++++++++++++++++++++++*/

static inline void pbf_skip(unsigned char **ptr,int type)
{
 uint32_t length;

 switch(type)
   {
   case 0: /* varint */
    while((**ptr)&0x80) (*ptr)++;
    (*ptr)++;
    break;
   case 1: /* 64-bit */
    *ptr+=8;
    break;
   case 2: /* length delimited */
    length=pbf_int32(ptr);
    *ptr+=length;
    break;
   case 3: /* deprecated */
    break;
   case 4: /* deprecated */
    break;
   case 5: /* 32-bit */
    *ptr+=4;
    break;
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Parse the PBF and call the functions for each OSM item as seen.

  int ParsePBF Returns 0 if OK or something else in case of an error.

  int fd The file descriptor of the file to parse.
  ++++++++++++++++++++++++++++++++++++++*/

int ParsePBF(int fd)
{
 int state;
 unsigned char *error=NULL;

 /* Print the initial message */

 nnodes=0,nways=0,nrelations=0;

 printf_first("Reading: Bytes=0 Nodes=0 Ways=0 Relations=0");

 /* The actual parser. */

 string_table_allocated=16384;
 string_table_length=0;
 string_table=(unsigned char **)malloc(string_table_allocated*sizeof(unsigned char *));
 string_table_string_lengths=(uint32_t *)malloc(string_table_allocated*sizeof(uint32_t));

 zbuffer_allocated=0;
 zbuffer=NULL;

 buffer_allocated=65536;
 buffer=(unsigned char*)malloc(buffer_allocated);

 buffer_ptr=buffer_end=buffer;

 while(1)
   {
    int32_t blob_header_length=0;
    int osm_data=0,osm_header=0;
    int32_t blob_length=0;
    uint32_t raw_size=0,compressed_size=0,uncompressed_size=0;
    unsigned char *raw_data=NULL,*zlib_data=NULL;
    uint32_t length;
    unsigned char *data;

    /* ================ Parsing states ================ */


    BUFFER_CHARS_EOF(4);

    blob_header_length=(256*(256*(256*(int)buffer_ptr[0])+(int)buffer_ptr[1])+(int)buffer_ptr[2])+buffer_ptr[3];
    buffer_ptr+=4;

    if(blob_header_length==0 || blob_header_length>LENGTH_32M)
       BEGIN(PBF_ERROR_BLOB_HEADER_LEN);


    BUFFER_CHARS(blob_header_length);

    osm_header=0;
    osm_data=0;

    while(buffer_ptr<buffer_end)
      {
       int fieldtype=pbf_int32(&buffer_ptr);
       int field=PBF_FIELD(fieldtype);

       switch(field)
         {
         case PBF_VAL_BLOBHEADER_TYPE: /* string */
          {
           uint32_t length=0;
           unsigned char *type=NULL;

           type=pbf_length_delimited(&buffer_ptr,&length);

           if(length==9 && !strncmp((char*)type,"OSMHeader",9))
              osm_header=1;

           if(length==7 && !strncmp((char*)type,"OSMData",7))
              osm_data=1;
          }
          break;

         case PBF_VAL_BLOBHEADER_SIZE: /* int32 */
          blob_length=pbf_int32(&buffer_ptr);
          break;

         default:
          pbf_skip(&buffer_ptr,PBF_TYPE(fieldtype));
         }
      }

    if(blob_length==0 || blob_length>LENGTH_32M)
       BEGIN(PBF_ERROR_BLOB_LEN);

    if(!osm_data && !osm_header)
       BEGIN(PBF_ERROR_NOT_OSM);


    BUFFER_CHARS(blob_length);

    while(buffer_ptr<buffer_end)
      {
       int fieldtype=pbf_int32(&buffer_ptr);
       int field=PBF_FIELD(fieldtype);

       switch(field)
         {
         case PBF_VAL_BLOB_RAW_DATA: /* bytes */
          raw_data=pbf_length_delimited(&buffer_ptr,&raw_size);
          break;

         case PBF_VAL_BLOB_RAW_SIZE: /* int32 */
          uncompressed_size=pbf_int32(&buffer_ptr);
          break;

         case PBF_VAL_BLOB_ZLIB_DATA: /* bytes */
          zlib_data=pbf_length_delimited(&buffer_ptr,&compressed_size);
          break;

         default:
          pbf_skip(&buffer_ptr,PBF_TYPE(fieldtype));
         }
      }

    if(raw_data && zlib_data)
       BEGIN(PBF_ERROR_BLOB_BOTH);

    if(!raw_data && !zlib_data)
       BEGIN(PBF_ERROR_BLOB_NEITHER);

    if(zlib_data)
      {
#if defined(USE_GZIP) && USE_GZIP
       int newstate=uncompress_pbf(zlib_data,compressed_size,uncompressed_size);

       if(newstate)
          BEGIN(newstate);
#else
       BEGIN(PBF_ERROR_NO_GZIP);
#endif
      }
    else
      {
       buffer_ptr=raw_data;
       buffer_end=raw_data+raw_size;
      }


    if(osm_header)
      {
       while(buffer_ptr<buffer_end)
         {
          int fieldtype=pbf_int32(&buffer_ptr);
          int field=PBF_FIELD(fieldtype);

          switch(field)
            {
            case PBF_VAL_REQUIRED_FEATURES: /* string */
             {
              uint32_t length=0;
              unsigned char *feature=NULL;

              feature=pbf_length_delimited(&buffer_ptr,&length);

              if(strncmp((char*)feature,"OsmSchema-V0.6",14) &&
                 strncmp((char*)feature,"DenseNodes",10))
                {
                 feature[length]=0;
                 error=feature;
                 BEGIN(PBF_ERROR_UNSUPPORTED);
                }
             }
             break;

            case PBF_VAL_OPTIONAL_FEATURES: /* string */
             pbf_length_delimited(&buffer_ptr,NULL);
             break;

            default:
             pbf_skip(&buffer_ptr,PBF_TYPE(fieldtype));
            }
         }
      }


    if(osm_data)
      {
       unsigned char *primitive_group[8]={NULL};
       uint32_t primitive_group_length[8]={0};
       int nprimitive_groups=0,i;

       granularity=100;
       lat_offset=lon_offset=0;

       while(buffer_ptr<buffer_end)
         {
          int fieldtype=pbf_int32(&buffer_ptr);
          int field=PBF_FIELD(fieldtype);

          switch(field)
            {
            case PBF_VAL_STRING_TABLE: /* bytes */
             data=pbf_length_delimited(&buffer_ptr,&length);
             process_string_table(data,length);
             break;

            case PBF_VAL_PRIMITIVE_GROUP: /* bytes */
             primitive_group[nprimitive_groups]=pbf_length_delimited(&buffer_ptr,&primitive_group_length[nprimitive_groups]);

             if(++nprimitive_groups>(sizeof(primitive_group)/sizeof(primitive_group[0])))
                BEGIN(PBF_ERROR_TOO_MANY_GROUPS);
             break;

            case PBF_VAL_GRANULARITY: /* int32 */
             granularity=pbf_int32(&buffer_ptr);
             break;

            case PBF_VAL_LAT_OFFSET: /* int64 */
             lat_offset=pbf_int64(&buffer_ptr);
             break;

            case PBF_VAL_LON_OFFSET: /* int64 */
             lon_offset=pbf_int64(&buffer_ptr);
             break;

            default:
             pbf_skip(&buffer_ptr,PBF_TYPE(fieldtype));
            }
         }

       if(nprimitive_groups)
          for(i=0;i<nprimitive_groups;i++)
             process_primitive_group(primitive_group[i],primitive_group_length[i]);
      }
   }


 finish_parsing:

 switch(state)
   {
    /* End of file */

   case PBF_EOF:
    break;


    /* ================ Error states ================ */


   case PBF_ERROR_UNEXP_EOF:
    fprintf(stderr,"PBF Parser: Error at byte %llu: unexpected end of file seen.\n",byteno);
    break;

   case PBF_ERROR_BLOB_HEADER_LEN:
    fprintf(stderr,"PBF Parser: Error at byte %llu: BlobHeader length is wrong (0<x<=32M).\n",byteno);
    break;

   case PBF_ERROR_BLOB_LEN:
    fprintf(stderr,"PBF Parser: Error at byte %llu: Blob length is wrong (0<x<=32M).\n",byteno);
    break;

   case PBF_ERROR_NOT_OSM:
    fprintf(stderr,"PBF Parser: Error at byte %llu: BlobHeader is neither 'OSMData' or 'OSMHeader'.\n",byteno);
    break;

   case PBF_ERROR_BLOB_BOTH:
    fprintf(stderr,"PBF Parser: Error at byte %llu: Blob has both zlib compressed and raw uncompressed data.\n",byteno);
    break;

   case PBF_ERROR_BLOB_NEITHER:
    fprintf(stderr,"PBF Parser: Error at byte %llu: Blob has neither zlib compressed or raw uncompressed data.\n",byteno);
    break;

   case PBF_ERROR_NO_GZIP:
    fprintf(stderr,"PBF Parser: Error at byte %llu: Blob is compressed but no gzip support is available.\n",byteno);
    break;

   case PBF_ERROR_GZIP_INIT:
    fprintf(stderr,"PBF Parser: Error at byte %llu: Blob is compressed but failed to initialise decompression.\n",byteno);
    break;

   case PBF_ERROR_GZIP_INFLATE:
    fprintf(stderr,"PBF Parser: Error at byte %llu: Blob is compressed but failed to uncompress it.\n",byteno);
    break;

   case PBF_ERROR_GZIP_WRONG_LEN:
    fprintf(stderr,"PBF Parser: Error at byte %llu: Blob is compressed and wrong size when uncompressed.\n",byteno);
    break;

   case PBF_ERROR_GZIP_END:
    fprintf(stderr,"PBF Parser: Error at byte %llu: Blob is compressed but failed to finalise decompression.\n",byteno);
    break;

   case PBF_ERROR_UNSUPPORTED:
    fprintf(stderr,"PBF Parser: Error at byte %llu: Unsupported required feature '%s'.\n",byteno,error);
    break;

   case PBF_ERROR_TOO_MANY_GROUPS:
    fprintf(stderr,"PBF Parser: Error at byte %llu: OsmData message contains too many PrimitiveGroup messages.\n",byteno);
    break;
   }

 /* Free the parser variables */

 free(string_table);
 free(string_table_string_lengths);

 free(buffer);
 if(zbuffer)
    free(zbuffer);

 /* Print the final message */

 printf_last("Read: Bytes=%"PRIu64" Nodes=%"PRIu64" Ways=%"PRIu64" Relations=%"PRIu64,byteno,nnodes,nways,nrelations);

 return(state);
}


/*++++++++++++++++++++++++++++++++++++++
  Process a PBF StringTable message.

  unsigned char *data The data to process.

  uint32_t length The length of the data.
  ++++++++++++++++++++++++++++++++++++++*/

static void process_string_table(unsigned char *data,uint32_t length)
{
 unsigned char *end=data+length;
 unsigned char *string;
 uint32_t string_length;

 string_table_length=0;

 while(data<end)
   {
    int fieldtype=pbf_int32(&data);
    int field=PBF_FIELD(fieldtype);

    switch(field)
      {
      case PBF_VAL_STRING:      /* string */
       string=pbf_length_delimited(&data,&string_length);

       if(string_table_length==string_table_allocated)
         {
          string_table_allocated+=8192;
          string_table=(unsigned char **)realloc(string_table,string_table_allocated*sizeof(unsigned char *));
          string_table_string_lengths=(uint32_t *)realloc(string_table_string_lengths,string_table_allocated*sizeof(uint32_t));
         }

       string_table[string_table_length]=string;
       string_table_string_lengths[string_table_length]=string_length;

       string_table_length++;
       break;

      default:
       pbf_skip(&data,PBF_TYPE(fieldtype));
      }
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Process a PBF PrimitiveGroup message.

  unsigned char *data The data to process.

  uint32_t length The length of the data.
  ++++++++++++++++++++++++++++++++++++++*/

static void process_primitive_group(unsigned char *data,uint32_t length)
{
 unsigned char *end=data+length;
 unsigned char *subdata;
 uint32_t sublength;
 int i;

 /* Fixup the strings (not null terminated in buffer) */

 for(i=0;i<string_table_length;i++)
    string_table[i][string_table_string_lengths[i]]=0;


 while(data<end)
   {
    int fieldtype=pbf_int32(&data);
    int field=PBF_FIELD(fieldtype);

    switch(field)
      {
      case PBF_VAL_NODES:       /* message */
       subdata=pbf_length_delimited(&data,&sublength);
       process_nodes(subdata,sublength);
       break;

      case PBF_VAL_DENSE_NODES: /* message */
       subdata=pbf_length_delimited(&data,&sublength);
       process_dense_nodes(subdata,sublength);
       break;

      case PBF_VAL_WAYS:        /* message */
       subdata=pbf_length_delimited(&data,&sublength);
       process_ways(subdata,sublength);
       break;

      case PBF_VAL_RELATIONS:   /* message */
       subdata=pbf_length_delimited(&data,&sublength);
       process_relations(subdata,sublength);
       break;

      default:
       pbf_skip(&data,PBF_TYPE(fieldtype));
      }
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Process a PBF Node message.

  unsigned char *data The data to process.

  uint32_t length The length of the data.
  ++++++++++++++++++++++++++++++++++++++*/

static void process_nodes(unsigned char *data,uint32_t length)
{
 unsigned char *end=data+length;
 int64_t id=0;
 unsigned char *keys=NULL,*vals=NULL;
 unsigned char *keys_end=NULL,*vals_end=NULL;
 uint32_t keylen=0,vallen=0;
 int64_t lat=0,lon=0;
 TagList *tags=NULL,*result=NULL;

 while(data<end)
   {
    int fieldtype=pbf_int32(&data);
    int field=PBF_FIELD(fieldtype);

    switch(field)
      {
      case PBF_VAL_NODE_ID:     /* sint64 */
       id=pbf_sint64(&data);
       break;

      case PBF_VAL_NODE_KEYS:   /* packed int32 */
       keys=pbf_length_delimited(&data,&keylen);
       keys_end=keys+keylen;
       break;

      case PBF_VAL_NODE_VALS:   /* packed int32 */
       vals=pbf_length_delimited(&data,&vallen);
       vals_end=vals+vallen;
       break;

      case PBF_VAL_NODE_LAT:    /* sint64 */
       lat=pbf_sint64(&data);
       break;

      case PBF_VAL_NODE_LON:    /* sint64 */
       lon=pbf_sint64(&data);
       break;

      default:
       pbf_skip(&data,PBF_TYPE(fieldtype));
      }
   }

 /* Mangle the data and send it to the OSM parser */

 nnodes++;

 if(!(nnodes%10000))
    printf_middle("Reading: Bytes=%"PRIu64" Nodes=%"PRIu64" Ways=%"PRIu64" Relations=%"PRIu64,byteno,nnodes,nways,nrelations);

 tags=NewTagList();

 if(keys && vals)
   {
    while(keys<keys_end && vals<vals_end)
      {
       uint32_t key=pbf_int32(&keys);
       uint32_t val=pbf_int32(&vals);

       AppendTag(tags,(char*)string_table[key],(char*)string_table[val]);
      }
   }

 result=ApplyNodeTaggingRules(tags,id);

 ProcessNodeTags(result,id,PBF_LATITUDE(lat),PBF_LONGITUDE(lon),MODE_NORMAL);

 DeleteTagList(tags);
 DeleteTagList(result);
}


/*++++++++++++++++++++++++++++++++++++++
  Process a PBF DenseNode message.

  unsigned char *data The data to process.

  uint32_t length The length of the data.
  ++++++++++++++++++++++++++++++++++++++*/

static void process_dense_nodes(unsigned char *data,uint32_t length)
{
 unsigned char *end=data+length;
 unsigned char *ids=NULL,*keys_vals=NULL,*lats=NULL,*lons=NULL;
 unsigned char *ids_end=NULL;
 uint32_t idlen=0;
 int64_t id=0;
 int64_t lat=0,lon=0;
 TagList *tags=NULL,*result;

 while(data<end)
   {
    int fieldtype=pbf_int32(&data);
    int field=PBF_FIELD(fieldtype);

    switch(field)
      {
      case PBF_VAL_DENSE_NODE_ID: /* packed sint64 */
       ids=pbf_length_delimited(&data,&idlen);
       ids_end=ids+idlen;
       break;

      case PBF_VAL_DENSE_NODE_LAT: /* packed sint64 */
       lats=pbf_length_delimited(&data,NULL);
       break;

      case PBF_VAL_DENSE_NODE_LON: /* packed sint64 */
       lons=pbf_length_delimited(&data,NULL);
       break;

      case PBF_VAL_DENSE_NODE_KEYS_VALS: /* packed int32 */
       keys_vals=pbf_length_delimited(&data,NULL);
       break;

      default:
       pbf_skip(&data,PBF_TYPE(fieldtype));
      }
   }

 while(ids<ids_end)
   {
    int64_t delta_id;
    int64_t delta_lat,delta_lon;

    delta_id=pbf_sint64(&ids);
    delta_lat=pbf_sint64(&lats);
    delta_lon=pbf_sint64(&lons);

    id+=delta_id;
    lat+=delta_lat;
    lon+=delta_lon;

    /* Mangle the data and send it to the OSM parser */

    nnodes++;

    if(!(nnodes%10000))
       printf_middle("Reading: Bytes=%"PRIu64" Nodes=%"PRIu64" Ways=%"PRIu64" Relations=%"PRIu64,byteno,nnodes,nways,nrelations);

    tags=NewTagList();

    if(keys_vals)
      {
       while(1)
         {
          uint32_t key=pbf_int32(&keys_vals),val;

          if(key==0)
             break;

          val=pbf_int32(&keys_vals);

          AppendTag(tags,(char*)string_table[key],(char*)string_table[val]);
         }
      }

    result=ApplyNodeTaggingRules(tags,id);

    ProcessNodeTags(result,id,PBF_LATITUDE(lat),PBF_LONGITUDE(lon),MODE_NORMAL);

    DeleteTagList(tags);
    DeleteTagList(result);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Process a PBF Way message.

  unsigned char *data The data to process.

  uint32_t length The length of the data.
  ++++++++++++++++++++++++++++++++++++++*/

static void process_ways(unsigned char *data,uint32_t length)
{
 unsigned char *end=data+length;
 int64_t id=0;
 unsigned char *keys=NULL,*vals=NULL,*refs=NULL;
 unsigned char *keys_end=NULL,*vals_end=NULL,*refs_end=NULL;
 uint32_t keylen=0,vallen=0,reflen=0;
 int64_t ref=0;
 TagList *tags=NULL,*result;

 while(data<end)
   {
    int fieldtype=pbf_int32(&data);
    int field=PBF_FIELD(fieldtype);

    switch(field)
      {
      case PBF_VAL_WAY_ID:      /* int64 */
       id=pbf_int64(&data);
       break;

      case PBF_VAL_WAY_KEYS:    /* packed int32 */
       keys=pbf_length_delimited(&data,&keylen);
       keys_end=keys+keylen;
       break;

      case PBF_VAL_WAY_VALS:    /* packed int32 */
       vals=pbf_length_delimited(&data,&vallen);
       vals_end=vals+vallen;
       break;

      case PBF_VAL_WAY_REFS:    /* packed sint64 */
       refs=pbf_length_delimited(&data,&reflen);
       refs_end=refs+reflen;
       break;

      default:
       pbf_skip(&data,PBF_TYPE(fieldtype));
      }
   }

 /* Mangle the data and send it to the OSM parser */

 nways++;

 if(!(nways%1000))
    printf_middle("Reading: Bytes=%"PRIu64" Nodes=%"PRIu64" Ways=%"PRIu64" Relations=%"PRIu64,byteno,nnodes,nways,nrelations);

 tags=NewTagList();

 if(keys && vals)
   {
    while(keys<keys_end && vals<vals_end)
      {
       uint32_t key=pbf_int32(&keys);
       uint32_t val=pbf_int32(&vals);

       AppendTag(tags,(char*)string_table[key],(char*)string_table[val]);
      }
   }

 AddWayRefs(0);

 if(refs)
    while(refs<refs_end)
      {
       int64_t delta_ref;

       delta_ref=pbf_sint64(&refs);

       ref+=delta_ref;

       if(ref==0)
          break;

       AddWayRefs(ref);
      }

 result=ApplyWayTaggingRules(tags,id);

 ProcessWayTags(result,id,MODE_NORMAL);

 DeleteTagList(tags);
 DeleteTagList(result);
}


/*++++++++++++++++++++++++++++++++++++++
  Process a PBF Relation message.

  unsigned char *data The data to process.

  uint32_t length The length of the data.
  ++++++++++++++++++++++++++++++++++++++*/

static void process_relations(unsigned char *data,uint32_t length)
{
 unsigned char *end=data+length;
 int64_t id=0;
 unsigned char *keys=NULL,*vals=NULL,*roles=NULL,*memids=NULL,*types=NULL;
 unsigned char *keys_end=NULL,*vals_end=NULL,*memids_end=NULL,*types_end=NULL;
 uint32_t keylen=0,vallen=0,rolelen=0,memidlen=0,typelen=0;
 int64_t memid=0;
 TagList *tags=NULL,*result;

 while(data<end)
   {
    int fieldtype=pbf_int32(&data);
    int field=PBF_FIELD(fieldtype);

    switch(field)
      {
      case PBF_VAL_RELATION_ID: /* int64 */
       id=pbf_int64(&data);
       break;

      case PBF_VAL_RELATION_KEYS: /* packed string */
       keys=pbf_length_delimited(&data,&keylen);
       keys_end=keys+keylen;
       break;

      case PBF_VAL_RELATION_VALS: /* packed string */
       vals=pbf_length_delimited(&data,&vallen);
       vals_end=vals+vallen;
       break;

      case PBF_VAL_RELATION_ROLES: /* packed int32 */
       roles=pbf_length_delimited(&data,&rolelen);
       break;

      case PBF_VAL_RELATION_MEMIDS: /* packed sint64 */
       memids=pbf_length_delimited(&data,&memidlen);
       memids_end=memids+memidlen;
       break;

      case PBF_VAL_RELATION_TYPES: /* packed enum */
       types=pbf_length_delimited(&data,&typelen);
       types_end=types+typelen;
       break;

      default:
       pbf_skip(&data,PBF_TYPE(fieldtype));
      }
   }

 /* Mangle the data and send it to the OSM parser */

 nrelations++;

 if(!(nrelations%1000))
    printf_middle("Reading: Bytes=%"PRIu64" Nodes=%"PRIu64" Ways=%"PRIu64" Relations=%"PRIu64,byteno,nnodes,nways,nrelations);

 AddRelationRefs(0,0,0,NULL);

 tags=NewTagList();

 if(keys && vals)
   {
    while(keys<keys_end && vals<vals_end)
      {
       uint32_t key=pbf_int32(&keys);
       uint32_t val=pbf_int32(&vals);

       AppendTag(tags,(char*)string_table[key],(char*)string_table[val]);
      }
   }

 if(memids && types)
    while(memids<memids_end && types<types_end)
      {
       int64_t delta_memid;
       unsigned char *role=NULL;
       int type;

       delta_memid=pbf_sint64(&memids);
       type=pbf_int32(&types);

       if(roles)
          role=string_table[pbf_int32(&roles)];

       memid+=delta_memid;

       if(type==0)
          AddRelationRefs(memid,0,0,(char*)role);
       else if(type==1)
          AddRelationRefs(0,memid,0,(char*)role);
       else if(type==2)
          AddRelationRefs(0,0,memid,(char*)role);
      }

 result=ApplyRelationTaggingRules(tags,id);

 ProcessRelationTags(result,id,MODE_NORMAL);

 DeleteTagList(tags);
 DeleteTagList(result);
}


#if defined(USE_GZIP) && USE_GZIP

/*++++++++++++++++++++++++++++++++++++++
  Uncompress the part of the PBF data that is compressed.

  int uncompress_pbf Returns the error state or 0 if OK.

  unsigned char *data The data to uncompress.

  uint32_t compressed The number of bytes to uncompress.

  uint32_t uncompressed The number of bytes expected when uncompressed.
  ++++++++++++++++++++++++++++++++++++++*/

static int uncompress_pbf(unsigned char *data,uint32_t compressed,uint32_t uncompressed)
{
 z_stream z={0};

 if(uncompressed>zbuffer_allocated)
    zbuffer=(unsigned char *)realloc(zbuffer,zbuffer_allocated=uncompressed);

 if(inflateInit2(&z,15+32)!=Z_OK)
    return(PBF_ERROR_GZIP_INIT);

 z.next_in=data;
 z.avail_in=compressed;

 z.next_out=zbuffer;
 z.avail_out=uncompressed;

 if(inflate(&z,Z_FINISH)!=Z_STREAM_END)
    return(PBF_ERROR_GZIP_INFLATE);

 if(z.avail_out!=0)
    return(PBF_ERROR_GZIP_WRONG_LEN);

 if(inflateEnd(&z)!=Z_OK)
    return(PBF_ERROR_GZIP_END);

 buffer_ptr=zbuffer;
 buffer_end=zbuffer+uncompressed;

 return(0);
}

#endif /* USE_GZIP */
