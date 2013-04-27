/***************************************
 Memory file dumper.

 Part of the Routino routing software.
 ******************/ /******************
 This file Copyright 2008-2013 Andrew M. Bishop

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
#include <sys/time.h>
#include <time.h>
#include <math.h>

#include "types.h"
#include "nodes.h"
#include "segments.h"
#include "ways.h"
#include "relations.h"

#include "files.h"
#include "visualiser.h"
#include "xmlparse.h"


/* Local functions */

static void print_node(Nodes *nodes,index_t item);
static void print_segment(Segments *segments,index_t item);
static void print_way(Ways *ways,index_t item);
static void print_turn_relation(Relations *relations,index_t item,Segments *segments,Nodes *nodes);

static void print_head_osm(int coordcount,double latmin,double latmax,double lonmin,double lonmax);
static void print_region_osm(Nodes *nodes,Segments *segments,Ways *ways,Relations *relations,
                             double latmin,double latmax,double lonmin,double lonmax,int option_no_super);
static void print_node_osm(Nodes *nodes,index_t item);
static void print_segment_osm(Segments *segments,index_t item,Ways *ways);
static void print_turn_relation_osm(Relations *relations,index_t item,Segments *segments,Nodes *nodes);
static void print_tail_osm(void);

static char *RFC822Date(time_t t);

static void print_usage(int detail,const char *argerr,const char *err);


/*++++++++++++++++++++++++++++++++++++++
  The main program for the file dumper.
  ++++++++++++++++++++++++++++++++++++++*/

