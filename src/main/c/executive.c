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

#include <glib.h>

#include "executive/executive.h"

typedef struct Executive {
  GList* events;
  Event* sentinel;
} Executive;

struct Event {
  Executive* executive;
  struct timeval scheduledTime;
  Action action;
  gpointer env;
  void (*envFree)( gpointer );
};

static Event* executiveEventNew( Executive* source,
								 struct timeval* scheduledTime, 
								 Action action, gpointer env, 
								 void (*envFree)( gpointer ) );

static void	executiveEventInit( Event* thiz, Executive* source,
								struct timeval* scheduledTime, Action a, 
								gpointer env, void (*envFree)( gpointer ) );

static void executiveEventFree( Event* thiz );

static int executiveEventComparator( gconstpointer a, gconstpointer b );

static size_t executiveAddImpl( Executive* thiz,
								struct timeval* scheduledTime, 
								Action action, 
								gpointer env, void (*envFree)(gpointer) );

static struct timeval ARMAGEDDON = { .tv_sec = INT_MAX,
									 .tv_usec = 999999 };

Executive* executiveNew(void) {
  Executive* result = (Executive*)malloc( sizeof( Executive ) );
  if( !result )
	return NULL;
  result->sentinel = executiveEventNew( NULL, &ARMAGEDDON, NULL, NULL, NULL );
  result->events = g_list_insert_sorted( NULL, result->sentinel, 
										 executiveEventComparator );
  return result;
}

void executiveFree( Executive* thiz ) {
  executiveEventFree( thiz->sentinel );
  g_list_free( thiz->events );
  free( thiz );
}

size_t executiveAdd( Executive* e, 
					 struct timeval* scheduledTime, Action action ) {
  return executiveAddImpl( e, scheduledTime, action, NULL, NULL );
}

size_t executiveAddWithEnv( Executive* e, 
							struct timeval* scheduledTime, Action action,
							void* env ) {
  return executiveAddImpl( e, scheduledTime, action, env, NULL );
}

size_t executiveAddWithFreeFunc( Executive* e,
								 struct timeval* scheduledTime, 
								 Action action, 
								 gpointer env, void (*envFree)(gpointer) ) {
  return executiveAddImpl( e, scheduledTime, action, env, envFree );
}


/**
 * OK to return the sentinel, aka the armageddon, here.  This will
 * happen if/when the executive is empty.
 */
struct timeval* executivePeek( Executive* thiz ) {
  Event* head = (Event*)g_list_nth_data( thiz->events, 0 );
  return &head->scheduledTime;
}

void executiveFire( Executive* thiz, struct timeval* actualTime ) {
  Event* head = (Event*)g_list_nth_data( thiz->events, 0 );
  // the sentinel can never be fired/removed...
  if( head == thiz->sentinel )
	return;
  thiz->events = g_list_remove( thiz->events, head );

  // we permit null actions, of course not very useful!
  if( head->action ) {
	(head->action)( head, actualTime );
  }
  executiveEventFree( head );
}

/*
  the number of user Events is one less than the actual length, which
  includes the sentinel
*/
size_t executiveLength( Executive* thiz ) {
  return g_list_length( thiz->events ) - 1;
}

/**
   Clearing the Executive effectively abandons all Events in it, and
   does NOT invoke their Actions
*/
size_t executiveClear( Executive* thiz ) {
  size_t result = 0;
  while( true ) {
	Event* head = (Event*)g_list_nth_data( thiz->events, 0 );
	if( head == thiz->sentinel )
	  break;
	thiz->events = g_list_remove( thiz->events, head );
	executiveEventFree( head );
	result++;
  }
  return result;
}

/**
 * @result number of events removed
 */
