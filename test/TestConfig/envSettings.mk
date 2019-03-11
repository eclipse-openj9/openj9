##############################################################################
#  Copyright (c) 2019, 2019 IBM Corp. and others
#
#  This program and the accompanying materials are made available under
#  the terms of the Eclipse Public License 2.0 which accompanies this
#  distribution and is available at https://www.eclipse.org/legal/epl-2.0/
#  or the Apache License, Version 2.0 which accompanies this distribution and
#  is available at https://www.apache.org/licenses/LICENSE-2.0.
#
#  This Source Code may also be made available under the following
#  Secondary Licenses when the conditions for such availability set
#  forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
#  General Public License, version 2 with the GNU Classpath
#  Exception [1] and GNU General Public License, version 2 with the
#  OpenJDK Assembly Exception [2].
#
#  [1] https://www.gnu.org/software/classpath/license.html
#  [2] http://openjdk.java.net/legal/assembly-exception.html
#
#  SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
##############################################################################

#
# Makefile to compare auto detect values with setting values
#

-include autoGenEnv.mk

HOTSPOT_IMPLS=$(or hotspot,sap)

ifneq ($(AUTO_DETECT), false) 
    ifndef SPEC
        export SPEC:=$(DETECTED_SPEC)
    else
        ifneq ($(SPEC), $(DETECTED_SPEC))
            ifeq ($(DETECTED_JDK_IMPL), $(HOTSPOT_IMPLS))
                ifeq ($(SPEC), linux_ppc-64_cmprssptrs_le)
                    ifneq ($(DETECTED_SPEC), linux_ppc-64_le)
                        $(error DETECTED_SPEC value is $(DETECTED_SPEC), settled SPEC value is $(SPEC). SPEC value does not match. Please reset or unset SPEC)
                    endif
                else ifneq ($(SPEC), $(DETECTED_SPEC)_cmprssptrs)
                    $(error DETECTED_SPEC value is $(DETECTED_SPEC), settled SPEC value is $(SPEC). SPEC value does not match. Please reset or unset SPEC)
                endif
            else
                $(error DETECTED_SPEC value is $(DETECTED_SPEC), settled SPEC value is $(SPEC). SPEC value does not match. Please reset or unset SPEC)
            endif 
        endif
    endif

    ifndef JDK_VERSION
		export JDK_VERSION:=$(DETECTED_JDK_VERSION)
    else
        ifneq ($(JDK_VERSION), $(DETECTED_JDK_VERSION))
            $(error DETECTED_JDK_VERSION value is $(DETECTED_JDK_VERSION), settled JDK_VERSION value is $(JDK_VERSION). JDK_VERSION value does not match. Please reset or unset JDK_VERSION)
        endif
    endif

    ifndef JDK_IMPL
		export JDK_IMPL:=$(DETECTED_JDK_IMPL)
    else
        ifneq ($(JDK_IMPL), $(DETECTED_JDK_IMPL))
            ifneq ($(JDK_IMPL), sap)
                $(error DETECTED_JDK_IMPL value is $(DETECTED_JDK_IMPL), settled JDK_IMPL value is $(JDK_IMPL). JDK_IMPL value does not match. Please reset or unset JDK_IMPL)
	        endif
	    endif
	endif
else
    ifndef SPEC
        $(error Please export AUTO_DETECT=true or export SPEC value manually (i.e., export SPEC=linux_x86-64_cmprssptrs))
    endif
    ifndef JDK_VERSION
        $(error Please export AUTO_DETECT=true or export JDK_VERSION value manually (i.e., export JDK_VERSION=8))
    endif
    ifndef JDK_IMPL
        $(error Please export AUTO_DETECT=true or export JDK_IMPL value manually (i.e., export JDK_IMPL=openj9))
    endif
endif

