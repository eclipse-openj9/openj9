# Copyright (c) 1998, 2019 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception

# The following section converts Shared Libraries to Static Libraries
# when doing a static build.

OMRGLUE=$(UMA_PATH_TO_ROOT)oti $(UMA_PATH_TO_ROOT)include $(UMA_PATH_TO_ROOT)gc_glue_java $(UMA_PATH_TO_ROOT)gc_base $(UMA_PATH_TO_ROOT)gc_include $(UMA_PATH_TO_ROOT)gc_stats $(UMA_PATH_TO_ROOT)gc_structs
include $(UMA_PATH_TO_ROOT)makelib/uma_macros.mk

# Function to convert a system library name into a link command (i.e. m into -lm)
addSystemLib=
<#if uma.spec.type.unix>
UMA_SYSTEM_LIB_PREFIX=-l
UMA_EXTERNAL_LIB_PREFIX=-l
<#else>
# must be a windows system
UMA_EXTERNAL_LIB_PREFIX=$(UMA_PATH_TO_ROOT)lib/
UMA_EXTERNAL_LIB_SUFFIX=.lib
</#if>
addSystemLib=$(UMA_SYSTEM_LIB_PREFIX)$(1)
decorateExternalLib=$(UMA_EXTERNAL_LIB_PREFIX)$(1)$(UMA_EXTERNAL_LIB_SUFFIX)

# Function to return the dependencies of an UMA artifact library
convertToDepency=$(if $($(1)_alllib),$($(1)_depend))

# Function to convert a library name to a linker command.
convertToLink=$(if $($(1)_alllib),$($(1)_link),$(call addSystemLib,$(1)))

# Library Dependencies and linker commands
# The following section converts short library names (e.g., j9util)
# into the dependency and link equivalents (e.g., ../lib/j9util.a and -lj9util

# Target libraries which must be built before the current artifact
UMA_TARGET_LIBRARIES:=$(foreach lib,$(UMA_LIBRARIES),$(call convertToDepency,$(lib)))

# Externally built libraries
UMA_LINK_EXTERNAL_LIBS:=$(UMA_EXTERNAL_LIBRARIES)
UMA_LINK_EXTERNAL_LIBRARIES:=$(foreach lib, $(UMA_LINK_EXTERNAL_LIBS),$(call decorateExternalLib,$(lib)))

# All static libraries.  Includes external libraries.
UMA_LINK_STATIC_LIBS:=$(foreach lib,$(UMA_LIBRARIES),$($(lib)_staticlib))
UMA_LINK_STATIC_LIBRARIES:=$(foreach lib,$(UMA_LINK_STATIC_LIBS),$(call convertToLink,$(lib)))
UMA_LINK_STATIC_LIBRARIES+=$(UMA_LINK_EXTERNAL_LIBRARIES)

# System libraries.
UMA_LINK_SYSTEM_LIBS:=$(foreach lib,$(UMA_LIBRARIES),$(if $($(lib)_alllib),,$(lib)))

# All shared libraries.  Includes system libraries
UMA_LINK_SHARED_LIBS:=$(foreach lib,$(UMA_LIBRARIES),$($(lib)_sharedlib))
UMA_LINK_SHARED_LIBRARIES:=$(foreach lib,$(UMA_LINK_SHARED_LIBS),$(call convertToLink,$(lib)))
UMA_LINK_SHARED_LIBRARIES+=$(foreach lib,$(UMA_LINK_SYSTEM_LIBS),$(call convertToLink,$(lib)))

# All libraries. Includes all shared and static libraries.
UMA_LINK_LIBRARIES:=$(UMA_LINK_SHARED_LIBRARIES) $(UMA_LINK_STATIC_LIBRARIES)

# Define the static library target name (e.g., ../lib/j9util.a)

ifeq ($(UMA_TARGET_TYPE),LIB)
  UMA_LIBTARGET:=$($(UMA_TARGET_NAME)_ondisk)
endif

# Define the shared library target name (e.g., ../lib/j9thr24.so)

