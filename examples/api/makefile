# The contents of this file are subject to the Interbase Public
# License Version 1.0 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy
# of the License at http://www.Inprise.com/IPL.html
#
# Software distributed under the License is distributed on an
# "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express
# or implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code was created by Inprise Corporation
# and its predecessors. Portions created by Inprise Corporation are
# Copyright (C) Inprise Corporation.
#
# All Rights Reserved.
# Contributor(s): ______________________________________.
# ---------------------------------------------------------------------

# -----------------------------------------------------------------------
# Firebird Installation Directory
#
# CHANGE this definition to point to your Firebird installation directory
# -----------------------------------------------------------------------
FIREBIRD	= /opt/firebird

DB    		= employee.fdb

INCLUDE		:= -I$(FIREBIRD)/include -I../include
FBCLIENT	= $(FIREBIRD)/lib/libfbclient.so
GPRE_M		= $(FIREBIRD)/bin/gpre -c -n -m
ISQL		= $(FIREBIRD)/bin/isql

# ---------------------------------------------------------------------
# General Compiler and linker Defines for Linux
# ---------------------------------------------------------------------
CC=		gcc
LINK=		gcc
LIB_LINK=	gcc
CFLAGS=		-c -std=c99 -g3 -Wall $(INCLUDE) -o
LIB_CFLAGS=	-fPIC -pthread $(CFLAGS)
LIB_LINK_FLAGS=	-shared -pthread -lfbclient -lrt -lm -ldl -lncurses
RM=		rm -f

#
# Do NOT change anything below this point.
# ---------------------------------------------------------------------
.PHONY: clean all

.SUFFIXES: .o .c .e

.e.c:
	$(GPRE_M) $< -d $(DB)

.c.o:
	$(CC) $< $(CFLAGS) $@

.o:
	$(LINK) -o $@ $< ${FBCLIENT} $(LINKFLAGS)


OUTBIN	= api1 api2 api3 api4 api5 api6 api7 api8 api9 api9f api10 api11 api12 api13 api14 api15 api16 api16t apifull

OUTCODE = api14.c

OBJS = *.o api9f.so

all:	api1 api2 api3 api4 api5 api6 api7 api8 api9f api9 api10 api11 api12 api13 api14 api15 api16 api16t apifull

api1.o: api1.c

api2.o: api2.c

api3.o: api3.c

api4.o: api4.c

api5.o: api5.c

api6.o: api6.c

api7.o: api7.c

api8.o: api8.c

api9f.o:api9f.c
	$(CC) $< $(LIB_CFLAGS) $@

api9f:	api9f.o api9f.sql
	$(CC) $@.o -o $@.so $(LIB_LINK_FLAGS)
	@echo ------------------------------------------------------
	@echo You may need to restart the firebird service
	@echo in order for api9f to work correctly.
	@echo ------------------------------------------------------
	cp api9f.so $(FIREBIRD)/UDF
	@echo Declaring desc_filter in database $(DB)
	$(ISQL) -e -i api9f.sql -o api9f.txt -m -m2 $(DB) 
	@echo ------------------------------------------------------

api9.o: api9.c

api9:	api9.o
	$(LINK) $@.o -o $@ ${FBCLIENT} $(LINKFLAGS)

api10.o: api10.c

api11.o: api11.c

api12.o: api12.c

api13.o: api13.c

api14.c: api14.e
	$(ISQL) -e -i ../empbuild/employe2.sql -o employe2.txt -m -m2
	$(GPRE_M) $< -d $(DB)

api14.o: api14.c

api15.o: api15.c

api16.o: api16.c

api16t.o: api16t.c

apifull.o: apifull.c

clean:
	$(RM) $(OBJS) $(OUTBIN) $(OUTCODE)
	@echo Dropping desc_filter in database $(DB)
	-$(ISQL) -echo -input api9fdrop.sql -output api9fdrop.txt -merge -m2 $(DB)
	$(RM) new.fdb desc.txt a.out api9f.txt api9fdrop.txt employe2.txt employe2.fdb
  
