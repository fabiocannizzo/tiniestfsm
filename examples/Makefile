INCLUDE_DIR=../include

CFLAGS += -std=c++20 -O3 -I$(INCLUDE_DIR)

HEADERS := $(shell find $(INCLUDE_DIR) -name "*.h")
$(info HEADERS: $(HEADERS))

MAINS=$(shell find . -name "*.cpp")
$(info MAINS: $(MAINS))

TARGETS := $(patsubst %.cpp,%.exe,$(MAINS))
$(info TARGETS: $(TARGETS))

%.exe : %.cpp $(HEADERS) Makefile
	g++ $(CFLAGS) -o $@ $<

all: $(TARGETS)

.PHONY: clean
clean:
	rm *.exe
