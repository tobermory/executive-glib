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
 * Add events to an executive, then fire them.
 */

static void execActionIntAdder( Event* e, struct timeval* actualTime ) {
  (void)actualTime;
  int* ip = executiveEventEnv( e );
  (*ip)++;
}

/* 
   Have an executive event fire action increment an int.  The int is
   the 'environment' (env) of the event, i.e. the data it can 'see'.
*/
static void test1(void) {
  Executive* e = executiveNew();

  int i = 27;
  
  struct timeval tv = { 21, 22 };
  executiveAddWithEnv( e, &tv, execActionIntAdder, &i );

  /* In a real app, this could be ages after the Add */
  struct timeval now;
  gettimeofday( &now, NULL );
  executiveFire( e, &now );

  assert( i == 27+1 );
}

int main(void) {

  if(1)
	test1();
  
  return 0;
}

// eof

				 
