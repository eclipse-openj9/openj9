<#--
Copyright (c) 1998, 2019 IBM Corp. and others

This program and the accompanying materials are made available under
the terms of the Eclipse Public License 2.0 which accompanies this
distribution and is available at https://www.eclipse.org/legal/epl-2.0/
or the Apache License, Version 2.0 which accompanies this distribution and
is available at https://www.apache.org/licenses/LICENSE-2.0.

This Source Code may also be made available under the following
Secondary Licenses when the conditions for such availability set
forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
General Public License, version 2 with the GNU Classpath
Exception [1] and GNU General Public License, version 2 with the
OpenJDK Assembly Exception [2].

[1] https://www.gnu.org/software/classpath/license.html
[2] http://openjdk.java.net/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->

<#assign lib_target_rule>
$(UMA_LIBTARGET) : $(UMA_OBJECTS)
	$(AR) $(UMA_LIB_LINKER_FLAGS) rcv $(UMA_LIBTARGET) $(UMA_OBJECTS)
	$(RANLIB) $(UMA_LIBTARGET)
</#assign>

<#assign dll_target_rule>
$(UMA_DLLTARGET) : $(UMA_OBJECTS) $(UMA_TARGET_LIBRARIES)
	-rm -f $(UMA_DLLTARGET)
	$(UMA_DLL_LD) $(UMA_DLL_LINK_FLAGS) $(UMA_SYS_LINK_PATH) \
		$(VMLINK) $(UMA_LINK_PATH) -o $(UMA_DLLTARGET) \
		$(UMA_OBJECTS) \
		$(UMA_DLL_LINK_POSTFLAGS)
ifdef j9vm_uma_gnuDebugSymbols
	cp $(UMA_DLLTARGET) $(UMA_DLLTARGET).dbg
endif
	strip -X32_64 -t $(UMA_DLLTARGET)
</#assign>

<#assign exe_target_rule>
$(UMA_EXETARGET) : $(UMA_OBJECTS) $(UMA_TARGET_LIBRARIES)
	$(UMA_EXE_LD) $(UMA_CC_MODE) -brtl $(UMA_SYS_LINK_PATH) $(UMA_LINK_PATH) $(VMLINK) \
		$(UMA_OBJECTS) \
		$(UMA_LINK_LIBRARIES) \
		-o $@ -lm -lpthread -liconv -ldl \
		$(UMA_EXE_LINK_POSTFLAGS)
</#assign>

ifeq ($(j9vm_env_data64),1)
  UMA_ASPP_DEBUG += -g
  UMA_LIB_LINKER_FLAGS += -X64
  UMA_CC_MODE += -q64
else
  UMA_ASPP_DEBUG += $(VMASMDEBUG)
  UMA_LIB_LINKER_FLAGS += -X32
  UMA_CC_MODE += -q32
endif

ifeq ($(j9vm_env_data64),1)
  ASFLAGS += -a64 -many
else
  ASFLAGS += -a32 -mppc
endif
ASFLAGS += -o $@

ifdef UMA_DO_NOT_OPTIMIZE_CCODE
  CFLAGS += -O0
  CXXFLAGS += -O0
else
  CFLAGS += -O3
  CXXFLAGS += -O3
endif

ifdef j9vm_uma_supportsIpv6
  CFLAGS += -DIPv6_FUNCTION_SUPPORT
  CXXFLAGS += -DIPv6_FUNCTION_SUPPORT
  CPPFLAGS += -DIPv6_FUNCTION_SUPPORT
endif

ifdef j9vm_uma_gnuDebugSymbols
  # compile most files with debug information
  CFLAGS += -g
  CXXFLAGS += -g
  # except when compiling interpreters
  FLAGS_TO_REMOVE += -g
endif

ifdef I5_VERSION
  CFLAGS += $(UMA_CC_MODE)
else
  CFLAGS += -s $(UMA_CC_MODE)
