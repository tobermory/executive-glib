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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>

#include "executive/executive.h"

/**
 * @author Stuart Maclean
 *
 * For one minute, read lines of text from the user at a keyboard.
 * For each line L received, print the line length.  Concurrently with
 * servicing this user I/O, print 'foo' every 5 seconds and 'bar'
 * every 7. Exit gracefully after the one minute.
 *
 * See also ./foobar-executive-env.c for a version of this that uses simple
 * 'environment' objects that Executive event actions manipulate.
 * This can reduce need for globals, and concentrates the main program
 * logic more in the main executive/select loop.
 *
 * @see ./foobar-executive.c
 */

static void execActionFoo( Event* e, struct timeval* actualTime ) {

  printf( "foo!\n" );
  
  /*
	Next instance of same event can be based on ACTUAL time of this
	instance firing, or SCHEDULED time, up to us.
  */
  struct timeval* tv = true ? executiveEventScheduledTime(e) : actualTime;
  struct timeval fooTime = { 5, 0 };
  struct timeval nextTime;
  timeradd( tv, &fooTime, &nextTime );
  executiveAdd( executiveEventExecutive(e), &nextTime, execActionFoo );
}

static void execActionBar( Event* e, struct timeval* actualTime ) {

  printf( "bar!\n" );

  /*
	See execActionFoo ...
  */
  struct timeval* tv = true ? executiveEventScheduledTime(e) : actualTime;
  struct timeval barTime = { 7, 0 };
  struct timeval nextTime;
  timeradd( tv, &barTime, &nextTime );
  executiveAdd( executiveEventExecutive(e), &nextTime, execActionBar );
}

/* Globals ?  Pah! See ./foobar2.c for a better way! */
static bool done = false;

static void execActionDone( Event* e, struct timeval* actualTime ) {
  (void)e;
  (void)actualTime;

  done = true;
}

int main(void) {

  Executive* E = executiveNew();

  struct timeval now;
  gettimeofday( &now, NULL );

  /* Program end is 60 seconds from now, post it to the Executive */
  struct timeval run = { 60, 0 };
  struct timeval end;
  timeradd( &now, &run, &end );
  executiveAdd( E, &end, execActionDone );

  /* First foo action 5 secs from now, post it */
  struct timeval fooTime = { 5, 0 };
  timeradd( &now, &fooTime, &end );
  executiveAdd( E, &end, execActionFoo );

  /* First bar action 7 secs from now, post it */
  struct timeval barTime = { 7, 0 };
  timeradd( &now, &barTime, &end );
  executiveAdd( E, &end, execActionBar );

  /* 
	 Could be reading from N file descriptors: sockets, pipes, ttys,
	 in fact anything that is selectable.  

	 Here we are just interested in stdin, likely the keyboard if this
	 program is run from a terminal, as we expect.
  */
  fd_set fds;
  FD_ZERO( &fds );
  FD_SET( STDIN_FILENO, &fds );

  /* The max of ALL fds of interest */
  int fdMax = STDIN_FILENO;

  /* 
	 We call this the 'Executive loop'. It combines an Executive
	 object with the select() and read() system calls to process input
	 data from any number of fds, while at the same time paying
	 attention to Executive events becoming ready.
  */
  while( !done ) {

	/* Step 1: peek at earliest Executive event time */
    gettimeofday( &now, NULL );
    struct timeval *head = executivePeek(E);

	/* Step 2a: Earliest event is in the past, fire it! */
	if( timercmp( head, &now, < ) ) {
      executiveFire( E, &now );
      continue;
	}	  

	/* 
	   Step 2b: Earliest event time T is in the future, compute the
	   delta from now to T, and select on all fds for that delta. If
	   the select returns with no I/O ready, then it is time T and the
	   'time fd' that is the Executive is ready!
	*/
	struct timeval wait;
    timersub( head, &now, &wait );
	
	fd_set work = fds;
    int ready = select( fdMax + 1, &work, NULL, NULL, &wait );
    if( ready == -1 ) {
	  // ?? check for EINTR, maybe continue ??
      break; 
    }
	
    if( ready == 0 ) {
      // fire the executive head Event, its time has come...
      gettimeofday( &now, NULL );
      executiveFire( E, &now );
      continue;
    }

	/* 
	   Step 3: Process any input from ready fds. Check each one in turn.
	
	   In this application, there is just ONE fd of interest.  So, if
	   ready == 1, which it must now be since we have checked for
	   ready < 1, it MUST be STDIN that has data to read. Thus the
	   FD_ISSET call below is unnecessary. In a larger app with MANY
	   fds however, we WOULD need a call to FD_ISSET for each fd,
	   since > 1 could be ready.
	*/
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
	
  }
  
  executiveFree( E );
  return 0;
}

// eof

				 
