/***************************************
 Data pruning functions.

 Part of the Routino routing software.
 ******************/ /******************
 This file Copyright 2011-2013 Andrew M. Bishop

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

#include "types.h"
#include "segments.h"

#include "typesx.h"
#include "nodesx.h"
#include "segmentsx.h"
#include "waysx.h"

#include "prunex.h"

#include "files.h"
#include "logging.h"


/* Global variables */

/*+ The command line '--tmpdir' option or its default value. +*/
extern char *option_tmpdirname;

/* Local functions */

static void prune_segment(SegmentsX *segmentsx,SegmentX *segmentx);
static void modify_segment(SegmentsX *segmentsx,SegmentX *segmentx,index_t newnode1,index_t newnode2);

static void unlink_segment_node1_refs(SegmentsX *segmentsx,SegmentX *segmentx);
static void unlink_segment_node2_refs(SegmentsX *segmentsx,SegmentX *segmentx);

static double distance(double lat1,double lon1,double lat2,double lon2);


/*++++++++++++++++++++++++++++++++++++++
  Initialise the data structures needed for pruning.

  NodesX *nodesx The set of nodes to use.

  SegmentsX *segmentsx The set of segments to use.

  WaysX *waysx The set of ways to use.
  ++++++++++++++++++++++++++++++++++++++*/

void StartPruning(NodesX *nodesx,SegmentsX *segmentsx,WaysX *waysx)
{
 SegmentX segmentx;
 index_t index=0,lastnode1=NO_NODE;

 if(segmentsx->number==0)
    return;

 /* Print the start message */

 printf_first("Adding Extra Segment Indexes: Segments=0");

 /* Allocate the array of next segment */

 segmentsx->next1=(index_t*)calloc(segmentsx->number,sizeof(index_t));

 logassert(segmentsx->next1,"Failed to allocate memory (try using slim mode?)"); /* Check malloc() worked */

 /* Open the file read-only */

 segmentsx->fd=ReOpenFile(segmentsx->filename_tmp);

 /* Read the on-disk image */

 while(!ReadFile(segmentsx->fd,&segmentx,sizeof(SegmentX)))
   {
    index_t node1=segmentx.node1;

    if(index==0)
       ;
    else if(lastnode1==node1)
       segmentsx->next1[index-1]=index;
    else
       segmentsx->next1[index-1]=NO_SEGMENT;

    lastnode1=node1;
    index++;

    if(!(index%10000))
       printf_middle("Added Extra Segment Indexes: Segments=%"Pindex_t,index);
   }

 segmentsx->next1[index-1]=NO_SEGMENT;

 /* Close the file */

 segmentsx->fd=CloseFile(segmentsx->fd);

 /* Print the final message */

 printf_last("Added Extra Segment Indexes: Segments=%"Pindex_t,segmentsx->number);
}


/*++++++++++++++++++++++++++++++++++++++
  Delete the data structures needed for pruning.

  NodesX *nodesx The set of nodes to use.

  SegmentsX *segmentsx The set of segments to use.

  WaysX *waysx The set of ways to use.
  ++++++++++++++++++++++++++++++++++++++*/

void FinishPruning(NodesX *nodesx,SegmentsX *segmentsx,WaysX *waysx)
{
 if(segmentsx->next1)
    free(segmentsx->next1);

 segmentsx->next1=NULL;
}


/*++++++++++++++++++++++++++++++++++++++
  Prune out any groups of nodes and segments whose total length is less than a
  specified minimum.

  NodesX *nodesx The set of nodes to use.

  SegmentsX *segmentsx The set of segments to use.

  WaysX *waysx The set of ways to use.

  distance_t minimum The minimum distance to keep.
  ++++++++++++++++++++++++++++++++++++++*/

