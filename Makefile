TOP=$(shell pwd)

include $(TOP)/build/config.mk

SUBDIRS=external-deps node-compat node-llvm ejs-llvm lib runtime

STAGE1_EXE = ejs.exe.stage1
STAGE2_EXE = ejs.exe.stage2
STAGE3_EXE = ejs.exe.stage3

# run git submodule magic if somebody is antsy and doesn't type the magic incantation before typing make
all-local:: ensure-npmmodules ensure-submodules

NODE_PATH?=$(shell $(MAKE) --no-print-directory -C test node-path)

all-hook:: stage1

install-local::
	@$(MKDIR) $(bindir)
	$(INSTALL) -c ejs.exe $(bindir)/ejs

clean-local::
	@rm -f $(STAGE1_EXE) $(STAGE2_EXE) $(STAGE3_EXE) ejs.exe

TARNAME=$(PRODUCT_name)-$(PRODUCT_VERSION)
TARFILE=$(TARNAME).tar.gz
DISTROOT=$(TOP)
TAR_EXCLUDES=	--exclude .travis.yml \
		--exclude .git \
		--exclude .gitmodules \
		--exclude .gitignore \
		--exclude .deps \
		--exclude host-config.mk \
		--exclude host-config.js \
		--exclude host-config-es6.js \
		--exclude $(TARFILE) \
		--exclude $(TARNAME)
dist-hook:: ensure-submodules
	@echo creating $(DISTROOT)/$(TARNAME).tar.gz
	@rm -rf $(DISTROOT)/$(TARNAME)
	@$(MKDIR) $(DISTROOT)/$(TARNAME)
	@COPYFILE_DISABLE=1 tar -c $(TAR_EXCLUDES) * | tar -C $(DISTROOT)/$(TARNAME) -xp
	@(cd $(DISTROOT); \
	  COPYFILE_DISABLE=1 tar -czf $(TARFILE) $(TARNAME))
	@rm -rf $(DISTROOT)/$(TARNAME)
	@ls -l $(DISTROOT)/$(TARFILE)

check:
	@$(MAKE) -C test $@

check-%:
	@$(MAKE) -C test $@

bootstrap: stage3

MODULE_DIRS = --moduledir $(TOP)/node-compat --moduledir $(TOP)/ejs-llvm

lib/generated:
	@$(MAKE) -C lib

stage1: $(STAGE1_EXE)
	@cp $(STAGE1_EXE) ejs.exe
	@ls -l ejs.exe
	@echo DONE

stage2: $(STAGE2_EXE)
	@cp $(STAGE2_EXE) ejs.exe
	@ls -l ejs.exe
	@echo DONE

stage3: $(STAGE3_EXE)
	@cp $(STAGE3_EXE) ejs.exe
	@ls -l ejs.exe
	@echo DONE

$(STAGE1_EXE): lib/generated
	@echo Building stage 1
	@NODE_PATH="$(NODE_PATH)" ./ejs --srcdir --leave-temp $(MODULE_DIRS) ejs-es6.js
	@mv ejs-es6.js.exe $@

$(STAGE2_EXE): $(STAGE1_EXE) lib/*.js lib/*.js.in
	@echo Building stage 2
	@./$(STAGE1_EXE) --srcdir --leave-temp $(MODULE_DIRS) ejs-es6.js
	@mv ejs-es6.js.exe $@

$(STAGE3_EXE): $(STAGE2_EXE) lib/*.js lib/*.js.in
	@echo Building stage 3
	@./$(STAGE2_EXE) --srcdir --leave-temp $(MODULE_DIRS) ejs-es6.js
	@mv ejs-es6.js.exe $@

echo-command-line:
	@echo ./ejs.exe --leave-temp $(MODULE_DIRS) ejs-es6.js

osx-tarball:
	$(MAKE) -C release osx-tarball

ensure-submodules:
	@if [ ! -f pcre/configure.ac ]; then \
	  git submodule init; \
	  git submodule update; \
	fi

.stamp-npmmodules: package.json
	@npm install && touch $@

ensure-npmmodules: .stamp-npmmodules

include $(TOP)/build/build.mk
