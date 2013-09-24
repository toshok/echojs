.IGNORE: # ignore all errors during test compilation/run so we always get a full run.
.SILENT:

TOPDIR=$(shell pwd)/..
NODE_PATH=$(TOPDIR)/node-llvm/build/Release:$(TOPDIR)/lib/generated:$(TOPDIR)/lib:$(TOPDIR)/esprima:$(TOPDIR)/escodegen:$(TOPDIR)/estraverse
NODE_FLAGS=--harmony --harmony-typeof

TESTS:=$(wildcard *[0-9].js)

# this test just sucks
TESTS:=$(filter-out alloc1.js,$(TESTS))

# these test for es6 stuff that neither node nor ejs supports
TESTS:=$(filter-out destructure1.js,$(TESTS))
TESTS:=$(filter-out module1.js,$(TESTS))

ESPRIMA_TESTS=$(filter esprima%,$(TESTS))
ESPRIMA_TESTS:=$(ESPRIMA_TESTS:%.js=%.js.exe)

addExpectedFailureInner=EXPECTED_FAILURES += $(1)

define addExpectedFailure
  $(eval $(call addExpectedFailureInner, $(1)))
endef


EXPECTED_FAILURES=
$(call addExpectedFailure, argv1.js,                "node passes ['node','argv1.js'], we return ['argv1.js.exe']")
$(call addExpectedFailure, date1.js,                "fails because we test the Date() function, which returns the current time.  this differs from the current time when the node test runs.  bleah.")
$(call addExpectedFailure, forin2.js,               "fails because we don't properly handle deleting properties while iterating")
$(call addExpectedFailure, forin5.js,               "fails because we enumerate the entire range 0..length of an array, including the holes.")
$(call addExpectedFailure, json4.js,                "fails due to properties being serialized in a different order.")
$(call addExpectedFailure, object6.js,              "fails because we don't define everything we should.")
$(call addExpectedFailure, number1.js,              "fails because node outputs {} for console.log(new Number(5)), while spidermonkey and jsc output '5'.  we err on the SM/JSC side of things here.")
$(call addExpectedFailure, typedarray3.js,          "we use %g to format floats/doubles, and differ when printing numbers.")
$(call addExpectedFailure, array19.js,              "we don't support property descriptors at all for array elements.  also needs more from ECMA262 15.4.5.1")
$(call addExpectedFailure, sparsearray1.js,         "sparse array support is very much not there yet")
$(call addExpectedFailure, fib1.js,                 "we don't output numbers correctly (integers with exponent <= 20 should print out normally)")
$(call addExpectedFailure, const1.js,               "we permit assigning to const bindings")

check: check-unit-tests run-tests

check-stage1: check-unit-tests run-tests-stage1

check-unit-tests: check-llvm check-set #check-free

build-tests: $(patsubst %.js,%.js.exe,$(TESTS))

build-tests-stage1:
	@$(MAKE) EJS_DRIVER="$(TOPDIR)/ejs.js.exe" EJS_STAGE=1 build-tests

check-llvm: terminal.js echo-assert.js
	@NODE_PATH=$(NODE_PATH):. coffee llvm-test.coffee

check-set: terminal.js echo-assert.js
	@NODE_PATH=$(NODE_PATH):. coffee set-test.coffee

check-free: terminal.js echo-assert.js
	@NODE_PATH=$(NODE_PATH):. coffee free-test.coffee

run-tests: clean-results prepare-xfail $(patsubst %.js,test-%,$(TESTS)) format-results

run-tests-stage1:
	@$(MAKE) EJS_DRIVER="$(TOPDIR)/ejs.js.exe" EJS_STAGE=1 run-tests

clean-results:
	@rm -f .successes .failures .xfail .xsuccess .expected-failures

write_xfail=echo $(1) >> .expected-failures

prepare-xfail:
	@cat /dev/null > .expected-failures
	@$(foreach xfail,$(EXPECTED_FAILURES),$(call write_xfail,$(xfail));)

