/***************************************
 Routing output generator.

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
#include <ctype.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <unistd.h>

#include "types.h"
#include "nodes.h"
#include "segments.h"
#include "ways.h"

#include "functions.h"
#include "fakes.h"
#include "translations.h"
#include "results.h"
#include "xmlparse.h"


/* Constants */

#define IMP_IGNORE      -1      /*+ Ignore this point. +*/
#define IMP_UNIMPORTANT  0      /*+ An unimportant, intermediate, node. +*/
#define IMP_RB_NOT_EXIT  1      /*+ A roundabout exit that is not taken. +*/
#define IMP_JUNCT_CONT   2      /*+ An un-interesting junction where the route continues without comment. +*/
#define IMP_CHANGE       3      /*+ The highway changes type but nothing else happens. +*/
#define IMP_JUNCT_IMPORT 4      /*+ An interesting junction to be described. +*/
#define IMP_RB_ENTRY     5      /*+ The entrance to a roundabout. +*/
#define IMP_RB_EXIT      6      /*+ The exit from a roundabout. +*/
#define IMP_MINI_RB      7      /*+ The location of a mini-roundabout. +*/
#define IMP_UTURN        8      /*+ The location of a U-turn. +*/
#define IMP_WAYPOINT     9      /*+ A waypoint. +*/


/* Global variables */

/*+ The option to calculate the quickest route insted of the shortest. +*/
extern int option_quickest;

/*+ The options to select the format of the output. +*/
extern int option_html,option_gpx_track,option_gpx_route,option_text,option_text_all;


/* Local variables */

