################################################################################
# Copyright IBM Corp. and others 2017
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
################################################################################

string(TIMESTAMP J9VM_CURRENT_YEAR "%Y")
string(TIMESTAMP J9VM_BUILD_DATE "%Y%m%d")
string(TIMESTAMP J9VM_BUILD_TIME "%Y%m%d%H%M%S")
string(RANDOM ALPHABET "0123456789abcdef" LENGTH 16 RANDOM_SEED ${J9VM_BUILD_TIME} J9VM_UNIQUE_BUILD_ID)
set(J9VM_UNIQUE_BUILD_ID "0x${J9VM_UNIQUE_BUILD_ID}")

# information intended to be overridden at build time
set(BUILD_ID 000000 CACHE STRING "")
set(OPENJ9_SHA unknown CACHE STRING "")
