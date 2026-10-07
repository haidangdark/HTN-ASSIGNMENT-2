# ============================================================================
#  Washing machine controller - CO3053 Assignment
#
#  make            build everything (tests + simulator)
#  make test       build and run the automated test suite (64 test cases)
#  make sim        build and start the interactive keyboard simulator
#  make report     run tests and save the result to rpt/test_report.txt
#  make clean      remove build outputs
#
#  On Windows with MSYS2 use:  mingw32-make <target>
# ============================================================================
ifeq ($(origin CC),default)
  CC := gcc
endif
CFLAGS  := -std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -O2 -Isrc
BUILD   := build

ifeq ($(OS),Windows_NT)
  EXE    := .exe
  MKDIR   = if not exist $(BUILD) mkdir $(BUILD)
  RMDIR   = if exist $(BUILD) rmdir /s /q $(BUILD)
  RUN     = $(BUILD)\\
else
  EXE    :=
  MKDIR   = mkdir -p $(BUILD)
  RMDIR   = rm -rf $(BUILD)
  RUN     = ./$(BUILD)/
endif

CORE_SRC := src/wm_fsm.c src/wm_debounce.c src/wm_app.c
CORE_HDR := $(wildcard src/*.h)

TEST_BIN := $(BUILD)/test_wm$(EXE)
SIM_BIN  := $(BUILD)/interactive_sim$(EXE)

.PHONY: all test sim report clean

all: $(TEST_BIN) $(SIM_BIN)

$(TEST_BIN): $(CORE_SRC) $(CORE_HDR) testbench/test_wm.c testbench/test_framework.h
	@$(MKDIR)
	$(CC) $(CFLAGS) $(CORE_SRC) testbench/test_wm.c -o $@

$(SIM_BIN): $(CORE_SRC) $(CORE_HDR) testbench/interactive_sim.c
	@$(MKDIR)
	$(CC) $(CFLAGS) $(CORE_SRC) testbench/interactive_sim.c -o $@

test: $(TEST_BIN)
	$(RUN)test_wm$(EXE)

sim: $(SIM_BIN)
	$(RUN)interactive_sim$(EXE)

report: $(TEST_BIN)
	$(RUN)test_wm$(EXE) --no-color > rpt/test_report.txt

clean:
	@$(RMDIR)
