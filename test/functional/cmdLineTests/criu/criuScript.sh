#!/bin/sh

#
# Copyright (c) 2022, 2022 IBM Corp. and others
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

echo "start running script";
# the expected arguments are:
# $1 is the TEST_ROOT
# $2 is the JAVA_COMMAND
# $3 is the JVM_OPTIONS
# $4 is the MAINCLASS
# $5 is the APP ARGS
# $6 is the NUM_CHECKPOINT
# $7 is the KEEP_CHECKPOINT

$2 -XX:+EnableCRIUSupport $3 -cp "$1/criu.jar" $4 $5 "$6" >testOutput 2>&1;

if [ "$6" != true ]; then
    NUM_CHECKPOINT=$6
    for ((i=0; i<$NUM_CHECKPOINT; i++)); do
        sleep 2;
        criu-ns restore -D ./cpData --shell-job;
    done
fi

cat testOutput;

if  [ "$7" != true ]; then
    rm -rf testOutput
fi
echo "finished script";
