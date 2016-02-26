include $(buildSysPath)/imagineCommonTarget.mk
include $(buildSysPath)/package/stdc++.mk
include $(buildSysPath)/evalPkgConfigCFlags.mk
include $(buildSysPath)/evalPkgConfigLibs.mk

ifeq ($(ENV), android)
 target := lib$(android_soName).so
 LDFLAGS += $(LDFLAGS_SO)
endif

ifdef O_LTO
 LDFLAGS += $(CFLAGS_CODEGEN)
endif

targetFile := $(target)$(targetSuffix)$(targetExtension)

linkerLibsDep := $(linkerLibs)
# TODO: add Xcode lib path to VPATH so next lines aren't needed
linkerLibsDep := $(linkerLibsDep:-lz=)
linkerLibsDep := $(linkerLibsDep:-lm=)

linkerLibsDep := $(linkerLibsDep:-lgcc=)

# standard target
$(targetDir)/$(targetFile) : $(OBJ)
	@echo "Linking $@"
	@mkdir -p `dirname $@`
	$(PRINT_CMD) $(LD) -o $@ $(OBJ) $(LDFLAGS) $(STDCXXLIB)
ifeq ($(ENV), ios)
 ifndef iOSNoCodesign
	@echo "Signing $@"
	$(PRINT_CMD)ldid -S $@
 endif
endif

main: $(targetDir)/$(targetFile)

.PHONY: clean
clean :
	rm -f $(targetDir)/$(targetFile)
	rm -rf $(genPath)
	rm -rf $(objDir)
