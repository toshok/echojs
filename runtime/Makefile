
SOURCES=object.c \
	ops.c \
	main.c

OBJECTS=$(SOURCES:%.c=%.o)

CFLAGS=-I. -Wall -g -O0

libecho.a: $(OBJECTS)
	$(AR) cru $@ $(OBJECTS)

test: $(OBJECTS)
	$(CC) -o $@ $(OBJECTS)

clean:
	rm -f test $(OBJECTS)
