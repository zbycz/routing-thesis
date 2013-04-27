/***************************************
 Function prototypes for file uncompression.

 Part of the Routino routing software.
 ******************/ /******************
 This file Copyright 2012 Andrew M. Bishop

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


#ifndef UNCOMPRESS_H
#define UNCOMPRESS_H    /*+ To stop multiple inclusions. +*/

int Uncompress_Bzip2(int filefd);

int Uncompress_Gzip(int filefd);

#endif /* UNCOMPRESS_H */
