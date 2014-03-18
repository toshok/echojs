-include $(TOP)/env.mk
include $(TOP)/lib/files.mk

CONFIG?=Release

PCRE= \
	pcre-osx \
	pcre-iosdev \
	pcre-iossim

ALL_TARGETS= \
	$(PCRE:%=$(TOP)/external-deps/%/.libs/libpcre16.a) \
	$(TOP)/node-llvm/build/$(CONFIG)/llvm.node \
	$(ALL_LIBRARIES:%=$(TOP)/runtime/%) \
	$(COFFEE_SOURCES:%.coffee=$(TOP)/lib/generated/%.js) \
	$(JS_SOURCES:%=$(TOP)/lib/generated/%) \
	$(TOP)/ejs-llvm/libejsllvm-module.a \
	$(TOP)/lib/generated/ejs.js.exe

define pcrelib
$(TOP)/external-deps/$(1)/.libs/libpcre16.a:
	$(MAKE) -C external-deps build-$(1)
endef
$(foreach dir,$(PCRE),$(eval $(call pcrelib,$(dir))))

define jslib
$(TOP)/lib/generated/$(1).js: $(TOP)/lib/$(2)
	make -C lib
endef
$(foreach file,$(COFFEE_SOURCES),$(eval $(call jslib,$(basename $(file)),$(file))))
$(foreach file,$(JS_SOURCES),$(eval $(call jslib,$(basename $(file)),$(file))))

$(TOP)/node-llvm/build/$(CONFIG)/llvm.node:
	make -C node-llvm

$(TOP)/ejs-llvm/libejsllvm-module.a:
	make -C ejs-llvm

$(TOP)/lib/generated/ejs.js.exe: $(TOP)/ejs $(TOP)/ejs-llvm/libejsllvm-module.a $(TOP)/node-llvm/build/$(CONFIG)/llvm.node
	$(MAKE) -C lib/generated

$(ALL_LIBRARIES:%=$(TOP)/runtime/%):
	$(MAKE) -C runtime

.PHONY: libs
libs: $(ALL_TARGETS)