size_t executiveClearMatchingTime( Executive* thiz, struct timeval* tv ) {
  size_t result = 0;
  int index = 0;
  while( true ) {
	Event* el = (Event*)g_list_nth_data( thiz->events, index );
	if( el == thiz->sentinel )
	  break;
	// can short-circuit the search since time-ordered list...
	int cmp = timercmp( &el->scheduledTime, tv, > );
	if( cmp )
	  break;
	cmp = timercmp( &el->scheduledTime, tv, == );
	if( cmp ) {
	  thiz->events = g_list_remove( thiz->events, el );
	  executiveEventFree( el );
	  result++;
	} else {
	  index++;
	}
  }
  return result;
}

/**
 * @result number of events removed
 */
size_t executiveClearMatchingAction( Executive* thiz, Action a ) {
  size_t result = 0;
  int index = 0;
  while( true ) {
	Event* el = (Event*)g_list_nth_data( thiz->events, index );
	if( el == thiz->sentinel )
	  break;
	if( el->action == a ) {
	  thiz->events = g_list_remove( thiz->events, el );
	  executiveEventFree( el );
	  result++;
	} else {
	  index++;
	}
  }
  return result;
}

/**
 * @result number of events removed
 */
size_t executiveClearMatchingEnv( Executive* thiz, gpointer env ) {
  size_t result = 0;
  int index = 0;
  while( TRUE ) {
	Event* el = (Event*)g_list_nth_data( thiz->events, index );
	if( el == thiz->sentinel )
	  break;
	if( el->env == env ) {
	  thiz->events = g_list_remove( thiz->events, el );
	  executiveEventFree( el );
	  result++;
	} else {
	  index++;
	}
  }
  return result;
}

/**
 * @result number of events removed
 */
size_t executiveClearMatchingActionAndEnv( Executive* thiz, 
										Action a, gpointer env ) {
  size_t result = 0;
  int index = 0;
  while( true ) {
	Event* el = (Event*)g_list_nth_data( thiz->events, index );
	if( el == thiz->sentinel )
	  break;
	if( el->action == a && el->env == env ) {
	  thiz->events = g_list_remove( thiz->events, el );
	  executiveEventFree( el );
	  result++;
	} else {
	  index++;
	}
  }
  return result;
}

Executive* executiveEventExecutive( Event* e ) {
  return e->executive;
}

struct timeval* executiveEventScheduledTime( Event* e ) {
  return &e->scheduledTime;
}

void* executiveEventEnv( Event* e ) {
  return e->env;
}

/******************************* STATICS **********************************/

static size_t executiveAddImpl( Executive* thiz,
								struct timeval* scheduledTime, 
								Action action, 
								gpointer env, void (*envFree)(gpointer) ) {
  
  Event* e = executiveEventNew( thiz, scheduledTime, action, env, envFree );
  thiz->events = g_list_insert_sorted
	( thiz->events, e, executiveEventComparator );
  return executiveLength( thiz );
}

static Event* executiveEventNew( Executive* source,
								 struct timeval* scheduledTime, Action action, 
								 gpointer env, void (*envFree)(gpointer) ) {
  Event* result = (Event*)malloc( sizeof( Event ) );
  if( !result )
	return NULL;
  executiveEventInit( result, source, scheduledTime, action, env, envFree );
  return result;
}

static void executiveEventInit( Event* thiz, Executive* source,
								struct timeval* scheduledTime, 
								Action action,
								gpointer env, void (*envFree)(gpointer) ) {
  thiz->executive = source;
  thiz->scheduledTime = *scheduledTime;
  thiz->action = action;
  thiz->env = env;
  thiz->envFree = envFree;
}

// conforming to GLibCompareFunc, orders Events on an Executive
int executiveEventComparator( gconstpointer a, gconstpointer b ) {
  Event* e1 = (Event*)a;
  Event* e2 = (Event*)b;
  struct timeval* tv1 = &e1->scheduledTime;
  struct timeval* tv2 = &e2->scheduledTime;
  if( timercmp( tv1, tv2, < ) )
	return -1;
  if( timercmp( tv1, tv2, > ) )
	return 1;
  return 0;
}

static void executiveEventFree( Event* thiz ) {
  if( thiz->env && thiz->envFree )
	(*thiz->envFree)( thiz->env );
  free( thiz );
}

// eof
