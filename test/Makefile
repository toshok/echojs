TOP=..

-include $(TOP)/build/config.mk

.IGNORE: # ignore all errors during test compilation/run so we always get a full run.
.SILENT:

TESTS:=$(wildcard *[0-9].js)
TEST_NAMES:= $(patsubst %.js,%,$(TESTS))
CHECK_TESTS:= $(patsubst %.js,check-%,$(TESTS))

TOPDIR=$(shell pwd)/..
NPM_PREFIX=$(shell npm prefix -g)
NODE_PATH=$(TOPDIR)/node-llvm/build/Release:$(TOPDIR)/lib/generated:$(NPM_PREFIX)/lib/node_modules
NODE_FLAGS=--harmony --harmony-typeof
TRACEUR_FLAGS=--block-binding true

MODULE_DIRS=--moduledir $(TOP)/node-compat --moduledir $(TOP)/ejs-llvm

all-local:: check

check: check-unit-tests check-stage0

check-stage0:
	@NODE_PATH=$(NODE_PATH) PATH=$(PATH):$(TOPDIR)/node_modules/.bin node tester.js -s 0

check-stage1:
	@NODE_PATH=$(NODE_PATH) PATH=$(PATH):$(TOPDIR)/node_modules/.bin node tester.js -s 1

check-stage2:
	@NODE_PATH=$(NODE_PATH) PATH=$(PATH):$(TOPDIR)/node_modules/.bin node tester.js -s 2

check-sim-stage0:
	@NODE_PATH=$(NODE_PATH) PATH=$(PATH):$(TOPDIR)/node_modules/.bin node tester.js -p sim -s 0

check-sim-stage1:
	NODE_PATH=$(NODE_PATH) PATH=$(PATH):$(TOPDIR)/node_modules/.bin node tester.js -p sim -s 1

$(CHECK_TESTS): check-%: %.js
	@NODE_PATH=$(NODE_PATH) PATH=$(PATH):$(TOPDIR)/node_modules/.bin node tester.js -s 0 -t $<

check-unit-tests: check-llvm

build-tests: $(patsubst %.js,%.js.exe,$(TESTS))

build-tests-stage1:
	@$(MAKE) EJS_DRIVER="$(TOPDIR)/ejs.exe" EJS_STAGE=1 build-tests

check-llvm:
	NODE_PATH=$(NODE_PATH) mocha llvm-test.js

run-tests: generate-expected prepare-xfail $(patsubst %.js,test-%,$(TESTS)) format-results

run-tests-stage1:
	@$(MAKE) EJS_DRIVER="$(TOPDIR)/ejs.exe" EJS_STAGE=1 run-tests

$(ESPRIMA_TESTS): %.js.exe: %.js $(TOPDIR)/esprima/esprima-es6.js $(TOPDIR)/escodegen/escodegen-es6.js $(TOPDIR)/estraverse/estraverse-es6.js $(TOPDIR)/runtime/libecho.a
	@cp $(TOPDIR)/esprima/esprima-es6.js .
	@cp $(TOPDIR)/escodegen/escodegen-es6.js .
	@cp $(TOPDIR)/estraverse/estraverse-es6.js .
	@echo [ejs stage$(EJS_STAGE)] $< && $(EJS_DRIVER) $(EJS_DRIVER_ARGS) $<

clean-esprima-roundtrip:
	@rm -f esprima-es6.js escodegen-es6.js estraverse-es6.js

EJS_DRIVER_ARGS ?= --leave-temp -q
EJS_DRIVER ?= NODE_PATH=$(NODE_PATH) $(TOPDIR)/ejs
EJS_STAGE ?= 0

%.js.exe: %.js $(TOPDIR)/runtime/libecho.a
	@$(EJS_DRIVER) $(EJS_DRIVER_ARGS) --srcdir $(MODULE_DIRS) $< 

v8-%.js.exe: v8/%.js $(TOPDIR)/runtime/libecho.a
	NODE_PATH=$(NODE_PATH) $(TOPDIR)/ejs --srcdir $(MODULE_DIRS) $<

run-node:
	NODE_PATH=$(NODE_PATH) node

node-path:
	echo $(NODE_PATH)

clean-local:: clean-esprima-roundtrip
	rm -f *.o *.js.exe .*.diff .*-out .failures .successes .xfail .xsuccess
	rm -rf *.dSYM

.PRECIOUS: $(TESTS:%.js=expected/%.js.expected-out)


compare-%:
	-@test_js=`echo $@|sed -e s,compare-,,`.js; \
	$(TOPDIR)/ejs.js.exe.stage0 $(EJS_DRIVER_ARGS) $$test_js; \
	mv /tmp/$$test_js.1.ll $$test_js.ll.stage0; \
	$(TOPDIR)/ejs.js.exe.stage1 $(EJS_DRIVER_ARGS) $$test_js; \
	mv /tmp/$$test_js.1.ll $$test_js.ll.stage1; \
	diff -us $$test_js.ll.stage0 $$test_js.ll.stage1; \
	rm $$test_js.ll.stage0 $$test_js.ll.stage1

-include $(TOP)/build/build.mk
