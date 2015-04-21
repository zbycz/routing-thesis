/***************************************
 Routing optimiser.

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


#include "types.h"
#include "nodes.h"
#include "segments.h"
#include "ways.h"
#include "relations.h"

#include "logging.h"
#include "functions.h"
#include "fakes.h"
#include "results.h"


/*+ To help when debugging +*/
#define DEBUG 0


/* Global variables */

/*+ The option not to print any progress information. +*/
extern int option_quiet;

/*+ The option to calculate the quickest route insted of the shortest. +*/
extern int option_quickest;


/* Local functions */

static index_t FindSuperSegment(Nodes *nodes,Segments *segments,Ways *ways,Relations *relations,index_t finish_node,index_t finish_segment);
static Results *FindSuperRoute(Nodes *nodes,Segments *segments,Ways *ways,Relations *relations,index_t start_node,index_t finish_node);


/*++++++++++++++++++++++++++++++++++++++
  Find the optimum route between two nodes not passing through a super-node.

  Results *FindNormalRoute Returns a set of results.

  Nodes *nodes The set of nodes to use.

  Segments *segments The set of segments to use.

  Ways *ways The set of ways to use.

  Relations *relations The set of relations to use.

  Profile *profile The profile containing the transport type, speeds and allowed highways.

  index_t start_node The start node.

  index_t prev_segment The previous segment before the start node.

  index_t finish_node The finish node.
  ++++++++++++++++++++++++++++++++++++++*/

Results *FindNormalRoute(Nodes *nodes,Segments *segments,Ways *ways,Relations *relations,Profile *profile,index_t start_node,index_t prev_segment,index_t finish_node)
{
 Results *results;
 Queue   *queue;
 score_t finish_score;
 double  finish_lat,finish_lon;
 Result  *finish_result;
 Result  *result1,*result2;
 int     force_uturn=0;

#if DEBUG
 printf("  FindNormalRoute(...,start_node=%"Pindex_t" prev_segment=%"Pindex_t" finish_node=%"Pindex_t")\n",start_node,prev_segment,finish_node);
#endif

 /* Set up the finish conditions */

 finish_score=INF_SCORE;
 finish_result=NULL;

 if(IsFakeNode(finish_node))
    GetFakeLatLong(finish_node,&finish_lat,&finish_lon);
 else
    GetLatLong(nodes,finish_node,&finish_lat,&finish_lon);

 /* Create the list of results and insert the first node into the queue */

 results=NewResultsList(64);
 queue=NewQueueList();

 results->start_node=start_node;
 results->prev_segment=prev_segment;

 result1=InsertResult(results,results->start_node,results->prev_segment);

 InsertInQueue(queue,result1);

 /* Check for barrier at start waypoint - must perform U-turn */

 if(prev_segment!=NO_SEGMENT && !IsFakeNode(start_node))
   {
    Node *startp=LookupNode(nodes,start_node,1);

    if(!(startp->allow&profile->allow))
       force_uturn=1;
   }

 /* Loop across all nodes in the queue */

 while((result1=PopFromQueue(queue)))
   {
    Node *node1p=NULL;
    Segment *segmentp;
    index_t node1,seg1,seg1r;
    index_t turnrelation=NO_RELATION;

    /* score must be better than current best score */
    if(result1->score>=finish_score)
       continue;

    node1=result1->node;
    seg1=result1->segment;

    if(IsFakeSegment(seg1))
       seg1r=IndexRealSegment(seg1);
    else
       seg1r=seg1;

    if(!IsFakeNode(node1))
       node1p=LookupNode(nodes,node1,1);

    /* lookup if a turn restriction applies */
    if(profile->turns && node1p && IsTurnRestrictedNode(node1p))
       turnrelation=FindFirstTurnRelation2(relations,node1,seg1r);

    /* Loop across all segments */

    if(IsFakeNode(node1))
       segmentp=FirstFakeSegment(node1);
    else
       segmentp=FirstSegment(segments,node1p,1);

    while(segmentp)
      {
       Node *node2p=NULL;
       Way *wayp;
       index_t node2,seg2,seg2r;
       score_t segment_pref,segment_score,cumulative_score;
       int i;

       node2=OtherNode(segmentp,node1); /* need this here because we use node2 at the end of the loop */

       /* must be a normal segment */
       if(!IsNormalSegment(segmentp))
          goto endloop;

       /* must obey one-way restrictions (unless profile allows) */
       if(profile->oneway && IsOnewayTo(segmentp,node1))
          goto endloop;

       if(IsFakeNode(node1) || IsFakeNode(node2))
         {
          seg2 =IndexFakeSegment(segmentp);
          seg2r=IndexRealSegment(seg2);
         }
       else
         {
          seg2 =IndexSegment(segments,segmentp);
          seg2r=seg2;
         }

       /* must perform U-turn in special cases */
       if(force_uturn && node1==results->start_node)
         {
          if(seg2r!=result1->segment)
             goto endloop;
         }
       else
          /* must not perform U-turn (unless profile allows) */
          if(profile->turns && (seg1==seg2 || seg1==seg2r || seg1r==seg2 || (seg1r==seg2r && IsFakeUTurn(seg1,seg2))))
             goto endloop;

       /* must obey turn relations */
       if(turnrelation!=NO_RELATION && !IsTurnAllowed(relations,turnrelation,node1,seg1r,seg2r,profile->allow))
          goto endloop;

       if(!IsFakeNode(node2))
          node2p=LookupNode(nodes,node2,2);

       /* must not pass over super-node */
       if(node2!=finish_node && node2p && IsSuperNode(node2p))
          goto endloop;

       wayp=LookupWay(ways,segmentp->way,1);

       /* mode of transport must be allowed on the highway */
       if(!(wayp->allow&profile->allow))
          goto endloop;

       /* must obey weight restriction (if exists) */
       if(wayp->weight && wayp->weight<profile->weight)
          goto endloop;

       /* must obey height/width/length restriction (if exist) */
       if((wayp->height && wayp->height<profile->height) ||
          (wayp->width  && wayp->width <profile->width ) ||
          (wayp->length && wayp->length<profile->length))
          goto endloop;

       segment_pref=profile->highway[HIGHWAY(wayp->type)];

       /* highway preferences must allow this highway */
       if(segment_pref==0)
          goto endloop;

       for(i=1;i<Property_Count;i++)
          if(ways->file.props & PROPERTIES(i))
            {
             if(wayp->props & PROPERTIES(i))
                segment_pref*=profile->props_yes[i];
             else
                segment_pref*=profile->props_no[i];
            }

       /* profile preferences must allow this highway */
       if(segment_pref==0)
          goto endloop;

       /* mode of transport must be allowed through node2 unless it is the final node */
       if(node2p && node2!=finish_node && !(node2p->allow&profile->allow))
          goto endloop;

       if(option_quickest==0)
          segment_score=(score_t)DISTANCE(segmentp->distance)/segment_pref;
       else
          segment_score=(score_t)Duration(segmentp,wayp,profile)/segment_pref;

       cumulative_score=result1->score+segment_score;

       /* score must be better than current best score */
       if(cumulative_score>=finish_score)
          goto endloop;

       result2=FindResult(results,node2,seg2);

       if(!result2) /* New end node/segment combination */
         {
          result2=InsertResult(results,node2,seg2);
          result2->prev=result1;
          result2->score=cumulative_score;
         }
       else if(cumulative_score<result2->score) /* New score for end node/segment combination is better */
         {
          result2->prev=result1;
          result2->score=cumulative_score;
          result2->segment=seg2;
         }
       else
          goto endloop;

       if(node2==finish_node)
         {
          finish_score=cumulative_score;
          finish_result=result2;
         }
       else
         {
          result2->sortby=result2->score;
          InsertInQueue(queue,result2);
         }

      endloop:

       if(IsFakeNode(node1))
          segmentp=NextFakeSegment(segmentp,node1);
       else if(IsFakeNode(node2))
          segmentp=NULL; /* cannot call NextSegment() with a fake segment */
       else
         {
          segmentp=NextSegment(segments,segmentp,node1);

          if(!segmentp && IsFakeNode(finish_node))
             segmentp=ExtraFakeSegment(node1,finish_node);
         }
      }
   }

 FreeQueueList(queue);

 /* Check it worked */

 if(!finish_result)
   {
#if DEBUG
    printf("    Failed\n");
#endif

    FreeResultsList(results);
    return(NULL);
   }

 FixForwardRoute(results,finish_result);

#if DEBUG
 Result *r=FindResult(results,results->start_node,results->prev_segment);

 while(r)
   {
    printf("    node=%"Pindex_t" segment=%"Pindex_t" score=%f\n",r->node,r->segment,r->score);

    r=r->next;
   }
#endif

 return(results);
}


