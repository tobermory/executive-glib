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

#include <pthread.h>

/**
 * @author Stuart Maclean
 *
 * The 'foobar' problem solved via use of pthreads. 
 *
 * See ./foobar-executive.c for the problem statement. 
 *
 * @see https://man7.org/linux/man-pages/man3/pthread_create.3.html
 *
 * I defy you to reason about what happens at time 5 * 7 = 35. You
 * might get 'foo+bar' or you might get 'bar+foo'. Threads are evil,
 * avoid them like the plague.
 */

static void* foo( void* unused ) {
  while( 2 ) {
	sleep( 5 );
	printf( "foo!\n" );
  }
  return NULL;
}

static void* bar( void* unused ) {
  while( 2 ) {
	sleep( 7 );
	printf( "bar!\n" );
  }
  return NULL;
}

static volatile bool done = false;

static void* end( void* unused ) {
  sleep( 60 );
  done = true;
  return NULL;
}

int main(void ) {

  pthread_t tFoo;
  pthread_t tBar;
  pthread_t tEnd;

  int sc;

  sc = pthread_create( &tFoo, NULL, foo, NULL );
  if( sc )
	return -1;
  
  sc = pthread_create( &tBar, NULL, bar, NULL );
  if( sc )
	return -1;
  
  sc = pthread_create( &tEnd, NULL, end, NULL );
  if( sc )
	return -1;

  /* 
	 This is flawed. Even though the 'end' thread has set done, we are
	 blocked on a stdin read. We REQUIRE a read AFTER end time before
	 we can end!  

	 We could solve this by calling exit() in the 'end' thread itself,
	 but how yuk is that! As I said, threads are evil.
  */
  while( !done ) {
	char input[128];
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
  
  return 0;
}

// eof
