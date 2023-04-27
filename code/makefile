ROOTCFLAGS  = $(shell root-config --cflags)
ROOTLIBS    = $(shell root-config --libs)
ROOTLIBS    += -lpthread -lcaenhvwrapper

SRC = $(wildcard *.cpp)
OBJ = $(patsubst %.cpp,%.o,$(filter %.cpp,$(SRC)))
DEP = $(patsubst %.cpp,%.d,$(filter %.cpp,$(SRC)))
EXE = N1470

#CFLAGS = -Wall -g -I. -DLINUX -DDEBUG
CFLAGS = -Wall -O2 -I. -DLINUX

GXX = g++
GCC = gcc

all:$(EXE)

$(EXE): $(OBJ)
	$(GXX) -lrt $(CFLAGS) $(ROOTCFLAGS) $(ROOTLIBS) -o $@ $(OBJ)

include $(DEP)
%.d:%.cpp
	set -e;rm -f $@;\
	$(GXX) $(CFLAGS) $(ROOTCFLAGS) -MM $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@;\
	rm -f $@.$$$$

%.o:%.cpp 
	$(GXX) $(CFLAGS) $(ROOTCFLAGS) $(ROOTLIBS) -o $@ -c $< 

clean:
	rm -f *~ .*.cpp.*  *.d *.o $(EXE)