/*++++++++++++++++++++++++++++++++++++++
  Find the optimum route between two nodes where the start and end are a set of pre/post-routed super-nodes.

  Results *FindMiddleRoute Returns a set of results.

  Nodes *nodes The set of nodes to use.

  Segments *segments The set of segments to use.

  Ways *ways The set of ways to use.

  Relations *relations The set of relations to use.

  Profile *profile The profile containing the transport type, speeds and allowed highways.

  Results *begin The initial portion of the route.

  Results *end The final portion of the route.
  ++++++++++++++++++++++++++++++++++++++*/

Results *FindMiddleRoute(Nodes *nodes,Segments *segments,Ways *ways,Relations *relations,Profile *profile,Results *begin,Results *end)
{
 Results *results;
 Queue   *queue;
 Result  *finish_result;
 score_t finish_score;
 double  finish_lat,finish_lon;
 Result  *result1,*result2,*result3,*result4;
 int     force_uturn=0;

#if DEBUG
 printf("  FindMiddleRoute(...,[begin has %d nodes],[end has %d nodes])\n",begin->number,end->number);
#endif

#if !DEBUG
 if(!option_quiet)
    printf_first("Routing: Super-Nodes checked = 0");
#endif

 /* Set up the finish conditions */

 finish_score=INF_SCORE;
 finish_result=NULL;

 if(IsFakeNode(end->finish_node))
    GetFakeLatLong(end->finish_node,&finish_lat,&finish_lon);
 else
    GetLatLong(nodes,end->finish_node,&finish_lat,&finish_lon);

 /* Create the list of results and insert the first node into the queue */

 results=NewResultsList(65536);
 queue=NewQueueList();

 results->start_node=begin->start_node;
 results->prev_segment=begin->prev_segment;

 if(begin->number==1)
   {
    if(begin->prev_segment==NO_SEGMENT)
       results->prev_segment=NO_SEGMENT;
    else
      {
       index_t superseg=FindSuperSegment(nodes,segments,ways,relations,begin->start_node,begin->prev_segment);

       results->prev_segment=superseg;
      }
   }

 result1=InsertResult(results,results->start_node,results->prev_segment);

 /* Insert the finish points of the beginning part of the path into the queue,
    translating the segments into super-segments. */

 result3=FirstResult(begin);

 while(result3)
   {
    if((results->start_node!=result3->node || results->prev_segment!=result3->segment) &&
       !IsFakeNode(result3->node) && IsSuperNode(LookupNode(nodes,result3->node,5)))
      {
       Result *result5=result1;
       index_t superseg=FindSuperSegment(nodes,segments,ways,relations,result3->node,result3->segment);

       if(superseg!=result3->segment)
         {
          result5=InsertResult(results,result3->node,result3->segment);

          result5->prev=result1;
         }

       if(!FindResult(results,result3->node,superseg))
         {
          result2=InsertResult(results,result3->node,superseg);
          result2->prev=result5;

          result2->score=result3->score;
          result2->sortby=result3->score;

          InsertInQueue(queue,result2);

          if((result4=FindResult(end,result2->node,result2->segment)))
            {
             if((result2->score+result4->score)<finish_score)
               {
                finish_score=result2->score+result4->score;
                finish_result=result2;
               }
            }
         }
      }

    result3=NextResult(begin,result3);
   }

 if(begin->number==1)
    InsertInQueue(queue,result1);

 /* Check for barrier at start waypoint - must perform U-turn */

 if(begin->number==1 && results->prev_segment!=NO_SEGMENT)
   {
    Node *startp=LookupNode(nodes,result1->node,1);

    if(!(startp->allow&profile->allow))
       force_uturn=1;
   }

 /* Loop across all nodes in the queue */

 while((result1=PopFromQueue(queue)))
   {
    Node *node1p;
    Segment *segmentp;
    index_t node1,seg1;
    index_t turnrelation=NO_RELATION;

    /* score must be better than current best score */
    if(result1->score>=finish_score)
       continue;

    node1=result1->node;
    seg1=result1->segment;

    node1p=LookupNode(nodes,node1,1); /* node1 cannot be a fake node (must be a super-node) */

    /* lookup if a turn restriction applies */
    if(profile->turns && IsTurnRestrictedNode(node1p)) /* node1 cannot be a fake node (must be a super-node) */
       turnrelation=FindFirstTurnRelation2(relations,node1,seg1);

    /* Loop across all segments */

    segmentp=FirstSegment(segments,node1p,1); /* node1 cannot be a fake node (must be a super-node) */

    while(segmentp)
      {
       Node *node2p;
       Way *wayp;
       index_t node2,seg2;
       score_t segment_pref,segment_score,cumulative_score;
       int i;

       /* must be a super segment */
       if(!IsSuperSegment(segmentp))
          goto endloop;

       /* must obey one-way restrictions (unless profile allows) */
       if(profile->oneway && IsOnewayTo(segmentp,node1))
          goto endloop;

       seg2=IndexSegment(segments,segmentp); /* segment cannot be a fake segment (must be a super-segment) */

       /* must perform U-turn in special cases */
       if(force_uturn && node1==results->start_node)
         {
          if(seg2!=result1->segment)
             goto endloop;
         }
       else
          /* must not perform U-turn */
          if(seg1==seg2) /* No fake segments, applies to all profiles */
             goto endloop;

       /* must obey turn relations */
       if(turnrelation!=NO_RELATION && !IsTurnAllowed(relations,turnrelation,node1,seg1,seg2,profile->allow))
          goto endloop;

       wayp=LookupWay(ways,segmentp->way,1);

       /* mode of transport must be allowed on the highway */
       if(!(wayp->allow&profile->allow))
          goto endloop;

       /* must obey weight restriction (if exists) */
       if(wayp->weight && wayp->weight<profile->weight)
          goto endloop;

       /* must obey height/width/length restriction (if exist) */
       if((wayp->height && wayp->height<profile->height) ||
          (wayp->width  && wayp->width <profile->width ) ||
          (wayp->length && wayp->length<profile->length))
          goto endloop;

       segment_pref=profile->highway[HIGHWAY(wayp->type)];

       /* highway preferences must allow this highway */
       if(segment_pref==0)
          goto endloop;

       for(i=1;i<Property_Count;i++)
          if(ways->file.props & PROPERTIES(i))
            {
             if(wayp->props & PROPERTIES(i))
                segment_pref*=profile->props_yes[i];
             else
                segment_pref*=profile->props_no[i];
            }

       /* profile preferences must allow this highway */
       if(segment_pref==0)
          goto endloop;

       node2=OtherNode(segmentp,node1);

       node2p=LookupNode(nodes,node2,2); /* node2 cannot be a fake node (must be a super-node) */

       /* mode of transport must be allowed through node2 unless it is the final node */
       if(node2!=end->finish_node && !(node2p->allow&profile->allow))
          goto endloop;

       if(option_quickest==0)
          segment_score=(score_t)DISTANCE(segmentp->distance)/segment_pref;
       else
          segment_score=(score_t)Duration(segmentp,wayp,profile)/segment_pref;

       cumulative_score=result1->score+segment_score;

       /* score must be better than current best score */
       if(cumulative_score>=finish_score)
          goto endloop;

       result2=FindResult(results,node2,seg2);

       if(!result2) /* New end node/segment pair */
         {
          result2=InsertResult(results,node2,seg2);
          result2->prev=result1;
          result2->score=cumulative_score;
         }
       else if(cumulative_score<result2->score) /* New end node/segment pair is better */
         {
          result2->prev=result1;
          result2->score=cumulative_score;
         }
       else
          goto endloop;

       if((result3=FindResult(end,node2,seg2)))
         {
          if((result2->score+result3->score)<finish_score)
            {
             finish_score=result2->score+result3->score;
             finish_result=result2;
            }
         }
       else
         {
          double lat,lon;
          distance_t direct;

          GetLatLong(nodes,node2,&lat,&lon); /* node2 cannot be a fake node (must be a super-node) */

          direct=Distance(lat,lon,finish_lat,finish_lon);

          if(option_quickest==0)
             result2->sortby=result2->score+(score_t)direct/profile->max_pref;
          else
             result2->sortby=result2->score+(score_t)distance_speed_to_duration(direct,profile->max_speed)/profile->max_pref;

          if(result2->sortby<finish_score)
             InsertInQueue(queue,result2);
         }

#if !DEBUG
       if(!option_quiet && !(results->number%1000))
          printf_middle("Routing: Super-Nodes checked = %d",results->number);
#endif

      endloop:

       segmentp=NextSegment(segments,segmentp,node1); /* node1 cannot be a fake node (must be a super-node) */
      }
   }

#if !DEBUG
 if(!option_quiet)
    printf_last("Routing: Super-Nodes checked = %d",results->number);
#endif

 FreeQueueList(queue);

 /* Check it worked */

 if(!finish_result)
   {
#if DEBUG
    printf("    Failed\n");
#endif

    FreeResultsList(results);
    return(NULL);
   }

 /* Finish off the end part of the route */

 if(finish_result->node!=end->finish_node)
   {
    result3=InsertResult(results,end->finish_node,NO_SEGMENT);

    result3->prev=finish_result;
    result3->score=finish_score;

    finish_result=result3;
   }

 FixForwardRoute(results,finish_result);

#if DEBUG
 Result *r=FindResult(results,results->start_node,results->prev_segment);

 while(r)
   {
    printf("    node=%"Pindex_t" segment=%"Pindex_t" score=%f\n",r->node,r->segment,r->score);

    r=r->next;
   }
#endif

 return(results);
}


