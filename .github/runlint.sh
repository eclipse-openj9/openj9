#!/bin/bash

###############################################################################
# Copyright IBM Corp. and others 2021
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
# [2] https://openjdk.org/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
###############################################################################

# Exit immediately if any unexpected error occurs.
set -e
# trace the commands that are run
set -x

# github runner ubuntu-16.04 has 2 cores and 8 gigs of memory.  Attempt to double provision the number of cores for the make.
MAKE_JOBS=4

# explicitly select the desired version of GCC via the environment
export CC=gcc-7
export CXX=g++-7

export JAVA_HOME="$JAVA_HOME_11_X64"
export PATH="$JAVA_HOME/bin:$PATH"

export J9SRC=$PWD/runtime
export OMRCHECKER_DIR=$J9SRC/omr/tools/compiler/OMRChecker
export CMAKE_BUILD_DIR=$PWD/build
export LLVM_CONFIG=llvm-config-3.8 CLANG=clang++-3.8 CXX_PATH=clang++-3.8 CXX=clang++-3.8

cd $J9SRC
git clone --depth 1 --branch openj9 https://github.com/eclipse-openj9/openj9-omr.git omr
cd ..

# We need some generated headers for the linter to run properly
# so we run cmake and build the targets we need
mkdir $CMAKE_BUILD_DIR
cd $CMAKE_BUILD_DIR
cmake -C $J9SRC/cmake/caches/linux_x86-64_cmprssptrs.cmake -DBOOT_JDK="$JAVA_HOME" -DJAVA_SPEC_VERSION=11 ..
make -j $MAKE_JOBS run_cptool omrgc_hookgen j9vm_hookgen j9jit_tracegen j9vm_nlsgen j9vm_m4gen

# Now we can build the linter plugin
cd $OMRCHECKER_DIR
sh smartmake.sh

# and finally, run the linter
cd $J9SRC/compiler
make -j $MAKE_JOBS -f linter.mk
