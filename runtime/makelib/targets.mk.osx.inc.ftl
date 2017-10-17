<#-- 
	Copyright (c) 1998, 2017 IBM Corp. and others
	
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

	SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
-->

<#assign lib_target_rule>
$(UMA_LIBTARGET):  $(UMA_OBJECTS)
	$(AR) rcv $(UMA_LIBTARGET) $(UMA_OBJECTS)
</#assign>

<#assign dll_target_rule>
$(UMA_DLLTARGET): $(UMA_OBJECTS) $(UMA_TARGET_LIBRARIES)
	$(UMA_DLL_LD) $(UMA_DLL_LINK_FLAGS) \
     $(VMLINK) $(UMA_LINK_PATH) -o $(UMA_DLLTARGET)\
	 $(UMA_OBJECTS) \
     $(UMA_DLL_LINK_POSTFLAGS)
</#assign>

<#assign exe_target_rule>
$(UMA_EXETARGET): $(UMA_OBJECTS) $(UMA_TARGET_LIBRARIES)
	$(UMA_EXE_LD) $(UMA_EXE_PREFIX_FLAGS) $(UMA_LINK_PATH) $(VMLINK) \
    $(UMA_OBJECTS) \
    $(UMA_BEGIN_DASH_L) \
    $(UMA_LINK_STATIC_LIBRARIES) \
    $(UMA_END_DASH_L) \
    $(UMA_LINK_SHARED_LIBRARIES) \
    -o $@ $(UMA_EXE_POSTFIX_FLAGS)
</#assign>


<#if uma.spec.processor.x86>
    UMA_EXE_PREFIX_FLAGS+=-m32
</#if>

UMA_BEGIN_DASH_L=
UMA_END_DASH_L=

UMA_EXE_POSTFIX_FLAGS+=-lm -liconv -lc -ldl -lutil -Wl,-rpath,\$$ORIGIN
 
<#if uma.spec.processor.amd64>
UMA_MASM2GAS_FLAGS+=--64
</#if>

<#if uma.spec.properties.uma_crossCompilerPath.defined>
# Put the tools on the path.
PATH:=${uma.spec.properties.uma_crossCompilerPath.value}:<#noparse>${PATH}</#noparse>
XCOMP_TOOLCHAIN_BASEDIR:=${uma.spec.properties.uma_crossCompilerPath.value}/../
</#if>

ifndef UMA_DO_NOT_OPTIMIZE_CCODE
<#if uma.spec.properties.uma_optimization_cflags.defined>
  UMA_OPTIMIZATION_CFLAGS+=${uma.spec.properties.uma_optimization_cflags.value}
<#else>
  <#if uma.spec.processor.amd64>
  UMA_OPTIMIZATION_CFLAGS+=-O3 -fno-strict-aliasing
  <#elseif uma.spec.processor.x86>
  UMA_OPTIMIZATION_CFLAGS+=-O3 -fno-strict-aliasing -march=pentium4 -mtune=prescott -mpreferred-stack-boundary=4
  <#else>
  UMA_OPTIMIZATION_CFLAGS+=-O
  </#if>
</#if>
<#if uma.spec.properties.uma_optimization_cxxflags.defined>
  UMA_OPTIMIZATION_CXXFLAGS+=${uma.spec.properties.uma_optimization_cxxflags.value}
<#else>
  <#if uma.spec.processor.amd64>
  UMA_OPTIMIZATION_CXXFLAGS+=-O3 -fno-strict-aliasing
  <#elseif uma.spec.processor.x86>
  UMA_OPTIMIZATION_CXXFLAGS+=-O3 -fno-strict-aliasing -march=pentium4 -mtune=prescott -mpreferred-stack-boundary=4
  <#if uma.spec.flags.env_littleEndian.enabled && uma.spec.type.linux>
  UMA_OPTIMIZATION_CXXFLAGS+=-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=1
  </#if>
  <#else>
  UMA_OPTIMIZATION_CXXFLAGS+=-O
  </#if>
</#if>
else
  UMA_OPTIMIZATION_CFLAGS+=-O0
  UMA_OPTIMIZATION_CXXFLAGS+=-O0
endif


CFLAGS+=$(UMA_OPTIMIZATION_CFLAGS)
CXXFLAGS+=$(UMA_OPTIMIZATION_CXXFLAGS)

<#if uma.spec.flags.env_gcc.enabled>
	CXXFLAGS+=-fno-exceptions -fno-threadsafe-statics
</#if>

ifdef j9vm_uma_gnuDebugSymbols
CFLAGS+=-g
CXXFLAGS+=-g
endif