/*++++++++++++++++++++++++++++++++++++++
  Find the super-segment that represents the route that contains a particular segment.

  index_t FindSuperSegment Returns the index of the super-segment.

  Nodes *nodes The set of nodes to use.

  Segments *segments The set of segments to use.

  Ways *ways The set of ways to use.

  Relations *relations The set of relations to use.

  index_t finish_node The super-node that the route ends at.

  index_t finish_segment The segment that the route ends with.
  ++++++++++++++++++++++++++++++++++++++*/

static index_t FindSuperSegment(Nodes *nodes,Segments *segments,Ways *ways,Relations *relations,index_t finish_node,index_t finish_segment)
{
 Node *supernodep;
 Segment *supersegmentp;

 if(IsFakeSegment(finish_segment))
    finish_segment=IndexRealSegment(finish_segment);

 supernodep=LookupNode(nodes,finish_node,5); /* finish_node cannot be a fake node (must be a super-node) */
 supersegmentp=LookupSegment(segments,finish_segment,2); /* finish_segment cannot be a fake segment. */

 if(IsSuperSegment(supersegmentp))
    return(finish_segment);

 /* Loop across all segments */

 supersegmentp=FirstSegment(segments,supernodep,3); /* supernode cannot be a fake node (must be a super-node) */

 while(supersegmentp)
   {
    if(IsSuperSegment(supersegmentp))
      {
       Results *results;
       Result *result;
       index_t start_node;

       start_node=OtherNode(supersegmentp,finish_node);

       results=FindSuperRoute(nodes,segments,ways,relations,start_node,finish_node);

       if(!results)
          continue;

       result=FindResult(results,finish_node,finish_segment);

       if(result && (distance_t)result->score==DISTANCE(supersegmentp->distance))
         {
          FreeResultsList(results);
          return(IndexSegment(segments,supersegmentp));
         }

       if(results)
          FreeResultsList(results);
      }

    supersegmentp=NextSegment(segments,supersegmentp,finish_node); /* finish_node cannot be a fake node (must be a super-node) */
   }

 return(finish_segment);
}


