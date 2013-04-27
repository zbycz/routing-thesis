/***************************************
 Extended Way data type functions.

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

#include "types.h"
#include "ways.h"

#include "typesx.h"
#include "segmentsx.h"
#include "waysx.h"

#include "files.h"
#include "logging.h"
#include "sorting.h"


/* Global variables */

/*+ The command line '--tmpdir' option or its default value. +*/
extern char *option_tmpdirname;

/* Local variables */

/*+ Temporary file-local variables for use by the sort functions. +*/
static WaysX *sortwaysx;
static SegmentsX *sortsegmentsx;

/* Local functions */

static int sort_by_id(WayX *a,WayX *b);
static int deduplicate_by_id(WayX *wayx,index_t index);

static int sort_by_name(WayX *a,WayX *b);
static int index_by_id(WayX *wayx,index_t index);

static int delete_unused(WayX *wayx,index_t index);
static int sort_by_name_and_prop_and_id(WayX *a,WayX *b);
static int deduplicate_and_index_by_compact_id(WayX *wayx,index_t index);


/*++++++++++++++++++++++++++++++++++++++
  Allocate a new way list (create a new file or open an existing one).

  WaysX *NewWayList Returns the way list.

  int append Set to 1 if the file is to be opened for appending.

  int readonly Set to 1 if the file is to be opened for reading.
  ++++++++++++++++++++++++++++++++++++++*/

WaysX *NewWayList(int append,int readonly)
{
 WaysX *waysx;

 waysx=(WaysX*)calloc(1,sizeof(WaysX));

 logassert(waysx,"Failed to allocate memory (try using slim mode?)"); /* Check calloc() worked */

 waysx->filename    =(char*)malloc(strlen(option_tmpdirname)+32);
 waysx->filename_tmp=(char*)malloc(strlen(option_tmpdirname)+32);

 sprintf(waysx->filename    ,"%s/waysx.parsed.mem",option_tmpdirname);
 sprintf(waysx->filename_tmp,"%s/waysx.%p.tmp"    ,option_tmpdirname,(void*)waysx);

 if(append || readonly)
    if(ExistsFile(waysx->filename))
      {
       off_t size,position=0;
       int fd;

       size=SizeFile(waysx->filename);

       fd=ReOpenFile(waysx->filename);

       while(position<size)
         {
          FILESORT_VARINT waysize;

          SeekReadFile(fd,&waysize,FILESORT_VARSIZE,position);

          waysx->number++;
          position+=waysize+FILESORT_VARSIZE;
         }

       CloseFile(fd);

       RenameFile(waysx->filename,waysx->filename_tmp);
      }

 if(append)
    waysx->fd=OpenFileAppend(waysx->filename_tmp);
 else if(!readonly)
    waysx->fd=OpenFileNew(waysx->filename_tmp);
 else
    waysx->fd=-1;


 waysx->nfilename_tmp=(char*)malloc(strlen(option_tmpdirname)+32);

 sprintf(waysx->nfilename_tmp,"%s/waynames.%p.tmp",option_tmpdirname,(void*)waysx);

 return(waysx);
}


/*++++++++++++++++++++++++++++++++++++++
  Free a way list.

  WaysX *waysx The set of ways to be freed.

  int keep If set then the results file is to be kept.
  ++++++++++++++++++++++++++++++++++++++*/

void FreeWayList(WaysX *waysx,int keep)
{
 if(keep)
    RenameFile(waysx->filename_tmp,waysx->filename);
 else
    DeleteFile(waysx->filename_tmp);

 free(waysx->filename);
 free(waysx->filename_tmp);

 if(waysx->idata)
    free(waysx->idata);

 if(waysx->cdata)
    free(waysx->cdata);

 DeleteFile(waysx->nfilename_tmp);

 free(waysx->nfilename_tmp);

 free(waysx);
}


/*++++++++++++++++++++++++++++++++++++++
  Append a single way to an unsorted way list.

  WaysX *waysx The set of ways to process.

  way_t id The ID of the way.

  Way *way The way data itself.

  const char *name The name or reference of the way.
  ++++++++++++++++++++++++++++++++++++++*/

