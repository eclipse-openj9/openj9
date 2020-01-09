##############################################################################
#  Copyright (c) 2019, 2020 IBM Corp. and others
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
#  [2] http://openjdk.java.net/legal/assembly-exception.html
#
#  SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
##############################################################################

ADD_EXPORTS=
ASM_JAR=$(LIB_DIR)$(D)asm-all.jar
# ADD_EXPORTS needs to set for JDK9 and up
# if JDK_VERSION is not 8
ifeq ($(filter 8, $(JDK_VERSION)),)
 ADD_EXPORTS=--add-exports=java.base/com.ibm.oti.vm=ALL-UNNAMED
 ASM_JAR=$(LIB_DIR)$(D)asm-7.3.jar
endif