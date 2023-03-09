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
 * As per ./foobar.c, but here we make use of some simple
 * 'environment' objects that Executive event actions can manipulate.
 * With this approach, the actions simply signal, via boolean
 * 'environments', some program state update. The main loop notices
 * these, and acts.
 *
 * @see ./foobar.c
 */

static void execActionFoo( Event* e, struct timeval* actualTime ) {

  bool* pFoo = executiveEventEnv( e );
  *pFoo = true;

  /*
	Next instance of same event can be based on ACTUAL time of this
	instance firing, or SCHEDULED time, up to us.
  */
  struct timeval* tv = true ? executiveEventScheduledTime(e) : actualTime;
  struct timeval fooTime = { 5, 0 };
  struct timeval nextTime;
  timeradd( tv, &fooTime, &nextTime );
  executiveAddWithEnv( executiveEventExecutive(e), &nextTime,
					   execActionFoo, pFoo );
}

static void execActionBar( Event* e, struct timeval* actualTime ) {

  bool* pBar = executiveEventEnv( e );
  *pBar = true;

  /*
	See execActionFoo ...
  */
  struct timeval* tv = true ? executiveEventScheduledTime(e) : actualTime;
  struct timeval barTime = { 7, 0 };
  struct timeval nextTime;
  timeradd( tv, &barTime, &nextTime );
  executiveAddWithEnv( executiveEventExecutive(e), &nextTime,
					   execActionBar, pBar );
}

static void execActionDone( Event* e, struct timeval* actualTime ) {
  (void)actualTime;

  bool* pDone = executiveEventEnv( e );
  *pDone = true;
}


int main(void) {

  Executive* E = executiveNew();

  bool done = false;

  struct timeval now;
  gettimeofday( &now, NULL );

  struct timeval run = { 60, 0 };
  struct timeval end;
  timeradd( &now, &run, &end );
  executiveAddWithEnv( E, &end, execActionDone, &done );

  bool foo = false;
  bool bar = false;

  struct timeval fooTime = { 5, 0 };
  timeradd( &now, &fooTime, &end );
  executiveAddWithEnv( E, &end, execActionFoo, &foo );

  struct timeval barTime = { 7, 0 };
  timeradd( &now, &barTime, &end );
  executiveAddWithEnv( E, &end, execActionBar, &bar );

  
  fd_set fds;
  FD_ZERO( &fds );
  FD_SET( STDIN_FILENO, &fds );
  int fdMax = STDIN_FILENO;
  
  while( !done ) {

	if( foo ) {
	  printf( "foo!\n" );
	  foo = false;
	}

	if( bar ) {
	  printf( "bar!\n" );
	  bar = false;
	}

    struct timeval now;
    gettimeofday( &now, NULL );
    struct timeval *head = executivePeek(E);

	// Earliest event is in the past, fire it!
	if( timercmp( head, &now, < ) ) {
      executiveFire( E, &now );
      continue;
	}	  

	// Earliest event is in the future, select on all fds until that time
	struct timeval wait;
    timersub( head, &now, &wait );
	
	fd_set work = fds;
    int ready = select( fdMax + 1, &work, NULL, NULL, &wait );
    if( ready == -1 ) {
	  // ?? check for EINTR ??
      break; 
    }
	
    if( ready == 0 ) {
      // fire the executive head Event, its time has come...
      gettimeofday( &now, NULL );
      executiveFire( E, &now );
      continue;
    }

    if( FD_ISSET( STDIN_FILENO, &work ) ) {
      char input[1024];
      int nin = read( STDIN_FILENO, input, sizeof(input) - 1 );
      if( nin < 1 )
        break;
	  // There IS room for the NULL, we read one less than capacity
	  input[nin] = 0;

	  int lenInput = strlen( input );
	  printf( "%d\n", lenInput );
	}
	
  }
  
  executiveFree( E );
  return 0;
}

// eof

				 
