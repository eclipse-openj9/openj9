#!/bin/bash
###############################################################################
# Copyright (c) 2018, 2019 IBM Corp. and others
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

# Exit immediately if any unexpected error occurs.
set -e
# trace the commands that are run
set -x

# Based on https://blog.travis-ci.com/2014-12-17-faster-builds-with-container-based-infrastructure/ travis container
# Build machines have 2 cores and 4 gigs of memory.  Attempt to double provision the number of cores for the make.
MAKE_JOBS=4

# download bootstrap jdk
SRCDIR=$PWD
cd ~
wget -O bootjdk11.tar.gz "https://api.adoptopenjdk.net/v2/binary/releases/openjdk11?openjdk_impl=openj9&os=linux&arch=x64&release=latest&type=jdk&heap_size=normal"
tar -xzf bootjdk11.tar.gz
rm -f bootjdk11.tar.gz
mv $(ls | grep -i jdk) bootjdk11
export JAVA_HOME="$PWD/bootjdk11"
export PATH="${JAVA_HOME}/bin:${PATH}"
cd $SRCDIR

# Note Currently enabling RUN_LINT and RUN_BUILD simultaneously is not supported
if test "x$RUN_LINT" = "xyes"; then
  llvm-config --version
  clang++ --version

  export J9SRC=$PWD/runtime
  export OMRCHECKER_DIR=$J9SRC/omr/tools/compiler/OMRChecker
  export CMAKE_BUILD_DIR=$PWD/build
  export LLVM_CONFIG=llvm-config-3.8 CLANG=clang++-3.8 CXX_PATH=clang++-3.8 CXX=clang++-3.8

  cd $J9SRC
  git clone --depth 1 --branch openj9 https://github.com/eclipse/openj9-omr.git omr
  cd ..

  # We need some generated headers for the linter to run properly
  # so we run cmake and build the targets we need
  mkdir $CMAKE_BUILD_DIR
  cd $CMAKE_BUILD_DIR
  cmake -C $J9SRC/cmake/caches/linux_x86-64_cmprssptrs.cmake ..
  make -j $MAKE_JOBS run_cptool omrgc_hookgen j9vm_hookgen j9jit_tracegen j9vm_nlsgen j9vm_m4gen

  # Now we can build the linter plugin
  cd $OMRCHECKER_DIR
  sh smartmake.sh

  # and finally, run the linter
  cd $J9SRC/compiler
  make -j $MAKE_JOBS -f linter.mk
fi

if test "x$RUN_BUILD" = "xyes"; then
  if [ ! `wget https://ci.eclipse.org/openj9/userContent/freemarker-2.3.8.jar -O freemarker.jar` ]; then
    wget https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download -O freemarker.tgz
    tar -xzf freemarker.tgz freemarker-2.3.8/lib/freemarker.jar --strip=2
  fi
  cd ..
  # Shallow clone of the openj9-openjdk-jdk9 repo to speed up clone / reduce server load.
  git clone --depth 1 https://github.com/ibmruntimes/openj9-openjdk-jdk11.git

  # Clear this option so it doesn't interfere with configure detecting the bootjdk.
  unset _JAVA_OPTIONS

  # Point the get_sources script at the OpenJ9 repo that's already been cloned to disk.
  # Results in a copy of the source (disk space =( ) but no new network activity so overall a win.
  OPENJ9_SHA=$(git -C $TRAVIS_BUILD_DIR rev-parse HEAD)
  if test x"$OPENJ9_SHA" != x"$TRAVIS_COMMIT" ; then
    echo "Warning using SHA $OPENJ9_SHA instead of $TRAVIS_COMMIT."
  fi

  cd openj9-openjdk-jdk11 && bash get_source.sh -openj9-repo=$TRAVIS_BUILD_DIR -openj9-branch=$TRAVIS_BRANCH -openj9-sha=$OPENJ9_SHA

  # Limit number of jobs to work around g++ internal compiler error.
  bash configure --with-freemarker-jar=$TRAVIS_BUILD_DIR/freemarker.jar --with-jobs=$MAKE_JOBS --with-num-cores=$MAKE_JOBS --enable-ccache --with-cmake --disable-ddr
  make images EXTRA_CMAKE_ARGS="-DOMR_WARNINGS_AS_ERRORS=FALSE"
  # Minimal sniff test - ensure java -version works.
  ./build/linux-x86_64-normal-server-release/images/jdk/bin/java -version
fi
