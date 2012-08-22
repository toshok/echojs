
SOURCES= \
	ejs-array.c \
	ejs-init.c \
	ejs-object.c \
	ejs-ops.c \
	ejs-require.c \
	ejs-string.c \
	main.c

OBJECTS=$(SOURCES:%.c=%.o)

CFLAGS=-I. -Wall -g -O2

libecho.a: $(OBJECTS)
	$(AR) cru $@ $(OBJECTS)

$(OBJECTS): %.o: %.c
	@mkdir -p .deps
	@$(CC) -MM $(CFLAGS) $< > .deps/$@-deps
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f test $(OBJECTS)

-include $(patsubst %.o,.deps/%.o-deps,$(OBJECTS))
