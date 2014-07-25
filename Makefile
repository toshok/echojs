TOP=$(shell pwd)

include $(TOP)/build/config.mk

#SUBDIRS=external-deps node-llvm ejs-llvm lib runtime
SUBDIRS=external-deps node-llvm lib runtime

# run git submodule magic if somebody is antsy and doesn't type the magic incantation before typing make
all-local::
	@if [ ! -f pcre/configure.ac ]; then \
	  git submodule init; \
	  git submodule update; \
	fi

check:
	$(MAKE) -C test check

bootstrap: stage1 stage2

stage1:
	./ejs --warn-on-undeclared --module "./ejs-llvm/libejsllvm-module.a,llvm,_ejs_llvm_init,`llvm-config-3.4 --ldflags --libs`" ejs-es6

stage2:
	./ejs-es6.exe --warn-on-undeclared --module "./ejs-llvm/libejsllvm-module.a,llvm,_ejs_llvm_init,`llvm-config-3.4 --ldflags --libs`" ejs-es6

include $(TOP)/build/build.mk
