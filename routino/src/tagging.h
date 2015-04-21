/***************************************
 The data types for the tagging rules.

 Part of the Routino routing software.
 ******************/ /******************
 This file Copyright 2010-2012 Andrew M. Bishop

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

#ifndef TAGGING_H
#define TAGGING_H    /*+ To stop multiple inclusions. +*/

#include <stdint.h>


/* Data types */

typedef struct _TaggingRuleList TaggingRuleList;


/*+ A structure to contain the tagging rule/action. +*/
typedef struct _TaggingRule
{
 int action;                    /*+ A flag to indicate the type of action. +*/

 char *k;                       /*+ The tag key (or NULL). +*/
 char *v;                       /*+ The tag value (or NULL). +*/
 char *message;                 /*+ The message string for logerror (or NULL). +*/

 TaggingRuleList *rulelist;     /*+ The sub-rules belonging to this rule. +*/
}
 TaggingRule;


/*+ A structure to contain the list of rules and associated information. +*/
struct _TaggingRuleList
{
 TaggingRule *rules;            /*+ The array of rules. +*/
 int          nrules;           /*+ The number of rules. +*/
};


/*+ A structure to hold a list of tags to be processed. +*/
typedef struct _TagList
{
 int ntags;                     /*+ The number of tags. +*/

 char **k;                      /*+ The list of tag keys. +*/
 char **v;                      /*+ The list of tag values. +*/
}
 TagList;


/* Functions in tagging.c */

int ParseXMLTaggingRules(const char *filename);
void DeleteXMLTaggingRules(void);

TagList *NewTagList(void);
void DeleteTagList(TagList *tags);

void AppendTag(TagList *tags,const char *k,const char *v);

TagList *ApplyNodeTaggingRules(TagList *tags,int64_t id);
TagList *ApplyWayTaggingRules(TagList *tags,int64_t id);
TagList *ApplyRelationTaggingRules(TagList *tags,int64_t id);


#endif /* TAGGING_H */
