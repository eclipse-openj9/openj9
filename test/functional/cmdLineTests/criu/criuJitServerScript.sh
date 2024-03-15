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
# $3 is the JVM_OPTIONS
# $4 is the MAINCLASS
# $5 is the APP ARGS
# $6 is the NUM_CHECKPOINT
# $7 is the KEEP_CHECKPOINT
# $8 is the KEEP_TEST_OUTPUT

TEST_ROOT=$1
TEST_JDK_BIN=$2
JVM_OPTIONS="$3"
MAINCLASS="$4"
APP_ARGS="$5"
NUM_CHECKPOINT="$6"
KEEP_CHECKPOINT="$7"
KEEP_TEST_OUTPUT="$8"

source $TEST_ROOT/jitserverconfig.sh

echo "export GLIBC_TUNABLES=glibc.cpu.hwcaps=-XSAVEC,-XSAVE,-AVX2,-ERMS,-AVX,-AVX_Fast_Unaligned_Load";
export GLIBC_TUNABLES=glibc.pthread.rseq=0:glibc.cpu.hwcaps=-XSAVEC,-XSAVE,-AVX2,-ERMS,-AVX,-AVX_Fast_Unaligned_Load
echo "export LD_BIND_NOT=on";
export LD_BIND_NOT=on

JITSERVER_PORT=$(random_port)
HEALTH_PORT=$(random_port)
JITSERVER_SSL="-XX:JITServerSSLRootCerts"
if grep -q -- "$JITSERVER_SSL" <<< "$APP_ARGS"; then
    echo "Generate SSL certificates"
    source $TEST_ROOT/jitserversslconfig.sh
    if ! grep -q "nosslserverCert.pem" <<< "$APP_ARGS"; then
	SSL_OPTS="-XX:JITServerSSLKey=key.pem -XX:JITServerSSLCert=cert.pem -Xjit:verbose={JITServer}"
    fi
fi

JITSERVER_OPTIONS="-XX:JITServerPort=$JITSERVER_PORT -XX:JITServerHealthProbePort=$HEALTH_PORT $SSL_OPTS"

echo "Starting $TEST_JDK_BIN/jitserver $JITSERVER_OPTIONS"
$TEST_JDK_BIN/jitserver $JITSERVER_OPTIONS &
JITSERVER_PID=$!
sleep 2

ps | grep $JITSERVER_PID | grep 'jitserver'
JITSERVER_EXISTS=$?

if [ "$JITSERVER_EXISTS" == 0 ]; then
    echo "JITSERVER EXISTS"

    $TEST_JDK_BIN/java -XX:+EnableCRIUSupport -XX:JITServerPort=$JITSERVER_PORT $JVM_OPTIONS -cp "$TEST_ROOT/criu.jar" $MAINCLASS $APP_ARGS -XX:JITServerPort=$JITSERVER_PORT $NUM_CHECKPOINT>testOutput 2>&1;

    if [ "$KEEP_CHECKPOINT" != true ]; then
        for ((i=0; i<$NUM_CHECKPOINT; i++)); do
            sleep 2;
            criu restore -D ./cpData --shell-job >criuOutput 2>&1;
        done
    fi

    cat testOutput criuOutput;

    if  [ "$KEEP_CHECKPOINT" != true ]; then
        if [ "$KEEP_TEST_OUTPUT" != true ]; then
            rm -rf testOutput criuOutput
            echo "Removed test output files"
        fi
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
    # For consistency with the jitserver cmdline tests, use kill
    #pkill -9 -xf "$TEST_JDK_BIN/jitserver $JITSERVER_OPTIONS"
    sleep 2

    if grep -q "nosslserverCert.pem" <<< "$APP_ARGS"; then
        rm -f *.pem
    fi

else
    echo "JITSERVER DOES NOT EXIST"
fi

echo "finished script";
