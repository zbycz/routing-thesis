
#include <stdio.h>
#include "srtmHgtReader.h"
#include "types.h"
#include "nodesx.h"
#include "segmentsx.h"

//#define radians_to_degrees(xxx) ((xxx)*(180.0/M_PI))
//#define degrees_to_radians(xxx) ((xxx)*(M_PI/180.0))




void AddAscentDescentToSegments(NodesX* nodesx, SegmentsX* segmentsx)
{
 SegmentX segmentx;
 index_t index=0;

 if(segmentsx->number==0)
    return;

 /* Print the start message */

 printf_first("Computing ascent/descent: Segments=0");

 /* Open the file read-only */

 segmentsx->fd=ReOpenFile(segmentsx->filename_tmp);

 /* Read the on-disk image */

 while(!ReadFile(segmentsx->fd,&segmentx,sizeof(SegmentX)))
   {
    index_t node1=segmentx.node1; 
    index_t node2=segmentx.node2; 
    
    printf("n1 %u, n2 %u\n", node1, node2);
    
    NodeX *nodex1=LookupNodeX(nodesx,node1,1);
    
    printf("%f", radians_to_degrees(nodex1->latitude));
    NodeX *nodex2=LookupNodeX(nodesx,node2,2);
    printf("%f", radians_to_degrees(nodex2->latitude));
    
    float ele1 = srtmGetElevation(radians_to_degrees(nodex1->latitude), radians_to_degrees(nodex1->longitude));
    float ele2 = srtmGetElevation(radians_to_degrees(nodex2->latitude), radians_to_degrees(nodex2->longitude));
    
    float eledif = ele1-ele2;
    segmentx.ascent = eledif < 0 ? fabs(eledif) : 0;
    segmentx.descent = eledif > 0 ? eledif : 0;
    
    
    PutBackSegmentX(segmentsx, segmentx);
    
    index++;

    if(!(index%10000))
       printf_middle("Computing ascent/descent: Segments=%"Pindex_t,index);
   }

 /* Close the file */

 segmentsx->fd=CloseFile(segmentsx->fd);

 /* Print the final message */

 printf_last("Computing ascent/descent: Segments=%"Pindex_t,segmentsx->number);
    
}
