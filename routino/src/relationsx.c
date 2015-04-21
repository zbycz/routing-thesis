/***************************************
 Extended Relation data type functions.

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


#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "segments.h"
#include "relations.h"

#include "nodesx.h"
#include "segmentsx.h"
#include "waysx.h"
#include "relationsx.h"

#include "files.h"
#include "logging.h"
#include "sorting.h"


/* Global variables */

/*+ The command line '--tmpdir' option or its default value. +*/
extern char *option_tmpdirname;

/* Local variables */

/*+ Temporary file-local variables for use by the sort functions. +*/
static SegmentsX *sortsegmentsx;
static NodesX *sortnodesx;

/* Local functions */

static int sort_route_by_id(RouteRelX *a,RouteRelX *b);
static int deduplicate_route_by_id(RouteRelX *relationx,index_t index);

static int sort_turn_by_id(TurnRelX *a,TurnRelX *b);
static int deduplicate_turn_by_id(TurnRelX *relationx,index_t index);

static int geographically_index(TurnRelX *relationx,index_t index);
static int sort_by_via(TurnRelX *a,TurnRelX *b);


/*++++++++++++++++++++++++++++++++++++++
  Allocate a new relation list (create a new file or open an existing one).

  RelationsX *NewRelationList Returns the relation list.

  int append Set to 1 if the file is to be opened for appending.

  int readonly Set to 1 if the file is to be opened for reading.
  ++++++++++++++++++++++++++++++++++++++*/

RelationsX *NewRelationList(int append,int readonly)
{
 RelationsX *relationsx;

 relationsx=(RelationsX*)calloc(1,sizeof(RelationsX));

 logassert(relationsx,"Failed to allocate memory (try using slim mode?)"); /* Check calloc() worked */


 /* Route Relations */

 relationsx->rfilename    =(char*)malloc(strlen(option_tmpdirname)+32);
 relationsx->rfilename_tmp=(char*)malloc(strlen(option_tmpdirname)+32);

 sprintf(relationsx->rfilename    ,"%s/relationsx.route.parsed.mem",option_tmpdirname);
 sprintf(relationsx->rfilename_tmp,"%s/relationsx.route.%p.tmp"    ,option_tmpdirname,(void*)relationsx);

 if(append || readonly)
    if(ExistsFile(relationsx->rfilename))
      {
       off_t size,position=0;
       int rfd;

       size=SizeFile(relationsx->rfilename);

       rfd=ReOpenFile(relationsx->rfilename);

       while(position<size)
         {
          FILESORT_VARINT relationsize;

          SeekReadFile(rfd,&relationsize,FILESORT_VARSIZE,position);

          relationsx->rnumber++;
          position+=relationsize+FILESORT_VARSIZE;
         }

       CloseFile(rfd);

       RenameFile(relationsx->rfilename ,relationsx->rfilename_tmp);
      }

 if(append)
    relationsx->rfd=OpenFileAppend(relationsx->rfilename_tmp);
 else if(!readonly)
    relationsx->rfd=OpenFileNew(relationsx->rfilename_tmp);
 else
    relationsx->rfd=-1;


 /* Turn Restriction Relations */

 relationsx->trfilename    =(char*)malloc(strlen(option_tmpdirname)+32);
 relationsx->trfilename_tmp=(char*)malloc(strlen(option_tmpdirname)+32);

 sprintf(relationsx->trfilename    ,"%s/relationsx.turn.parsed.mem",option_tmpdirname);
 sprintf(relationsx->trfilename_tmp,"%s/relationsx.turn.%p.tmp"    ,option_tmpdirname,(void*)relationsx);

 if(append || readonly)
    if(ExistsFile(relationsx->trfilename))
      {
       off_t size;

       size=SizeFile(relationsx->trfilename);

       relationsx->trnumber=size/sizeof(TurnRelX);

       RenameFile(relationsx->trfilename,relationsx->trfilename_tmp);
      }

 if(append)
    relationsx->trfd=OpenFileAppend(relationsx->trfilename_tmp);
 else if(!readonly)
    relationsx->trfd=OpenFileNew(relationsx->trfilename_tmp);
 else
    relationsx->trfd=-1;

 return(relationsx);
}


/*++++++++++++++++++++++++++++++++++++++
  Free a relation list.

  RelationsX *relationsx The set of relations to be freed.

  int keep If set then the results file is to be kept.
  ++++++++++++++++++++++++++++++++++++++*/

void FreeRelationList(RelationsX *relationsx,int keep)
{
 /* Route relations */

 if(keep)
    RenameFile(relationsx->rfilename_tmp,relationsx->rfilename);
 else
    DeleteFile(relationsx->rfilename_tmp);

 free(relationsx->rfilename);
 free(relationsx->rfilename_tmp);


 /* Turn Restriction relations */

 if(keep)
    RenameFile(relationsx->trfilename_tmp,relationsx->trfilename);
 else
    DeleteFile(relationsx->trfilename_tmp);

 free(relationsx->trfilename);
 free(relationsx->trfilename_tmp);

 free(relationsx);
}


