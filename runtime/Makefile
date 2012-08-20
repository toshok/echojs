
SOURCES=array.c \
	object.c \
	ops.c \
	init.c \
	require.c \
	main.c

OBJECTS=$(SOURCES:%.c=%.o)

CFLAGS=-I. -Wall -g -O0

libecho.a: $(OBJECTS)
	$(AR) cru $@ $(OBJECTS)

test: $(OBJECTS)
	$(CC) -o $@ $(OBJECTS)

clean:
	rm -f test $(OBJECTS)

$(OBJECTS): %.o: object.h ejs.h ops.h
