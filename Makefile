TOP=$(shell pwd)

include $(TOP)/build/config.mk

#SUBDIRS=external-deps node-llvm ejs-llvm lib runtime
SUBDIRS=external-deps node-llvm lib runtime

check:
	$(MAKE) -C test check

bootstrap:
	$(MAKE) -C lib/generated bootstrap

include $(TOP)/build/build.mk
