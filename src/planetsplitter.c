/***************************************
 OSM planet file splitter.

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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

#include "types.h"
#include "ways.h"

#include "typesx.h"
#include "nodesx.h"
#include "segmentsx.h"
#include "waysx.h"
#include "relationsx.h"
#include "superx.h"
#include "prunex.h"

#include "files.h"
#include "logging.h"
#include "functions.h"
#include "osmparser.h"
#include "tagging.h"
#include "uncompress.h"


/* Global variables */

/*+ The name of the temporary directory. +*/
char *option_tmpdirname=NULL;

/*+ The amount of RAM to use for filesorting. +*/
size_t option_filesort_ramsize=0;

/*+ The number of threads to use for filesorting. +*/
int option_filesort_threads=1;


/* Local functions */

static void print_usage(int detail,const char *argerr,const char *err);


/*++++++++++++++++++++++++++++++++++++++
  The main program for the planetsplitter.
  ++++++++++++++++++++++++++++++++++++++*/

int main(int argc,char** argv)
{
 struct timeval start_time;
 NodesX     *Nodes;
 SegmentsX  *Segments,*SuperSegments=NULL,*MergedSegments=NULL;
 WaysX      *Ways;
 RelationsX *Relations;
 int         iteration=0,quit=0;
 int         max_iterations=5;
 char       *dirname=NULL,*prefix=NULL,*tagging=NULL,*errorlog=NULL;
 int         option_parse_only=0,option_process_only=0;
 int         option_append=0,option_keep=0,option_changes=0;
 int         option_filenames=0;
 int         option_prune_isolated=500,option_prune_short=5,option_prune_straight=3;
 int         arg;

 gettimeofday(&start_time,NULL);

 /* Parse the command line arguments */

 for(arg=1;arg<argc;arg++)
   {
    if(!strcmp(argv[arg],"--help"))
       print_usage(1,NULL,NULL);
    else if(!strncmp(argv[arg],"--dir=",6))
       dirname=&argv[arg][6];
    else if(!strncmp(argv[arg],"--prefix=",9))
       prefix=&argv[arg][9];
    else if(!strncmp(argv[arg],"--sort-ram-size=",16))
       option_filesort_ramsize=atoi(&argv[arg][16]);
#if defined(USE_PTHREADS) && USE_PTHREADS
    else if(!strncmp(argv[arg],"--sort-threads=",15))
       option_filesort_threads=atoi(&argv[arg][15]);
#endif
    else if(!strncmp(argv[arg],"--tmpdir=",9))
       option_tmpdirname=&argv[arg][9];
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
    else if(!strcmp(argv[arg],"--parse-only"))
       option_parse_only=1;
    else if(!strcmp(argv[arg],"--process-only"))
       option_process_only=1;
    else if(!strcmp(argv[arg],"--append"))
       option_append=1;
    else if(!strcmp(argv[arg],"--keep"))
       option_keep=1;
    else if(!strcmp(argv[arg],"--changes"))
       option_changes=1;
    else if(!strncmp(argv[arg],"--max-iterations=",17))
       max_iterations=atoi(&argv[arg][17]);
    else if(!strncmp(argv[arg],"--prune",7))
      {
       if(!strcmp(&argv[arg][7],"-none"))
          option_prune_isolated=option_prune_short=option_prune_straight=0;
       else if(!strncmp(&argv[arg][7],"-isolated=",10))
          option_prune_isolated=atoi(&argv[arg][17]);
       else if(!strncmp(&argv[arg][7],"-short=",7))
          option_prune_short=atoi(&argv[arg][14]);
       else if(!strncmp(&argv[arg][7],"-straight=",10))
          option_prune_straight=atoi(&argv[arg][17]);
       else
          print_usage(0,argv[arg],NULL);
      }
    else if(argv[arg][0]=='-' && argv[arg][1]=='-')
       print_usage(0,argv[arg],NULL);
    else
       option_filenames++;
   }

 /* Check the specified command line options */

 if(option_parse_only && option_process_only)
    print_usage(0,NULL,"Cannot use '--parse-only' and '--process-only' at the same time.");

 if(option_append && option_process_only)
    print_usage(0,NULL,"Cannot use '--append' and '--process-only' at the same time.");

 if(option_filenames && option_process_only)
    print_usage(0,NULL,"Cannot use '--process-only' and filenames at the same time.");

 if(!option_filenames && !option_process_only)
    print_usage(0,NULL,"File names must be specified unless using '--process-only'");

 if(!option_filesort_ramsize)
   {
#if SLIM
    option_filesort_ramsize=64*1024*1024;
#else
    option_filesort_ramsize=256*1024*1024;
#endif
   }
 else
    option_filesort_ramsize*=1024*1024;

 if(!option_tmpdirname)
   {
    if(!dirname)
       option_tmpdirname=".";
    else
       option_tmpdirname=dirname;
   }

 if(!option_process_only)
   {
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
       if(ExistsFile(FileName(dirname,prefix,"tagging.xml")))
          tagging=FileName(dirname,prefix,"tagging.xml");
       else if(ExistsFile(FileName(DATADIR,NULL,"tagging.xml")))
          tagging=FileName(DATADIR,NULL,"tagging.xml");
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
   }

 /* Create new node, segment, way and relation variables */

 Nodes=NewNodeList(option_append||option_changes,option_process_only);

 Segments=NewSegmentList(option_append||option_changes,option_process_only);

 Ways=NewWayList(option_append||option_changes,option_process_only);

 Relations=NewRelationList(option_append||option_changes,option_process_only);

 /* Create the error log file */

 if(errorlog)
    open_errorlog(FileName(dirname,prefix,errorlog),option_append||option_changes||option_process_only);

 /* Parse the file */

if(!option_process_only)
  {
   for(arg=1;arg<argc;arg++)
     {
      int fd;
      char *filename,*p;

      if(argv[arg][0]=='-' && argv[arg][1]=='-')
         continue;

      filename=strcpy(malloc(strlen(argv[arg])+1),argv[arg]);

      fd=ReOpenFile(filename);

      if((p=strstr(filename,".bz2")) && !strcmp(p,".bz2"))
        {
         fd=Uncompress_Bzip2(fd);
         *p=0;
        }

      if((p=strstr(filename,".gz")) && !strcmp(p,".gz"))
        {
         fd=Uncompress_Gzip(fd);
         *p=0;
        }

      if(option_changes)
        {
         printf("\nParse OSC Data [%s]\n==============\n\n",filename);
         fflush(stdout);

         if((p=strstr(filename,".pbf")) && !strcmp(p,".pbf"))
           {
            logassert(0,"Unable to read a PBF file to apply changes (format does not permit this)");
           }
         else if((p=strstr(filename,".o5c")) && !strcmp(p,".o5c"))
           {
            if(ParseO5CFile(fd,Nodes,Segments,Ways,Relations))
               exit(EXIT_FAILURE);
           }
         else
           {
            if(ParseOSCFile(fd,Nodes,Segments,Ways,Relations))
               exit(EXIT_FAILURE);
           }
        }
      else
        {
         printf("\nParse OSM Data [%s]\n==============\n\n",filename);
         fflush(stdout);

         if((p=strstr(filename,".pbf")) && !strcmp(p,".pbf"))
           {
            if(ParsePBFFile(fd,Nodes,Segments,Ways,Relations))
               exit(EXIT_FAILURE);
           }
         else if((p=strstr(filename,".o5m")) && !strcmp(p,".o5m"))
           {
            if(ParseO5MFile(fd,Nodes,Segments,Ways,Relations))
               exit(EXIT_FAILURE);
           }
         else
           {
            if(ParseOSMFile(fd,Nodes,Segments,Ways,Relations))
               exit(EXIT_FAILURE);
           }
        }

      CloseFile(fd);

      free(filename);
     }

   DeleteXMLTaggingRules();
  }

 FinishNodeList(Nodes);
 FinishSegmentList(Segments);
 FinishWayList(Ways);
 FinishRelationList(Relations);

 if(option_parse_only)
   {
    FreeNodeList(Nodes,1);
    FreeSegmentList(Segments,1);
    FreeWayList(Ways,1);
    FreeRelationList(Relations,1);

    return(0);
   }


 /* Sort the data */

 printf("\nSort OSM Data\n=============\n\n");
 fflush(stdout);

 /* Sort the nodes, segments, ways and relations */

 SortNodeList(Nodes);

 if(option_changes)
    ApplySegmentChanges(Segments);

 SortSegmentList(Segments);

 SortWayList(Ways);

 SortRelationList(Relations);

 /* Process the data */

 printf("\nProcess OSM Data\n================\n\n");
 fflush(stdout);

 /* Extract the way names (must be before using the ways) */

 ExtractWayNames(Ways,option_keep||option_changes);

 /* Remove bad segments (must be after sorting the nodes, segments and ways) */

 RemoveBadSegments(Segments,Nodes,Ways,option_keep||option_changes);

 /* Remove non-highway nodes (must be after removing the bad segments) */

 RemoveNonHighwayNodes(Nodes,Segments,option_keep||option_changes);

 /* Process the route relations and first part of turn relations (must be before compacting the ways) */

 ProcessRouteRelations(Relations,Ways,option_keep||option_changes);

 ProcessTurnRelations1(Relations,Nodes,Ways,option_keep||option_changes);

 /* Measure the segments and replace node/way id with index (must be after removing non-highway nodes) */

 MeasureSegments(Segments,Nodes,Ways);

 /* Index the segments */

 IndexSegments(Segments,Nodes,Ways);

 /* Convert the turn relations from ways into nodes */

 ProcessTurnRelations2(Relations,Nodes,Segments,Ways);

 /* Compact the ways (must be after turn relations 2) */

 CompactWayList(Ways,Segments);

 /* Index the segments */

 IndexSegments(Segments,Nodes,Ways);

 /* Prune unwanted nodes/segments. */

 if(option_prune_straight || option_prune_isolated || option_prune_short)
   {
    printf("\nPrune Unneeded Data\n===================\n\n");
    fflush(stdout);

    StartPruning(Nodes,Segments,Ways);

    if(option_prune_isolated)
       PruneIsolatedRegions(Nodes,Segments,Ways,option_prune_isolated);

    if(option_prune_short)
       PruneShortSegments(Nodes,Segments,Ways,option_prune_short);

    if(option_prune_straight)
       PruneStraightHighwayNodes(Nodes,Segments,Ways,option_prune_straight);

    FinishPruning(Nodes,Segments,Ways);

    /* Remove the pruned nodes and segments and update the indexes */

    RemovePrunedNodes(Nodes,Segments);
    RemovePrunedSegments(Segments,Ways);
    CompactWayList(Ways,Segments);
    RemovePrunedTurnRelations(Relations,Nodes);
    IndexSegments(Segments,Nodes,Ways);
   }

 /* Repeated iteration on Super-Nodes and Super-Segments */

 do
   {
    int nsuper;

    printf("\nProcess Super-Data (iteration %d)\n================================%s\n\n",iteration,iteration>9?"=":"");
    fflush(stdout);

    if(iteration==0)
      {
       /* Select the super-nodes */

       ChooseSuperNodes(Nodes,Segments,Ways);

       /* Select the super-segments */

       SuperSegments=CreateSuperSegments(Nodes,Segments,Ways);

       nsuper=Segments->number;
      }
    else
      {
       SegmentsX *SuperSegments2;

       /* Select the super-nodes */

       ChooseSuperNodes(Nodes,SuperSegments,Ways);

       /* Select the super-segments */

       SuperSegments2=CreateSuperSegments(Nodes,SuperSegments,Ways);

       nsuper=SuperSegments->number;

       FreeSegmentList(SuperSegments,0);

       SuperSegments=SuperSegments2;
      }

    /* Sort the super-segments and remove duplicates */

    DeduplicateSuperSegments(SuperSegments,Ways);

    /* Index the segments */

    IndexSegments(SuperSegments,Nodes,Ways);

    /* Check for end condition */

    if(SuperSegments->number==nsuper)
       quit=1;

    iteration++;

    if(iteration>max_iterations)
       quit=1;
   }
 while(!quit);

 /* Combine the super-segments */

 printf("\nCombine Segments and Super-Segments\n===================================\n\n");
 fflush(stdout);

 /* Merge the super-segments */

 MergedSegments=MergeSuperSegments(Segments,SuperSegments);

 FreeSegmentList(Segments,0);

 FreeSegmentList(SuperSegments,0);

 Segments=MergedSegments;

 /* Re-index the merged segments */

 IndexSegments(Segments,Nodes,Ways);

 /* Cross reference the nodes and segments */

 printf("\nCross-Reference Nodes and Segments\n==================================\n\n");
 fflush(stdout);

 /* Sort the nodes and segments geographically */

 SortNodeListGeographically(Nodes);

 SortSegmentListGeographically(Segments,Nodes);

 /* Re-index the segments */

 IndexSegments(Segments,Nodes,Ways);

 /* Sort the turn relations geographically */

 SortTurnRelationListGeographically(Relations,Nodes,Segments);

 /* Output the results */

 printf("\nWrite Out Database Files\n========================\n\n");
 fflush(stdout);

 /* Write out the nodes */

 SaveNodeList(Nodes,FileName(dirname,prefix,"nodes.mem"),Segments);

 FreeNodeList(Nodes,0);

 /* Write out the segments */

 SaveSegmentList(Segments,FileName(dirname,prefix,"segments.mem"));

 FreeSegmentList(Segments,0);

 /* Write out the ways */

 SaveWayList(Ways,FileName(dirname,prefix,"ways.mem"));

 FreeWayList(Ways,0);

 /* Write out the relations */

 SaveRelationList(Relations,FileName(dirname,prefix,"relations.mem"));

 FreeRelationList(Relations,0);

 /* Close the error log file */

 if(errorlog)
    close_errorlog();

 /* Print the total time */

 if(option_logtime)
   {
    printf("\n");
    fprintf_elapsed_time(stdout,&start_time);
    printf("Complete\n");
    fflush(stdout);
   }

 return(0);
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
         "Usage: planetsplitter [--help]\n"
         "                      [--dir=<dirname>] [--prefix=<name>]\n"
#if defined(USE_PTHREADS) && USE_PTHREADS
         "                      [--sort-ram-size=<size>] [--sort-threads=<number>]\n"
#else
         "                      [--sort-ram-size=<size>]\n"
#endif
         "                      [--tmpdir=<dirname>]\n"
         "                      [--tagging=<filename>]\n"
         "                      [--loggable] [--logtime]\n"
         "                      [--errorlog[=<name>]]\n"
         "                      [--parse-only | --process-only]\n"
         "                      [--append] [--keep] [--changes]\n"
         "                      [--max-iterations=<number>]\n"
         "                      [--prune-none]\n"
         "                      [--prune-isolated=<len>]\n"
         "                      [--prune-short=<len>]\n"
         "                      [--prune-straight=<len>]\n"
         "                      [<filename.osm> ... | <filename.osc> ...\n"
         "                       | <filename.pbf> ...\n"
         "                       | <filename.osm> ... | <filename.osc> ..."
#if defined(USE_BZIP2) && USE_BZIP2
         "\n                       | <filename.(osm|osc|o5m|o5c).bz2> ..."
#endif
#if defined(USE_GZIP) && USE_GZIP
         "\n                       | <filename.(osm|osc|o5m|o5c).gz> ..."
#endif
         "]\n");

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
            "--sort-ram-size=<size>    The amount of RAM (in MB) to use for data sorting\n"
