
FORMATTED_SRC=$(TOP)/runtime/*.h $(TOP)/runtime/*.c $(TOP)/runtime/*.m
FORMATTED_EJS_LLVM=$(TOP)/ejs-llvm/*.h $(TOP)/ejs-llvm/*.cpp
FORMATTED_NODE_LLVM=$(TOP)/node-llvm/*.h $(TOP)/node-llvm/*.cpp

format:
	clang-format -i $(FORMATTED_SRC) $(FORMATTED_EJS_LLVM) $(FORMATTED_NODE_LLVM)

check-format:
	@for i in $(FORMATTED_SRC) $(FORMATTED_EJS_LLVM) $(FORMATTED_NODE_LLVM); do \
		if (clang-format $$i | diff -q $$i -)>/dev/null; then \
			true; \
		else \
			(clang-format $$i | diff -u $$i -); \
			exit 1; \
		fi \
	done
