TOP=../..

-include $(TOP)/build/config.mk

EJS_DRIVER?=$(TOP)/ejs

fetch: fetch.js
	$(EJS_DRIVER) --srcdir -o $@ $<

all-local:: fetch

-include $(TOP)/build/build.mk
