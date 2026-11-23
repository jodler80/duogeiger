# Host (PC) unit tests for the pure logic in duogeiger/mqtt_logic.h
# Usage: make -C test/host        (builds and runs)
CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O0 -g
SRC := $(wildcard test_*.cpp)

run: run_tests
	./run_tests

run_tests: $(SRC) minitest.h ../../duogeiger/mqtt_logic.h
	TZ=UTC $(CXX) $(CXXFLAGS) -I../../duogeiger -o $@ $(SRC)

clean:
	rm -f run_tests

.PHONY: run clean