/*++++++++++++++++++++++++++++++++++++++
  Append a single relation to an unsorted route relation list.

  RelationsX* relationsx The set of relations to process.

  relation_t id The ID of the relation.

  transports_t routes The types of routes that this relation is for.

  way_t *ways The array of ways that are members of the relation.

  int nways The number of ways that are members of the relation.

  relation_t *relations The array of relations that are members of the relation.

  int nrelations The number of relations that are members of the relation.
  ++++++++++++++++++++++++++++++++++++++*/

void AppendRouteRelationList(RelationsX* relationsx,relation_t id,
                             transports_t routes,
                             way_t *ways,int nways,
                             relation_t *relations,int nrelations)
{
 RouteRelX relationx={0};
 FILESORT_VARINT size;
 way_t noway=NO_WAY_ID;
 relation_t norelation=NO_RELATION_ID;

 relationx.id=id;
 relationx.routes=routes;

 size=sizeof(RouteRelX)+(nways+1)*sizeof(way_t)+(nrelations+1)*sizeof(relation_t);

 WriteFile(relationsx->rfd,&size,FILESORT_VARSIZE);
 WriteFile(relationsx->rfd,&relationx,sizeof(RouteRelX));

 WriteFile(relationsx->rfd,ways  ,nways*sizeof(way_t));
 WriteFile(relationsx->rfd,&noway,      sizeof(way_t));

 WriteFile(relationsx->rfd,relations  ,nrelations*sizeof(relation_t));
 WriteFile(relationsx->rfd,&norelation,           sizeof(relation_t));

 relationsx->rnumber++;

 logassert(relationsx->rnumber!=0,"Too many route relations (change index_t to 64-bits?)"); /* Zero marks the high-water mark for relations. */
}


/*++++++++++++++++++++++++++++++++++++++
  Append a single relation to an unsorted turn restriction relation list.

  RelationsX* relationsx The set of relations to process.

  relation_t id The ID of the relation.

  way_t from The way that the turn restriction starts from.

  way_t to The way that the restriction finished on.

  node_t via The node that the turn restriction passes through.

  TurnRestriction restriction The type of restriction.

  transports_t except The set of transports allowed to bypass the restriction.
  ++++++++++++++++++++++++++++++++++++++*/

void AppendTurnRelationList(RelationsX* relationsx,relation_t id,
                            way_t from,way_t to,node_t via,
                            TurnRestriction restriction,transports_t except)
{
 TurnRelX relationx={0};

 relationx.id=id;
 relationx.from=from;
 relationx.to=to;
 relationx.via=via;
 relationx.restriction=restriction;
 relationx.except=except;

 WriteFile(relationsx->trfd,&relationx,sizeof(TurnRelX));

 relationsx->trnumber++;

 logassert(relationsx->trnumber!=0,"Too many turn relations (change index_t to 64-bits?)"); /* Zero marks the high-water mark for relations. */
}


/*++++++++++++++++++++++++++++++++++++++
  Finish appending relations and change the filename over.

  RelationsX *relationsx The relations that have been appended.
  ++++++++++++++++++++++++++++++++++++++*/

void FinishRelationList(RelationsX *relationsx)
{
 if(relationsx->rfd!=-1)
    relationsx->rfd =CloseFile(relationsx->rfd);

 if(relationsx->trfd!=-1)
    relationsx->trfd=CloseFile(relationsx->trfd);
}


/*++++++++++++++++++++++++++++++++++++++
  Sort the list of relations.

  RelationsX* relationsx The set of relations to process.
  ++++++++++++++++++++++++++++++++++++++*/

