###############################################################################
# Copyright (c) 2016, 2018 IBM Corp. and others
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

# This makefile should be invoked as the default makefile for building J9,
# instead of the UMA-generated makefile.

# Add the omr dependency to the UMA makefile's "all" target. 
# Force omr to be built first, and not in parallel with any UMA targets.
.NOTPARALLEL all: phase_omr
clean: clean_phase_omr

phase_omr:
	$(MAKE) $(MFLAGS) -C omr

clean_phase_omr:
	$(MAKE) $(MFLAGS) -C omr clean

.PHONY: phase_omr clean_phase_omr

# As a convenience, add phase_omr as a prereq to commonly used phases.
phase_core: phase_omr
phase_j2se: phase_omr
phase_jit: phase_omr
phase_quick: phase_omr
phase_util: phase_omr

# Include the UMA-generated makefile
include ./makefile
