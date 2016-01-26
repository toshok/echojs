TOP=..
include $(TOP)/build/config.mk

PWD:=$(shell pwd)

.NOTPARALLEL:

#INSTALLKBYTES:=$(shell du -sk $(DIST_ROOT) | cut -f 1)
#NUMFILES=$(shell find $(DIST_ROOT) | wc -l)

# arg1 = input directory
# arg2 = output path
define make_archive
	@echo [ARCHIVE] `basename $2`
	@(cd $1; find . -print0 | pax -w -0 -x cpio 2>/dev/null | gzip -9 > $2)
endef

# arg1 = input directory
# arg2 = output path
define mkbom
	@echo [MKBOM] $2
	@mkbom $1 $2
endef

# arg1 = packagename-version
define mkpkg
	@echo [PKG] $1.pkg
	@(cd pkgtmp; xar -z --no-compress .*Payload --no-compress .*Scripts -c -f ../$1.pkg * && echo made $(TOP)/packaging/$1.pkg)
endef

release:
	@rm -rf pkgtmp Scripts
	@mkdir -p pkgtmp/Resources/en.lproj
	@mkdir -p pkgtmp/$(PRODUCT_name).pkg
	$(call rewrite, package-template/Resources/en.lproj/Readme.rtf.in, pkgtmp/Resources/en.lproj/Readme)
	$(call rewrite, package-template/Resources/en.lproj/License.rtf.in, pkgtmp/Resources/en.lproj/License)
	$(call rewrite, package-template/Distribution.in, pkgtmp/Distribution)
	$(call rewrite, package-template/product.pkg/PackageInfo.in, pkgtmp/$(PRODUCT_name).pkg/PackageInfo)
	$(call mkbom,$(DIST_ROOT),pkgtmp/$(PRODUCT_name).pkg/Bom)
	@mkdir Scripts
	$(call rewrite, package-template/Scripts/postinstall.in, Scripts/postinstall)
	@chmod a+x Scripts/postinstall
	$(call make_archive,Scripts,$(PWD)/pkgtmp/$(PRODUCT_name).pkg/Scripts)
	@rm -r Scripts
	$(call make_archive,$(DIST_ROOT),$(PWD)/pkgtmp/$(PRODUCT_name).pkg/Payload)
	$(call mkpkg,$(PRODUCT_NAME)-$(PRODUCT_VERSION))

npmtmp=npm-tmp
bindir=$(npmtmp)/bin
libdir=$(npmtmp)/lib
includedir=$(npmtmp)/include
libarchdir_osx=$(npmtmp)/lib/x86_64-darwin
libarchdir_sim=$(npmtmp)/lib/x86-darwin
libarchdir_dev=$(npmtmp)/lib/arm-darwin
npm-release-dist:
	@rm -rf $(npmtmp)
	@rm -f $(npmtmp)/package.json
	@mkdir -p $(bindir)
	@mkdir -p $(libdir)
	@mkdir -p $(libarchdir_osx)
	@mkdir -p $(libarchdir_sim)
	@mkdir -p $(libarchdir_dev)
	@mkdir -p $(npmtmp)/include
	$(call rewrite, npm/package.json.in, $(npmtmp)/package.json)
	@cp $(TOP)/ejs.exe $(bindir)/ejs
	@cp $(TOP)/runtime/*.h $(includedir)
	@cp $(TOP)/runtime/libecho.a $(libarchdir_osx)
	@cp $(TOP)/external-deps/pcre-osx/.libs/libpcre16.a $(libarchdir_osx)
	@cp $(TOP)/external-deps/double-conversion-osx/double-conversion/libdouble-conversion.a $(libarchdir_osx)
	@cp $(TOP)/runtime/libecho.a.ios $(libarchdir_sim)
	@cp $(TOP)/external-deps/pcre-iossim/.libs/libpcre16.a $(libarchdir_sim)
	@cp $(TOP)/external-deps/double-conversion-iossim/double-conversion/libdouble-conversion.a $(libarchdir_sim)
	@cp $(TOP)/runtime/libecho.a.ios $(libarchdir_dev)
	@cp $(TOP)/external-deps/pcre-iosdev/.libs/libpcre16.a $(libarchdir_dev)
	@cp $(TOP)/external-deps/double-conversion-iosdev/double-conversion/libdouble-conversion.a $(libarchdir_dev)
	@cp $(TOP)/modules/objc_internal/objc_internal.ejs $(libdir)
	@cp $(TOP)/node-compat/node-compat.ejs $(libdir)
	@cp $(TOP)/node-compat/libejsnodecompat-module.a $(libarchdir_osx)
	@cp $(TOP)/node-compat/libejsnodecompat-module.a.sim $(libarchdir_sim)
	@cp $(TOP)/node-compat/libejsnodecompat-module.a.armv7 $(libarchdir_dev)




# gross, but for now strip the .a's we've installed so that we aren't passing debug/local symbols around...
dist-local::
	strip -S -x -X $(DIST_ROOT)/usr/lib/$(LIBCOFFEEKIT_A)
	strip -S -x -X $(DIST_ROOT)/usr/lib/libjs_static.a

clean-local::
	@rm -rf pkgtmp Scripts

include $(TOP)/build/build.mk
