# Copyright (c) 1998, 2017 IBM Corp. and others
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0

# List all flags that are enabled.
<#list uma.spec.flags as flag>
<#if flag.enabled>j9vm_${flag.name}=1<#else># j9vm_${flag.name}=0</#if>
</#list>

# Define the Java Version we are compiling
VERSION_MAJOR?=9
export VERSION_MAJOR

# Define a default target of the root directory for all targets.
ifndef UMA_TARGET_PATH
  UMA_TARGET_PATH=$(UMA_PATH_TO_ROOT)
endif

# Define all the tool used for compilation and linking.
<#if uma.spec.tools.interp_gcc.needed>INTERP_GCC=${uma.spec.tools.interp_gcc.name}<#else>#INTERP_GCC not used</#if>
<#if uma.spec.tools.as.needed>AS=${uma.spec.tools.as.name}<#else># AS not used</#if>
<#if uma.spec.tools.cc.needed>CC=${uma.spec.tools.cc.name}<#else># CC not used</#if>
<#if uma.spec.tools.cpp.needed>CPP=${uma.spec.tools.cpp.name}<#else># CPP not used</#if>
<#if uma.spec.tools.cxx.needed>CXX=${uma.spec.tools.cxx.name}<#else># CXX not used</#if>
<#if uma.spec.tools.rc.needed>RC=${uma.spec.tools.rc.name}<#else># RC not used</#if>
<#if uma.spec.tools.link.needed>LINK=${uma.spec.tools.link.name}<#else># LINK not used</#if>
<#if uma.spec.tools.mt.needed>MT=${uma.spec.tools.mt.name}<#else># MT not used</#if>
<#if uma.spec.tools.implib.needed>IMPLIB=${uma.spec.tools.implib.name}<#else># IMPLIB not used</#if>
<#if uma.spec.tools.ar.needed>AR=${uma.spec.tools.ar.name}<#else># AR not used</#if>
<#if uma.spec.tools.ranlib.needed>RANLIB=${uma.spec.tools.ranlib.name}<#else># RANLIB not used</#if>
<#if uma.spec.tools.dll_ld.needed>DLL_LD=${uma.spec.tools.dll_ld.name}<#else># DLL_LD not used</#if>
<#if uma.spec.tools.cxx_dll_ld.needed>CXX_DLL_LD=${uma.spec.tools.cxx_dll_ld.name}<#else># CXX_DLL_LD not used</#if>
<#if uma.spec.tools.exe_ld.needed>EXE_LD=${uma.spec.tools.exe_ld.name}<#else># EXE_LD not used</#if>
<#if uma.spec.tools.cxx_exe_ld.needed>CXX_EXE_LD=${uma.spec.tools.cxx_exe_ld.name}<#else># CXX_EXE_LD not used</#if>
<#if uma.spec.tools.dll_ld.needed>UMA_DLL_LD=$(if $(UMA_IS_C_PLUS_PLUS),$(CXX_DLL_LD),$(DLL_LD))</#if>
<#if uma.spec.tools.exe_ld.needed>UMA_EXE_LD=$(if $(UMA_IS_C_PLUS_PLUS),$(CXX_EXE_LD),$(EXE_LD))</#if>

ifdef UMA_CLANG
  CC=clang
  CXX=clang++
endif

# Define the JIT HOST type.
<#if uma.spec.processor.x86 || uma.spec.processor.amd64>
TR_HOST=TR_HOST_X86
<#elseif uma.spec.processor.arm>
TR_HOST=TR_HOST_ARM
<#elseif uma.spec.processor.ppc>
TR_HOST=TR_HOST_POWER
<#elseif uma.spec.processor.s390>
TR_HOST=TR_HOST_S390
</#if>

<#if uma.spec.flags.opt_cuda.enabled>
# Export the CUDA properties.
  export ENABLE_GPU := 1
  ifeq ($(VERSION_MAJOR),8)
    CUDA_VERSION := 5.5
  else
    # we will want to adopt a newer version for Java 9
    CUDA_VERSION := 5.5
  endif
<#if uma.spec.id?starts_with("win_x86-64")>
  export CUDA_HOME ?= $(DEV_TOOLS)/NVIDIA/CUDA/v$(CUDA_VERSION)
  export GDK_HOME  ?= $(DEV_TOOLS)/NVIDIA/gdk