ifeq ($(UMA_TARGET_TYPE),DLL)
  UMA_DLLFILENAME:=$($(UMA_TARGET_NAME)_filename)
  UMA_DLLTARGET:=$($(UMA_TARGET_NAME)_ondisk)
endif
<#if uma.spec.type.windows>
ifeq ($(UMA_TARGET_TYPE),DLL)
  UMA_LIBTARGET:=$($(UMA_TARGET_NAME)_link)
endif
</#if>

# Define the executable library target name (e.g., ../j9.exe)

ifeq ($(UMA_TARGET_TYPE),EXE)
  UMA_EXETARGET:=$(UMA_TARGET_PATH)$(UMA_TARGET_NAME)$(UMA_DOT_EXE)
endif

ifeq ($(OPENJ9_BUILD),true)
CFLAGS+=-DOPENJ9_BUILD
CXXFLAGS+=-DOPENJ9_BUILD
CPPFLAGS+=-DOPENJ9_BUILD
endif

# Declare the 'all' target

TARGETS+=$(UMA_LIBTARGET) $(UMA_DLLTARGET) $(UMA_EXETARGET)

all: $(TARGETS)

# Write Platform specific flags.
<#if uma.spec.type.windows>
<#include "targets.mk.win.inc.ftl">
<#elseif uma.spec.type.zos>
<#include "targets.mk.zos.inc.ftl">
<#elseif uma.spec.type.aix>
<#include "targets.mk.aix.inc.ftl">
<#elseif uma.spec.type.ztpf>
<#include "targets.mk.ztpf.inc.ftl">
<#elseif uma.spec.type.linux>
<#include "targets.mk.linux.inc.ftl">
<#elseif uma.spec.type.osx>
<#include "targets.mk.osx.inc.ftl">
</#if>
<#include "targets.mk.vendor.inc.ftl">

# Add OMR include paths
#  Note assumptions about the directory layout of the OMR project.
UMA_INCLUDES+=$(UMA_PATH_TO_ROOT)nls
UMA_INCLUDES+=$(OMR_DIR)/include_core

<#if !uma.spec.type.aix>
# Create the include directives for the assembler.
<#if uma.spec.type.windows>
UMA_ASM_INCLUDE_PREFIX=/I
<#else>
UMA_ASM_INCLUDE_PREFIX=-I
</#if>
UMA_ASM_INCLUDES:=$(addprefix $(UMA_ASM_INCLUDE_PREFIX),$(UMA_INCLUDES)) $(UMA_ASM_INCLUDES)
ASFLAGS+=$(UMA_ASM_INCLUDES)
</#if>

# Create the include directives for the c and c++ compilers, and the c pre-processor.
<#if uma.spec.type.windows>
UMA_C_INCLUDE_PREFIX=/I
<#else>
UMA_C_INCLUDE_PREFIX=-I
</#if>
<#if uma.spec.type.windows>
ifdef USE_CLANG
  UMA_C_INCLUDE_PREFIX=-I
endif
</#if>
# Add these flags later on if using a z/tpf spec file.
<#if !uma.spec.type.ztpf>
UMA_C_INCLUDES:=$(addprefix $(UMA_C_INCLUDE_PREFIX),$(UMA_INCLUDES)) $(UMA_C_INCLUDES)
CFLAGS+=$(UMA_C_INCLUDES)
CXXFLAGS+=$(UMA_C_INCLUDES)
CPPFLAGS+=$(UMA_C_INCLUDES)
</#if>
<#if uma.spec.type.ztpf && uma.spec.properties.crossCompilerPath.defined>
# Put the required z/TPF tools on the path.
space :=
space +=
PATH := $(subst $(space),:,$(foreach d,$(TPF_ROOT),$d/linux/bin)):<#noparse>$(PATH)</#noparse>

