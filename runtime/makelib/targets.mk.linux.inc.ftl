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

<#if uma.spec.processor.arm>
  ARM_ARCH_FLAGS := -march=armv6 -marm -mfpu=vfp -mfloat-abi=hard
  OPENJ9_CC_PREFIX ?= bcm2708hardfp
  OBJCOPY := $(OPENJ9_CC_PREFIX)-objcopy
<#elseif uma.spec.processor.aarch64>
  OBJCOPY := $(OPENJ9_CC_PREFIX)-objcopy
<#else>
  OBJCOPY := objcopy
</#if>

<#assign lib_target_rule>
$(UMA_LIBTARGET) : $(UMA_OBJECTS)
	$(AR) rcv $(UMA_LIBTARGET) $(UMA_OBJECTS)
</#assign>

<#assign dll_target_rule>
$(UMA_DLLTARGET) : $(UMA_OBJECTS) $(UMA_TARGET_LIBRARIES)
	$(UMA_DLL_LD) $(UMA_DLL_LINK_FLAGS) \
		$(VMLINK) $(UMA_LINK_PATH) -o $(UMA_DLLTARGET) \
		$(UMA_OBJECTS) \
		$(UMA_DLL_LINK_POSTFLAGS)
ifdef j9vm_uma_gnuDebugSymbols
	$(OBJCOPY) --only-keep-debug $(UMA_DLLTARGET) $(UMA_DLLTARGET).dbg
	$(OBJCOPY) --strip-debug $(UMA_DLLTARGET)
	$(OBJCOPY) --add-gnu-debuglink=$(UMA_DLLTARGET).dbg $(UMA_DLLTARGET)
endif
</#assign>

<#assign exe_target_rule>
$(UMA_EXETARGET) : $(UMA_OBJECTS) $(UMA_TARGET_LIBRARIES)
	$(UMA_EXE_LD) $(UMA_EXE_PREFIX_FLAGS) $(UMA_LINK_PATH) $(VMLINK) \
		$(UMA_OBJECTS) \
		$(UMA_BEGIN_DASH_L) \
		$(UMA_LINK_STATIC_LIBRARIES) \
		$(UMA_END_DASH_L) \
		$(UMA_LINK_SHARED_LIBRARIES) \
		-o $@ $(UMA_EXE_POSTFIX_FLAGS)
</#assign>

<#if uma.spec.processor.s390>
  ifndef j9vm_env_data64
    J9M31 = -m31
  endif
  UMA_EXE_PREFIX_FLAGS += $(J9M31)
</#if>

<#if uma.spec.processor.ppc && !uma.spec.flags.env_gcc.enabled>
  ifdef j9vm_env_data64
    UMA_EXE_PREFIX_FLAGS += -q64
  endif
</#if>

<#if uma.spec.processor.x86>
  UMA_EXE_PREFIX_FLAGS += -m32
</#if>

<#if uma.spec.processor.ppc>
  UMA_BEGIN_DASH_L = -Wl,--start-group
  UMA_END_DASH_L = -Wl,--end-group
<#else>
  UMA_BEGIN_DASH_L = -Xlinker --start-group
  UMA_END_DASH_L = -Xlinker --end-group
</#if>

UMA_EXE_POSTFIX_FLAGS += -lm -lrt -lpthread -lc -ldl -lutil -Wl,-z,origin,-rpath,\$$ORIGIN,--disable-new-dtags,-rpath-link,$(UMA_PATH_TO_ROOT)

<#if uma.spec.processor.amd64>
  UMA_MASM2GAS_FLAGS += --64
</#if>

<#if uma.spec.properties.uma_crossCompilerPath.defined>
# Put the tools on the path.
  PATH := ${uma.spec.properties.uma_crossCompilerPath.value}:<#noparse>${PATH}</#noparse>
  XCOMP_TOOLCHAIN_BASEDIR := ${uma.spec.properties.uma_crossCompilerPath.value}/../
</#if>

