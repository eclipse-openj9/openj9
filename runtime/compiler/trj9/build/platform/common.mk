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

#
# Common file for setting platform tokens
# Mostly just guesses the platform and includes the relevant
# HOST and TARGET files
#

#
# Try and guess platform if user didn't give it
#
ifeq (,$(PLATFORM))
    PLATFORM:=$(shell $(SHELL) $(JIT_SCRIPT_DIR)/guess-platform.sh)
    $(warning PLATFORM not set. Guessing '$(PLATFORM)')
endif

#
# If we still can't figure it out, bomb out with an error
#
ifeq (,$(PLATFORM))
    $(error PLATFORM not set and unable to guess)
endif

# Make sure platform file exists
$(if $(wildcard $(JIT_MAKE_DIR)/platform/host/$(PLATFORM).mk),,$(error Error: $(PLATFORM) not implemented))

#
# This is where we set variables based on the "host triplet"
# These variables will be tested all throughout the rest of the makefile
# to make decisions based on our host architecture, OS, and toolchain
#
include $(JIT_MAKE_DIR)/platform/host/$(PLATFORM).mk

#
# This will set up the target variables
# We will use these later to make decisions related to target arch
#
# Right now, we really don't have anything special like a
# "target spec", but this leaves it open in the future
#
include $(JIT_MAKE_DIR)/platform/target/all.mk