TPF_INCLUDES := $(foreach d,$(TPF_ROOT),-I$d/base/a2e/headers)
TPF_INCLUDES += $(foreach d,$(TPF_ROOT),-I$d/base/include)
TPF_INCLUDES += $(foreach d,$(TPF_ROOT),-I$d/opensource/include)
TPF_INCLUDES += $(foreach d,$(TPF_ROOT),-I$d/opensource/include46/g++)
TPF_INCLUDES += $(foreach d,$(TPF_ROOT),-I$d/opensource/include46/g++/backward)
TPF_INCLUDES += $(foreach d,$(TPF_ROOT),-I$d/noship/include)
TPF_INCLUDES += $(foreach d,$(TPF_ROOT),-isystem $d/opensource/include)
TPF_INCLUDES += $(foreach d,$(TPF_ROOT),-isystem $d/noship/include)
TPF_INCLUDES += $(foreach d,$(TPF_ROOT),-isystem $d)

TPF_FLAGS := -D_GNU_SOURCE -DIBM_ATOE -D_TPF_SOURCE -DZTPF_POSIX_SOCKET -DJ9ZTPF
TPF_FLAGS += -fexec-charset=ISO-8859-1 -fmessage-length=0 -funsigned-char -fverbose-asm -fno-builtin-abort -fno-builtin-exit -fno-builtin-sprintf -ffloat-store -gdwarf-2 -Wno-format-extra-args -Wno-int-to-pointer-cast -Wno-unknown-pragmas -Wno-unused-but-set-variable -Wno-write-strings
TPF_FLAGS += -Wno-unused
TPF_FLAGS += -fno-delete-null-pointer-checks -fno-tree-dse -fno-lifetime-dse -fno-optimize-strlen
TPF_C_FLAGS := -std=gnu90
</#if>

<#if uma.spec.type.ztpf && uma.spec.properties.tpfRoot.defined && uma.spec.properties.tpfProj.defined>

CFLAGS += $(TPF_FLAGS) $(TPF_C_FLAGS) $(TPF_INCLUDES) -iquote ../include
CXXFLAGS += $(TPF_FLAGS) $(TPF_INCLUDES)
CPPFLAGS+= -I$(UMA_PATH_TO_ROOT)oti

# Add these flags now since we're using a z/tpf spec file.
UMA_C_INCLUDES:=$(addprefix $(UMA_C_INCLUDE_PREFIX),$(UMA_INCLUDES)) $(UMA_C_INCLUDES)
CFLAGS+=$(UMA_C_INCLUDES)
CXXFLAGS+=$(UMA_C_INCLUDES)
CPPFLAGS+=$(UMA_C_INCLUDES)

UMA_LINK_PATH += $(foreach d,$(TPF_ROOT),-L$d/base/lib)
UMA_LINK_PATH += $(foreach d,$(TPF_ROOT),-L$d/base/stdlib)
UMA_LINK_PATH += $(foreach d,$(TPF_ROOT),-L$d/opensource/stdlib)

</#if>

<#if uma.spec.type.windows>
ifdef USE_CLANG
  CLANG_CXXFLAGS+=$(UMA_C_INCLUDES)
  ifneq ($(OPENJ9_BUILD),true)
    CLANG_INCLUDES:="$(subst ;," -I",$(INCLUDE))"
    CLANG_CXXFLAGS+=$(UMA_C_INCLUDE_PREFIX)$(CLANG_INCLUDES)
  endif # OPENJ9_BUILD
endif
</#if>
<#if uma.spec.processor.ppc>
ifdef USE_PPC_GCC
  PPC_GCC_CXXFLAGS+=$(UMA_C_INCLUDES)
endif
</#if>

#remove duplicate objects
UMA_OBJECTS:=$(sort $(UMA_OBJECTS))

# Determine if we need to add a copyright file to the object list.
ifeq ($(UMA_TARGET_TYPE),DLL)
  UMA_ADD_COPYRIGHT=1
endif

ifeq ($(UMA_TARGET_TYPE),EXE)
  UMA_ADD_COPYRIGHT=1
endif

ifdef UMA_ADD_COPYRIGHT
  # add copyright file to objects to built/cleaned
  ifndef UMA_COPYRIGHT_FILE
    UMA_COPYRIGHT_FILE := copyright
  endif
  vpath $(UMA_COPYRIGHT_FILE).c $(UMA_PATH_TO_ROOT)makelib