void AppendWayList(WaysX *waysx,way_t id,Way *way,const char *name)
{
 WayX wayx;
 FILESORT_VARINT size;

 wayx.id=id;
 wayx.way=*way;

 size=sizeof(WayX)+strlen(name)+1;

 WriteFile(waysx->fd,&size,FILESORT_VARSIZE);
 WriteFile(waysx->fd,&wayx,sizeof(WayX));
 WriteFile(waysx->fd,name,strlen(name)+1);

 waysx->number++;

 logassert(waysx->number!=0,"Too many ways (change index_t to 64-bits?)"); /* Zero marks the high-water mark for ways. */
}


/*++++++++++++++++++++++++++++++++++++++
  Finish appending ways and change the filename over.

  WaysX *waysx The ways that have been appended.
  ++++++++++++++++++++++++++++++++++++++*/

void FinishWayList(WaysX *waysx)
{
 if(waysx->fd!=-1)
    waysx->fd=CloseFile(waysx->fd);
}


/*++++++++++++++++++++++++++++++++++++++
  Find a particular way index.

  index_t IndexWayX Returns the index of the extended way with the specified id.

  WaysX *waysx The set of ways to process.

  way_t id The way id to look for.
  ++++++++++++++++++++++++++++++++++++++*/

index_t IndexWayX(WaysX *waysx,way_t id)
{
 index_t start=0;
 index_t end=waysx->number-1;
 index_t mid;

 if(waysx->number==0)           /* There are no ways */
    return(NO_WAY);

 if(id<waysx->idata[start])     /* Key is before start */
    return(NO_WAY);

 if(id>waysx->idata[end])       /* Key is after end */
    return(NO_WAY);

 /* Binary search - search key exact match only is required.
  *
  *  # <- start  |  Check mid and move start or end if it doesn't match
  *  #           |
  *  #           |  Since an exact match is wanted we can set end=mid-1
  *  # <- mid    |  or start=mid+1 because we know that mid doesn't match.
  *  #           |
  *  #           |  Eventually either end=start or end=start+1 and one of
  *  # <- end    |  start or end is the wanted one.
  */

 do
   {
    mid=(start+end)/2;            /* Choose mid point */

    if(waysx->idata[mid]<id)      /* Mid point is too low */
       start=mid+1;
    else if(waysx->idata[mid]>id) /* Mid point is too high */
       end=mid?(mid-1):mid;
    else                          /* Mid point is correct */
       return(mid);
   }
 while((end-start)>1);

 if(waysx->idata[start]==id)      /* Start is correct */
    return(start);

 if(waysx->idata[end]==id)        /* End is correct */
    return(end);

 return(NO_WAY);
}


/*++++++++++++++++++++++++++++++++++++++
  Sort the list of ways.

  WaysX *waysx The set of ways to process.
  ++++++++++++++++++++++++++++++++++++++*/

void SortWayList(WaysX *waysx)
{
 index_t xnumber;
 int fd;

 /* Print the start message */

 printf_first("Sorting Ways");

 /* Re-open the file read-only and a new file writeable */

 waysx->fd=ReOpenFile(waysx->filename_tmp);

 DeleteFile(waysx->filename_tmp);

 fd=OpenFileNew(waysx->filename_tmp);

 /* Sort the ways by ID and index them */

 xnumber=waysx->number;

 waysx->number=filesort_vary(waysx->fd,fd,NULL,
                                          (int (*)(const void*,const void*))sort_by_id,
                                          (int (*)(void*,index_t))deduplicate_by_id);

 /* Close the files */

 waysx->fd=CloseFile(waysx->fd);
 CloseFile(fd);

 /* Print the final message */

 printf_last("Sorted Ways: Ways=%"Pindex_t" Duplicates=%"Pindex_t,xnumber,xnumber-waysx->number);
}


/*++++++++++++++++++++++++++++++++++++++
  Sort the ways into id order.

  int sort_by_id Returns the comparison of the id fields.

  WayX *a The first extended way.

  WayX *b The second extended way.
  ++++++++++++++++++++++++++++++++++++++*/

