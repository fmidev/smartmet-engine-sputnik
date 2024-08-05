SUBNAME = sputnik
SPEC = smartmet-engine-$(SUBNAME)
INCDIR = smartmet/engines/$(SUBNAME)

REQUIRES = configpp

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

DEFINES = -DUNIX -D_REENTRANT

LIBS += $(PREFIX_LDFLAGS) \
	$(REQUIRED_LIBS) \
	-lsmartmet-spine \
	-lsmartmet-macgyver \
	-lboost_thread \
	-lboost_system \
	-lpthread \
	-lconfig++ \
	-lprotobuf

# What to install

LIBFILE = $(SUBNAME).so

# Compilation directories

vpath %.cpp $(SUBNAME)
vpath %.h $(SUBNAME)
vpath %.o $(objdir)

# The files to be compiled
PB_SRCS = $(wildcard *.proto)
COMPILED_PB_SRCS = $(patsubst %.proto, $(SUBNAME)/%.pb.cpp, $(PB_SRCS))
COMPILED_PB_HDRS = $(patsubst %.proto, $(SUBNAME)/%.pb.h, $(PB_SRCS))

SRCS = $(filter-out %.pb.cpp, $(wildcard $(SUBNAME)/*.cpp)) $(COMPILED_PB_SRCS)
HDRS = $(filter-out %.pb.h, $(wildcard $(SUBNAME)/*.h)) $(COMPILED_PB_HDRS)
OBJS = $(patsubst %.cpp, obj/%.o, $(notdir $(SRCS)))

.PHONY: rpm

# The rules

all: objdir $(LIBFILE)
debug: all
release: all
profile: all

$(LIBFILE): $(SRCS) $(OBJS)
	$(CXX) $(LDFLAGS) -shared -rdynamic -o $(LIBFILE) $(OBJS) $(LIBS)
	@echo Checking $(LIBFILE) for unresolved references
	@if ldd -r $(LIBFILE) 2>&1 | c++filt | grep ^undefined\ symbol |\
			grep -Pv ':\ __(?:(?:a|t|ub)san_|sanitizer_)'; \
		then rm -v $(LIBFILE); \
		exit 1; \
	fi

clean:
	rm -f $(LIBFILE) *~ $(SUBNAME)/*~
	rm -f $(SUBNAME)/BroadcastMessage.pb.cpp $(SUBNAME)/BroadcastMessage.pb.h
	rm -rf obj

format:
	clang-format -i -style=file $(SUBNAME)/*.h $(SUBNAME)/*.cpp examples/*.cpp

install:
	@mkdir -p $(includedir)/$(INCDIR)
	@list='$(HDRS)'; \
	for hdr in $$list; do \
	  echo $(INSTALL_DATA) $$hdr $(includedir)/$(INCDIR)/$$(basename $$hdr); \
	  $(INSTALL_DATA) $$hdr $(includedir)/$(INCDIR)/$$(basename $$hdr); \
	done
	@mkdir -p $(enginedir)
	$(INSTALL_PROG) $(LIBFILE) $(enginedir)/$(LIBFILE)

objdir:
	@mkdir -p $(objdir)

rpm: clean protoc $(SPEC).spec
	rm -f $(SPEC).tar.gz # Clean a possible leftover from previous attempt
	tar -czvf $(SPEC).tar.gz --exclude test --exclude-vcs --transform "s,^,$(SPEC)/," *
	rpmbuild -tb $(SPEC).tar.gz
	rm -f $(SPEC).tar.gz

.SUFFIXES: $(SUFFIXES) .cpp

obj/%.o: %.cpp
	$(CXX) $(CFLAGS) $(INCLUDES) -c -MD -MF $(patsubst obj/%.o, obj/%.d, $@) -MT $@ -o $@ $<

obj/Engine.o obj/Messages.o : protoc

protoc: $(COMPILED_PB_SRCS)

sputnik/%.pb.cpp: %.proto; mkdir -p tmp
	protoc --cpp_out=tmp BroadcastMessage.proto
	mv tmp/BroadcastMessage.pb.h $(SUBNAME)/
	mv tmp/BroadcastMessage.pb.cc $(SUBNAME)/BroadcastMessage.pb.cpp
	rm -rf tmp 

test:
	@test "$$CI" = "true" && true || false

-include $(wildcard obj/*.d)