ifndef UMA_DO_NOT_OPTIMIZE_CCODE
  <#if uma.spec.properties.uma_optimization_cflags.defined>
    UMA_OPTIMIZATION_CFLAGS += ${uma.spec.properties.uma_optimization_cflags.value}
  <#else>
    <#if uma.spec.processor.amd64>
      UMA_OPTIMIZATION_CFLAGS += -O3 -fno-strict-aliasing
    <#elseif uma.spec.processor.x86>
      UMA_OPTIMIZATION_CFLAGS += -O3 -fno-strict-aliasing -march=pentium4 -mtune=prescott -mpreferred-stack-boundary=4
    <#elseif uma.spec.processor.arm>
      UMA_OPTIMIZATION_CFLAGS += -g -O3 -fno-strict-aliasing $(ARM_ARCH_FLAGS) -Wno-unused-but-set-variable
    <#elseif uma.spec.processor.ppc>
      UMA_OPTIMIZATION_CFLAGS += -O3
      <#if uma.spec.flags.env_gcc.enabled>
        UMA_OPTIMIZATION_CFLAGS += -fno-strict-aliasing
      </#if>
      <#if uma.spec.flags.env_littleEndian.enabled && uma.spec.type.linux>
        UMA_OPTIMIZATION_CFLAGS += -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=1
        ifndef USE_PPC_GCC
          UMA_OPTIMIZATION_CFLAGS += -qsimd=noauto
        endif
      </#if>
    <#elseif uma.spec.processor.s390>
      UMA_OPTIMIZATION_CFLAGS += -O3 -mtune=z10 -march=z9-109 -mzarch
    <#else>
      UMA_OPTIMIZATION_CFLAGS += -O
    </#if>
  </#if>
  <#if uma.spec.properties.uma_optimization_cxxflags.defined>
    UMA_OPTIMIZATION_CXXFLAGS += ${uma.spec.properties.uma_optimization_cxxflags.value}
  <#else>
    <#if uma.spec.processor.amd64>
      UMA_OPTIMIZATION_CXXFLAGS += -O3 -fno-strict-aliasing
    <#elseif uma.spec.processor.x86>
      UMA_OPTIMIZATION_CXXFLAGS += -O3 -fno-strict-aliasing -march=pentium4 -mtune=prescott -mpreferred-stack-boundary=4
    <#elseif uma.spec.processor.arm>
      UMA_OPTIMIZATION_CXXFLAGS += -g -O3 -fno-strict-aliasing $(ARM_ARCH_FLAGS) -Wno-unused-but-set-variable
    <#elseif uma.spec.processor.ppc>
      UMA_OPTIMIZATION_CXXFLAGS += -O3
      <#if uma.spec.flags.env_gcc.enabled>
        UMA_OPTIMIZATION_CXXFLAGS += -fno-strict-aliasing
      </#if>
      <#if uma.spec.flags.env_littleEndian.enabled && uma.spec.type.linux>
        UMA_OPTIMIZATION_CXXFLAGS += -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=1
      </#if>
    <#elseif uma.spec.processor.s390>
      UMA_OPTIMIZATION_CXXFLAGS += -O3 -mtune=z10 -march=z9-109 -mzarch
    <#else>
      UMA_OPTIMIZATION_CXXFLAGS += -O
    </#if>
  </#if>
else
  UMA_OPTIMIZATION_CFLAGS += -O0
  UMA_OPTIMIZATION_CXXFLAGS += -O0
endif

CFLAGS += $(UMA_OPTIMIZATION_CFLAGS)
CXXFLAGS += $(UMA_OPTIMIZATION_CXXFLAGS)
<#if uma.spec.processor.ppc>
  ifdef USE_PPC_GCC
    PPC_GCC_CXXFLAGS += -O3 -fno-strict-aliasing
  endif
</#if>

<#--
OpenJ9 uses GNU89 inline semantics, but GCC versions 5 and newer default
to GNU11 inline semantics. The option '-fgnu89-inline' is appended to CFLAGS
to allow compilation with newer GCC versions.

Reference - https://gcc.gnu.org/gcc-5/porting_to.html.
-->
<#if uma.spec.flags.env_gcc.enabled || !uma.spec.processor.ppc>
# If $(CC) doesn't accept the '-dumpversion' option, assume it's not GCC versions 5 or newer. 
GCC_MAJOR_VERSION := $(shell ($(CC) -dumpversion 2>/dev/null || echo 1) | cut -d. -f1)

ifeq (,$(findstring $(GCC_MAJOR_VERSION),1 2 3 4))
  CFLAGS += -fgnu89-inline
endif
</#if>

<#if !uma.spec.processor.ppc>
  CXXFLAGS += -fno-exceptions -fno-rtti -fno-threadsafe-statics