void PruneIsolatedRegions(NodesX *nodesx,SegmentsX *segmentsx,WaysX *waysx,distance_t minimum)
{
 transport_t transport;
 BitMask *connected,*region;
 index_t *regionsegments,*othersegments;
 int nallocregionsegments,nallocothersegments;
 index_t nnewways=0;
 int fd;

 if(nodesx->number==0 || segmentsx->number==0)
    return;

 /* Map into memory / open the files */

#if !SLIM
 nodesx->data=MapFile(nodesx->filename_tmp);
 segmentsx->data=MapFileWriteable(segmentsx->filename_tmp);
 waysx->data=MapFile(waysx->filename_tmp);
#else
 nodesx->fd=ReOpenFile(nodesx->filename_tmp);
 segmentsx->fd=ReOpenFileWriteable(segmentsx->filename_tmp);
 waysx->fd=ReOpenFile(waysx->filename_tmp);
#endif

 fd=ReOpenFileWriteable(waysx->filename_tmp);

 connected=AllocBitMask(segmentsx->number);
 region   =AllocBitMask(segmentsx->number);

 logassert(connected,"Failed to allocate memory (try using slim mode?)"); /* Check AllocBitMask() worked */
 logassert(region,"Failed to allocate memory (try using slim mode?)");    /* Check AllocBitMask() worked */

 regionsegments=(index_t*)malloc((nallocregionsegments=1024)*sizeof(index_t));
 othersegments =(index_t*)malloc((nallocothersegments =1024)*sizeof(index_t));

 logassert(regionsegments,"Failed to allocate memory (try using slim mode?)"); /* Check malloc() worked */
 logassert(othersegments,"Failed to allocate memory (try using slim mode?)");  /* Check malloc() worked */

 /* Loop through the transport types */

 for(transport=Transport_None+1;transport<Transport_Count;transport++)
   {
    index_t i,j;
    index_t nregions=0,npruned=0,nadjusted=0;
    const char *transport_str=TransportName(transport);
    transports_t transports=TRANSPORTS(transport);

    /* Print the start message */

    printf_first("Pruning Isolated Regions (%s): Segments=0 Adjusted=0 Pruned=0",transport_str);

    /* Loop through the segments and find the disconnected ones */

    ClearAllBits(connected,segmentsx->number);
    ClearAllBits(region   ,segmentsx->number);

    for(i=0;i<segmentsx->number;i++)
      {
       int nregionsegments=0,nothersegments=0;
       distance_t total=0;
       SegmentX *segmentx;
       WayX *wayx,tmpwayx;

       if(IsBitSet(connected,i))
          goto endloop;

       segmentx=LookupSegmentX(segmentsx,i,1);

       if(IsPrunedSegmentX(segmentx))
          goto endloop;

       if(segmentx->way<waysx->number)
          wayx=LookupWayX(waysx,segmentx->way,1);
       else
          SeekReadFile(fd,(wayx=&tmpwayx),sizeof(WayX),segmentx->way*sizeof(WayX));

       if(!(wayx->way.allow&transports))
          goto endloop;

       othersegments[nothersegments++]=i;
       SetBit(region,i);

       do
         {
          index_t thissegment,nodes[2];

          thissegment=othersegments[--nothersegments];

          if(nregionsegments==nallocregionsegments)
             regionsegments=(index_t*)realloc(regionsegments,(nallocregionsegments+=1024)*sizeof(index_t));

          regionsegments[nregionsegments++]=thissegment;

          segmentx=LookupSegmentX(segmentsx,thissegment,1);

          nodes[0]=segmentx->node1;
          nodes[1]=segmentx->node2;
          total+=DISTANCE(segmentx->distance);

          for(j=0;j<2;j++)
            {
             NodeX *nodex=LookupNodeX(nodesx,nodes[j],1);

             if(!(nodex->allow&transports))
                continue;

             segmentx=FirstSegmentX(segmentsx,nodes[j],1);

             while(segmentx)
               {
                index_t segment=IndexSegmentX(segmentsx,segmentx);

                if(segment!=thissegment)
                  {
                   if(segmentx->way<waysx->number)
                      wayx=LookupWayX(waysx,segmentx->way,1);
                   else
                      SeekReadFile(fd,(wayx=&tmpwayx),sizeof(WayX),segmentx->way*sizeof(WayX));

                   if(wayx->way.allow&transports)
                     {
                      /* Already connected - finish */

                      if(IsBitSet(connected,segment))
                        {
                         total=minimum;
                         goto foundconnection;
                        }

                      /* Not in region - add to list */

                      if(!IsBitSet(region,segment))
                        {
                         if(nothersegments==nallocothersegments)
                            othersegments=(index_t*)realloc(othersegments,(nallocothersegments+=1024)*sizeof(index_t));

                         othersegments[nothersegments++]=segment;
                         SetBit(region,segment);
                        }
                     }
                  }

                segmentx=NextSegmentX(segmentsx,segmentx,nodes[j]);
               }
            }
         }
       while(nothersegments>0 && total<minimum);

      foundconnection:

       /* Prune the segments or mark them as connected */

       if(total<minimum)        /* not connected - delete them */
         {
          nregions++;

          for(j=0;j<nregionsegments;j++)
            {
             SegmentX *segmentx;
             WayX *wayx,tmpwayx;

             SetBit(connected,regionsegments[j]); /* not really connected, but don't need to check again */
             ClearBit(region,regionsegments[j]);

             segmentx=LookupSegmentX(segmentsx,regionsegments[j],1);

             if(segmentx->way<waysx->number)
                wayx=LookupWayX(waysx,segmentx->way,1);
             else
                SeekReadFile(fd,(wayx=&tmpwayx),sizeof(WayX),segmentx->way*sizeof(WayX));

             if(wayx->way.allow==transports)
               {
                prune_segment(segmentsx,segmentx);

                npruned++;
               }
             else
               {
                if(segmentx->way<waysx->number) /* create a new way */
                  {
                   tmpwayx=*wayx;

                   tmpwayx.way.allow&=~transports;

                   segmentx->way=waysx->number+nnewways;

                   SeekWriteFile(fd,&tmpwayx,sizeof(WayX),segmentx->way*sizeof(WayX));

                   nnewways++;

                   PutBackSegmentX(segmentsx,segmentx);
                  }
                else            /* modify the existing one */
                  {
                   tmpwayx.way.allow&=~transports;

                   SeekWriteFile(fd,&tmpwayx,sizeof(WayX),segmentx->way*sizeof(WayX));
                  }

                nadjusted++;
               }
            }
         }
       else                     /* connected - mark as part of the main region */
         {
          for(j=0;j<nregionsegments;j++)
            {
             SetBit(connected,regionsegments[j]);
             ClearBit(region,regionsegments[j]);
            }

          for(j=0;j<nothersegments;j++)
            {
             SetBit(connected,othersegments[j]);
             ClearBit(region,othersegments[j]);
            }
         }

      endloop:

       if(!((i+1)%10000))
          printf_middle("Pruning Isolated Regions (%s): Segments=%"Pindex_t" Adjusted=%"Pindex_t" Pruned=%"Pindex_t" (%"Pindex_t" Regions)",transport_str,i+1,nadjusted,npruned,nregions);
      }

    /* Print the final message */

    printf_last("Pruned Isolated Regions (%s): Segments=%"Pindex_t" Adjusted=%"Pindex_t" Pruned=%"Pindex_t" (%"Pindex_t" Regions)",transport_str,segmentsx->number,nadjusted,npruned,nregions);
   }

 /* Unmap from memory / close the files */

 free(region);
 free(connected);

 free(regionsegments);
 free(othersegments);

#if !SLIM
 nodesx->data=UnmapFile(nodesx->data);
 segmentsx->data=UnmapFile(segmentsx->data);
 waysx->data=UnmapFile(waysx->data);
#else
 nodesx->fd=CloseFile(nodesx->fd);
 segmentsx->fd=CloseFile(segmentsx->fd);
 waysx->fd=CloseFile(waysx->fd);
#endif

 CloseFile(fd);

 waysx->number+=nnewways;
}


