#
# Copyright (c) 2004, 2018 IBM Corp. and others
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

# @test 1.0
# @outputpassed
# @author Alix Pickerill
# @summary check the illegal � name command line option does not create a cache,

export TESTSCRIPT=nameOption11
export NAME=�%u
$1/java -Xshareclasses:name=$NAME HelloWorld 2> $TESTSCRIPT.out

if [ ! -e $TESTSCRIPT.out ]
then
    echo $TESTSCRIPT: TEST FAILED: No file created
else
#pound symbol is not printing correctly on ZOS .Hence added .* in grep instead of pound
    if ( ! grep  "JVMSHRC147E Character.*not valid for cache name" $TESTSCRIPT.out >/dev/null )
    then
		echo $TESTSCRIPT: TEST FAILED
		echo $TESTSCRIPT: Expected not to find the cache name
	else
		echo TEST PASSED
    fi
fi