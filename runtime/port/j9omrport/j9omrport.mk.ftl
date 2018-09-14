###############################################################################
# Copyright (c) 2012, 2018 IBM Corp. and others
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
###############################################################################

# This makefile is generated using an UMA template.

# UMA_PATH_TO_ROOT can be overridden by a command-line argument to 'make'.
UMA_PATH_TO_ROOT ?= ../../
OMR_DIR ?= $(UMA_PATH_TO_ROOT)omr

# top_srcdir is the top of the omr source tree
top_srcdir := $(OMR_DIR)
include $(top_srcdir)/omrmakefiles/configure.mk

# PORT_SRCDIR is the location of the OMR port source code.
PORT_SRCDIR := $(top_srcdir)/port/

MODULE_NAME := j9omrport
ARTIFACT_TYPE := archive

<#if uma.spec.flags.port_omrsigSupport.enabled>
MODULE_INCLUDES += $(UMA_PATH_TO_ROOT)include
MODULE_SHARED_LIBS += omrsig
</#if>

include $(PORT_SRCDIR)port_objects.mk

include $(top_srcdir)/omrmakefiles/rules.mk
