##############################################################################
#  Copyright (c) 2018, 2019 IBM Corp. and others
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

ADD_MODULE_JAVA_SE_EE=
# java.se.ee should only used for jdk 9 and 10
# if JDK_VERSION is 9 10
ifneq ($(filter 9 10, $(JDK_VERSION)),)
 ADD_MODULE_JAVA_SE_EE=--add-modules java.se.ee
endif

JAXB_API_JAR=
ADD_EXPORTS_JDK_INTERNAL_REFLECT=
ADD_EXPORTS_JDK_INTERNAL_MISC=
# JAXB_API_JAR and ADD_EXPORTS_JDK_INTERNAL_REFLECT need to set for JDK11 and up
# if JDK_VERSION is not 8 9 10
ifeq ($(filter 8 9 10, $(JDK_VERSION)),)
 JAXB_API_JAR=$(P)$(LIB_DIR)$(D)jaxb-api.jar
 ADD_EXPORTS_JDK_INTERNAL_REFLECT=--add-exports java.base/jdk.internal.reflect=ALL-UNNAMED
 ADD_EXPORTS_JDK_INTERNAL_MISC=--add-exports java.base/jdk.internal.misc=ALL-UNNAMED
endif

ADD_EXPORTS_JDK_INTERNAL_ACCESS=
# some jdk.internal.misc packages were moved to jdk.internal.access in JDK12 and up
# if JDK_VERSION is not 8 9 10 11
ifeq ($(filter 8 9 10 11, $(JDK_VERSION)),)
 ADD_EXPORTS_JDK_INTERNAL_ACCESS=--add-exports java.base/jdk.internal.access=ALL-UNNAMED
endif
