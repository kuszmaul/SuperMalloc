COMPILER_FLAGS = -W -Wall -Werror -O3
CFLAGS=-std=c99 $(COMPILER_FLAGS)
CXXFLAGS=-std=c++11  $(COMPILER_FLAGS)
default: coreid division lock-overhead
division-generated.cc: division
	./division 13 > $@
divtest: divtest.cc | division-generated.cc
lock-overhead: CXXFLAGS += -pthread
