/***************************************
 Header file for sorting function prototypes

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


#ifndef SORTING_H
#define SORTING_H    /*+ To stop multiple inclusions. +*/

#include <sys/types.h>

#include "types.h"


/* Constants */

/*+ The type, size and alignment of variable to store the variable length +*/
#define FILESORT_VARINT   unsigned short
#define FILESORT_VARSIZE  sizeof(FILESORT_VARINT)
#define FILESORT_VARALIGN sizeof(void*)


/* Macros */

/*+ A macro to use as a last resort in the comparison function to preserve
    on the output the input order of items that compare equally. +*/
#define FILESORT_PRESERVE_ORDER(a,b) ( ((a)<(b)) ? -1 : +1)


/* Functions in sorting.c */

index_t filesort_fixed(int fd_in,int fd_out,size_t itemsize,int (*pre_sort_function)(void*,index_t),
                                                            int (*compare_function)(const void*,const void*),
                                                            int (*post_sort_function)(void*,index_t));

index_t filesort_vary(int fd_in,int fd_out,int (*pre_sort_function)(void*,index_t),
                                           int (*compare_function)(const void*,const void*),
                                           int (*post_sort_function)(void*,index_t));

void filesort_heapsort(void **datap,size_t nitems,int(*compare)(const void*, const void*));


#endif /* SORTING_H */
