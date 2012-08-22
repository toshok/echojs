TOPDIR=$(shell pwd)/../..
NODE_PATH=$(TOPDIR)/node-llvm/build/default:$(TOPDIR)/lib/coffee:$(TOPDIR)/lib:$(TOPDIR)/esprima:$(TOPDIR)/escodegen

check-require.js.exe: check-require.js require1.js require2.js
	NODE_PATH=$(NODE_PATH) $(TOPDIR)/ejs check-require.js require1.js require2.js

clean:
	rm -f *.js.exe
