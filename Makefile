LLVM_CXXFLAGS="`$LLVM_CONFIG --cxxflags` -fno-rtti"
LLVM_LDFLAGS=`$LLVM_CONFIG --ldflags`
LLVM_LIBS=`$LLVM_CONFIG --libs core bitwriter jit x86codegen`
LLVM_LIBS:="$LLVM_LDFLAGS $LLVM_LIBS -lstdc++"

make-shell:
	$(MAKE) -C shell

make-llvm:
	cd llvm && ./configure --disable-jit
	$(MAKE) -C llvm


test: foo.js main.o libecho.a
	-./ejs -f foo.js 2> foo-tmp.ll
	llvm-as < foo-tmp.ll | opt -mem2reg -simplifycfg | llvm-dis > foo.ll
	llc -march=x86-64 -O0 foo.ll
	sed -e s/vmovsd/movsd/ < foo.s > foo.sed.s
	gcc -o test foo.sed.s main.o -lecho
