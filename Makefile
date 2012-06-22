NJS_SHELL=/Users/toshok/src/coffeekit/coffeekit-internal/external-deps/spidermonkey-osx/js
LLVM_CXXFLAGS="`$LLVM_CONFIG --cxxflags` -fno-rtti"
LLVM_LDFLAGS=`$LLVM_CONFIG --ldflags`
LLVM_LIBS=`$LLVM_CONFIG --libs core bitwriter jit x86codegen`
LLVM_LIBS:="$LLVM_LDFLAGS $LLVM_LIBS -lstdc++"

CFLAGS=-I../echo1

all: foo

make-shell:
	$(MAKE) -C shell

make-llvm:
	cd llvm && ./configure --disable-jit
	$(MAKE) -C llvm

foo-tmp.ll: foo.js
	-NJS_SHELL=$(NJS_SHELL) ./ejs -f foo.js 2> foo-tmp.ll

foo.ll: foo-tmp.ll
	llvm-as < foo-tmp.ll | opt -mem2reg -simplifycfg | llvm-dis > foo.ll

foo.s: foo.ll
	llc -march=x86-64 -O0 foo.ll
	sed -e s/vmovsd/movsd/ < foo.s > foo.sed.s && mv foo.sed.s foo.s

foo: foo.s main.o libecho.a
	gcc -o foo foo.s main.o -L. -lecho

clean:
	rm -f foo foo.sed.s foo.s foo.ll foo-tmp.ll main.o


test-foo:
	NJS_SHELL=$(NJS_SHELL) ./ejs -f foo.js