void SortRelationList(RelationsX* relationsx)
{
 /* Route Relations */

 if(relationsx->rnumber)
   {
    index_t rxnumber;
    int rfd;

    /* Print the start message */

    printf_first("Sorting Route Relations");

    /* Re-open the file read-only and a new file writeable */

    relationsx->rfd=ReOpenFile(relationsx->rfilename_tmp);

    DeleteFile(relationsx->rfilename_tmp);

    rfd=OpenFileNew(relationsx->rfilename_tmp);

    /* Sort the relations */

    rxnumber=relationsx->rnumber;

    relationsx->rnumber=filesort_vary(relationsx->rfd,rfd,NULL,
                                                          (int (*)(const void*,const void*))sort_route_by_id,
                                                          (int (*)(void*,index_t))deduplicate_route_by_id);

    /* Close the files */

    relationsx->rfd=CloseFile(relationsx->rfd);
    CloseFile(rfd);

    /* Print the final message */

    printf_last("Sorted Route Relations: Relations=%"Pindex_t" Duplicates=%"Pindex_t,rxnumber,rxnumber-relationsx->rnumber);
   }

 /* Turn Restriction Relations. */

 if(relationsx->trnumber)
   {
    index_t trxnumber;
    int trfd;

    /* Print the start message */

    printf_first("Sorting Turn Relations");

    /* Re-open the file read-only and a new file writeable */

    relationsx->trfd=ReOpenFile(relationsx->trfilename_tmp);

    DeleteFile(relationsx->trfilename_tmp);

    trfd=OpenFileNew(relationsx->trfilename_tmp);

    /* Sort the relations */

    trxnumber=relationsx->trnumber;

    relationsx->trnumber=filesort_fixed(relationsx->trfd,trfd,sizeof(TurnRelX),NULL,
                                                                               (int (*)(const void*,const void*))sort_turn_by_id,
                                                                               (int (*)(void*,index_t))deduplicate_turn_by_id);

    /* Close the files */

    relationsx->trfd=CloseFile(relationsx->trfd);
    CloseFile(trfd);

    /* Print the final message */

    printf_last("Sorted Turn Relations: Relations=%"Pindex_t" Duplicates=%"Pindex_t,trxnumber,trxnumber-relationsx->trnumber);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Sort the route relations into id order.

  int sort_route_by_id Returns the comparison of the id fields.

  RouteRelX *a The first extended relation.

  RouteRelX *b The second extended relation.
  ++++++++++++++++++++++++++++++++++++++*/

static int sort_route_by_id(RouteRelX *a,RouteRelX *b)
{
 relation_t a_id=a->id;
 relation_t b_id=b->id;

 if(a_id<b_id)
    return(-1);
 else if(a_id>b_id)
    return(1);
 else
    return(-FILESORT_PRESERVE_ORDER(a,b)); /* latest version first */
}


/*++++++++++++++++++++++++++++++++++++++
  Deduplicate the route relations using the id after sorting.

  int deduplicate_route_by_id Return 1 if the value is to be kept, otherwise 0.

  RouteRelX *relationx The extended relation.

  index_t index The number of sorted relations that have already been written to the output file.
  ++++++++++++++++++++++++++++++++++++++*/

static int deduplicate_route_by_id(RouteRelX *relationx,index_t index)
{
 static relation_t previd=NO_RELATION_ID;

 if(relationx->id!=previd)
   {
    previd=relationx->id;

    if(relationx->routes==RELATION_DELETED)
       return(0);
    else
       return(1);
   }
 else
    return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Sort the turn restriction relations into id order.

  int sort_turn_by_id Returns the comparison of the id fields.

  TurnRelX *a The first extended relation.

  TurnRelX *b The second extended relation.
  ++++++++++++++++++++++++++++++++++++++*/

static int sort_turn_by_id(TurnRelX *a,TurnRelX *b)
{
 relation_t a_id=a->id;
 relation_t b_id=b->id;

 if(a_id<b_id)
    return(-1);
 else if(a_id>b_id)
    return(1);
 else
    return(-FILESORT_PRESERVE_ORDER(a,b)); /* latest version first */
}


/*++++++++++++++++++++++++++++++++++++++
  Deduplicate the turn restriction relations using the id after sorting.

  int deduplicate_turn_by_id Return 1 if the value is to be kept, otherwise 0.

  TurnRelX *relationx The extended relation.

  index_t index The number of sorted relations that have already been written to the output file.
  ++++++++++++++++++++++++++++++++++++++*/

static int deduplicate_turn_by_id(TurnRelX *relationx,index_t index)
{
 static relation_t previd=NO_RELATION_ID;

 if(relationx->id!=previd)
   {
    previd=relationx->id;

    if(relationx->except==RELATION_DELETED)
       return(0);
    else
       return(1);
   }
 else
    return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Process the route relations and apply the information to the ways.

  RelationsX *relationsx The set of relations to use.

  WaysX *waysx The set of ways to modify.

  int keep If set to 1 then keep the old data file otherwise delete it.
  ++++++++++++++++++++++++++++++++++++++*/

void ProcessRouteRelations(RelationsX *relationsx,WaysX *waysx,int keep)
{
 RouteRelX *unmatched=NULL,*lastunmatched=NULL;
 int nunmatched=0,lastnunmatched=0,iteration=1;

 if(waysx->number==0)
    return;

 /* Map into memory / open the files */

#if !SLIM
 waysx->data=MapFileWriteable(waysx->filename_tmp);
#else
 waysx->fd=ReOpenFileWriteable(waysx->filename_tmp);
#endif

 /* Re-open the file read-only */

 relationsx->rfd=ReOpenFile(relationsx->rfilename_tmp);

 /* Read through the file. */

 do
   {
    int ways=0,relations=0;
    index_t i;

    SeekFile(relationsx->rfd,0);

    /* Print the start message */

    printf_first("Processing Route Relations (%d): Relations=0 Modified Ways=0",iteration);

    for(i=0;i<relationsx->rnumber;i++)
      {
       FILESORT_VARINT size;
       RouteRelX relationx;
       way_t wayid;
       relation_t relationid;
       transports_t routes=Transports_None;

       /* Read each route relation */

       ReadFile(relationsx->rfd,&size,FILESORT_VARSIZE);
       ReadFile(relationsx->rfd,&relationx,sizeof(RouteRelX));

       /* Decide what type of route it is */

       if(iteration==1)
         {
          relations++;
          routes=relationx.routes;
         }
       else
         {
          int j;

          for(j=0;j<lastnunmatched;j++)
             if(lastunmatched[j].id==relationx.id)
               {
                relations++;

                if((lastunmatched[j].routes|relationx.routes)==relationx.routes)
                   routes=0; /* Nothing new to add */
                else
                   routes=lastunmatched[j].routes;

                break;
               }
         }

       /* Loop through the ways */

       do
         {
          ReadFile(relationsx->rfd,&wayid,sizeof(way_t));

          /* Update the ways that are listed for the relation */

          if(wayid==NO_WAY_ID)
             continue;

          if(routes)
            {
             index_t way=IndexWayX(waysx,wayid);

             if(way!=NO_WAY)
               {
                WayX *wayx=LookupWayX(waysx,way,1);

                if(routes&Transports_Foot)
                  {
                   if(!(wayx->way.allow&Transports_Foot))
                     {
                      logerror("Route Relation %"Prelation_t" for Foot contains Way %"Pway_t" that does not allow Foot transport; overriding.\n",relationx.id,wayid);
                      wayx->way.allow|=Transports_Foot;
                     }
                   wayx->way.props|=Properties_FootRoute;
                  }

                if(routes&Transports_Bicycle)
                  {
                   if(!(wayx->way.allow&Transports_Bicycle))
                     {
                      logerror("Route Relation %"Prelation_t" for Bicycle contains Way %"Pway_t" that does not allow Bicycle transport; overriding.\n",relationx.id,wayid);
                      wayx->way.allow|=Transports_Bicycle;
                     }
                   wayx->way.props|=Properties_BicycleRoute;
                  }

                PutBackWayX(waysx,wayx);

                ways++;
               }
             else
                logerror("Route Relation %"Prelation_t" contains Way %"Pway_t" that does not exist in the Routino database (not a highway?).\n",relationx.id,wayid);
            }
         }
       while(wayid!=NO_WAY_ID);

       /* Loop through the relations */

       do
         {
          ReadFile(relationsx->rfd,&relationid,sizeof(relation_t));

          /* Add the relations that are listed for this relation to the list for next time */

          if(relationid==NO_RELATION_ID)
             continue;

          if(relationid==relationx.id)
             logerror("Relation %"Prelation_t" contains itself.\n",relationx.id);
          else if(routes)
            {
             if(nunmatched%256==0)
                unmatched=(RouteRelX*)realloc((void*)unmatched,(nunmatched+256)*sizeof(RouteRelX));

             unmatched[nunmatched].id=relationid;
             unmatched[nunmatched].routes=routes;

             nunmatched++;
            }
         }
       while(relationid!=NO_RELATION_ID);

       if(!((i+1)%1000))
          printf_middle("Processing Route Relations (%d): Relations=%"Pindex_t" Modified Ways=%"Pindex_t,iteration,relations,ways);
      }

    if(lastunmatched)
       free(lastunmatched);

    lastunmatched=unmatched;
    lastnunmatched=nunmatched;

    unmatched=NULL;
    nunmatched=0;

    /* Print the final message */

    printf_last("Processed Route Relations (%d): Relations=%"Pindex_t" Modified Ways=%"Pindex_t,iteration,relations,ways);
   }
 while(lastnunmatched && iteration++<8);

 if(lastunmatched)
    free(lastunmatched);

 /* Close the file */

 relationsx->rfd=CloseFile(relationsx->rfd);

 if(keep)
    RenameFile(relationsx->rfilename_tmp,relationsx->rfilename);

 /* Unmap from memory / close the files */

#if !SLIM
 waysx->data=UnmapFile(waysx->data);
#else
 waysx->fd=CloseFile(waysx->fd);
#endif
}


/*++++++++++++++++++++++++++++++++++++++
  Process the turn relations (first part) to update them with the node/way information.

  RelationsX *relationsx The set of relations to modify.

  NodesX *nodesx The set of nodes to use.

  WaysX *waysx The set of ways to use.

  int keep If set to 1 then keep the old data file otherwise delete it.
  ++++++++++++++++++++++++++++++++++++++*/

void ProcessTurnRelations1(RelationsX *relationsx,NodesX *nodesx,WaysX *waysx,int keep)
{
 int trfd;
 index_t i,deleted=0;

 /* Print the start message */

 printf_first("Processing Turn Relations (1): Relations=0");

 /* Re-open the file read-only and a new file writeable */

 relationsx->trfd=ReOpenFile(relationsx->trfilename_tmp);

 if(keep)
    RenameFile(relationsx->trfilename_tmp,relationsx->trfilename);
 else
    DeleteFile(relationsx->trfilename_tmp);

 trfd=OpenFileNew(relationsx->trfilename_tmp);

 /* Process all of the relations */

 for(i=0;i<relationsx->trnumber;i++)
   {
    TurnRelX relationx;
    index_t via,from,to;

    ReadFile(relationsx->trfd,&relationx,sizeof(TurnRelX));

    via =IndexNodeX(nodesx,relationx.via);
    from=IndexWayX(waysx,relationx.from);
    to  =IndexWayX(waysx,relationx.to);

    if(via==NO_NODE)
       logerror("Turn Relation %"Prelation_t" contains Node %"Pnode_t" that does not exist in the Routino database (not a highway node?).\n",relationx.id,relationx.via);

    if(from==NO_WAY)
       logerror("Turn Relation %"Prelation_t" contains Way %"Pway_t" that does not exist in the Routino database (not a highway?).\n",relationx.id,relationx.from);

    if(to==NO_WAY)
       logerror("Turn Relation %"Prelation_t" contains Way %"Pway_t" that does not exist in the Routino database (not a highway?).\n",relationx.id,relationx.to);

    relationx.via =via;
    relationx.from=from;
    relationx.to  =to;

    if(relationx.via==NO_NODE || relationx.from==NO_WAY || relationx.to==NO_WAY)
       deleted++;
    else
       WriteFile(trfd,&relationx,sizeof(TurnRelX));

    if(!((i+1)%1000))
       printf_middle("Processing Turn Relations (1): Relations=%"Pindex_t" Deleted=%"Pindex_t,i+1,deleted);
   }

 /* Close the files */

 relationsx->trfd=CloseFile(relationsx->trfd);
 CloseFile(trfd);

 /* Print the final message */

 printf_last("Processed Turn Relations (1): Relations=%"Pindex_t" Deleted=%"Pindex_t,relationsx->trnumber,deleted);

 relationsx->trnumber-=deleted;
}


/*++++++++++++++++++++++++++++++++++++++
  Process the turn relations (second part) to convert them to nodes.

  RelationsX *relationsx The set of relations to modify.

  NodesX *nodesx The set of nodes to use.

  SegmentsX *segmentsx The set of segments to use.

  WaysX *waysx The set of ways to use.
  ++++++++++++++++++++++++++++++++++++++*/

void ProcessTurnRelations2(RelationsX *relationsx,NodesX *nodesx,SegmentsX *segmentsx,WaysX *waysx)
{
 TurnRelX relationx;
 int trfd;
 index_t total=0,deleted=0;

 if(nodesx->number==0 || segmentsx->number==0)
    return;

 /* Print the start message */

 printf_first("Processing Turn Relations (2): Relations=0");

 /* Map into memory / open the files */

#if !SLIM
 nodesx->data=MapFileWriteable(nodesx->filename_tmp);
 segmentsx->data=MapFile(segmentsx->filename_tmp);
 waysx->data=MapFile(waysx->filename_tmp);
#else
 nodesx->fd=ReOpenFileWriteable(nodesx->filename_tmp);
 segmentsx->fd=ReOpenFile(segmentsx->filename_tmp);
 waysx->fd=ReOpenFile(waysx->filename_tmp);
#endif

 /* Re-open the file read-only and a new file writeable */

 relationsx->trfd=ReOpenFile(relationsx->trfilename_tmp);

 DeleteFile(relationsx->trfilename_tmp);

 trfd=OpenFileNew(relationsx->trfilename_tmp);

 /* Process all of the relations */

 while(!ReadFile(relationsx->trfd,&relationx,sizeof(TurnRelX)))
   {
    NodeX *nodex;
    SegmentX *segmentx;

    if(relationx.restriction==TurnRestrict_no_right_turn ||
       relationx.restriction==TurnRestrict_no_left_turn ||
       relationx.restriction==TurnRestrict_no_u_turn ||
       relationx.restriction==TurnRestrict_no_straight_on)
      {
       index_t node_from=NO_NODE,node_to=NO_NODE;
       int oneway_from=0,oneway_to=0,vehicles_from=1,vehicles_to=1;

       /* Find the segments that join the node 'via' */

       segmentx=FirstSegmentX(segmentsx,relationx.via,1);

       while(segmentx)
         {
          if(segmentx->way==relationx.from)
            {
             WayX *wayx=LookupWayX(waysx,segmentx->way,1);

             if(node_from!=NO_NODE) /* Only one segment can be on the 'from' way */
               {
                logerror("Turn Relation %"Prelation_t" is not stored because the 'via' node is not at the end of the 'from' way.\n",relationx.id);
                deleted++;
                goto endloop;
               }

             node_from=OtherNode(segmentx,relationx.via);

             if(IsOnewayFrom(segmentx,relationx.via))
                oneway_from=1;  /* not allowed */

             if(!(wayx->way.allow&(Transports_Bicycle|Transports_Moped|Transports_Motorcycle|Transports_Motorcar|Transports_Goods|Transports_HGV|Transports_PSV)))
                vehicles_from=0;  /* not allowed */
            }

          if(segmentx->way==relationx.to)
            {
             WayX *wayx=LookupWayX(waysx,segmentx->way,1);

             if(node_to!=NO_NODE) /* Only one segment can be on the 'to' way */
               {
                logerror("Turn Relation %"Prelation_t" is not stored because the 'via' node is not at the end of the 'to' way.\n",relationx.id);
                deleted++;
                goto endloop;
               }

             node_to=OtherNode(segmentx,relationx.via);

             if(IsOnewayTo(segmentx,relationx.via))
                oneway_to=1;  /* not allowed */

             if(!(wayx->way.allow&(Transports_Bicycle|Transports_Moped|Transports_Motorcycle|Transports_Motorcar|Transports_Goods|Transports_HGV|Transports_PSV)))
                vehicles_to=0;  /* not allowed */
            }

          segmentx=NextSegmentX(segmentsx,segmentx,relationx.via);
         }

       if(node_from==NO_NODE)
          logerror("Turn Relation %"Prelation_t" is not stored because the 'via' node is not part of the 'from' way.\n",relationx.id);

       if(node_to==NO_NODE)
          logerror("Turn Relation %"Prelation_t" is not stored because the 'via' node is not part of the 'to' way.\n",relationx.id);

       if(oneway_from)
          logerror("Turn Relation %"Prelation_t" is not needed because the 'from' way is oneway away from the 'via' node.\n",relationx.id);

       if(oneway_to)
          logerror("Turn Relation %"Prelation_t" is not needed because the 'to' way is oneway towards the 'via' node.\n",relationx.id);

       if(!vehicles_from)
          logerror("Turn Relation %"Prelation_t" is not needed because the 'from' way does not allow vehicles.\n",relationx.id);

       if(!vehicles_to)
          logerror("Turn Relation %"Prelation_t" is not needed because the 'to' way does not allow vehicles.\n",relationx.id);

       if(oneway_from || oneway_to || !vehicles_from || !vehicles_to || node_from==NO_NODE || node_to==NO_NODE)
         {
          deleted++;
          goto endloop;
         }

       /* Write the results */

       relationx.from=node_from;
       relationx.to  =node_to;

       WriteFile(trfd,&relationx,sizeof(TurnRelX));

       total++;

       if(!(total%1000))
          printf_middle("Processing Turn Relations (2): Relations=%"Pindex_t" Deleted=%"Pindex_t" Added=%"Pindex_t,total,deleted,total-relationsx->trnumber+deleted);
      }
    else
      {
       index_t node_from=NO_NODE,node_to=NO_NODE,node_other[MAX_SEG_PER_NODE];
       int nnodes_other=0,i;
       int oneway_from=0,vehicles_from=1;

       /* Find the segments that join the node 'via' */

       segmentx=FirstSegmentX(segmentsx,relationx.via,1);

       while(segmentx)
         {
          if(segmentx->way==relationx.from)
            {
             WayX *wayx=LookupWayX(waysx,segmentx->way,1);

             if(node_from!=NO_NODE) /* Only one segment can be on the 'from' way */
               {
                logerror("Turn Relation %"Prelation_t" is not stored because the 'via' node is not at the end of the 'from' way.\n",relationx.id);
                deleted++;
                goto endloop;
               }

             node_from=OtherNode(segmentx,relationx.via);

             if(IsOnewayFrom(segmentx,relationx.via))
                oneway_from=1;  /* not allowed */

             if(!(wayx->way.allow&(Transports_Bicycle|Transports_Moped|Transports_Motorcycle|Transports_Motorcar|Transports_Goods|Transports_HGV|Transports_PSV)))
                vehicles_from=0;  /* not allowed */
            }

          if(segmentx->way==relationx.to)
            {
             if(node_to!=NO_NODE) /* Only one segment can be on the 'to' way */
               {
                logerror("Turn Relation %"Prelation_t" is not stored because the 'via' node is not at the end of the 'to' way.\n",relationx.id);
                deleted++;
                goto endloop;
               }

             node_to=OtherNode(segmentx,relationx.via);
            }

          if(segmentx->way!=relationx.from && segmentx->way!=relationx.to)
            {
             WayX *wayx=LookupWayX(waysx,segmentx->way,1);

             if(IsOnewayTo(segmentx,relationx.via))
                ;  /* not allowed */
             else if(!(wayx->way.allow&(Transports_Bicycle|Transports_Moped|Transports_Motorcycle|Transports_Motorcar|Transports_Goods|Transports_HGV|Transports_PSV)))
                ;  /* not allowed */
             else
               {
                logassert(nnodes_other<MAX_SEG_PER_NODE,"Too many segments for one node (increase MAX_SEG_PER_NODE?)"); /* Only a limited amount of information stored. */

                node_other[nnodes_other++]=OtherNode(segmentx,relationx.via);
               }
            }

          segmentx=NextSegmentX(segmentsx,segmentx,relationx.via);
         }

       if(node_from==NO_NODE)
          logerror("Turn Relation %"Prelation_t" is not stored because the 'via' node is not part of the 'from' way.\n",relationx.id);

       if(node_to==NO_NODE)
          logerror("Turn Relation %"Prelation_t" is not stored because the 'via' node is not part of the 'to' way.\n",relationx.id);

       if(nnodes_other==0)
          logerror("Turn Relation %"Prelation_t" is not needed because the only allowed exit from the 'via' node is the 'to' way.\n",relationx.id);

       if(oneway_from)
          logerror("Turn Relation %"Prelation_t" is not needed because the 'from' way is oneway away from the 'via' node.\n",relationx.id);

       if(!vehicles_from)
          logerror("Turn Relation %"Prelation_t" is not needed because the 'from' way does not allow vehicles.\n",relationx.id);

       if(oneway_from || !vehicles_from || node_from==NO_NODE || node_to==NO_NODE || nnodes_other==0)
         {
          deleted++;
          goto endloop;
         }

       /* Write the results */

       for(i=0;i<nnodes_other;i++)
         {
          relationx.from=node_from;
          relationx.to  =node_other[i];

          WriteFile(trfd,&relationx,sizeof(TurnRelX));

          total++;

          if(!(total%1000))
             printf_middle("Processing Turn Relations (2): Relations=%"Pindex_t" Deleted=%"Pindex_t" Added=%"Pindex_t,total,deleted,total-relationsx->trnumber+deleted);
         }
      }

    /* Force super nodes on via node and adjacent nodes */

    nodex=LookupNodeX(nodesx,relationx.via,1);
    nodex->flags|=NODE_TURNRSTRCT;
    PutBackNodeX(nodesx,nodex);

    segmentx=FirstSegmentX(segmentsx,relationx.via,1);

    while(segmentx)
      {
       index_t othernode=OtherNode(segmentx,relationx.via);

       nodex=LookupNodeX(nodesx,othernode,1);
       nodex->flags|=NODE_TURNRSTRCT2;
       PutBackNodeX(nodesx,nodex);

       segmentx=NextSegmentX(segmentsx,segmentx,relationx.via);
      }

   endloop: ;
   }

 /* Close the files */

 relationsx->trfd=CloseFile(relationsx->trfd);
 CloseFile(trfd);

 /* Unmap from memory / close the files */

#if !SLIM
 nodesx->data=UnmapFile(nodesx->data);
 segmentsx->data=UnmapFile(segmentsx->data);
 waysx->data=UnmapFile(waysx->data);
#else
 nodesx->fd=CloseFile(nodesx->fd);
 segmentsx->fd=CloseFile(segmentsx->fd);
 waysx->fd=CloseFile(waysx->fd);
#endif

 /* Print the final message */

 printf_last("Processed Turn Relations (2): Relations=%"Pindex_t" Deleted=%"Pindex_t" Added=%"Pindex_t,total,deleted,total-relationsx->trnumber+deleted);

 relationsx->trnumber=total;
}


/*++++++++++++++++++++++++++++++++++++++
  Remove pruned turn relations and update the node indexes after pruning nodes.

  RelationsX *relationsx The set of relations to modify.

  NodesX *nodesx The set of nodes to use.
  ++++++++++++++++++++++++++++++++++++++*/

void RemovePrunedTurnRelations(RelationsX *relationsx,NodesX *nodesx)
{
 TurnRelX relationx;
 index_t total=0,pruned=0,notpruned=0;
 int trfd;

 if(relationsx->trnumber==0)
    return;

 /* Print the start message */

 printf_first("Deleting Pruned Turn Relations: Relations=0 Pruned=0");

 /* Re-open the file read-only and a new file writeable */

 relationsx->trfd=ReOpenFile(relationsx->trfilename_tmp);

 DeleteFile(relationsx->trfilename_tmp);

 trfd=OpenFileNew(relationsx->trfilename_tmp);

 /* Process all of the relations */

 while(!ReadFile(relationsx->trfd,&relationx,sizeof(TurnRelX)))
   {
    relationx.from=nodesx->pdata[relationx.from];
    relationx.via =nodesx->pdata[relationx.via];
    relationx.to  =nodesx->pdata[relationx.to];

    if(relationx.from==NO_NODE || relationx.via==NO_NODE || relationx.to==NO_NODE)
       pruned++;
    else
      {
       WriteFile(trfd,&relationx,sizeof(TurnRelX));

       notpruned++;
      }

    total++;

    if(!(total%1000))
       printf_middle("Deleting Pruned Turn Relations: Relations=%"Pindex_t" Pruned=%"Pindex_t,total,pruned);
   }

 relationsx->trnumber=notpruned;

 /* Close the files */

 relationsx->trfd=CloseFile(relationsx->trfd);
 CloseFile(trfd);

 /* Print the final message */

 printf_last("Deleted Pruned Turn Relations: Relations=%"Pindex_t" Pruned=%"Pindex_t,total,pruned);
}


/*++++++++++++++++++++++++++++++++++++++
  Sort the turn relations geographically after updating the node indexes.

  RelationsX *relationsx The set of relations to modify.

  NodesX *nodesx The set of nodes to use.

  SegmentsX *segmentsx The set of segments to use.
  ++++++++++++++++++++++++++++++++++++++*/

void SortTurnRelationListGeographically(RelationsX *relationsx,NodesX *nodesx,SegmentsX *segmentsx)
{
 int trfd;

 if(segmentsx->number==0)
    return;

 /* Print the start message */

 printf_first("Sorting Turn Relations Geographically");

 /* Map into memory / open the files */

#if !SLIM
 segmentsx->data=MapFile(segmentsx->filename_tmp);
#else
 segmentsx->fd=ReOpenFile(segmentsx->filename_tmp);
#endif

 /* Re-open the file read-only and a new file writeable */

 relationsx->trfd=ReOpenFile(relationsx->trfilename_tmp);

 DeleteFile(relationsx->trfilename_tmp);

 trfd=OpenFileNew(relationsx->trfilename_tmp);

 /* Update the segments with geographically sorted node indexes and sort them */

 sortnodesx=nodesx;
 sortsegmentsx=segmentsx;

 filesort_fixed(relationsx->trfd,trfd,sizeof(TurnRelX),(int (*)(void*,index_t))geographically_index,
                                                       (int (*)(const void*,const void*))sort_by_via,
                                                       NULL);

 /* Close the files */

 relationsx->trfd=CloseFile(relationsx->trfd);
 CloseFile(trfd);

 /* Unmap from memory / close the files */

#if !SLIM
 segmentsx->data=UnmapFile(segmentsx->data);
#else
 segmentsx->fd=CloseFile(segmentsx->fd);
#endif

 /* Print the final message */

 printf_last("Sorted Turn Relations Geographically: Turn Relations=%"Pindex_t,relationsx->trnumber);
}


/*++++++++++++++++++++++++++++++++++++++
  Update the turn relation indexes.

  int geographically_index Return 1 if the value is to be kept, otherwise 0.

  TurnRelX *relationx The extended turn relation.

  index_t index The number of unsorted turn relations that have been read from the input file.
  ++++++++++++++++++++++++++++++++++++++*/

static int geographically_index(TurnRelX *relationx,index_t index)
{
 SegmentX *segmentx;
 index_t from_node,via_node,to_node;

 from_node=sortnodesx->gdata[relationx->from];
 via_node =sortnodesx->gdata[relationx->via];
 to_node  =sortnodesx->gdata[relationx->to];

 segmentx=FirstSegmentX(sortsegmentsx,via_node,1);

 do
   {
    if(OtherNode(segmentx,via_node)==from_node)
       relationx->from=IndexSegmentX(sortsegmentsx,segmentx);

    if(OtherNode(segmentx,via_node)==to_node)
       relationx->to=IndexSegmentX(sortsegmentsx,segmentx);

    segmentx=NextSegmentX(sortsegmentsx,segmentx,via_node);
   }
 while(segmentx);

 relationx->via=via_node;

 return(1);
}


/*++++++++++++++++++++++++++++++++++++++
  Sort the turn restriction relations into via index order (then by from and to segments).

  int sort_by_via Returns the comparison of the via, from and to fields.

  TurnRelX *a The first extended relation.

  TurnRelX *b The second extended relation.
  ++++++++++++++++++++++++++++++++++++++*/

static int sort_by_via(TurnRelX *a,TurnRelX *b)
{
 index_t a_id=a->via;
 index_t b_id=b->via;

 if(a_id<b_id)
    return(-1);
 else if(a_id>b_id)
    return(1);
 else
   {
    index_t a_id=a->from;
    index_t b_id=b->from;

    if(a_id<b_id)
       return(-1);
    else if(a_id>b_id)
       return(1);
    else
      {
       index_t a_id=a->to;
       index_t b_id=b->to;

       if(a_id<b_id)
          return(-1);
       else if(a_id>b_id)
          return(1);
       else
          return(FILESORT_PRESERVE_ORDER(a,b));
      }
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Save the relation list to a file.

  RelationsX* relationsx The set of relations to save.

  const char *filename The name of the file to save.
  ++++++++++++++++++++++++++++++++++++++*/

void SaveRelationList(RelationsX* relationsx,const char *filename)
{
 index_t i;
 int fd;
 RelationsFile relationsfile={0};

 /* Print the start message */

 printf_first("Writing Relations: Turn Relations=0");

 /* Re-open the file read-only */

 relationsx->trfd=ReOpenFile(relationsx->trfilename_tmp);

 /* Write out the relations data */

 fd=OpenFileNew(filename);

 SeekFile(fd,sizeof(RelationsFile));

 for(i=0;i<relationsx->trnumber;i++)
   {
    TurnRelX relationx;
    TurnRelation relation={0};

    ReadFile(relationsx->trfd,&relationx,sizeof(TurnRelX));

    relation.from=relationx.from;
    relation.via=relationx.via;
    relation.to=relationx.to;
    relation.except=relationx.except;

    WriteFile(fd,&relation,sizeof(TurnRelation));

    if(!((i+1)%1000))
       printf_middle("Writing Relations: Turn Relations=%"Pindex_t,i+1);
   }

 /* Write out the header structure */

 relationsfile.trnumber=relationsx->trnumber;

 SeekFile(fd,0);
 WriteFile(fd,&relationsfile,sizeof(RelationsFile));

 CloseFile(fd);

 /* Close the file */

 relationsx->trfd=CloseFile(relationsx->trfd);

 /* Print the final message */

 printf_last("Wrote Relations: Turn Relations=%"Pindex_t,relationsx->trnumber);
}
