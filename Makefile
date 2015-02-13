OBJS = src/app.o \
       src/obj/binary.o \
       src/g13_binoutcmdev.o \
       src/g120_auth.o \
       src/hammer.o \
       src/util.o \
       src/dnp3.o

INCLUDES = include/dnp3.h

CFLAGS += -O0 -ggdb  # XXX debuging
CFLAGS += -Iinclude -std=c99 `pkg-config --cflags glib-2.0`
LDFLAGS += -lhammer `pkg-config --libs glib-2.0`


test : $(OBJS) test.o


$(OBJS) test.o : $(INCLUDES)

clean :
	rm -f $(OBJS) test.o test

.PHONY: clean