# Ensure that the copyright file is not already a part of the object list and then add it to the front.
  UMA_OBJECTS:=$(filter-out $(UMA_COPYRIGHT_FILE)$(UMA_DOT_O),$(UMA_OBJECTS))
  UMA_OBJECTS:=$(UMA_COPYRIGHT_FILE)$(UMA_DOT_O) $(UMA_OBJECTS)
endif

<#if uma.spec.type.unix>
UMA_LINK_PATH+=-L. -L$(UMA_PATH_TO_ROOT) -L$(UMA_PATH_TO_ROOT)lib/
UMA_BUILD_LIBRARIES:=$(foreach lib,$(UMA_LIBRARIES),$(if $($(lib)_alllib),$(lib)))
UMA_LINK_PATH+=$(foreach lib,$(UMA_BUILD_LIBRARIES),-L$($(lib)_path))
<#if !uma.spec.type.ztpf>
# Remove duplicate paths and sort, but don't do this for z/TPF because z/TPF is
# sensitive to link order for some scenarios.
UMA_LINK_PATH:=$(sort $(UMA_LINK_PATH))
</#if>
</#if>

CFLAGS+=$(A_CFLAGS)
CXXFLAGS+=$(A_CXXFLAGS)
<#if uma.spec.type.windows>
ifdef USE_CLANG
  CLANG_CXXFLAGS+=$(A_CXXFLAGS)
endif
</#if>
CPPFLAGS+=$(A_CPPFLAGS)
ASFLAGS+=$(A_ASFLAGS)

CPPFLAGS_NOCOV:=$(CPPFLAGS)
<#if uma.spec.flags.uma_codeCoverage.enabled>
ifndef UMA_IGNORE_CODECOV
CFLAGS+=-fprofile-arcs -ftest-coverage
CXXFLAGS+=-fprofile-arcs -ftest-coverage
CPPFLAGS+=-fprofile-arcs -ftest-coverage
endif
UMA_DLL_LINK_FLAGS+=-fprofile-arcs -ftest-coverage
UMA_EXE_PREFIX_FLAGS+=-fprofile-arcs -ftest-coverage
</#if>

# Add possibility of debug flags
CFLAGS+=$(VMDEBUG)
CXXFLAGS+=$(VMDEBUG)
<#if uma.spec.type.windows>
ifdef USE_CLANG
  CLANG_CXXFLAGS+=$(VMDEBUG)
endif
</#if>
<#if uma.spec.processor.ppc>
ifdef USE_PPC_GCC
  PPC_GCC_CXXFLAGS+=$(VMDEBUG)
endif
</#if>
<#if !uma.spec.type.aix>
<#-- Don't add VMASMDEBUG to the $(AS) assembler.
  VMASMDEBUG is used on the aspp assembler line which has
  incompatible options with the as assembler.-->
ASFLAGS+=$(VMASMDEBUG)
</#if>
<#if uma.spec.type.ztpf>
#we may need absolute path for debugger and would have to update $@ to include an obj dir: $(PWD)/$@
LIBCDEFS := $(word 1,$(wildcard $(foreach d,$(TPF_ROOT),$d/base/lib/libCDEFSFORASM.so)))

# compilation rule for C files.
%$(UMA_DOT_O): %.c
###	$(CC) $(CFLAGS) -c -o $@ $< > $*.asmlist
	$(CC) $(CFLAGS) -c -Wa,-alshd=$*.lst -o $@ $<
	tpfobjpp -O $(if $(filter -O%,$(CFLAGS)),$(patsubst -%,%,$(filter -O%,$(CFLAGS))),O3) -g $(if $(filter -g2,$(CFLAGS)),$(patsubst -%,%,$(filter -g2,$(CFLAGS))),gNotApplicable) $@

# compilation rule for C++ files.
%$(UMA_DOT_O): %.cpp
###	$(CXX) $(CXXFLAGS) -c $< > $*.asmlist
	$(CXX) $(CXXFLAGS) -c -Wa,-alshd=$*.lst -o $@ $<
	tpfobjpp -O $(if $(filter -O%,$(CXXFLAGS)),$(patsubst -%,%,$(filter -O%,$(CXXFLAGS))),O3) -g $(if $(filter -g2,$(CXXFLAGS)),$(patsubst -%,%,$(filter -g2,$(CXXFLAGS))),gNotApplicable) $@

