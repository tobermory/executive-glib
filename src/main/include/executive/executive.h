#ifndef _EXECUTIVE_TIME_ORDERED_QUEUE_H
#define _EXECUTIVE_TIME_ORDERED_QUEUE_H

#include <stddef.h>
#include <sys/time.h>

/** 
    @author Stuart Maclean

    An 'Executive' is a time-ordered list of 'Events', where an Event
    is a pair: time and action.  Actions are themselves pairs: a
    function and an (optional) arbitrary data environment
    (env). (Action is actually a triple: a env destructor function may
    also be required but this is purely a GC thing).

    We can 'schedule' (add) new events on the Executive, peek
    (inspect) the 'head time' of the Executive and 'fire' the
    Executive.  The latter means removing the head Event and invoking
    its action. We can also cancel (remove) previously added Events
    before they fire.

    Use of an Executive enables us to use time as a pseudo file
    descriptor in an app using 'select' for multiplexing read-ready
    I/O channels.  If no fds are ready by some time, we may take some
    alternative action.  A basic 'reactive' type of app has such a
    main loop:

    E = executiveNew();
    E.add( some event(s) for time(s) Ti in the future );

    while( 2 ) {
      gettimeofday( &now, NULL );
      tE = E.peek();
	  if( tE < now ) {
        // event is late, fire, and continue.  Action will receive
		// both scheduled AND actual times, so can act accordingly.
		executiveFire( E, &now );
		continue;
      }
      compute time delta D from now to tE
      ready = select on all fds, with timeval D in the wait arg
      if( ready == 0 ) {
        // no I/O ready, instead we are at event time for executive head event
    	gettimeofday( &now, NULL );
	    executiveFire( E, &now );
        continue;
      }

	  There IS some I/O to do, check all fds and process as needed.
	  This processing may of course add more events to E.
	 
	  if( FD_ISSET( fd1, &slave ) ) {
	    int nin = read( fd1, buf, sizeof(buf) );
	  }
	  // same for other fds
    }

    Each time round the main loop we compute the delta to the next
    ready event (the head of the Executive), and pass that delta to
    the select.  If the select times out (ready == 0), next time
    around Et will be <= now, so all relevant Es will 'fire'.  If the
    select does not time out, some fd is ready, and we process it as
    usual.  Next time around the loop, the delta is recomputed, and so
    on...

    Of course events can be added to E from anywhere in the app,
    either from firing event handlers or from some I/O.

    We may also want to cancel events previously added to an
    Executive, perhaps as a result of some data available on some I/O
    channel, or even canceling an Event from within the Action
    function of a firing Event.

    Impl note: We install a sentinel event into the Executive before
    any use of the Executive (see executiveInit).  This is to avoid
    testing for a NULL executive.  An executive with just the sentinel
    event is logically an empty executive.  Firing the executive
    with just the sentinel present is a no-op.  So no matter what the order
    of 'schedule' and 'fire', we will always get a defined
    response/action.

	For the data structures needed for our Executive, we use GLib.
*/

#ifdef __cplusplus
extern "C" {
#endif
  
  struct Event;
  typedef struct Event Event;
  
  typedef void (*Action)( Event* e, struct timeval* actualTime );

  struct Executive;
  typedef struct Executive Executive;
  
  Executive* executiveNew(void);

  void executiveFree( Executive* );

  /**
   * @param scheduledTime - absolute time at which event should fire
   * @param action - what action to perform at event time
   */
  size_t executiveAdd( Executive* e, 
					   struct timeval* scheduledTime, Action action );

  /**
   * As above, but where the action wants/needs access to some
   * reference 'environment' object, passed to it as a void*
   * (gpointer). It is assumed that the Action 'knows' the actual type
   * of *env and will cast and use accordingly.
  */
  size_t executiveAddWithEnv( Executive* e, 
							  struct timeval* scheduledTime, Action action,
							  void* env );

  /**
   * As above, but where the environment object needs freeing (by the
   * Action) after the event fires. Rarely used in practice.
   *
   * @param envFree - the 'destructor' for 'env'
   */
  size_t executiveAddWithFreeFunc( Executive* e, 
								 struct timeval* scheduledTime, Action action,
								 void* env, void (*envFree)( void* ) );
  
  /**
   * Peek at the time of earliest event on the supplied executive.
   * 
   * @return time of earliest event, or the armageddon if executive is
   * empty.
   */
 struct timeval* executivePeek( Executive* );

  /**
   * Remove the head event from the Executive. It will have the
   * earliest time of all events on that Executive. Call the Action
   * associated with the event.  By passing in the actual time of the
   * event firing, the action itself can decide if it is 'too late to
   * run'.
   */
  void	executiveFire( Executive*, struct timeval* actualTime );

  size_t executiveLength( Executive* );


  /**
	 Empty the executive, discarded events are NOT fired. Named 'clear'
	 after java.util data structures.
	 
	 @result number of user Events discarded
  */
  size_t executiveClear( Executive* );

  /**
	 Cancel (clear) all events with a time matching that supplied.
	 Discarded events are NOT fired...
	 
	 @result number of user Events discarded
  */
  size_t executiveClearMatchingTime( Executive*, struct timeval* t );

  /**
	 Cancel (clear) all events with an Action matching that supplied
	 (straight function pointer comparison is used). Discarded events
	 are NOT fired...

	 @result number of user Events discarded
  */
  size_t executiveClearMatchingAction( Executive*, Action a );
  
  /**
	 Cancel (clear) all events with a env matching that supplied
	 (straight pointer comparison is used). Discarded events are NOT
	 fired.

	 @result number of user Events discarded
  */
  size_t executiveClearMatchingEnv( Executive*, void* env );

  Executive* executiveEventExecutive( Event* );

  struct timeval* executiveEventScheduledTime( Event* );

  void* executiveEventEnv( Event* );

  
#ifdef __cplusplus
}
#endif
  
#endif
