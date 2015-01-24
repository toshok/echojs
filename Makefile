TOP=$(shell pwd)

include $(TOP)/build/config.mk

SUBDIRS=external-deps node-compat node-llvm ejs-llvm lib runtime

# run git submodule magic if somebody is antsy and doesn't type the magic incantation before typing make
all-local:: ensure-submodules

NODE_PATH=$(TOP)/node-llvm/build/Release:$(TOP)/lib/generated:$(TOP)/esprima:$(TOP)/escodegen:$(TOP)/estraverse

all-hook:: bootstrap

install-local::
	@$(MKDIR) $(bindir)
	$(INSTALL) -c ejs.exe $(bindir)/ejs

clean-local::
	rm -f ejs-es6.js.exe.stage1 ejs-es6.js.exe.stage2 ejs-es6.js.exe.stage3 ejs.exe

TARNAME=$(PRODUCT_name)-$(PRODUCT_VERSION)
TARFILE=$(TARNAME).tar.gz
DISTROOT=$(TOP)
dist-hook:: ensure-submodules
	@echo creating $(DISTROOT)/$(TARNAME).tar.gz
	@rm -rf $(DISTROOT)/$(TARNAME)
	@$(MKDIR) $(DISTROOT)/$(TARNAME)
	@tar -c --exclude .git --exclude .gitmodules --exclude .gitignore --exclude .deps --exclude host-config.mk --exclude $(TARFILE) --exclude $(TARNAME) * | tar -C $(DISTROOT)/$(TARNAME) -x
	@(cd $(DISTROOT); \
	  tar -czf $(TARFILE) $(TARNAME))
	@rm -rf $(DISTROOT)/$(TARNAME)
	@ls -l $(DISTROOT)/$(TARFILE)

check:
	EJS_DRIVER=$(TOP)/ejs.exe $(MAKE) -C test check

bootstrap: stage3

MODULE_DIRS = --moduledir $(TOP)/node-compat --moduledir $(TOP)/ejs-llvm

stage1: ejs-es6.js.exe.stage1
	@cp ejs-es6.js.exe.stage1 ejs.exe
	@ls -l ejs.exe
	@echo DONE

stage2: ejs-es6.js.exe.stage2
	@cp ejs-es6.js.exe.stage2 ejs.exe
	@ls -l ejs.exe
	@echo DONE

stage3: ejs-es6.js.exe.stage3
	@cp ejs-es6.js.exe.stage3 ejs.exe
	@ls -l ejs.exe
	@echo DONE

ejs-es6.js.exe.stage1:
	@echo Building stage 1
	@NODE_PATH="$(NODE_PATH)" time ./ejs --srcdir --leave-temp $(MODULE_DIRS) ejs-es6.js
	@mv ejs-es6.js.exe ejs-es6.js.exe.stage1

ejs-es6.js.exe.stage2: ejs-es6.js.exe.stage1
	@echo Building stage 2
	@time ./ejs-es6.js.exe.stage1 --srcdir --leave-temp $(MODULE_DIRS) ejs-es6.js
	@mv ejs-es6.js.exe ejs-es6.js.exe.stage2

ejs-es6.js.exe.stage3: ejs-es6.js.exe.stage2
	@echo Building stage 3
	@time ./ejs-es6.js.exe.stage2 --srcdir --leave-temp $(MODULE_DIRS) ejs-es6.js
	@mv ejs-es6.js.exe ejs-es6.js.exe.stage3

echo-command-line:
	@echo ./ejs.exe --leave-temp $(MODULE_DIRS) ejs-es6.js

osx-tarball:
	$(MAKE) -C release osx-tarball

ensure-submodules:
	@if [ ! -f pcre/configure.ac ]; then \
	  git submodule init; \
	  git submodule update; \
	fi

include $(TOP)/build/build.mk
