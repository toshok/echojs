TOP=..

-include $(TOP)/build/config.mk

.IGNORE: # ignore all errors during test compilation/run so we always get a full run.
.SILENT:

TOPDIR=$(shell pwd)/..
NODE_PATH=$(TOPDIR)/node-llvm/build/Release:$(TOPDIR)/lib/generated:$(TOPDIR)/lib:$(TOPDIR)/esprima:$(TOPDIR)/escodegen:$(TOPDIR)/estraverse
NODE_FLAGS=--harmony --harmony-typeof
TRACEUR_FLAGS=--block-binding true

NODE_COMPAT_MODULES=	--module "../node-compat/libejsnodecompat-module.a,@node-compat/path,_ejs_path_module_func," \
			--module "../node-compat/libejsnodecompat-module.a,@node-compat/os,_ejs_os_module_func," \
			--module "../node-compat/libejsnodecompat-module.a,@node-compat/fs,_ejs_fs_module_func," \
			--module "../node-compat/libejsnodecompat-module.a,@node-compat/child_process,_ejs_child_process_module_func,"

TESTS:=$(wildcard *[0-9].js)

# this test just sucks
TESTS:=$(filter-out alloc1.js,$(TESTS))

# these test for es6 stuff that neither node nor ejs supports
TESTS:=$(filter-out modules2.js,$(TESTS))
TESTS:=$(filter-out iterators1.js,$(TESTS))
TESTS:=$(filter-out proxy4.js,$(TESTS))
# esprima uses the JS regexp ctor while parsing regexps to validate them.  node can't deal with the regexp in this file.
TESTS:=$(filter-out regexp6.js,$(TESTS))

# disable the esprima tests for the time being, as the new module syntax (and losing require) breaks them.
# we need to fork esprima/escodegen/estraverse and port them to es6 modules for these tests.
TESTS:=$(filter-out esprima%,$(TESTS))

ESPRIMA_TESTS=$(filter esprima%,$(TESTS))
ESPRIMA_TESTS:=$(ESPRIMA_TESTS:%.js=%.js.exe)

addExpectedFailureInner=EXPECTED_FAILURES += $(1)
addTraceurTestInner=TRACEUR_TESTS += $(1)
addNodeCompatTestInner=NODE_COMPAT_TESTS += $(1)

define addExpectedFailure
  $(eval $(call addExpectedFailureInner, $(1)))
endef

define testAgainstTraceur
  $(eval $(call addTraceurTestInner, $(1)))
endef

define nodeCompatTest
  $(eval $(call addNodeCompatTestInner, $(1)))
endef

EXPECTED_FAILURES=
$(call addExpectedFailure, argv1.js,                "node passes ['node','argv1.js'], we return ['argv1.js.exe']")
$(call addExpectedFailure, date1.js,                "fails because we test the Date() function, which returns the current time.  this differs from the current time when the node test runs.  bleah.")
$(call addExpectedFailure, date3.js,                "the first date is off by an hour.  timegm/localtime_r screwup?")
$(call addExpectedFailure, forin2.js,               "fails because we don't properly handle deleting properties while iterating")
$(call addExpectedFailure, forin5.js,               "fails because we enumerate the entire range 0..length of an array, including the holes.")
$(call addExpectedFailure, object6.js,              "fails because we don't define everything we should.")
$(call addExpectedFailure, number1.js,              "fails because node outputs {} for console.log(new Number(5)), while SM and JSC output '5'.  we err on the SM/JSC side of things here.")
$(call addExpectedFailure, typedarray0.js,          "we throw an exception when typed array constructors are called as functions.")
$(call addExpectedFailure, typedarray3.js,          "we use %g to format floats/doubles, and differ when printing numbers.")
$(call addExpectedFailure, typedarray6.js,          "we use %g to format floats/doubles, and differ when printing numbers.")
$(call addExpectedFailure, array19.js,              "we don't support property descriptors at all for array elements.  also needs more from ECMA262 15.4.5.1")
$(call addExpectedFailure, fib1.js,                 "we don't output numbers correctly (integers with exponent <= 20 should print out normally)")
$(call addExpectedFailure, const1.js,               "we permit assigning to const bindings")
$(call addExpectedFailure, regexp5.js,              "we don't support for the y flag")
$(call addExpectedFailure, number-toFixed1.js,      "not implemented fully yet")
$(call addExpectedFailure, number-toPrecision1.js,  "not implemented fully yet")
$(call addExpectedFailure, object7.js,              "we differ from node on property descriptor flags.  verify with spec")
$(call addExpectedFailure, object9.js,              "we differ from node on property descriptor flags.  verify with spec")

