
SOURCES= \
	ejs-arguments.c \
	ejs-array.c \
	ejs-boolean.c \
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
	parson.c \
	main.c

VPATH=.:../parson

OBJECTS=$(SOURCES:%.c=%.o)

CFLAGS=-I. -Wall -g -O0 -Wno-unused-function
DEV_CFLAGS=	-arch armv6 \
		-isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS5.0.sdk \
		-miphoneos-version-min=4.3

SIM_CFLAGS=	-arch i386 \
		-isysroot /Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator5.0.sdk \
		-miphoneos-version-min=4.3


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
