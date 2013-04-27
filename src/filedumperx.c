/***************************************
 Memory file dumper for the intermediate files containing parsed data.

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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "typesx.h"
#include "nodesx.h"
#include "segmentsx.h"
#include "waysx.h"
#include "relationsx.h"

#include "files.h"
#include "sorting.h"


/* Local functions */

static void print_nodes(const char *filename);
static void print_segments(const char *filename);
static void print_ways(const char *filename);
static void print_route_relations(const char *filename);
static void print_turn_relations(const char *filename);

static void print_usage(int detail,const char *argerr,const char *err);


/*++++++++++++++++++++++++++++++++++++++
  The main program for the file dumper.
  ++++++++++++++++++++++++++++++++++++++*/

int main(int argc,char** argv)
{
 int   arg;
 char *dirname=NULL,*prefix=NULL;
 char *nodes_filename,*segments_filename,*ways_filename,*route_relations_filename,*turn_relations_filename;
 int   option_dump;

 /* Parse the command line arguments */

 for(arg=1;arg<argc;arg++)
   {
    if(!strcmp(argv[arg],"--help"))
       print_usage(1,NULL,NULL);
    else if(!strncmp(argv[arg],"--dir=",6))
       dirname=&argv[arg][6];
    else if(!strncmp(argv[arg],"--prefix=",9))
       prefix=&argv[arg][9];
    else if(!strcmp(argv[arg],"--dump"))
       option_dump=1;
    else if(!strcmp(argv[arg],"--nodes"))
       ;
    else if(!strcmp(argv[arg],"--segments"))
       ;
    else if(!strcmp(argv[arg],"--ways"))
       ;
    else if(!strcmp(argv[arg],"--route-relations"))
       ;
    else if(!strcmp(argv[arg],"--turn-relations"))
       ;
    else
       print_usage(0,argv[arg],NULL);
   }

 if((option_dump)!=1)
    print_usage(0,NULL,"Must choose --dump.");

 /* Load in the data - Note: No error checking because Load*List() will call exit() in case of an error. */

 nodes_filename=FileName(dirname,prefix,"nodesx.parsed.mem");

 segments_filename=FileName(dirname,prefix,"segmentsx.parsed.mem");

 ways_filename=FileName(dirname,prefix,"waysx.parsed.mem");

 route_relations_filename=FileName(dirname,prefix,"relationsx.route.parsed.mem");

 turn_relations_filename=FileName(dirname,prefix,"relationsx.turn.parsed.mem");

 /* Print out internal data (in plain text format) */

 if(option_dump)
   {
    for(arg=1;arg<argc;arg++)
       if(!strcmp(argv[arg],"--nodes"))
         {
          print_nodes(nodes_filename);
         }
       else if(!strcmp(argv[arg],"--segments"))
         {
          print_segments(segments_filename);
         }
       else if(!strcmp(argv[arg],"--ways"))
         {
          print_ways(ways_filename);
         }
       else if(!strcmp(argv[arg],"--route-relations"))
         {
          print_route_relations(route_relations_filename);
         }
       else if(!strcmp(argv[arg],"--turn-relations"))
         {
          print_turn_relations(turn_relations_filename);
         }
   }

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Print out all of the nodes.

  const char *filename The name of the file containing the data.
  ++++++++++++++++++++++++++++++++++++++*/

static void print_nodes(const char *filename)
{
 off_t size,position=0;
 int fd;

 size=SizeFile(filename);

 fd=ReOpenFile(filename);

 while(position<size)
   {
    NodeX nodex;

    ReadFile(fd,&nodex,sizeof(NodeX));

    printf("Node %"Pnode_t"\n",nodex.id);
    printf("  lat=%d lon=%d\n",nodex.latitude,nodex.longitude);
    printf("  allow=%02x\n",nodex.allow);
    printf("  flags=%02x\n",nodex.flags);

    position+=sizeof(NodeX);
   }

 CloseFile(fd);
}


/*++++++++++++++++++++++++++++++++++++++
  Print out all of the segments.

  const char *filename The name of the file containing the data.
  ++++++++++++++++++++++++++++++++++++++*/

static void print_segments(const char *filename)
{
 off_t size,position=0;
 int fd;

 size=SizeFile(filename);

 fd=ReOpenFile(filename);

 while(position<size)
   {
    SegmentX segmentx;

    ReadFile(fd,&segmentx,sizeof(SegmentX));

    printf("Segment\n");
    printf("  node1=%"Pnode_t" node2=%"Pnode_t"\n",segmentx.node1,segmentx.node2);
    printf("  way=%"Pway_t"\n",segmentx.way);
    if(segmentx.distance&SEGMENT_AREA)
       printf("  Part of area\n");
    if(segmentx.distance&ONEWAY_1TO2)
       printf("  One-way (forward)\n");
    if(segmentx.distance&ONEWAY_2TO1)
       printf("  One-way (reverse)\n");

    position+=sizeof(SegmentX);
   }

 CloseFile(fd);
}


/*++++++++++++++++++++++++++++++++++++++
  Print out all of the ways.

  const char *filename The name of the file containing the data.
  ++++++++++++++++++++++++++++++++++++++*/

static void print_ways(const char *filename)
{
 off_t size,position=0;
 int fd;

 size=SizeFile(filename);

 fd=ReOpenFile(filename);

 while(position<size)
   {
    FILESORT_VARINT waysize;
    WayX wayx;
    char *name=NULL;
    int malloced=0;

    ReadFile(fd,&waysize,FILESORT_VARSIZE);

    ReadFile(fd,&wayx,sizeof(WayX));

    if(malloced<(waysize-sizeof(WayX)))
      {
       malloced=(waysize-sizeof(WayX));
       name=(char*)realloc((void*)name,malloced);
      }

    ReadFile(fd,name,(waysize-sizeof(WayX)));

    printf("Way %"Pway_t"\n",wayx.id);
    if(*name)
       printf("  name=%s\n",name);
    printf("  type=%02x\n",wayx.way.type);
    printf("  allow=%02x\n",wayx.way.allow);
    if(wayx.way.props)
       printf("  props=%02x\n",wayx.way.props);
    if(wayx.way.speed)
       printf("  speed=%d\n",wayx.way.speed);
    if(wayx.way.weight)
       printf("  weight=%d\n",wayx.way.weight);
    if(wayx.way.height)
       printf("  height=%d\n",wayx.way.height);
    if(wayx.way.width)
       printf("  width=%d\n",wayx.way.width);
    if(wayx.way.length)
       printf("  length=%d\n",wayx.way.length);

    position+=waysize+FILESORT_VARSIZE;
   }

 CloseFile(fd);
}


/*++++++++++++++++++++++++++++++++++++++
  Print out all of the route relations.

  const char *filename The name of the file containing the data.
  ++++++++++++++++++++++++++++++++++++++*/

static void print_route_relations(const char *filename)
{
 off_t size,position=0;
 int fd;

 size=SizeFile(filename);

 fd=ReOpenFile(filename);

 while(position<size)
   {
    FILESORT_VARINT relationsize;
    RouteRelX relationx;
    way_t wayid;
    relation_t relationid;

    ReadFile(fd,&relationsize,FILESORT_VARSIZE);

    ReadFile(fd,&relationx,sizeof(RouteRelX));

    printf("Relation %"Prelation_t"\n",relationx.id);
    printf("  routes=%02x\n",relationx.routes);

    do
      {
       ReadFile(fd,&wayid,sizeof(way_t));

       printf("  way=%"Pway_t"\n",wayid);
      }
    while(wayid!=NO_WAY_ID);

    do
      {
       ReadFile(fd,&relationid,sizeof(relation_t));

       printf("  relation=%"Prelation_t"\n",relationid);
      }
    while(relationid!=NO_RELATION_ID);

    position+=relationsize+FILESORT_VARSIZE;
   }

 CloseFile(fd);
}


/*++++++++++++++++++++++++++++++++++++++
  Print out all of the turn relations.

  const char *filename The name of the file containing the data.
  ++++++++++++++++++++++++++++++++++++++*/

static void print_turn_relations(const char *filename)
{
 off_t size,position=0;
 int fd;

 size=SizeFile(filename);

 fd=ReOpenFile(filename);

 while(position<size)
   {
    TurnRelX relationx;

    ReadFile(fd,&relationx,sizeof(TurnRelX));

    printf("Relation %"Prelation_t"\n",relationx.id);
    printf("  from=%"Pway_t"\n",relationx.from);
    printf("  via=%"Pnode_t"\n",relationx.via);
    printf("  to=%"Pway_t"\n",relationx.to);
    printf("  type=%d\n",relationx.restriction);
    if(relationx.except)
       printf("  except=%02x\n",relationx.except);

    position+=sizeof(TurnRelX);
   }

 CloseFile(fd);
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
         "Usage: filedumper [--help]\n"
         "                  [--dir=<dirname>] [--prefix=<name>]\n"
         "                  [--dump [--nodes]\n"
         "                          [--segments]\n"
         "                          [--ways]\n"
         "                          [--route-relations]\n"
         "                          [--turn-relations]]\n");

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
            "--dir=<dirname>           The directory containing the routing database.\n"
            "--prefix=<name>           The filename prefix for the routing database.\n"
            "\n"
            "--dump                    Dump the intermediate files after parsing.\n"
            "  --nodes                 * all of the nodes.\n"
            "  --segments              * all of the segments.\n"
            "  --ways                  * all of the ways.\n"
            "  --route-relations       * all of the route relations.\n"
            "  --turn-relations        * all of the turn relations.\n");

 exit(!detail);
}
