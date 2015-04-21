/***************************************
 Header file for OSM parser function prototype.

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


#ifndef OSMPARSER_H
#define OSMPARSER_H    /*+ To stop multiple inclusions. +*/

#include <stdint.h>

#include "typesx.h"
#include "xmlparse.h"
#include "tagging.h"


/* Constants */

#define MODE_NORMAL  3
#define MODE_CREATE  2
#define MODE_MODIFY  1
#define MODE_DELETE -1


/* Variables in osmxmlparse.c */

extern xmltag *xml_osm_toplevel_tags[];
extern xmltag *xml_osc_toplevel_tags[];


/* Functions in osmpbfparse.c */

int ParsePBF(int fd);


/* Functions in osmo5mparse.c */

int ParseO5M(int fd,int changes);


/* Functions in osmparser.c */

int ParseOSMFile(int fd,NodesX *OSMNodes,SegmentsX *OSMSegments,WaysX *OSMWays,RelationsX *OSMRelations);

int ParseOSCFile(int fd,NodesX *OSMNodes,SegmentsX *OSMSegments,WaysX *OSMWays,RelationsX *OSMRelations);

int ParsePBFFile(int fd,NodesX *OSMNodes,SegmentsX *OSMSegments,WaysX *OSMWays,RelationsX *OSMRelations);

int ParseO5MFile(int fd,NodesX *OSMNodes,SegmentsX *OSMSegments,WaysX *OSMWays,RelationsX *OSMRelations);

int ParseO5CFile(int fd,NodesX *OSMNodes,SegmentsX *OSMSegments,WaysX *OSMWays,RelationsX *OSMRelations);

void AddWayRefs(int64_t node_id);
void AddRelationRefs(int64_t node_id,int64_t way_id,int64_t relation_id,const char *role);

void ProcessNodeTags(TagList *tags,int64_t node_id,double latitude,double longitude,int mode);
void ProcessWayTags(TagList *tags,int64_t way_id, int mode);
void ProcessRelationTags(TagList *tags,int64_t relation_id,int mode);


#endif /* OSMPARSER_H */
