PROG = $(patsubst %.cpp,%,$(wildcard *Test.cpp))

REQUIRES = configpp

include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

CFLAGS = -DUNIX -O0 -g $(FLAGS)

INCLUDES += \
	-I../sputnik \
	-isystem /usr/include/smartmet

LIBS += \
	../sputnik.so \
	$(PREFIX_LDFLAGS) \
	$(REQUIRED_LIBS) \
	-lsmartmet-spine \
	-lsmartmet-macgyver \
	-lboost_thread \
	-lprotobuf \
	-lpthread

all: $(PROG)
clean:
	rm -f $(PROG) *~

test: $(PROG)
	@echo Running tests:
	@ok=true; for prog in $(PROG); do ./$$prog || ok=false; done; $$ok

$(PROG) : % : %.cpp ../sputnik.so
	$(CXX) $(CFLAGS) -o $@ $@.cpp $(INCLUDES) $(LIBS)
