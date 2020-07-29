
##############################################################################
#  Copyright (c) 2020, 2020 IBM Corp. and others

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

 # if JDK_VERSION is 8
ifeq (8, $(JDK_VERSION))
 TEST_JAVA=$(REPORTDIR_NQ)/testJava/jre
 ifneq (,$(findstring -64, $(SPEC)))
  ZOS_SPEC=s390x
 else
  ZOS_SPEC=s390
 endif
 ifneq (,$(findstring cmprssptrs,$(SPEC))) 
  PATH_TO_LIB=$(TEST_JAVA)/bin/j9vm:$(TEST_JAVA)/lib/$(ZOS_SPEC)/compressedrefs/j9vm:$(TEST_JAVA)/lib/$(ZOS_SPEC)/compressedrefs
  LIBJ9IFA29=/jre/lib/$(ZOS_SPEC)/compressedrefs/libj9ifa29.so
 else 
  PATH_TO_LIB=$(TEST_JAVA)/bin/j9vm:$(TEST_JAVA)/lib/$(ZOS_SPEC)/default/j9vm:$(TEST_JAVA)/lib/$(ZOS_SPEC)/default
  LIBJ9IFA29=/jre/lib/$(ZOS_SPEC)/default/libj9ifa29.so
 endif
else
 TEST_JAVA=$(REPORTDIR_NQ)/testJava
 PATH_TO_LIB=$(TEST_JAVA)/lib/j9vm:$(TEST_JAVA)/lib/compressedrefs:$(TEST_JAVA)/lib
 LIBJ9IFA29=/lib/compressedrefs/libj9ifa29.so
endif