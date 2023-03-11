##############################################################################
#  Copyright IBM Corp. and others 2022
#
#  This program and the accompanying materials are made available under
#  the terms of the Eclipse Public License 2.0 which accompanies this
#  distribution and is available at https://www.eclipse.org/legal/epl-2.0/
#  or the Apache License, Version 2.0 which accompanies this distribution and
#  is available at https://www.apache.org/licenses/LICENSE-2.0.
#
#  This Source Code may also be made available under the following
#  Secondary Licenses when the conditions for such availability set
#  forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
#  General Public License, version 2 with the GNU Classpath
#  Exception [1] and GNU General Public License, version 2 with the
#  OpenJDK Assembly Exception [2].
#
#  [1] https://www.gnu.org/software/classpath/license.html
#  [2] https://openjdk.org/legal/assembly-exception.html
#
#  SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
##############################################################################

# In JDK18+, java.security.manager == null behaves as -Djava.security.manager=disallow.
# In JDK17-, java.security.manager == null behaves as -Djava.security.manager=allow.
# For OpenJ9 tests to work as expected, -Djava.security.manager=allow behaviour is
# needed in JDK18+.
ifeq ($(filter 8 9 10 11 12 13 14 15 16 17, $(JDK_VERSION)),)
  export JAVA_SECURITY_MANAGER = -Djava.security.manager=allow
else
  export JAVA_SECURITY_MANAGER =
endif
