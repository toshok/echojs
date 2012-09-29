
SOURCES= \
	ejs-arguments.c \
	ejs-array.c \
	ejs-init.c \
	ejs-console.c \
	ejs-process.c \
	ejs-function.c \
	ejs-object.c \
	ejs-ops.c \
	ejs-regexp.c \
	ejs-require.c \
	ejs-string.c \
	ejs-date.c \
	ejs-number.c \
	ejs-builtin-modules.c \
	main.c

OBJECTS=$(SOURCES:%.c=%.o)

CFLAGS=-I. -Wall -g -O0

libecho.a: $(OBJECTS)
	$(AR) cru $@ $(OBJECTS)

$(OBJECTS): %.o: %.c
	@mkdir -p .deps
	@$(CC) -MM $(CFLAGS) $< > .deps/$@-deps
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f test $(OBJECTS) libecho.a

-include $(patsubst %.o,.deps/%.o-deps,$(OBJECTS))