TRACEUR_TESTS=
$(call testAgainstTraceur, array-subclass1.js)
$(call testAgainstTraceur, class3.js)
$(call testAgainstTraceur, closure3.js)
$(call testAgainstTraceur, for5.js)
$(call testAgainstTraceur, for6.js)
$(call testAgainstTraceur, forof1.js)
$(call testAgainstTraceur, forof2.js)
$(call testAgainstTraceur, computed-props1.js)
$(call testAgainstTraceur, computed-props3.js)
$(call testAgainstTraceur, shorthand-prop1.js)
$(call testAgainstTraceur, shorthand-method1.js)
$(call testAgainstTraceur, spread1.js)
$(call testAgainstTraceur, spread2.js)
$(call testAgainstTraceur, spread3.js)
$(call testAgainstTraceur, spread4.js)
$(call testAgainstTraceur, spread5.js)
$(call testAgainstTraceur, spread6.js)
$(call testAgainstTraceur, spread7.js)
$(call testAgainstTraceur, string-fromCodePoint1.js)
$(call testAgainstTraceur, string-raw1.js)
$(call testAgainstTraceur, symbol-iterator1.js)
$(call testAgainstTraceur, promise1.js)
$(call testAgainstTraceur, promise2.js)

NODE_COMPAT_TESTS=
$(call nodeCompatTest, path-relative1.js)
$(call nodeCompatTest, path-resolve1.js)
$(call nodeCompatTest, stat1.js)

NODE_COMPAT_TESTS_EXES:=$(NODE_COMPAT_TESTS:%.js=%.js.exe)

all-local:: check

check: check-unit-tests run-tests

check-stage1: check-unit-tests run-tests-stage1

check-unit-tests: check-llvm check-set

build-tests: $(patsubst %.js,%.js.exe,$(TESTS))

build-tests-stage1:
	@$(MAKE) EJS_DRIVER="$(TOPDIR)/ejs.js.exe" EJS_STAGE=1 build-tests

check-llvm:
	mocha llvm-test.js

check-set:
	mocha set-test.js

run-tests: clean-results generate-expected prepare-xfail $(patsubst %.js,test-%,$(TESTS)) format-results

run-tests-stage1:
	@$(MAKE) EJS_DRIVER="$(TOPDIR)/ejs.js.exe" EJS_STAGE=1 run-tests

clean-results:
	@rm -f .successes .failures .xfail .xsuccess .expected-failures

write_xfail=echo $(1) >> .expected-failures

generate-expected: $(patsubst %.js,expected/%.js.expected-out,$(TESTS))

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
	failed=false; \
	if test -f .failures; then \
	  echo "Unexpected failures:"; \
	  cat .failures; \
	  echo; \
	  failed=true; \
	fi; \
	if test -f .xsuccess; then \
	  echo "Unexpected successes:"; \
	  cat .xsuccess; \
	  echo; \
	  failed=true; \
	fi; \
	if test "x$$failed" = xtrue; then \
	  exit 1; \
	fi

compile-tests: $(patsubst %.js,%.js.exe,$(TESTS))

