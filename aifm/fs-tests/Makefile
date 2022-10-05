FLAGS  = -g -Wall -D_GNU_SOURCE -O3
LDXX	= g++-9
CXX	= g++-9

override CXXFLAGS += -Wno-subobject-linkage $(FLAGS)

test_tcp_array_add_src = test_tcp_array_add.cpp
test_tcp_array_add_obj = $(test_tcp_array_add_src:.cpp=.o)
random_block_src = random_block.cpp
random_block_obj = $(random_block_src:.cpp=.o)

override CXXFLAGS += -std=gnu++2a -fconcepts -Wno-unused-function

all: test_tcp_array_add random_block

test_tcp_array_add: $(test_tcp_array_add_obj)
	$(LDXX) -o $@.out $(test_tcp_array_add_obj)

random_block: $(random_block_obj)
	$(LDXX) -o $@.out $(random_block_obj)

clean:
	rm *.o *.out