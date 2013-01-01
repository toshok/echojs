
SOURCES= \
	ejs-arguments.c \
	ejs-array.c \
	ejs-builtin-modules.c \
	ejs-console.c \
	ejs-date.c \
	ejs-exception.c \
	ejs-function.c \
	ejs-gc.c \
	ejs-init.c \
	ejs-json.c \
	ejs-math.c \
	ejs-number.c \
	ejs-object.c \
	ejs-ops.c \
	ejs-process.c \
	ejs-regexp.c \
	ejs-require.c \
	ejs-string.c \
	ejs-uri.c \
	main.c

OBJECTS=$(SOURCES:%.c=%.o)

CFLAGS=-I. -Wall -g -O2 -Wno-unused-function

libecho.a: $(OBJECTS)
	$(AR) cru $@ $(OBJECTS)

CC=clang

$(OBJECTS): %.o: %.c
	@mkdir -p .deps
	@$(CC) -MM $(CFLAGS) $< > .deps/$@-deps
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f test $(OBJECTS) libecho.a

-include $(patsubst %.o,.deps/%.o-deps,$(OBJECTS))