/*+ Heuristics for determining if a junction is important. +*/
static char junction_other_way[Highway_Count][Highway_Count]=
 { /* M, T, P, S, T, U, R, S, T, C, P, S, F = Way type of route not taken */
  {   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }, /* Motorway     */
  {   1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }, /* Trunk        */
  {   1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }, /* Primary      */
  {   1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1 }, /* Secondary    */
  {   1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1 }, /* Tertiary     */
  {   1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1 }, /* Unclassified */
  {   1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1 }, /* Residential  */
  {   1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1 }, /* Service      */
  {   1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1 }, /* Track        */
  {   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1 }, /* Cycleway     */
  {   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, /* Path         */
  {   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, /* Steps        */
  {   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, /* Ferry        */
 };


/*++++++++++++++++++++++++++++++++++++++
  Print the optimum route between two nodes.

  Results **results The set of results to print (some may be NULL - ignore them).

  int nresults The number of results in the list.

  Nodes *nodes The set of nodes to use.

  Segments *segments The set of segments to use.

  Ways *ways The set of ways to use.

  Profile *profile The profile containing the transport type, speeds and allowed highways.
  ++++++++++++++++++++++++++++++++++++++*/

void PrintRoute(Results **results,int nresults,Nodes *nodes,Segments *segments,Ways *ways,Profile *profile)
{
 FILE *htmlfile=NULL,*gpxtrackfile=NULL,*gpxroutefile=NULL,*textfile=NULL,*textallfile=NULL;

 char *prev_bearing=NULL,*prev_wayname=NULL;
 distance_t cum_distance=0;
 duration_t cum_duration=0;

 int point=1;
 int segment_count=0,route_count=0;
 int point_count=0;
 int roundabout=0;

 /* Open the files */

 if(option_quickest==0)
   {
    /* Print the result for the shortest route */

    if(option_html)
       htmlfile    =fopen("shortest.html","w");
    if(option_gpx_track)
       gpxtrackfile=fopen("shortest-track.gpx","w");
    if(option_gpx_route)
       gpxroutefile=fopen("shortest-route.gpx","w");
    if(option_text)
       textfile    =fopen("shortest.txt","w");
    if(option_text_all)
       textallfile =fopen("shortest-all.txt","w");

    if(option_html && !htmlfile)
       fprintf(stderr,"Warning: Cannot open file 'shortest.html' for writing [%s].\n",strerror(errno));
    if(option_gpx_track && !gpxtrackfile)
       fprintf(stderr,"Warning: Cannot open file 'shortest-track.gpx' for writing [%s].\n",strerror(errno));
    if(option_gpx_route && !gpxroutefile)
       fprintf(stderr,"Warning: Cannot open file 'shortest-route.gpx' for writing [%s].\n",strerror(errno));
    if(option_text && !textfile)
       fprintf(stderr,"Warning: Cannot open file 'shortest.txt' for writing [%s].\n",strerror(errno));
    if(option_text_all && !textallfile)
       fprintf(stderr,"Warning: Cannot open file 'shortest-all.txt' for writing [%s].\n",strerror(errno));
   }
 else
   {
    /* Print the result for the quickest route */

    if(option_html)
       htmlfile    =fopen("quickest.html","w");
    if(option_gpx_track)
       gpxtrackfile=fopen("quickest-track.gpx","w");
    if(option_gpx_route)
       gpxroutefile=fopen("quickest-route.gpx","w");
    if(option_text)
       textfile    =fopen("quickest.txt","w");
    if(option_text_all)
       textallfile =fopen("quickest-all.txt","w");

    if(option_html && !htmlfile)
       fprintf(stderr,"Warning: Cannot open file 'quickest.html' for writing [%s].\n",strerror(errno));
    if(option_gpx_track && !gpxtrackfile)
       fprintf(stderr,"Warning: Cannot open file 'quickest-track.gpx' for writing [%s].\n",strerror(errno));
    if(option_gpx_route && !gpxroutefile)
       fprintf(stderr,"Warning: Cannot open file 'quickest-route.gpx' for writing [%s].\n",strerror(errno));
    if(option_text && !textfile)
       fprintf(stderr,"Warning: Cannot open file 'quickest.txt' for writing [%s].\n",strerror(errno));
    if(option_text_all && !textallfile)
       fprintf(stderr,"Warning: Cannot open file 'quickest-all.txt' for writing [%s].\n",strerror(errno));
   }

 /* Print the head of the files */

 if(htmlfile)
   {
    fprintf(htmlfile,"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\n");
    fprintf(htmlfile,"<HTML>\n");
    if(translate_xml_copyright_creator[0] && translate_xml_copyright_creator[1])
       fprintf(htmlfile,"<!-- %s : %s -->\n",translate_xml_copyright_creator[0],translate_xml_copyright_creator[1]);
    if(translate_xml_copyright_source[0] && translate_xml_copyright_source[1])
       fprintf(htmlfile,"<!-- %s : %s -->\n",translate_xml_copyright_source[0],translate_xml_copyright_source[1]);
    if(translate_xml_copyright_license[0] && translate_xml_copyright_license[1])
       fprintf(htmlfile,"<!-- %s : %s -->\n",translate_xml_copyright_license[0],translate_xml_copyright_license[1]);
    fprintf(htmlfile,"<HEAD>\n");
    fprintf(htmlfile,"<TITLE>");
    fprintf(htmlfile,translate_html_title,option_quickest?translate_xml_route_quickest:translate_xml_route_shortest);
    fprintf(htmlfile,"</TITLE>\n");
    fprintf(htmlfile,"<META http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n");
    fprintf(htmlfile,"<STYLE type=\"text/css\">\n");
    fprintf(htmlfile,"<!--\n");
    fprintf(htmlfile,"   table   {table-layout: fixed; border: none; border-collapse: collapse;}\n");
    fprintf(htmlfile,"   table.c {color: grey; font-size: x-small;} /* copyright */\n");
    fprintf(htmlfile,"   tr      {border: 0px;}\n");
    fprintf(htmlfile,"   tr.c    {display: none;} /* coords */\n");
    fprintf(htmlfile,"   tr.n    {} /* node */\n");
    fprintf(htmlfile,"   tr.s    {} /* segment */\n");
    fprintf(htmlfile,"   tr.t    {font-weight: bold;} /* total */\n");
    fprintf(htmlfile,"   td.l    {font-weight: bold;}\n");
    fprintf(htmlfile,"   td.r    {}\n");
    fprintf(htmlfile,"   span.w  {font-weight: bold;} /* waypoint */\n");
    fprintf(htmlfile,"   span.h  {text-decoration: underline;} /* highway */\n");
    fprintf(htmlfile,"   span.d  {} /* segment distance */\n");
    fprintf(htmlfile,"   span.j  {font-style: italic;} /* total journey distance */\n");
    fprintf(htmlfile,"   span.t  {font-variant: small-caps;} /* turn */\n");
    fprintf(htmlfile,"   span.b  {font-variant: small-caps;} /* bearing */\n");
    fprintf(htmlfile,"-->\n");
    fprintf(htmlfile,"</STYLE>\n");
    fprintf(htmlfile,"</HEAD>\n");
    fprintf(htmlfile,"<BODY>\n");
    fprintf(htmlfile,"<H1>");
    fprintf(htmlfile,translate_html_title,option_quickest?translate_xml_route_quickest:translate_xml_route_shortest);
    fprintf(htmlfile,"</H1>\n");
    fprintf(htmlfile,"<table>\n");
   }

 if(gpxtrackfile)
   {
    fprintf(gpxtrackfile,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(gpxtrackfile,"<gpx version=\"1.1\" creator=\"Routino\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns=\"http://www.topografix.com/GPX/1/1\" xsi:schemaLocation=\"http://www.topografix.com/GPX/1/1 http://www.topografix.com/GPX/1/1/gpx.xsd\">\n");

    fprintf(gpxtrackfile,"<metadata>\n");
    fprintf(gpxtrackfile,"<desc>%s : %s</desc>\n",translate_xml_copyright_creator[0],translate_xml_copyright_creator[1]);
    if(translate_xml_copyright_source[1])
      {
       fprintf(gpxtrackfile,"<copyright author=\"%s\">\n",translate_xml_copyright_source[1]);

       if(translate_xml_copyright_license[1])
          fprintf(gpxtrackfile,"<license>%s</license>\n",translate_xml_copyright_license[1]);

       fprintf(gpxtrackfile,"</copyright>\n");
      }
    fprintf(gpxtrackfile,"</metadata>\n");

    fprintf(gpxtrackfile,"<trk>\n");
    fprintf(gpxtrackfile,"<name>");
    fprintf(gpxtrackfile,translate_gpx_name,option_quickest?translate_xml_route_quickest:translate_xml_route_shortest);
    fprintf(gpxtrackfile,"</name>\n");
    fprintf(gpxtrackfile,"<desc>");
    fprintf(gpxtrackfile,translate_gpx_desc,option_quickest?translate_xml_route_quickest:translate_xml_route_shortest);
    fprintf(gpxtrackfile,"</desc>\n");
   }

 if(gpxroutefile)
   {
    fprintf(gpxroutefile,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(gpxroutefile,"<gpx version=\"1.1\" creator=\"Routino\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns=\"http://www.topografix.com/GPX/1/1\" xsi:schemaLocation=\"http://www.topografix.com/GPX/1/1 http://www.topografix.com/GPX/1/1/gpx.xsd\">\n");

    fprintf(gpxroutefile,"<metadata>\n");
    fprintf(gpxroutefile,"<desc>%s : %s</desc>\n",translate_xml_copyright_creator[0],translate_xml_copyright_creator[1]);
    if(translate_xml_copyright_source[1])
      {
       fprintf(gpxroutefile,"<copyright author=\"%s\">\n",translate_xml_copyright_source[1]);

       if(translate_xml_copyright_license[1])
          fprintf(gpxroutefile,"<license>%s</license>\n",translate_xml_copyright_license[1]);

       fprintf(gpxroutefile,"</copyright>\n");
      }
    fprintf(gpxroutefile,"</metadata>\n");

    fprintf(gpxroutefile,"<rte>\n");
    fprintf(gpxroutefile,"<name>");
    fprintf(gpxroutefile,translate_gpx_name,option_quickest?translate_xml_route_quickest:translate_xml_route_shortest);
    fprintf(gpxroutefile,"</name>\n");
    fprintf(gpxroutefile,"<desc>");
    fprintf(gpxroutefile,translate_gpx_desc,option_quickest?translate_xml_route_quickest:translate_xml_route_shortest);
    fprintf(gpxroutefile,"</desc>\n");
   }

 if(textfile)
   {
    if(translate_raw_copyright_creator[0] && translate_raw_copyright_creator[1])
       fprintf(textfile,"# %s : %s\n",translate_raw_copyright_creator[0],translate_raw_copyright_creator[1]);
    if(translate_raw_copyright_source[0] && translate_raw_copyright_source[1])
       fprintf(textfile,"# %s : %s\n",translate_raw_copyright_source[0],translate_raw_copyright_source[1]);
    if(translate_raw_copyright_license[0] && translate_raw_copyright_license[1])
       fprintf(textfile,"# %s : %s\n",translate_raw_copyright_license[0],translate_raw_copyright_license[1]);
    if((translate_raw_copyright_creator[0] && translate_raw_copyright_creator[1]) ||
       (translate_raw_copyright_source[0]  && translate_raw_copyright_source[1]) ||
       (translate_raw_copyright_license[0] && translate_raw_copyright_license[1]))
       fprintf(textfile,"#\n");

    fprintf(textfile,"#Latitude\tLongitude\tSection \tSection \tTotal   \tTotal   \tPoint\tTurn\tBearing\tHighway\n");
    fprintf(textfile,"#        \t         \tDistance\tDuration\tDistance\tDuration\tType \t    \t       \t       \n");
                     /* "%10.6f\t%11.6f\t%6.3f km\t%4.1f min\t%5.1f km\t%4.0f min\t%s\t %+d\t %+d\t%s\n" */
   }

 if(textallfile)
   {
    if(translate_raw_copyright_creator[0] && translate_raw_copyright_creator[1])
       fprintf(textallfile,"# %s : %s\n",translate_raw_copyright_creator[0],translate_raw_copyright_creator[1]);
    if(translate_raw_copyright_source[0] && translate_raw_copyright_source[1])
       fprintf(textallfile,"# %s : %s\n",translate_raw_copyright_source[0],translate_raw_copyright_source[1]);
    if(translate_raw_copyright_license[0] && translate_raw_copyright_license[1])
       fprintf(textallfile,"# %s : %s\n",translate_raw_copyright_license[0],translate_raw_copyright_license[1]);
    if((translate_raw_copyright_creator[0] && translate_raw_copyright_creator[1]) ||
       (translate_raw_copyright_source[0]  && translate_raw_copyright_source[1]) ||
       (translate_raw_copyright_license[0] && translate_raw_copyright_license[1]))
       fprintf(textallfile,"#\n");

    fprintf(textallfile,"#Latitude\tLongitude\t    Node\tType\tSegment\tSegment\tTotal\tTotal  \tSpeed\tBearing\tHighway\n");
    fprintf(textallfile,"#        \t         \t        \t    \tDist   \tDurat'n\tDist \tDurat'n\t     \t       \t       \n");
                        /* "%10.6f\t%11.6f\t%8d%c\t%s\t%5.3f\t%5.2f\t%5.2f\t%5.1f\t%3d\t%4d\t%s\n" */
   }

 /* Loop through all the sections of the route and print them */

 while(!results[point])
    point++;

 while(point<=nresults)
   {
    int next_point=point;
    Result *result;

    result=FindResult(results[point],results[point]->start_node,results[point]->prev_segment);

    /* Print the start of the segment */

    if(gpxtrackfile)
       fprintf(gpxtrackfile,"<trkseg>\n");

    /* Loop through all the points within a section of the route and print them */

    do
      {
       double latitude,longitude;
       Result *next_result;


       /* Calculate the information about this point */

       if(IsFakeNode(result->node))
          GetFakeLatLong(result->node,&latitude,&longitude);
       else
          GetLatLong(nodes,result->node,&latitude,&longitude);

       /* Calculate the next result */

       next_result=result->next;

       if(!next_result)
          for(next_point=point+1;next_point<=nresults;next_point++)
             if(results[next_point])
               {
                next_result=FindResult(results[next_point],results[next_point]->start_node,results[next_point]->prev_segment);
                next_result=next_result->next;
                break;
               }

       /* Print out all of the results */

       if(gpxtrackfile)
          fprintf(gpxtrackfile,"<trkpt lat=\"%.6f\" lon=\"%.6f\"/>\n",
                               radians_to_degrees(latitude),radians_to_degrees(longitude));

       result=next_result;

      }
    while(point==next_point);

    /* Print the end of the segment */

    if(gpxtrackfile)
       fprintf(gpxtrackfile,"</trkseg>\n");

    point=next_point;
   }

 /* Print the tail of the files */

 if(htmlfile)
   {
    fprintf(htmlfile,"</table>\n");

    if((translate_xml_copyright_creator[0] && translate_xml_copyright_creator[1]) ||
       (translate_xml_copyright_source[0]  && translate_xml_copyright_source[1]) ||
       (translate_xml_copyright_license[0] && translate_xml_copyright_license[1]))
      {
       fprintf(htmlfile,"<p>\n");
       fprintf(htmlfile,"<table class='c'>\n");
       if(translate_xml_copyright_creator[0] && translate_xml_copyright_creator[1])
          fprintf(htmlfile,"<tr><td class='l'>%s:<td class='r'>%s\n",translate_xml_copyright_creator[0],translate_xml_copyright_creator[1]);
       if(translate_xml_copyright_source[0] && translate_xml_copyright_source[1])
          fprintf(htmlfile,"<tr><td class='l'>%s:<td class='r'>%s\n",translate_xml_copyright_source[0],translate_xml_copyright_source[1]);
       if(translate_xml_copyright_license[0] && translate_xml_copyright_license[1])
          fprintf(htmlfile,"<tr><td class='l'>%s:<td class='r'>%s\n",translate_xml_copyright_license[0],translate_xml_copyright_license[1]);
       fprintf(htmlfile,"</table>\n");
      }

    fprintf(htmlfile,"</BODY>\n");
    fprintf(htmlfile,"</HTML>\n");
   }

 if(gpxtrackfile)
   {
    fprintf(gpxtrackfile,"</trk>\n");
    fprintf(gpxtrackfile,"</gpx>\n");
   }

 if(gpxroutefile)
   {
    fprintf(gpxroutefile,"</rte>\n");
    fprintf(gpxroutefile,"</gpx>\n");
   }

 /* Close the files */

 if(htmlfile)
    fclose(htmlfile);
 if(gpxtrackfile)
    fclose(gpxtrackfile);
 if(gpxroutefile)
    fclose(gpxroutefile);
 if(textfile)
    fclose(textfile);
 if(textallfile)
    fclose(textallfile);
}
