TOP=$(shell pwd)

SUBDIRS=external-deps node-llvm ejs-llvm lib runtime

all:

check:
	$(MAKE) -C test check

bootstrap:
	$(MAKE) -C lib/generated bootstrap

include $(TOP)/build/build.mk