# compilation rule for precompiled listings, for C and C++ files.
%$(UMA_DOT_I): %.c
	$(CC) $(CFLAGS) -E -dDI -o $@ $<

%$(UMA_DOT_II): %.cpp
	$(CXX) $(CXXFLAGS) -E -dDI -o $@ $<

</#if>
<#if uma.spec.type.zos>
# compilation rule for C files.
%$(UMA_DOT_O): %.c
	$(CC) $(CFLAGS) -c -o $@ $< > $*.asmlist

# compilation rule for metal-C files.
%$(UMA_DOT_O): %.mc
	cp $< $*.c
	xlc $(MCFLAGS) -qnosearch -I /usr/include/metal/ -qmetal -qlongname -S -o $*.s $*.c > $*.asmlist
	rm -f $*.c
	as -mgoff $(UMA_MCASM_INCLUDES) $*.s
	rm -f $*.s

# compilation rule for C++ files.
%$(UMA_DOT_O): %.cpp
	$(CXX) $(CXXFLAGS) -c $< > $*.asmlist
<#elseif !uma.spec.type.ztpf>

# compilation rule for C files.
%$(UMA_DOT_O): %.c
<#if uma.spec.type.windows>
	$(CC) $(CFLAGS) -c $<
<#else>
	$(CC) $(CFLAGS) -c -o $@ $<
</#if>

# compilation rule for C++ files.
%$(UMA_DOT_O): %.cpp
		$(CXX) $(CXXFLAGS) -c $<
</#if>

# ddrgen preprocessor targets
# We use sed in these scripts to retain only those lines that will be interesting
# to omr/ddr/tools/getmacros; this reduces the total footprint of all *.i files by
# over 1GB and reduces the workload of getmacros as it reads them.

DDR_SED_COMMAND := \
	sed -n -e '/^DDRFILE_BEGIN /,/^DDRFILE_END /s/^/@/' -e '/^@./p'

# On z/OS, CFLAGS and CXXFLAGS contain '-Wc,convlit(ISO8859-1)' and '-Wc,list,offset'
# which are incompatible with the use of '-E' below, trigger numerous warnings.
# The solution is to use '-Wc,noconvlit' and '-Wc,nolist,nooffset' to negate those options.

DDR_NOLIST := <#if uma.spec.type.zos>-Wc,noconvlit -Wc,nolist,nooffset</#if>

%.i : %.c
	$(CC) $(CFLAGS) $(DDR_NOLIST) -E $< | $(DDR_SED_COMMAND) > $@

%.i : %.cpp
	$(CXX) $(CXXFLAGS) $(DDR_NOLIST) -E $< | $(DDR_SED_COMMAND) > $@

# just create empty output files
%.i : %.asm ; touch $@
%.i : %.m4  ; touch $@
%.i : %.mc  ; touch $@
%.i : %.s   ; touch $@

<#if uma.spec.type.windows>
# compilation rule for resource files.
%.res: %.rc
	$(RC) $(UMA_RC_OPTIONS) $<

# compilation rule for message text files
%.res: %.mc
	mc $<
	$(RC) $(UMA_RC_OPTIONS) $*.rc
	$(RM) $*.rc

</#if>
<#if !uma.spec.type.windows>
# compilation rule for .s files
<#if uma.spec.type.aix>
%$(UMA_DOT_O): %.s
	$(AS) $(ASFLAGS) $<
<#elseif uma.spec.type.zos>
%$(UMA_DOT_O): %.s
	$(CC) -Wa,list $(CFLAGS) -c $< > $*.asmlist
<#elseif uma.spec.type.ztpf>
%$(UMA_DOT_O): %.s
	$(AS) $(ASFLAGS) -alshd=$*.lst -o $@ $<
	tpfobjpp -O ONotApplicable -g gNotApplicable -f $(LIBCDEFS) $@
