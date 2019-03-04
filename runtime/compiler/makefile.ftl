#        Copyright (c) 2000, 2019 IBM Corp. and others
#
#        This program and the accompanying materials are made available under
#        the terms of the Eclipse Public License 2.0 which accompanies this
#        distribution and is available at https://www.eclipse.org/legal/epl-2.0/
#        or the Apache License, Version 2.0 which accompanies this distribution and
#        is available at https://www.apache.org/licenses/LICENSE-2.0.
#
#        This Source Code may also be made available under the following
#        Secondary Licenses when the conditions for such availability set
#        forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
#        General Public License, version 2 with the GNU Classpath
#        Exception [1] and GNU General Public License, version 2 with the
#        OpenJDK Assembly Exception [2].
#
#        [1] https://www.gnu.org/software/classpath/license.html
#        [2] http://openjdk.java.net/legal/assembly-exception.html
#
#        SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
#

# top-level makefile for J9 JIT

include ../makelib/mkconstants.mk

export J9_VERSION ?=${uma.buildinfo.version.major}${uma.buildinfo.version.minor}
export TR_BUILD_NAME=${uma.buildinfo.jit_buildtag}

# OMR_DIR needs to be set to what is expected in tr.open because its not the same omr repo that j9vm uses
# this line needs to be removed once the omr submodule is removed from use within tr.open
#unexport OMR_DIR

ifneq (,$(findstring CYGWIN, $(shell uname)))
export J9SRC=$(shell cygpath -w -a $(CURDIR)/..)
else
export J9SRC=$(CURDIR)/..
endif
export JIT_SRCBASE=$(J9SRC)
export JIT_OBJBASE=$(J9SRC)/objs
export JIT_DLL_DIR=$(J9SRC)
export OMR_DIR=$(J9SRC)/omr
export BUILD_CONFIG?=prod

# Define the JIT PLATFORM

<#if uma.spec.id?starts_with("win_x86-64")>
  export PLATFORM=amd64-win64-mvs
<#elseif uma.spec.id?starts_with("win_x86")>
  export PLATFORM=ia32-win32-mvs
</#if>
<#if uma.spec.id?starts_with("aix_ppc-64")>
  export PLATFORM=ppc64-aix-vacpp
<#elseif uma.spec.id?starts_with("aix_ppc")>
  export PLATFORM=ppc-aix-vacpp
</#if>
<#if uma.spec.id?starts_with("zos_390-64")>
  export PLATFORM=s390-zos64-vacpp
  export ASMLIST=1
<#elseif uma.spec.id?starts_with("zos_390")>
  export PLATFORM=s390-zos-vacpp
  export ASMLIST=1
</#if>
<#if uma.spec.id?starts_with("linux_x86-64")>
  export PLATFORM=amd64-linux64-gcc
<#elseif uma.spec.id?starts_with("linux_x86")>
  export PLATFORM=amd64-linux-gcc
</#if>
<#if uma.spec.id?starts_with("linux_ppc-64")>
  <#if uma.spec.flags.env_gcc.enabled>
    export PLATFORM=ppc64-linux64-gcc
  <#elseif uma.spec.flags.env_littleEndian.enabled>
    export PLATFORM=ppc64le-linux64-vacpp
  <#else>
    export PLATFORM=ppc64-linux64-vacpp
  </#if>
<#elseif uma.spec.id?starts_with("linux_ppc")>
  <#if uma.spec.flags.env_gcc.enabled>
    export PLATFORM=ppc-linux-gcc
  <#else>
    export PLATFORM=ppc-linux-vacpp
  </#if>
</#if>
<#if uma.spec.id?starts_with("linux_390-64")>
  export PLATFORM=s390-linux64-gcc
<#elseif uma.spec.id?starts_with("linux_390")>
  export PLATFORM=s390-linux-gcc
<#elseif uma.spec.id?starts_with("linux_arm_linaro")>
  export PLATFORM=arm-linux-gcc-cross
</#if>
<#if uma.spec.id?starts_with("linux_aarch64")>
  export PLATFORM=aarch64-linux-gcc
</#if>
<#if uma.spec.id?starts_with("osx_x86-64")>
  export PLATFORM=amd64-osx-clang
</#if>

<#if uma.spec.flags.uma_codeCoverage.enabled>
export FE_CFLAGS+=-fprofile-arcs -ftest-coverage
export FE_LDFLAGS+=-fprofile-arcs -ftest-coverage
</#if>

# export FE_CFLAGS+=-fprofile-arcs -ftest-coverage
# export FE_CFLAGS+=-DPROD_WITH_ASSUMES
# export FE_LDFLAGS+=-fprofile-arcs -ftest-coverage
# export FE_ASFLAGS+=

<#if uma.spec.type.windows>
UMA_WINDOWS_PARRALLEL_HACK=-j $(NUMBER_OF_PROCESSORS)
</#if>

default:
	@ echo "J9_VERSION:           $(J9_VERSION)"
	@ echo "TR_BUILD_NAME:        ${uma.buildinfo.jit_buildtag}"
	@ echo "J9SRC:                $(J9SRC)"
	@ echo "JIT_SRCBASE:          $(JIT_SRCBASE)"
	@ echo "JIT_OBJBASE:          $(JIT_OBJBASE)"
	@ echo "JIT_DLL_DIR:          $(JIT_DLL_DIR)"
	@ echo "OMR_DIR:              $(OMR_DIR)"
	@ echo "BUILD_CONFIG:         $(BUILD_CONFIG)"
	@ echo "NUMBER_OF_PROCESSORS: $(NUMBER_OF_PROCESSORS)"
	@ echo "VERSION_MAJOR:        $(VERSION_MAJOR)"
	@ echo "ENABLE_GPU:           $(ENABLE_GPU)"
	@ echo "CUDA_HOME:            $(CUDA_HOME)"
	@ echo "GDK_HOME:             $(GDK_HOME)"
	$(MAKE) $(UMA_WINDOWS_PARRALLEL_HACK) -C $(JIT_SRCBASE)/compiler -f compiler.mk

clean:
	$(MAKE) $(UMA_WINDOWS_PARRALLEL_HACK) -C $(JIT_SRCBASE)/compiler -f compiler.mk clean