/*++++++++++++++++++++++++++++++++++++++
  Find the shortest route between two super-nodes using only normal nodes.
  This is effectively the same function as is used in superx.c when finding super-segments initially.

  Results *FindSuperRoute Returns a set of results.

  Nodes *nodes The set of nodes to use.

  Segments *segments The set of segments to use.

  Ways *ways The set of ways to use.

  Relations *relations The set of relations to use.

  index_t start_node The start node.

  index_t finish_node The finish node.
  ++++++++++++++++++++++++++++++++++++++*/

static Results *FindSuperRoute(Nodes *nodes,Segments *segments,Ways *ways,Relations *relations,index_t start_node,index_t finish_node)
{
 Results *results;
 Queue   *queue;
 Result  *result1,*result2;

#if DEBUG
 printf("    FindSuperRoute(...,start_node=%"Pindex_t" finish_node=%"Pindex_t")\n",start_node,finish_node);
#endif

 /* Create the list of results and insert the first node into the queue */

 results=NewResultsList(64);
 queue=NewQueueList();

 results->start_node=start_node;
 results->prev_segment=NO_SEGMENT;

 result1=InsertResult(results,results->start_node,results->prev_segment);

 InsertInQueue(queue,result1);

 /* Loop across all nodes in the queue */

 while((result1=PopFromQueue(queue)))
   {
    Node *node1p=NULL;
    Segment *segmentp;
    index_t node1,seg1;

    node1=result1->node;
    seg1=result1->segment;

    node1p=LookupNode(nodes,node1,1); /* node1 cannot be a fake node */

    /* Loop across all segments */

    segmentp=FirstSegment(segments,node1p,1); /* node1 cannot be a fake node */

    while(segmentp)
      {
       Node *node2p=NULL;
       index_t node2,seg2;
       score_t cumulative_score;

       /* must be a normal segment */
       if(!IsNormalSegment(segmentp))
          goto endloop;

       /* must obey one-way restrictions */
       if(IsOnewayTo(segmentp,node1))
          goto endloop;

       seg2=IndexSegment(segments,segmentp);

       /* must not perform U-turn */
       if(seg1==seg2)
          goto endloop;

       node2=OtherNode(segmentp,node1);

       node2p=LookupNode(nodes,node2,2); /* node2 cannot be a fake node */

       /* must not pass over super-node */
       if(node2!=finish_node && IsSuperNode(node2p))
          goto endloop;

       /* Specifically looking for the shortest route to emulate superx.c */
       cumulative_score=result1->score+(score_t)DISTANCE(segmentp->distance);

       result2=FindResult(results,node2,seg2);

       if(!result2) /* New end node/segment combination */
         {
          result2=InsertResult(results,node2,seg2);
          result2->prev=result1;
          result2->score=cumulative_score;
          result2->sortby=result2->score;
         }
       else if(cumulative_score<result2->score) /* New score for end node/segment combination is better */
         {
          result2->prev=result1;
          result2->segment=seg2;
          result2->score=cumulative_score;
          result2->sortby=result2->score;
         }
       else goto endloop;

       /* don't route beyond a super-node. */
       if(!IsSuperNode(node2p))
          InsertInQueue(queue,result2);

      endloop:

       segmentp=NextSegment(segments,segmentp,node1);
      }
   }

 FreeQueueList(queue);

#if DEBUG
 Result *r=FindResult(results,results->start_node,results->prev_segment);

 while(r)
   {
    printf("      node=%"Pindex_t" segment=%"Pindex_t" score=%f\n",r->node,r->segment,r->score);

    r=r->next;
   }
#endif

 return(results);
}


/*++++++++++++++++++++++++++++++++++++++
  Find all routes from a specified node to any super-node.

  Results *FindStartRoutes Returns a set of results.

  Nodes *nodes The set of nodes to use.

  Segments *segments The set of segments to use.

  Ways *ways The set of ways to use.

  Relations *relations The set of relations to use.

  Profile *profile The profile containing the transport type, speeds and allowed highways.

  index_t start_node The start node.

  index_t prev_segment The previous segment before the start node.

  index_t finish_node The finish node.
  ++++++++++++++++++++++++++++++++++++++*/