<#else>
%$(UMA_DOT_O): %.s
	$(AS) $(ASFLAGS) -o $@ $<
</#if>

</#if>
<#if !(uma.spec.type.aix || uma.spec.type.zos)>
# compilation rule for .asm files
<#if uma.spec.type.windows>
%$(UMA_DOT_O): %.asm
	$(AS) $(ASFLAGS) $<
<#elseif uma.spec.type.ztpf>
%$(UMA_DOT_O): %.asm
	perl $(UMA_PATH_TO_ROOT)compiler/makelib/masm2gas.pl $(UMA_MASM2GAS_FLAGS) $(UMA_C_INCLUDES) $*.asm
	$(AS) $(ASFLAGS) -alshd=$*.lst -o $*.o $*.s
	-rm $*.s
	tpfobjpp -O ONotApplicable -g gNotApplicable -f $(LIBCDEFS) $@
<#else>
%$(UMA_DOT_O): %.asm
	perl $(UMA_PATH_TO_ROOT)makelib/masm2gas.pl $(UMA_MASM2GAS_FLAGS) $(UMA_C_INCLUDES) $*.asm
	$(AS) $(ASFLAGS) -o $*.o $*.s
	-mv -f $*.s $*.hold
</#if>

</#if>
<#if !uma.spec.type.aix && !uma.spec.type.zos>
# compilation rule for .pasm files
<#if uma.spec.type.windows>
 # Jazz 41210 : the use of /I confuses the bash shell use -I instead and avoid shell redirection
UMA_PASM_INCLUDES:=$(addprefix -I ,$(UMA_INCLUDES))

%$(UMA_DOT_O): %.pasm
	$(CPP) -P -Fi$*.SPP $(UMA_PASM_INCLUDES) $*.PASM
	$(AS) $(ASFLAGS) $*.SPP
	-rm -f $*.SPP
<#else>
%$(UMA_DOT_O): %.pasm
	$(CPP) $(UMA_C_INCLUDES) -o $*.spp $*.pasm
	perl $(UMA_PATH_TO_ROOT)makelib/masm2gas.pl $(UMA_MASM2GAS_FLAGS) $(UMA_C_INCLUDES) $*.spp
	$(AS) $(ASFLAGS) -o $*.o $*.s
	-rm $*.s
	-rm -f $*.spp
</#if>

</#if>
<#if uma.spec.type.aix>

# compilation rule for .spp files - translate ! to newline
%$(UMA_DOT_O): %.spp
	$(CPP) $(CFLAGS) -o $*.ipp $*.spp
	tr ! '\n' < $*.i > $*.s
	-rm -f $*.i
	$(AS) $(ASFLAGS) -o $*.o $*.s
	-rm -f $*.s

#compilation rule for .m4 files
%$(UMA_DOT_O): %.m4
	m4 -DAIXPPC $(UMA_M4_FLAGS) $(UMA_C_INCLUDES) $< > $*.s
	$(AS) $(ASFLAGS) -o $*.o $*.s
	-mv -f $*.s $*.hold
</#if>

<#if uma.spec.type.linux || uma.spec.type.osx>
# compilation rule for .spp files - translate ! to newline and ^ to #
%$(UMA_DOT_O): %.spp
	$(CPP) $(CPPFLAGS) -o $*.i $*.spp
	tr ! '\n' < $*.i > $*.i2
	-rm -f $*.i
	tr '\136' '\043' < $*.i2 > $*.s
	-rm -f $*.i2
	$(AS) $(ASFLAGS) -o $*.o $*.s
	-rm -f $*.s
</#if>

<#if uma.spec.type.osx>
#compilation rule for .m4 files
%$(UMA_DOT_O): %.m4
	m4 -DOSX $(UMA_M4_FLAGS) $(UMA_C_INCLUDES) $< > $*.s
	$(AS) $(ASFLAGS) -o $*.o $*.s
	-mv -f $*.s $*.hold
</#if>

