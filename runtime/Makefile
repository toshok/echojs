SOURCES= \
	ejs-arguments.c \
	ejs-array.c \
	ejs-boolean.c \
	ejs-builtin-modules.c \
	ejs-console.c \
	ejs-date.c \
	ejs-error.c \
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
	ejs-typedarrays.c \
	ejs-types.c \
	ejs-uri.c \
	parson.c \
	main.c

libecho_OBJECTS=$(SOURCES:%.c=%.o)

LIBRARIES= libecho.a

all: $(LIBRARIES)

clean:
	rm -f test $(libecho_OBJECTS) $(LIBRARIES) ejs-atoms-gen.c

VPATH=.:../parson

CFLAGS=-I. -Wall -g -O0 -Wno-unused-function
SHARED_CFLAGS=-fPIC $(CFLAGS)

DEV_CFLAGS=	-arch armv6 \
		-isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS5.0.sdk \
		-miphoneos-version-min=4.3

SIM_CFLAGS=	-arch i386 \
		-isysroot /Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator5.0.sdk \
		-miphoneos-version-min=4.3

libecho.a: $(libecho_OBJECTS)
	@echo [$(AR)] $@ && $(AR) cru $@ $(libecho_OBJECTS)

CCLD=clang++
CC=clang

ejs-atoms-gen.c: ejs-atoms.h gen-atoms.js
	@echo [GEN] $@ && ./gen-atoms.js > .tmp-$@ && mv .tmp-$@ $@

$(libecho_OBJECTS): %.o: %.c
	@mkdir -p .deps
	@$(CC) -MM $(CFLAGS) $< > .deps/$@-deps
	@echo [$(CC)] $< && $(CC) $(CFLAGS) -c $< -o $@

-include $(patsubst %.o,.deps/%.o-deps,$(libecho_OBJECTS))
