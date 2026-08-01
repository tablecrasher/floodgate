CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pthread
INCLUDES := -Ithird_party -I. -I$(shell brew --prefix hiredis 2>/dev/null)/include
LDLIBS := -L$(shell brew --prefix hiredis 2>/dev/null)/lib -lhiredis

SRC := $(shell find limiter middleware store -name '*.cpp' 2>/dev/null)
MAIN_SRC := cmd/main.cpp

.PHONY: run build clean

build: bin/floodgate

run: bin/floodgate
	./bin/floodgate

bin/floodgate: $(MAIN_SRC) $(SRC)
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@ $(LDLIBS)

clean:
	rm -rf bin