Results *FindStartRoutes(Nodes *nodes,Segments *segments,Ways *ways,Relations *relations,Profile *profile,index_t start_node,index_t prev_segment,index_t finish_node)
{
 Results *results;
 Queue   *queue;
 Result  *result1,*result2;
 int     nsuper=0,force_uturn=0;

#if DEBUG
 printf("  FindStartRoutes(...,start_node=%"Pindex_t" prev_segment=%"Pindex_t" finish_node=%"Pindex_t")\n",start_node,prev_segment,finish_node);
#endif

 /* Create the list of results and insert the first node into the queue */

 results=NewResultsList(64);
 queue=NewQueueList();

 results->start_node=start_node;
 results->prev_segment=prev_segment;

 result1=InsertResult(results,results->start_node,results->prev_segment);

 InsertInQueue(queue,result1);

 /* Check for barrier at start waypoint - must perform U-turn */

 if(prev_segment!=NO_SEGMENT && !IsFakeNode(start_node))
   {
    Node *startp=LookupNode(nodes,start_node,1);

    if(!(startp->allow&profile->allow))
       force_uturn=1;
   }

 /* Loop across all nodes in the queue */

 while((result1=PopFromQueue(queue)))
   {
    Node *node1p=NULL;
    Segment *segmentp;
    index_t node1,seg1,seg1r;
    index_t turnrelation=NO_RELATION;

    node1=result1->node;
    seg1=result1->segment;

    if(IsFakeSegment(seg1))
       seg1r=IndexRealSegment(seg1);
    else
       seg1r=seg1;

    if(!IsFakeNode(node1))
       node1p=LookupNode(nodes,node1,1);

    /* lookup if a turn restriction applies */
    if(profile->turns && node1p && IsTurnRestrictedNode(node1p))
       turnrelation=FindFirstTurnRelation2(relations,node1,seg1r);

    /* Loop across all segments */

    if(IsFakeNode(node1))
       segmentp=FirstFakeSegment(node1);
    else
       segmentp=FirstSegment(segments,node1p,1);

    while(segmentp)
      {
       Node *node2p=NULL;
       Way *wayp;
       index_t node2,seg2,seg2r;
       score_t segment_pref,segment_score,cumulative_score;
       int i;

       node2=OtherNode(segmentp,node1); /* need this here because we use node2 at the end of the loop */

       /* must be a normal segment */
       if(!IsNormalSegment(segmentp))
          goto endloop;

       /* must obey one-way restrictions (unless profile allows) */
       if(profile->oneway && IsOnewayTo(segmentp,node1))
          goto endloop;

       if(IsFakeNode(node1) || IsFakeNode(node2))
         {
          seg2 =IndexFakeSegment(segmentp);
          seg2r=IndexRealSegment(seg2);
         }
       else
         {
          seg2 =IndexSegment(segments,segmentp);
          seg2r=seg2;
         }

       /* must perform U-turn in special cases */
       if(node1==start_node && force_uturn)
         {
          if(seg2r!=result1->segment)
             goto endloop;
         }
       else
          /* must not perform U-turn (unless profile allows) */
          if(profile->turns && (seg1==seg2 || seg1==seg2r || seg1r==seg2 || (seg1r==seg2r && IsFakeUTurn(seg1,seg2))))
             goto endloop;

       /* must obey turn relations */
       if(turnrelation!=NO_RELATION && !IsTurnAllowed(relations,turnrelation,node1,seg1r,seg2r,profile->allow))
          goto endloop;

       wayp=LookupWay(ways,segmentp->way,1);

       /* mode of transport must be allowed on the highway */
       if(!(wayp->allow&profile->allow))
          goto endloop;

       /* must obey weight restriction (if exists) */
       if(wayp->weight && wayp->weight<profile->weight)
          goto endloop;

       /* must obey height/width/length restriction (if exists) */
       if((wayp->height && wayp->height<profile->height) ||
          (wayp->width  && wayp->width <profile->width ) ||
          (wayp->length && wayp->length<profile->length))
          goto endloop;

       segment_pref=profile->highway[HIGHWAY(wayp->type)];

       /* highway preferences must allow this highway */
       if(segment_pref==0)
          goto endloop;

       for(i=1;i<Property_Count;i++)
          if(ways->file.props & PROPERTIES(i))
            {
             if(wayp->props & PROPERTIES(i))
                segment_pref*=profile->props_yes[i];
             else
                segment_pref*=profile->props_no[i];
            }

       /* profile preferences must allow this highway */
       if(segment_pref==0)
          goto endloop;

       if(!IsFakeNode(node2))
          node2p=LookupNode(nodes,node2,2);

       /* mode of transport must be allowed through node2 unless it is the final node */
       if(node2p && node2!=finish_node && !(node2p->allow&profile->allow))
          goto endloop;

       if(option_quickest==0)
          segment_score=(score_t)DISTANCE(segmentp->distance)/segment_pref;
       else
          segment_score=(score_t)Duration(segmentp,wayp,profile)/segment_pref;

       cumulative_score=result1->score+segment_score;

       result2=FindResult(results,node2,seg2);

       if(!result2) /* New end node/segment combination */
         {
          result2=InsertResult(results,node2,seg2);
          result2->prev=result1;
          result2->score=cumulative_score;

          if(node2p && IsSuperNode(node2p))
             nsuper++;

          if(node2==finish_node)
             nsuper++;
         }
       else if(cumulative_score<result2->score) /* New score for end node/segment combination is better */
         {
          result2->prev=result1;
          result2->score=cumulative_score;
         }
       else
          goto endloop;

       if(node2p && !IsSuperNode(node2p))
         {
          result2->sortby=result2->score;
          InsertInQueue(queue,result2);
         }

      endloop:

       if(IsFakeNode(node1))
          segmentp=NextFakeSegment(segmentp,node1);
       else if(IsFakeNode(node2))
          segmentp=NULL; /* cannot call NextSegment() with a fake segment */
       else
         {
          segmentp=NextSegment(segments,segmentp,node1);

          if(!segmentp && IsFakeNode(finish_node))
             segmentp=ExtraFakeSegment(node1,finish_node);
         }
      }
   }

 FreeQueueList(queue);

 /* Check it worked */

 if(results->number==1 || nsuper==0)
   {
#if DEBUG
    printf("    Failed (%d results, %d super)\n",results->number,nsuper);
#endif

    FreeResultsList(results);
    return(NULL);
   }

#if DEBUG
 Result *s=FirstResult(results);

 while(s)
   {
    if(s->node==finish_node)
      {
       Result *r=FindResult(results,s->node,s->segment);

       printf("    -------- route to finish node\n");

       while(r)
         {
          printf("    node=%"Pindex_t" segment=%"Pindex_t" score=%f\n",r->node,r->segment,r->score);

          r=r->prev;
         }
      }

    if(!IsFakeNode(s->node))
      {
       Node *n=LookupNode(nodes,s->node,1);

       if(IsSuperNode(n))
         {
          Result *r=FindResult(results,s->node,s->segment);

          printf("    -------- route to super node\n");

          while(r)
            {
             printf("    node=%"Pindex_t" segment=%"Pindex_t" score=%f\n",r->node,r->segment,r->score);

             r=r->prev;
            }
         }
      }

    s=NextResult(results,s);
   }
#endif

 return(results);
}


