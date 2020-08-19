<#--
Copyright (c) 1998, 2020 IBM Corp. and others

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

# z/os linker does not support link groups.  We use this feature on other platforms to help
# satisfy (sometimes circular) link dependencies. On z/os we instead repeat static libraries 4 times.
# 3 was not enough.

<#assign lib_target_rule>
$(UMA_LIBTARGET) : $(UMA_OBJECTS)
	$(AR) rcv $(UMA_LIBTARGET) $(UMA_OBJECTS)

</#assign>

<#assign dll_target_rule>
BUILDLIB : $(UMA_DLLTARGET)
UMA_BYPRODUCTS += $(UMA_DLLTARGET_XSRC) $(UMA_DLLTARGET_XDEST)
$(UMA_DLLTARGET) : $(UMA_OBJECTS) $(UMA_TARGET_LIBRARIES)
	$(UMA_DLL_LD) $(VMLINK) $(UMA_LINK_FLAGS) $(UMA_LINK_PATH) -o $(UMA_DLLTARGET)\
	  $(UMA_OBJECTS) \
	  $(UMA_LINK_STATIC_LIBRARIES) $(UMA_LINK_STATIC_LIBRARIES) $(UMA_LINK_STATIC_LIBRARIES) $(UMA_LINK_STATIC_LIBRARIES) \
	  $(UMA_LINK_SHARED_LIBRARIES) $(UMA_PATH_TO_ROOT)lib/libj9a2e.x
	mkdir -p $(UMA_DLLTARGET_XDEST_DIR)
	mv $(UMA_DLLTARGET_XSRC) $(UMA_DLLTARGET_XDEST)

</#assign>

<#assign exe_target_rule>
$(UMA_EXETARGET) : $(UMA_OBJECTS) $(UMA_TARGET_LIBRARIES)
	$(UMA_EXE_LD) $(UMA_LINK_FLAGS) $(UMA_LINK_PATH) $(VMLINK) -o $@ \
	  $(UMA_OBJECTS) \
	  $(UMA_LINK_STATIC_LIBRARIES) $(UMA_LINK_STATIC_LIBRARIES) $(UMA_LINK_STATIC_LIBRARIES) $(UMA_LINK_STATIC_LIBRARIES) \
	  $(UMA_LINK_SHARED_LIBRARIES) $(UMA_PATH_TO_ROOT)lib/libj9a2e.x

</#assign>

<#if uma.spec.flags.opt_useOmrDdr.enabled>
  CFLAGS   += -Wc,debug
  CXXFLAGS += -Wc,debug
</#if>

ifndef UMA_DO_NOT_OPTIMIZE_CCODE
  ifeq ($(VERSION_MAJOR),8)
    UMA_OPTIMIZATION_FLAGS = -O3 -Wc,"ARCH(7)" -Wc,"TUNE(10)"
    UMA_OPTIMIZATION_LINKER_FLAGS = -Wl,compat=ZOSV1R13
  else
    UMA_OPTIMIZATION_FLAGS = -O3 -Wc,"ARCH(10)" -Wc,"TUNE(10)"
    UMA_OPTIMIZATION_LINKER_FLAGS = -Wl,compat=ZOSV2R3
  endif
else
  UMA_OPTIMIZATION_FLAGS = -0
endif

ifdef j9vm_uma_supportsIpv6
  UMA_ZOS_FLAGS += -DIPv6_FUNCTION_SUPPORT
endif

# _ISOC99_SOURCE Exposes c99 standard library changes which dont require c99 compiler support. (Also needed for c++, see below)
# __STDC_LIMIT_MACROS Is needed to expose limit macros from <stdint.h> on c++ (also requires _ISOC99_SOURCE)
UMA_ZOS_FLAGS += -DJ9ZOS390 -DLONGLONG -DJ9VM_TIERED_CODE_CACHE -D_ALL_SOURCE -D_XOPEN_SOURCE_EXTENDED -DIBM_ATOE -D_POSIX_SOURCE -D_ISOC99_SOURCE -D__STDC_LIMIT_MACROS
UMA_ZOS_FLAGS += -I$(OMR_DIR)/util/a2e/headers $(UMA_OPTIMIZATION_FLAGS) $(UMA_OPTIMIZATION_LINKER_FLAGS) \
	-Wc,"convlit(ISO8859-1),xplink,rostring,FLOAT(IEEE,FOLD,AFP),enum(4)" -Wa,goff -Wc,NOANSIALIAS -Wc,"inline(auto,noreport,600,5000)"
UMA_ZOS_FLAGS += -Wc,"SERVICE(j${uma.buildinfo.build_date})"
ifeq ($(VERSION_MAJOR),8)
  UMA_ZOS_FLAGS += -Wc,"TARGET(zOSV1R13)"
