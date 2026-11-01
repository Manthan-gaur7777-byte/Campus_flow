CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2

ifeq ($(OS),Windows_NT)
EXE := .exe
else
EXE :=
endif

LIB_SRC := allocation.cpp hash_table.cpp persistence.cpp seat_grid.cpp segment_tree.cpp \
           waitlist_heap.cpp waitlist.cpp occupancy.cpp graph_menu.cpp venue_link.cpp
LIB_OBJ := $(LIB_SRC:.cpp=.o)
HDR     := $(wildcard *.h)

APP   := campusflow$(EXE)
TESTS := test_all$(EXE) test_waitlist$(EXE) test_occupancy$(EXE) test_graph$(EXE) test_integration$(EXE)

.PHONY: all test demo clean

all: $(APP)

$(APP): $(LIB_OBJ) main.o
	$(CXX) $(CXXFLAGS) $^ -o $@

test_%$(EXE): $(LIB_OBJ) test_%.o
	$(CXX) $(CXXFLAGS) $^ -o $@

%.o: %.cpp $(HDR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TESTS)
	@set -e; for t in $(TESTS); do echo "== $$t"; ./$$t; done
	@echo "ALL TEST SUITES PASSED"

demo: $(APP)
	@sh samples/run_samples.sh

clean:
	rm -f *.o $(APP) $(TESTS) *.dat
