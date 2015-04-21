/***************************************
 File uncompression.

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


#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#if defined(USE_BZIP2) && USE_BZIP2
#define BZ_NO_STDIO
#include <bzlib.h>
#endif

#if defined(USE_GZIP) && USE_GZIP
#include <zlib.h>
#endif

#include "logging.h"
#include "uncompress.h"


/* Local functions */

static int pipe_and_fork(int filefd,int *pipefd);

#if defined(USE_BZIP2) && USE_BZIP2
static void uncompress_bzip2_pipe(int filefd,int pipefd);
#endif

#if defined(USE_GZIP) && USE_GZIP
static void uncompress_gzip_pipe(int filefd,int pipefd);
#endif


/*++++++++++++++++++++++++++++++++++++++
  Create a child process to uncompress data on a file descriptor as if it were a pipe.

  int Uncompress_Bzip2 Returns the file descriptor of the uncompressed end of the pipe.

  int filefd The file descriptor of the compressed end of the pipe.
  ++++++++++++++++++++++++++++++++++++++*/

int Uncompress_Bzip2(int filefd)
{
#if defined(USE_BZIP2) && USE_BZIP2

 int pipefd=-1;

 if(pipe_and_fork(filefd,&pipefd))
    return(pipefd);

 uncompress_bzip2_pipe(filefd,pipefd);

 exit(EXIT_SUCCESS);

#else /* USE_BZIP2 */

 logassert(0,"No bzip2 compression support available (re-compile and try again)");

 return(0);

#endif /* USE_BZIP2 */
}


/*++++++++++++++++++++++++++++++++++++++
  Create a child process to uncompress data on a file descriptor as if it were a pipe.

  int Uncompress_Gzip Returns the file descriptor of the uncompressed end of the pipe.

  int filefd The file descriptor of the compressed end of the pipe.
  ++++++++++++++++++++++++++++++++++++++*/

int Uncompress_Gzip(int filefd)
{
#if defined(USE_GZIP) && USE_GZIP

 int pipefd=-1;

 if(pipe_and_fork(filefd,&pipefd))
    return(pipefd);

 uncompress_gzip_pipe(filefd,pipefd);

 exit(EXIT_SUCCESS);

#else /* USE_GZIP */

 logassert(0,"No gzip compression support available (re-compile and try again)");

 return(0);

#endif /* USE_GZIP */
}


/*++++++++++++++++++++++++++++++++++++++
  Create a pipe and then fork returning in the parent and child with a different end of the pipe.

  int pipe_and_fork Returns 1 for the reading (parent) end of the pipe and 0 for the writing (child) end.

  int filefd The file descriptor of the file.

  int *pipefd Returns the file descriptor for the end of the pipe.
  ++++++++++++++++++++++++++++++++++++++*/

static int pipe_and_fork(int filefd,int *pipefd)
{
 int pipe_fd[2]={-1,-1};
 pid_t childpid;

#define PIPE_READER 0
#define PIPE_WRITER 1

 if(pipe(pipe_fd))
   {
    logassert(0,"Cannot create pipe for uncompressor (try without using a compressed file)");
    return(1);
   }

 if((childpid=fork()) == -1)
   {
    logassert(0,"Cannot create new process for uncompressor (try without using a compressed file)");
    return(1);
   }

 if(childpid==0)   /* The child */
   {
    int i;

    *pipefd=pipe_fd[PIPE_WRITER];

    /* Close all unneeded file descriptors */

    for(i=0;i<255;i++)
       if(i!=filefd && i!=*pipefd)
          close(i);

    return(0);    
   }
 else   /* The parent */
   {
    struct sigaction action;

    *pipefd=pipe_fd[PIPE_READER];

    /* Close all unneeded file descriptors */

    close(pipe_fd[PIPE_WRITER]);
    close(filefd);

    /* Ignore child exiting and pipe signals */

    /* SIGCHLD */
    action.sa_handler=SIG_IGN;
    sigemptyset(&action.sa_mask);
    action.sa_flags=0;
    sigaction(SIGCHLD,&action,NULL);

    /* SIGPIPE */
    action.sa_handler=SIG_IGN;
    sigemptyset(&action.sa_mask);
    action.sa_flags=0;
    sigaction(SIGPIPE,&action,NULL);

    return(1);
   }
}