<#elseif uma.spec.id?starts_with("linux_x86-64")>
  export CUDA_HOME ?= /usr/local/cuda-$(CUDA_VERSION)
  export GDK_HOME  ?= /usr/include/nvidia/gdk
<#elseif uma.spec.id?matches("linux_ppc-64.*_le.*")>
  export CUDA_HOME ?= /usr/local/cuda-$(CUDA_VERSION)
  export GDK_HOME  ?= /usr/include/nvidia/gdk
</#if>
</#if>

<#if uma.spec.properties.use_ld_to_link.defined>
ifndef UMA_IS_C_PLUS_PLUS
  UMA_USING_LD_TO_LINK=1
endif
</#if>

<#if uma.spec.type.windows>
# definitions for UMA_CPU
# can be overriden by makefile including this one.
ifndef UMA_CPU
<#if uma.spec.processor.amd64>
  UMA_CPU:=AMD64
<#elseif uma.spec.processor.arm>
  UMA_CPU:=ARM
<#else>
  UMA_CPU:=i386
</#if>
endif
</#if>

# All the supported source file suffixes.
UMA_SOURCE_SUFFIX_LIST+=.c
UMA_SOURCE_SUFFIX_LIST+=.cpp
UMA_SOURCE_SUFFIX_LIST+=.asm
<#if !(uma.spec.type.windows && !uma.spec.processor.arm)>
UMA_SOURCE_SUFFIX_LIST+=.s
</#if>
UMA_SOURCE_SUFFIX_LIST+=.pasm
UMA_SOURCE_SUFFIX_LIST+=.spp
UMA_SOURCE_SUFFIX_LIST+=.m4

# define the executable extension
<#if uma.spec.type.windows>
UMA_DOT_EXE=.exe
<#else>
UMA_DOT_EXE=
</#if>

# define the object extension
<#if uma.spec.type.windows>
UMA_DOT_O=.obj
<#else>
UMA_DOT_O=.o
</#if>

# gather all the object files together, this can be overriden by a module
#
UMA_OBJECTS:=$(foreach suffix,$(UMA_SOURCE_SUFFIX_LIST),$(patsubst %$(suffix),%$(UMA_DOT_O),$(wildcard *$(suffix))))
# Remove XXXexp.o from the object list.  Will be added, if needed, by the appropriate makefile.
UMA_OBJECTS:=$(UMA_OBJECTS:$(UMA_TARGET_NAME)exp$(UMA_DOT_O)=)

# The following dependencies are declared in targets.mk.ftl.
#
# $(UMA_OBJECTS) : $(UMA_OBJECTS_PREREQS)
#
# It would be nicer to express this sort of dependency in those module.xml
# files that need it (via <makefilestubs>), but UMA adds to UMA_OBJECTS in
# vpath elements that appear in makefiles after all makefilestubs, leaving
# those object files without dependencies on $(UMA_OBJECTS_PREREQS).
# We define UMA_OBJECTS_PREREQS here to avoid interference from the environment.
#
UMA_OBJECTS_PREREQS :=

<#if uma.spec.type.windows>
# Add resource object files to object list.
UMA_OBJECTS+=$(patsubst %.rc,%.res,$(wildcard *.rc))
UMA_OBJECTS+=$(patsubst %.mc,%.res,$(wildcard *.mc))
</#if>

<#if uma.spec.type.windows>
UMA_WINDOWS_PARRALLEL_HACK=-j $(NUMBER_OF_PROCESSORS)
</#if>

<#if uma.spec.type.windows>
<#if uma.spec.flags.build_VS12AndHigher.enabled>
VS12AndHigher:=1
</#if>
ifndef NO_USE_MINGW
USE_MINGW:=1
endif
ifdef USE_MINGW
<#if uma.spec.tools.mingw_cxx.needed>
MINGW_CXX=${uma.spec.tools.mingw_cxx.name}
<#else>
# MINGW_CXX not used
</#if>
endif
</#if>

<#if uma.spec.processor.ppc && uma.spec.type.linux && !uma.spec.flags.env_advanceToolchain.enabled && !uma.spec.flags.uma_codeCoverage.enabled>
ifndef NO_USE_PPC_GCC
USE_PPC_GCC:=1
endif
ifdef USE_PPC_GCC
<#if uma.spec.tools.ppc_gcc_cxx.needed>
PPC_GCC_CXX=${uma.spec.tools.ppc_gcc_cxx.name}
<#else>
# PPC_GCC_CXX not used
</#if>
endif
</#if>