<#if uma.spec.processor.x86 || uma.spec.processor.amd64 >
<#-- GCC compilers support dependency generation -->
CFLAGS+=-MMD
CPPFLAGS+=-MMD 
CXXFLAGS+=-MMD
</#if>

CFLAGS+=-DOSX -D_REENTRANT -D_FILE_OFFSET_BITS=64 
CXXFLAGS+=-DOSX -D_REENTRANT -D_FILE_OFFSET_BITS=64 
CPPFLAGS+=-DOSX -D_REENTRANT

<#-- Add Position Indepdent compile flag -->
CFLAGS+=-fPIC 
CXXFLAGS+=-fPIC

ifdef j9vm_uma_supportsIpv6
  CFLAGS+=-DIPv6_FUNCTION_SUPPORT
  CXXFLAGS+=-DIPv6_FUNCTION_SUPPORT
  CPPFLAGS+=-DIPv6_FUNCTION_SUPPORT
endif

<#if uma.spec.processor.amd64>
CFLAGS+=-DJ9HAMMER -m64
CXXFLAGS+=-DJ9HAMMER -m64
CPPFLAGS+=-DJ9HAMMER -m64
<#elseif uma.spec.processor.x86>
CFLAGS+=-DJ9X86 -m32 -msse2
CXXFLAGS+=-DJ9X86 -m32 -msse2 -I/usr/include/nptl -fno-threadsafe-statics
CPPFLAGS+=-DJ9X86 -m32 -msse2 -I/usr/include/nptl
</#if>

<#if uma.spec.processor.ppc && !uma.spec.flags.env_gcc.enabled>
<#else>
UMA_DLL_LINK_FLAGS+=-shared -install_name lib$(UMA_TARGET_NAME).dylib
ifdef UMA_USING_LD_TO_LINK
  UMA_DLL_LINK_FLAGS+=origin -rpath \$$ORIGIN --disable-new-dtags
else
  UMA_DLL_LINK_FLAGS+=-Xlinker -rpath -Xlinker \$$ORIGIN
endif

UMA_DLL_LINK_POSTFLAGS+=$(UMA_LINK_STATIC_LIBRARIES)
UMA_DLL_LINK_POSTFLAGS+=$(UMA_LINK_SHARED_LIBRARIES)

ifdef j9vm_uma_gnuDebugSymbols
UMA_DLL_LINK_POSTFLAGS+=-g
endif

<#if uma.spec.processor.x86>
    UMA_DLL_LINK_FLAGS+=-m32
</#if>
<#if uma.spec.processor.amd64>
    UMA_DLL_LINK_FLAGS+=-m64
</#if>

<#if uma.spec.processor.x86>
UMA_DLL_LINK_POSTFLAGS+=-lc -lm -ldl
 <#else>
ifdef UMA_IS_C_PLUS_PLUS
  UMA_DLL_LINK_POSTFLAGS+=-lc
endif
UMA_DLL_LINK_POSTFLAGS+=-lm
 </#if>
 
</#if>

ifdef UMA_TREAT_WARNINGS_AS_ERRORS
ifndef UMA_SUPPRESS_WARNINGS_AS_ERRORS
  CFLAGS+=-Wimplicit -Wreturn-type -Werror
  CXXFLAGS+=-Wreturn-type -Werror
endif
endif

ifdef UMA_ENABLE_ALL_WARNINGS
ifndef UMA_SUPPRESS_ALL_WARNINGS
  CFLAGS+=-Wall 
  CXXFLAGS+=-Wall -Wno-non-virtual-dtor
endif
endif

<#if uma.spec.processor.amd64>
# special handling BytecodeInterpreter.cpp and DebugBytecodeInterpreter.cpp
BytecodeInterpreter$(UMA_DOT_O):BytecodeInterpreter.cpp
	$(INTERP_GCC) $(CXXFLAGS) -c $<

DebugBytecodeInterpreter$(UMA_DOT_O):DebugBytecodeInterpreter.cpp
	$(INTERP_GCC) $(CXXFLAGS) -c $<

MHInterpreter$(UMA_DOT_O):MHInterpreter.cpp
	$(INTERP_GCC) $(CXXFLAGS) -c $<
	
# Special handling for unused result warnings.
closures$(UMA_DOT_O):closures.c
	$(CC) $(CFLAGS) -Wno-unused-result -c -o $@ $<
	
j9process$(UMA_DOT_O):j9process.c
	$(CC) $(CFLAGS) -Wno-unused-result -c -o $@ $<
	
</#if>