<#else>
  <#if uma.spec.flags.env_gcc.enabled>
    CXXFLAGS += -fno-exceptions -fno-rtti -fno-threadsafe-statics
  <#else>
    CXXFLAGS += -qnortti
    ifndef UMA_COMPILING_OMR_GTEST_BASED_CODE
      # don't use this flag for gtest based code.
      CXXFLAGS += -qnoeh
    endif
  </#if>
</#if>
<#if uma.spec.processor.ppc>
  ifdef USE_PPC_GCC
    PPC_GCC_CXXFLAGS += -fno-rtti -fno-threadsafe-statics -fno-exceptions
  endif
</#if>

ASFLAGS += -noexecstack

<#if uma.spec.processor.ppc>
  <#if uma.spec.flags.env_gcc.enabled>
    ifdef j9vm_env_data64
      ASFLAGS += -a64 -mppc64
    else
      ASFLAGS += -a32 -mppc64
    endif
  <#else>
    ASFLAGS += -c -o $*.o -qpic=large
    ifdef j9vm_env_data64
      ASFLAGS += -q64
      <#if uma.spec.flags.env_littleEndian.enabled>
      ASFLAGS += -qarch=pwr7
      <#else>
      ASFLAGS += -qarch=ppc64
      </#if>
    else
      ASFLAGS += -qarch=ppc
    endif
  </#if>
</#if>
<#if uma.spec.processor.s390>
  ifndef j9vm_env_data64
    ASFLAGS += -mzarch
  endif
  ASFLAGS += -march=z9-109 $(J9M31) $(VMASMDEBUG) -o $*.o
</#if>
<#if uma.spec.processor.x86>
  ASFLAGS += -32
</#if>
<#if uma.spec.processor.amd64>
  ASFLAGS += -64
</#if>
<#if uma.spec.processor.arm>
  ASFLAGS += -march=armv6 -mfpu=vfp -mfloat-abi=hard
</#if>

ifdef j9vm_uma_gnuDebugSymbols
  CFLAGS += -g
  CXXFLAGS += -g
  <#if uma.spec.processor.ppc>
    ifdef USE_PPC_GCC
      PPC_GCC_CXXFLAGS += -g
    endif
  </#if>
  <#if uma.spec.processor.x86 || uma.spec.processor.amd64>
    ASFLAGS += --gdwarf2
  </#if>
endif

<#if uma.spec.processor.x86 || uma.spec.processor.amd64>
  <#-- GCC compilers support dependency generation -->
  CFLAGS += -MMD
  CPPFLAGS += -MMD
  CXXFLAGS += -MMD
</#if>

CFLAGS += -DLINUX -D_REENTRANT -D_FILE_OFFSET_BITS=64
CXXFLAGS += -DLINUX -D_REENTRANT -D_FILE_OFFSET_BITS=64
CPPFLAGS += -DLINUX -D_REENTRANT
<#if uma.spec.processor.ppc>
  ifdef USE_PPC_GCC
    PPC_GCC_CXXFLAGS += -DLINUX -D_REENTRANT -D_FILE_OFFSET_BITS=64
  endif
</#if>

<#-- Add Position Independent compile flag -->
<#if uma.spec.processor.amd64 || uma.spec.processor.arm || uma.spec.processor.s390>
  CFLAGS += -fPIC
  CXXFLAGS += -fPIC
<#elseif uma.spec.processor.ppc>
  <#if uma.spec.flags.env_gcc.enabled>
    CFLAGS += -fpic
    CXXFLAGS += -fpic
  <#else>
    CFLAGS += -qpic=large
    CXXFLAGS += -qpic=large
  </#if>
  ifdef USE_PPC_GCC
    PPC_GCC_CXXFLAGS += -fPIC
  endif
<#else>
  <#-- Used for GOTs under 4k, should we just go -fPIC for everyone? -->
  CFLAGS += -fpic
  CXXFLAGS += -fpic
</#if>

ifdef j9vm_uma_supportsIpv6
  CFLAGS += -DIPv6_FUNCTION_SUPPORT
  CXXFLAGS += -DIPv6_FUNCTION_SUPPORT
  CPPFLAGS += -DIPv6_FUNCTION_SUPPORT
  <#if uma.spec.processor.ppc>
    ifdef USE_PPC_GCC
      PPC_GCC_CXXFLAGS += -DIPv6_FUNCTION_SUPPORT
    endif
  </#if>
endif