/*++++++++++++++++++++++++++++++++++++++
  Prune out any segments that are shorter than a specified minimum.

  NodesX *nodesx The set of nodes to use.

  SegmentsX *segmentsx The set of segments to use.

  WaysX *waysx The set of ways to use.

  distance_t minimum The maximum length to remove or one less than the minimum length to keep.
  ++++++++++++++++++++++++++++++++++++++*/

void PruneShortSegments(NodesX *nodesx,SegmentsX *segmentsx,WaysX *waysx,distance_t minimum)
{
 index_t i;
 index_t nshort=0,npruned=0;

 if(nodesx->number==0 || segmentsx->number==0 || waysx->number==0)
    return;

 /* Print the start message */

 printf_first("Pruning Short Segments: Segments=0 Short=0 Pruned=0");

 /* Map into memory / open the files */

#if !SLIM
 nodesx->data=MapFileWriteable(nodesx->filename_tmp);
 segmentsx->data=MapFileWriteable(segmentsx->filename_tmp);
 waysx->data=MapFile(waysx->filename_tmp);
#else
 nodesx->fd=ReOpenFileWriteable(nodesx->filename_tmp);
 segmentsx->fd=ReOpenFileWriteable(segmentsx->filename_tmp);
 waysx->fd=ReOpenFile(waysx->filename_tmp);
#endif

 /* Loop through the segments and find the short ones for possible modification */

 for(i=0;i<segmentsx->number;i++)
   {
    SegmentX *segmentx2=LookupSegmentX(segmentsx,i,2);

    if(IsPrunedSegmentX(segmentx2))
       goto endloop;

    /*
                       :
      Initial state: ..N3 -------- N2
                       :     S2

                       :
      Final state:   ..N3
                       :

      = OR =

                       :                               :
      Initial state: ..N1 -------- N2 ---- N3 -------- N4..
                       :     S1        S2        S3    :

                       :                               :
      Final state:   ..N1 ------------ N3 ------------ N4..
                       :       S1               S3     :

      Not if N1 is the same as N4.
      Must not delete N2 (or N3) if S2 (or S3) has different one-way properties from S1.
      Must not delete N2 (or N3) if S2 (or S3) has different highway properties from S1.
      Must combine N2, S2 and N3 disallowed transports into new N3.
      Must not delete N2 (or N3) if it is a mini-roundabout.
      Must not delete N2 (or N3) if it is involved in a turn restriction.

      = OR =

                       :                   :
      Initial state: ..N1 -------- N2 ---- N3..
                       :     S1        S2  :

                       :               :
      Final state:   ..N1 ------------ N3..
                       :       S1      :

      Not if N1 is the same as N3.
      Not if S1 has different one-way properties from S2.
      Not if S1 has different highway properties from S2.
      Not if N2 disallows transports allowed on S1 and S2.
      Not if N2 is a mini-roundabout.
      Not if N2 is involved in a turn restriction.
     */

    if(DISTANCE(segmentx2->distance)<=minimum)
      {
       index_t node1=NO_NODE,node2,node3,node4=NO_NODE;
       index_t segment1=NO_SEGMENT,segment2=i,segment3=NO_SEGMENT;
       SegmentX *segmentx;
       int segcount2=0,segcount3=0;

       nshort++;

       node2=segmentx2->node1;
       node3=segmentx2->node2;

       /* Count the segments connected to N2 */

       segmentx=FirstSegmentX(segmentsx,node2,4);

       while(segmentx)
         {
          segcount2++;

          if(segment1==NO_SEGMENT)
            {
             index_t segment=IndexSegmentX(segmentsx,segmentx);

             if(segment!=segment2)
               {
                segment1=segment;
                node1=OtherNode(segmentx,node2);
               }
            }
          else if(segcount2>2)
             break;

          segmentx=NextSegmentX(segmentsx,segmentx,node2);
         }

       /* Count the segments connected to N3 */

       segmentx=FirstSegmentX(segmentsx,node3,4);

       while(segmentx)
         {
          segcount3++;

          if(segment3==NO_SEGMENT)
            {
             index_t segment=IndexSegmentX(segmentsx,segmentx);

             if(segment!=segment2)
               {
                segment3=segment;
                node4=OtherNode(segmentx,node3);
               }
            }
          else if(segcount3>2)
             break;

          segmentx=NextSegmentX(segmentsx,segmentx,node3);
         }

       /* Check which case we are handling (and canonicalise) */

       if(segcount2>2 && segcount3>2) /* none of the cases in diagram - too complicated */
         {
          goto endloop;
         }
       else if(segcount2==1 || segcount3==1) /* first case in diagram - prune segment */
         {
          prune_segment(segmentsx,segmentx2);
         }
       else if(segcount2==2 && segcount3==2) /* second case in diagram - modify one segment and prune segment */
         {
          SegmentX *segmentx1,*segmentx3;
          WayX *wayx1,*wayx2,*wayx3;
          NodeX *nodex2,*nodex3,*newnodex;
          index_t newnode;
          int join12=1,join23=1;

          /* Check if pruning would collapse a loop */

          if(node1==node4)
             goto endloop;

          /* Check if allowed due to one-way properties */

          segmentx1=LookupSegmentX(segmentsx,segment1,1);
          segmentx3=LookupSegmentX(segmentsx,segment3,3);

          if(!IsOneway(segmentx1) && !IsOneway(segmentx2))
             ;
          else if(IsOneway(segmentx1) && IsOneway(segmentx2))
            {
             if(IsOnewayTo(segmentx1,node2) && !IsOnewayFrom(segmentx2,node2)) /* S1 is one-way but S2 doesn't continue */
                join12=0;

             if(IsOnewayFrom(segmentx1,node2) && !IsOnewayTo(segmentx2,node2)) /* S1 is one-way but S2 doesn't continue */
                join12=0;
            }
          else
             join12=0;

          if(!IsOneway(segmentx3) && !IsOneway(segmentx2))
             ;
          else if(IsOneway(segmentx3) && IsOneway(segmentx2))
            {
             if(IsOnewayTo(segmentx3,node3) && !IsOnewayFrom(segmentx2,node3)) /* S3 is one-way but S2 doesn't continue */
                join23=0;

             if(IsOnewayFrom(segmentx3,node3) && !IsOnewayTo(segmentx2,node3)) /* S3 is one-way but S2 doesn't continue */
                join23=0;
            }
          else
             join23=0;

          if(!join12 && !join23)
             goto endloop;

          /* Check if allowed due to highway properties */

          wayx1=LookupWayX(waysx,segmentx1->way,1);
          wayx2=LookupWayX(waysx,segmentx2->way,2);
          wayx3=LookupWayX(waysx,segmentx3->way,3);

          if(WaysCompare(&wayx1->way,&wayx2->way))
             join12=0;

          if(WaysCompare(&wayx3->way,&wayx2->way))
             join23=0;

          if(!join12 && !join23)
             goto endloop;

          /* Check if allowed due to mini-roundabout and turn restriction */

          nodex2=LookupNodeX(nodesx,node2,2);
          nodex3=LookupNodeX(nodesx,node3,3);

          if(nodex2->flags&NODE_MINIRNDBT)
             join12=0;

          if(nodex3->flags&NODE_MINIRNDBT)
             join23=0;

          if(!join12 && !join23)
             goto endloop;

          if(nodex2->flags&NODE_TURNRSTRCT2 || nodex2->flags&NODE_TURNRSTRCT)
             join12=0;

          if(nodex3->flags&NODE_TURNRSTRCT2 || nodex3->flags&NODE_TURNRSTRCT)
             join23=0;

          if(!join12 && !join23)
             goto endloop;

          /* New node properties */

          if(join12)
            {
             newnode=node3;
             newnodex=nodex3;
            }
          else /* if(join23) */
            {
             newnode=node2;
             newnodex=nodex2;
            }

          newnodex->allow=nodex2->allow&nodex3->allow; /* combine the restrictions of the two nodes */
          newnodex->allow&=~((~wayx2->way.allow)&wayx3->way.allow); /* disallow anything blocked by segment2 */
          newnodex->allow&=~((~wayx2->way.allow)&wayx1->way.allow); /* disallow anything blocked by segment2 */

          newnodex->latitude =(nodex2->latitude +nodex3->latitude )/2;
          newnodex->longitude=(nodex2->longitude+nodex3->longitude)/2;

          PutBackNodeX(nodesx,newnodex);

          /* Modify segments */

          segmentx1->distance+=DISTANCE(segmentx2->distance)/2;
          segmentx3->distance+=DISTANCE(segmentx2->distance)-DISTANCE(segmentx2->distance)/2;

          if(segmentx1->node1==node1)
            {
             if(segmentx1->node2!=newnode)
                modify_segment(segmentsx,segmentx1,node1,newnode);
             else
                PutBackSegmentX(segmentsx,segmentx1);
            }
          else /* if(segmentx1->node2==node1) */
            {
             if(segmentx1->node1!=newnode)
                modify_segment(segmentsx,segmentx1,newnode,node1);
             else
                PutBackSegmentX(segmentsx,segmentx1);
            }

          if(segmentx3->node1==node4)
            {
             if(segmentx3->node2!=newnode)
                modify_segment(segmentsx,segmentx3,node4,newnode);
             else
                PutBackSegmentX(segmentsx,segmentx3);
            }
          else /* if(segmentx3->node2==node4) */
            {
             if(segmentx3->node1!=newnode)
                modify_segment(segmentsx,segmentx3,newnode,node4);
             else
                PutBackSegmentX(segmentsx,segmentx3);
            }

          ReLookupSegmentX(segmentsx,segmentx2);

          prune_segment(segmentsx,segmentx2);
         }
       else                     /* third case in diagram - prune one segment */
         {
          SegmentX *segmentx1;
          WayX *wayx1,*wayx2;
          NodeX *nodex2;

          if(segcount3==2) /* not as in diagram, shuffle things round */
            {
             index_t temp;

             temp=segment1; segment1=segment3; segment3=temp;
             temp=node1; node1=node4; node4=temp;
             temp=node2; node2=node3; node3=temp;
            }

          /* Check if pruning would collapse a loop */

          if(node1==node3)
             goto endloop;

          /* Check if allowed due to one-way properties */

          segmentx1=LookupSegmentX(segmentsx,segment1,1);

          if(!IsOneway(segmentx1) && !IsOneway(segmentx2))
             ;
          else if(IsOneway(segmentx1) && IsOneway(segmentx2))
            {
             if(IsOnewayTo(segmentx1,node2) && !IsOnewayFrom(segmentx2,node2)) /* S1 is one-way but S2 doesn't continue */
                goto endloop;

             if(IsOnewayFrom(segmentx1,node2) && !IsOnewayTo(segmentx2,node2)) /* S1 is one-way but S2 doesn't continue */
                goto endloop;
            }
          else
             goto endloop;

          /* Check if allowed due to highway properties */

          wayx1=LookupWayX(waysx,segmentx1->way,1);
          wayx2=LookupWayX(waysx,segmentx2->way,2);

          if(WaysCompare(&wayx1->way,&wayx2->way))
             goto endloop;

          /* Check if allowed due to mini-roundabout and turn restriction */

          nodex2=LookupNodeX(nodesx,node2,2);

          if(nodex2->flags&NODE_MINIRNDBT)
             goto endloop;

          if(nodex2->flags&NODE_TURNRSTRCT2 || nodex2->flags&NODE_TURNRSTRCT)
             goto endloop;

          /* Check if allowed due to node restrictions */

          if((nodex2->allow&wayx1->way.allow)!=wayx1->way.allow)
             goto endloop;

          if((nodex2->allow&wayx2->way.allow)!=wayx2->way.allow)
             goto endloop;

          /* Modify segments */

          segmentx1->distance+=DISTANCE(segmentx2->distance);

          if(segmentx1->node1==node1)
             modify_segment(segmentsx,segmentx1,node1,node3);
          else /* if(segmentx1->node2==node1) */
             modify_segment(segmentsx,segmentx1,node3,node1);

          ReLookupSegmentX(segmentsx,segmentx2);

          prune_segment(segmentsx,segmentx2);
         }

       npruned++;
      }

   endloop:

    if(!((i+1)%10000))
       printf_middle("Pruning Short Segments: Segments=%"Pindex_t" Short=%"Pindex_t" Pruned=%"Pindex_t,i+1,nshort,npruned);
   }

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

 printf_last("Pruned Short Segments: Segments=%"Pindex_t" Short=%"Pindex_t" Pruned=%"Pindex_t,segmentsx->number,nshort,npruned);
}


