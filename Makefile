OBJS = src/app.o \
       src/binary.o \
       src/dnp3.o \
       src/g1_binin.o \
       src/g2_bininev.o \
       src/g3_dblbitin.o \
       src/g4_dblbitinev.o \
       src/g10_binout.o \
       src/g120_auth.o \
       src/hammer.o \
       src/util.o

INCLUDES = include/dnp3.h

CFLAGS += -O0 -ggdb  # XXX debuging
CFLAGS += -Iinclude -std=c99 `pkg-config --cflags glib-2.0`
LDFLAGS += -lhammer `pkg-config --libs glib-2.0`


test : $(OBJS) test.o


$(OBJS) test.o : $(INCLUDES)

clean :
	rm -f $(OBJS) test.o test

.PHONY: clean