<#if uma.spec.processor.amd64>
  CFLAGS += -DJ9HAMMER -m64
  CXXFLAGS += -DJ9HAMMER -m64
  CPPFLAGS += -DJ9HAMMER -m64
<#elseif uma.spec.processor.arm>
  CFLAGS += -DJ9ARM -DARMGNU -DARM -DFIXUP_UNALIGNED -I$(XCOMP_TOOLCHAIN_BASEDIR)/arm-bcm2708/arm-bcm2708hardfp-linux-gnueabi/arm-bcm2708hardfp-linux-gnueabi/include
  CXXFLAGS += -DJ9ARM -DARMGNU -DARM -DFIXUP_UNALIGNED -I$(XCOMP_TOOLCHAIN_BASEDIR)/arm-bcm2708/arm-bcm2708hardfp-linux-gnueabi/arm-bcm2708hardfp-linux-gnueabi/include -fno-threadsafe-statics
  CPPFLAGS += -DJ9ARM -DARMGNU -DARM -DFIXUP_UNALIGNED-I$(XCOMP_TOOLCHAIN_BASEDIR)/arm-bcm2708/arm-bcm2708hardfp-linux-gnueabi/arm-bcm2708hardfp-linux-gnueabi/include
<#elseif uma.spec.processor.aarch64>
  CFLAGS += -DJ9AARCH64
  CXXFLAGS += -DJ9AARCH64
  CPPFLAGS += -DJ9AARCH64
<#elseif uma.spec.processor.ppc>
  CFLAGS += -DLINUXPPC
  CXXFLAGS += -DLINUXPPC
  CPPFLAGS += -DLINUXPPC
  <#if uma.spec.flags.env_gcc.enabled>
    ifdef j9vm_env_data64
      CFLAGS += -m64 -DLINUXPPC64 -DPPC64
      CXXFLAGS += -m64 -DLINUXPPC64 -DPPC64
      CPPFLAGS += -m64 -DLINUXPPC64 -DPPC64
    else
      CFLAGS += -m32
      CXXFLAGS += -m32
      CPPFLAGS += -m32
    endif
  <#else>
    CFLAGS += -qalias=noansi -qxflag=LTOL:LTOL0 -qxflag=selinux
    CXXFLAGS += -qalias=noansi -qxflag=LTOL:LTOL0 -qxflag=selinux -qsuppress=1540-1087:1540-1088:1540-1090
    ifdef j9vm_env_data64
    <#if uma.spec.flags.env_littleEndian.enabled>
      CFLAGS += -qarch=pwr7
      CXXFLAGS += -qarch=pwr7
    <#else>
      CFLAGS += -qarch=ppc64
      CXXFLAGS += -qarch=ppc64
    </#if>
      CFLAGS += -q64 -DLINUXPPC64 -DPPC64
      CXXFLAGS += -q64 -DLINUXPPC64 -DPPC64
      CPPFLAGS += -DLINUXPPC64 -DPPC64
    else
      CFLAGS += -qarch=ppc
      CXXFLAGS += -qarch=ppc
    endif
  </#if>
  ifdef j9vm_env_data64
    ifdef USE_PPC_GCC
      PPC_GCC_CXXFLAGS += -DLINUXPPC -DLINUXPPC64 -DPPC64 -m64
    endif
  else
    ifdef USE_PPC_GCC
      PPC_GCC_CXXFLAGS += -DLINUXPPC -m32
    endif
  endif
<#elseif uma.spec.processor.s390>
  CFLAGS += $(J9M31) -DS390 -D_LONG_LONG -DJ9VM_TIERED_CODE_CACHE -fno-strict-aliasing
  CXXFLAGS += $(J9M31) -DS390 -D_LONG_LONG -DJ9VM_TIERED_CODE_CACHE -fno-strict-aliasing
  CPPFLAGS += -DS390 -D_LONG_LONG -DJ9VM_TIERED_CODE_CACHE
  ifdef j9vm_env_data64
    CFLAGS += -DS39064
    CXXFLAGS += -DS39064
    CPPFLAGS += -DS39064
  endif
<#elseif uma.spec.processor.x86>
  CFLAGS += -DJ9X86 -m32 -msse2
  CXXFLAGS += -DJ9X86 -m32 -msse2 -I/usr/include/nptl -fno-threadsafe-statics
  CPPFLAGS += -DJ9X86 -m32 -msse2 -I/usr/include/nptl
