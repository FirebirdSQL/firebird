#  The contents of this file are subject to the Initial
#  Developer's Public License Version 1.0 (the "License");
#  you may not use this file except in compliance with the
#  License. You may obtain a copy of the License at
#  https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
#
#  Software distributed under the License is distributed AS IS,
#  WITHOUT WARRANTY OF ANY KIND, either express or implied.
#  See the License for the specific language governing rights
#  and limitations under the License.
#
#  The Original Code was created by Alexander Peshkoff
#  for the Firebird Open Source RDBMS project.
#
#  Copyright (c) 2020 Alexander Peshkoff <peshkoff@mail.ru>
#  and all contributors signed below.
#
#  All Rights Reserved.
#  Contributor(s): ______________________________________.

ifndef ROOT
  ROOT=../..
endif
#ROOT=$(shell cd ../..; pwd)

PLUGINS=$(ROOT)/plugins
BIN=$(ROOT)/bin
LIB=$(ROOT)/lib

LIB_PREFIX=lib
SHRLIB_EXT=so

ifndef INTERMED
  INTERMED=.
endif

ifdef OUT
  OUT_BIN=$(OUT)/bin
  OUT_PLUG=$(OUT)/plugins
else
  OUT_BIN=.
  OUT_PLUG=.
endif

TOMCRYPT_COMPILE=-DLTC_PTHREAD -DUSE_LTM -DLTM_DESC
OwnInclude=$(shell [ -d tomcrypt.include ] && echo Yes || echo No)
ifeq ($(OwnInclude), Yes)
TOMCRYPT_COMPILE += -Itomcrypt.include
TOMCRYPT_LINK=-L$(LIB)/.tm
endif

SIMPLE_KEY_AUTH_NAME=$(LIB_PREFIX)fbSampleExtAuth.$(SHRLIB_EXT)
BLD_SIMPLE_KEY_AUTH=$(OUT_PLUG)/$(SIMPLE_KEY_AUTH_NAME)
SIMPLE_KEY_AUTH=$(PLUGINS)/$(SIMPLE_KEY_AUTH_NAME)

KEYGEN_NAME=fbSampleExtAuthKeygen
BLD_KEYGEN=$(OUT_BIN)/$(KEYGEN_NAME)
KEYGEN=$(BIN)/$(KEYGEN_NAME)

KEYGEN_objects=$(INTERMED)/keygen.o
TCWRAP_objects=$(INTERMED)/TcWrapper.o
KEY_AUTH_objects=$(INTERMED)/ExtAuth.o

CXXFLAGS=-std=c++11 -pthread -I$(ROOT)/include -fPIC $(TOMCRYPT_COMPILE)
LDFLAGS=-pthread -L$(LIB) -Wl,-rpath,'$$ORIGIN/../lib' $(TOMCRYPT_LINK)

LINK_LIBS=-lfbclient -ltomcrypt -ltommath

$(INTERMED)/%.o: ./%.cpp
	$(CXX) $(CXXFLAGS) -c $(firstword $<) -o $@

.PHONY:		all keygen plugin install

all:		keygen plugin

keygen:		$(BLD_KEYGEN)

$(BLD_KEYGEN):	$(KEYGEN_objects) $(TCWRAP_objects)
	$(CXX) $(LDFLAGS) $^ -o $@ $(LINK_LIBS)

plugin:		$(BLD_SIMPLE_KEY_AUTH)

$(BLD_SIMPLE_KEY_AUTH):	$(KEY_AUTH_objects) $(TCWRAP_objects)
	$(CXX) -shared $(LDFLAGS) $^ -o $@ $(LINK_LIBS)

clean:
	rm -f $(INTERMED)/*.o* $(BLD_KEYGEN) $(BLD_SIMPLE_KEY_AUTH)

install:	$(SIMPLE_KEY_AUTH) $(KEYGEN)

$(SIMPLE_KEY_AUTH): $(BLD_SIMPLE_KEY_AUTH)
	cp $^ $@

$(KEYGEN): $(BLD_KEYGEN)
	cp $^ $@

