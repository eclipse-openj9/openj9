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
UMA_BYPRODUCTS+=$($(UMA_TARGET_NAME)_pdb)
$(UMA_LIBTARGET):  $(UMA_OBJECTS) $(UMA_TARGET_LIBRARIES)
	$(IMPLIB) -out:$@ $(UMA_OBJECTS) $(UMA_LINK_LIBRARIES)
</#assign>
<#assign dll_target_rule>
UMA_BYPRODUCTS+=$($(UMA_TARGET_NAME)_exp) $($(UMA_TARGET_NAME)_pdb)
		
$(UMA_LIBTARGET):  $(UMA_OBJECTS) $(UMA_TARGET_LIBRARIES)  $(UMA_TARGET_NAME).def
	$(IMPLIB) -subsystem:$(UMA_SUBSYSTEM_TYPE) -out:$@ -def:$(UMA_TARGET_NAME).def $(UMA_LIBRARIAN_OPTIONS) $(UMA_OBJECTS) $(UMA_LINK_PATH) $(UMA_LINK_LIBRARIES)
						
$(UMA_DLLTARGET): $(UMA_OBJECTS) $(UMA_TARGET_LIBRARIES) $(UMA_LIBTARGET)
	$(LINK) $(VMLINK) $(UMA_VMLINK_OPTIONS) $(DLLFLAGS) $(DLLBASEADDRESS) -machine:$(UMA_CPU) \
	  -subsystem:$(UMA_SUBSYSTEM_TYPE) -out:$@ -map:$(UMA_TARGET_NAME).map \
	  $(UMA_DELAYLOAD_INSTRUCTIONS) $(UMA_OBJECTS) $(UMA_LINK_PATH) $(UMA_LINK_LIBRARIES) $(UMA_DLL_LINK_FLAGS) \
	  $(UMA_EXTRA_SYSTEM_LIBRARIES) $($(UMA_TARGET_NAME)_exp)
ifeq ($(UMA_SINGLE_REBASE),1)
	$(warning UMA_SINGLE_REBASE specified, suppressing per-directory rebase.) 
else
	$(MAKE) -C $(UMA_PATH_TO_ROOT) rebase
endif
</#assign>
<#assign exe_target_rule>
$(UMA_EXETARGET): $(UMA_OBJECTS) $(UMA_TARGET_LIBRARIES)
	$(LINK) $(UMA_EXEFLAGS) $(UMA_VMLINK_OPTIONS) $(VMLINK) -out:$@ -subsystem:$(UMA_SUBSYSTEM_TYPE) \
	 -machine:$(UMA_CPU) $(UMA_EXE_LINK_FLAGS) $(UMA_OBJECTS) $(UMA_LINK_LIBRARIES) $(UMA_EXELIBS) \
	 $(UMA_EXTRA_SYSTEM_LIBRARIES)
	$(MT) -manifest $(UMA_EXE_MANIFEST) -outputresource:$@;#1
</#assign>

UMA_LIBRARIAN_OPTIONS+=-machine:$(UMA_CPU)

DLLFLAGS+=/INCREMENTAL:NO /NOLOGO 
<#if uma.spec.processor.x86>
DLLFLAGS+=-entry:_DllMainCRTStartup@12
<#elseif uma.spec.processor.amd64>
DLLFLAGS+=-entry:_DllMainCRTStartup
</#if>
DLLFLAGS+=-dll
UMA_DLL_LINK_FLAGS+=kernel32.lib ws2_32.lib advapi32.lib user32.lib gdi32.lib comdlg32.lib winspool.lib

ifdef UMA_DELAYLOAD_LIBS
  delayloadname=$(if $($(1)_alllib),$($(1)_filename),$(1))
  UMA_DELAYLOAD_INSTRUCTIONS+=Delayimp.lib
  UMA_DELAYLOAD_INSTRUCTIONS+=$(foreach lib,$(UMA_DELAYLOAD_LIBS),-delayload:$(call delayloadname,$(lib)))
endif

ifndef UMA_DO_NOT_OPTIMIZE_CCODE
	UMA_OPTIMIZATION_FLAGS=/Ox
