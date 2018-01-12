/*******************************************************************************
 * Copyright (c) 1998, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
#ifndef jclglob_h
#define jclglob_h

#include "jcl.h"
#include "j9vmls.h"

#include "jcltrace.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void *JCL_ID_CACHE; 

typedef struct JniIDCache {

	jfieldID FID_java_lang_ClassLoader_vmRef;

	jmethodID MID_com_ibm_lang_management_GarbageCollectorMXBeanImpl_getName;
	
	jmethodID MID_com_ibm_lang_management_SysinfoCpuTime_getCpuUtilization_init;
	
	jmethodID MID_java_lang_reflect_Parameter_init;

	jclass CLS_java_lang_String;
	jclass CLS_java_lang_reflect_Parameter;

#ifdef J9VM_OPT_SHARED_CLASSES
	jclass CLS_com_ibm_oti_shared_SharedClassCacheInfo;
	jmethodID MID_com_ibm_oti_shared_SharedClassCacheInfo_init;
	jclass CLS_java_util_ArrayList;
	jmethodID MID_java_util_ArrayList_add;
#endif
	
	traceDotCGlobalMemory traceGlobals;

	jclass CLS_java_lang_management_ThreadInfo;
	jclass CLS_java_lang_management_MonitorInfo;
	jclass CLS_java_lang_management_LockInfo;
	jmethodID MID_java_lang_management_ThreadInfo_init_nolocks;
	jmethodID MID_java_lang_management_ThreadInfo_init;
	jmethodID MID_java_lang_management_MonitorInfo_init;
	jmethodID MID_java_lang_management_LockInfo_init;
	jclass CLS_java_lang_StackTraceElement;
	jmethodID MID_java_lang_StackTraceElement_isNativeMethod;

	jmethodID MID_java_lang_ClassLoader_findLoadedClass;

	jclass CLS_sun_reflect_ConstantPool;
	jfieldID FID_sun_reflect_ConstantPool_constantPoolOop;

	jmethodID MID_java_lang_Class_getName;
	
	jclass CLS_com_ibm_lang_management_ProcessorUsage;
	jmethodID MID_com_ibm_lang_management_ProcessorUsage_init;
	jmethodID MID_com_ibm_lang_management_ProcessorUsage_updateValues;

	jclass CLS_com_ibm_lang_management_MemoryUsage;
	jmethodID MID_com_ibm_lang_management_MemoryUsage_updateValues;

	jclass CLS_java_com_ibm_virtualization_management_GuestOSProcessorUsage;
	jmethodID MID_java_com_ibm_virtualization_management_GuestOSProcessorUsage_updateValues;
	jclass CLS_java_com_ibm_virtualization_management_GuestOSMemoryUsage;
	jmethodID MID_java_com_ibm_virtualization_management_GuestOSMemoryUsage_updateValues;
	jmethodID MID_com_ibm_jvm_Stats_setFields;

	jclass CLS_java_lang_AnonymousClassLoader;
	jmethodID MID_java_lang_AnonymousClassLoader_init;

	jclass CLS_java_security_AccessController;
	jmethodID MID_java_security_AccessController_checkPermission;

	jclass CLS_java_com_ibm_lang_management_JvmCpuMonitorInfo;
	jmethodID MID_java_com_ibm_lang_management_JvmCpuMonitorInfo_updateValues;

	jclass CLS_java_net_URL;
	jmethodID MID_java_net_URL_getPath;
	jmethodID MID_java_net_URL_getProtocol;

	jmethodID MID_com_ibm_lang_management_internal_ExtendedGarbageCollectorMXBeanImpl_buildGcInfo;

	jmethodID MID_java_lang_StackWalker_walkWrapperImpl;

} JniIDCache;


#define JCL_CACHE_GET(env,x) \
	(((JniIDCache*) J9VMLS_GET((env), JCL_ID_CACHE))->x)

#define JCL_CACHE_SET(env,x,v) \
	(((JniIDCache*) J9VMLS_GET((env), JCL_ID_CACHE))->x = (v))

#define jclmem_allocate_memory(env, byteAmount) \
	j9mem_allocate_memory(byteAmount, J9MEM_CATEGORY_VM_JCL)

#define jclmem_free_memory(env, buf) \
	j9mem_free_memory(buf)

#ifdef __cplusplus
}
#endif

#endif /* jclglob_h */

