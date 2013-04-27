/***************************************
 OSM XML file parser (either JOSM or planet)

 Part of the Routino routing software.
 ******************/ /******************
 This file Copyright 2008-2012 Andrew M. Bishop

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


#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>

#include "osmparser.h"
#include "xmlparse.h"
#include "tagging.h"
#include "logging.h"


/* Local variables */

static int current_mode=MODE_NORMAL;

static uint64_t nnodes=0,nways=0,nrelations=0;

static TagList *current_tags=NULL;


/* The XML tag processing function prototypes */

//static int xmlDeclaration_function(const char *_tag_,int _type_,const char *version,const char *encoding);
static int osmType_function(const char *_tag_,int _type_,const char *version);
static int osmChangeType_function(const char *_tag_,int _type_,const char *version);
//static int boundsType_function(const char *_tag_,int _type_);
//static int boundType_function(const char *_tag_,int _type_);
static int changesetType_function(const char *_tag_,int _type_);
static int modifyType_function(const char *_tag_,int _type_);
static int createType_function(const char *_tag_,int _type_);
static int deleteType_function(const char *_tag_,int _type_);
static int nodeType_function(const char *_tag_,int _type_,const char *id,const char *lat,const char *lon);
static int wayType_function(const char *_tag_,int _type_,const char *id);
static int relationType_function(const char *_tag_,int _type_,const char *id);
static int tagType_function(const char *_tag_,int _type_,const char *k,const char *v);
static int ndType_function(const char *_tag_,int _type_,const char *ref);
static int memberType_function(const char *_tag_,int _type_,const char *type,const char *ref,const char *role);


/* The XML tag definitions (forward declarations) */

static xmltag xmlDeclaration_tag;
static xmltag osmType_tag;
static xmltag osmChangeType_tag;
static xmltag boundsType_tag;
static xmltag boundType_tag;
static xmltag changesetType_tag;
static xmltag modifyType_tag;
static xmltag createType_tag;
static xmltag deleteType_tag;
static xmltag nodeType_tag;
static xmltag wayType_tag;
static xmltag relationType_tag;
static xmltag tagType_tag;
static xmltag ndType_tag;
static xmltag memberType_tag;


/* The XML tag definition values */

/*+ The complete set of tags at the top level for OSM. +*/
xmltag *xml_osm_toplevel_tags[]={&xmlDeclaration_tag,&osmType_tag,NULL};

/*+ The complete set of tags at the top level for OSC. +*/
xmltag *xml_osc_toplevel_tags[]={&xmlDeclaration_tag,&osmChangeType_tag,NULL};

/*+ The xmlDeclaration type tag. +*/
static xmltag xmlDeclaration_tag=
              {"xml",
               2, {"version","encoding"},
               NULL,
               {NULL}};

/*+ The osmType type tag. +*/
static xmltag osmType_tag=
              {"osm",
               1, {"version"},
               osmType_function,
               {&boundsType_tag,&boundType_tag,&changesetType_tag,&nodeType_tag,&wayType_tag,&relationType_tag,NULL}};

/*+ The osmChangeType type tag. +*/
static xmltag osmChangeType_tag=
              {"osmChange",
               1, {"version"},
               osmChangeType_function,
               {&boundsType_tag,&modifyType_tag,&createType_tag,&deleteType_tag,NULL}};

/*+ The boundsType type tag. +*/
static xmltag boundsType_tag=
              {"bounds",
               0, {NULL},
               NULL,
               {NULL}};

/*+ The boundType type tag. +*/
static xmltag boundType_tag=
              {"bound",
               0, {NULL},
               NULL,
               {NULL}};

/*+ The changesetType type tag. +*/
static xmltag changesetType_tag=
              {"changeset",
               0, {NULL},
               changesetType_function,
               {&tagType_tag,NULL}};

/*+ The modifyType type tag. +*/
static xmltag modifyType_tag=
              {"modify",
               0, {NULL},
               modifyType_function,
               {&nodeType_tag,&wayType_tag,&relationType_tag,NULL}};

/*+ The createType type tag. +*/
static xmltag createType_tag=
              {"create",
               0, {NULL},
               createType_function,
               {&nodeType_tag,&wayType_tag,&relationType_tag,NULL}};

