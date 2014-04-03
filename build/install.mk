-include $(TOP)/env.mk
include $(TOP)/lib/files.mk

CONFIG?=Release

ESPRIMA= \
	esprima.js

ESCODEGEN= \
	escodegen.js

ESTRAVERSE= \
	estraverse.js

PCRE= \
	pcre-osx \
	pcre-iosdev \
	pcre-iossim

LIBDIRS= \
	$(PCRE:%=external-deps/%/.libs) \
	js \
	runtime

GENERATED = \
	$(COFFEE_SOURCES:%.coffee=%.js) \
	$(JS_SOURCES)

LIBDIR=$(INSTALLDIR)/lib
BINDIR=$(INSTALLDIR)/bin

VPATH = $(TOP)/esprima:$(TOP)/estraverse:$(TOP)/escodegen:$(TOP)/lib/generated:$(TOP)/lib:$(TOP)/ejs-llvm:$(TOP)/runtime

$(LIBDIRS:%=$(LIBDIR)/%) $(BINDIR) $(LIBDIR):
	@$(MKDIR) $@

$(LIBDIR)/js/%.js: %.js | $(LIBDIR)/js
	@echo [Link] $(lastword $(subst /, ,$@)) && $(LN) $< $@

$(LIBDIR)/js/llvm.node: $(TOP)/node-llvm/build/$(CONFIG)/llvm.node | $(LIBDIR)/js
	@echo [Link] $(lastword $(subst /, ,$@)) && $(LN) $< $@

define runtimelib
$(LIBDIR)/runtime/$(1): $(1) | $(LIBDIR)/runtime
	@echo [Link] $(1) && $(LN) $$< $$@
endef
$(foreach file,$(ALL_LIBRARIES),$(eval $(call runtimelib,$(file))))

define pcreinstall
$(LIBDIR)/external-deps/$(1)/.libs/libpcre16.a: $(TOP)/external-deps/$(1)/.libs/libpcre16.a | $(LIBDIR)/external-deps/$(1)/.libs
	@echo [Link] $(lastword $(subst /, ,$$@)) && $(LN) $$< $$@
endef
$(foreach dir,$(PCRE),$(eval $(call pcreinstall,$(dir))))

$(LIBDIR)/ejs: $(TOP)/ejs | $(LIBDIR)
	@echo [Link] $(lastword $(subst /, ,$@)) && $(LN) $< $@

$(LIBDIR)/ejs.js.exe: ejs.js.exe | $(LIBDIR)
	@echo [Link] $(lastword $(subst /, ,$@)) && $(LN) $< $@

$(BINDIR)/ejs: $(TOP)/build/ejs.in | $(BINDIR)
	@sed -e "s,@libpath@,$(LIBDIR),g" < $< > $@
	@chmod +x $@

install-all: $(PCRE:%=$(LIBDIR)/external-deps/%/.libs/libpcre16.a) $(BINDIR)/ejs $(LIBDIR)/ejs $(LIBDIR)/ejs.js.exe $(ESPRIMA:%=$(LIBDIR)/js/%) $(ESCODEGEN:%=$(LIBDIR)/js/%) $(ESTRAVERSE:%=$(LIBDIR)/js/%) $(GENERATED:%=$(LIBDIR)/js/%) $(LLVM:%=$(LIBDIR)/js/%) $(ALL_LIBRARIES:%=$(LIBDIR)/runtime/%)

install: libs install-all
