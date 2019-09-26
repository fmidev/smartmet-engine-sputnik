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

# DEFINES = -DUNIX -D_REENTRANT -DMYDEBUG
DEFINES = -DUNIX -D_REENTRANT

-include $(HOME)/.smartmet.mk
GCC_DIAG_COLOR ?= always

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

 FLAGS = -std=c++11 -fPIC -MD -Wall -W -Wno-unused-parameter -fno-omit-frame-pointer -fdiagnostics-color=$(GCC_DIAG_COLOR) -Wnon-virtual-dtor

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

ifeq ($(TSAN), yes)
  FLAGS += -fsanitize=thread
endif
ifeq ($(ASAN), yes)
  FLAGS += -fsanitize=address -fsanitize=pointer-compare -fsanitize=pointer-subtract -fsanitize=undefined -fsanitize-address-use-after-scope
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
	-lsmartmet-macgyver \
	-lboost_thread \
	-lboost_date_time \
	-lboost_system \
	-lpthread \
	-lconfig++ \
	-lprotobuf

# What to install

LIBFILE = $(SUBNAME).so

# How to install

INSTALL_PROG = install -p -m 775
INSTALL_DATA = install -p -m 664

# Compilation directories

vpath %.cpp $(SUBNAME)
vpath %.h $(SUBNAME)

# The files to be compiled

PB_SRCS = $(wildcard *.proto)
COMPILED_PB_SRCS = $(patsubst %.proto, $(SUBNAME)/%.pb.cpp, $(PB_SRCS))
COMPILED_PB_HDRS = $(patsubst %.proto, $(SUBNAME)/%.pb.h, $(PB_SRCS))

SRCS = $(filter-out %.pb.cpp, $(wildcard $(SUBNAME)/*.cpp)) $(COMPILED_PB_SRCS)
HDRS = $(filter-out %.pb.h, $(wildcard $(SUBNAME)/*.h)) $(COMPILED_PB_HDRS)
OBJS = $(patsubst %.cpp, obj/%.o, $(notdir $(SRCS)))

INCLUDES := -I$(SUBNAME) $(INCLUDES)

.PHONY: test rpm

# The rules

all: objdir $(LIBFILE)
debug: all
release: all
profile: all

$(LIBFILE): $(SRCS) $(OBJS)
	$(CXX) $(CFLAGS) -shared -rdynamic -o $(LIBFILE) $(OBJS) $(LIBS)

clean:
	rm -f $(LIBFILE) *~ $(SUBNAME)/*~
	rm -rf obj
	rm -f $(SUBNAME)/BroadcastMessage.pb.cpp $(SUBNAME)/BroadcastMessage.pb.h

format:
	clang-format -i -style=file $(SUBNAME)/*.h $(SUBNAME)/*.cpp

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

rpm: clean protoc $(SPEC).spec
	rm -f $(SPEC).tar.gz # Clean a possible leftover from previous attempt
	tar -czvf $(SPEC).tar.gz --transform "s,^,$(SPEC)/," *
	rpmbuild -ta $(SPEC).tar.gz
	rm -f $(SPEC).tar.gz

.SUFFIXES: $(SUFFIXES) .cpp

obj/%.o: %.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<

protoc: $(COMPILED_PB_SRCS)

sputnik/%.pb.cpp sputnik/%.pb.h : %.proto
	mkdir -p tmp
	protoc --cpp_out=tmp BroadcastMessage.proto
	mv tmp/BroadcastMessage.pb.h $(SUBNAME)/
	mv tmp/BroadcastMessage.pb.cc $(SUBNAME)/BroadcastMessage.pb.cpp
	rm -rf tmp

-include $(wildcard obj/*.d)
