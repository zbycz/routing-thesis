/***************************************
 A simple generic XML parser where the structure comes from the function parameters.
 Not intended to be fully conforming to XML standard or a validating parser but
 sufficient to parse OSM XML and simple program configuration files.

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
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

#include "xmlparse.h"


/* Parser states */

#define LEX_EOF                    0

#define LEX_FUNC_TAG_BEGIN         1
#define LEX_FUNC_XML_DECL_BEGIN    2
#define LEX_FUNC_TAG_POP           3
#define LEX_FUNC_TAG_PUSH          4
#define LEX_FUNC_XML_DECL_FINISH   5
#define LEX_FUNC_TAG_FINISH        6
#define LEX_FUNC_ATTR_KEY          7
#define LEX_FUNC_ATTR_VAL          8

#define LEX_STATE_INITIAL         10
#define LEX_STATE_BANGTAG         11
#define LEX_STATE_COMMENT         12
#define LEX_STATE_XML_DECL_START  13
#define LEX_STATE_XML_DECL        14
#define LEX_STATE_TAG_START       15
#define LEX_STATE_TAG             16
#define LEX_STATE_ATTR_KEY        17
#define LEX_STATE_ATTR_VAL        18
#define LEX_STATE_END_TAG1        19
#define LEX_STATE_END_TAG2        20
#define LEX_STATE_DQUOTED         21
#define LEX_STATE_SQUOTED         22

#define LEX_ERROR_TAG_START      101
#define LEX_ERROR_XML_DECL_START 102
#define LEX_ERROR_TAG            103
#define LEX_ERROR_XML_DECL       104
#define LEX_ERROR_ATTR           105
#define LEX_ERROR_END_TAG        106
#define LEX_ERROR_COMMENT        107
#define LEX_ERROR_CLOSE          108
#define LEX_ERROR_ATTR_VAL       109
#define LEX_ERROR_ENTITY_REF     110
#define LEX_ERROR_CHAR_REF       111
#define LEX_ERROR_TEXT_OUTSIDE   112

#define LEX_ERROR_UNEXP_TAG      201
#define LEX_ERROR_UNBALANCED     202
#define LEX_ERROR_NO_START       203
#define LEX_ERROR_UNEXP_ATT      204
#define LEX_ERROR_UNEXP_EOF      205
#define LEX_ERROR_XML_NOT_FIRST  206

#define LEX_ERROR_OUT_OF_MEMORY  254
#define LEX_ERROR_CALLBACK       255


/* Parsing variables and functions */

static uint64_t lineno;

static unsigned char buffer[2][16384];
static unsigned char *buffer_token,*buffer_end,*buffer_ptr;
static int buffer_active=0;


/*++++++++++++++++++++++++++++++++++++++
  Refill the data buffer making sure that the string starting at buffer_token is contiguous.

  int buffer_refill Return 0 if everything is OK or 1 for EOF.

  int fd The file descriptor to read from.
  ++++++++++++++++++++++++++++++++++++++*/

static inline int buffer_refill(int fd)
{
 ssize_t n,m=0;

 m=(buffer_end-buffer[buffer_active])+1;

 if(m>(sizeof(buffer[0])/2))    /* more than half full */
   {
    m=0;

    buffer_active=!buffer_active;

    if(buffer_token)
      {
       m=(buffer_end-buffer_token)+1;

       memcpy(buffer[buffer_active],buffer_token,m);

       buffer_token=buffer[buffer_active];
      }
   }

 n=read(fd,buffer[buffer_active]+m,sizeof(buffer[0])-m);

 buffer_ptr=buffer[buffer_active]+m;
 buffer_end=buffer[buffer_active]+m+n-1;

 if(n<=0)
    return(1);
 else
    return(0);
}


/* Macros to simplify the parser (and make it look more like lex) */

#define BEGIN(xx) do{ state=(xx); goto new_state; } while(0)
#define NEXT(xx)  next_state=(xx)

#define START_TOKEN buffer_token=buffer_ptr
#define END_TOKEN   buffer_token=NULL

#define NEXT_CHAR                                                       \
 do{                                                                    \
  if(buffer_ptr==buffer_end)                                            \
    { if(buffer_refill(fd)) BEGIN(LEX_EOF); }                           \
    else                                                                \
       buffer_ptr++;                                                    \
   } while(0)


 /* -------- equivalent flex definition --------

    S               [ \t\r]
    N               (\n)

    U1              [\x09\x0A\x0D\x20-\x7F]
    U2              [\xC2-\xDF][\x80-\xBF]
    U3a             \xE0[\xA0-\xBF][\x80-\xBF]
    U3b             [\xE1-\xEC][\x80-\xBF][\x80-\xBF]
    U3c             \xED[\x80-\x9F][\x80-\xBF]
    U3d             [\xEE-\xEF][\x80-\xBF][\x80-\xBF]
    U3              {U3a}|{U3b}|{U3c}|{U3d}
    U4a             \xF0[\x90-\xBF][\x80-\xBF][\x80-\xBF]
    U4b             [\xF1-\xF3][\x80-\xBF][\x80-\xBF][\x80-\xBF]
    U4c             \xF4[\x80-\x8F][\x80-\xBF][\x80-\xBF]
    U4              {U4a}|{U4b}|{U4c}

    U               ({U1}|{U2}|{U3}|{U4})

    U1_xml          ([\x09\x0A\x0D\x20-\x25\x27-\x3B\x3D\x3F-\x7F])

    U1quotedS_xml   ([\x09\x0A\x0D\x20-\x25\x28-\x3B\x3D\x3F-\x7F])
    U1quotedD_xml   ([\x09\x0A\x0D\x20-\x21\x23-\x25\x27-\x3B\x3D\x3F-\x7F])

    UquotedS        ({U1quotedS_xml}|{U2}|{U3}|{U4})
    UquotedD        ({U1quotedD_xml}|{U2}|{U3}|{U4})

    letter          [a-zA-Z]
    digit           [0-9]
    xdigit          [a-fA-F0-9]

    namechar        ({letter}|{digit}|[-._:])
    namestart       ({letter}|[_:])
    name            ({namestart}{namechar}*)

    entityref       (&{name};)
    charref         (&#({digit}+|x{xdigit}+);)

    -------- equivalent flex definition -------- */

/* Tables containing character class defintions (advance declaration for data at end of file). */
static const unsigned char quotedD[256],quotedS[256];
static const unsigned char *U2[1],*U3a[2],*U3b[2],*U3c[2],*U3d[2],*U4a[3],*U4b[3],*U4c[3];
static const unsigned char namestart[256],namechar[256],whitespace[256],digit[256],xdigit[256];


/*++++++++++++++++++++++++++++++++++++++
  A function to call the callback function with the parameters needed.

  int call_callback Returns 1 if the callback returned with an error.

  const char *name The name of the tag.

  int (*callback)() The callback function.

  int type The type of tag (start and/or end).

  int nattributes The number of attributes collected.

  unsigned char *attributes[XMLPARSE_MAX_ATTRS] The list of attributes.
  ++++++++++++++++++++++++++++++++++++++*/

