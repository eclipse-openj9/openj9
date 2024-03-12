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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
#

echo "start running script";
# the expected arguments are:
# $1 is the TEST_ROOT
# $2 is the TEST_JDK_BIN
# $3 is the JITServer Options
# $4 is the JVM Options
# $5 is the Metrics
# $6 is a boolean for testing the health port

TEST_ROOT=$1
TEST_JDK_BIN=$2
JITSERVER_OPTS="$3"
JVM_OPTS="$4"
METRICS=$5
HEALTHPORTTEST=$6

source $TEST_ROOT/jitserverconfig.sh

JITSERVER_PORT=$(random_port)
HEALTH_PORT=$(random_port)

JITSERVER_SSL="-XX:JITServerSSLRootCerts"

if grep -q -- "$JITSERVER_SSL" <<< "$JVM_OPTS"; then
    echo "Generate SSL certificates"
    source $TEST_ROOT/jitserversslconfig.sh
    if ! grep -q "nosslserverCert.pem" <<< "$JVM_OPTS"; then
	SSL_OPTS="-XX:JITServerSSLKey=key.pem -XX:JITServerSSLCert=cert.pem"
    fi
fi

if [ "$METRICS" == true ]; then
    METRICS_PORT=$(random_port)
    METRICS_OPTS="-XX:+JITServerMetrics -XX:JITServerMetricsPort=$METRICS_PORT"
fi

JITSERVER_OPTIONS="-XX:JITServerPort=$JITSERVER_PORT -XX:JITServerHealthProbePort=$HEALTH_PORT $METRICS_OPTS $JITSERVER_OPTS $SSL_OPTS"

echo "Starting $TEST_JDK_BIN/jitserver $JITSERVER_OPTIONS"
$TEST_JDK_BIN/jitserver $JITSERVER_OPTIONS &
JITSERVER_PID=$!
sleep 2

ps | grep $JITSERVER_PID | grep 'jitserver'
JITSERVER_EXISTS=$?

if [ "$JITSERVER_EXISTS" == 0 ]; then
    echo "JITSERVER EXISTS"

    $TEST_JDK_BIN/java -XX:JITServerPort=$JITSERVER_PORT $JVM_OPTS -version;

    if [ "$METRICS" == true ]; then
        curl http://localhost:$METRICS_PORT/metrics
    fi

    if [ "$HEALTHPORTTEST" == true ]; then
        curl localhost:$HEALTH_PORT
    fi

    ps | grep $JITSERVER_PID | grep 'jitserver'
    JITSERVER_STILL_EXISTS=$?
    if [ "$JITSERVER_STILL_EXISTS" == 0 ]; then
        echo "JITSERVER STILL EXISTS"
    else
        echo "JITSERVER NO LONGER EXISTS"
    fi

    echo "Terminating $TEST_JDK_BIN/jitserver $JITSERVER_OPTIONS"
    kill -9 $JITSERVER_PID
    # Running pkill seems to cause a hang...
    #pkill -9 -xf "$TEST_JDK_BIN/jitserver $JITSERVER_OPTIONS"
    sleep 2

    if grep -q "nosslserverCert.pem" <<< "$JVM_OPTS"; then
        rm -f *.pem
    fi

else
    echo "JITSERVER DOES NOT EXIST"
fi

echo "finished script";
