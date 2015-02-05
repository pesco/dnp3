OBJS = src/app.o \
       src/dnp3.o \
       src/g1_binin.o \
       src/g120_auth.o \
       src/hammer.o \
       src/util.o

INCLUDES = include/dnp3.h

CFLAGS += -Iinclude -std=c99 `pkg-config --cflags glib-2.0`
LDFLAGS += -lhammer `pkg-config --libs glib-2.0`


test : $(OBJS) test.o


$(OBJS) test.o : $(INCLUDES)

clean :
	rm -f $(OBJS) test.o test

.PHONY: clean
