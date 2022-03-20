TopDir=$(shell pwd)

export TopDir

ExeTargets=src/ASTLoader src/ASTManager src/sendcmd src/storage src/fs_sh
DllTargets=src/interface src/common src/sysfs src/fsdrv
NCTargets=src/config

ExeTargets:=$(addprefix $(TopDir)/,$(ExeTargets))
DllTargets:=$(addprefix $(TopDir)/,$(DllTargets))
NCTargets:=$(addprefix $(TopDir)/,$(NCTargets))

TargetDir:=$(ExeTargets) $(DllTargets) $(NCTargets)

ExeTargetsName=$(notdir $(ExeTargets))
DllTargetsName=$(notdir $(DllTargets))
NCTargetsName=$(notdir $(NCTargets))

Targets=$(ExeTargetsName) $(DllTargetsName) $(NCTargetsName)
TargetClean:=$(addsuffix _clean,$(Targets))

include $(TopDir)/.config

ifeq ($(CONFIG_X64), true)
CFLAGS+=-DCONFIG_X64
endif
CFLAGS+=-I$(TopDir)/include -I$(TopDir)/include/arch/linux -Os -Wno-write-strings
EXE_CFLAGS+=
DLL_CFLAGS+=-fPIC -fvisibility=hidden

all: $(Targets)
	@echo build successful!

common: interface
sysfs fsdrv: common
sendcmd ASTManager ASTLoader storage fs_sh: sysfs common

$(ExeTargetsName):
	@$(MAKE) -C $(filter %$@,$(ExeTargets)) CFLAGS="$(CFLAGS) $(EXE_CFLAGS)"

$(DllTargetsName):
	@$(MAKE) -C $(filter %$@,$(DllTargets)) CFLAGS="$(CFLAGS) $(DLL_CFLAGS)"

$(NCTargetsName):
	@$(MAKE) -C $(filter %$@,$(NCTargets))

.PHONY: clean
.PHONY: $(TargetClean)

clean: $(TargetClean)
	@$(RM) $(TopDir)/bin/* $(TopDir)/lib/*

$(TargetClean):
	@$(MAKE) -C $(filter %$(patsubst %_clean,%,$@),$(TargetDir)) clean CFLAGS="$(CFLAGS)"
