/**
 * Copyright © 2023 Stuart Maclean
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER NOR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/timerfd.h>

/**
 * @author Stuart Maclean
 *
 * The 'foobar' problem solved via use of the Linux timerfd API. This
 * program is thus Linux-specific.  It will not work (build!) on other
 * Unices.
 *
 * See ./foobar-executive.c for the problem statement. 
 *
 * @see https://man7.org/linux/man-pages/man2/timerfd_create.2.html
 */

int main(void ) {

  /* Set up printing 'foo' every 5 secs */
  struct timespec foo = { 5, 0 };
  struct itimerspec fooI = { foo, foo };
  int fdFoo = timerfd_create( CLOCK_REALTIME, 0 );
  timerfd_settime( fdFoo, 0, &fooI, NULL );
  
  /* Set up printing 'bar' every 7 secs */
  struct timespec bar = { 7, 0 };
  struct itimerspec barI = { bar, bar };
  int fdBar = timerfd_create( CLOCK_REALTIME, 0 );
  timerfd_settime( fdBar, 0, &barI, NULL );
  
  /* Set up ending the main loop after 1 minute */
  struct timespec end = { 60, 0 };
  struct itimerspec endI = { { 0, 0 } , end };
  int fdEnd = timerfd_create( CLOCK_REALTIME, 0 );
  timerfd_settime( fdEnd, 0, &endI, NULL );

  /* The fds of interest: stdin and the three timers */
  fd_set fds;
  FD_ZERO( &fds );
  FD_SET( STDIN_FILENO, &fds );
  FD_SET( fdFoo, &fds );
  FD_SET( fdBar, &fds );
  FD_SET( fdEnd, &fds );

  /* The max of ALL fds of interest. Biggest will be last created */
  int fdMax = fdEnd;

  bool done = false;
  
  while( !done ) {

	fd_set work = fds;
	int ready = select( fdMax + 1, &work, NULL, NULL, NULL );
    if( ready == -1 ) {
	  // ?? check for EINTR, maybe continue ??
      break; 
    }

	/* For user input, just print out line length entered */
    if( FD_ISSET( STDIN_FILENO, &work ) ) {
      char input[1024];
      int nin = read( STDIN_FILENO, input, sizeof(input) - 1 );

      /* User eof, via Ctrl-D ? */
	  if( nin < 1 )
        break;

	  /* There IS room for the NULL, we read at most one less than capacity */
	  input[nin] = 0;

	  /* WLOG we'll include the \r in the char count */
	  int lenInput = strlen( input );
	  printf( "%d\n", lenInput );
	}

	/* foo time */
    if( FD_ISSET( fdFoo, &work ) ) {
	  char time[8];
      int nin = read( fdFoo, time, sizeof(time) );
	  (void)nin;
	  printf( "foo!\n" );
	}
	  
	/* bar time */
    if( FD_ISSET( fdBar, &work ) ) {
	  char time[8];
      int nin = read( fdBar, time, sizeof(time) );
	  (void)nin;
	  printf( "bar!\n" );
	}
	  
	/* time to bail */
    if( FD_ISSET( fdEnd, &work ) ) {
	  char time[8];
      int nin = read( fdEnd, time, sizeof(time) );
	  (void)nin;
	  done = true;
	}
  }
  
  return 0;
}

// eof
