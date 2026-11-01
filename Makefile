# CampusFlow build file.
#   include/  header files (.h)
#   Source/   program source files (.cpp)
#   object/   compiled object files (.o)
#   tests/    unit and integration test programs
# The final program is ./a.out in this (parent) directory.

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
INCLUDES := -Iinclude

INC_DIR := include
SRC_DIR := Source
OBJ_DIR := object
TST_DIR := tests

APP := a.out

LIB_SRC := allocation.cpp hash_table.cpp persistence.cpp seat_grid.cpp segment_tree.cpp \
           waitlist_heap.cpp waitlist.cpp occupancy.cpp graph_menu.cpp venue_link.cpp
LIB_OBJ := $(addprefix $(OBJ_DIR)/,$(LIB_SRC:.cpp=.o))
MAIN_OBJ := $(OBJ_DIR)/main.o
HDR := $(wildcard $(INC_DIR)/*.h)

TEST_NAMES := test_all test_waitlist test_occupancy test_graph test_integration
TESTS := $(addprefix $(OBJ_DIR)/,$(TEST_NAMES))

.PHONY: all test demo clean

all: $(APP)

$(APP): $(LIB_OBJ) $(MAIN_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@

# Source/*.cpp -> object/*.o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(HDR) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# tests/*.cpp -> object/*.o
$(OBJ_DIR)/test_%.o: $(TST_DIR)/test_%.cpp $(HDR) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Test programs are linked into object/ so the parent stays clean.
$(OBJ_DIR)/test_%: $(LIB_OBJ) $(OBJ_DIR)/test_%.o
	$(CXX) $(CXXFLAGS) $^ -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

test: $(TESTS)
	@set -e; for t in $(TESTS); do echo "== $$t"; ./$$t; done
	@echo "ALL TEST SUITES PASSED"

demo: $(APP)
	@sh samples/run_samples.sh

clean:
	rm -f $(OBJ_DIR)/*.o $(TESTS) $(APP) *.dat