format-results:
	@success_count=`cat .successes 2>/dev/null| wc -l | sed -e 's, ,,g'`; \
	failure_count=`cat .failures 2>/dev/null| wc -l | sed -e 's, ,,g'`; \
	xfail_count=`cat .xfail 2>/dev/null| wc -l | sed -e 's, ,,g'`; \
	xsuccess_count=`cat .xsuccess 2>/dev/null| wc -l | sed -e 's, ,,g'`; \
	echo "$$xsuccess_count unexpected successes, $$success_count expected"; \
	echo "$$failure_count unexpected failures, $$xfail_count expected"; \
	echo "============"; \
	if test -f .failures; then \
	  echo "Unexpected failures:"; \
	  cat .failures; \
	  echo; \
	fi; \
	if test -f .xsuccess; then \
	  echo "Unexpected successes:"; \
	  cat .xsuccess; \
	  echo; \
	fi

compile-tests: $(patsubst %.js,%.js.exe,$(TESTS))

test-%: echo-% .%.js.ejs-out %.js.expected-out
	-@test_js=`echo $@|sed -e s,test-,,`.js; \
	if cmp -s .$$test_js.ejs-out $$test_js.expected-out ; then \
		if grep -e ^$$test_js .expected-failures /dev/null 2>&1; then \
			echo "\033[33mXSUCCESS\033[0m"; \
			echo $$test_js >> .xsuccess; \
		else \
			echo "\033[32mSUCCESS\033[0m"; \
			echo $$test_js >> .successes; \
		fi \
	else \
		if grep -e ^$$test_js .expected-failures > /dev/null 2>&1; then \
			echo "\033[33mXFAIL\033[0m"; \
			echo $$test_js >> .xfail; \
		else \
			echo "\033[31mFAILURE\033[0m"; \
			echo $$test_js >> .failures; \
			diff -u .$$test_js.ejs-out $$test_js.expected-out > .$$test_js.diff; \
		fi \
	fi

%.js: %.coffee
	@coffee -c $<

echo-%:
	-@test_js=`echo $@|sed -e s,echo-,,`; \
	/bin/echo -n "$$test_js:   "

.%.js.ejs-out: %.js.exe
	-@EXE=`echo $<`; ./$$EXE > $@

%.js.expected-out: %.js
	@NODE_PATH=$(NODE_PATH) node $(NODE_FLAGS) $< > $@

$(ESPRIMA_TESTS): %.js.exe: %.js $(TOPDIR)/esprima/esprima.js $(TOPDIR)/escodegen/escodegen.js $(TOPDIR)/estraverse/estraverse.js $(TOPDIR)/runtime/libecho.a
	@cp $(TOPDIR)/esprima/esprima.js .
	@cp $(TOPDIR)/escodegen/escodegen.js .
	@cp $(TOPDIR)/estraverse/estraverse.js .
	@echo [ejs stage$(EJS_STAGE)] $< && $(EJS_DRIVER) --leave-temp $< esprima.js escodegen.js estraverse.js

clean-esprima-roundtrip:
	@rm -f esprima.js escodegen.js estraverse.js

EJS_DRIVER ?= NODE_PATH=$(NODE_PATH) $(TOPDIR)/ejs --leave-temp
EJS_STAGE ?= 0

%.js.exe: %.js $(TOPDIR)/runtime/libecho.a
	@if [ -z "$V" ]; then echo [ejs stage$(EJS_STAGE)] $<; else echo $(EJS_DRIVER) $<; fi
	@$(EJS_DRIVER) $<

v8-%.js.exe: v8/%.js $(TOPDIR)/runtime/libecho.a
	NODE_PATH=$(NODE_PATH) $(TOPDIR)/ejs $<

run-node:
	NODE_PATH=$(NODE_PATH) node

node-path:
	echo $(NODE_PATH)

clean: clean-results clean-esprima-roundtrip
	rm -f *.o *.js.exe terminal.js echo-assert.js .*.diff .*-out .failures .successes .xfail .xsuccess

.PRECIOUS: $(TESTS:%.js=%.js.expected-out)


.INTERMEDIATE: terminal.js echo-assert.js

compare-%:
	-@test_js=`echo $@|sed -e s,compare-,,`.js; \
	$(TOPDIR)/ejs.js.exe.stage0 --leave-temp $$test_js; \
	mv /tmp/$$test_js.1.ll $$test_js.ll.stage0; \
	$(TOPDIR)/ejs.js.exe.stage1 --leave-temp $$test_js; \
	mv /tmp/$$test_js.1.ll $$test_js.ll.stage1; \
	diff -us $$test_js.ll.stage0 $$test_js.ll.stage1; \
	rm $$test_js.ll.stage0 $$test_js.ll.stage1
