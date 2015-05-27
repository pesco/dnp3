OBJS = src/app.o \
       src/oblock.o \
       src/obj/binary.o \
       src/obj/binoutcmd.o \
       src/obj/counter.o \
       src/obj/analog.o \
       src/obj/time.o \
       src/obj/class.o \
       src/obj/iin.o \
       src/obj/application.o \
       src/g120_auth.o \
       src/transport.o \
       src/link.o \
       src/hammer.o \
       src/util.o \
       src/format.o \
       src/dnp3.o

INCLUDES = include/dnp3.h

CFLAGS += -O0 -ggdb  # XXX debuging
CFLAGS += -Iinclude -std=c99 `pkg-config --cflags glib-2.0`
LDLIBS += -lhammer `pkg-config --libs glib-2.0`


all: test crc

test : $(OBJS) test.o
crc : $(OBJS) crc.o


$(OBJS) test.o : $(INCLUDES)
$(OBJS) crc.o : $(INCLUDES)

clean :
	rm -f $(OBJS) test.o test

.PHONY: clean
