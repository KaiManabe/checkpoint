# Simple Makefile for Linux

CXX ?= g++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Iinclude
LDFLAGS ?=
LDLIBS ?=

PREFIX ?= /usr/local
DESTDIR ?=
BINDIR ?= $(PREFIX)/bin

SRCDIR := src
OBJDIR := obj
OUTDIR := bin
TARGET := $(OUTDIR)/checkpoint

SOURCES := \
	$(SRCDIR)/arguments.cpp \
	$(SRCDIR)/checkpoint_hash.cpp \
	$(SRCDIR)/checkpoint_ignore.cpp \
	$(SRCDIR)/checkpoint_layout.cpp \
	$(SRCDIR)/checkpoint_repository.cpp \
	$(SRCDIR)/checkpoint_snapshot.cpp \
	$(SRCDIR)/main.cpp

OBJECTS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SOURCES))

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(OUTDIR)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/checkpoint

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/checkpoint

clean:
	rm -rf $(OBJDIR) $(OUTDIR)
