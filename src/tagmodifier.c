/***************************************
 Test application for OSM XML file parser / tagging rule testing.

 Part of the Routino routing software.
 ******************/ /******************
 This file Copyright 2010-2013 Andrew M. Bishop

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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>

#include "xmlparse.h"
#include "logging.h"
#include "tagging.h"

#include "files.h"
#include "uncompress.h"


/* Local variables */

static uint64_t nnodes=0,nways=0,nrelations=0;

TagList *current_tags=NULL;


/* Local functions */

static void print_usage(int detail,const char *argerr,const char *err);


/* The XML tag processing function prototypes */

static int xmlDeclaration_function(const char *_tag_,int _type_,const char *version,const char *encoding);
static int osmType_function(const char *_tag_,int _type_,const char *version,const char *generator);
static int relationType_function(const char *_tag_,int _type_,const char *id,const char *timestamp,const char *uid,const char *user,const char *visible,const char *version,const char *action);
static int wayType_function(const char *_tag_,int _type_,const char *id,const char *timestamp,const char *uid,const char *user,const char *visible,const char *version,const char *action);
static int memberType_function(const char *_tag_,int _type_,const char *type,const char *ref,const char *role);
static int ndType_function(const char *_tag_,int _type_,const char *ref);
static int nodeType_function(const char *_tag_,int _type_,const char *id,const char *lat,const char *lon,const char *timestamp,const char *uid,const char *user,const char *visible,const char *version,const char *action);
static int tagType_function(const char *_tag_,int _type_,const char *k,const char *v);
static int boundType_function(const char *_tag_,int _type_,const char *box,const char *origin);
static int boundsType_function(const char *_tag_,int _type_,const char *minlat,const char *minlon,const char *maxlat,const char *maxlon,const char *origin);


/* The XML tag definitions */

/*+ The boundsType type tag. +*/
static xmltag boundsType_tag=
              {"bounds",
               5, {"minlat","minlon","maxlat","maxlon","origin"},
               boundsType_function,
               {NULL}};

/*+ The boundType type tag. +*/
static xmltag boundType_tag=
              {"bound",
               2, {"box","origin"},
               boundType_function,
               {NULL}};

/*+ The tagType type tag. +*/
static xmltag tagType_tag=
              {"tag",
               2, {"k","v"},
               tagType_function,
               {NULL}};

/*+ The nodeType type tag. +*/
static xmltag nodeType_tag=
              {"node",
               9, {"id","lat","lon","timestamp","uid","user","visible","version","action"},
               nodeType_function,
               {&tagType_tag,NULL}};

/*+ The ndType type tag. +*/
static xmltag ndType_tag=
              {"nd",
               1, {"ref"},
               ndType_function,
               {NULL}};

/*+ The memberType type tag. +*/
static xmltag memberType_tag=
              {"member",
               3, {"type","ref","role"},
               memberType_function,
               {NULL}};

/*+ The wayType type tag. +*/
static xmltag wayType_tag=
              {"way",
               7, {"id","timestamp","uid","user","visible","version","action"},
               wayType_function,
               {&ndType_tag,&tagType_tag,NULL}};

/*+ The relationType type tag. +*/
static xmltag relationType_tag=
              {"relation",
               7, {"id","timestamp","uid","user","visible","version","action"},
               relationType_function,
               {&memberType_tag,&tagType_tag,NULL}};

/*+ The osmType type tag. +*/
static xmltag osmType_tag=
              {"osm",
               2, {"version","generator"},
               osmType_function,
               {&boundsType_tag,&boundType_tag,&nodeType_tag,&wayType_tag,&relationType_tag,NULL}};

/*+ The xmlDeclaration type tag. +*/
static xmltag xmlDeclaration_tag=
              {"xml",
               2, {"version","encoding"},
               xmlDeclaration_function,
               {NULL}};


/*+ The complete set of tags at the top level. +*/
static xmltag *xml_toplevel_tags[]={&xmlDeclaration_tag,&osmType_tag,NULL};


/* The XML tag processing functions */


