ROOTLIBS=`root-config --glibs --cflags`
CFLAGS=-O2 -march=native -std=c++11
CXX=g++

TARGETS=parser align

all: $(TARGETS)

parser: parser.cpp
	$(CXX) $(CFLAGS) $(ROOTLIBS) $^ -o $@

align: align.cpp
	$(CXX) $(CFLAGS) $(ROOTLIBS) $^ -o $@

install:
	cp $(TARGETS) $(HOME)/bin

.PHONY:clean

clean:
	rm $(TARGETS)