endif

CFLAGS+=$(UMA_OPTIMIZATION_FLAGS)
CXXFLAGS+=$(UMA_OPTIMIZATION_FLAGS)
ifdef USE_CLANG
  CLANG_CXXFLAGS+=-O3
endif
<#if uma.spec.processor.x86>
	UMA_SAFESEH=/SAFESEH
<#elseif uma.spec.processor.amd64>
	UMA_SAFESEH=
</#if>

<#if uma.spec.flags.size_smallCode.enabled>
UMA_VMLINK_OPTIONS+=/opt:nowin98 $(UMA_SAFESEH) /opt:icf,10 /opt:ref /map:$(UMA_TARGET_NAME).map
<#else>
UMA_VMLINK_OPTIONS+=/debug $(UMA_SAFESEH) /opt:icf /opt:ref
</#if>

ifdef UMA_NO_CONSOLE
  UMA_SUBSYSTEM_TYPE=windows
else
  UMA_SUBSYSTEM_TYPE=console
endif

UMA_RC_OPTIONS+=$(UMA_C_INCLUDES)

# from win32.mak - basic subsystem specific libraries, less the C Run-Time
UMA_EXELIBS+=kernel32.lib wsock32.lib advapi32.lib

ifdef UMA_NO_CONSOLE
  # from win32.mak
  UMA_EXELIBS+=user32.lib gdi32.lib comdlg32.lib winspool.lib
endif

ifeq ($(UMA_NO_CRT),1)
UMA_EXE_LINK_FLAGS+=/NODEFAULTLIB:MSVCRT
#UMA_EXELIBS+=libcmt.lib
else
UMA_EXE_LINK_FLAGS+=wsetargv.obj
endif # ($(UMA_NO_CRT),1)

<#if uma.spec.processor.amd64>
UMA_EXE_LINK_FLAGS+=/NODEFAULTLIB:MSVCRTD
</#if>

# from win32.mak - declarations common to all linker options
UMA_EXEFLAGS+=/INCREMENTAL:NO /NOLOGO /LARGEADDRESSAWARE

# Declare Flags

# Assembler flags
#	/c Assemble without linking
#	/Cp Preserve case of user identifiers
#	/W3 Set warning level
#	/nologo Suppress copyright message
#	/coff generate COFF format object file
#	/Zm Enable MASM 5.10 compatibility
#	/Zd Add line number info
#	/Zi Add symbolic debug info
#	/Gd Use C calls (i.e. prepend underscored to symbols)
ASFLAGS+=/c /Cp /W3 /nologo /Zd /Zi -DWIN32 -D_WIN32 -DOMR_OS_WINDOWS
<#if uma.spec.processor.x86>
ASFLAGS+=/safeseh /coff /Zm /Gd
<#elseif uma.spec.processor.amd64>
ASFLAGS+=-DWIN64 -D_WIN64 -DJ9HAMMER
</#if>

CFLAGS+=-DWIN32 -D_WIN32 -DOMR_OS_WINDOWS
CXXFLAGS+=-DWIN32 -D_WIN32 -DOMR_OS_WINDOWS
ifdef USE_CLANG
  CLANG_CXXFLAGS+=-DWIN32 -D_WIN32 -DOMR_OS_WINDOWS
endif

<#if uma.spec.processor.amd64>
  CFLAGS+=-D_AMD64_=1 -DWIN64 -D_WIN64 -DJ9HAMMER
  CXXFLAGS+=-D_AMD64_=1 -DWIN64 -D_WIN64 -DJ9HAMMER
  ifdef USE_CLANG
    CLANG_CXXFLAGS+=-D_AMD64_=1 -DWIN64 -D_WIN64 -DJ9HAMMER
  endif
<#elseif uma.spec.processor.x86>
  CFLAGS+=-D_X86_=1
  CXXFLAGS+=-D_X86_=1
  ifdef USE_CLANG
    CLANG_CXXFLAGS+=-D_X86_=1 -DJ9X86 -march=pentium4 -mtune=prescott -msse2
  endif
</#if>