#if SLIM
            "                          (defaults to 64MB otherwise.)\n"
#else
            "                          (defaults to 256MB otherwise.)\n"
#endif
#if defined(USE_PTHREADS) && USE_PTHREADS
            "--sort-threads=<number>   The number of threads to use for data sorting.\n"
#endif
            "\n"
            "--tmpdir=<dirname>        The directory name for temporary files.\n"
            "                          (defaults to the '--dir' option directory.)\n"
            "\n"
            "--tagging=<filename>      The name of the XML file containing the tagging rules\n"
            "                          (defaults to 'tagging.xml' with '--dir' and\n"
            "                           '--prefix' options or the file installed in\n"
            "                           '" DATADIR "').\n"
            "\n"
            "--loggable                Print progress messages suitable for logging to file.\n"
            "--logtime                 Print the elapsed time for each processing step.\n"
            "--errorlog[=<name>]       Log parsing errors to 'error.log' or the given name\n"
            "                          (the '--dir' and '--prefix' options are applied).\n"
            "\n"
            "--parse-only              Parse the OSM/OSC file(s) and store the results.\n"
            "--process-only            Process the stored results from previous option.\n"
            "--append                  Parse the OSM file(s) and append to existing results.\n"
            "--keep                    Keep the intermediate files after parsing & sorting.\n"
            "--changes                 Parse the data as an OSC file and apply the changes.\n"
            "\n"
            "--max-iterations=<number> The number of iterations for finding super-nodes\n"
            "                          (defaults to 5).\n"
            "\n"
            "--prune-none              Disable the prune options below, they are re-enabled\n"
            "                          by adding them to the command line after this option.\n"
            "--prune-isolated=<len>    Remove access from small disconnected segment groups\n"
            "                          (defaults to removing groups under 500m).\n"
            "--prune-short=<len>       Remove short segments (defaults to removing segments\n"
            "                          up to a maximum length of 5m).\n"
            "--prune-straight=<len>    Remove nodes in almost straight highways (defaults to\n"
            "                          removing nodes up to 3m offset from a straight line).\n"
            "\n"
            "<filename.osm>, <filename.osc>, <filename.pbf>, <filename.o5m>, <filename.o5c>\n"
            "                          The name(s) of the file(s) to read and parse.\n"
            "                          Filenames ending '.pbf' read as PBF, filenames ending\n"
            "                          '.o5m' or '.o5c' read as O5M/O5C, others as XML.\n"
#if defined(USE_BZIP2) && USE_BZIP2
            "                          Filenames ending '.bz2' will be bzip2 uncompressed.\n"
#endif
#if defined(USE_GZIP) && USE_GZIP
            "                          Filenames ending '.gz' will be gzip uncompressed.\n"
#endif
            "\n"
            "<transport> defaults to all but can be set to:\n"
            "%s"
            "\n"
            "<highway> can be selected from:\n"
            "%s"
            "\n"
            "<property> can be selected from:\n"
            "%s",
            TransportList(),HighwayList(),PropertyList());

 exit(!detail);
}