test-%: .%.js.ejs-out expected/%.js.expected-out
	-@test_js=`echo $@|sed -e s,test-,,`.js; \
	if [ -z "$V" ]; then /bin/echo -n [stage$(EJS_STAGE)] $$test_js; else echo $(EJS_DRIVER) $$test_js; fi; \
	if cmp -s .$$test_js.ejs-out expected/$$test_js.expected-out ; then \
		if grep -e ^$$test_js .expected-failures /dev/null 2>&1; then \
			echo "   \033[33mXSUCCESS\033[0m"; \
			echo $$test_js >> .xsuccess; \
		else \
			echo "   \033[32mSUCCESS\033[0m"; \
			echo $$test_js >> .successes; \
		fi \
	else \
		if grep -e ^$$test_js .expected-failures > /dev/null 2>&1; then \
			echo "   \033[33mXFAIL\033[0m"; \
			echo $$test_js >> .xfail; \
		else \
			echo "   \033[31mFAILURE\033[0m"; \
			echo $$test_js >> .failures; \
			diff -u .$$test_js.ejs-out expected/$$test_js.expected-out > .$$test_js.diff; \
		fi \
	fi

%.js: %.coffee
	@coffee -c $<

echo-%:
	-@test_js=`echo $@|sed -e s,echo-,,`; \
	/bin/echo -n "$$test_js:   "

.%.js.ejs-out: %.js.exe
	-@EXE=`echo $<`; ./$$EXE > $@

expected/%.js.expected-out: %.js
	@if echo $(TRACEUR_TESTS) | grep $< > /dev/null; then \
		echo Generating expected output for $< using traceur; \
		NODE_PATH=$(NODE_PATH) traceur $(TRACEUR_FLAGS) $< > $@ ; \
	else \
		echo Generating expected output for $< using node; \
		NODE_PATH=$(NODE_PATH) node $(NODE_FLAGS) $< > $@; \
	fi

$(ESPRIMA_TESTS): %.js.exe: %.js $(TOPDIR)/esprima/esprima-es6.js $(TOPDIR)/escodegen/escodegen-es6.js $(TOPDIR)/estraverse/estraverse-es6.js $(TOPDIR)/runtime/libecho.a
	@cp $(TOPDIR)/esprima/esprima-es6.js .
	@cp $(TOPDIR)/escodegen/escodegen-es6.js .
	@cp $(TOPDIR)/estraverse/estraverse-es6.js .
	@echo [ejs stage$(EJS_STAGE)] $< && $(EJS_DRIVER) --leave-temp $<

clean-esprima-roundtrip:
	@rm -f esprima-es6.js escodegen-es6.js estraverse-es6.js

EJS_DRIVER ?= NODE_PATH=$(NODE_PATH) $(TOPDIR)/ejs --leave-temp -q
EJS_STAGE ?= 0

$(NODE_COMPAT_TESTS_EXES): %.js.exe: %.js $(TOPDIR)/runtime/libecho.a
	@$(EJS_DRIVER) $(NODE_COMPAT_MODULES) $< 

%.js.exe: %.js $(TOPDIR)/runtime/libecho.a
	@$(EJS_DRIVER) $< 

v8-%.js.exe: v8/%.js $(TOPDIR)/runtime/libecho.a
	NODE_PATH=$(NODE_PATH) $(TOPDIR)/ejs $(NODE_COMPAT_MODULES) $<

run-node:
	NODE_PATH=$(NODE_PATH) node

node-path:
	echo $(NODE_PATH)

clean: clean-results clean-esprima-roundtrip
	rm -f *.o *.js.exe .*.diff .*-out .failures .successes .xfail .xsuccess
	rm -rf *.dSYM

.PRECIOUS: $(TESTS:%.js=expected/%.js.expected-out)


compare-%:
	-@test_js=`echo $@|sed -e s,compare-,,`.js; \
	$(TOPDIR)/ejs.js.exe.stage0 --leave-temp $$test_js; \
	mv /tmp/$$test_js.1.ll $$test_js.ll.stage0; \
	$(TOPDIR)/ejs.js.exe.stage1 --leave-temp $$test_js; \
	mv /tmp/$$test_js.1.ll $$test_js.ll.stage1; \
	diff -us $$test_js.ll.stage0 $$test_js.ll.stage1; \
	rm $$test_js.ll.stage0 $$test_js.ll.stage1

-include $(TOP)/build/build.mk
