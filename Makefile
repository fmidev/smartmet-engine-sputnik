SUBNAME = sputnik
SPEC = smartmet-engine-$(SUBNAME)
INCDIR = smartmet/engines/$(SUBNAME)

# Installation directories

processor := $(shell uname -p)

ifeq ($(origin PREFIX), undefined)
  PREFIX = /usr
else
  PREFIX = $(PREFIX)
endif

ifeq ($(processor), x86_64)
  libdir = $(PREFIX)/lib64
else
  libdir = $(PREFIX)/lib
endif

bindir = $(PREFIX)/bin
includedir = $(PREFIX)/include
datadir = $(PREFIX)/share
enginedir = $(datadir)/smartmet/engines
objdir = obj

# Compiler options

DEFINES = -DUNIX -D_REENTRANT

ifeq ($(CXX), clang++)

 FLAGS = \
	-std=c++11 -fPIC -MD \
	-Weverything \
	-Wno-c++98-compat \
	-Wno-padded \
	-Wno-weak-vtables \
	-Wno-missing-prototypes \
	-Wno-missing-variable-declarations \
	-Wno-shorten-64-to-32 \
	-Wno-global-constructors \
	-Wno-unused-macros \
	-Wno-sign-conversion

 INCLUDES = \
	-isystem $(includedir) \
	-I$(includedir)/smartmet

else

 FLAGS = -std=c++11 -fPIC -MD -Wall -W -Wno-unused-parameter -fno-omit-frame-pointer -fdiagnostics-color=always

 FLAGS_DEBUG = \
        -Wcast-align \
        -Winline \
        -Wno-multichar \
        -Wno-pmf-conversions \
        -Woverloaded-virtual  \
        -Wpointer-arith \
        -Wcast-qual \
        -Wredundant-decls \
        -Wwrite-strings \
        -Wsign-promo

 INCLUDES = \
	-I$(includedir) \
	-I$(includedir)/smartmet

endif

# Compile options in detault, debug and profile modes

CFLAGS_RELEASE = $(DEFINES) $(FLAGS) $(FLAGS_RELEASE) -DNDEBUG -O2 -g
CFLAGS_DEBUG   = $(DEFINES) $(FLAGS) $(FLAGS_DEBUG)   -Werror  -O0 -g

ifneq (,$(findstring debug,$(MAKECMDGOALS)))
  override CFLAGS += $(CFLAGS_DEBUG)
else
  override CFLAGS += $(CFLAGS_RELEASE)
endif

LIBS = -L$(libdir) \
	-lsmartmet-spine \
	-lboost_thread \
	-lboost_date_time \
	-lboost_system \
	-lpthread \
	-lconfig++ \
	-lprotobuf

# rpm variables

rpmsourcedir = /tmp/$(shell whoami)/rpmbuild
rpmerr = "There's no spec file ($(LIB).spec). RPM wasn't created. Please make a spec file or copy and rename it into $(LIB).spec"

# What to install

LIBFILE = $(SUBNAME).so

# How to install

INSTALL_PROG = install -p -m 775
INSTALL_DATA = install -p -m 664

# Compilation directories

vpath %.cpp source
vpath %.h include

# The files to be compiled

PB_SRCS = $(wildcard *.proto)
COMPILED_PB_SRCS = $(patsubst %.proto, source/%.pb.cpp, $(PB_SRCS))
COMPILED_PB_HDRS = $(patsubst %.proto, include/%.pb.h, $(PB_SRCS))

SRCS = $(filter-out %.pb.cpp, $(wildcard source/*.cpp)) $(COMPILED_PB_SRCS)
HDRS = $(filter-out %.pb.h, $(wildcard include/*.h)) $(COMPILED_PB_HDRS)
OBJS = $(patsubst %.cpp, obj/%.o, $(notdir $(SRCS)))

INCLUDES := -Iinclude $(INCLUDES)

.PHONY: test rpm

# The rules

all: objdir $(LIBFILE)
debug: all
release: all
profile: all

$(LIBFILE): $(SRCS) $(OBJS)
	$(CXX) $(CFLAGS) -shared -rdynamic -o $(LIBFILE) $(OBJS) $(LIBS)

clean:
	rm -f $(LIBFILE) *~ source/*~ include/*~
	rm -rf obj
	rm -f source/BroadcastMessage.pb.cpp include/BroadcastMessage.pb.h

format:
	clang-format -i -style=file include/*.h source/*.cpp

install:
	@mkdir -p $(includedir)/$(INCDIR)
	@list='$(HDRS)'; \
	for hdr in $$list; do \
	  echo $(INSTALL_DATA) $$hdr $(includedir)/$(INCDIR)/$$(basename $$hdr); \
	  $(INSTALL_DATA) $$hdr $(includedir)/$(INCDIR)/$$(basename $$hdr); \
	done
	@mkdir -p $(enginedir)
	$(INSTALL_PROG) $(LIBFILE) $(enginedir)/$(LIBFILE)

test:
	cd test && make test

objdir:
	@mkdir -p $(objdir)

rpm: clean protoc
	@if [ -e $(SPEC).spec ]; \
	then \
	  mkdir -p $(rpmsourcedir) ; \
	  tar -C ../../ -cf $(rpmsourcedir)/$(SPEC).tar engines/$(SUBNAME) ; \
	  gzip -f $(rpmsourcedir)/$(SPEC).tar ; \
	  TAR_OPTIONS=--wildcards rpmbuild -v -ta $(rpmsourcedir)/$(SPEC).tar.gz ; \
	  rm -f $(rpmsourcedir)/$(SPEC).tar.gz ; \
	else \
	  echo $(rpmerr); \
	fi;

.SUFFIXES: $(SUFFIXES) .cpp

obj/%.o: %.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<

protoc: $(COMPILED_PB_SRCS)

source/%.pb.cpp include/%.pb.h : %.proto
	mkdir -p tmp
	protoc --cpp_out=tmp BroadcastMessage.proto
	mv tmp/BroadcastMessage.pb.h include/
	mv tmp/BroadcastMessage.pb.cc source/BroadcastMessage.pb.cpp
	rm -rf tmp

ifneq ($(wildcard obj/*.d),)
-include $(wildcard obj/*.d)
endif