/*+ The deleteType type tag. +*/
static xmltag deleteType_tag=
              {"delete",
               0, {NULL},
               deleteType_function,
               {&nodeType_tag,&wayType_tag,&relationType_tag,NULL}};

/*+ The nodeType type tag. +*/
static xmltag nodeType_tag=
              {"node",
               3, {"id","lat","lon"},
               nodeType_function,
               {&tagType_tag,NULL}};

/*+ The wayType type tag. +*/
static xmltag wayType_tag=
              {"way",
               1, {"id"},
               wayType_function,
               {&ndType_tag,&tagType_tag,NULL}};

/*+ The relationType type tag. +*/
static xmltag relationType_tag=
              {"relation",
               1, {"id"},
               relationType_function,
               {&memberType_tag,&tagType_tag,NULL}};

/*+ The tagType type tag. +*/
static xmltag tagType_tag=
              {"tag",
               2, {"k","v"},
               tagType_function,
               {NULL}};

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


/* The XML tag processing functions */


/*++++++++++++++++++++++++++++++++++++++
  The function that is called when the XML declaration is seen

  int xmlDeclaration_function Returns 0 if no error occured or something else otherwise.

  const char *_tag_ Set to the name of the element tag that triggered this function call.

  int _type_ Set to XMLPARSE_TAG_START at the start of a tag and/or XMLPARSE_TAG_END at the end of a tag.

  const char *version The contents of the 'version' attribute (or NULL if not defined).

  const char *encoding The contents of the 'encoding' attribute (or NULL if not defined).
  ++++++++++++++++++++++++++++++++++++++*/

//static int xmlDeclaration_function(const char *_tag_,int _type_,const char *version,const char *encoding)
//{
// return(0);
//}


/*++++++++++++++++++++++++++++++++++++++
  The function that is called when the osmType XSD type is seen

  int osmType_function Returns 0 if no error occured or something else otherwise.

  const char *_tag_ Set to the name of the element tag that triggered this function call.

  int _type_ Set to XMLPARSE_TAG_START at the start of a tag and/or XMLPARSE_TAG_END at the end of a tag.

  const char *version The contents of the 'version' attribute (or NULL if not defined).
  ++++++++++++++++++++++++++++++++++++++*/

