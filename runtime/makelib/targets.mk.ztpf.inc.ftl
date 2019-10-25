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

<#if uma.spec.properties.uma_crossCompilerPath.defined>
# Put the tools on the path.
PATH := ${uma.spec.properties.crossCompilerPath.value}:<#noparse>${PATH}</#noparse>
</#if>

<#assign lib_target_rule>
$(UMA_LIBTARGET) : $(UMA_OBJECTS)
	$(AR) rcv $(UMA_LIBTARGET) $(UMA_OBJECTS)
</#assign>

<#assign dll_target_rule>
$(UMA_DLLTARGET) : $(UMA_OBJECTS) $(UMA_TARGET_LIBRARIES)
	$(UMA_DLL_LD) $(UMA_DLL_LINK_FLAGS) \
		$(VMLINK) $(UMA_LINK_PATH) -o $(UMA_DLLTARGET)\
		$(UMA_OBJECTS) \
		$(UMA_DLL_LINK_POSTFLAGS)
ifdef j9vm_uma_gnuDebugSymbols
	cp $(UMA_DLLTARGET) $(UMA_DLLTARGET).dbg
	objcopy --strip-debug $(UMA_DLLTARGET)
	objcopy --add-gnu-debuglink=$(UMA_DLLTARGET).dbg $(UMA_DLLTARGET)
endif
</#assign>

UMA_BEGIN_DASH_L=-Xlinker --start-group
UMA_END_DASH_L=-Xlinker --end-group

<#assign exe_target_rule>
$(UMA_EXETARGET) : $(UMA_OBJECTS) $(UMA_TARGET_LIBRARIES)
	$(UMA_EXE_LD) $(UMA_EXE_PREFIX_FLAGS) $(UMA_LINK_PATH) $(VMLINK) \
	$(UMA_OBJECTS) \
	$(word 1,$(wildcard $(foreach d,$(TPF_ROOT),$d/base/obj/crt0.o))) \
	$(UMA_BEGIN_DASH_L) \
	$(UMA_LINK_STATIC_LIBRARIES) \
	$(UMA_END_DASH_L) \
	$(UMA_LINK_SHARED_LIBRARIES) \
	-o $@ $(UMA_EXE_POSTFIX_FLAGS)
</#assign>

TPF_ROOT ?= /ztpf/java/bld/jvm/userfiles /zbld/svtcur/gnu/all /ztpf/commit

ifndef j9vm_env_data64
  J9M31 = -m31
endif
UMA_EXE_PREFIX_FLAGS += $(J9M31)

ifndef UMA_DO_NOT_OPTIMIZE_CCODE
  <#if uma.spec.properties.uma_optimization_cflags.defined>
    UMA_OPTIMIZATION_CFLAGS += ${uma.spec.properties.uma_optimization_cflags.value}
  <#else>
    UMA_OPTIMIZATION_CFLAGS += -O3 -march=z10 -mtune=z9-109
  </#if>
  <#if uma.spec.properties.uma_optimization_cxxflags.defined>
    UMA_OPTIMIZATION_CXXFLAGS += ${uma.spec.properties.uma_optimization_cxxflags.value}
  <#else>
    UMA_OPTIMIZATION_CXXFLAGS += -O3 -march=z10 -mtune=z9-109
  </#if>
else
  UMA_OPTIMIZATION_CFLAGS += -O0
  UMA_OPTIMIZATION_CXXFLAGS += -O0
endif

CFLAGS += $(UMA_OPTIMIZATION_CFLAGS)
CXXFLAGS += $(UMA_OPTIMIZATION_CXXFLAGS)

CXXFLAGS += -fno-exceptions -fno-rtti -fno-threadsafe-statics

<#if uma.spec.processor.s390>
  ifndef j9vm_env_data64
    ASFLAGS += -mzarch
  endif
  ASFLAGS += -march=z10 $(J9M31) $(VMASMDEBUG) -o $*.o
</#if>

ifdef j9vm_uma_gnuDebugSymbols
  CFLAGS += -g
  CXXFLAGS += -g
endif

CFLAGS += -DLINUX -D_REENTRANT
# -D_FILE_OFFSET_BITS=64
CXXFLAGS += -DLINUX -D_REENTRANT
# -D_FILE_OFFSET_BITS=64
CPPFLAGS += -DLINUX -D_REENTRANT

<#-- Add Position Independent compile flag -->
CFLAGS += -fPIC
CXXFLAGS += -fPIC

ifdef j9vm_uma_supportsIpv6
  CFLAGS +=
  CXXFLAGS +=
  CPPFLAGS +=
endif

CFLAGS += $(J9M31) -DS390 -D_LONG_LONG -DJ9VM_TIERED_CODE_CACHE -DIBMLOCKS -fno-strict-aliasing
CXXFLAGS += $(J9M31) -DS390 -D_LONG_LONG -DJ9VM_TIERED_CODE_CACHE -DIBMLOCKS -fno-strict-aliasing
CPPFLAGS += -DS390 -D_LONG_LONG -DJ9VM_TIERED_CODE_CACHE -DIBMLOCKS
ifdef j9vm_env_data64
  CFLAGS += -DS39064
  CXXFLAGS += -DS39064
  CPPFLAGS += -DS39064