/*++++++++++++++++++++++++++++++++++++++
  The function that is called when the boundsType XSD type is seen

  int boundsType_function Returns 0 if no error occured or something else otherwise.

  const char *_tag_ Set to the name of the element tag that triggered this function call.

  int _type_ Set to XMLPARSE_TAG_START at the start of a tag and/or XMLPARSE_TAG_END at the end of a tag.

  const char *minlat The contents of the 'minlat' attribute (or NULL if not defined).

  const char *minlon The contents of the 'minlon' attribute (or NULL if not defined).

  const char *maxlat The contents of the 'maxlat' attribute (or NULL if not defined).

  const char *maxlon The contents of the 'maxlon' attribute (or NULL if not defined).

  const char *origin The contents of the 'origin' attribute (or NULL if not defined).
  ++++++++++++++++++++++++++++++++++++++*/

static int boundsType_function(const char *_tag_,int _type_,const char *minlat,const char *minlon,const char *maxlat,const char *maxlon,const char *origin)
{
 printf("  <%s%s",(_type_==XMLPARSE_TAG_END)?"/":"",_tag_);
 if(minlat) printf(" minlat='%s'",ParseXML_Encode_Safe_XML(minlat));
 if(minlon) printf(" minlon='%s'",ParseXML_Encode_Safe_XML(minlon));
 if(maxlat) printf(" maxlat='%s'",ParseXML_Encode_Safe_XML(maxlat));
 if(maxlon) printf(" maxlon='%s'",ParseXML_Encode_Safe_XML(maxlon));
 if(origin) printf(" origin='%s'",ParseXML_Encode_Safe_XML(origin));
 printf("%s>\n",(_type_==(XMLPARSE_TAG_START|XMLPARSE_TAG_END))?" /":"");
 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  The function that is called when the boundType XSD type is seen

  int boundType_function Returns 0 if no error occured or something else otherwise.

  const char *_tag_ Set to the name of the element tag that triggered this function call.

  int _type_ Set to XMLPARSE_TAG_START at the start of a tag and/or XMLPARSE_TAG_END at the end of a tag.

  const char *box The contents of the 'box' attribute (or NULL if not defined).

  const char *origin The contents of the 'origin' attribute (or NULL if not defined).
  ++++++++++++++++++++++++++++++++++++++*/

static int boundType_function(const char *_tag_,int _type_,const char *box,const char *origin)
{
 printf("  <%s%s",(_type_==XMLPARSE_TAG_END)?"/":"",_tag_);
 if(box) printf(" box='%s'",ParseXML_Encode_Safe_XML(box));
 if(origin) printf(" origin='%s'",ParseXML_Encode_Safe_XML(origin));
 printf("%s>\n",(_type_==(XMLPARSE_TAG_START|XMLPARSE_TAG_END))?" /":"");
 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  The function that is called when the tagType XSD type is seen

  int tagType_function Returns 0 if no error occured or something else otherwise.

  const char *_tag_ Set to the name of the element tag that triggered this function call.

  int _type_ Set to XMLPARSE_TAG_START at the start of a tag and/or XMLPARSE_TAG_END at the end of a tag.

  const char *k The contents of the 'k' attribute (or NULL if not defined).

  const char *v The contents of the 'v' attribute (or NULL if not defined).
  ++++++++++++++++++++++++++++++++++++++*/

static int tagType_function(const char *_tag_,int _type_,const char *k,const char *v)
{
 if(_type_&XMLPARSE_TAG_START)
   {
    XMLPARSE_ASSERT_STRING(_tag_,k);
    XMLPARSE_ASSERT_STRING(_tag_,v);

    AppendTag(current_tags,k,v);
   }

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  The function that is called when the nodeType XSD type is seen

  int nodeType_function Returns 0 if no error occured or something else otherwise.

  const char *_tag_ Set to the name of the element tag that triggered this function call.

  int _type_ Set to XMLPARSE_TAG_START at the start of a tag and/or XMLPARSE_TAG_END at the end of a tag.

  const char *id The contents of the 'id' attribute (or NULL if not defined).

  const char *lat The contents of the 'lat' attribute (or NULL if not defined).

  const char *lon The contents of the 'lon' attribute (or NULL if not defined).

  const char *timestamp The contents of the 'timestamp' attribute (or NULL if not defined).

  const char *uid The contents of the 'uid' attribute (or NULL if not defined).

  const char *user The contents of the 'user' attribute (or NULL if not defined).

  const char *visible The contents of the 'visible' attribute (or NULL if not defined).

  const char *version The contents of the 'version' attribute (or NULL if not defined).

  const char *action The contents of the 'action' attribute (or NULL if not defined).
  ++++++++++++++++++++++++++++++++++++++*/

static int nodeType_function(const char *_tag_,int _type_,const char *id,const char *lat,const char *lon,const char *timestamp,const char *uid,const char *user,const char *visible,const char *version,const char *action)
{
 static int64_t llid;

 if(_type_&XMLPARSE_TAG_START)
   {
    nnodes++;

    if(!(nnodes%10000))
       fprintf_middle(stderr,"Reading: Lines=%"PRIu64" Nodes=%"PRIu64" Ways=%"PRIu64" Relations=%"PRIu64,ParseXML_LineNumber(),nnodes,nways,nrelations);

    current_tags=NewTagList();

    /* Handle the node information */

    XMLPARSE_ASSERT_INTEGER(_tag_,id); llid=atoll(id); /* need int64_t conversion */
   }

 if(_type_&XMLPARSE_TAG_END)
   {
    TagList *result=ApplyNodeTaggingRules(current_tags,llid);
    int i;

    for(i=0;i<result->ntags;i++)
      {
       printf("    <tag");
       printf(" k='%s'",ParseXML_Encode_Safe_XML(result->k[i]));
       printf(" v='%s'",ParseXML_Encode_Safe_XML(result->v[i]));
       printf(" />\n");
      }

    DeleteTagList(current_tags);
    DeleteTagList(result);
   }

 printf("  <%s%s",(_type_==XMLPARSE_TAG_END)?"/":"",_tag_);
 if(id) printf(" id='%s'",ParseXML_Encode_Safe_XML(id));
 if(lat) printf(" lat='%s'",ParseXML_Encode_Safe_XML(lat));
 if(lon) printf(" lon='%s'",ParseXML_Encode_Safe_XML(lon));
 if(timestamp) printf(" timestamp='%s'",ParseXML_Encode_Safe_XML(timestamp));
 if(uid) printf(" uid='%s'",ParseXML_Encode_Safe_XML(uid));
 if(user) printf(" user='%s'",ParseXML_Encode_Safe_XML(user));
 if(visible) printf(" visible='%s'",ParseXML_Encode_Safe_XML(visible));
 if(version) printf(" version='%s'",ParseXML_Encode_Safe_XML(version));
 if(action) printf(" action='%s'",ParseXML_Encode_Safe_XML(action));
 printf("%s>\n",(_type_==(XMLPARSE_TAG_START|XMLPARSE_TAG_END))?" /":"");
 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  The function that is called when the ndType XSD type is seen

  int ndType_function Returns 0 if no error occured or something else otherwise.

  const char *_tag_ Set to the name of the element tag that triggered this function call.

  int _type_ Set to XMLPARSE_TAG_START at the start of a tag and/or XMLPARSE_TAG_END at the end of a tag.

  const char *ref The contents of the 'ref' attribute (or NULL if not defined).
  ++++++++++++++++++++++++++++++++++++++*/

static int ndType_function(const char *_tag_,int _type_,const char *ref)
{
 printf("    <%s%s",(_type_==XMLPARSE_TAG_END)?"/":"",_tag_);
 if(ref) printf(" ref='%s'",ParseXML_Encode_Safe_XML(ref));
 printf("%s>\n",(_type_==(XMLPARSE_TAG_START|XMLPARSE_TAG_END))?" /":"");
 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  The function that is called when the memberType XSD type is seen

  int memberType_function Returns 0 if no error occured or something else otherwise.

  const char *_tag_ Set to the name of the element tag that triggered this function call.

  int _type_ Set to XMLPARSE_TAG_START at the start of a tag and/or XMLPARSE_TAG_END at the end of a tag.

  const char *type The contents of the 'type' attribute (or NULL if not defined).

  const char *ref The contents of the 'ref' attribute (or NULL if not defined).

  const char *role The contents of the 'role' attribute (or NULL if not defined).
  ++++++++++++++++++++++++++++++++++++++*/

static int memberType_function(const char *_tag_,int _type_,const char *type,const char *ref,const char *role)
{
 printf("    <%s%s",(_type_==XMLPARSE_TAG_END)?"/":"",_tag_);
 if(type) printf(" type='%s'",ParseXML_Encode_Safe_XML(type));
 if(ref) printf(" ref='%s'",ParseXML_Encode_Safe_XML(ref));
 if(role) printf(" role='%s'",ParseXML_Encode_Safe_XML(role));
 printf("%s>\n",(_type_==(XMLPARSE_TAG_START|XMLPARSE_TAG_END))?" /":"");
 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  The function that is called when the wayType XSD type is seen

  int wayType_function Returns 0 if no error occured or something else otherwise.

  const char *_tag_ Set to the name of the element tag that triggered this function call.

  int _type_ Set to XMLPARSE_TAG_START at the start of a tag and/or XMLPARSE_TAG_END at the end of a tag.

  const char *id The contents of the 'id' attribute (or NULL if not defined).

  const char *timestamp The contents of the 'timestamp' attribute (or NULL if not defined).

  const char *uid The contents of the 'uid' attribute (or NULL if not defined).

  const char *user The contents of the 'user' attribute (or NULL if not defined).

  const char *visible The contents of the 'visible' attribute (or NULL if not defined).

  const char *version The contents of the 'version' attribute (or NULL if not defined).

  const char *action The contents of the 'action' attribute (or NULL if not defined).
  ++++++++++++++++++++++++++++++++++++++*/

static int wayType_function(const char *_tag_,int _type_,const char *id,const char *timestamp,const char *uid,const char *user,const char *visible,const char *version,const char *action)
{
 static int64_t llid;

 if(_type_&XMLPARSE_TAG_START)
   {
    nways++;

    if(!(nways%1000))
       fprintf_middle(stderr,"Reading: Lines=%"PRIu64" Nodes=%"PRIu64" Ways=%"PRIu64" Relations=%"PRIu64,ParseXML_LineNumber(),nnodes,nways,nrelations);

    current_tags=NewTagList();

    /* Handle the way information */

    XMLPARSE_ASSERT_INTEGER(_tag_,id); llid=atoll(id); /* need int64_t conversion */
   }

 if(_type_&XMLPARSE_TAG_END)
   {
    TagList *result=ApplyWayTaggingRules(current_tags,llid);
    int i;

    for(i=0;i<result->ntags;i++)
      {
       printf("    <tag");
       printf(" k='%s'",ParseXML_Encode_Safe_XML(result->k[i]));
       printf(" v='%s'",ParseXML_Encode_Safe_XML(result->v[i]));
       printf(" />\n");
      }

    DeleteTagList(current_tags);
    DeleteTagList(result);
   }

 printf("  <%s%s",(_type_==XMLPARSE_TAG_END)?"/":"",_tag_);
 if(id) printf(" id='%s'",ParseXML_Encode_Safe_XML(id));
 if(timestamp) printf(" timestamp='%s'",ParseXML_Encode_Safe_XML(timestamp));
 if(uid) printf(" uid='%s'",ParseXML_Encode_Safe_XML(uid));
 if(user) printf(" user='%s'",ParseXML_Encode_Safe_XML(user));
 if(visible) printf(" visible='%s'",ParseXML_Encode_Safe_XML(visible));
 if(version) printf(" version='%s'",ParseXML_Encode_Safe_XML(version));
 if(action) printf(" action='%s'",ParseXML_Encode_Safe_XML(action));
 printf("%s>\n",(_type_==(XMLPARSE_TAG_START|XMLPARSE_TAG_END))?" /":"");
 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  The function that is called when the relationType XSD type is seen

  int relationType_function Returns 0 if no error occured or something else otherwise.

  const char *_tag_ Set to the name of the element tag that triggered this function call.

  int _type_ Set to XMLPARSE_TAG_START at the start of a tag and/or XMLPARSE_TAG_END at the end of a tag.

  const char *id The contents of the 'id' attribute (or NULL if not defined).

  const char *timestamp The contents of the 'timestamp' attribute (or NULL if not defined).

  const char *uid The contents of the 'uid' attribute (or NULL if not defined).

  const char *user The contents of the 'user' attribute (or NULL if not defined).

  const char *visible The contents of the 'visible' attribute (or NULL if not defined).

  const char *version The contents of the 'version' attribute (or NULL if not defined).

  const char *action The contents of the 'action' attribute (or NULL if not defined).
  ++++++++++++++++++++++++++++++++++++++*/

static int relationType_function(const char *_tag_,int _type_,const char *id,const char *timestamp,const char *uid,const char *user,const char *visible,const char *version,const char *action)
{
 static int64_t llid;

 if(_type_&XMLPARSE_TAG_START)
   {
    nrelations++;

    if(!(nrelations%1000))
       fprintf_middle(stderr,"Reading: Lines=%"PRIu64" Nodes=%"PRIu64" Ways=%"PRIu64" Relations=%"PRIu64,ParseXML_LineNumber(),nnodes,nways,nrelations);

    current_tags=NewTagList();

    /* Handle the relation information */

    XMLPARSE_ASSERT_INTEGER(_tag_,id); llid=atoll(id); /* need int64_t conversion */
   }

 if(_type_&XMLPARSE_TAG_END)
   {
    TagList *result=ApplyRelationTaggingRules(current_tags,llid);
    int i;

    for(i=0;i<result->ntags;i++)
      {
       printf("    <tag");
       printf(" k='%s'",ParseXML_Encode_Safe_XML(result->k[i]));
       printf(" v='%s'",ParseXML_Encode_Safe_XML(result->v[i]));
       printf(" />\n");
      }

    DeleteTagList(current_tags);
    DeleteTagList(result);
   }

 printf("  <%s%s",(_type_==XMLPARSE_TAG_END)?"/":"",_tag_);
 if(id) printf(" id='%s'",ParseXML_Encode_Safe_XML(id));
 if(timestamp) printf(" timestamp='%s'",ParseXML_Encode_Safe_XML(timestamp));
 if(uid) printf(" uid='%s'",ParseXML_Encode_Safe_XML(uid));
 if(user) printf(" user='%s'",ParseXML_Encode_Safe_XML(user));
 if(visible) printf(" visible='%s'",ParseXML_Encode_Safe_XML(visible));
 if(version) printf(" version='%s'",ParseXML_Encode_Safe_XML(version));
 if(action) printf(" action='%s'",ParseXML_Encode_Safe_XML(action));
 printf("%s>\n",(_type_==(XMLPARSE_TAG_START|XMLPARSE_TAG_END))?" /":"");
 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  The function that is called when the osmType XSD type is seen

  int osmType_function Returns 0 if no error occured or something else otherwise.

  const char *_tag_ Set to the name of the element tag that triggered this function call.

  int _type_ Set to XMLPARSE_TAG_START at the start of a tag and/or XMLPARSE_TAG_END at the end of a tag.

  const char *version The contents of the 'version' attribute (or NULL if not defined).

  const char *generator The contents of the 'generator' attribute (or NULL if not defined).
  ++++++++++++++++++++++++++++++++++++++*/

static int osmType_function(const char *_tag_,int _type_,const char *version,const char *generator)
{
 printf("<%s%s",(_type_==XMLPARSE_TAG_END)?"/":"",_tag_);
 if(version) printf(" version='%s'",ParseXML_Encode_Safe_XML(version));
 if(generator) printf(" generator='%s'",ParseXML_Encode_Safe_XML(generator));
 printf("%s>\n",(_type_==(XMLPARSE_TAG_START|XMLPARSE_TAG_END))?" /":"");
 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  The function that is called when the XML declaration is seen

  int xmlDeclaration_function Returns 0 if no error occured or something else otherwise.

  const char *_tag_ Set to the name of the element tag that triggered this function call.

  int _type_ Set to XMLPARSE_TAG_START at the start of a tag and/or XMLPARSE_TAG_END at the end of a tag.

  const char *version The contents of the 'version' attribute (or NULL if not defined).

  const char *encoding The contents of the 'encoding' attribute (or NULL if not defined).
  ++++++++++++++++++++++++++++++++++++++*/

static int xmlDeclaration_function(const char *_tag_,int _type_,const char *version,const char *encoding)
{
 printf("<?%s",_tag_);
 if(version) printf(" version='%s'",ParseXML_Encode_Safe_XML(version));
 if(encoding) printf(" encoding='%s'",ParseXML_Encode_Safe_XML(encoding));
 printf(" ?>\n");
 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  The main program for the tagmodifier.
  ++++++++++++++++++++++++++++++++++++++*/

int main(int argc,char **argv)
{
 char *tagging=NULL,*filename=NULL,*errorlog=NULL;
 char *p;
 int fd;
 int arg,retval;

 /* Parse the command line arguments */

 for(arg=1;arg<argc;arg++)
   {
    if(!strcmp(argv[arg],"--help"))
       print_usage(1,NULL,NULL);
    else if(!strncmp(argv[arg],"--tagging=",10))
       tagging=&argv[arg][10];
    else if(!strcmp(argv[arg],"--loggable"))
       option_loggable=1;
    else if(!strcmp(argv[arg],"--logtime"))
       option_logtime=1;
    else if(!strcmp(argv[arg],"--errorlog"))
       errorlog="error.log";
    else if(!strncmp(argv[arg],"--errorlog=",11))
       errorlog=&argv[arg][11];
    else if(argv[arg][0]=='-' && argv[arg][1]=='-')
       print_usage(0,argv[arg],NULL);
    else if(filename)
       print_usage(0,argv[arg],"Only one file name can be specified on the command line.");
    else
       filename=argv[arg];
   }

 /* Check the specified command line options */

 if(!filename)
    print_usage(0,NULL,"A single file name must be specified on the command line.");

 if(tagging)
   {
    if(!ExistsFile(tagging))
      {
       fprintf(stderr,"Error: The '--tagging' option specifies a file that does not exist.\n");
       return(1);
      }
   }
 else
   {
    if(ExistsFile("tagging.xml"))
       tagging="tagging.xml";
    else
      {
       fprintf(stderr,"Error: The '--tagging' option was not used and the default 'tagging.xml' does not exist.\n");
       return(1);
      }
   }

 if(ParseXMLTaggingRules(tagging))
   {
    fprintf(stderr,"Error: Cannot read the tagging rules in the file '%s'.\n",tagging);
    return(1);
   }

 /* Open the input file */

 fd=ReOpenFile(filename);

 if((p=strstr(filename,".bz2")) && !strcmp(p,".bz2"))
    fd=Uncompress_Bzip2(fd);

 if((p=strstr(filename,".gz")) && !strcmp(p,".gz"))
    fd=Uncompress_Gzip(fd);

 /* Create the error log file */

 if(errorlog)
    open_errorlog(errorlog,0);

 /* Parse the file */

 fprintf_first(stderr,"Reading: Lines=0 Nodes=0 Ways=0 Relations=0");

 retval=ParseXML(fd,xml_toplevel_tags,XMLPARSE_UNKNOWN_ATTR_IGNORE);

 fprintf_last(stderr,"Read: Lines=%"PRIu64" Nodes=%"PRIu64" Ways=%"PRIu64" Relations=%"PRIu64,ParseXML_LineNumber(),nnodes,nways,nrelations);

 /* Close the error log file */

 if(errorlog)
    close_errorlog();

 /* Tidy up */

 CloseFile(fd);

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Print out the usage information.

  int detail The level of detail to use - 0 = low, 1 = high.

  const char *argerr The argument that gave the error (if there is one).

  const char *err Other error message (if there is one).
  ++++++++++++++++++++++++++++++++++++++*/

static void print_usage(int detail,const char *argerr,const char *err)
{
 fprintf(stderr,
         "Usage: tagmodifier [--help]\n"
         "                   [--tagging=<filename>]\n"
         "                   [--loggable] [--logtime]\n"
         "                   [--errorlog[=<name>]]\n"
         "                   <filename.osm>"
#if defined(USE_BZIP2) && USE_BZIP2
         " | <filename.osm.bz2>"
#endif
#if defined(USE_GZIP) && USE_GZIP
         " | <filename.osm.gz>"
#endif
         "\n");

 if(argerr)
    fprintf(stderr,
            "\n"
            "Error with command line parameter: %s\n",argerr);

 if(err)
    fprintf(stderr,
            "\n"
            "Error: %s\n",err);

 if(detail)
    fprintf(stderr,
            "\n"
            "--help                    Prints this information.\n"
            "\n"
            "--tagging=<filename>      The name of the XML file containing the tagging rules\n"
            "                          (defaults to 'tagging.xml' in current directory).\n"
            "\n"
            "--loggable                Print progress messages suitable for logging to file.\n"
            "--logtime                 Print the elapsed time for the processing.\n"
            "--errorlog[=<name>]       Log parsing errors to 'error.log' or the given name.\n"
            "\n"
            "<filename.osm>            The name of the file to process.\n"
#if defined(USE_BZIP2) && USE_BZIP2
            "                          Filenames ending '.bz2' will be bzip2 uncompressed.\n"
#endif
#if defined(USE_GZIP) && USE_GZIP
            "                          Filenames ending '.gz' will be gzip uncompressed.\n"
#endif
            );

 exit(!detail);
}