</#if>

<#if uma.spec.processor.ppc && !uma.spec.flags.env_gcc.enabled>
  ifdef j9vm_env_data64
    UMA_DLL_LINK_FLAGS += -q64
  endif
  UMA_DLL_LINK_FLAGS += -qmkshrobj -qxflag=selinux -Wl,-Map=$(UMA_TARGET_NAME).map
  UMA_DLL_LINK_FLAGS += -Wl,-soname=$(UMA_DLLFILENAME),--version-script=$(UMA_TARGET_NAME).exp

  UMA_DLL_LINK_POSTFLAGS += -Wl,--start-group $(UMA_LINK_STATIC_LIBRARIES) -Wl,--end-group
  UMA_DLL_LINK_POSTFLAGS += $(UMA_LINK_SHARED_LIBRARIES)
  UMA_DLL_LINK_POSTFLAGS += -lm -lpthread -lc -Wl,-z,origin,-rpath,\$$ORIGIN,--disable-new-dtags,-rpath-link,$(UMA_PATH_TO_ROOT)
<#else>
  UMA_DLL_LINK_FLAGS += -shared
  UMA_DLL_LINK_FLAGS += -Wl,-Map=$(UMA_TARGET_NAME).map
  UMA_DLL_LINK_FLAGS += -Wl,--version-script,$(UMA_TARGET_NAME).exp
  UMA_DLL_LINK_FLAGS += -Wl,-soname=$(UMA_DLLFILENAME)
  UMA_DLL_LINK_FLAGS += -Xlinker -z -Xlinker origin -Xlinker -rpath -Xlinker \$$ORIGIN -Xlinker --disable-new-dtags

  UMA_DLL_LINK_POSTFLAGS += -Xlinker --start-group
  UMA_DLL_LINK_POSTFLAGS += $(UMA_LINK_STATIC_LIBRARIES)
  UMA_DLL_LINK_POSTFLAGS += -Xlinker --end-group
  UMA_DLL_LINK_POSTFLAGS += $(UMA_LINK_SHARED_LIBRARIES)

  ifdef j9vm_uma_gnuDebugSymbols
    UMA_DLL_LINK_POSTFLAGS += -g
  endif

  <#if uma.spec.processor.x86>
    UMA_DLL_LINK_FLAGS += -m32
  </#if>
  <#if uma.spec.processor.amd64>
    UMA_DLL_LINK_FLAGS += -m64
  </#if>

  <#if uma.spec.processor.ppc && uma.spec.flags.env_gcc.enabled>
    ifdef j9vm_env_data64
      UMA_EXE_PREFIX_FLAGS += -m64
      UMA_DLL_LINK_FLAGS += -m64
    else
      UMA_EXE_PREFIX_FLAGS += -m32
      UMA_DLL_LINK_FLAGS += -m32
    endif
  </#if>

  <#if uma.spec.processor.x86>
    UMA_DLL_LINK_POSTFLAGS += -lc -lm -ldl
  <#else>
    ifdef UMA_IS_C_PLUS_PLUS
      UMA_DLL_LINK_POSTFLAGS += -lc
    endif
    UMA_DLL_LINK_POSTFLAGS += -lm
  </#if>

  <#if uma.spec.processor.s390>
    UMA_DLL_LINK_FLAGS += $(J9M31)
    UMA_DLL_LINK_POSTFLAGS += -Xlinker -z -Xlinker origin
    UMA_DLL_LINK_POSTFLAGS += -Xlinker -rpath -Xlinker \$$ORIGIN -Xlinker --disable-new-dtags
    UMA_DLL_LINK_POSTFLAGS += -Xlinker -rpath-link -Xlinker $(UMA_PATH_TO_ROOT)
  </#if>
</#if>

<#if uma.spec.processor.s390>
  ifdef j9vm_jit_32bitUses64bitRegisters
    UMA_M4_FLAGS += -DJ9VM_JIT_32BIT_USES64BIT_REGISTERS
  endif
</#if>

<#if uma.spec.processor.arm>
$(patsubst %.s,%.o,$(filter %.s,$(UMA_FILES_TO_PREPROCESS))) : %$(UMA_DOT_O) : %.s
	sed -f $(UMA_PATH_TO_ROOT)compiler/build/scripts/armasm2gas.sed $*.s > $*.S
	$(CPP) $(CPPFLAGS) $(UMA_C_INCLUDES) $*.S > $*.spp
	-rm $*.S
	$(AS) $(ASFLAGS) $(VMASMDEBUG) -o $*.o $*.spp
	-rm $*.spp