int main(int argc,char** argv)
{
 Nodes    *OSMNodes;
 Segments *OSMSegments;
 Ways     *OSMWays;
 Relations*OSMRelations;
 int       arg;
 char     *dirname=NULL,*prefix=NULL;
 char     *nodes_filename,*segments_filename,*ways_filename,*relations_filename;
 int       option_statistics=0;
 int       option_visualiser=0,coordcount=0;
 double    latmin=0,latmax=0,lonmin=0,lonmax=0;
 char     *option_data=NULL;
 int       option_dump=0;
 int       option_dump_osm=0,option_no_super=0;

 /* Parse the command line arguments */

 for(arg=1;arg<argc;arg++)
   {
    if(!strcmp(argv[arg],"--help"))
       print_usage(1,NULL,NULL);
    else if(!strncmp(argv[arg],"--dir=",6))
       dirname=&argv[arg][6];
    else if(!strncmp(argv[arg],"--prefix=",9))
       prefix=&argv[arg][9];
    else if(!strcmp(argv[arg],"--statistics"))
       option_statistics=1;
    else if(!strcmp(argv[arg],"--visualiser"))
       option_visualiser=1;
    else if(!strcmp(argv[arg],"--dump"))
       option_dump=1;
    else if(!strcmp(argv[arg],"--dump-osm"))
       option_dump_osm=1;
    else if(!strncmp(argv[arg],"--latmin",8) && argv[arg][8]=='=')
      {latmin=degrees_to_radians(atof(&argv[arg][9]));coordcount++;}
    else if(!strncmp(argv[arg],"--latmax",8) && argv[arg][8]=='=')
      {latmax=degrees_to_radians(atof(&argv[arg][9]));coordcount++;}
    else if(!strncmp(argv[arg],"--lonmin",8) && argv[arg][8]=='=')
      {lonmin=degrees_to_radians(atof(&argv[arg][9]));coordcount++;}
    else if(!strncmp(argv[arg],"--lonmax",8) && argv[arg][8]=='=')
      {lonmax=degrees_to_radians(atof(&argv[arg][9]));coordcount++;}
    else if(!strncmp(argv[arg],"--data",6) && argv[arg][6]=='=')
       option_data=&argv[arg][7];
    else if(!strcmp(argv[arg],"--no-super"))
       option_no_super=1;
    else if(!strncmp(argv[arg],"--node=",7))
       ;
    else if(!strncmp(argv[arg],"--segment=",10))
       ;
    else if(!strncmp(argv[arg],"--way=",6))
       ;
    else if(!strncmp(argv[arg],"--turn-relation=",16))
       ;
    else
       print_usage(0,argv[arg],NULL);
   }

 if((option_statistics + option_visualiser + option_dump + option_dump_osm)!=1)
    print_usage(0,NULL,"Must choose --visualiser, --statistics, --dump or --dump-osm.");

 /* Load in the data - Note: No error checking because Load*List() will call exit() in case of an error. */

 OSMNodes=LoadNodeList(nodes_filename=FileName(dirname,prefix,"nodes.mem"));

 OSMSegments=LoadSegmentList(segments_filename=FileName(dirname,prefix,"segments.mem"));

 OSMWays=LoadWayList(ways_filename=FileName(dirname,prefix,"ways.mem"));

 OSMRelations=LoadRelationList(relations_filename=FileName(dirname,prefix,"relations.mem"));

 /* Write out the visualiser data */

 if(option_visualiser)
   {
    Highway highway;
    Transport transport;
    Property property;

    if(coordcount!=4)
       print_usage(0,NULL,"The --visualiser option must have --latmin, --latmax, --lonmin, --lonmax.\n");

    if(!option_data)
       print_usage(0,NULL,"The --visualiser option must have --data.\n");

    if(!strcmp(option_data,"junctions"))
       OutputJunctions(OSMNodes,OSMSegments,OSMWays,OSMRelations,latmin,latmax,lonmin,lonmax);
    else if(!strcmp(option_data,"super"))
       OutputSuper(OSMNodes,OSMSegments,OSMWays,OSMRelations,latmin,latmax,lonmin,lonmax);
    else if(!strcmp(option_data,"oneway"))
       OutputOneway(OSMNodes,OSMSegments,OSMWays,OSMRelations,latmin,latmax,lonmin,lonmax);
    else if(!strncmp(option_data,"highway",7) && option_data[7]=='-' && (highway=HighwayType(option_data+8))!=Highway_None)
       OutputHighway(OSMNodes,OSMSegments,OSMWays,OSMRelations,latmin,latmax,lonmin,lonmax,highway);
    else if(!strncmp(option_data,"transport",9) && option_data[9]=='-' && (transport=TransportType(option_data+10))!=Transport_None)
       OutputTransport(OSMNodes,OSMSegments,OSMWays,OSMRelations,latmin,latmax,lonmin,lonmax,transport);
    else if(!strncmp(option_data,"barrier",7) && option_data[7]=='-' && (transport=TransportType(option_data+8))!=Transport_None)
       OutputBarrier(OSMNodes,OSMSegments,OSMWays,OSMRelations,latmin,latmax,lonmin,lonmax,transport);
    else if(!strcmp(option_data,"turns"))
       OutputTurnRestrictions(OSMNodes,OSMSegments,OSMWays,OSMRelations,latmin,latmax,lonmin,lonmax);
    else if(!strcmp(option_data,"speed"))
       OutputSpeedLimits(OSMNodes,OSMSegments,OSMWays,OSMRelations,latmin,latmax,lonmin,lonmax);
    else if(!strcmp(option_data,"weight"))
       OutputWeightLimits(OSMNodes,OSMSegments,OSMWays,OSMRelations,latmin,latmax,lonmin,lonmax);
    else if(!strcmp(option_data,"height"))
       OutputHeightLimits(OSMNodes,OSMSegments,OSMWays,OSMRelations,latmin,latmax,lonmin,lonmax);
    else if(!strcmp(option_data,"width"))
       OutputWidthLimits(OSMNodes,OSMSegments,OSMWays,OSMRelations,latmin,latmax,lonmin,lonmax);
    else if(!strcmp(option_data,"length"))
       OutputLengthLimits(OSMNodes,OSMSegments,OSMWays,OSMRelations,latmin,latmax,lonmin,lonmax);
    else if(!strncmp(option_data,"property",8) && option_data[8]=='-' && (property=PropertyType(option_data+9))!=Property_None)
       OutputProperty(OSMNodes,OSMSegments,OSMWays,OSMRelations,latmin,latmax,lonmin,lonmax,property);
    else
       print_usage(0,option_data,NULL);
   }

 /* Print out statistics */

 if(option_statistics)
   {
    struct stat buf;

    /* Examine the files */

    printf("Files\n");
    printf("-----\n");
    printf("\n");

    stat(nodes_filename,&buf);

    printf("'%s%snodes.mem'     - %9"PRIu64" Bytes\n",prefix?prefix:"",prefix?"-":"",(uint64_t)buf.st_size);
    printf("%s\n",RFC822Date(buf.st_mtime));
    printf("\n");

    stat(segments_filename,&buf);

    printf("'%s%ssegments.mem'  - %9"PRIu64" Bytes\n",prefix?prefix:"",prefix?"-":"",(uint64_t)buf.st_size);
    printf("%s\n",RFC822Date(buf.st_mtime));
    printf("\n");

    stat(ways_filename,&buf);

    printf("'%s%sways.mem'      - %9"PRIu64" Bytes\n",prefix?prefix:"",prefix?"-":"",(uint64_t)buf.st_size);
    printf("%s\n",RFC822Date(buf.st_mtime));
    printf("\n");

    stat(relations_filename,&buf);

    printf("'%s%srelations.mem' - %9"PRIu64" Bytes\n",prefix?prefix:"",prefix?"-":"",(uint64_t)buf.st_size);
    printf("%s\n",RFC822Date(buf.st_mtime));
    printf("\n");

    /* Examine the nodes */

    printf("Nodes\n");
    printf("-----\n");
    printf("\n");

    printf("sizeof(Node) =%9lu Bytes\n",(unsigned long)sizeof(Node));
    printf("Number       =%9"Pindex_t"\n",OSMNodes->file.number);
    printf("Number(super)=%9"Pindex_t"\n",OSMNodes->file.snumber);
    printf("\n");

    printf("Lat bins= %4d\n",(int)OSMNodes->file.latbins);
    printf("Lon bins= %4d\n",(int)OSMNodes->file.lonbins);
    printf("\n");

    printf("Lat zero=%5d (%8.4f deg)\n",(int)OSMNodes->file.latzero,radians_to_degrees(latlong_to_radians(bin_to_latlong(OSMNodes->file.latzero))));
    printf("Lon zero=%5d (%8.4f deg)\n",(int)OSMNodes->file.lonzero,radians_to_degrees(latlong_to_radians(bin_to_latlong(OSMNodes->file.lonzero))));

    /* Examine the segments */

    printf("\n");
    printf("Segments\n");
    printf("--------\n");
    printf("\n");

    printf("sizeof(Segment)=%9lu Bytes\n",(unsigned long)sizeof(Segment));
    printf("Number(total)  =%9"Pindex_t"\n",OSMSegments->file.number);
    printf("Number(super)  =%9"Pindex_t"\n",OSMSegments->file.snumber);
    printf("Number(normal) =%9"Pindex_t"\n",OSMSegments->file.nnumber);

    /* Examine the ways */

    printf("\n");
    printf("Ways\n");
    printf("----\n");
    printf("\n");

    printf("sizeof(Way)=%9lu Bytes\n",(unsigned long)sizeof(Way));
    printf("Number     =%9"Pindex_t"\n",OSMWays->file.number);
    printf("\n");

    stat(ways_filename,&buf);
    printf("Total names=%9lu Bytes\n",(unsigned long)buf.st_size-(unsigned long)sizeof(Ways)-(unsigned long)OSMWays->file.number*(unsigned long)sizeof(Way));
    printf("\n");

    printf("Included highways  : %s\n",HighwaysNameList(OSMWays->file.highways));
    printf("Included transports: %s\n",AllowedNameList(OSMWays->file.allow));
    printf("Included properties: %s\n",PropertiesNameList(OSMWays->file.props));

    /* Examine the relations */

    printf("\n");
    printf("Relations\n");
    printf("---------\n");
    printf("\n");

    printf("sizeof(TurnRelation)=%9lu Bytes\n",(unsigned long)sizeof(TurnRelation));
    printf("Number              =%9"Pindex_t"\n",OSMRelations->file.trnumber);
   }

 /* Print out internal data (in plain text format) */

 if(option_dump)
   {
    index_t item;

    for(arg=1;arg<argc;arg++)
       if(!strcmp(argv[arg],"--node=all"))
         {
          for(item=0;item<OSMNodes->file.number;item++)
             print_node(OSMNodes,item);
         }
       else if(!strncmp(argv[arg],"--node=",7))
         {
          item=atoi(&argv[arg][7]);

          if(item>=0 && item<OSMNodes->file.number)
             print_node(OSMNodes,item);
          else
             printf("Invalid node number; minimum=0, maximum=%"Pindex_t".\n",OSMNodes->file.number-1);
         }
       else if(!strcmp(argv[arg],"--segment=all"))
         {
          for(item=0;item<OSMSegments->file.number;item++)
             print_segment(OSMSegments,item);
         }
       else if(!strncmp(argv[arg],"--segment=",10))
         {
          item=atoi(&argv[arg][10]);

          if(item>=0 && item<OSMSegments->file.number)
             print_segment(OSMSegments,item);
          else
             printf("Invalid segment number; minimum=0, maximum=%"Pindex_t".\n",OSMSegments->file.number-1);
         }
       else if(!strcmp(argv[arg],"--way=all"))
         {
          for(item=0;item<OSMWays->file.number;item++)
             print_way(OSMWays,item);
         }
       else if(!strncmp(argv[arg],"--way=",6))
         {
          item=atoi(&argv[arg][6]);

          if(item>=0 && item<OSMWays->file.number)
             print_way(OSMWays,item);
          else
             printf("Invalid way number; minimum=0, maximum=%"Pindex_t".\n",OSMWays->file.number-1);
         }
       else if(!strcmp(argv[arg],"--turn-relation=all"))
         {
          for(item=0;item<OSMRelations->file.trnumber;item++)
             print_turn_relation(OSMRelations,item,OSMSegments,OSMNodes);
         }
       else if(!strncmp(argv[arg],"--turn-relation=",16))
         {
          item=atoi(&argv[arg][16]);

          if(item>=0 && item<OSMRelations->file.trnumber)
             print_turn_relation(OSMRelations,item,OSMSegments,OSMNodes);
          else
             printf("Invalid turn relation number; minimum=0, maximum=%"Pindex_t".\n",OSMRelations->file.trnumber-1);
         }
   }

 /* Print out internal data (in OSM XML format) */

 if(option_dump_osm)
   {
    if(coordcount>0 && coordcount!=4)
       print_usage(0,NULL,"The --dump-osm option must have all of --latmin, --latmax, --lonmin, --lonmax or none.\n");

    print_head_osm(coordcount,latmin,latmax,lonmin,lonmax);

    if(coordcount)
       print_region_osm(OSMNodes,OSMSegments,OSMWays,OSMRelations,latmin,latmax,lonmin,lonmax,option_no_super);
    else
      {
       index_t item;

       for(item=0;item<OSMNodes->file.number;item++)
          print_node_osm(OSMNodes,item);

       for(item=0;item<OSMSegments->file.number;item++)
          if(!option_no_super || IsNormalSegment(LookupSegment(OSMSegments,item,1)))
             print_segment_osm(OSMSegments,item,OSMWays);

       for(item=0;item<OSMRelations->file.trnumber;item++)
          print_turn_relation_osm(OSMRelations,item,OSMSegments,OSMNodes);
      }

    print_tail_osm();
   }

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Print out the contents of a node from the routing database (as plain text).

  Nodes *nodes The set of nodes to use.

  index_t item The node index to print.
  ++++++++++++++++++++++++++++++++++++++*/

static void print_node(Nodes *nodes,index_t item)
{
 Node *nodep=LookupNode(nodes,item,1);
 double latitude,longitude;

 GetLatLong(nodes,item,&latitude,&longitude);

 printf("Node %"Pindex_t"\n",item);
 printf("  firstseg=%"Pindex_t"\n",nodep->firstseg);
 printf("  latoffset=%d lonoffset=%d (latitude=%.6f longitude=%.6f)\n",nodep->latoffset,nodep->lonoffset,radians_to_degrees(latitude),radians_to_degrees(longitude));
 printf("  allow=%02x (%s)\n",nodep->allow,AllowedNameList(nodep->allow));
 if(IsSuperNode(nodep))
    printf("  Super-Node\n");
 if(nodep->flags & NODE_MINIRNDBT)
    printf("  Mini-roundabout\n");
}


/*++++++++++++++++++++++++++++++++++++++
  Print out the contents of a segment from the routing database (as plain text).

  Segments *segments The set of segments to use.

  index_t item The segment index to print.
  ++++++++++++++++++++++++++++++++++++++*/

static void print_segment(Segments *segments,index_t item)
{
 Segment *segmentp=LookupSegment(segments,item,1);

 printf("Segment %"Pindex_t"\n",item);
 printf("  node1=%"Pindex_t" node2=%"Pindex_t"\n",segmentp->node1,segmentp->node2);
 printf("  next2=%"Pindex_t"\n",segmentp->next2);
 printf("  way=%"Pindex_t"\n",segmentp->way);
 printf("  distance=%d (%.3f km)\n",DISTANCE(segmentp->distance),distance_to_km(DISTANCE(segmentp->distance)));
 if(IsSuperSegment(segmentp) && IsNormalSegment(segmentp))
    printf("  Super-Segment AND normal Segment\n");
 else if(IsSuperSegment(segmentp) && !IsNormalSegment(segmentp))
    printf("  Super-Segment\n");
 if(IsOnewayTo(segmentp,segmentp->node1))
    printf("  One-Way from node2 to node1\n");
 if(IsOnewayTo(segmentp,segmentp->node2))
    printf("  One-Way from node1 to node2\n");
}


/*++++++++++++++++++++++++++++++++++++++
  Print out the contents of a way from the routing database (as plain text).

  Ways *ways The set of ways to use.

  index_t item The way index to print.
  ++++++++++++++++++++++++++++++++++++++*/

static void print_way(Ways *ways,index_t item)
{
 Way *wayp=LookupWay(ways,item,1);
 char *name=WayName(ways,wayp);

 printf("Way %"Pindex_t"\n",item);
 if(*name)
    printf("  name=%s\n",name);
 printf("  type=%02x (%s%s%s)\n",wayp->type,HighwayName(HIGHWAY(wayp->type)),wayp->type&Highway_OneWay?",One-Way":"",wayp->type&Highway_Roundabout?",Roundabout":"");
 printf("  allow=%02x (%s)\n",wayp->allow,AllowedNameList(wayp->allow));
 if(wayp->props)
    printf("  props=%02x (%s)\n",wayp->props,PropertiesNameList(wayp->props));
 if(wayp->speed)
    printf("  speed=%d (%d km/hr)\n",wayp->speed,speed_to_kph(wayp->speed));
 if(wayp->weight)
    printf("  weight=%d (%.1f tonnes)\n",wayp->weight,weight_to_tonnes(wayp->weight));
 if(wayp->height)
    printf("  height=%d (%.1f m)\n",wayp->height,height_to_metres(wayp->height));
 if(wayp->width)
    printf("  width=%d (%.1f m)\n",wayp->width,width_to_metres(wayp->width));
 if(wayp->length)
    printf("  length=%d (%.1f m)\n",wayp->length,length_to_metres(wayp->length));
}


/*++++++++++++++++++++++++++++++++++++++
  Print out the contents of a turn relation from the routing database (as plain text).

  Relations *relations The set of relations to use.

  index_t item The turn relation index to print.

  Segments *segments The set of segments to use.

  Nodes *nodes The set of nodes to use.
  ++++++++++++++++++++++++++++++++++++++*/

static void print_turn_relation(Relations *relations,index_t item,Segments *segments,Nodes *nodes)
{
 Segment *segmentp;
 TurnRelation *relationp=LookupTurnRelation(relations,item,1);
 Node *nodep=LookupNode(nodes,relationp->via,1);
 index_t from_way=NO_WAY,to_way=NO_WAY;
 index_t from_node=NO_NODE,to_node=NO_NODE;

 segmentp=FirstSegment(segments,nodep,1);

 do
   {
    index_t seg=IndexSegment(segments,segmentp);

    if(seg==relationp->from)
      {
       from_node=OtherNode(segmentp,relationp->from);
       from_way=segmentp->way;
      }

    if(seg==relationp->to)
      {
       to_node=OtherNode(segmentp,relationp->to);
       to_way=segmentp->way;
      }

    segmentp=NextSegment(segments,segmentp,relationp->via);
   }
 while(segmentp);

 printf("Relation %"Pindex_t"\n",item);
 printf("  from=%"Pindex_t" (segment) = %"Pindex_t" (way) = %"Pindex_t" (node)\n",relationp->from,from_way,from_node);
 printf("  via=%"Pindex_t" (node)\n",relationp->via);
 printf("  to=%"Pindex_t" (segment) = %"Pindex_t" (way) = %"Pindex_t" (node)\n",relationp->to,to_way,to_node);
 if(relationp->except)
    printf("  except=%02x (%s)\n",relationp->except,AllowedNameList(relationp->except));
}


/*++++++++++++++++++++++++++++++++++++++
  Print out a header in OSM XML format.

  int coordcount If true then include a bounding box.

  double latmin The minimum latitude.

  double latmax The maximum latitude.

  double lonmin The minimum longitude.

  double lonmax The maximum longitude.
  ++++++++++++++++++++++++++++++++++++++*/

static void print_head_osm(int coordcount,double latmin,double latmax,double lonmin,double lonmax)
{
 printf("<?xml version='1.0' encoding='UTF-8'?>\n");
 printf("<osm version='0.6' generator='Routino'>\n");

 if(coordcount)
    printf("  <bounds minlat='%.6f' maxlat='%.6f' minlon='%.6f' maxlon='%.6f' />\n",
           radians_to_degrees(latmin),radians_to_degrees(latmax),radians_to_degrees(lonmin),radians_to_degrees(lonmax));
}


/*++++++++++++++++++++++++++++++++++++++
  Print a region of the database in OSM XML format.

  Nodes *nodes The set of nodes to use.

  Segments *segments The set of segments to use.

  Ways *ways The set of ways to use.

  Relations *relations The set of relations to use.

  double latmin The minimum latitude.

  double latmax The maximum latitude.

  double lonmin The minimum longitude.

  double lonmax The maximum longitude.

  int option_no_super The option to print no super-segments.
  ++++++++++++++++++++++++++++++++++++++*/

static void print_region_osm(Nodes *nodes,Segments *segments,Ways *ways,Relations *relations,
                             double latmin,double latmax,double lonmin,double lonmax,int option_no_super)
{
 ll_bin_t latminbin=latlong_to_bin(radians_to_latlong(latmin))-nodes->file.latzero;
 ll_bin_t latmaxbin=latlong_to_bin(radians_to_latlong(latmax))-nodes->file.latzero;
 ll_bin_t lonminbin=latlong_to_bin(radians_to_latlong(lonmin))-nodes->file.lonzero;
 ll_bin_t lonmaxbin=latlong_to_bin(radians_to_latlong(lonmax))-nodes->file.lonzero;
 ll_bin_t latb,lonb;
 index_t item,index1,index2;

 if(latminbin<0)                   latminbin=0;
 if(latmaxbin>nodes->file.latbins) latmaxbin=nodes->file.latbins-1;
 if(lonminbin<0)                   lonminbin=0;
 if(lonmaxbin>nodes->file.lonbins) lonmaxbin=nodes->file.lonbins-1;

 /* Loop through all of the nodes. */

 for(latb=latminbin;latb<=latmaxbin;latb++)
    for(lonb=lonminbin;lonb<=lonmaxbin;lonb++)
      {
       ll_bin2_t llbin=lonb*nodes->file.latbins+latb;

       if(llbin<0 || llbin>(nodes->file.latbins*nodes->file.lonbins))
          continue;

       index1=LookupNodeOffset(nodes,llbin);
       index2=LookupNodeOffset(nodes,llbin+1);

       for(item=index1;item<index2;item++)
         {
          Node *nodep=LookupNode(nodes,item,1);
          double lat=latlong_to_radians(bin_to_latlong(nodes->file.latzero+latb)+off_to_latlong(nodep->latoffset));
          double lon=latlong_to_radians(bin_to_latlong(nodes->file.lonzero+lonb)+off_to_latlong(nodep->lonoffset));

          if(lat>latmin && lat<latmax && lon>lonmin && lon<lonmax)
            {
             Segment *segmentp;

             print_node_osm(nodes,item);

             segmentp=FirstSegment(segments,nodep,1);

             while(segmentp)
               {
                double olat,olon;
                index_t oitem=OtherNode(segmentp,item);

                GetLatLong(nodes,oitem,&olat,&olon);

                if(olat>latmin && olat<latmax && olon>lonmin && olon<lonmax)
                   if(item>oitem)
                      if(!option_no_super || IsNormalSegment(segmentp))
                         print_segment_osm(segments,IndexSegment(segments,segmentp),ways);

                segmentp=NextSegment(segments,segmentp,item);
               }

             if(IsTurnRestrictedNode(nodep))
               {
                index_t relindex=FindFirstTurnRelation1(relations,item);

                while(relindex!=NO_RELATION)
                  {
                   print_turn_relation_osm(relations,relindex,segments,nodes);

                   relindex=FindNextTurnRelation1(relations,relindex);
                  }
               }
            }
         }
      }
}


/*++++++++++++++++++++++++++++++++++++++
  Print out the contents of a node from the routing database (in OSM XML format).

  Nodes *nodes The set of nodes to use.

  index_t item The node index to print.
  ++++++++++++++++++++++++++++++++++++++*/

static void print_node_osm(Nodes *nodes,index_t item)
{
 Node *nodep=LookupNode(nodes,item,1);
 double latitude,longitude;
 int i;

 GetLatLong(nodes,item,&latitude,&longitude);

 if(nodep->allow==Transports_ALL && nodep->flags==0)
    printf("  <node id='%lu' lat='%.7f' lon='%.7f' version='1' />\n",(unsigned long)item+1,radians_to_degrees(latitude),radians_to_degrees(longitude));
 else
   {
    printf("  <node id='%lu' lat='%.7f' lon='%.7f' version='1'>\n",(unsigned long)item+1,radians_to_degrees(latitude),radians_to_degrees(longitude));

    if(nodep->flags & NODE_SUPER)
       printf("    <tag k='routino:super' v='yes' />\n");

    if(nodep->flags & NODE_UTURN)
       printf("    <tag k='routino:uturn' v='yes' />\n");

    if(nodep->flags & NODE_MINIRNDBT)
       printf("    <tag k='junction' v='roundabout' />\n");

    if(nodep->flags & NODE_TURNRSTRCT)
       printf("    <tag k='routino:turnrestriction' v='yes' />\n");

    for(i=1;i<Transport_Count;i++)
       if(!(nodep->allow & TRANSPORTS(i)))
          printf("    <tag k='%s' v='no' />\n",TransportName(i));

    printf("  </node>\n");
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Print out the contents of a segment from the routing database (as a way in OSM XML format).

  Segments *segments The set of segments to use.

  index_t item The segment index to print.

  Ways *ways The set of ways to use.
  ++++++++++++++++++++++++++++++++++++++*/

static void print_segment_osm(Segments *segments,index_t item,Ways *ways)
{
 Segment *segmentp=LookupSegment(segments,item,1);
 Way *wayp=LookupWay(ways,segmentp->way,1);
 char *name=WayName(ways,wayp);
 int i;

 printf("  <way id='%lu' version='1'>\n",(unsigned long)item+1);

 if(IsOnewayTo(segmentp,segmentp->node1))
   {
    printf("    <nd ref='%lu' />\n",(unsigned long)segmentp->node2+1);
    printf("    <nd ref='%lu' />\n",(unsigned long)segmentp->node1+1);
   }
 else
   {
    printf("    <nd ref='%lu' />\n",(unsigned long)segmentp->node1+1);
    printf("    <nd ref='%lu' />\n",(unsigned long)segmentp->node2+1);
   }

 if(IsSuperSegment(segmentp))
    printf("    <tag k='routino:super' v='yes' />\n");
 if(IsNormalSegment(segmentp))
    printf("    <tag k='routino:normal' v='yes' />\n");

 printf("    <tag k='routino:distance' v='%.3f' />\n",distance_to_km(DISTANCE(segmentp->distance)));

 if(wayp->type & Highway_OneWay)
    printf("    <tag k='oneway' v='yes' />\n");

 if(wayp->type & Highway_Roundabout)
    printf("    <tag k='roundabout' v='yes' />\n");

 printf("    <tag k='highway' v='%s' />\n",HighwayName(HIGHWAY(wayp->type)));

 if(IsNormalSegment(segmentp) && *name)
    printf("    <tag k='name' v='%s' />\n",ParseXML_Encode_Safe_XML(name));

 for(i=1;i<Transport_Count;i++)
    if(wayp->allow & TRANSPORTS(i))
       printf("    <tag k='%s' v='yes' />\n",TransportName(i));

 for(i=1;i<Property_Count;i++)
    if(wayp->props & PROPERTIES(i))
       printf("    <tag k='%s' v='yes' />\n",PropertyName(i));

 if(wayp->speed)
    printf("    <tag k='maxspeed' v='%d' />\n",speed_to_kph(wayp->speed));

 if(wayp->weight)
    printf("    <tag k='maxweight' v='%.1f' />\n",weight_to_tonnes(wayp->weight));
 if(wayp->height)
    printf("    <tag k='maxheight' v='%.1f' />\n",height_to_metres(wayp->height));
 if(wayp->width)
    printf("    <tag k='maxwidth' v='%.1f' />\n",width_to_metres(wayp->width));
 if(wayp->length)
    printf("    <tag k='maxlength' v='%.1f' />\n",length_to_metres(wayp->length));

 printf("  </way>\n");
}


/*++++++++++++++++++++++++++++++++++++++
  Print out the contents of a turn relation from the routing database (in OSM XML format).

  Relations *relations The set of relations to use.

  index_t item The relation index to print.

  Segments *segments The set of segments to use.

  Nodes *nodes The set of nodes to use.
  ++++++++++++++++++++++++++++++++++++++*/

static void print_turn_relation_osm(Relations *relations,index_t item,Segments *segments,Nodes *nodes)
{
 TurnRelation *relationp=LookupTurnRelation(relations,item,1);

 Segment *segmentp_from=LookupSegment(segments,relationp->from,1);
 Segment *segmentp_to  =LookupSegment(segments,relationp->to  ,2);

 double angle=TurnAngle(nodes,segmentp_from,segmentp_to,relationp->via);

 char *restriction;

 if(angle>150 || angle<-150)
    restriction="no_u_turn";
 else if(angle>30)
    restriction="no_right_turn";
 else if(angle<-30)
    restriction="no_left_turn";
 else
    restriction="no_straight_on";

 printf("  <relation id='%lu' version='1'>\n",(unsigned long)item+1);
 printf("    <tag k='type' v='restriction' />\n");
 printf("    <tag k='restriction' v='%s'/>\n",restriction);

 if(relationp->except)
    printf("    <tag k='except' v='%s' />\n",AllowedNameList(relationp->except));

 printf("    <member type='way' ref='%lu' role='from' />\n",(unsigned long)relationp->from+1);
 printf("    <member type='node' ref='%lu' role='via' />\n",(unsigned long)relationp->via+1);
 printf("    <member type='way' ref='%lu' role='to' />\n",(unsigned long)relationp->to+1);

 printf("  </relation>\n");
}


/*++++++++++++++++++++++++++++++++++++++
  Print out a tail in OSM XML format.
  ++++++++++++++++++++++++++++++++++++++*/

static void print_tail_osm(void)
{
 printf("</osm>\n");
}


/*+ Conversion from time_t to date string (day of week). +*/
static const char* const weekdays[7]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

/*+ Conversion from time_t to date string (month of year). +*/
static const char* const months[12]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};


/*++++++++++++++++++++++++++++++++++++++
  Convert the time into an RFC 822 compliant date.

  char *RFC822Date Returns a pointer to a fixed string containing the date.

  time_t t The time.
  ++++++++++++++++++++++++++++++++++++++*/

static char *RFC822Date(time_t t)
{
 static char value[32];
 char weekday[4];
 char month[4];
 struct tm *tim;

 tim=gmtime(&t);

 strcpy(weekday,weekdays[tim->tm_wday]);
 strcpy(month,months[tim->tm_mon]);

 /* Sun, 06 Nov 1994 08:49:37 GMT    ; RFC 822, updated by RFC 1123 */

 sprintf(value,"%3s, %02d %3s %4d %02d:%02d:%02d %s",
         weekday,
         tim->tm_mday,
         month,
         tim->tm_year+1900,
         tim->tm_hour,
         tim->tm_min,
         tim->tm_sec,
         "GMT"
         );

 return(value);
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
         "                  [--statistics]\n"
         "                  [--visualiser --latmin=<latmin> --latmax=<latmax>\n"
         "                                --lonmin=<lonmin> --lonmax=<lonmax>\n"
         "                                --data=<data-type>]\n"
         "                  [--dump [--node=<node> ...]\n"
         "                          [--segment=<segment> ...]\n"
         "                          [--way=<way> ...]\n"
         "                          [--turn-relation=<rel> ...]]\n"
         "                  [--dump-osm [--no-super]\n"
         "                              [--latmin=<latmin> --latmax=<latmax>\n"
         "                               --lonmin=<lonmin> --lonmax=<lonmax>]]\n");

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
            "--statistics              Print statistics about the routing database.\n"
            "\n"
            "--visualiser              Extract selected data from the routing database:\n"
            "  --latmin=<latmin>       * the minimum latitude (degrees N).\n"
            "  --latmax=<latmax>       * the maximum latitude (degrees N).\n"
            "  --lonmin=<lonmin>       * the minimum longitude (degrees E).\n"
            "  --lonmax=<lonmax>       * the maximum longitude (degrees E).\n"
            "  --data=<data-type>      * the type of data to select.\n"
            "\n"
            "  <data-type> can be selected from:\n"
            "      junctions   = segment count at each junction.\n"
            "      super       = super-node and super-segments.\n"
            "      oneway      = oneway segments.\n"
            "      highway-*   = segments of the specified highway type.\n"
            "      transport-* = segments allowing the specified transport type.\n"
            "      barrier-*   = nodes disallowing the specified transport type.\n"
            "      turns       = turn restrictions.\n"
            "      speed       = speed limits.\n"
            "      weight      = weight limits.\n"
            "      height      = height limits.\n"
            "      width       = width limits.\n"
            "      length      = length limits.\n"
            "      property-*  = segments with the specified property.\n"
            "\n"
            "--dump                    Dump selected contents of the database.\n"
            "  --node=<node>           * the node with the selected index.\n"
            "  --segment=<segment>     * the segment with the selected index.\n"
            "  --way=<way>             * the way with the selected index.\n"
            "  --turn-relation=<rel>   * the turn relation with the selected index.\n"
            "                          Use 'all' instead of a number to get all of them.\n"
            "\n"
            "--dump-osm                Dump all or part of the database as an XML file.\n"
            "  --no-super              * exclude the super-segments.\n"
            "  --latmin=<latmin>       * the minimum latitude (degrees N).\n"
            "  --latmax=<latmax>       * the maximum latitude (degrees N).\n"
            "  --lonmin=<lonmin>       * the minimum longitude (degrees E).\n"
            "  --lonmax=<lonmax>       * the maximum longitude (degrees E).\n");

 exit(!detail);
}
