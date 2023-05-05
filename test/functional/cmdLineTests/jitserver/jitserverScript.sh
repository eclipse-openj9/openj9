#!/bin/sh

#
# Copyright IBM Corp. and others 2023
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

echo "start running script";
# the expected arguments are:
# $1 is the TEST_ROOT
# $2 is the TEST_JDK_BIN
# $3 is the JITServer Options
# $4 is the JVM Options
# %5 is the Metrics

TEST_ROOT=$1
TEST_JDK_BIN=$2
JITSERVER_OPTS="$3"
JVM_OPTS="$4"
METRICS=$5

source $TEST_ROOT/jitserverconfig.sh

JITSERVER_PORT=$(random_port)

if [ "$METRICS" == true ]; then
    METRICS_PORT=$(random_port)
    METRICS_OPTS="-XX:+JITServerMetrics -XX:JITServerMetricsPort=$METRICS_PORT"
fi

JITSERVER_OPTIONS="-XX:JITServerPort=$JITSERVER_PORT $METRICS_OPTS $JITSERVER_OPTS"

echo "Starting $TEST_JDK_BIN/jitserver $JITSERVER_OPTIONS"
$TEST_JDK_BIN/jitserver $JITSERVER_OPTIONS &
JITSERVER_PID=$!
sleep 2

$TEST_JDK_BIN/java -XX:JITServerPort=$JITSERVER_PORT $JVM_OPTS -version;

if [ "$METRICS" == true ]; then
    curl http://localhost:$METRICS_PORT/metrics
fi

echo "Checking that JITServer Process is still alive"
ps -ef | grep $JITSERVER_PID

echo "Terminating $TEST_JDK_BIN/jitserver $JITSERVER_OPTIONS"
kill -9 $JITSERVER_PID
# Running pkill seems to cause a hang...
#pkill -9 -xf "$TEST_JDK_BIN/jitserver $JITSERVER_OPTIONS"
sleep 2

echo "finished script";
