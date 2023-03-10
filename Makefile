#
# Copyright © 2023 Stuart Maclean
#
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#
#     * Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER NOR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
# TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
# USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.
#

OS = $(shell uname)

SHELL = /bin/bash

BASENAME = executive-glib

LIB = lib$(BASENAME).a

TESTS = memTests fireTests

TESTS += foobar-executive foobar-executive-env

TESTS += foobar-pthreads

ifeq ($(OS), Linux)
TESTS += foobar-timerfd
endif

# We use local pkgconfig info to locate glib's settings for cflags,
# libs. Replace as necessary. To install glib-dev on Debian/Ubuntu:
#
# $ sudo apt install libglib2.0-dev

CPPFLAGS  =  $(shell pkg-config --cflags glib-2.0)

LOADLIBES =  $(shell pkg-config --libs glib-2.0)

VPATH = src/main/c src/test/c

CPPFLAGS += -I src/main/include

CFLAGS += -Wall -Werror

LIB_OBJS = $(LIB_SRCS:.c=.o)

# Print out recipes only if V set (make V=1), else quiet to avoid clutter
ifndef V
ECHO=@
endif

default : $(LIB)

$(LIB) : executive.o
	@echo AR $(@F)
	$(ECHO)$(AR) cr $@ $^

tests: $(TESTS)

clean:
	-@$(RM) $(LIB) *.o *.i

%.o : %.c
	@echo CC $(<F)
	$(ECHO)$(CC) -c $(CPPFLAGS) $(CFLAGS) $< $(OUTPUT_OPTION)

$(TESTS) : % : %.o $(LIB)
	@echo LD $(@F) = $(^F)
	$(ECHO)$(CC) $(LDFLAGS)	$^ $(LOADLIBES) $(LDLIBS) $(OUTPUT_OPTION)

foobar-pthreads: LDLIBS += -lpthread

.PHONY: default tests clean

# eof