/*++++++++++++++++++++++++++++++++++++++
  Continue finding routes from a set of super-nodes to a finish point.

  Results *ExtendStartRoutes Returns the set of results that were passed in.

  Nodes *nodes The set of nodes to use.

  Segments *segments The set of segments to use.

  Ways *ways The set of ways to use.

  Relations *relations The set of relations to use.

  Profile *profile The profile containing the transport type, speeds and allowed highways.

  Results *being The partial set of routes already computed.

  index_t finish_node The finish node.
  ++++++++++++++++++++++++++++++++++++++*/

Results *ExtendStartRoutes(Nodes *nodes,Segments *segments,Ways *ways,Relations *relations,Profile *profile,Results *begin,index_t finish_node)
{
 Results *results=begin;
 Queue   *queue;
 Result  *result1,*result2,*result3;
 Result  *finish_result=NULL;
 score_t finish_score=INF_SCORE;

#if DEBUG
 printf("  ExtendStartRoutes(...,[begin has %d nodes],finish_node=%"Pindex_t")\n",begin->number,finish_node);
#endif

 /* Check the list of results and insert the super nodes into the queue */

 queue=NewQueueList();

 result3=FirstResult(begin);

 while(result3)
   {
    if(result3->node==finish_node)
       if(result3->score<finish_score)
         {
          finish_score=result3->score;
          finish_result=result3;
         }

    if(!IsFakeNode(result3->node))
       if(IsSuperNode(LookupNode(nodes,result3->node,5)))
          InsertInQueue(queue,result3);

    result3=NextResult(results,result3);
   }

 /* Loop across all nodes in the queue */

 while((result1=PopFromQueue(queue)))
   {
    Node *node1p=NULL;
    Segment *segmentp;
    index_t node1,seg1,seg1r;
    index_t turnrelation=NO_RELATION;

    /* score must be better than current best score */
    if(result1->score>=finish_score)
       continue;

    node1=result1->node;
    seg1=result1->segment;

    if(IsFakeSegment(seg1))
       seg1r=IndexRealSegment(seg1);
    else
       seg1r=seg1;

    if(!IsFakeNode(node1))
       node1p=LookupNode(nodes,node1,1);

    /* lookup if a turn restriction applies */
    if(profile->turns && node1p && IsTurnRestrictedNode(node1p))
       turnrelation=FindFirstTurnRelation2(relations,node1,seg1r);

    /* Loop across all segments */

    if(IsFakeNode(node1))
       segmentp=FirstFakeSegment(node1);
    else
       segmentp=FirstSegment(segments,node1p,1);

    while(segmentp)
      {
       Node *node2p=NULL;
       Way *wayp;
       index_t node2,seg2,seg2r;
       score_t segment_pref,segment_score,cumulative_score;
       int i;

       node2=OtherNode(segmentp,node1); /* need this here because we use node2 at the end of the loop */

       /* must be a normal segment */
       if(!IsNormalSegment(segmentp))
          goto endloop;

       /* must obey one-way restrictions (unless profile allows) */
       if(profile->oneway && IsOnewayTo(segmentp,node1))
          goto endloop;

       if(IsFakeNode(node1) || IsFakeNode(node2))
         {
          seg2 =IndexFakeSegment(segmentp);
          seg2r=IndexRealSegment(seg2);
         }
       else
         {
          seg2 =IndexSegment(segments,segmentp);
          seg2r=seg2;
         }

       /* must not perform U-turn (unless profile allows) */
       if(profile->turns && (seg1==seg2 || seg1==seg2r || seg1r==seg2 || (seg1r==seg2r && IsFakeUTurn(seg1,seg2))))
          goto endloop;

       /* must obey turn relations */
       if(turnrelation!=NO_RELATION && !IsTurnAllowed(relations,turnrelation,node1,seg1r,seg2r,profile->allow))
          goto endloop;

       wayp=LookupWay(ways,segmentp->way,1);

       /* mode of transport must be allowed on the highway */
       if(!(wayp->allow&profile->allow))
          goto endloop;

       /* must obey weight restriction (if exists) */
       if(wayp->weight && wayp->weight<profile->weight)
          goto endloop;

       /* must obey height/width/length restriction (if exists) */
       if((wayp->height && wayp->height<profile->height) ||
          (wayp->width  && wayp->width <profile->width ) ||
          (wayp->length && wayp->length<profile->length))
          goto endloop;

       segment_pref=profile->highway[HIGHWAY(wayp->type)];

       /* highway preferences must allow this highway */
       if(segment_pref==0)
          goto endloop;

       for(i=1;i<Property_Count;i++)
          if(ways->file.props & PROPERTIES(i))
            {
             if(wayp->props & PROPERTIES(i))
                segment_pref*=profile->props_yes[i];
             else
                segment_pref*=profile->props_no[i];
            }

       /* profile preferences must allow this highway */
       if(segment_pref==0)
          goto endloop;

       if(!IsFakeNode(node2))
          node2p=LookupNode(nodes,node2,2);

       /* mode of transport must be allowed through node2 unless it is the final node */
       if(node2p && node2!=finish_node && !(node2p->allow&profile->allow))
          goto endloop;

       if(option_quickest==0)
          segment_score=(score_t)DISTANCE(segmentp->distance)/segment_pref;
       else
          segment_score=(score_t)Duration(segmentp,wayp,profile)/segment_pref;

       cumulative_score=result1->score+segment_score;

       /* score must be better than current best score */
       if(cumulative_score>=finish_score)
          goto endloop;

       result2=FindResult(results,node2,seg2);

       if(!result2) /* New end node/segment combination */
         {
          result2=InsertResult(results,node2,seg2);
          result2->prev=result1;
          result2->score=cumulative_score;
         }
       else if(cumulative_score<result2->score) /* New score for end node/segment combination is better */
         {
          result2->prev=result1;
          result2->score=cumulative_score;
         }
       else
          goto endloop;

       if(node2==finish_node)
         {
          if(cumulative_score<finish_score)
            {
             finish_score=cumulative_score;
             finish_result=result2;
            }
         }
       else
         {
          result2->sortby=result2->score;
          InsertInQueue(queue,result2);
         }

      endloop:

       if(IsFakeNode(node1))
          segmentp=NextFakeSegment(segmentp,node1);
       else if(IsFakeNode(node2))
          segmentp=NULL; /* cannot call NextSegment() with a fake segment */
       else
         {
          segmentp=NextSegment(segments,segmentp,node1);

          if(!segmentp && IsFakeNode(finish_node))
             segmentp=ExtraFakeSegment(node1,finish_node);
         }
      }
   }

 FreeQueueList(queue);

 FixForwardRoute(results,finish_result);

#if DEBUG
 Result *r=FindResult(results,results->start_node,results->prev_segment);

 while(r)
   {
    printf("    node=%"Pindex_t" segment=%"Pindex_t" score=%f\n",r->node,r->segment,r->score);

    r=r->next;
   }
#endif

 return(results);
}


