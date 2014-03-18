TOP=..
CURRENT=$(TOP)/lib

include $(CURRENT)/files.mk

DESTDIR = generated

GENERATED_FILES=$(COFFEE_SOURCES:%.coffee=$(DESTDIR)/%.js) $(JS_SOURCES:%.js=$(DESTDIR)/%.js)

all-local:: $(GENERATED_FILES)

clean-local::
	rm -f $(GENERATED_FILES)

$(DESTDIR)/%.js: %.coffee
	coffee -m -o $(DESTDIR) -c $<

$(DESTDIR)/%.js: %.js
	cp $< $@

include $(TOP)/build/build.mk