/*++++++++++++++++++++++++++++++++++++++
  Prune out any nodes from straight highways where the introduced error is smaller than a specified maximum.

  NodesX *nodesx The set of nodes to use.

  SegmentsX *segmentsx The set of segments to use.

  WaysX *waysx The set of ways to use.

  distance_t maximum The maximum error to introduce.
  ++++++++++++++++++++++++++++++++++++++*/

void PruneStraightHighwayNodes(NodesX *nodesx,SegmentsX *segmentsx,WaysX *waysx,distance_t maximum)
{
 index_t i;
 index_t npruned=0;
 BitMask *checked;
 int nalloc;
 index_t *nodes,*segments;
 double *lats,*lons;
 double maximumf;

 if(nodesx->number==0 || segmentsx->number==0 || waysx->number==0)
    return;

 maximumf=distance_to_km(maximum);

 /* Print the start message */

 printf_first("Pruning Straight Highway Nodes: Nodes=0 Pruned=0");

 /* Map into memory / open the files */

#if !SLIM
 nodesx->data=MapFile(nodesx->filename_tmp);
 segmentsx->data=MapFileWriteable(segmentsx->filename_tmp);
 waysx->data=MapFile(waysx->filename_tmp);
#else
 nodesx->fd=ReOpenFile(nodesx->filename_tmp);
 segmentsx->fd=ReOpenFileWriteable(segmentsx->filename_tmp);
 waysx->fd=ReOpenFile(waysx->filename_tmp);
#endif

 checked=AllocBitMask(nodesx->number);

 logassert(checked,"Failed to allocate memory (try using slim mode?)"); /* Check AllocBitMask() worked */

 nodes   =(index_t*)malloc((nalloc=1024)*sizeof(index_t));
 segments=(index_t*)malloc( nalloc      *sizeof(index_t));

 logassert(nodes,"Failed to allocate memory (try using slim mode?)");    /* Check malloc() worked */
 logassert(segments,"Failed to allocate memory (try using slim mode?)"); /* Check malloc() worked */

 lats=(double*)malloc(nalloc*sizeof(double));
 lons=(double*)malloc(nalloc*sizeof(double));

 logassert(lats,"Failed to allocate memory (try using slim mode?)");    /* Check malloc() worked */
 logassert(lons,"Failed to allocate memory (try using slim mode?)");    /* Check malloc() worked */

 /* Loop through the nodes and find stretches of simple highway for possible modification */

 for(i=0;i<nodesx->number;i++)
   {
    int lowerbounded=0,upperbounded=0;
    index_t lower=nalloc/2,current=nalloc/2,upper=nalloc/2;

    if(IsBitSet(checked,i))
       goto endloop;

    if(segmentsx->firstnode[i]==NO_SEGMENT)
       goto endloop;

    /* Find all connected nodes */

    nodes[current]=i;

    do
      {
       index_t node1=NO_NODE,node2=NO_NODE;
       index_t segment1=NO_SEGMENT,segment2=NO_SEGMENT;
       index_t way1=NO_WAY,way2=NO_WAY;
       int segcount=0;
       NodeX *nodex;

       /* Get the node data */

       nodex=LookupNodeX(nodesx,nodes[current],1);

       lats[current]=latlong_to_radians(nodex->latitude);
       lons[current]=latlong_to_radians(nodex->longitude);

       /* Count the segments at the node if not forced to be an end node */

       if(IsBitSet(checked,nodes[current]))
          ;
       else if(nodex->flags&NODE_MINIRNDBT)
          ;
       else if(nodex->flags&NODE_TURNRSTRCT2 || nodex->flags&NODE_TURNRSTRCT)
          ;
       else
         {
          SegmentX *segmentx;

          /* Count the segments connected to the node */

          segmentx=FirstSegmentX(segmentsx,nodes[current],3);

          while(segmentx)
            {
             segcount++;

             if(node1==NO_NODE)
               {
                segment1=IndexSegmentX(segmentsx,segmentx);
                node1=OtherNode(segmentx,nodes[current]);
                way1=segmentx->way;
               }
             else if(node2==NO_NODE)
               {
                segment2=IndexSegmentX(segmentsx,segmentx);
                node2=OtherNode(segmentx,nodes[current]);
                way2=segmentx->way;
               }
             else
                break;

             segmentx=NextSegmentX(segmentsx,segmentx,nodes[current]);
            }
         }

       /* Check if allowed due to one-way properties */

       if(segcount==2)
         {
          SegmentX *segmentx1,*segmentx2;

          segmentx1=LookupSegmentX(segmentsx,segment1,1);
          segmentx2=LookupSegmentX(segmentsx,segment2,2);

          if(!IsOneway(segmentx1) && !IsOneway(segmentx2))
             ;
          else if(IsOneway(segmentx1) && IsOneway(segmentx2))
            {
             if(IsOnewayTo(segmentx1,nodes[current]) && !IsOnewayFrom(segmentx2,nodes[current])) /* S1 is one-way but S2 doesn't continue */
                segcount=0;

             if(IsOnewayFrom(segmentx1,nodes[current]) && !IsOnewayTo(segmentx2,nodes[current])) /* S1 is one-way but S2 doesn't continue */
                segcount=0;
            }
          else
             segcount=0;
         }

       /* Check if allowed due to highway properties and node restrictions */

       if(segcount==2)
         {
          WayX *wayx1,*wayx2;

          wayx1=LookupWayX(waysx,way1,1);
          wayx2=LookupWayX(waysx,way2,2);

          if(WaysCompare(&wayx1->way,&wayx2->way))
             segcount=0;

          if(wayx1->way.name!=wayx2->way.name)
             segcount=0;

          if((nodex->allow&wayx1->way.allow)!=wayx1->way.allow)
             segcount=0;

          if((nodex->allow&wayx2->way.allow)!=wayx2->way.allow)
             segcount=0;
         }

       /* Update the lists */

       if(segcount==2)
         {
          /* Make space in the lists */

          if(upper==(nalloc-1))
            {
             nodes   =(index_t*)realloc(nodes   ,(nalloc+=1024)*sizeof(index_t));
             segments=(index_t*)realloc(segments, nalloc       *sizeof(index_t));

             lats=(double*)realloc(lats,nalloc*sizeof(double));
             lons=(double*)realloc(lons,nalloc*sizeof(double));
            }

          if(lower==0)     /* move everything up by one */
            {
             memmove(nodes+1   ,nodes   ,(1+upper-lower)*sizeof(index_t));
             memmove(segments+1,segments,(1+upper-lower)*sizeof(index_t));

             memmove(lats+1,lats,(1+upper-lower)*sizeof(double));
             memmove(lons+1,lons,(1+upper-lower)*sizeof(double));

             current++;
             lower++;
             upper++;
            }

          if(lower==upper) /* first */
            {
             lower--;

             nodes[lower]=node1;
             segments[lower]=segment1;

             upper++;

             nodes[upper]=node2;
             segments[upper-1]=segment2;
             segments[upper]=NO_SEGMENT;

             current--;
            }
          else if(current==lower)
            {
             lower--;

             if(nodes[current+1]==node2)
               {
                nodes[lower]=node1;
                segments[lower]=segment1;
               }
             else /* if(nodes[current+1]==node1) */
               {
                nodes[lower]=node2;
                segments[lower]=segment2;
               }

             current--;
            }
          else /* if(current==upper) */
            {
             upper++;

             if(nodes[current-1]==node2)
               {
                nodes[upper]=node1;
                segments[upper-1]=segment1;
               }
             else /* if(nodes[current-1]==node1) */
               {
                nodes[upper]=node2;
                segments[upper-1]=segment2;
               }

             segments[upper]=NO_SEGMENT;

             current++;
            }

          if(nodes[upper]==nodes[lower])
            {
             if(!lowerbounded && !upperbounded)
               {
                nodex=LookupNodeX(nodesx,nodes[lower],1);

                lats[lower]=latlong_to_radians(nodex->latitude);
                lons[lower]=latlong_to_radians(nodex->longitude);
               }

             lats[upper]=lats[lower];
             lons[upper]=lons[lower];

             lowerbounded=1;
             upperbounded=1;
            }
         }
       else /* if(segment!=2) */
         {
          if(current==upper)
             upperbounded=1;

          if(current==lower)
            {
             lowerbounded=1;
             current=upper;
            }
         }
      }
    while(!(lowerbounded && upperbounded));

    /* Mark the nodes */

    for(current=lower;current<=upper;current++)
       SetBit(checked,nodes[current]);

    /* Check for straight highway */

    for(;lower<(upper-1);lower++)
      {
       for(current=upper;current>(lower+1);current--)
         {
          SegmentX *segmentx;
          distance_t dist=0;
          double dist1,dist2,dist3,dist3a,dist3b,distp;
          index_t c;

          dist3=distance(lats[lower],lons[lower],lats[current],lons[current]);

          for(c=lower+1;c<current;c++)
            {
             dist1=distance(lats[lower]  ,lons[lower]  ,lats[c],lons[c]);
             dist2=distance(lats[current],lons[current],lats[c],lons[c]);

             /* Use law of cosines (assume flat Earth) */

             dist3a=(dist1*dist1-dist2*dist2+dist3*dist3)/(2.0*dist3);
             dist3b=dist3-dist3a;

             if((dist1+dist2)<dist3)
                distp=0;
             else if(dist3a>=0 && dist3b>=0)
                distp=sqrt(dist1*dist1-dist3a*dist3a);
             else if(dist3a>0)
                distp=dist2;
             else /* if(dist3b>0) */
                distp=dist1;

             if(distp>maximumf) /* gone too far */
                break;
            }

          if(c<current) /* not finished */
             continue;

          /* Delete some segments and shift along */

          for(c=lower+1;c<current;c++)
            {
             segmentx=LookupSegmentX(segmentsx,segments[c],1);

             dist+=DISTANCE(segmentx->distance);

             prune_segment(segmentsx,segmentx);

             npruned++;
            }

          segmentx=LookupSegmentX(segmentsx,segments[lower],1);

          if(nodes[lower]==nodes[current]) /* loop; all within maximum distance */
            {
             prune_segment(segmentsx,segmentx);

             npruned++;
            }
          else
            {
             segmentx->distance+=dist;

             if(segmentx->node1==nodes[lower])
                modify_segment(segmentsx,segmentx,nodes[lower],nodes[current]);
             else /* if(segmentx->node2==nodes[lower]) */
                modify_segment(segmentsx,segmentx,nodes[current],nodes[lower]);
            }

          lower=current-1;
          break;
         }
      }

   endloop:

    if(!((i+1)%10000))
       printf_middle("Pruning Straight Highway Nodes: Nodes=%"Pindex_t" Pruned=%"Pindex_t,i+1,npruned);
   }

 /* Unmap from memory / close the files */

 free(checked);

 free(nodes);
 free(segments);

 free(lats);
 free(lons);

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

 printf_last("Pruned Straight Highway Nodes: Nodes=%"Pindex_t" Pruned=%"Pindex_t,nodesx->number,npruned);
}


