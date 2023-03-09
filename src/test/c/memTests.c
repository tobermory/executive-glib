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
#include <assert.h>
#include <stdlib.h>

#include "executive/executive.h"

/**
 * Run with valgrind and look for memory leaks:
 *
 * $ valgrind ./memTests
 */

static void someExecAction( Event* e, struct timeval* actualTime ) {
  (void)e;
  (void)actualTime;
}

static void test1(void) {
  Executive* e = executiveNew();
  executiveFree( e );
}

static void test2(void) {
  Executive* e = executiveNew();

  struct timeval tv = { 10, 0 };
  executiveAdd( e, &tv, someExecAction );

  size_t n = executiveClear( e );
  assert( n == 1 );
  
  executiveFree( e );
}

static void test3(void) {
  Executive* e = executiveNew();

  char* heapRef = malloc( 10 );

  struct timeval tv = { 10, 0 };

  /* 
	 Normally, we have the executive event firing handle free'ing
	 heapRef, by passing free (the dual of malloc) as the
	 'envFreeFunc'.  In this test, the event never actually fires, but
	 cancelling it has some 'clean-up' result.
  */
  executiveAddWithFreeFunc( e, &tv, someExecAction, heapRef, free );

  size_t n = executiveClear( e );
  assert( n == 1 );
  
  executiveFree( e );
}

int main(void) {

  if(1)
	test1();

  if(2)
	test2();

  if(3)
	test3();
  
  return 0;
}

// eof

				 
