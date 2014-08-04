TOP=$(shell pwd)

include $(TOP)/build/config.mk

SUBDIRS=external-deps node-compat node-llvm ejs-llvm lib runtime

# run git submodule magic if somebody is antsy and doesn't type the magic incantation before typing make
all-local::
	@if [ ! -f pcre/configure.ac ]; then \
	  git submodule init; \
	  git submodule update; \
	fi

check:
	$(MAKE) -C test check

bootstrap: stage1 stage2

NODE_COMPAT_MODULES = --module "./node-compat/libejsnodecompat-module.a,@node-compat/path,_ejs_path_module_func," \
		      --module "./node-compat/libejsnodecompat-module.a,@node-compat/os,_ejs_os_module_func," \
		      --module "./node-compat/libejsnodecompat-module.a,@node-compat/fs,_ejs_fs_module_func," \
		      --module "./node-compat/libejsnodecompat-module.a,@node-compat/child_process,_ejs_child_process_module_func,"

LLVM_MODULE = --module "./ejs-llvm/libejsllvm-module.a,llvm,_ejs_llvm_init,`llvm-config-3.4 --ldflags --libs`"

stage1:
	./ejs --warn-on-undeclared $(LLVM_MODULE) $(NODE_COMPAT_MODULES) ejs-es6.js

stage2:
	./ejs-es6.js.exe --warn-on-undeclared $(LLVM_MODULE) $(NODE_COMPAT_MODULES) ejs-es6.js

include $(TOP)/build/build.mk