/*++++++++++++++++++++++++++++++++++++++
  Prune a segment; unused nodes and ways will get marked for pruning later.

  SegmentsX *segmentsx The set of segments to use.

  SegmentX *segmentx The segment to be pruned.
  ++++++++++++++++++++++++++++++++++++++*/

static void prune_segment(SegmentsX *segmentsx,SegmentX *segmentx)
{
 unlink_segment_node1_refs(segmentsx,segmentx);

 unlink_segment_node2_refs(segmentsx,segmentx);

 segmentx->node1=NO_NODE;
 segmentx->node2=NO_NODE;
 segmentx->next2=NO_SEGMENT;

 PutBackSegmentX(segmentsx,segmentx);
}


/*++++++++++++++++++++++++++++++++++++++
  Modify a segment's nodes; unused nodes will get marked for pruning later.

  SegmentsX *segmentsx The set of segments to use.

  SegmentX *segmentx The segment to be modified.

  index_t newnode1 The new value of node1.

  index_t newnode2 The new value of node2.
  ++++++++++++++++++++++++++++++++++++++*/

static void modify_segment(SegmentsX *segmentsx,SegmentX *segmentx,index_t newnode1,index_t newnode2)
{
 index_t thissegment=IndexSegmentX(segmentsx,segmentx);

 if(newnode1>newnode2)          /* rotate the segment around */
   {
    index_t temp;

    if(segmentx->distance&(ONEWAY_2TO1|ONEWAY_1TO2))
       segmentx->distance^=ONEWAY_2TO1|ONEWAY_1TO2;

    temp=newnode1;
    newnode1=newnode2;
    newnode2=temp;
   }

 if(newnode1!=segmentx->node1)
    unlink_segment_node1_refs(segmentsx,segmentx);

 if(newnode2!=segmentx->node2)
    unlink_segment_node2_refs(segmentsx,segmentx);

 if(newnode1!=segmentx->node1) /* only modify it if the node has changed */
   {
    segmentx->node1=newnode1;

    segmentsx->next1[thissegment]=segmentsx->firstnode[newnode1];
    segmentsx->firstnode[newnode1]=thissegment;
   }

 if(newnode2!=segmentx->node2) /* only modify it if the node has changed */
   {
    segmentx->node2=newnode2;

    segmentx->next2=segmentsx->firstnode[newnode2];
    segmentsx->firstnode[newnode2]=thissegment;
   }

 PutBackSegmentX(segmentsx,segmentx);
}