static inline int call_callback(const char *name,int (*callback)(),int type,int nattributes,unsigned char *attributes[XMLPARSE_MAX_ATTRS])
{
 switch(nattributes)
   {
   case  0: return (*callback)(name,type);
   case  1: return (*callback)(name,type,attributes[0]);
   case  2: return (*callback)(name,type,attributes[0],attributes[1]);
   case  3: return (*callback)(name,type,attributes[0],attributes[1],attributes[2]);
   case  4: return (*callback)(name,type,attributes[0],attributes[1],attributes[2],attributes[3]);
   case  5: return (*callback)(name,type,attributes[0],attributes[1],attributes[2],attributes[3],attributes[4]);
   case  6: return (*callback)(name,type,attributes[0],attributes[1],attributes[2],attributes[3],attributes[4],attributes[5]);
   case  7: return (*callback)(name,type,attributes[0],attributes[1],attributes[2],attributes[3],attributes[4],attributes[5],attributes[6]);
   case  8: return (*callback)(name,type,attributes[0],attributes[1],attributes[2],attributes[3],attributes[4],attributes[5],attributes[6],attributes[7]);
   case  9: return (*callback)(name,type,attributes[0],attributes[1],attributes[2],attributes[3],attributes[4],attributes[5],attributes[6],attributes[7],attributes[8]);
   case 10: return (*callback)(name,type,attributes[0],attributes[1],attributes[2],attributes[3],attributes[4],attributes[5],attributes[6],attributes[7],attributes[8],attributes[9]);
   case 11: return (*callback)(name,type,attributes[0],attributes[1],attributes[2],attributes[3],attributes[4],attributes[5],attributes[6],attributes[7],attributes[8],attributes[9],attributes[10]);
   case 12: return (*callback)(name,type,attributes[0],attributes[1],attributes[2],attributes[3],attributes[4],attributes[5],attributes[6],attributes[7],attributes[8],attributes[9],attributes[10],attributes[11]);
   case 13: return (*callback)(name,type,attributes[0],attributes[1],attributes[2],attributes[3],attributes[4],attributes[5],attributes[6],attributes[7],attributes[8],attributes[9],attributes[10],attributes[11],attributes[12]);
   case 14: return (*callback)(name,type,attributes[0],attributes[1],attributes[2],attributes[3],attributes[4],attributes[5],attributes[6],attributes[7],attributes[8],attributes[9],attributes[10],attributes[11],attributes[12],attributes[13]);
   case 15: return (*callback)(name,type,attributes[0],attributes[1],attributes[2],attributes[3],attributes[4],attributes[5],attributes[6],attributes[7],attributes[8],attributes[9],attributes[10],attributes[11],attributes[12],attributes[13],attributes[14]);
   case 16: return (*callback)(name,type,attributes[0],attributes[1],attributes[2],attributes[3],attributes[4],attributes[5],attributes[6],attributes[7],attributes[8],attributes[9],attributes[10],attributes[11],attributes[12],attributes[13],attributes[14],attributes[15]);

   default:
    fprintf(stderr,"XML Parser: Error on line %llu: too many attributes for tag '%s' source code needs changing.\n",lineno,name);
    exit(1);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Parse the XML and call the functions for each tag as seen.

  int ParseXML Returns 0 if OK or something else in case of an error.

  in fd The file descriptor of the file to parse.

  xmltag **tags The array of pointers to tags for the top level.

  int options A list of XML Parser options OR-ed together.
  ++++++++++++++++++++++++++++++++++++++*/

int ParseXML(int fd,xmltag **tags,int options)
{
 int i;
 int state,next_state,after_attr;
 unsigned char saved_buffer_ptr=0;
 const unsigned char *quoted;

 unsigned char *attributes[XMLPARSE_MAX_ATTRS]={NULL};
 int attribute=0;

 int stackdepth=0,stackused=0;
 xmltag ***tags_stack=NULL;
 xmltag **tag_stack=NULL;
 xmltag *tag=NULL;

 /* The actual parser. */

 lineno=1;

 buffer_end=buffer[buffer_active]+sizeof(buffer[0])-1;
 buffer_token=NULL;

 buffer_refill(fd);

 BEGIN(LEX_STATE_INITIAL);

 new_state:

 switch(state)
   {
    /* ================ Parsing states ================ */


    /* -------- equivalent flex definition --------

       <INITIAL>"<!"                        { BEGIN(BANGTAG); }
       <INITIAL>"</"                        { BEGIN(END_TAG1); }
       <INITIAL>"<?"                        { BEGIN(XML_DECL_START); }
       <INITIAL>"<"                         { BEGIN(TAG_START); }

       <INITIAL>">"                         { return(LEX_ERROR_CLOSE); }

       <INITIAL>{N}                         { lineno++; }
       <INITIAL>{S}+                        { }
       <INITIAL>.                           { return(LEX_ERROR_TEXT_OUTSIDE); }

       -------- equivalent flex definition -------- */

   case LEX_STATE_INITIAL:

    while(1)
      {
       while(whitespace[(int)*buffer_ptr])
          NEXT_CHAR;

       if(*buffer_ptr=='\n')
         {
          NEXT_CHAR;

          lineno++;
         }
       else if(*buffer_ptr=='<')
         {
          NEXT_CHAR;

          if(*buffer_ptr=='/')
            {
             NEXT_CHAR;
             BEGIN(LEX_STATE_END_TAG1);
            }
          else if(*buffer_ptr=='!')
            {
             NEXT_CHAR;
             BEGIN(LEX_STATE_BANGTAG);
            }
          else if(*buffer_ptr=='?')
            {
             NEXT_CHAR;
             BEGIN(LEX_STATE_XML_DECL_START);
            }
          else
             BEGIN(LEX_STATE_TAG_START);
         }
       else if(*buffer_ptr=='>')
          BEGIN(LEX_ERROR_CLOSE);
       else
          BEGIN(LEX_ERROR_TEXT_OUTSIDE);
      }

    break;

    /* -------- equivalent flex definition --------

       <BANGTAG>"--"               { BEGIN(COMMENT); }
       <BANGTAG>{N}                { return(LEX_ERROR_TAG_START); }
       <BANGTAG>.                  { return(LEX_ERROR_TAG_START); }

       -------- equivalent flex definition -------- */

   case LEX_STATE_BANGTAG:

    if(*buffer_ptr!='-')
       BEGIN(LEX_ERROR_TAG_START);

    NEXT_CHAR;

    if(*buffer_ptr!='-')
       BEGIN(LEX_ERROR_TAG_START);

    NEXT_CHAR;
    BEGIN(LEX_STATE_COMMENT);

    break;

    /* -------- equivalent flex definition --------

       <COMMENT>"-->"              { BEGIN(INITIAL); }
       <COMMENT>"--"[^>]           { return(LEX_ERROR_COMMENT); }
       <COMMENT>"-"                { }
       <COMMENT>{N}                { lineno++; }
       <COMMENT>[^-\n]+            { }

       -------- equivalent flex definition -------- */

   case LEX_STATE_COMMENT:

    while(1)
      {
       while(*buffer_ptr!='-' && *buffer_ptr!='\n')
          NEXT_CHAR;

       if(*buffer_ptr=='-')
         {
          NEXT_CHAR;

          if(*buffer_ptr!='-')
             continue;

          NEXT_CHAR;
          if(*buffer_ptr=='>')
            {
             NEXT_CHAR;
             BEGIN(LEX_STATE_INITIAL);
            }

          BEGIN(LEX_ERROR_COMMENT);
         }
       else /* if(*buffer_ptr=='\n') */
         {
          NEXT_CHAR;

          lineno++;
         }
      }

    break;

    /* -------- equivalent flex definition --------

       <XML_DECL_START>xml         { BEGIN(XML_DECL); return(LEX_XML_DECL_BEGIN); }
       <XML_DECL_START>{N}         { return(LEX_ERROR_XML_DECL_START); }
       <XML_DECL_START>.           { return(LEX_ERROR_XML_DECL_START); }

       -------- equivalent flex definition -------- */

   case LEX_STATE_XML_DECL_START:

    START_TOKEN;

    if(*buffer_ptr=='x')
      {
       NEXT_CHAR;
       if(*buffer_ptr=='m')
         {
          NEXT_CHAR;
          if(*buffer_ptr=='l')
            {
             NEXT_CHAR;

             saved_buffer_ptr=*buffer_ptr;
             *buffer_ptr=0;

             NEXT(LEX_STATE_XML_DECL);
             BEGIN(LEX_FUNC_XML_DECL_BEGIN);
            }
         }
      }

    BEGIN(LEX_ERROR_XML_DECL_START);

    /* -------- equivalent flex definition --------

       <XML_DECL>"?>"              { BEGIN(INITIAL); return(LEX_XML_DECL_FINISH); }
       <XML_DECL>{S}+              { }
       <XML_DECL>{N}               { lineno++; }
       <XML_DECL>{name}            { after_attr=XML_DECL; BEGIN(ATTR_KEY); return(LEX_ATTR_KEY); }
       <XML_DECL>.                 { return(LEX_ERROR_XML_DECL); }

       -------- equivalent flex definition -------- */

   case LEX_STATE_XML_DECL:

    while(1)
      {
       while(whitespace[(int)*buffer_ptr])
          NEXT_CHAR;

       if(namestart[(int)*buffer_ptr])
         {
          START_TOKEN;

          NEXT_CHAR;
          while(namechar[(int)*buffer_ptr])
             NEXT_CHAR;

          saved_buffer_ptr=*buffer_ptr;
          *buffer_ptr=0;

          after_attr=LEX_STATE_XML_DECL;
          NEXT(LEX_STATE_ATTR_KEY);
          BEGIN(LEX_FUNC_ATTR_KEY);
         }
       else if(*buffer_ptr=='?')
         {
          NEXT_CHAR;
          if(*buffer_ptr=='>')
            {
             NEXT_CHAR;
             NEXT(LEX_STATE_INITIAL);
             BEGIN(LEX_FUNC_XML_DECL_FINISH);
            }

          BEGIN(LEX_ERROR_XML_DECL);
         }
       else if(*buffer_ptr=='\n')
         {
          NEXT_CHAR;
          lineno++;
         }
       else
          BEGIN(LEX_ERROR_XML_DECL);
      }

    break;

    /* -------- equivalent flex definition --------

       <TAG_START>{name}           { BEGIN(TAG); return(LEX_TAG_BEGIN); }
       <TAG_START>{N}              { return(LEX_ERROR_TAG_START); }
       <TAG_START>.                { return(LEX_ERROR_TAG_START); }

       -------- equivalent flex definition -------- */

   case LEX_STATE_TAG_START:

    if(namestart[(int)*buffer_ptr])
      {
       START_TOKEN;

       NEXT_CHAR;
       while(namechar[(int)*buffer_ptr])
          NEXT_CHAR;

       saved_buffer_ptr=*buffer_ptr;
       *buffer_ptr=0;

       NEXT(LEX_STATE_TAG);
       BEGIN(LEX_FUNC_TAG_BEGIN);
      }

    BEGIN(LEX_ERROR_TAG_START);

    /* -------- equivalent flex definition --------

       <END_TAG1>{name}            { BEGIN(END_TAG2); return(LEX_TAG_POP); }
       <END_TAG1>{N}               { return(LEX_ERROR_END_TAG); }
       <END_TAG1>.                 { return(LEX_ERROR_END_TAG); }

       -------- equivalent flex definition -------- */

   case LEX_STATE_END_TAG1:

    if(namestart[(int)*buffer_ptr])
      {
       START_TOKEN;

       NEXT_CHAR;
       while(namechar[(int)*buffer_ptr])
          NEXT_CHAR;

       saved_buffer_ptr=*buffer_ptr;
       *buffer_ptr=0;

       NEXT(LEX_STATE_END_TAG2);
       BEGIN(LEX_FUNC_TAG_POP);
      }

    BEGIN(LEX_ERROR_END_TAG);

    /* -------- equivalent flex definition --------

       <END_TAG2>">"               { BEGIN(INITIAL); }
       <END_TAG2>{N}               { return(LEX_ERROR_END_TAG); }
       <END_TAG2>.                 { return(LEX_ERROR_END_TAG); }

       -------- equivalent flex definition -------- */

   case LEX_STATE_END_TAG2:

    if(*buffer_ptr=='>')
      {
       NEXT_CHAR;

       BEGIN(LEX_STATE_INITIAL);
      }

    BEGIN(LEX_ERROR_END_TAG);

    /* -------- equivalent flex definition --------

       <TAG>"/>"                   { BEGIN(INITIAL); return(LEX_TAG_FINISH); }
       <TAG>">"                    { BEGIN(INITIAL); return(LEX_TAG_PUSH); }
       <TAG>{S}+                   { }
       <TAG>{N}                    { lineno++; }
       <TAG>{name}                 { after_attr=TAG; BEGIN(ATTR_KEY); return(LEX_ATTR_KEY); }
       <TAG>.                      { return(LEX_ERROR_TAG); }

       -------- equivalent flex definition -------- */

   case LEX_STATE_TAG:

    while(1)
      {
       while(whitespace[(int)*buffer_ptr])
          NEXT_CHAR;

       if(namestart[(int)*buffer_ptr])
         {
          START_TOKEN;

          NEXT_CHAR;
          while(namechar[(int)*buffer_ptr])
             NEXT_CHAR;

          saved_buffer_ptr=*buffer_ptr;
          *buffer_ptr=0;

          after_attr=LEX_STATE_TAG;
          NEXT(LEX_STATE_ATTR_KEY);
          BEGIN(LEX_FUNC_ATTR_KEY);
         }
       else if(*buffer_ptr=='/')
         {
          NEXT_CHAR;
          if(*buffer_ptr=='>')
            {
             NEXT_CHAR;
             NEXT(LEX_STATE_INITIAL);
             BEGIN(LEX_FUNC_TAG_FINISH);
            }

          BEGIN(LEX_ERROR_TAG);
         }
       else if(*buffer_ptr=='>')
         {
          NEXT_CHAR;
          NEXT(LEX_STATE_INITIAL);
          BEGIN(LEX_FUNC_TAG_PUSH);
         }
       else if(*buffer_ptr=='\n')
         {
          NEXT_CHAR;
          lineno++;
         }
       else
          BEGIN(LEX_ERROR_TAG);
      }

    break;

    /* -------- equivalent flex definition --------

       <ATTR_KEY>=                 { BEGIN(ATTR_VAL); }
       <ATTR_KEY>{N}               { return(LEX_ERROR_ATTR); }
       <ATTR_KEY>.                 { return(LEX_ERROR_ATTR); }

       -------- equivalent flex definition -------- */

   case LEX_STATE_ATTR_KEY:

    if(*buffer_ptr=='=')
      {
       NEXT_CHAR;
       BEGIN(LEX_STATE_ATTR_VAL);
      }

    BEGIN(LEX_ERROR_ATTR);

    /* -------- equivalent flex definition --------

       <ATTR_VAL>\"                { BEGIN(DQUOTED); }
       <ATTR_VAL>\'                { BEGIN(SQUOTED); }
       <ATTR_VAL>{N}               { return(LEX_ERROR_ATTR); }
       <ATTR_VAL>.                 { return(LEX_ERROR_ATTR); }

       -------- equivalent flex definition -------- */

   case LEX_STATE_ATTR_VAL:

    if(*buffer_ptr=='"')
      {
       NEXT_CHAR;
       BEGIN(LEX_STATE_DQUOTED);
      }
    else if(*buffer_ptr=='\'')
      {
       NEXT_CHAR;
       BEGIN(LEX_STATE_SQUOTED);
      }

    BEGIN(LEX_ERROR_ATTR);

    /* -------- equivalent flex definition --------

       <DQUOTED>\"                 { BEGIN(after_attr); return(LEX_ATTR_VAL); }
       <DQUOTED>{entityref}        { if(options&XMLPARSE_RETURN_ATTR_ENCODED) {append_string(yytext);}
                                     else { const char *str=ParseXML_Decode_Entity_Ref(yytext); if(str) {append_string(str);} else {return(LEX_ERROR_ENTITY_REF);} } }
       <DQUOTED>{charref}          { if(options&XMLPARSE_RETURN_ATTR_ENCODED) {append_string(yytext);}
                                     else { const char *str=ParseXML_Decode_Char_Ref(yytext);   if(str) {append_string(str);} else {return(LEX_ERROR_CHAR_REF);} } }
       <DQUOTED>{UquotedD}         { }
       <DQUOTED>[<>&]              { return(LEX_ERROR_ATTR_VAL); }
       <DQUOTED>.                  { return(LEX_ERROR_ATTR_VAL); }

       <SQUOTED>\'                 { BEGIN(after_attr); return(LEX_ATTR_VAL); }
       <SQUOTED>{entityref}        { if(options&XMLPARSE_RETURN_ATTR_ENCODED) {append_string(yytext);}
                                     else { const char *str=ParseXML_Decode_Entity_Ref(yytext); if(str) {append_string(str);} else {return(LEX_ERROR_ENTITY_REF);} } }
       <SQUOTED>{charref}          { if(options&XMLPARSE_RETURN_ATTR_ENCODED) {append_string(yytext);}
                                     else { const char *str=ParseXML_Decode_Char_Ref(yytext);   if(str) {append_string(str);} else {return(LEX_ERROR_CHAR_REF);} } }
       <SQUOTED>{UquotedS}         { append_string(yytext); }
       <SQUOTED>[<>&]              { return(LEX_ERROR_ATTR_VAL); }
       <SQUOTED>.                  { return(LEX_ERROR_ATTR_VAL); }

       -------- equivalent flex definition -------- */

   case LEX_STATE_DQUOTED:
   case LEX_STATE_SQUOTED:

    if(state==LEX_STATE_DQUOTED)
       quoted=quotedD;
    else
       quoted=quotedS;

    START_TOKEN;

    while(1)
      {
       switch(quoted[(int)*buffer_ptr])
         {
         case 10:            /* U1 - used by all tag keys and many values */
          do
            {
             NEXT_CHAR;
            }
          while(quoted[(int)*buffer_ptr]==10);
          break;

         case 20:            /* U2 */
          NEXT_CHAR;
          if(!U2[0][(int)*buffer_ptr])
             BEGIN(LEX_ERROR_ATTR_VAL);
          NEXT_CHAR;
          break;

         case 31:            /* U3a */
          NEXT_CHAR;
          if(!U3a[0][(int)*buffer_ptr])
             BEGIN(LEX_ERROR_ATTR_VAL);
          NEXT_CHAR;
          if(!U3a[1][(int)*buffer_ptr])
             BEGIN(LEX_ERROR_ATTR_VAL);
          NEXT_CHAR;
          break;

         case 32:            /* U3b */
          NEXT_CHAR;
          if(!U3b[0][(int)*buffer_ptr])
             BEGIN(LEX_ERROR_ATTR_VAL);
          NEXT_CHAR;
          if(!U3b[1][(int)*buffer_ptr])
             BEGIN(LEX_ERROR_ATTR_VAL);
          NEXT_CHAR;
          break;

         case 33:            /* U3c */
          NEXT_CHAR;
          if(!U3c[0][(int)*buffer_ptr])
             BEGIN(LEX_ERROR_ATTR_VAL);
          NEXT_CHAR;
          if(!U3c[1][(int)*buffer_ptr])
             BEGIN(LEX_ERROR_ATTR_VAL);
          NEXT_CHAR;
          break;

         case 34:            /* U3d */
          NEXT_CHAR;
          if(!U3d[0][(int)*buffer_ptr])
             BEGIN(LEX_ERROR_ATTR_VAL);
          NEXT_CHAR;
          if(!U3d[1][(int)*buffer_ptr])
             BEGIN(LEX_ERROR_ATTR_VAL);
          NEXT_CHAR;
          break;

         case 41:            /* U4a */
          NEXT_CHAR;
          if(!U4a[0][(int)*buffer_ptr])
             BEGIN(LEX_ERROR_ATTR_VAL);
          NEXT_CHAR;
          if(!U4a[1][(int)*buffer_ptr])
             BEGIN(LEX_ERROR_ATTR_VAL);
          NEXT_CHAR;
          if(!U4a[2][(int)*buffer_ptr])
             BEGIN(LEX_ERROR_ATTR_VAL);
          NEXT_CHAR;
          break;

         case 42:            /* U4b */
          NEXT_CHAR;
          if(!U4b[0][(int)*buffer_ptr])
             BEGIN(LEX_ERROR_ATTR_VAL);
          NEXT_CHAR;
          if(!U4b[1][(int)*buffer_ptr])
             BEGIN(LEX_ERROR_ATTR_VAL);
          NEXT_CHAR;
          if(!U4b[2][(int)*buffer_ptr])
             BEGIN(LEX_ERROR_ATTR_VAL);
          NEXT_CHAR;
          break;

         case 43:            /* U4c */
          NEXT_CHAR;
          if(!U4c[0][(int)*buffer_ptr])
             BEGIN(LEX_ERROR_ATTR_VAL);
          NEXT_CHAR;
          if(!U4c[1][(int)*buffer_ptr])
             BEGIN(LEX_ERROR_ATTR_VAL);
          NEXT_CHAR;
          if(!U4c[2][(int)*buffer_ptr])
             BEGIN(LEX_ERROR_ATTR_VAL);
          NEXT_CHAR;
          break;

         case 50:            /* entityref or charref */
          NEXT_CHAR;

          if(*buffer_ptr=='#') /* charref */
            {
             int charref_len=3;

             NEXT_CHAR;
             if(digit[(int)*buffer_ptr]) /* decimal */
               {
                NEXT_CHAR;
                charref_len++;

                while(digit[(int)*buffer_ptr])
                  {
                   NEXT_CHAR;
                   charref_len++;
                  }

                if(*buffer_ptr!=';')
                   BEGIN(LEX_ERROR_ATTR_VAL);
               }
             else if(*buffer_ptr=='x') /* hex */
               {
                NEXT_CHAR;
                charref_len++;

                while(xdigit[(int)*buffer_ptr])
                  {
                   NEXT_CHAR;
                   charref_len++;
                  }

                if(*buffer_ptr!=';')
                   BEGIN(LEX_ERROR_ATTR_VAL);
               }
             else            /* other */
                BEGIN(LEX_ERROR_ATTR_VAL);

             NEXT_CHAR;

             if(!(options&XMLPARSE_RETURN_ATTR_ENCODED))
               {
                const char *str;

                saved_buffer_ptr=*buffer_ptr;
                *buffer_ptr=0;

                str=ParseXML_Decode_Char_Ref((char*)(buffer_ptr-charref_len));

                if(!str)
                  {
                   buffer_ptr-=charref_len;
                   BEGIN(LEX_ERROR_CHAR_REF);
                  }

                buffer_token=memmove(buffer_token+(charref_len-strlen(str)),buffer_token,buffer_ptr-buffer_token-charref_len);
                memcpy(buffer_ptr-strlen(str),str,strlen(str));

                *buffer_ptr=saved_buffer_ptr;
               }
            }
          else if(namestart[(int)*buffer_ptr]) /* entityref */
            {
             int entityref_len=3;

             NEXT_CHAR;
             while(namechar[(int)*buffer_ptr])
               {
                NEXT_CHAR;
                entityref_len++;
               }

             if(*buffer_ptr!=';')
                BEGIN(LEX_ERROR_ATTR_VAL);

             NEXT_CHAR;

             if(!(options&XMLPARSE_RETURN_ATTR_ENCODED))
               {
                const char *str;

                saved_buffer_ptr=*buffer_ptr;
                *buffer_ptr=0;

                str=ParseXML_Decode_Entity_Ref((char*)(buffer_ptr-entityref_len));

                if(!str)
                  {
                   buffer_ptr-=entityref_len;
                   BEGIN(LEX_ERROR_ENTITY_REF);
                  }

                buffer_token=memmove(buffer_token+(entityref_len-strlen(str)),buffer_token,buffer_ptr-buffer_token-entityref_len);
                memcpy(buffer_ptr-strlen(str),str,strlen(str));

                *buffer_ptr=saved_buffer_ptr;
               }
            }
          else               /* other */
             BEGIN(LEX_ERROR_ATTR_VAL);

          break;

         case 99:            /* quote */
          *buffer_ptr=0;
          NEXT_CHAR;

          NEXT(after_attr);
          BEGIN(LEX_FUNC_ATTR_VAL);

         default:            /* other */
          BEGIN(LEX_ERROR_ATTR_VAL);
         }
      }

    break;


    /* ================ Functional states ================ */


    /* The start of a tag for an XML declaration */

   case LEX_FUNC_XML_DECL_BEGIN:

    if(tag_stack)
       BEGIN(LEX_ERROR_XML_NOT_FIRST);

    /* The start of a tag for an element */

   case LEX_FUNC_TAG_BEGIN:

    tag=NULL;

    for(i=0;tags[i];i++)
       if(!strcasecmp((char*)buffer_token,tags[i]->name))
         {
          tag=tags[i];

          for(i=0;i<tag->nattributes;i++)
             attributes[i]=NULL;

          break;
         }

    if(tag==NULL)
       BEGIN(LEX_ERROR_UNEXP_TAG);

    END_TOKEN;

    *buffer_ptr=saved_buffer_ptr;
    BEGIN(next_state);

    /* The end of the start-tag for an element */

   case LEX_FUNC_TAG_PUSH:

    if(stackused==stackdepth)
      {
       tag_stack =(xmltag**) realloc((void*)tag_stack ,(stackdepth+=8)*sizeof(xmltag*));
       tags_stack=(xmltag***)realloc((void*)tags_stack,(stackdepth+=8)*sizeof(xmltag**));
      }

    tag_stack [stackused]=tag;
    tags_stack[stackused]=tags;
    stackused++;

    if(tag->callback)
       if(call_callback(tag->name,tag->callback,XMLPARSE_TAG_START,tag->nattributes,attributes))
          BEGIN(LEX_ERROR_CALLBACK);

    tags=tag->subtags;

    BEGIN(next_state);

    /* The end of the empty-element-tag for an XML declaration */

   case LEX_FUNC_XML_DECL_FINISH:

    /* The end of the empty-element-tag for an element */

   case LEX_FUNC_TAG_FINISH:

    if(tag->callback)
       if(call_callback(tag->name,tag->callback,XMLPARSE_TAG_START|XMLPARSE_TAG_END,tag->nattributes,attributes))
          BEGIN(LEX_ERROR_CALLBACK);

    if(stackused>0)
       tag=tag_stack[stackused-1];
    else
       tag=NULL;

    BEGIN(next_state);

    /* The end of the end-tag for an element */

   case LEX_FUNC_TAG_POP:

    stackused--;
    tags=tags_stack[stackused];
    tag =tag_stack [stackused];

    if(strcmp((char*)buffer_token,tag->name))
       BEGIN(LEX_ERROR_UNBALANCED);

    if(stackused<0)
       BEGIN(LEX_ERROR_NO_START);

    for(i=0;i<tag->nattributes;i++)
       attributes[i]=NULL;

    if(tag->callback)
       if(call_callback(tag->name,tag->callback,XMLPARSE_TAG_END,tag->nattributes,attributes))
          BEGIN(LEX_ERROR_CALLBACK);

    if(stackused>0)
       tag=tag_stack[stackused-1];
    else
       tag=NULL;

    END_TOKEN;

    *buffer_ptr=saved_buffer_ptr;
    BEGIN(next_state);

    /* An attribute key */

   case LEX_FUNC_ATTR_KEY:

    attribute=-1;

    for(i=0;i<tag->nattributes;i++)
       if(!strcasecmp((char*)buffer_token,tag->attributes[i]))
         {
          attribute=i;

          break;
         }

    if(attribute==-1)
      {
       if((options&XMLPARSE_UNKNOWN_ATTRIBUTES)==XMLPARSE_UNKNOWN_ATTR_ERROR ||
          ((options&XMLPARSE_UNKNOWN_ATTRIBUTES)==XMLPARSE_UNKNOWN_ATTR_ERRNONAME && !strchr((char*)buffer_token,':')))
          BEGIN(LEX_ERROR_UNEXP_ATT);
       else if((options&XMLPARSE_UNKNOWN_ATTRIBUTES)==XMLPARSE_UNKNOWN_ATTR_WARN)
          fprintf(stderr,"XML Parser: Warning on line %llu: unexpected attribute '%s' for tag '%s'.\n",lineno,buffer_token,tag->name);
      }

    END_TOKEN;

    *buffer_ptr=saved_buffer_ptr;
    BEGIN(next_state);

    /* An attribute value */

   case LEX_FUNC_ATTR_VAL:

    if(tag->callback && attribute!=-1)
       attributes[attribute]=buffer_token;

    END_TOKEN;

    BEGIN(next_state);

    /* End of file */

   case LEX_EOF:

    if(tag)
       BEGIN(LEX_ERROR_UNEXP_EOF);

    break;


    /* ================ Error states ================ */


   case LEX_ERROR_TAG_START:
    fprintf(stderr,"XML Parser: Error on line %llu: character '<' seen not at start of tag.\n",lineno);
    break;

   case LEX_ERROR_XML_DECL_START:
    fprintf(stderr,"XML Parser: Error on line %llu: characters '<?' seen not at start of XML declaration.\n",lineno);
    break;

   case LEX_ERROR_TAG:
    fprintf(stderr,"XML Parser: Error on line %llu: invalid character seen inside tag '<%s...>'.\n",lineno,tag->name);
    break;

   case LEX_ERROR_XML_DECL:
    fprintf(stderr,"XML Parser: Error on line %llu: invalid character seen inside XML declaration '<?xml...>'.\n",lineno);
    break;

   case LEX_ERROR_ATTR:
    fprintf(stderr,"XML Parser: Error on line %llu: invalid attribute definition seen in tag.\n",lineno);
    break;
    
   case LEX_ERROR_END_TAG:
    fprintf(stderr,"XML Parser: Error on line %llu: invalid character seen in end-tag.\n",lineno);
    break;

   case LEX_ERROR_COMMENT:
    fprintf(stderr,"XML Parser: Error on line %llu: invalid comment seen.\n",lineno);
    break;

   case LEX_ERROR_CLOSE:
    fprintf(stderr,"XML Parser: Error on line %llu: character '>' seen not at end of tag.\n",lineno);
    break;

   case LEX_ERROR_ATTR_VAL:
    fprintf(stderr,"XML Parser: Error on line %llu: invalid character '%c' seen in attribute value.\n",lineno,*buffer_ptr);
    break;

   case LEX_ERROR_ENTITY_REF:
    fprintf(stderr,"XML Parser: Error on line %llu: invalid entity reference '%s' seen in attribute value.\n",lineno,buffer_ptr);
    break;

   case LEX_ERROR_CHAR_REF:
    fprintf(stderr,"XML Parser: Error on line %llu: invalid character reference '%s' seen in attribute value.\n",lineno,buffer_ptr);
    break;

   case LEX_ERROR_TEXT_OUTSIDE:
    fprintf(stderr,"XML Parser: Error on line %llu: non-whitespace '%c' seen outside tag.\n",lineno,*buffer_ptr);
    break;

   case LEX_ERROR_UNEXP_TAG:
    fprintf(stderr,"XML Parser: Error on line %llu: unexpected tag '%s'.\n",lineno,buffer_token);
    break;

   case LEX_ERROR_UNBALANCED:
    fprintf(stderr,"XML Parser: Error on line %llu: end tag '</%s>' doesn't match start tag '<%s ...>'.\n",lineno,buffer_token,tag->name);
    break;

   case LEX_ERROR_NO_START:
    fprintf(stderr,"XML Parser: Error on line %llu: end tag '</%s>' seen but there was no start tag '<%s ...>'.\n",lineno,buffer_token,buffer_token);
    break;

   case LEX_ERROR_UNEXP_ATT:
    fprintf(stderr,"XML Parser: Error on line %llu: unexpected attribute '%s' for tag '%s'.\n",lineno,buffer_token,tag->name);
    break;

   case LEX_ERROR_UNEXP_EOF:
    fprintf(stderr,"XML Parser: Error on line %llu: end of file seen without end tag '</%s>'.\n",lineno,tag->name);
    break;

   case LEX_ERROR_XML_NOT_FIRST:
    fprintf(stderr,"XML Parser: Error on line %llu: XML declaration '<?xml...>' not before all other tags.\n",lineno);
    break;
   }

 /* Delete the tagdata */

 if(stackdepth)
   {
    free(tag_stack);
    free(tags_stack);
   }

 return(state);
}


/*++++++++++++++++++++++++++++++++++++++
  Return the current parser line number.

  uint64_t ParseXML_LineNumber Returns the line number.
  ++++++++++++++++++++++++++++++++++++++*/

uint64_t ParseXML_LineNumber(void)
{
 return(lineno);
}


/*++++++++++++++++++++++++++++++++++++++
  Convert an XML entity reference into an ASCII string.

  char *ParseXML_Decode_Entity_Ref Returns a pointer to the replacement decoded string.

  const char *string The entity reference string.
  ++++++++++++++++++++++++++++++++++++++*/

char *ParseXML_Decode_Entity_Ref(const char *string)
{
 if(!strcmp(string,"&amp;"))  return("&");
 if(!strcmp(string,"&lt;"))   return("<");
 if(!strcmp(string,"&gt;"))   return(">");
 if(!strcmp(string,"&apos;")) return("'");
 if(!strcmp(string,"&quot;")) return("\"");
 return(NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Convert an XML character reference into an ASCII string.

  char *ParseXML_Decode_Char_Ref Returns a pointer to the replacement decoded string.

  const char *string The character reference string.
  ++++++++++++++++++++++++++++++++++++++*/

char *ParseXML_Decode_Char_Ref(const char *string)
{
 static char result[5]="";
 long int unicode;

 if(string[2]=='x') unicode=strtol(string+3,NULL,16);
 else               unicode=strtol(string+2,NULL,10);

 if(unicode<0x80)
   {
    /* 0000 0000-0000 007F  =>  0xxxxxxx */
    result[0]=unicode;
    result[1]=0;
   }
 else if(unicode<0x07FF)
   {
    /* 0000 0080-0000 07FF  =>  110xxxxx 10xxxxxx */
    result[0]=0xC0+((unicode&0x07C0)>>6);
    result[1]=0x80+ (unicode&0x003F);
    result[2]=0;
   }
 else if(unicode<0xFFFF)
   {
    /* 0000 0800-0000 FFFF  =>  1110xxxx 10xxxxxx 10xxxxxx */
    result[0]=0xE0+((unicode&0xF000)>>12);
    result[1]=0x80+((unicode&0x0FC0)>>6);
    result[2]=0x80+ (unicode&0x003F);
    result[3]=0;
   }
 else if(unicode<0x1FFFFF)
   {
    /* 0001 0000-001F FFFF  =>  11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
    result[0]=0xF0+((unicode&0x1C0000)>>18);
    result[1]=0x80+((unicode&0x03F000)>>12);
    result[2]=0x80+((unicode&0x000FC0)>>6);
    result[3]=0x80+ (unicode&0x00003F);
    result[4]=0;
   }
 else
   {
    result[0]=0xFF;
    result[1]=0xFD;
    result[2]=0;
   }

 return(result);
}


/*++++++++++++++++++++++++++++++++++++++
  Convert a string into something that is safe to output in an XML file.

  char *ParseXML_Encode_Safe_XML Returns a pointer to the replacement encoded string (or the original if no change needed).

  const char *string The string to convert.
  ++++++++++++++++++++++++++++++++++++++*/

char *ParseXML_Encode_Safe_XML(const char *string)
{
 static const char hexstring[17]="0123456789ABCDEF";
 int i=0,j=0,len;
 char *result;

 for(i=0;string[i];i++)
    if(string[i]=='<' || string[i]=='>' || string[i]=='&' || string[i]=='\'' || string[i]=='"' || string[i]<32 || (unsigned char)string[i]>127)
       break;

 if(!string[i])
    return((char*)string);

 len=i+256-6;

 result=(char*)malloc(len+7);
 strncpy(result,string,j=i);

 do
   {
    for(;j<len && string[i];i++)
       if(string[i]=='\'')
         {
          result[j++]='&';
          result[j++]='a';
          result[j++]='p';
          result[j++]='o';
          result[j++]='s';
          result[j++]=';';
         }
       else if(string[i]=='&')
         {
          result[j++]='&';
          result[j++]='a';
          result[j++]='m';
          result[j++]='p';
          result[j++]=';';
         }
       else if(string[i]=='"')
         {
          result[j++]='&';
          result[j++]='q';
          result[j++]='u';
          result[j++]='o';
          result[j++]='t';
          result[j++]=';';
         }
       else if(string[i]=='<')
         {
          result[j++]='&';
          result[j++]='l';
          result[j++]='t';
          result[j++]=';';
         }
       else if(string[i]=='>')
         {
          result[j++]='&';
          result[j++]='g';
          result[j++]='t';
          result[j++]=';';
         }
       else if(string[i]>=32 && (unsigned char)string[i]<=127)
          result[j++]=string[i];
       else
         {
          unsigned int unicode;

          /* Decode the UTF-8 */

          if((string[i]&0x80)==0)
            {
             /* 0000 0000-0000 007F  =>  0xxxxxxx */
             unicode=string[i];
            }
          else if((string[i]&0xE0)==0xC0 && (string[i]&0x1F)>=2 && (string[i+1]&0xC0)==0x80)
            {
             /* 0000 0080-0000 07FF  =>  110xxxxx 10xxxxxx */
             unicode =(string[i++]&0x1F)<<6;
             unicode|= string[i  ]&0x3F;
            }
          else if((string[i]&0xF0)==0xE0 && (string[i+1]&0xC0)==0x80 && (string[i+2]&0xC0)==0x80)
            {
             /* 0000 0800-0000 FFFF  =>  1110xxxx 10xxxxxx 10xxxxxx */
             unicode =(string[i++]&0x0F)<<12;
             unicode|=(string[i++]&0x3F)<<6;
             unicode|= string[i  ]&0x3F;
            }
          else if((string[i]&0xF8)==0xF0 && (string[i+1]&0xC0)==0x80 && (string[i+2]&0xC0)==0x80 && (string[i+3]&0xC0)==0x80)
            {
             /* 0001 0000-001F FFFF  =>  11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
             unicode =(string[i++]&0x07)<<18;
             unicode|=(string[i++]&0x3F)<<12;
             unicode|=(string[i++]&0x3F)<<6;
             unicode|= string[i  ]&0x3F;
            }
          else
             unicode=0xFFFD;

          /* Output the character entity */

          result[j++]='&';
          result[j++]='#';
          result[j++]='x';

          if(unicode&0x00FF0000)
            {
             result[j++]=hexstring[((unicode>>16)&0xf0)>>4];
             result[j++]=hexstring[((unicode>>16)&0x0f)   ];
            }
          if(unicode&0x00FFFF00)
            {
             result[j++]=hexstring[((unicode>>8)&0xf0)>>4];
             result[j++]=hexstring[((unicode>>8)&0x0f)   ];
            }
          result[j++]=hexstring[(unicode&0xf0)>>4];
          result[j++]=hexstring[(unicode&0x0f)   ];

          result[j++]=';';
         }

    if(string[i])                  /* Not finished */
      {
       len+=256;
       result=(char*)realloc((void*)result,len+7);
      }
   }
 while(string[i]);

 result[j]=0;

 return(result);
}


/*++++++++++++++++++++++++++++++++++++++
  Check that a string really is an integer.

  int ParseXML_IsInteger Returns 1 if an integer could be found or 0 otherwise.

  const char *string The string to be parsed.
  ++++++++++++++++++++++++++++++++++++++*/

int ParseXML_IsInteger(const char *string)
{
 const unsigned char *p=(unsigned char*)string;

 if(*p=='-' || *p=='+')
    p++;

 while(digit[(int)*p])
    p++;

 if(*p)
    return(0);
 else
    return(1);
}


/*++++++++++++++++++++++++++++++++++++++
  Check that a string really is a floating point number.

  int ParseXML_IsFloating Returns 1 if a floating point number could be found or 0 otherwise.

  const char *string The string to be parsed.
  ++++++++++++++++++++++++++++++++++++++*/

int ParseXML_IsFloating(const char *string)
{
 const unsigned char *p=(unsigned char*)string;

 if(*p=='-' || *p=='+')
    p++;

 while(digit[(int)*p] || *p=='.')
    p++;

 if(*p=='e' || *p=='E')
   {
    p++;

    if(*p=='-' || *p=='+')
       p++;

    while(digit[*p])
       p++;
   }

 if(*p)
    return(0);
 else
    return(1);
}


/* Table for checking for double-quoted characters. */
static const unsigned char quotedD[256]={ 0, 0, 0, 0, 0, 0, 0, 0, 0,10,10, 0, 0,10, 0, 0,  /* 0x00-0x0f "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x10-0x1f "                " */
                                         10,10,99,10,10,10,50,10,10,10,10,10,10,10,10,10,  /* 0x20-0x2f " !"#$%&'()*+,-./" */
                                         10,10,10,10,10,10,10,10,10,10,10,10, 0,10, 0,10,  /* 0x30-0x3f "0123456789:;<=>?" */
                                         10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  /* 0x40-0x4f "@ABCDEFGHIJKLMNO" */
                                         10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  /* 0x50-0x5f "PQRSTUVWXYZ[\]^_" */
                                         10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  /* 0x60-0x6f "`abcdefghijklmno" */
                                         10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  /* 0x70-0x7f "pqrstuvwxyz{|}~ " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x80-0x8f "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x90-0x9f "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xa0-0xaf "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xb0-0xbf "                " */
                                          0, 0,20,20,20,20,20,20,20,20,20,20,20,20,20,20,  /* 0xc0-0xcf "                " */
                                         20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,  /* 0xd0-0xdf "                " */
                                         31,32,32,32,32,32,32,32,32,32,32,32,32,33,34,34,  /* 0xe0-0xef "                " */
                                         41,42,42,42,43, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; /* 0xf0-0xff "                " */

/* Table for checking for single-quoted characters. */
static const unsigned char quotedS[256]={ 0, 0, 0, 0, 0, 0, 0, 0, 0,10,10, 0, 0,10, 0, 0,  /* 0x00-0x0f "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x10-0x1f "                " */
                                         10,10,10,10,10,10,50,99,10,10,10,10,10,10,10,10,  /* 0x20-0x2f " !"#$%&'()*+,-./" */
                                         10,10,10,10,10,10,10,10,10,10,10,10, 0,10, 0,10,  /* 0x30-0x3f "0123456789:;<=>?" */
                                         10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  /* 0x40-0x4f "@ABCDEFGHIJKLMNO" */
                                         10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  /* 0x50-0x5f "PQRSTUVWXYZ[\]^_" */
                                         10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  /* 0x60-0x6f "`abcdefghijklmno" */
                                         10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  /* 0x70-0x7f "pqrstuvwxyz{|}~ " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x80-0x8f "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x90-0x9f "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xa0-0xaf "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xb0-0xbf "                " */
                                          0, 0,20,20,20,20,20,20,20,20,20,20,20,20,20,20,  /* 0xc0-0xcf "                " */
                                         20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,  /* 0xd0-0xdf "                " */
                                         31,32,32,32,32,32,32,32,32,32,32,32,32,33,34,34,  /* 0xe0-0xef "                " */
                                         41,42,42,42,43, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; /* 0xf0-0xff "                " */

/* Table for checking for characters between 0x80 and 0x8f. */
static const unsigned char U_80_8F[256]={ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x00-0x0f "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x10-0x1f "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x20-0x2f " !"#$%&'()*+,-./" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x30-0x3f "0123456789:;<=>?" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x40-0x4f "@ABCDEFGHIJKLMNO" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x50-0x5f "PQRSTUVWXYZ[\]^_" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x60-0x6f "`abcdefghijklmno" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x70-0x7f "pqrstuvwxyz{|}~ " */
                                          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0x80-0x8f "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x90-0x9f "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xa0-0xaf "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xb0-0xbf "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xc0-0xcf "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xd0-0xdf "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xe0-0xef "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; /* 0xf0-0xff "                " */

/* Table for checking for characters between 0x80 and 0x9f. */
static const unsigned char U_80_9F[256]={ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x00-0x0f "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x10-0x1f "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x20-0x2f " !"#$%&'()*+,-./" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x30-0x3f "0123456789:;<=>?" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x40-0x4f "@ABCDEFGHIJKLMNO" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x50-0x5f "PQRSTUVWXYZ[\]^_" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x60-0x6f "`abcdefghijklmno" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x70-0x7f "pqrstuvwxyz{|}~ " */
                                          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0x80-0x8f "                " */
                                          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0x90-0x9f "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xa0-0xaf "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xb0-0xbf "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xc0-0xcf "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xd0-0xdf "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xe0-0xef "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; /* 0xf0-0xff "                " */

/* Table for checking for characters between 0x80 and 0xbf. */
static const unsigned char U_80_BF[256]={ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x00-0x0f "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x10-0x1f "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x20-0x2f " !"#$%&'()*+,-./" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x30-0x3f "0123456789:;<=>?" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x40-0x4f "@ABCDEFGHIJKLMNO" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x50-0x5f "PQRSTUVWXYZ[\]^_" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x60-0x6f "`abcdefghijklmno" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x70-0x7f "pqrstuvwxyz{|}~ " */
                                          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0x80-0x8f "                " */
                                          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0x90-0x9f "                " */
                                          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0xa0-0xaf "                " */
                                          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0xb0-0xbf "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xc0-0xcf "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xd0-0xdf "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xe0-0xef "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; /* 0xf0-0xff "                " */

/* Table for checking for characters between 0x90 and 0xbf. */
static const unsigned char U_90_BF[256]={ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x00-0x0f "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x10-0x1f "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x20-0x2f " !"#$%&'()*+,-./" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x30-0x3f "0123456789:;<=>?" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x40-0x4f "@ABCDEFGHIJKLMNO" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x50-0x5f "PQRSTUVWXYZ[\]^_" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x60-0x6f "`abcdefghijklmno" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x70-0x7f "pqrstuvwxyz{|}~ " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x80-0x8f "                " */
                                          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0x90-0x9f "                " */
                                          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0xa0-0xaf "                " */
                                          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0xb0-0xbf "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xc0-0xcf "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xd0-0xdf "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xe0-0xef "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; /* 0xf0-0xff "                " */

/* Table for checking for characters between 0xa0 and 0xbf. */
static const unsigned char U_A0_BF[256]={ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x00-0x0f "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x10-0x1f "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x20-0x2f " !"#$%&'()*+,-./" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x30-0x3f "0123456789:;<=>?" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x40-0x4f "@ABCDEFGHIJKLMNO" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x50-0x5f "PQRSTUVWXYZ[\]^_" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x60-0x6f "`abcdefghijklmno" */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x70-0x7f "pqrstuvwxyz{|}~ " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x80-0x8f "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x90-0x9f "                " */
                                          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0xa0-0xaf "                " */
                                          1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0xb0-0xbf "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xc0-0xcf "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xd0-0xdf "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xe0-0xef "                " */
                                          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; /* 0xf0-0xff "                " */

/* Table for checking for U2 characters = C2-DF,80-BF = U+0080-U+07FF. */
static const unsigned char *U2[1]={ U_80_BF };

/* Table for checking for U3a characters = E0,A0-BF,80-BF = U+0800-U+0FFF. */
static const unsigned char *U3a[2]={ U_A0_BF, U_80_BF };

/* Table for checking for U3b characters = E1-EC,80-BF,80-BF = U+1000-U+CFFF. */
static const unsigned char *U3b[2]={ U_80_BF, U_80_BF };

/* Table for checking for U3c characters = ED,80-9F,80-BF = U+D000-U+D7FF (U+D800-U+DFFF are not legal in XML). */
static const unsigned char *U3c[2]={ U_80_9F, U_80_BF };

/* Table for checking for U3d characters = EE-EF,80-BF,80-BF = U+E000-U+FFFF (U+FFFE-U+FFFF are not legal in XML but handled). */
static const unsigned char *U3d[2]={ U_80_BF, U_80_BF };

/* Table for checking for U4a characters = F0,90-BF,80-BF,80-BF = U+10000-U+3FFFF. */
static const unsigned char *U4a[3]={ U_90_BF, U_80_BF, U_80_BF };

/* Table for checking for U4b characters = F1-F3,80-BF,80-BF,80-BF = U+40000-U+FFFFF. */
static const unsigned char *U4b[3]={ U_80_BF, U_80_BF, U_80_BF };

/* Table for checking for U4c characters = F4,80-8F,80-BF,80-BF = U+100000-U+10FFFF (U+110000- are not legal in XML). */
static const unsigned char *U4c[3]={ U_80_8F, U_80_BF, U_80_BF };

/* Table for checking for namestart characters. */
static const unsigned char namestart[256]={ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x00-0x0f "                " */
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x10-0x1f "                " */
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x20-0x2f " !"#$%&'()*+,-./" */
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0,  /* 0x30-0x3f "0123456789:;<=>?" */
                                            0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0x40-0x4f "@ABCDEFGHIJKLMNO" */
                                            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,  /* 0x50-0x5f "PQRSTUVWXYZ[\]^_" */
                                            0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0x60-0x6f "`abcdefghijklmno" */
                                            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,  /* 0x70-0x7f "pqrstuvwxyz{|}~ " */
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x80-0x8f "                " */
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x90-0x9f "                " */
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xa0-0xaf "                " */
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xb0-0xbf "                " */
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xc0-0xcf "                " */
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xd0-0xdf "                " */
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xe0-0xef "                " */
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; /* 0xf0-0xff "                " */

/* Table for checking for namechar characters. */
static const unsigned char namechar[256] ={ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x00-0x0f "                " */
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x10-0x1f "                " */
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0,  /* 0x20-0x2f " !"#$%&'()*+,-./" */
                                            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,  /* 0x30-0x3f "0123456789:;<=>?" */
                                            0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0x40-0x4f "@ABCDEFGHIJKLMNO" */
                                            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,  /* 0x50-0x5f "PQRSTUVWXYZ[\]^_" */
                                            0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 0x60-0x6f "`abcdefghijklmno" */
                                            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,  /* 0x70-0x7f "pqrstuvwxyz{|}~ " */
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x80-0x8f "                " */
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x90-0x9f "                " */
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xa0-0xaf "                " */
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xb0-0xbf "                " */
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xc0-0xcf "                " */
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xd0-0xdf "                " */
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xe0-0xef "                " */
                                            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; /* 0xf0-0xff "                " */

/* Table for checking for whitespace characters. */
static const unsigned char whitespace[256]={ 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0,  /* 0x00-0x0f "                " */
                                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x10-0x1f "                " */
                                             1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x20-0x2f " !"#$%&'()*+,-./" */
                                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x30-0x3f "0123456789:;<=>?" */
                                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x40-0x4f "@ABCDEFGHIJKLMNO" */
                                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x50-0x5f "PQRSTUVWXYZ[\]^_" */
                                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x60-0x6f "`abcdefghijklmno" */
                                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x70-0x7f "pqrstuvwxyz{|}~ " */
                                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x80-0x8f "                " */
                                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x90-0x9f "                " */
                                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xa0-0xaf "                " */
                                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xb0-0xbf "                " */
                                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xc0-0xcf "                " */
                                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xd0-0xdf "                " */
                                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xe0-0xef "                " */
                                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; /* 0xf0-0xff "                " */

/* Table for checking for digit characters. */
static const unsigned char digit[256]={ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x00-0x0f "                " */
                                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x10-0x1f "                " */
                                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x20-0x2f " !"#$%&'()*+,-./" */
                                        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,  /* 0x30-0x3f "0123456789:;<=>?" */
                                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x40-0x4f "@ABCDEFGHIJKLMNO" */
                                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x50-0x5f "PQRSTUVWXYZ[\]^_" */
                                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x60-0x6f "`abcdefghijklmno" */
                                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x70-0x7f "pqrstuvwxyz{|}~ " */
                                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x80-0x8f "                " */
                                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x90-0x9f "                " */
                                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xa0-0xaf "                " */
                                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xb0-0xbf "                " */
                                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xc0-0xcf "                " */
                                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xd0-0xdf "                " */
                                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xe0-0xef "                " */
                                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; /* 0xf0-0xff "                " */

/* Table for checking for xdigit characters. */
static const unsigned char xdigit[256]={ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x00-0x0f "                " */
                                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x10-0x1f "                " */
                                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x20-0x2f " !"#$%&'()*+,-./" */
                                         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,  /* 0x30-0x3f "0123456789:;<=>?" */
                                         0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x40-0x4f "@ABCDEFGHIJKLMNO" */
                                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x50-0x5f "PQRSTUVWXYZ[\]^_" */
                                         0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x60-0x6f "`abcdefghijklmno" */
                                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x70-0x7f "pqrstuvwxyz{|}~ " */
                                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x80-0x8f "                " */
                                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x90-0x9f "                " */
                                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xa0-0xaf "                " */
                                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xb0-0xbf "                " */
                                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xc0-0xcf "                " */
                                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xd0-0xdf "                " */
                                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xe0-0xef "                " */
                                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; /* 0xf0-0xff "                " */