<#if uma.spec.type.linux>
#compilation rule for .m4 files
%$(UMA_DOT_O): %.m4
	m4 $(UMA_M4_FLAGS) $(UMA_C_INCLUDES) $< > $*.s
	$(AS) $(ASFLAGS) -o $*.o $*.s
	-mv -f $*.s $*.hold
</#if>

<#if uma.spec.type.zos>
#compilation rule for .m4 files
%$(UMA_DOT_O): %.m4
	m4 -DJ9ZOS390 -DJ9VM_TIERED_CODE_CACHE $(UMA_M4_FLAGS) $(UMA_C_INCLUDES) $< > $*.s
	$(AS) -DJ9ZOS390=1 -Wa,goff -Wa,"SYSPARM(BIT64)" $(UMA_ASM_INCLUDES) -c -o $*.o $*.s
	-mv -f $*.s $*.hold
</#if>

<#if uma.spec.type.ztpf>
%$(UMA_DOT_O): %.m4
	m4 $(CPPFLAGS_NOCOV) $(UMA_M4_FLAGS) $*.m4 > $*.s
	$(AS) $(ASFLAGS) -alshd=$*.lst $*.s
	-rm $*.s
	tpfobjpp -O ONotApplicable -g gNotApplicable -f $(LIBCDEFS) $@
</#if>

<#if uma.spec.type.windows>
ifdef USE_CLANG
UMA_M4_INCLUDES = $(UMA_C_INCLUDES)
else
UMA_M4_INCLUDES = $(patsubst /I%,-I%,$(UMA_C_INCLUDES))
endif

#compilation rule for .m4 files
%$(UMA_DOT_O): %.m4
	m4 -DWIN32 $(UMA_M4_FLAGS) $(UMA_M4_INCLUDES) $< > $*.asm
	$(AS) $(ASFLAGS) $*.asm
	-mv -f $*.asm $*.hold

ifdef USE_CLANG
CLANG_CXXFLAGS+=-fno-rtti -fno-threadsafe-statics -fno-strict-aliasing -fno-exceptions -fno-asynchronous-unwind-tables
ifdef VS12AndHigher
CLANG_CXXFLAGS+=-std=c++0x -D_CRT_SUPPRESS_RESTRICT -DVS12AndHigher
<#if uma.spec.processor.x86>
	CLANG_CXXFLAGS+=-D_M_IX86
<#elseif uma.spec.processor.amd64>
	CLANG_CXXFLAGS+=-D_M_X64
</#if>
endif
# special handling BytecodeInterpreter.cpp and DebugBytecodeInterpreter.cpp
BytecodeInterpreter$(UMA_DOT_O):BytecodeInterpreter.cpp
	$(CLANG_CXX) $(CLANG_CXXFLAGS) -c $< -o $@

DebugBytecodeInterpreter$(UMA_DOT_O):DebugBytecodeInterpreter.cpp
	$(CLANG_CXX) $(CLANG_CXXFLAGS) -c $< -o $@

MHInterpreter$(UMA_DOT_O):MHInterpreter.cpp
	$(CLANG_CXX) $(CLANG_CXXFLAGS) -c $< -o $@

endif
</#if>

<#if uma.spec.type.linux && uma.spec.processor.x86>
cnathelp$(UMA_DOT_O):cnathelp.cpp
	$(CXX) $(CXXFLAGS) -mstackrealign -c $<
</#if>

# JIT helper methods require us to disable buffer security checks on Windows
<#if uma.spec.type.windows>
cnathelp$(UMA_DOT_O):cnathelp.cpp
	$(CXX) $(CXXFLAGS) /GS- -c $<
SharedService$(UMA_DOT_O):SharedService.c
	$(CC) $(CFLAGS) /GS- -c $<
</#if>

<#if uma.spec.processor.ppc>
ifndef USE_PPC_GCC
# special handling BytecodeInterpreter.cpp and DebugBytecodeInterpreter.cpp
FLAGS_TO_REMOVE += -O3
NEW_OPTIMIZATION_FLAG=-O2 -qdebug=lincomm:ptranl:tfbagg
<#if uma.spec.type.linux>
FLAGS_TO_REMOVE += -qpic=large
NEW_OPTIMIZATION_FLAG+=-qmaxmem=-1 -qpic
</#if>
SPECIALCXXFLAGS=$(filter-out $(FLAGS_TO_REMOVE),$(CXXFLAGS))

