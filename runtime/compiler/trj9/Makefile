# Copyright (c) 2000, 2017 IBM Corp. and others
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

.PHONY: all clean cleanobjs cleandeps cleandll
all:
clean:
cleanobjs:
cleandeps:
cleandll:

# This is the logic right now for locating Clang and LLVM-config
# There's probably a nicer way to do all of this... it's pretty bad

#
# Detect the VM's build SPEC for compiling ARM.  This should eventually
# be done cleaner.
#
ifeq ($(SPEC),linux_arm)
    PLATFORM=arm-linux-gcc-cross
    $(warning ARM SPEC detected)
endif

ifeq ($(PLATFORM),amd64-linux64-clang)
    # Luckily we can just use the default path for Clang :)
endif

ifeq ($(PLATFORM),ppc64-linux64-clang)
    ifeq (default,$(origin CC))
        export CC=/tr/llvm_checker/ppc-64/sles11/bin/clang
    endif
    ifeq (default,$(origin CXX))
        export CXX=/tr/llvm_checker/ppc-64/sles11/bin/clang++
    endif
endif

ifeq ($(PLATFORM),s390-zos64-vacpp)
    ifeq (default,$(origin CC))
        export CC=/usr/lpp/cbclib/xlc/bin/xlc
    endif
    ifeq (default,$(origin CXX))
        export CXX=/usr/lpp/cbclib/xlc/bin/xlC
    endif
    export A2E_INCLUDE_PATH?=/usr/lpp/cbclib/include
endif

ifeq ($(PLATFORM),arm-linux-gcc-cross)
    OPENJ9_CC_PREFIX ?= arm-bcm2708hardfp-linux-gnueabi
    ifeq (default,$(origin CC))
        export CC=$(OPENJ9_CC_PREFIX)-gcc
    endif
    ifeq (default,$(origin CXX))
        export CXX=$(OPENJ9_CC_PREFIX)-g++
    endif
    ifeq (default,$(origin AS))
        export AS=$(OPENJ9_CC_PREFIX)-as
    endif
    SUBMAKE_EXTRAARGS=PLATFORM=arm-linux-gcc
endif

ifneq ($(findstring DPROD_WITH_ASSUMES, $(USERCFLAGS)),)
    SUBMAKE_EXTRAARGS+=ASSUMES=1
endif

#
# Add J9 to base targets
#
all: trj9
clean: trj9_clean
cleanobjs: trj9_cleanobjs
cleandeps: trj9_cleandeps
cleandll: trj9_cleandll


#
# Add J9 targets
#
.PHONY: trj9 trj9_clean trj9_cleanobjs trj9_cleandeps trj9_cleandll

trj9:
	$(MAKE) -f trj9.mk $(SUBMAKE_EXTRAARGS)

trj9_clean:
	$(MAKE) -f trj9.mk clean $(SUBMAKE_EXTRAARGS)

trj9_cleanobjs:
	$(MAKE) -f trj9.mk cleanobjs $(SUBMAKE_EXTRAARGS)

trj9_cleandeps:
	$(MAKE) -f trj9.mk cleandeps $(SUBMAKE_EXTRAARGS)

trj9_cleandll:
	$(MAKE) -f trj9.mk cleandll $(SUBMAKE_EXTRAARGS)
