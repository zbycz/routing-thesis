/***************************************
 Way data type functions.

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

#include "ways.h"

#include "files.h"


/*++++++++++++++++++++++++++++++++++++++
  Load in a way list from a file.

  Ways *LoadWayList Returns the way list.

  const char *filename The name of the file to load.
  ++++++++++++++++++++++++++++++++++++++*/

Ways *LoadWayList(const char *filename)
{
 Ways *ways;
#if SLIM
 int i;
#endif

 ways=(Ways*)malloc(sizeof(Ways));

#if !SLIM

 ways->data=MapFile(filename);

 /* Copy the WaysFile structure from the loaded data */

 ways->file=*((WaysFile*)ways->data);

 /* Set the pointers in the Ways structure. */

 ways->ways =(Way *)(ways->data+sizeof(WaysFile));
 ways->names=(char*)(ways->data+sizeof(WaysFile)+ways->file.number*sizeof(Way));

#else

 ways->fd=ReOpenFile(filename);

 /* Copy the WaysFile header structure from the loaded data */

 ReadFile(ways->fd,&ways->file,sizeof(WaysFile));

 for(i=0;i<sizeof(ways->cached)/sizeof(ways->cached[0]);i++)
    ways->incache[i]=NO_WAY;

 ways->namesoffset=sizeof(WaysFile)+ways->file.number*sizeof(Way);

 for(i=0;i<sizeof(ways->cached)/sizeof(ways->cached[0]);i++)
    ways->ncached[i]=NULL;

#endif

 return(ways);
}


/*++++++++++++++++++++++++++++++++++++++
  Return 0 if the two ways are the same (in respect of their types and limits),
           otherwise return positive or negative to allow sorting.

  int WaysCompare Returns a comparison.

  Way *way1p The first way.

  Way *way2p The second way.
  ++++++++++++++++++++++++++++++++++++++*/

int WaysCompare(Way *way1p,Way *way2p)
{
 if(way1p==way2p)
    return(0);

 if(way1p->type!=way2p->type)
    return((int)way1p->type - (int)way2p->type);

 if(way1p->allow!=way2p->allow)
    return((int)way1p->allow - (int)way2p->allow);

 if(way1p->props!=way2p->props)
    return((int)way1p->props - (int)way2p->props);

 if(way1p->speed!=way2p->speed)
    return((int)way1p->speed - (int)way2p->speed);

 if(way1p->weight!=way2p->weight)
    return((int)way1p->weight - (int)way2p->weight);

 if(way1p->height!=way2p->height)
    return((int)way1p->height - (int)way2p->height);

 if(way1p->width!=way2p->width)
    return((int)way1p->width - (int)way2p->width);

 if(way1p->length!=way2p->length)
    return((int)way1p->length - (int)way2p->length);

 return(0);
}