/*++++++++++++++++++++++++++++++++++++++
  Unlink a node1 from a segment by modifying the linked list type arrangement of node references.

  SegmentsX *segmentsx The set of segments to use.

  SegmentX *segmentx The segment to be modified.
  ++++++++++++++++++++++++++++++++++++++*/

static void unlink_segment_node1_refs(SegmentsX *segmentsx,SegmentX *segmentx)
{
 index_t segment,thissegment;

 thissegment=IndexSegmentX(segmentsx,segmentx);

 segment=segmentsx->firstnode[segmentx->node1];

 if(segment==thissegment)
    segmentsx->firstnode[segmentx->node1]=segmentsx->next1[thissegment];
 else
   {
    do
      {
       index_t nextsegment;
       SegmentX *segx=LookupSegmentX(segmentsx,segment,4);

       if(segx->node1==segmentx->node1)
         {
          nextsegment=segmentsx->next1[segment];

          if(nextsegment==thissegment)
             segmentsx->next1[segment]=segmentsx->next1[thissegment];
         }
       else /* if(segx->node2==segmentx->node1) */
         {
          nextsegment=segx->next2;

          if(nextsegment==thissegment)
            {
             segx->next2=segmentsx->next1[thissegment];

             PutBackSegmentX(segmentsx,segx);
            }
         }

       segment=nextsegment;
      }
    while(segment!=thissegment && segment!=NO_SEGMENT);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Unlink a node2 from a segment by modifying the linked list type arrangement of node references.

  SegmentsX *segmentsx The set of segments to use.

  SegmentX *segmentx The segment to be modified.
  ++++++++++++++++++++++++++++++++++++++*/

static void unlink_segment_node2_refs(SegmentsX *segmentsx,SegmentX *segmentx)
{
 index_t segment,thissegment;

 thissegment=IndexSegmentX(segmentsx,segmentx);

 segment=segmentsx->firstnode[segmentx->node2];

 if(segment==thissegment)
    segmentsx->firstnode[segmentx->node2]=segmentx->next2;
 else
   {
    do
      {
       index_t nextsegment;
       SegmentX *segx=LookupSegmentX(segmentsx,segment,4);

       if(segx->node1==segmentx->node2)
         {
          nextsegment=segmentsx->next1[segment];

          if(nextsegment==thissegment)
             segmentsx->next1[segment]=segmentx->next2;
         }
       else /* if(segx->node2==segmentx->node2) */
         {
          nextsegment=segx->next2;

          if(nextsegment==thissegment)
            {
             segx->next2=segmentx->next2;

             PutBackSegmentX(segmentsx,segx);
            }
         }

       segment=nextsegment;
      }
    while(segment!=thissegment && segment!=NO_SEGMENT);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Calculate the distance between two locations.

  double distance Returns the distance between the locations.

  double lat1 The latitude of the first location.

  double lon1 The longitude of the first location.

  double lat2 The latitude of the second location.

  double lon2 The longitude of the second location.
  ++++++++++++++++++++++++++++++++++++++*/

static double distance(double lat1,double lon1,double lat2,double lon2)
{
 double dlon = lon1 - lon2;
 double dlat = lat1 - lat2;

 double a1,a2,a,sa,c,d;

 if(dlon==0 && dlat==0)
   return 0;

 a1 = sin (dlat / 2);
 a2 = sin (dlon / 2);
 a = (a1 * a1) + cos (lat1) * cos (lat2) * a2 * a2;
 sa = sqrt (a);
 if (sa <= 1.0)
   {c = 2 * asin (sa);}
 else
   {c = 2 * asin (1.0);}
 d = 6378.137 * c;

 return(d);
}
