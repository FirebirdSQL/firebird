LIBRARY=libi128$(CROSS).a

$(LIBRARY):		$(wildcard *.cc) $(wildcard *.h) $(wildcard internal/*.h) $(wildcard ../base/*.h) $(wildcard *.inc) Makefile
	$(RM) -f *.o
	$(CXX) $(CROSS_FLAGS) -I../.. -c -O3 -fPIC int128.cc
	$(AR) crs $(LIBRARY) *.o
	$(RM) -f *.o

.PHONY:	clean
clean:
	$(RM) -f *.o *.a