BytecodeInterpreter$(UMA_DOT_O):BytecodeInterpreter.cpp
	$(CXX) $(SPECIALCXXFLAGS) $(NEW_OPTIMIZATION_FLAG) -c $<

DebugBytecodeInterpreter$(UMA_DOT_O):DebugBytecodeInterpreter.cpp
	$(CXX) $(SPECIALCXXFLAGS) $(NEW_OPTIMIZATION_FLAG) -c $<

MHInterpreter$(UMA_DOT_O):MHInterpreter.cpp
	$(CXX) $(SPECIALCXXFLAGS) $(NEW_OPTIMIZATION_FLAG) -c $<

endif
<#if uma.spec.flags.env_littleEndian.enabled && !uma.spec.flags.env_gcc.enabled>
# special handling of fltconv.c
# This is a work around for a compiler defect (see JAZZ 76038)
fltconv$(UMA_DOT_O):fltconv.c
	$(CC) $(CFLAGS) -qdebug=ndi -c -o $@ $<
</#if>
</#if>

# Static Library Target Rule(s)

ifeq ($(UMA_TARGET_TYPE),LIB)
${lib_target_rule}
endif

# Shared Library Target Rule(s)

ifeq ($(UMA_TARGET_TYPE),DLL)
${dll_target_rule}
endif

# Executable Target Rule(s)

ifeq ($(UMA_TARGET_TYPE),EXE)
${exe_target_rule}
endif

# Convenience target that allows rebuilding of the makefiles from
# any directory.  Not required to build.
uma:
	$(MAKE) -C $(UMA_PATH_TO_ROOT) uma

ifndef UMA_NO_CLEAN_TARGET
#
# standard clean directive
#
clean:
	$(RM) $(UMA_OBJECTS) $(UMA_BYPRODUCTS) $(TARGETS)
	-$(RM) *.d $(UMA_OBJECTS:$(UMA_DOT_O)=.i)
<#if uma.spec.type.ztpf>
	-$(RM) *.d $(UMA_OBJECTS:$(UMA_DOT_O)=.lst)
</#if>
<#if uma.spec.type.windows>
	$(RM) *.pdb
</#if>
endif

ifneq (1,$(UMA_DISABLE_DDRGEN))
ddrgen: $(UMA_OBJECTS:$(UMA_DOT_O)=.i)
endif

# It would be nicer to express this sort of dependency in those module.xml
# files that need it (via <makefilestubs>), but UMA adds to UMA_OBJECTS in
# vpath elements that appear in makefiles after all makefilestubs, leaving
# those object files without dependencies on $(UMA_OBJECTS_PREREQS).
#
ifneq (,$(and $(strip $(UMA_OBJECTS)),$(strip $(UMA_OBJECTS_PREREQS))))
$(UMA_OBJECTS) : $(UMA_OBJECTS_PREREQS)
endif

.PHONY : all clean ddrgen

<#if uma.spec.type.windows || uma.spec.type.linux || uma.spec.type.osx>
# GNU make magic, replace the .o in UMA_OBJECTS with .d's
UMA_DEPS := $(filter-out %.res,$(UMA_OBJECTS))
UMA_DEPS := $(UMA_DEPS:$(UMA_DOT_O)=.d)
show_deps:
	echo "Dependencies are: $(UMA_DEPS)"
-include $(UMA_DEPS)
</#if>

EXTRA_FLAGS=-DUT_DIRECT_TRACE_REGISTRATION -D$(TR_HOST)

CFLAGS+=$(EXTRA_FLAGS)
CXXFLAGS+=$(EXTRA_FLAGS)
CPPFLAGS+=$(EXTRA_FLAGS)
CLANG_CXXFLAGS+=$(EXTRA_FLAGS)
PPC_GCC_CXXFLAGS+=$(EXTRA_FLAGS)
