################################################################################
# Copyright (c) 2017, 2019 IBM Corp. and others
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

add_library(j9vmtest SHARED
	jvmjni.c
)

target_link_libraries(j9vmtest
	PRIVATE
		j9vm_interface

		jvm
)

target_include_directories(j9vmtest
	PRIVATE
		"${j9vm_BINARY_DIR}/j9vm"
)

omr_add_exports(j9vmtest
	Java_com_ibm_oti_jvmtests_SupportJVM_GetSystemPackage
	Java_com_ibm_oti_jvmtests_SupportJVM_GetSystemPackages
	Java_com_ibm_oti_jvmtests_SupportJVM_GCNoCompact
	Java_com_ibm_oti_jvmtests_SupportJVM_MaxObjectInspectionAge
	Java_com_ibm_oti_jvmtests_SupportJVM_GetArrayLength
	Java_com_ibm_oti_jvmtests_SupportJVM_GetArrayElement
	Java_com_ibm_oti_jvmtests_SupportJVM_SetArrayElement
	Java_com_ibm_oti_jvmtests_SupportJVM_GetPrimitiveArrayElementWCodeParam
	Java_com_ibm_oti_jvmtests_SupportJVM_GetShortArrayElement
	Java_com_ibm_oti_jvmtests_SupportJVM_GetIntArrayElement
	Java_com_ibm_oti_jvmtests_SupportJVM_GetLongArrayElement
	Java_com_ibm_oti_jvmtests_SupportJVM_GetByteArrayElement
	Java_com_ibm_oti_jvmtests_SupportJVM_GetCharArrayElement
	Java_com_ibm_oti_jvmtests_SupportJVM_GetFloatArrayElement
	Java_com_ibm_oti_jvmtests_SupportJVM_GetDoubleArrayElement
	Java_com_ibm_oti_jvmtests_SupportJVM_GetBooleanArrayElement
	Java_com_ibm_oti_jvmtests_SupportJVM_SetPrimitiveArrayElementWCodeParam
	Java_com_ibm_oti_jvmtests_SupportJVM_SetShortArrayElement
	Java_com_ibm_oti_jvmtests_SupportJVM_SetIntArrayElement
	Java_com_ibm_oti_jvmtests_SupportJVM_SetLongArrayElement
	Java_com_ibm_oti_jvmtests_SupportJVM_SetByteArrayElement
	Java_com_ibm_oti_jvmtests_SupportJVM_SetCharArrayElement
	Java_com_ibm_oti_jvmtests_SupportJVM_SetFloatArrayElement
	Java_com_ibm_oti_jvmtests_SupportJVM_SetDoubleArrayElement
	Java_com_ibm_oti_jvmtests_SupportJVM_SetBooleanArrayElement
	Java_com_ibm_oti_jvmtests_SupportJVM_ArrayCopy
)

if(NOT JAVA_SPEC_VERSION LESS 11)
	omr_add_exports(j9vmtest Java_com_ibm_oti_jvmtests_SupportJVM_GetNanoTimeAdjustment)
endif()


install(
	TARGETS j9vmtest
	LIBRARY DESTINATION ${j9vm_SOURCE_DIR}
	RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
)