#if defined(USE_BZIP2) && USE_BZIP2

/*++++++++++++++++++++++++++++++++++++++
  Uncompress a file using bzip2 as a pipeline.

  int filefd The incoming, compressed, data.

  int pipefd The outgoing, uncompressed, data.
  ++++++++++++++++++++++++++++++++++++++*/

static void uncompress_bzip2_pipe(int filefd,int pipefd)
{
 bz_stream bz={0};
 char inbuffer[16384],outbuffer[16384];
 int infinished=0;
 int state;

 if(BZ2_bzDecompressInit(&bz,0,0)!=BZ_OK)
    exit(EXIT_FAILURE);

 do
   {
    if(bz.avail_in==0 && !infinished)
      {
       ssize_t n=read(filefd,inbuffer,sizeof(inbuffer));

       if(n<=0)
          infinished=1;
       else
         {
          bz.next_in=inbuffer;
          bz.avail_in=n;
         }
      }

    bz.next_out=outbuffer;
    bz.avail_out=sizeof(outbuffer);

    state=BZ2_bzDecompress(&bz);

    if(state!=BZ_OK && state!=BZ_STREAM_END)
       exit(EXIT_FAILURE);

    if(bz.avail_out<sizeof(outbuffer))
      {
       char *p;
       ssize_t m,n;

       p=outbuffer;
       n=sizeof(outbuffer)-bz.avail_out;

       while(n>0)
         {
          m=write(pipefd,p,n);

          if(m<=0)
             exit(EXIT_FAILURE);

          p+=m;
          n-=m;
         }
      }
   }
 while(state!=BZ_STREAM_END);

 if(BZ2_bzDecompressEnd(&bz)!=BZ_OK)
    exit(EXIT_FAILURE);

 exit(EXIT_SUCCESS);
}

#endif /* USE_BZIP2 */


#if defined(USE_GZIP) && USE_GZIP

/*++++++++++++++++++++++++++++++++++++++
  Uncompress a file using gzip as a pipeline.

  int filefd The incoming, compressed, data.

  int pipefd The outgoing, uncompressed, data.
  ++++++++++++++++++++++++++++++++++++++*/

static void uncompress_gzip_pipe(int filefd,int pipefd)
{
 z_stream z={0};
 unsigned char inbuffer[16384],outbuffer[16384];
 int infinished=0;
 int state;

 if(inflateInit2(&z,15+32)!=Z_OK)
    exit(EXIT_FAILURE);

 do
   {
    if(z.avail_in==0 && !infinished)
      {
       ssize_t n=read(filefd,inbuffer,sizeof(inbuffer));

       if(n<=0)
          infinished=1;
       else
         {
          z.next_in=inbuffer;
          z.avail_in=n;
         }
      }

    z.next_out=outbuffer;
    z.avail_out=sizeof(outbuffer);

    state=inflate(&z,Z_NO_FLUSH);

    if(state!=Z_OK && state!=Z_STREAM_END)
      {
       exit(EXIT_FAILURE);
      }

    if(z.avail_out<sizeof(outbuffer))
      {
       unsigned char *p;
       ssize_t n,m;

       p=outbuffer;
       n=sizeof(outbuffer)-z.avail_out;

       while(n>0)
         {
          m=write(pipefd,p,n);

          if(m<=0)
             exit(EXIT_FAILURE);

          p+=m;
          n-=m;
         }
      }
   }
 while(state!=Z_STREAM_END);

 if(inflateEnd(&z)!=Z_OK)
    exit(EXIT_FAILURE);

 exit(EXIT_SUCCESS);
}

#endif /* USE_GZIP */
