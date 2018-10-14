################################################################################
# Copyright (c) 2017, 2018 IBM Corp. and others
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
################################################################################

add_jar(j9vmcp
	SOURCES
		com/ibm/oti/VMCPTool/ClassRef.java
		com/ibm/oti/VMCPTool/CmakeFlagInfo.java
		com/ibm/oti/VMCPTool/ConstantPoolItem.java
		com/ibm/oti/VMCPTool/ConstantPool.java
		com/ibm/oti/VMCPTool/ConstantPoolStream.java
		com/ibm/oti/VMCPTool/Constants.java
		com/ibm/oti/VMCPTool/FieldRef.java
		com/ibm/oti/VMCPTool/InterfaceMethodRef.java
		com/ibm/oti/VMCPTool/IFlagInfo.java
		com/ibm/oti/VMCPTool/J9UTF8.java
		com/ibm/oti/VMCPTool/Main.java
		com/ibm/oti/VMCPTool/NameAndSignature.java
		com/ibm/oti/VMCPTool/PrimaryItem.java
		com/ibm/oti/VMCPTool/SpecialMethodRef.java
		com/ibm/oti/VMCPTool/StaticFieldRef.java
		com/ibm/oti/VMCPTool/StaticMethodRef.java
		com/ibm/oti/VMCPTool/UmaFlagInfo.java
		com/ibm/oti/VMCPTool/Util.java
		com/ibm/oti/VMCPTool/VirtualMethodRef.java

	INCLUDE_JARS objectmodel
)
