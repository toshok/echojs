#.SILENT:    # this makes everything quiet, so we don't really need @'s

TOPDIR=$(shell pwd)/..
NODE_PATH=$(TOPDIR)/node-llvm/build/default:$(TOPDIR)/lib/coffee:$(TOPDIR)/lib:$(TOPDIR)/esprima:$(TOPDIR)/escodegen

TESTS=$(wildcard *[0-9].js)


EXPECTED_FAILURES= \
	date1.js \
	forin2.js \
	func1.js \
	function-overriding1.js \
	iife2.js \
	instanceof1.js

# date1 fails because we test the Date() function, which returns the current time.  this differs from the current time when the node test runs.  bleah.
# forin2 fails because we don't properly handle deleting properties while iterating
# func1 fails because we can't print out closures at the moment (which should really be renamed to JS's Function objects)
# iife2 fails because of number formatting and/or arithmetic
# instanceof1 fails because _ejs_op_instanceof isn't implemented and just aborts
# function-overriding1.js fail because we don't throw an exception on a null function, we just crash.


check: check-unit-tests run-tests

check-unit-tests: check-llvm check-set check-free

check-llvm: terminal.js echo-assert.js
	@NODE_PATH=$(NODE_PATH):. coffee llvm-test.coffee

check-set: terminal.js echo-assert.js
	@NODE_PATH=$(NODE_PATH):. coffee set-test.coffee

check-free: terminal.js echo-assert.js
	@NODE_PATH=$(NODE_PATH):. coffee free-test.coffee

run-tests: clean-results prepare-xfail $(patsubst %.js,test-%,$(TESTS)) format-results

clean-results:
	@rm -f .successes .failures .xfail .xsuccess .expected-failures

write_xfail=echo $(1) >> .expected-failures

prepare-xfail:
	@$(foreach xfail,$(EXPECTED_FAILURES),$(call write_xfail,$(xfail));)

format-results:
	@success_count=`cat .successes 2>/dev/null| wc -l | sed -e 's, ,,g'`; \
	failure_count=`cat .failures 2>/dev/null| wc -l | sed -e 's, ,,g'`; \
	xfail_count=`cat .xfail 2>/dev/null| wc -l | sed -e 's, ,,g'`; \
	xsuccess_count=`cat .xsuccess 2>/dev/null| wc -l | sed -e 's, ,,g'`; \
	echo "$$success_count expected successes, $$xsuccess_count unexpected"; \
	echo "$$failure_count unexpected failures, $$xfail_count expected"

compile-tests: $(patsubst %.js,%.js.exe,$(TESTS))

test-%: echo-% %.ejs-out %.node-out
	-@test_js=`echo $@|sed -e s,test-,,`; \
	if cmp -s $$test_js.ejs-out $$test_js.node-out ; then \
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
			diff $$test_js.ejs-out $$test_js.node-out > $$test_js.diff; \
		fi \
	fi

%.js: %.coffee
	@coffee -c $<

echo-%:
	-@test_js=`echo $@|sed -e s,echo-,,`; \
	/bin/echo -n "$$test_js:   "

%.ejs-out: %.js.exe
	-@EXE=`echo $<`; ./$$EXE > $@

%.node-out: %.js
	@node $< > $@

%.js.exe: %.js $(TOPDIR)/runtime/libecho.a
	-@NODE_PATH=$(NODE_PATH) $(TOPDIR)/ejs $<

v8-%.js.exe: v8/%.js $(TOPDIR)/runtime/libecho.a
	NODE_PATH=$(NODE_PATH) $(TOPDIR)/ejs $<

run-node:
	NODE_PATH=$(NODE_PATH) node

clean: clean-results
	rm -f *.o *.js.exe terminal.js echo-assert.js *.diff

.INTERMEDIATE: terminal.js echo-assert.js
