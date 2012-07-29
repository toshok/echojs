TOPDIR=$(shell pwd)/..
NODE_PATH=$(TOPDIR)/node-llvm/build/default:$(TOPDIR)/esprima:$(TOPDIR)/lib

check: foo
	./foo

foo: foo.s ../runtime/main.o ../runtime/libecho.a
	gcc -o foo foo.s ../runtime/main.o -L../runtime -lecho

foo.s: foo.ll
	llc -march=x86-64 -O0 foo.ll
	sed -e s/vmovsd/movsd/ < foo.s > foo.sed.s && mv foo.sed.s foo.s

foo.ll: foo-tmp.ll
	llvm-as < foo-tmp.ll | opt -mem2reg -simplifycfg | llvm-dis > foo.ll

foo-tmp.ll: foo.js
	NODE_PATH=$(NODE_PATH) $(TOPDIR)/ejs -f foo.js #2> foo-tmp.ll

clean:
	rm -f foo foo.sed.s foo.s foo.ll foo-tmp.ll


test-foo:
	NODE_PATH=$(NODE_PATH) $(TOPDIR)/ejs foo.js

run-node:
	NODE_PATH=$(NODE_PATH) node