/*++++++++++++++++++++++++++++++++++++++
  Find all routes from any super-node to a specific node (by working backwards from the specific node to all super-nodes).

  Results *FindFinishRoutes Returns a set of results.

  Nodes *nodes The set of nodes to use.

  Segments *segments The set of segments to use.

  Ways *ways The set of ways to use.

  Relations *relations The set of relations to use.

  Profile *profile The profile containing the transport type, speeds and allowed highways.

  index_t finish_node The finishing node.
  ++++++++++++++++++++++++++++++++++++++*/

Results *FindFinishRoutes(Nodes *nodes,Segments *segments,Ways *ways,Relations *relations,Profile *profile,index_t finish_node)
{
 Results *results,*results2;
 Queue   *queue;
 Result  *result1,*result2,*result3;

#if DEBUG
 printf("  FindFinishRoutes(...,finish_node=%"Pindex_t")\n",finish_node);
#endif

 /* Create the results and insert the finish node into the queue */

 results=NewResultsList(64);
 queue=NewQueueList();

 results->finish_node=finish_node;

 result1=InsertResult(results,finish_node,NO_SEGMENT);

 InsertInQueue(queue,result1);

 /* Loop across all nodes in the queue */

 while((result1=PopFromQueue(queue)))
   {
    Node *node1p=NULL;
    Segment *segmentp;
    index_t node1,seg1,seg1r;
    index_t turnrelation=NO_RELATION;

    node1=result1->node;
    seg1=result1->segment;

    if(IsFakeSegment(seg1))
       seg1r=IndexRealSegment(seg1);
    else
       seg1r=seg1;

    if(!IsFakeNode(node1))
       node1p=LookupNode(nodes,node1,1);

    /* lookup if a turn restriction applies */
    if(profile->turns && node1p && IsTurnRestrictedNode(node1p))
       turnrelation=FindFirstTurnRelation1(relations,node1); /* working backwards => turn relation sort order doesn't help */

    /* Loop across all segments */

    if(IsFakeNode(node1))
       segmentp=FirstFakeSegment(node1);
    else
       segmentp=FirstSegment(segments,node1p,1);

    while(segmentp)
      {
       Node *node2p=NULL;
       Way *wayp;
       index_t node2,seg2,seg2r;
       score_t segment_pref,segment_score,cumulative_score;
       int i;

       /* must be a normal segment unless node1 is a super-node (see below). */
       if((IsFakeNode(node1) || !IsSuperNode(node1p)) && !IsNormalSegment(segmentp))
          goto endloop;

       /* must be a super segment if node1 is a super-node to give starting super-segment for finding middle route. */
       if((!IsFakeNode(node1) && IsSuperNode(node1p)) && !IsSuperSegment(segmentp))
          goto endloop;

       /* must obey one-way restrictions (unless profile allows) */
       if(profile->oneway && IsOnewayFrom(segmentp,node1)) /* working backwards => disallow oneway *from* node1 */
          goto endloop;

       node2=OtherNode(segmentp,node1);

       if(IsFakeNode(node1) || IsFakeNode(node2))
         {
          seg2 =IndexFakeSegment(segmentp);
          seg2r=IndexRealSegment(seg2);
         }
       else
         {
          seg2 =IndexSegment(segments,segmentp);
          seg2r=seg2;
         }

       /* must not perform U-turn (unless profile allows) */
       if(profile->turns && (seg1==seg2 || seg1==seg2r || seg1r==seg2 || (seg1r==seg2r && IsFakeUTurn(seg1,seg2))))
          goto endloop;

       /* must obey turn relations */
       if(turnrelation!=NO_RELATION)
         {
          index_t turnrelation2=FindFirstTurnRelation2(relations,node1,seg2r); /* node2 -> node1 -> result1->next->node */

          if(turnrelation2!=NO_RELATION && !IsTurnAllowed(relations,turnrelation2,node1,seg2r,seg1r,profile->allow))
             goto endloop;
         }

       wayp=LookupWay(ways,segmentp->way,1);

       /* mode of transport must be allowed on the highway */
       if(!(wayp->allow&profile->allow))
          goto endloop;

       /* must obey weight restriction (if exists) */
       if(wayp->weight && wayp->weight<profile->weight)
          goto endloop;

       /* must obey height/width/length restriction (if exist) */
       if((wayp->height && wayp->height<profile->height) ||
          (wayp->width  && wayp->width <profile->width ) ||
          (wayp->length && wayp->length<profile->length))
          goto endloop;

       segment_pref=profile->highway[HIGHWAY(wayp->type)];

       /* highway preferences must allow this highway */
       if(segment_pref==0)
          goto endloop;

       for(i=1;i<Property_Count;i++)
          if(ways->file.props & PROPERTIES(i))
            {
             if(wayp->props & PROPERTIES(i))
                segment_pref*=profile->props_yes[i];
             else
                segment_pref*=profile->props_no[i];
            }

       /* profile preferences must allow this highway */
       if(segment_pref==0)
          goto endloop;

       if(!IsFakeNode(node2))
          node2p=LookupNode(nodes,node2,2);

       /* mode of transport must be allowed through node2 */
       if(node2p && !(node2p->allow&profile->allow))
          goto endloop;

       if(option_quickest==0)
          segment_score=(score_t)DISTANCE(segmentp->distance)/segment_pref;
       else
          segment_score=(score_t)Duration(segmentp,wayp,profile)/segment_pref;

       cumulative_score=result1->score+segment_score;

       result2=FindResult(results,node2,seg2);

       if(!result2) /* New end node */
         {
          result2=InsertResult(results,node2,seg2);
          result2->next=result1;   /* working backwards */
          result2->score=cumulative_score;
         }
       else if(cumulative_score<result2->score) /* New end node is better */
         {
          result2->next=result1; /* working backwards */
          result2->score=cumulative_score;
         }
       else
          goto endloop;

       if(IsFakeNode(node1) || !IsSuperNode(node1p))
         {
          result2->sortby=result2->score;
          InsertInQueue(queue,result2);
         }

      endloop:

       if(IsFakeNode(node1))
          segmentp=NextFakeSegment(segmentp,node1);
       else
          segmentp=NextSegment(segments,segmentp,node1);
      }
   }

 FreeQueueList(queue);

 /* Check it worked */

 if(results->number==1)
   {
#if DEBUG
    printf("    Failed\n");
#endif

    FreeResultsList(results);
    return(NULL);
   }

 /* Create a results structure with the node at the end of the segment opposite the start */

 results2=NewResultsList(64);

 results2->finish_node=results->finish_node;

 result3=FirstResult(results);

 while(result3)
   {
    if(result3->next)
      {
       result2=InsertResult(results2,result3->next->node,result3->segment);

       result2->score=result3->next->score;
      }

    result3=NextResult(results,result3);
   }

 /* Fix up the result->next pointers */

 result3=FirstResult(results);

 while(result3)
   {
    if(result3->next && result3->next->next)
      {
       result1=FindResult(results2,result3->next->node,result3->segment);
       result2=FindResult(results2,result3->next->next->node,result3->next->segment);

       result1->next=result2;
      }

    result3=NextResult(results,result3);
   }

 FreeResultsList(results);

#if DEBUG
 Result *s=FirstResult(results2);

 while(s)
   {
    if(!IsFakeNode(s->node))
      {
       Node *n=LookupNode(nodes,s->node,1);

       if(IsSuperNode(n))
         {
          Result *r=FindResult(results2,s->node,s->segment);

          printf("    --------\n");

          while(r)
            {
             printf("    node=%"Pindex_t" segment=%"Pindex_t" score=%f\n",r->node,r->segment,r->score);

             r=r->next;
            }
         }
      }

    s=NextResult(results2,s);
   }
#endif

 return(results2);
}


