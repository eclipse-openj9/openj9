#!/bin/sh

#
# Copyright (c) 2022, 2023 IBM Corp. and others
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
#

# the expected arguments are:
# $1 is the RUN_CRIU_SCRIPT_PATH
# $2 is the TEST_ROOT
# $3 is the JAVA_COMMAND
# $4 is the JVM_OPTIONS
# $5 is the trc file name
# $6 is the verbosegc file name

PASS () {
   echo "criuVerifyGCThreading PASS"
   exit 0
}

FAIL () {
    echo "criuVerifyGCThreading FAIL"
    exit 1
}

# Invoke the main script to run the CRIUSimpleTest app
source $1 $2 $3 "$4" "org.openj9.criu.CRIUSimpleTest" 1 1 false

echo "Start running verify gc threading script";

# Check for the tracefile and verbosegc file
if [ ! -f "$5" ]
then
    echo "$5 does not exist."
    FAIL
fi

if [ ! -f "$6" ]
then
    echo "$6 does not exist."
    FAIL
fi

# Set the file name for the formatted trace file
trcFile="$5.fmt"
# Format the test tracefile
"$TEST_JDK_HOME/bin/traceformat" $5 $trcFile

# Check if the Metronome GC Policy was used. This is a special case, GC threads will be fixed for all: pre-checkpoit, checkpoint and restore.
if (grep -q "Xgcpolicy:metronome" "$trcFile")
then
    # The thread adjustment routines should never be called.
    if (grep -i "contractThreadPool\|expandThreadPool" "$trcFile")
    then
        FAIL
    else
        # TODO: Verify Pre-checkpoint thread count for metronome.
        PASS
    fi
fi

# Function to check for trace events in the trace file.
# The expected arguments are: $1 is the string to check for
verifyEvent () {
    if !(grep -q "$1" "$trcFile")
    then
            echo "Missing ["$1"] event"
            FAIL
    fi

    return 1
}

# Determine the expected:
# - Pre-checkpoint thread Count (inital VM startup thread count)
# - Checkpoint thread count (# of threads part of the CRIU Dump)
# - post-restore thread count

# The expected inital VM thread count (pre-checkpoint) is either user specificed (-Xgcthreads), or defaults to # processors on checkpoint machine.
expectedPrecheckpointTC=$(grep "\-Xgcthreads" $trcFile | sed s/\-Xgcthreads//)
if [ -z "$expectedPrecheckpointTC" ]
then
    # The thread count is not explicitly set, look to the processor info in the trace file
    expectedPrecheckpointTC=$(grep "Num Processors" $trcFile | sed 's/.*Num Processors://' | tr -d " \t\n\r")

    # GC thread cout can't exceed 64 (internal VM/GC cap)
    expectedPrecheckpointTC=$(( $expectedPrecheckpointTC  >= 64 ? 64 : $expectedPrecheckpointTC ))
fi

# The checkpoint thread count is either a user specificed value (-XX:CheckpointGCThreads=), or it defaults to 4 (or the inital TC, in case threads if inital TC <= 4)
expectedCheckpointTC=$(grep "\-XX:CheckpointGCThreads=" $trcFile | sed s/\-XX:CheckpointGCThreads=//)
if [ -z "$expectedCheckpointTC" ]
then
    expectedCheckpointTC=$(( $expectedPrecheckpointTC  >= 4 ? 4 : $expectedPrecheckpointTC ))
fi

# Are threads being shutdown for checkpoint?
if (("$expectedCheckpointTC" >= "$expectedPrecheckpointTC"))
then
    # Don't expcet threads to shutdown for checkpoint, checkpointTC should be equal to precheckpointTC
    expectedCheckpointTC=$expectedPrecheckpointTC
else
    verifyEvent "Successfully shutdown GC threads: $expectedPrecheckpointTC -> $expectedCheckpointTC"
fi

verifyEvent "contractThreadPool.*Entry.*gcThreadCount: $expectedPrecheckpointTC"
verifyEvent "contractThreadPool.*Exit.*gcThreadCount: $expectedCheckpointTC"

# The restore thread count is based on restore machine's # of processors, check verbosegc log for numCPUs
expectedPostRestoreTC=$(grep "numCPUs" $5 | tr -d -c 0-9)
expectedPostRestoreTC=$(( $expectedPostRestoreTC  > 64 ? 64 : $expectedPostRestoreTC ))

# Restore thread count is capped by inital VM GC thread count
if (("$expectedPostRestoreTC" > "$expectedPrecheckpointTC"))
then
    expectedPostRestoreTC=$expectedPrecheckpointTC
fi

# Thread are only started up during restore, hence the thread count can't be lower than the checkpoint thread count
if (("$expectedPostRestoreTC" < "$expectedCheckpointTC"))
then
    expectedPostRestoreTC=$expectedCheckpointTC
fi

verifyEvent "expandThreadPool.*Entry"
verifyEvent "<expandThreadPool.*Exit.*preExpandThreadCount: $expectedCheckpointTC.*gcThreadCount: $expectedPostRestoreTC"

PASS
