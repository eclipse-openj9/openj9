#
# Copyright (c) 2009, 2018 IBM Corp. and others
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
# @author Radhakrishnan Thangamuthu
# @summary exports the user and group name.

idOutput=`id`

# get the username from the environment variable LOGNAME
export TESTUSER=$LOGNAME

export GROUPNAME=`echo $idOutput | awk '{print $2}' | awk '{print $1}' | cut -f 2 -d '(' | cut -f 1 -d ')'`

#some random user name
export CFGUSER="%utest"
export CFGFULLUSER="${TESTUSER}test"