static int sort_by_id(WayX *a,WayX *b)
{
 way_t a_id=a->id;
 way_t b_id=b->id;

 if(a_id<b_id)
    return(-1);
 else if(a_id>b_id)
    return(1);
 else
    return(-FILESORT_PRESERVE_ORDER(a,b)); /* latest version first */
}


/*++++++++++++++++++++++++++++++++++++++
  Discard duplicate ways.

  int deduplicate_by_id Return 1 if the value is to be kept, otherwise 0.

  WayX *wayx The extended way.

  index_t index The number of sorted ways that have already been written to the output file.
  ++++++++++++++++++++++++++++++++++++++*/

static int deduplicate_by_id(WayX *wayx,index_t index)
{
 static way_t previd=NO_WAY_ID;

 if(wayx->id!=previd)
   {
    previd=wayx->id;

    if(wayx->way.type==WAY_DELETED)
       return(0);
    else
       return(1);
   }
 else
    return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Extract the way names from the ways and reference the list of names from the ways.

  WaysX *waysx The set of ways to process.

  int keep If set to 1 then keep the old data file otherwise delete it.
  ++++++++++++++++++++++++++++++++++++++*/

void ExtractWayNames(WaysX *waysx,int keep)
{
 index_t i;
 int fd;
 char *names[2]={NULL,NULL};
 int namelen[2]={0,0};
 int nnames=0;
 uint32_t lastlength=0;

 /* Print the start message */

 printf_first("Sorting Ways by Name");

 /* Re-open the file read-only and a new file writeable */

 waysx->fd=ReOpenFile(waysx->filename_tmp);

 if(keep)
    RenameFile(waysx->filename_tmp,waysx->filename);
 else
    DeleteFile(waysx->filename_tmp);

 fd=OpenFileNew(waysx->filename_tmp);

 /* Sort the ways to allow separating the names */

 filesort_vary(waysx->fd,fd,NULL,
                            (int (*)(const void*,const void*))sort_by_name,
                            NULL);

 /* Close the files */

 waysx->fd=CloseFile(waysx->fd);
 CloseFile(fd);

 /* Print the final message */

 printf_last("Sorted Ways by Name: Ways=%"Pindex_t,waysx->number);


 /* Print the start message */

 printf_first("Separating Way Names: Ways=0 Names=0");

 /* Re-open the file read-only and new files writeable */

 waysx->fd=ReOpenFile(waysx->filename_tmp);

 DeleteFile(waysx->filename_tmp);

 fd=OpenFileNew(waysx->filename_tmp);

 waysx->nfd=OpenFileNew(waysx->nfilename_tmp);

 /* Copy from the single file into two files */

 for(i=0;i<waysx->number;i++)
   {
    WayX wayx;
    FILESORT_VARINT size;

    ReadFile(waysx->fd,&size,FILESORT_VARSIZE);

    if(namelen[nnames%2]<size)
       names[nnames%2]=(char*)realloc((void*)names[nnames%2],namelen[nnames%2]=size);

    ReadFile(waysx->fd,&wayx,sizeof(WayX));
    ReadFile(waysx->fd,names[nnames%2],size-sizeof(WayX));

    if(nnames==0 || strcmp(names[0],names[1]))
      {
       WriteFile(waysx->nfd,names[nnames%2],size-sizeof(WayX));

       lastlength=waysx->nlength;
       waysx->nlength+=size-sizeof(WayX);

       nnames++;
      }

    wayx.way.name=lastlength;

    WriteFile(fd,&wayx,sizeof(WayX));

    if(!((i+1)%1000))
       printf_middle("Separating Way Names: Ways=%"Pindex_t" Names=%"Pindex_t,i+1,nnames);
   }

 if(names[0]) free(names[0]);
 if(names[1]) free(names[1]);

 /* Close the files */

 waysx->fd=CloseFile(waysx->fd);
 CloseFile(fd);

 waysx->nfd=CloseFile(waysx->nfd);

 /* Print the final message */

 printf_last("Separated Way Names: Ways=%"Pindex_t" Names=%"Pindex_t,waysx->number,nnames);


 /* Print the start message */

 printf_first("Sorting Ways");

 /* Re-open the file read-only and a new file writeable */

 waysx->fd=ReOpenFile(waysx->filename_tmp);

 DeleteFile(waysx->filename_tmp);

 fd=OpenFileNew(waysx->filename_tmp);

 /* Allocate the array of indexes */

 waysx->idata=(way_t*)malloc(waysx->number*sizeof(way_t));

 logassert(waysx->idata,"Failed to allocate memory (try using slim mode?)"); /* Check malloc() worked */

 /* Sort the ways by ID */

 sortwaysx=waysx;

 filesort_fixed(waysx->fd,fd,sizeof(WayX),NULL,
                                          (int (*)(const void*,const void*))sort_by_id,
                                          (int (*)(void*,index_t))index_by_id);

 /* Close the files */

 waysx->fd=CloseFile(waysx->fd);
 CloseFile(fd);

 /* Print the final message */

 printf_last("Sorted Ways: Ways=%"Pindex_t,waysx->number);
}


/*++++++++++++++++++++++++++++++++++++++
  Sort the ways into name order and then id order.

  int sort_by_name Returns the comparison of the name fields.

  WayX *a The first extended Way.

  WayX *b The second extended Way.
  ++++++++++++++++++++++++++++++++++++++*/

static int sort_by_name(WayX *a,WayX *b)
{
 int compare;
 char *a_name=(char*)a+sizeof(WayX);
 char *b_name=(char*)b+sizeof(WayX);

 compare=strcmp(a_name,b_name);

 if(compare)
    return(compare);
 else
    return(FILESORT_PRESERVE_ORDER(a,b));
}


/*++++++++++++++++++++++++++++++++++++++
  Create the index of identifiers.

  int index_by_id Return 1 if the value is to be kept, otherwise 0.

  WayX *wayx The extended way.

  index_t index The number of sorted ways that have already been written to the output file.
  ++++++++++++++++++++++++++++++++++++++*/

static int index_by_id(WayX *wayx,index_t index)
{
 sortwaysx->idata[index]=wayx->id;

 return(1);
}


/*++++++++++++++++++++++++++++++++++++++
  Compact the way list, removing duplicated ways and unused ways.

  WaysX *waysx The set of ways to process.

  SegmentsX *segmentsx The set of segments to check.
  ++++++++++++++++++++++++++++++++++++++*/

void CompactWayList(WaysX *waysx,SegmentsX *segmentsx)
{
 int fd;
 index_t cnumber;

 if(waysx->number==0)
    return;

 /* Print the start message */

 printf_first("Sorting Ways and Compacting");

 /* Allocate the array of indexes */

 waysx->cdata=(index_t*)malloc(waysx->number*sizeof(index_t));

 logassert(waysx->cdata,"Failed to allocate memory (try using slim mode?)"); /* Check malloc() worked */

 /* Re-open the file read-only and a new file writeable */

 waysx->fd=ReOpenFile(waysx->filename_tmp);

 DeleteFile(waysx->filename_tmp);

 fd=OpenFileNew(waysx->filename_tmp);

 /* Sort the ways to allow compacting according to the properties */

 sortwaysx=waysx;
 sortsegmentsx=segmentsx;

 cnumber=filesort_fixed(waysx->fd,fd,sizeof(WayX),(int (*)(void*,index_t))delete_unused,
                                                  (int (*)(const void*,const void*))sort_by_name_and_prop_and_id,
                                                  (int (*)(void*,index_t))deduplicate_and_index_by_compact_id);

 /* Close the files */

 waysx->fd=CloseFile(waysx->fd);
 CloseFile(fd);

 /* Print the final message */

 printf_last("Sorted and Compacted Ways: Ways=%"Pindex_t" Unique=%"Pindex_t,waysx->number,cnumber);
 waysx->number=cnumber;

 free(segmentsx->usedway);
 segmentsx->usedway=NULL;
}


/*++++++++++++++++++++++++++++++++++++++
  Delete the ways that are no longer being used.

  int delete_unused Return 1 if the value is to be kept, otherwise 0.

  WayX *wayx The extended way.

  index_t index The number of unsorted ways that have been read from the input file.
  ++++++++++++++++++++++++++++++++++++++*/

static int delete_unused(WayX *wayx,index_t index)
{
 if(sortsegmentsx && !IsBitSet(sortsegmentsx->usedway,index))
   {
    sortwaysx->cdata[index]=NO_WAY;

    return(0);
   }
 else
   {
    wayx->id=index;

    return(1);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Sort the ways into name, properties and id order.

  int sort_by_name_and_prop_and_id Returns the comparison of the name, properties and id fields.

  WayX *a The first extended Way.

  WayX *b The second extended Way.
  ++++++++++++++++++++++++++++++++++++++*/

static int sort_by_name_and_prop_and_id(WayX *a,WayX *b)
{
 int compare;
 index_t a_name=a->way.name;
 index_t b_name=b->way.name;

 if(a_name<b_name)
    return(-1);
 else if(a_name>b_name)
    return(1);

 compare=WaysCompare(&a->way,&b->way);

 if(compare)
    return(compare);

 return(sort_by_id(a,b));
}


/*++++++++++++++++++++++++++++++++++++++
  Create the index of compacted Way identifiers and ignore Ways with duplicated properties.

  int deduplicate_and_index_by_compact_id Return 1 if the value is to be kept, otherwise 0.

  WayX *wayx The extended way.

  index_t index The number of sorted ways that have already been written to the output file.
  ++++++++++++++++++++++++++++++++++++++*/

static int deduplicate_and_index_by_compact_id(WayX *wayx,index_t index)
{
 static Way lastway;

 if(index==0 || wayx->way.name!=lastway.name || WaysCompare(&lastway,&wayx->way))
   {
    lastway=wayx->way;

    sortwaysx->cdata[wayx->id]=index;

    wayx->id=index;

    return(1);
   }
 else
   {
    sortwaysx->cdata[wayx->id]=index-1;

    return(0);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Save the way list to a file.

  WaysX *waysx The set of ways to save.

  const char *filename The name of the file to save.
  ++++++++++++++++++++++++++++++++++++++*/

void SaveWayList(WaysX *waysx,const char *filename)
{
 index_t i;
 int fd;
 int position=0;
 WayX wayx;
 WaysFile waysfile={0};
 highways_t   highways=0;
 transports_t allow=0;
 properties_t props=0;

 /* Print the start message */

 printf_first("Writing Ways: Ways=0");

 /* Re-open the files */

 waysx->fd=ReOpenFile(waysx->filename_tmp);
 waysx->nfd=ReOpenFile(waysx->nfilename_tmp);

 /* Write out the ways data */

 fd=OpenFileNew(filename);

 SeekFile(fd,sizeof(WaysFile));

 for(i=0;i<waysx->number;i++)
   {
    ReadFile(waysx->fd,&wayx,sizeof(WayX));

    highways|=HIGHWAYS(wayx.way.type);
    allow   |=wayx.way.allow;
    props   |=wayx.way.props;

    WriteFile(fd,&wayx.way,sizeof(Way));

    if(!((i+1)%1000))
       printf_middle("Writing Ways: Ways=%"Pindex_t,i+1);
   }

 /* Write out the ways names */

 SeekFile(fd,sizeof(WaysFile)+(off_t)waysx->number*sizeof(Way));

 while(position<waysx->nlength)
   {
    int len=1024;
    char temp[1024];

    if((waysx->nlength-position)<1024)
       len=waysx->nlength-position;

    ReadFile(waysx->nfd,temp,len);

    WriteFile(fd,temp,len);

    position+=len;
   }

 /* Close the files */

 waysx->fd=CloseFile(waysx->fd);
 waysx->nfd=CloseFile(waysx->nfd);

 /* Write out the header structure */

 waysfile.number =waysx->number;

 waysfile.highways=highways;
 waysfile.allow   =allow;
 waysfile.props   =props;

 SeekFile(fd,0);
 WriteFile(fd,&waysfile,sizeof(WaysFile));

 CloseFile(fd);

 /* Print the final message */

 printf_last("Wrote Ways: Ways=%"Pindex_t,waysx->number);
}