static int osmType_function(const char *_tag_,int _type_,const char *version)
{
 /* Print the initial message */

 if(_type_&XMLPARSE_TAG_START)
    printf_first("Read: Lines=%"PRIu64" Nodes=%"PRIu64" Ways=%"PRIu64" Relations=%"PRIu64,ParseXML_LineNumber(),nnodes=0,nways=0,nrelations=0);

 /* Check the tag values */

 if(_type_&XMLPARSE_TAG_START)
   {
    current_mode=MODE_NORMAL;

    if(!version || strcmp(version,"0.6"))
       XMLPARSE_MESSAGE(_tag_,"Invalid value for 'version' (only '0.6' accepted)");
   }

 /* Print the final message */

 if(_type_&XMLPARSE_TAG_END)
    printf_last("Read: Lines=%"PRIu64" Nodes=%"PRIu64" Ways=%"PRIu64" Relations=%"PRIu64,ParseXML_LineNumber(),nnodes,nways,nrelations);

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  The function that is called when the osmChangeType XSD type is seen

  int osmChangeType_function Returns 0 if no error occured or something else otherwise.

  const char *_tag_ Set to the name of the element tag that triggered this function call.

  int _type_ Set to XMLPARSE_TAG_START at the start of a tag and/or XMLPARSE_TAG_END at the end of a tag.

  const char *version The contents of the 'version' attribute (or NULL if not defined).
  ++++++++++++++++++++++++++++++++++++++*/

static int osmChangeType_function(const char *_tag_,int _type_,const char *version)
{
 /* Print the initial message */

 if(_type_&XMLPARSE_TAG_START)
    printf_first("Read: Lines=%"PRIu64" Nodes=%"PRIu64" Ways=%"PRIu64" Relations=%"PRIu64,ParseXML_LineNumber(),nnodes=0,nways=0,nrelations=0);

 /* Check the tag values */

 if(_type_&XMLPARSE_TAG_START)
   {
    if(!version || strcmp(version,"0.6"))
       XMLPARSE_MESSAGE(_tag_,"Invalid value for 'version' (only '0.6' accepted)");
   }

 /* Print the final message */

 if(_type_&XMLPARSE_TAG_END)
    printf_last("Read: Lines=%"PRIu64" Nodes=%"PRIu64" Ways=%"PRIu64" Relations=%"PRIu64,ParseXML_LineNumber(),nnodes,nways,nrelations);

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  The function that is called when the boundsType XSD type is seen

  int boundsType_function Returns 0 if no error occured or something else otherwise.

  const char *_tag_ Set to the name of the element tag that triggered this function call.

  int _type_ Set to XMLPARSE_TAG_START at the start of a tag and/or XMLPARSE_TAG_END at the end of a tag.
  ++++++++++++++++++++++++++++++++++++++*/

//static int boundsType_function(const char *_tag_,int _type_)
//{
// return(0);
//}


/*++++++++++++++++++++++++++++++++++++++
  The function that is called when the boundType XSD type is seen

  int boundType_function Returns 0 if no error occured or something else otherwise.

  const char *_tag_ Set to the name of the element tag that triggered this function call.

  int _type_ Set to XMLPARSE_TAG_START at the start of a tag and/or XMLPARSE_TAG_END at the end of a tag.
  ++++++++++++++++++++++++++++++++++++++*/

//static int boundType_function(const char *_tag_,int _type_)
//{
// return(0);
//}


/*++++++++++++++++++++++++++++++++++++++
  The function that is called when the changesetType XSD type is seen

  int changesetType_function Returns 0 if no error occured or something else otherwise.

  const char *_tag_ Set to the name of the element tag that triggered this function call.

  int _type_ Set to XMLPARSE_TAG_START at the start of a tag and/or XMLPARSE_TAG_END at the end of a tag.
  ++++++++++++++++++++++++++++++++++++++*/

static int changesetType_function(const char *_tag_,int _type_)
{
 current_tags=NULL;

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  The function that is called when the modifyType XSD type is seen

  int modifyType_function Returns 0 if no error occured or something else otherwise.

  const char *_tag_ Set to the name of the element tag that triggered this function call.

  int _type_ Set to XMLPARSE_TAG_START at the start of a tag and/or XMLPARSE_TAG_END at the end of a tag.
  ++++++++++++++++++++++++++++++++++++++*/

static int modifyType_function(const char *_tag_,int _type_)
{
 if(_type_&XMLPARSE_TAG_START)
    current_mode=MODE_MODIFY;

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  The function that is called when the createType XSD type is seen

  int createType_function Returns 0 if no error occured or something else otherwise.

  const char *_tag_ Set to the name of the element tag that triggered this function call.

  int _type_ Set to XMLPARSE_TAG_START at the start of a tag and/or XMLPARSE_TAG_END at the end of a tag.
  ++++++++++++++++++++++++++++++++++++++*/

static int createType_function(const char *_tag_,int _type_)
{
 if(_type_&XMLPARSE_TAG_START)
    current_mode=MODE_CREATE;

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  The function that is called when the deleteType XSD type is seen

  int deleteType_function Returns 0 if no error occured or something else otherwise.

  const char *_tag_ Set to the name of the element tag that triggered this function call.

  int _type_ Set to XMLPARSE_TAG_START at the start of a tag and/or XMLPARSE_TAG_END at the end of a tag.
  ++++++++++++++++++++++++++++++++++++++*/

static int deleteType_function(const char *_tag_,int _type_)
{
 if(_type_&XMLPARSE_TAG_START)
    current_mode=MODE_DELETE;

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
  ++++++++++++++++++++++++++++++++++++++*/

static int nodeType_function(const char *_tag_,int _type_,const char *id,const char *lat,const char *lon)
{
 static int64_t llid;
 static double latitude,longitude;

 if(_type_&XMLPARSE_TAG_START)
   {
    nnodes++;

    if(!(nnodes%10000))
       printf_middle("Reading: Lines=%"PRIu64" Nodes=%"PRIu64" Ways=%"PRIu64" Relations=%"PRIu64,ParseXML_LineNumber(),nnodes,nways,nrelations);

    current_tags=NewTagList();

    /* Handle the node information */

    XMLPARSE_ASSERT_INTEGER(_tag_,id); llid=atoll(id); /* need int64_t conversion */

    if(current_mode!=MODE_DELETE)
      {
       XMLPARSE_ASSERT_FLOATING(_tag_,lat); latitude =atof(lat);
       XMLPARSE_ASSERT_FLOATING(_tag_,lon); longitude=atof(lon);
      }
   }

 if(_type_&XMLPARSE_TAG_END)
   {
    TagList *result=ApplyNodeTaggingRules(current_tags,llid);

    ProcessNodeTags(result,llid,latitude,longitude,current_mode);

    DeleteTagList(current_tags); current_tags=NULL;
    DeleteTagList(result);
   }

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  The function that is called when the wayType XSD type is seen

  int wayType_function Returns 0 if no error occured or something else otherwise.

  const char *_tag_ Set to the name of the element tag that triggered this function call.

  int _type_ Set to XMLPARSE_TAG_START at the start of a tag and/or XMLPARSE_TAG_END at the end of a tag.

  const char *id The contents of the 'id' attribute (or NULL if not defined).
  ++++++++++++++++++++++++++++++++++++++*/

static int wayType_function(const char *_tag_,int _type_,const char *id)
{
 static int64_t llid;

 if(_type_&XMLPARSE_TAG_START)
   {
    nways++;

    if(!(nways%1000))
       printf_middle("Reading: Lines=%"PRIu64" Nodes=%"PRIu64" Ways=%"PRIu64" Relations=%"PRIu64,ParseXML_LineNumber(),nnodes,nways,nrelations);

    current_tags=NewTagList();

    AddWayRefs(0);

    /* Handle the way information */

    XMLPARSE_ASSERT_INTEGER(_tag_,id); llid=atoll(id); /* need int64_t conversion */
   }

 if(_type_&XMLPARSE_TAG_END)
   {
    TagList *result=ApplyWayTaggingRules(current_tags,llid);

    ProcessWayTags(result,llid,current_mode);

    DeleteTagList(current_tags); current_tags=NULL;
    DeleteTagList(result);
   }

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  The function that is called when the relationType XSD type is seen

  int relationType_function Returns 0 if no error occured or something else otherwise.

  const char *_tag_ Set to the name of the element tag that triggered this function call.

  int _type_ Set to XMLPARSE_TAG_START at the start of a tag and/or XMLPARSE_TAG_END at the end of a tag.

  const char *id The contents of the 'id' attribute (or NULL if not defined).
  ++++++++++++++++++++++++++++++++++++++*/

static int relationType_function(const char *_tag_,int _type_,const char *id)
{
 static int64_t llid;

 if(_type_&XMLPARSE_TAG_START)
   {
    nrelations++;

    if(!(nrelations%1000))
       printf_middle("Reading: Lines=%"PRIu64" Nodes=%"PRIu64" Ways=%"PRIu64" Relations=%"PRIu64,ParseXML_LineNumber(),nnodes,nways,nrelations);

    current_tags=NewTagList();

    AddRelationRefs(0,0,0,NULL);

    /* Handle the relation information */

    XMLPARSE_ASSERT_INTEGER(_tag_,id); llid=atoll(id); /* need int64_t conversion */
   }

 if(_type_&XMLPARSE_TAG_END)
   {
    TagList *result=ApplyRelationTaggingRules(current_tags,llid);

    ProcessRelationTags(result,llid,current_mode);

    DeleteTagList(current_tags); current_tags=NULL;
    DeleteTagList(result);
   }

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
 if(_type_&XMLPARSE_TAG_START && current_tags)
   {
    XMLPARSE_ASSERT_STRING(_tag_,k);
    XMLPARSE_ASSERT_STRING(_tag_,v);

    AppendTag(current_tags,k,v);
   }

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
 if(_type_&XMLPARSE_TAG_START)
   {
    int64_t llid;

    XMLPARSE_ASSERT_INTEGER(_tag_,ref); llid=atoll(ref); /* need int64_t conversion */

    AddWayRefs(llid);
   }

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
 if(_type_&XMLPARSE_TAG_START)
   {
    int64_t llid;

    XMLPARSE_ASSERT_STRING(_tag_,type);
    XMLPARSE_ASSERT_INTEGER(_tag_,ref); llid=atoll(ref); /* need int64_t conversion */

    if(!strcmp(type,"node"))
       AddRelationRefs(llid,0,0,role);
    else if(!strcmp(type,"way"))
       AddRelationRefs(0,llid,0,role);
    else if(!strcmp(type,"relation"))
       AddRelationRefs(0,0,llid,role);
   }

 return(0);
}
