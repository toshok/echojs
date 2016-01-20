replace=-e "s,@$1@,$($1),g"

dosed=sed $(call replace,ORGANIZATION) \
	  $(call replace,PRODUCT_RELEASE_NOTES_URL) \
	  $(call replace,PRODUCT_VERSION) \
	  $(call replace,PRODUCT_INSTALL_ROOT) \
	  $(call replace,PRODUCT_NAME) \
	  $(call replace,PRODUCT_UTI) \
	  $(call replace,PRODUCT_GITHUB_URL) \
	  $(call replace,PRODUCT_EMAIL) \
	  $(call replace,PRODUCT_name) \
	  $(call replace,INSTALLKBYTES) \
	  $(call replace,NUMFILES)

# arg1 = input path
# arg2 = output path
define rewrite
	@echo [GEN] $2
	@$(dosed) < $1 > $2
endef

