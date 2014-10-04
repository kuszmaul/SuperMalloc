ROOT      = ..
BUILD     = coverage
OPTFLAGS  = -O0
COVERAGE = -fprofile-arcs -ftest-coverage -DCOVERAGE
UNITTESTS = unit-tests
TESTING   = -DTESTING
LINKMODE  = static
include ../Makefile.include

gcov:
	gcov -bc $(patsubst %,../src/%.cc,$(LIBOBJECTS)) -o . -s ..