/*++++++++++++++++++++++++++++++++++++++
  Create an optimum route given the set of super-nodes to follow.

  Results *CombineRoutes Returns the results from joining the super-nodes.

  Nodes *nodes The set of nodes to use.

  Segments *segments The set of segments to use.

  Ways *ways The set of ways to use.

  Relations *relations The set of relations to use.

  Profile *profile The profile containing the transport type, speeds and allowed highways.

  Results *begin The set of results for the start of the route.

  Results *middle The set of results from the super-node route.
  ++++++++++++++++++++++++++++++++++++++*/

Results *CombineRoutes(Nodes *nodes,Segments *segments,Ways *ways,Relations *relations,Profile *profile,Results *begin,Results *middle)
{
 Result *midres,*comres1;
 Results *combined;

#if DEBUG
 printf("  CombineRoutes(...,[begin has %d nodes],[middle has %d nodes])\n",begin->number,middle->number);
#endif

 combined=NewResultsList(256);

 combined->start_node=begin->start_node;
 combined->prev_segment=begin->prev_segment;

 /* Insert the start point */

 midres=FindResult(middle,middle->start_node,middle->prev_segment);

 comres1=InsertResult(combined,combined->start_node,combined->prev_segment);

 /* Insert the start of the route */

 if(begin->number>1 && midres->next)
   {
    Result *begres;

    midres=FindResult(middle,midres->next->node,midres->next->segment);

    begres=FindResult(begin,midres->node,midres->segment);

    FixForwardRoute(begin,begres);

    if(midres->next && midres->next->node==midres->node)
       midres=midres->next;

    begres=FindResult(begin,begin->start_node,begin->prev_segment);

    begres=begres->next;

    do
      {
       Result *comres2;

       comres2=InsertResult(combined,begres->node,begres->segment);

       comres2->score=begres->score;
       comres2->prev=comres1;

       begres=begres->next;

       comres1=comres2;
      }
    while(begres);
   }

 /* Sort out the combined route */

 do
   {
    Result *result;

    if(midres->next)
      {
       Results *results=FindNormalRoute(nodes,segments,ways,relations,profile,comres1->node,comres1->segment,midres->next->node);

       if(!results)
          return(NULL);

       result=FindResult(results,midres->node,comres1->segment);

       result=result->next;

       /*
        *      midres                          midres->next
        *         =                                  =
        *      ---*----------------------------------*  = middle
        *
        *      ---*----.----.----.----.----.----.----*  = results
        *              =
        *             result
        *
        *      ---*----.----.----.----.----.----.----*  = combined
        *         =    =
        *   comres1  comres2
        */

       do
         {
          Result *comres2;

          comres2=InsertResult(combined,result->node,result->segment);

          comres2->score=midres->score+result->score;
          comres2->prev=comres1;

          result=result->next;

          comres1=comres2;
         }
       while(result);

       FreeResultsList(results);
      }

    midres=midres->next;
   }
 while(midres);

 FixForwardRoute(combined,comres1);

#if DEBUG
 Result *r=FindResult(combined,combined->start_node,combined->prev_segment);

 while(r)
   {
    printf("    node=%"Pindex_t" segment=%"Pindex_t" score=%f\n",r->node,r->segment,r->score);

    r=r->next;
   }
#endif

 return(combined);
}


/*++++++++++++++++++++++++++++++++++++++
  Fix the forward route (i.e. setup next pointers for forward path from prev nodes on reverse path).

  Results *results The set of results to update.

  Result *finish_result The result for the finish point.
  ++++++++++++++++++++++++++++++++++++++*/

void FixForwardRoute(Results *results,Result *finish_result)
{
 Result *result2=finish_result;

 /* Erase the old route if there is one */

 if(results->finish_node!=NO_NODE)
   {
    Result *result1=FirstResult(results);

    while(result1)
      {
       result1->next=NULL;

       result1=NextResult(results,result1);
      }
   }

 /* Create the forward links for the optimum path */

 do
   {
    Result *result1;

    if(result2->prev)
      {
       index_t node1=result2->prev->node;
       index_t seg1=result2->prev->segment;

       result1=FindResult(results,node1,seg1);

       logassert(!result1->next,"Unable to reverse route through results (report a bug)"); /* Bugs elsewhere can lead to infinite loop here. */

       result1->next=result2;

       result2=result1;
      }
    else
       result2=NULL;
   }
 while(result2);

 results->finish_node=finish_result->node;
 results->last_segment=finish_result->segment;
}