# To support parallel make we write a PDB file for each source file
# e.g. foo.c creates foo.pdb
CFLAGS+=-Fd$*.pdb
CXXFLAGS+=-Fd$*.pdb

# We don't currently want CRT security warnings
CFLAGS+=-D_CRT_SECURE_NO_WARNINGS
CXXFLAGS+=-D_CRT_SECURE_NO_WARNINGS
ifdef USE_CLANG
  CLANG_CXXFLAGS+=-D_CRT_SECURE_NO_WARNINGS
endif

CFLAGS+=-DCRTAPI1=_cdecl -DCRTAPI2=_cdecl -nologo
CXXFLAGS+=-DCRTAPI1=_cdecl -DCRTAPI2=_cdecl -nologo
ifdef USE_CLANG
  CLANG_CXXFLAGS+=-DCRTAPI1=_cdecl -DCRTAPI2=_cdecl
endif

# Target Win 7 by default
ifndef UMA_WINVER
  UMA_WINVER=0x0601
endif

CFLAGS+=-DWINVER=$(UMA_WINVER) -D_WIN32_WINNT=$(UMA_WINVER) -D_WIN32_WINDOWS=$(UMA_WINVER)
CXXFLAGS+=-DWINVER=$(UMA_WINVER) -D_WIN32_WINNT=$(UMA_WINVER) -D_WIN32_WINDOWS=$(UMA_WINVER)
ifdef USE_CLANG
  CLANG_CXXFLAGS+=-DWINVER=$(UMA_WINVER) -D_WIN32_WINNT=$(UMA_WINVER) -D_WIN32_WINDOWS=$(UMA_WINVER)
endif

# -Zm200 max memory is 200% default
# -Zi add debug symbols
CFLAGS+=  -D_MT -D_WINSOCKAPI_ -Zm400 -W3 -Zi
CXXFLAGS+=-D_MT -D_WINSOCKAPI_ -Zm400 -W3 -Zi
ifdef USE_CLANG
  CLANG_CXXFLAGS+=-D_MT -D_WINSOCKAPI_
endif

# Determine if we are linking to static or dynamic CRT
ifeq ($(UMA_NO_CRT),1)
CFLAGS+=-Zl -Oi -Gs-
CXXFLAGS+=-Zl -Oi -Gs-
else
CFLAGS+=-MD -D_DLL
CXXFLAGS+=-MD -D_DLL
  ifdef USE_CLANG
	CLANG_CXXFLAGS+=-D_DLL
  endif
endif # ifeq ($(UMA_NO_CRT),1)


<#if !uma.spec.properties.uma_does_not_require_resource_files.defined>
ifeq ($(UMA_TARGET_TYPE),EXE)
UMA_OBJECTS+=$(UMA_TARGET_NAME).res
endif
ifeq ($(UMA_TARGET_TYPE),DLL)
UMA_OBJECTS+=$(UMA_TARGET_NAME).res
endif
</#if>

ifeq ($(UMA_NO_RES),1)
  UMA_OBJECTS:=$(filter-out %.res,$(UMA_OBJECTS))
  UMA_OBJECTS:=$(filter-out %.rc,$(UMA_OBJECTS))
endif

<#if !uma.spec.properties.uma_does_not_require_manifest_files.defined>
ifeq ($(UMA_TARGET_TYPE),EXE)
UMA_EXE_MANIFEST=$(UMA_TARGET_NAME).exe.manifest
endif
</#if>

ifdef UMA_TREAT_WARNINGS_AS_ERRORS
ifndef UMA_SUPPRESS_WARNINGS_AS_ERRORS
CFLAGS+=-WX
CXXFLAGS+=-WX
  ifdef USE_CLANG
	CLANG_CXXFLAGS+=-Werror -Wno-deprecated-declarations -Wno-ignored-attributes
  endif
endif
endif

ifdef UMA_ENABLE_ALL_WARNINGS
ifndef UMA_SUPPRESS_ALL_WARNINGS
CFLAGS+=
CXXFLAGS+=
endif
endif

ifdef UMA_COMPILING_OMR_GTEST_BASED_CODE
# special handling omr gtest
	CXXFLAGS+=/EHsc
endif