<#elseif uma.spec.processor.ppc>
$(patsubst %.s,%.o,$(filter %.s,$(UMA_FILES_TO_PREPROCESS))) : %$(UMA_DOT_O) : %.s
	$(CPP) -P $(CPPFLAGS) $*.s | sed 's/\!/\#/g' > $*.spp.s
	$(AS) $(ASFLAGS) $*.spp.s
	-rm $*.spp.s
</#if>

ifdef UMA_TREAT_WARNINGS_AS_ERRORS
  ifndef UMA_SUPPRESS_WARNINGS_AS_ERRORS
    <#if uma.spec.processor.ppc>
      <#if uma.spec.flags.env_gcc.enabled>
        CFLAGS += -Wreturn-type -Werror
        CXXFLAGS += -Wreturn-type -Werror
      <#else>
        CFLAGS += -qhalt=w
        CXXFLAGS += -qhalt=w
      </#if>
      ifdef USE_PPC_GCC
        PPC_GCC_CXXFLAGS += -Wreturn-type -Werror
      endif
    <#else>
      CFLAGS += -Wimplicit -Wreturn-type -Werror
      CXXFLAGS += -Wreturn-type -Werror
    </#if>
  endif
endif

ifdef UMA_ENABLE_ALL_WARNINGS
  ifndef UMA_SUPPRESS_ALL_WARNINGS
    <#if uma.spec.processor.ppc>
      CFLAGS +=
      CXXFLAGS +=
      ifdef USE_PPC_GCC
        PPC_GCC_CXXFLAGS += -Wall -Wno-non-virtual-dtor
      endif
    <#else>
      CFLAGS += -Wall
      CXXFLAGS += -Wall -Wno-non-virtual-dtor
    </#if>
  endif
endif

<#if uma.spec.flags.env_advanceToolchain.enabled>
  AT_HOME = /opt/at7.0-0-rc3

  <#if uma.spec.flags.env_gcc.enabled>
    #
    AS = $(AT_HOME)/bin/as
    CC = $(AT_HOME)/bin/cc
    CXX = $(AT_HOME)/bin/c++
    UMA_DLL_LD = $(AT_HOME)/bin/cc
    UMA_EXE_LD = $(AT_HOME)/bin/cc
  <#else>
    # We always need to invoke xlc (also used to link and assemble) with
    # the -F <config> option. This raises an additional warning which
    # we'll also want to suppress.
    #
    AT_CONFIG = $(AT_HOME)/scripts/vac-12_1-AT7_0-0-RC3.dfp.cfg
    UMA_EXE_LD += -F $(AT_CONFIG) -qsuppress=1501-274
    UMA_DLL_LD += -F $(AT_CONFIG) -qsuppress=1501-274
    CFLAGS += -F $(AT_CONFIG) -qsuppress=1501-274
    CXXFLAGS += -F $(AT_CONFIG) -qsuppress=1501-274
    ASFLAGS += -F $(AT_CONFIG) -qsuppress=1501-274
  </#if>
</#if>

<#if uma.spec.processor.ppc && !uma.spec.type.aix>
ifdef USE_PPC_GCC

# special handling BytecodeInterpreter.cpp and DebugBytecodeInterpreter.cpp
BytecodeInterpreter$(UMA_DOT_O) : BytecodeInterpreter.cpp
	$(PPC_GCC_CXX) $(PPC_GCC_CXXFLAGS) -c $<

DebugBytecodeInterpreter$(UMA_DOT_O) : DebugBytecodeInterpreter.cpp
	$(PPC_GCC_CXX) $(PPC_GCC_CXXFLAGS) -c $<

MHInterpreter$(UMA_DOT_O) : MHInterpreter.cpp
	$(PPC_GCC_CXX) $(PPC_GCC_CXXFLAGS) -c $<

endif
</#if>
<#if uma.spec.processor.amd64>
# Special handling for unused result warnings.
closures$(UMA_DOT_O) : closures.c
	$(CC) $(CFLAGS) -Wno-unused-result -c -o $@ $<

j9process$(UMA_DOT_O) : j9process.c
	$(CC) $(CFLAGS) -Wno-unused-result -c -o $@ $<

</#if>