endif
CFLAGS += -q mbcs -qlanglvl=extended -qarch=ppc -qinfo=pro -qalias=noansi -qxflag=LTOL:LTOL0 -qsuppress=1506-1108
CFLAGS += -D_XOPEN_SOURCE_EXTENDED=1 -D_ALL_SOURCE -DRS6000 -DAIXPPC -D_LARGE_FILES

ifdef I5_VERSION
  CXXFLAGS += $(UMA_CC_MODE)
else
  CXXFLAGS += -s $(UMA_CC_MODE)
endif
CXXFLAGS += -q mbcs -qlanglvl=extended -qarch=ppc -qinfo=pro -qalias=noansi -qxflag=LTOL:LTOL0 -qsuppress=1506-1108
CXXFLAGS += -D_XOPEN_SOURCE_EXTENDED=1 -D_ALL_SOURCE -DRS6000 -DAIXPPC -D_LARGE_FILES
CPPFLAGS += -D_XOPEN_SOURCE_EXTENDED=1 -D_ALL_SOURCE -DRS6000 -DAIXPPC -D_LARGE_FILES

ifeq ($(j9vm_env_data64),1)
  CFLAGS += -DPPC64
  CXXFLAGS += -DPPC64
  CPPFLAGS += -DPPC64
endif

ifdef I5_VERSION
  I5_FLAGS += -qtbtable=full -qlist -qsource
  I5_DEFINES += -DJ9OS_I5 -DJ9OS_$(I5_VERSION) -I$(UMA_PATH_TO_ROOT)/iseries

  CFLAGS += $(I5_FLAGS) $(I5_DEFINES)
  CXXFLAGS += $(I5_FLAGS) $(I5_DEFINES)
  CPPFLAGS += $(I5_DEFINES)
endif

# For consistency with previous use of makeC++SharedLib_r.
UMA_SYS_LINK_PATH := -L/usr/lib/threads

ifeq ($(j9vm_env_data64),1)
  UMA_DLL_LINK_FLAGS += -q64
else
  UMA_DLL_LINK_FLAGS += -q32
endif
# Link options like '-brtl', '-G', etc. must be prefixed by '-Wl,' to make xlc_r pass them through.
UMA_DLL_LINK_FLAGS += -qmkshrobj -Wl,-brtl -Wl,-G -Wl,-bernotok -Wl,-bnoentry
UMA_DLL_LINK_FLAGS += -Wl,-bmap:$(UMA_TARGET_NAME).map
UMA_DLL_LINK_FLAGS += -Wl,-bE:$(UMA_TARGET_NAME).exp

ifdef I5_VERSION
  UMA_DLL_LINK_FLAGS += -Wl,-bI:$(UMA_PATH_TO_ROOT)/iseries/i5exports.exp
endif

ifdef UMA_AIX_MSG_TEST
  UMA_DLL_LINK_FLAGS += -Wl,-bI:dummy.exp -Wl,-bexpall
endif

UMA_DLL_LINK_POSTFLAGS += $(UMA_LINK_LIBRARIES)
UMA_DLL_LINK_POSTFLAGS += -lc_r -lm -lpthreads
UMA_EXE_LINK_POSTFLAGS += -lc_r -lm -lpthreads

ifdef UMA_IS_C_PLUS_PLUS
  UMA_DLL_LINK_POSTFLAGS += -lC_r -lC
  UMA_EXE_LINK_POSTFLAGS := -lC_r -lC
else
  UMA_EXE_LINK_POSTFLAGS :=
endif

$(patsubst %.s,%.o,$(filter %.s,$(UMA_FILES_TO_PREPROCESS))) : %$(UMA_DOT_O) : %.s
	cc -P $(CPPFLAGS) $*.s
	sed 's/\!/\#/g' $*.i > $*.spp
	aspp $(UMA_ASPP_DEBUG) $*.spp $*.dbg
	$(AS) $(ASFLAGS) $*.dbg
	-rm $*.dbg $*.i $*.spp

ifdef UMA_TREAT_WARNINGS_AS_ERRORS
  ifndef UMA_SUPPRESS_WARNINGS_AS_ERRORS
    CFLAGS += -qhalt=w
    CXXFLAGS += -qhalt=w
  endif
endif
