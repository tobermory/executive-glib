# The Executive: Multiplexing I/O and Timed Actions

+ A Problem Statement

+ Introducing the Executive

+ Executive API

+ Executive In Action

+ Building The Library

+ Running Test Applications

## A Problem Statement

Write a (Unix/C) program that does the following:

**Read lines of text from the user at a keyboard. For each line L
received, print the line length. Concurrently with servicing this
user I/O, print 'foo' every 5 seconds and 'bar' every 7. Exit
gracefully after one minute.**

I can think of several ways to attack this 'foobar' problem:

+ Use the POSIX [pthread](https://man7.org/linux/man-pages/man3/pthread_create.3.html) API. Create three threads, one each to handle the foo printing, the bar
printing and the end condition.  All can use sleep() for their
timing. The main thread can service user input. A variation on this
could use alarm() for the end condition instead.  Our
[version](src/test/c/foobar-pthreads.c).

+ Use the POSIX
[alarm](https://man7.org/linux/man-pages/man2/alarm.2.html) API for
both foo and bar printing and main loop can process user I/O. A final
alarm() can end program at 60s. Alas, only one alarm can be pending at
once, on Linux at least, so this solution gets messy quickly.

+ Use the Linux
[timerfd](https://man7.org/linux/man-pages/man2/timerfd_create.2.html)
API. I am fairly new to this. It does a clean, terse job of solving
the problem. Alas it is Linux-specific, so not for other Unices. Our
[version](src/test/c/foobar-timerfd.c).

To that list we can add the focus of the page you are reading: the
Executive solution.

## Introducing the Executive

The C code here (Unix/Linux flavoured) implements a minimal API (5
main routines, a few supplementary ones) called an Executive. At its
heart, the *Executive* is just a time-ordered list of *Event* objects.
Events are 'posted' to the Executive for some time in the future.
When that time occurs, the Event is extracted from the (head of) the
Executive and its associated *Action* takes place.

The Executive is used in combination with Unix *select* system
call. We compute the time delta from now to the earliest Executive
event time, then select on fds of interest for that delta.  If/when
I/O data does arrive, we process it and re-compute a new delta and
start over.  Eventually, no data will arrive in the interval and we
pop the head Event off the Executive and do its Action. Actions may
add new Events to the Executive, and the whole process perpetuates.

The Executive is packaged in the form of a library of just one
[.c](src/main/c/executive.c) and one
[.h](src/main/include/executive/executive.h) file.  Once built, you
have the .h plus a .a/.so file to use in larger applications.

This version of the Executive makes use of the GList data structure
from the GLib C library, hence the repo name Executive-GLib.  It
should be buildable anywhere that GLib is.  There is a second
implementation of the Executive, one suited to embedded systems (has
no Unix depenedency, and no mallocs!). More to follow on that.

## Executive API

The Executive API revolves around two abstract data types: Executive
and Event. We say abstract because user access to them is via an API
only.  No internal details of either type are revealed by
[executive.h](src/main/include/executive/executive.h) --- no struct
members are visible to calling code. Chapeau [David Parnas](https://en.wikipedia.org/wiki/David_Parnas).

The primary routines are:


```
Executive* executiveNew(void);

size_t executiveAdd( Executive* e, 
                     struct timeval* scheduledTime,
                     Action action );

size_t executiveAddWithEnv( Executive* e, 
                            struct timeval* scheduledTime,
                            Action action,
                            void* env );

struct timeval* executivePeek( Executive* );

void executiveFire( Executive*, struct timeval* actualTime );
```

These are supplemented by the lesser-used:

```
size_t executiveAddWithFreeFunc( Executive* e,
                                 struct timeval* scheduledTime,
                                 Action action,
                                 void* env,
                                 void (*envFree)( void* ) );

size_t executiveLength( Executive* );

size_t executiveClear( Executive* );

size_t executiveClearMatchingAction( Executive*, Action a );

Executive* executiveEventExecutive( Event* );

struct timeval* executiveEventScheduledTime( Event* );

void* executiveEventEnv( Event* );
```

plus a few more of rare use.  See [executive.h](src/main/include/executive/executive.h) for the full API.


## Executive In Action

The problem statement above can be solved using an Executive.  The
code below is copied from
[foobar-executive.c](src/test/c/foobar-executive.c), with commentary
removed for brevity:

```
static void execActionFoo( Event* e, struct timeval* actualTime ) {
  printf( "foo!\n" );
  struct timeval* tv = true ? executiveEventScheduledTime(e) : actualTime;
  struct timeval fooTime = { 5, 0 };
  struct timeval nextTime;
  timeradd( tv, &fooTime, &nextTime );
  executiveAdd( executiveEventExecutive(e), &nextTime, execActionFoo );
}

static void execActionBar( Event* e, struct timeval* actualTime ) {
  printf( "bar!\n" );
  struct timeval* tv = true ? executiveEventScheduledTime(e) : actualTime;
  struct timeval barTime = { 7, 0 };
  struct timeval nextTime;
  timeradd( tv, &barTime, &nextTime );
  executiveAdd( executiveEventExecutive(e), &nextTime, execActionBar );
}

static bool done = false;

static void execActionDone( Event* e, struct timeval* actualTime ) {
  done = true;
}

int main(void) {

  Executive* E = executiveNew();

  struct timeval now;
  gettimeofday( &now, NULL );
  struct timeval end;

  struct timeval run = { 60, 0 };
  timeradd( &now, &run, &end );
  executiveAdd( E, &end, execActionDone );

  struct timeval fooTime = { 5, 0 };
  timeradd( &now, &fooTime, &end );
  executiveAdd( E, &end, execActionFoo );

  struct timeval barTime = { 7, 0 };
  timeradd( &now, &barTime, &end );
  executiveAdd( E, &end, execActionBar );

  fd_set fds;
  FD_ZERO( &fds );
  FD_SET( STDIN_FILENO, &fds );
  int fdMax = STDIN_FILENO;

  while( !done ) {

    gettimeofday( &now, NULL );
    struct timeval *head = executivePeek(E);

    if( timercmp( head, &now, < ) ) {
      executiveFire( E, &now );
      continue;
    }	  

    struct timeval wait;
    timersub( head, &now, &wait );
	
    fd_set work = fds;
    int ready = select( fdMax + 1, &work, NULL, NULL, &wait );
    if( ready == -1 ) {
      break; 
    }
	
    if( ready == 0 ) {
      gettimeofday( &now, NULL );
      executiveFire( E, &now );
      continue;
    }

    if( FD_ISSET( STDIN_FILENO, &work ) ) {
      char input[1024];
      int nin = read( STDIN_FILENO, input, sizeof(input) - 1 );
      if( nin < 1 )
        break;
      input[nin] = 0;
      int lenInput = strlen( input );
      printf( "%d\n", lenInput );
    }
  }
}

```

TODO: add commentary of [foobar-executive-env.c](src/test/c/foobar-executive-env.c), compare/contrast the two.  Explain use of event environments.

## Building The Library

As stated above, the code here uses the GLib C library for a core
[data structure](https://docs.gtk.org/glib/struct.List.html). Assuming
this library is not installed on your system, you'll need to:

```
$ sudo apt install libglib2.0-dev
```

or similar for other package managers (yum, etc). Or perhaps get GLib
and build from source.

Building the Executive library presented here is via GNU make, we
supply a simple [Makefile](./Makefile).  If needed

```
$ sudo apt install make
```

The build should locate the GLib library (*.h, *.a, *.so) via
`pkg-config`.  If this does not work, you may need to alter the
Makefile:

```
$ ed Makefile
```

The lines of interest that may need changing are:

```
CPPFLAGS  ?=  $(shell pkg-config --cflags glib-2.0)
LOADLIBES ?=  $(shell pkg-config --libs   glib-2.0)
```

With that done, it should now be a case of just:

```
$ make
```

to go from this:

```
src/main
????????? c
??????? ????????? executive.c
????????? include
    ????????? executive/executive.h
```

to this:

```
libexecutive-glib.a
```

By default, the build details are terse.  To see a bit more:

```
$ make clean
$ make V=1
```

## Test Applications

Some simple test cases are also provided:

```
$ make tests
$ make tests V=1
```

to go from this:

```
src/test/c/memTests.c
src/test/c/fireTests.c
src/test/c/foobar-executive.c
src/test/c/foobar-executive-env.c
src/test/c/foobar-pthreads.c
src/test/c/foobar-timerfd.c
```

to this:

```
memTests, fireTests, foobar-executive, etc
```

and we can run any of them:

```
$ ./fireTests ; ./foobar-executive ; ./foobar-pthreads ; etc
```

---

For other work of mine, see [here](https://github.com/tobermory).

sdmaclean AT jeemale


