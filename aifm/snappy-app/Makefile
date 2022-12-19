CXX=g++-9
LDXX=g++-9
#SNAPPY_PATH=/mnt/ssd/AIFM/aifm/snappy-original/snappy-1.1.8
SNAPPY_PATH=/mnt/ssd/AIFM/aifm/snappy
CXXFLAGS= -I$(SNAPPY_PATH) -I$(SNAPPY_PATH)/build -L$(SNAPPY_PATH)/build -lsnappy -std=c++2a -O3 -fconcepts -Wno-unused-function -mcmodel=medium

#must be first
all: main

main: main-contiguous-objects.cpp main-contiguous-objects.o
	$(LDXX) -o $@ main-contiguous-objects.o $(CXXFLAGS)

#rule to generate a dep file by using the C preprocessor
#(see man cpp for details on the - MM and - MT options)
%.d: %.cpp
	@$(CXX) $(CXXFLAGS) $< -MM -MT $(@:.d=.o) >$@
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf main-contiguous-objects.o main