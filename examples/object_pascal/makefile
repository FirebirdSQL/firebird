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
# The Original Code was created by Paul Reeves for IBPhoenix
#  and the Firebird Open Source RDBMS project.
#
# Copyright (c) 2020 Paul Reeves and all contributors signed below.
#
# All Rights Reserved.
# Contributor(s): ______________________________________.
#

# -----------------------------------------------------------------------
# Firebird Installation Directory
#
# Change this definition to point to your Firebird installation directory
# -----------------------------------------------------------------------
FIREBIRD	= /opt/firebird

INCLUDE_FB	= $(FIREBIRD)/include/firebird
FBCLIENT	= $(FIREBIRD)/lib/libfbclient.so

# Directory to store compiled unit files
UNITBINDIR	= lib

# ---------------------------------------------------------------------
# General Compiler and linker Defines for Free Pascal
# ---------------------------------------------------------------------
FPC			=	fpc
FPCFLAGS	=	-g -Fucommon -Fu$(INCLUDE_FB) -FU$(UNITBINDIR) -Mdelphi -FE.
RM			=	rm -f

OBJECTS		= $(UNITBINDIR)/*
OUTBIN 		= 01.create 02.update 03.select

# To Do...
# 04.print_table 05.user_metadata.cpp 06.fb_message 07.blob 08.events 09.service 10.backup 11.batch 12.batch_isc 13.null_pk


.PHONY:  clean all

.SUFFIXES: .pas

.pas:
	-mkdir $(UNITBINDIR)
	$(FPC) $(FPCFLAGS) $<

all: $(OUTBIN)

01.create: 01.create.pas
02.update: 02.update.pas
03.select: 03.select.pas

# clean up
clean:
	$(RM) $(OBJECTS) $(OUTBIN) fbtests.fdb
	-rm -d $(UNITBINDIR)