endif

UMA_DLL_LINK_FLAGS += -shared
ifdef UMA_USING_LD_TO_LINK
  UMA_DLL_LINK_FLAGS+=-Map $(UMA_TARGET_NAME).map
  UMA_DLL_LINK_FLAGS+=--version-script $(UMA_TARGET_NAME).exp
  UMA_DLL_LINK_FLAGS+=-soname=$(UMA_DLLFILENAME)
  UMA_DLL_LINK_FLAGS+=-z origin -rpath \$$ORIGIN --disable-new-dtags
else
  UMA_DLL_LINK_FLAGS+=-Wl,-Map=$(UMA_TARGET_NAME).map
  UMA_DLL_LINK_FLAGS+=-Wl,--version-script,$(UMA_TARGET_NAME).exp
  UMA_DLL_LINK_FLAGS+=-Wl,-soname=$(UMA_DLLFILENAME)
  UMA_DLL_LINK_FLAGS+=-Xlinker -z -Xlinker origin -Xlinker -rpath -Xlinker \$$ORIGIN -Xlinker --disable-new-dtags
  UMA_DLL_LINK_FLAGS+=-Wl,-entry=0
  UMA_DLL_LINK_FLAGS+=-Wl,-script=$(word 1,$(wildcard $(foreach d,$(TPF_ROOT),$d/base/util/tools/tpfscript)))
  UMA_DLL_LINK_FLAGS+=-Wl,--as-needed
  UMA_DLL_LINK_FLAGS+=-Wl,--eh-frame-hdr
endif

ifdef UMA_USING_LD_TO_LINK
  UMA_DLL_LINK_POSTFLAGS+=--start-group
else
  UMA_DLL_LINK_POSTFLAGS+=-Xlinker --start-group
endif

UMA_DLL_LINK_POSTFLAGS+=$(UMA_LINK_STATIC_LIBRARIES)
ifdef UMA_USING_LD_TO_LINK
  UMA_DLL_LINK_POSTFLAGS+=--end-group
else
  UMA_DLL_LINK_POSTFLAGS+=-Xlinker --end-group
endif

UMA_DLL_LINK_POSTFLAGS+=$(UMA_LINK_SHARED_LIBRARIES)

# CTIS needs to be linked before CISO from sysroot for gettimeofday
UMA_DLL_LINK_POSTFLAGS+=-lgcc
UMA_DLL_LINK_POSTFLAGS+=-lCTOE
UMA_DLL_LINK_POSTFLAGS+=-lCTIS

ifdef j9vm_uma_gnuDebugSymbols
  UMA_DLL_LINK_POSTFLAGS += -g
endif

UMA_DLL_LINK_POSTFLAGS += -Xlinker -z -Xlinker origin
UMA_DLL_LINK_POSTFLAGS += -Xlinker -rpath -Xlinker \$$ORIGIN -Xlinker -disable-new-dtags
UMA_DLL_LINK_POSTFLAGS += -Xlinker -rpath-link -Xlinker $(UMA_PATH_TO_ROOT)

UMA_EXE_PREFIX_FLAGS += -Wl,-Map=$(UMA_TARGET_NAME).map
UMA_EXE_POSTFIX_FLAGS += -Wl,-entry=_start
UMA_EXE_POSTFIX_FLAGS += -Wl,-script=$(word 1,$(wildcard $(foreach d,$(TPF_ROOT),$d/base/util/tools/tpfscript)))
UMA_EXE_POSTFIX_FLAGS += -Wl,-soname=$(UMA_TARGET_NAME)
UMA_EXE_POSTFIX_FLAGS += -Wl,--as-needed
UMA_EXE_POSTFIX_FLAGS += -Wl,--eh-frame-hdr
UMA_EXE_POSTFIX_FLAGS += -lgcc
UMA_EXE_POSTFIX_FLAGS += -lCTOE

ifdef j9vm_jit_32bitUses64bitRegisters
  UMA_M4_FLAGS += -DJ9VM_JIT_32BIT_USES64BIT_REGISTERS
endif

ifdef UMA_TREAT_WARNINGS_AS_ERRORS
  ifndef UMA_SUPPRESS_WARNINGS_AS_ERRORS
    CFLAGS += -Wimplicit -Wreturn-type -Werror
    CXXFLAGS += -Wreturn-type -Werror
  endif
endif

ifdef UMA_ENABLE_ALL_WARNINGS
  ifndef UMA_SUPPRESS_ALL_WARNINGS
    CFLAGS += -Wall
    CXXFLAGS += -Wall -Wno-non-virtual-dtor
  endif
endif

UMA_DOT_EXE = .so
UMA_DOT_I = .i
UMA_DOT_II = .ii
