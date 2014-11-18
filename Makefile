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

bootstrap: stage3

NODE_COMPAT_MODULES = --module "./node-compat/libejsnodecompat-module.a,@node-compat/path,_ejs_path_module_func," \
		      --module "./node-compat/libejsnodecompat-module.a,@node-compat/os,_ejs_os_module_func," \
		      --module "./node-compat/libejsnodecompat-module.a,@node-compat/fs,_ejs_fs_module_func," \
		      --module "./node-compat/libejsnodecompat-module.a,@node-compat/child_process,_ejs_child_process_module_func,"

LLVM_MODULE = --module "./ejs-llvm/libejsllvm-module.a,llvm,_ejs_llvm_init,`llvm-config-3.4 --ldflags --libs`"

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
	@time ./ejs --leave-temp $(LLVM_MODULE) $(NODE_COMPAT_MODULES) ejs-es6.js
	@mv ejs-es6.js.exe ejs-es6.js.exe.stage1

ejs-es6.js.exe.stage2: ejs-es6.js.exe.stage1
	@echo Building stage 2
	@time ./ejs-es6.js.exe.stage1 --leave-temp $(LLVM_MODULE) $(NODE_COMPAT_MODULES) ejs-es6.js
	@mv ejs-es6.js.exe ejs-es6.js.exe.stage2

ejs-es6.js.exe.stage3: ejs-es6.js.exe.stage2
	@echo Building stage 3
	@time ./ejs-es6.js.exe.stage2 --leave-temp $(LLVM_MODULE) $(NODE_COMPAT_MODULES) ejs-es6.js
	@mv ejs-es6.js.exe ejs-es6.js.exe.stage3

echo-command-line:
	@echo ./ejs.exe --leave-temp $(LLVM_MODULE) $(NODE_COMPAT_MODULES) ejs-es6.js

include $(TOP)/build/build.mk