else
  UMA_ZOS_FLAGS += -Wc,"TARGET(zOSV2R3)"
endif
UMA_ZOS_FLAGS += -Wc,list,offset
ifdef j9vm_env_data64
  UMA_ZOS_FLAGS += -DJ9ZOS39064 -Wc,lp64 -Wa,"SYSPARM(BIT64)"
  UMA_WC_64 = -Wc,lp64
else
  UMA_ZOS_FLAGS += -D_LARGE_FILES -Wc,gonumber
endif

ifeq ($(UMA_TARGET_TYPE),DLL)
  UMA_ZOS_FLAGS += -Wc,DLL,EXPORTALL
  UMA_LINK_FLAGS += -Wl,xplink,dll
endif
ifeq ($(UMA_TARGET_TYPE),EXE)
UMA_LINK_FLAGS += -Wl,xplink
endif
ifdef j9vm_env_data64
  UMA_LINK_FLAGS += -Wl,lp64
  UMA_M4_FLAGS += -DTR_64Bit -DTR_HOST_64BIT
endif

COMMA := ,

# Compile jniargtestssystemlink with non-XPLINK (system) linkage
ifeq ($(UMA_TARGET_NAME),jniargtestssystemlink)
  UMA_ZOS_FLAGS := $(subst $(COMMA)xplink,,$(UMA_ZOS_FLAGS))
  UMA_LINK_FLAGS := $(subst $(COMMA)xplink,,$(UMA_LINK_FLAGS))
endif

ifndef j9vm_env_data64
ASFLAGS += -mzarch
endif

ASFLAGS += -Wa,goff -Wa,"SYSPARM(BIT64)"

CFLAGS += -Wc,"langlvl(extc99)" $(UMA_ZOS_FLAGS)
CPPFLAGS += -Wc,"langlvl(extc99)"
CXXFLAGS += -Wc,"langlvl(extended)" $(UMA_WC_64) -+ $(UMA_ZOS_FLAGS)

UMA_ZOS_CXX_LD_FLAGS += -Wc,"langlvl(extended)" $(UMA_WC_64) -+

UMA_DLLTARGET_XSRC = $($(UMA_TARGET_NAME)_xsrc)
UMA_DLLTARGET_XDEST = $($(UMA_TARGET_NAME)_xdest)
UMA_DLLTARGET_XDEST_DIR = $($(UMA_TARGET_NAME)_xdestdir)

UMA_MCASM_INCLUDES_PREFIX = -I
UMA_MCASM_INCLUDES = $(UMA_MCASM_INCLUDES_PREFIX) CBC.SCCNSAM
ifdef j9vm_env_data64
  MCFLAGS = -q64
endif

# JAZZ103 49015 - compile with MRABIG debug option to reduce stack size required for -Xmt
MRABIG = -Wc,"TBYDBG(-qdebug=MRABIG)"
SPECIALCXXFLAGS = $(filter-out -Wc$(COMMA)debug -O3,$(CXXFLAGS))
NEW_OPTIMIZATION_FLAG = -O2 -Wc,"TBYDBG(-qdebug=lincomm:ptranl:tfbagg)" -Wc,"FEDBG(-qxflag=InlineDespiteVolatileInArgs)"

BytecodeInterpreterFull.o : BytecodeInterpreterFull.cpp
	$(CXX) $(SPECIALCXXFLAGS) $(MRABIG) $(NEW_OPTIMIZATION_FLAG) -c $< > $*.asmlist

BytecodeInterpreterCompressed.o : BytecodeInterpreterCompressed.cpp
	$(CXX) $(SPECIALCXXFLAGS) $(MRABIG) $(NEW_OPTIMIZATION_FLAG) -c $< > $*.asmlist

DebugBytecodeInterpreterFull.o : DebugBytecodeInterpreterFull.cpp
	$(CXX) $(SPECIALCXXFLAGS) $(MRABIG) $(NEW_OPTIMIZATION_FLAG) -c $< > $*.asmlist

DebugBytecodeInterpreterCompressed.o : DebugBytecodeInterpreterCompressed.cpp
	$(CXX) $(SPECIALCXXFLAGS) $(MRABIG) $(NEW_OPTIMIZATION_FLAG) -c $< > $*.asmlist

MHInterpreterFull$(UMA_DOT_O) : MHInterpreterFull.cpp
	$(CXX) $(SPECIALCXXFLAGS) $(MRABIG) $(NEW_OPTIMIZATION_FLAG) -c $< > $*.asmlist

MHInterpreterCompressed$(UMA_DOT_O) : MHInterpreterCompressed.cpp
	$(CXX) $(SPECIALCXXFLAGS) $(MRABIG) $(NEW_OPTIMIZATION_FLAG) -c $< > $*.asmlist
