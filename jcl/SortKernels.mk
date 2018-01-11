###############################################################################
# Copyright (c) 2018, 2018 IBM Corp. and others
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

# Makefile to compile generated GPU code in SortKernels.cu.
#
# Example call:
#   $(MAKE) -f SortKernels.mk GPU_BIN_DIR=$(GPU_BIN_DIR) GPU_TMP_DIR=$(GPU_TMP_DIR) NVCC=$(NVCC)

# Verify required macros are defined.
ifndef GPU_BIN_DIR
$(error GPU_BIN_DIR is undefined)
endif
ifndef GPU_TMP_DIR
$(error GPU_TMP_DIR is undefined)
endif
ifndef NVCC
$(error NVCC is undefined)
endif

# Find all unique occurrences of compute_NN and sm_NN in the output
# of 'nvcc --help'; we assume they are supported GPU architectures.
GPU_ARCH_LIST := $(strip $(sort $(shell \
	$(NVCC) --help \
		| sed -e "s/[^a-z_0-9]/ /g" -e "y/ /\n/" \
		| grep -E "^(compute|sm)_[0-9]+$$")))

# The most important '--generate-code' options are those that
# yield PTX code: Make sure there will be at least one version.
ifeq (,$(filter compute_20 compute_30,$(GPU_ARCH_LIST)))
$(error NVCC must support at least one of compute_20 or compute_30)
endif

# Expands to a '--generate-code' option and value if both the arguments are
# supported GPU architectures (possibly virtual) or nothing otherwise.
# The next two macros are helpers: GPU_ARCH_TEST tests if the given architectures
# are supported, GPU_ARCH_GENERATE is needed to hide the comma from the 'if' function.
GPU_ARCH_CODE = \
	$(if $(call GPU_ARCH_TEST,$1,$2), $(call GPU_ARCH_GENERATE,$1,$2))
GPU_ARCH_TEST = \
	$(and $(findstring $1,$(GPU_ARCH_LIST)), $(findstring $2,$(GPU_ARCH_LIST)))
GPU_ARCH_GENERATE = \
	--generate-code arch=$1,code=$2

# Expands to the subset of '--generate-code' options supported by the version
# of nvcc being used.
GPU_ARCH_CODES := \
	$(call GPU_ARCH_CODE,compute_20,compute_20) \
	$(call GPU_ARCH_CODE,compute_20,sm_20) \
	$(call GPU_ARCH_CODE,compute_30,compute_30) \
	$(call GPU_ARCH_CODE,compute_30,sm_30) \
	$(call GPU_ARCH_CODE,compute_35,sm_35)

compile :
	$(NVCC) $(GPU_ARCH_CODES) -fatbin --machine 64 --ptxas-options -O4 \
		-o $(GPU_BIN_DIR)/SortKernels.fatbin $(GPU_TMP_DIR)/SortKernels.cu
