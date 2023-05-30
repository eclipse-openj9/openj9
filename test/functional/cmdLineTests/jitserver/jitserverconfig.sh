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

START_PORT=38400
END_PORT=60000

DIFF=$(($END_PORT-$START_PORT+1))
RANDOM=$$

random_port () {
    RANDOM_PORT=$(($(($RANDOM%$DIFF))+$START_PORT))
    out=$(lsof -i -P -n | grep LISTEN | grep $RANDOM_PORT)
    retVal=$?

    while [ $retVal -eq 0 ]
    do
        RANDOM_PORT=$(($(($RANDOM%$DIFF))+$START_PORT))
        out=$(lsof -i -P -n | grep LISTEN | grep $RANDOM_PORT)
        retVal=$?
    done

    echo "$RANDOM_PORT"
}
