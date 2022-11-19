AIFM_PATH=../../../
#SHENANGO_PATH=$(AIFM_PATH)/../shenango
#include $(SHENANGO_PATH)/shared.mk

INC = -I$(AIFM_PATH)/snappy-original -I$(AIFM_PATH)/snappy-original/build
FLAGS  = -g -Wall -D_GNU_SOURCE $(INC)
#LDFLAGS = -T $(SHENANGO_PATH)/base/base.ld
LD      = gcc-9
CC      = gcc-9
LDXX	= g++-9
CXX	= g++-9
AR      = ar
SPARSE  = sparse

#librt_libs = $(SHENANGO_PATH)/bindings/cc/librt++.a
#INC += -I$(SHENANGO_PATH)/bindings/cc -I$(AIFM_PATH)/inc -I$(SHENANGO_PATH)/ksched \
#       -I$(AIFM_PATH)/snappy -I$(AIFM_PATH)/snappy/build

main_src = main.cpp
main_obj = $(main_src:.cpp=.o)

#lib_src = $(wildcard $(AIFM_PATH)/src/*.cpp)
#lib_src := $(filter-out $(AIFM_PATH)/src/tcp_device_server.cpp,$(lib_src))
#lib_obj = $(lib_src:.cpp=.o)

src = $(main_src) #$(lib_src)
obj = $(src:.cpp=.o)
dep = $(obj:.o=.d)

CXXFLAGS := $(filter-out -std=gnu++17,$(CXXFLAGS))
override CXXFLAGS += -std=gnu++2a -fconcepts -Wno-unused-function -mcmodel=medium $(FLAGS)

SNAPPY_LIB = -L$(AIFM_PATH)/snappy-original/build -lsnappy

#must be first
all: main

main: $(main_obj) $(librt_libs) $(RUNTIME_DEPS) $(main_obj) $(lib_obj)
	$(LDXX) -o $@ $(LDFLAGS) $(main_obj) $(lib_obj) $(librt_libs) $(RUNTIME_LIBS) \
        $(SNAPPY_LIB) -lnuma

ifneq ($(MAKECMDGOALS),clean)
-include $(dep)   # include all dep files in the makefile
endif

#rule to generate a dep file by using the C preprocessor
#(see man cpp for details on the - MM and - MT options)
%.d: %.cpp
	@$(CXX) $(CXXFLAGS) $< -MM -MT $(@:.d=.o) >$@
%.o: %.cpp
# Shi: understand what this is doing
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f *.o $(dep) main
