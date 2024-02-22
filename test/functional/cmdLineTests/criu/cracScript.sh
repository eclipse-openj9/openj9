#!/bin/sh

#
# Copyright IBM Corp. and others 2024
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
#

echo "start running script";
# the expected arguments are:
# $1 is the TEST_ROOT
# $2 is the JAVA_COMMAND
# $3 is the JVM_OPTIONS
# $4 is the TESTNG
# $5 is the MAINCLASS
# $6 is the APP ARGS
# $7 is the NUM_CHECKPOINT
# $8 is the KEEP_CHECKPOINT
# $9 is the KEEP_TEST_OUTPUT

echo "export GLIBC_TUNABLES=glibc.cpu.hwcaps=-XSAVEC,-XSAVE,-AVX2,-ERMS,-AVX,-AVX_Fast_Unaligned_Load";
export GLIBC_TUNABLES=glibc.pthread.rseq=0:glibc.cpu.hwcaps=-XSAVEC,-XSAVE,-AVX2,-ERMS,-AVX,-AVX_Fast_Unaligned_Load
echo "export LD_BIND_NOT=on";
export LD_BIND_NOT=on
echo "$2 $3 -cp "$1/criu.jar:$4" $5 $6 $7"
$2 $3 -cp "$1/criu.jar:$4" $5 $6 $7 >testOutput 2>&1;

if [ "$8" != true ]; then
    NUM_CHECKPOINT=$7
    for ((i=0; i<$NUM_CHECKPOINT; i++)); do
        sleep 2;
        echo "initiate restore" >>criuOutput
        criu restore -D ./cpData -v2 --shell-job >>criuOutput 2>&1;
    done
fi

if [ -e criuOutput ]; then
    cat testOutput criuOutput
else
    cat testOutput
fi

if  [ "$8" != true ]; then
    if [ "$9" != true ]; then
        rm -rf testOutput criuOutput
        echo "Removed test output files"
    fi
fi
echo "finished script";
