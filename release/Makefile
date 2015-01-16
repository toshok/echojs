TOP=..

include $(TOP)/build/config.mk

TARBALL_DIR=$(PRODUCT_name)-$(PRODUCT_VERSION)
libdir=$(TARBALL_DIR)/lib
archlibdir=$(TARBALL_DIR)/lib/darwin-x86-64
includedir=$(TARBALL_DIR)/include/runtime
bindir=$(TARBALL_DIR)/bin
sampledir=$(TARBALL_DIR)/samples

osx-tarball: osx-tarball-deps
	rm -f $(TARBALL_DIR).tar.bz2
	rm -rf $(TARBALL_DIR)
	mkdir -p $(bindir)
	mkdir -p $(archlibdir)
	mkdir -p $(includedir)
	mkdir -p $(sampledir)
	cp release-readme.md $(TARBALL_DIR)/README.md
	cp $(TOP)/ejs.exe $(bindir)/ejs
	cp $(TOP)/runtime/*.h $(includedir)
	cp $(TOP)/runtime/libecho.a $(archlibdir)
	cp $(TOP)/external-deps/pcre-osx/.libs/libpcre16.a $(archlibdir)
	cp $(TOP)/node-compat/node-compat.ejs $(libdir)
	cp $(TOP)/node-compat/libejsnodecompat-module.a $(archlibdir)
	cp $(TOP)/ejs-llvm/ejs-llvm.ejs $(libdir)
	cp $(TOP)/ejs-llvm/libejsllvm-module.a $(archlibdir)
	cp $(TOP)/test/fetch.js $(sampledir)
	tar -cvzf $(TARBALL_DIR).tar.gz $(TARBALL_DIR)

osx-tarball-deps: release-readme.md
	$(MAKE) -C .. all bootstrap

release-readme.md: release-readme.md.in
	@echo [gen] $< && sed -e "s,@PRODUCT_VERSION@,$(PRODUCT_VERSION)," $< > $@

include $(TOP)/build/build.mk
