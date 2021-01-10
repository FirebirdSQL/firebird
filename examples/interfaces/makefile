#
# The contents of this file are subject to the Initial
# Developer's Public License Version 1.0 (the "License");
# you may not use this file except in compliance with the
# License. You may obtain a copy of the License at
# https://www.ibphoenix.com/about/firebird/idpl.
#
# Software distributed under the License is distributed AS IS,
# WITHOUT WARRANTY OF ANY KIND, either express or implied.
# See the License for the specific language governing rights
# and limitations under the License.
#
# The Original Code was created by Paul Reeves
# for the Firebird Open Source RDBMS project.
#
# Copyright (c) 2020 Paul Reeves and all contributors signed below.
#
# All Rights Reserved.
# Contributor(s): ______________________________________.
#

# -----------------------------------------------------------------------
# Firebird Installation Directory
#
# CHANGE this definition to point to your Firebird installation directory
# -----------------------------------------------------------------------

FIREBIRD =	/opt/firebird
INCLUDE  := -I$(FIREBIRD)/include/
FBCLIENT = $(FIREBIRD)/lib/libfbclient.so

# ---------------------------------------------------------------------
# General Compiler and linker Defines for Linux
# ---------------------------------------------------------------------
CXX	= c++
CXXFLAGS= -c -Wall -g3 -std=c++11  -fno-rtti $(INCLUDE)
RM	= rm -f

#
# Do NOT change anything below this point.
# ---------------------------------------------------------------------
.PHONY: all clean cleanall

.SUFFIXES: .o .cpp

.cpp.o:
	$(CXX) $(CXXFLAGS) $< -o $@

.o:
	$(CXX) -g -o $@ $< $(FBCLIENT)

OUTBIN = 01.create 02.update 03.select 04.print_table 05.user_metadata 06.fb_message 07.blob 08.events 09.service 10.backup 11.batch 12.batch_isc 13.null_pk

#FAILED =

OBJS = *.o

all: $(OUTBIN)

# As far as make is concerned, these entries are
# redundant as the above rules are sufficient.
01.create.o: 01.create.cpp
02.update.o: 02.update.cpp
03.select.o: 03.select.cpp
04.print_table.o: 04.print_table.cpp
05.user_metadata.o: 05.user_metadata.cpp
06.fb_message.o: 06.fb_message.cpp
07.blob.o: 07.blob.cpp
08.events.o: 08.events.cpp
09.service.o: 09.service.cpp
10.backup.o: 10.backup.cpp
11.batch.o: 11.batch.cpp
12.batch_isc.o: 12.batch_isc.cpp
13.null_pk.o: 13.null_pk.cpp

# clean up
clean:
	$(RM) $(OBJS) $(OUTBIN)

cleanall: clean
	$(RM) fbtests.fdb
