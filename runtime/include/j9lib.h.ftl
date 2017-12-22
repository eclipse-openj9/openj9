/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#if !defined(J9LIB_H)
#define J9LIB_H

#ifdef __cplusplus
extern"C"{
#endif

#define J9_DLL_VERSION_STRING "${uma.buildinfo.version.major}${uma.buildinfo.version.minor}"

#define J9_DEFAULT_JCL_DLL_NAME "${uma.spec.properties.defaultJclDll.value}"

<#list uma.spec.artifacts as artifact>
<#if artifact.data.dllDescription.present>
#define J9${artifact.data.dllDescription.underscored_data}_DLL_NAME "${artifact.targetNameWithRelease}"
</#if>
</#list>

/* We need certain defines... this is a big hack until we can do this another way.*/
#ifndef J9_VERBOSE_DLL_NAME
#define J9_VERBOSE_DLL_NAME "j9vrb"
#endif
#ifndef J9_IFA_DLL_NAME
#define J9_IFA_DLL_NAME "j9ifa"
#endif
#ifndef J9_JIT_DEBUG_DLL_NAME
#define J9_JIT_DEBUG_DLL_NAME "j9jitd${uma.buildinfo.version.major}${uma.buildinfo.version.minor}"
#endif
#ifndef J9_VM_DLL_NAME
#define J9_VM_DLL_NAME "j9vm${uma.buildinfo.version.major}${uma.buildinfo.version.minor}"
#endif
#ifndef J9_SHARED_DLL_NAME
#define J9_SHARED_DLL_NAME "j9shr${uma.buildinfo.version.major}${uma.buildinfo.version.minor}"
#endif
#ifndef J9_VERIFY_DLL_NAME
#define J9_VERIFY_DLL_NAME "j9bcv${uma.buildinfo.version.major}${uma.buildinfo.version.minor}"
#endif
#ifndef J9_DEBUG_DLL_NAME
#define J9_DEBUG_DLL_NAME "j9dbg${uma.buildinfo.version.major}${uma.buildinfo.version.minor}"
#endif
#ifndef J9_GC_DLL_NAME
#define J9_GC_DLL_NAME "j9gc${uma.buildinfo.version.major}${uma.buildinfo.version.minor}"
#endif
#ifndef J9_HOOKABLE_DLL_NAME
#define J9_HOOKABLE_DLL_NAME "j9hookable${uma.buildinfo.version.major}${uma.buildinfo.version.minor}"
#endif
#ifndef J9_RAS_DUMP_DLL_NAME
#define J9_RAS_DUMP_DLL_NAME "j9dmp${uma.buildinfo.version.major}${uma.buildinfo.version.minor}"
#endif
#ifndef J9_DYNLOAD_DLL_NAME
#define J9_DYNLOAD_DLL_NAME "j9dyn${uma.buildinfo.version.major}${uma.buildinfo.version.minor}"
#endif
#ifndef J9_CHECK_JNI_DLL_NAME
#define J9_CHECK_JNI_DLL_NAME "j9jnichk${uma.buildinfo.version.major}${uma.buildinfo.version.minor}"
#endif
#ifndef J9_CHECK_VM_DLL_NAME
#define J9_CHECK_VM_DLL_NAME "j9vmchk${uma.buildinfo.version.major}${uma.buildinfo.version.minor}"
#endif
#ifndef J9_THREAD_DLL_NAME
#define J9_THREAD_DLL_NAME "j9thr${uma.buildinfo.version.major}${uma.buildinfo.version.minor}"
#endif
#ifndef J9_GATEWAY_RESMAN_DLL_NAME
#define J9_GATEWAY_RESMAN_DLL_NAME "jclrm_${uma.buildinfo.version.major}${uma.buildinfo.version.minor}"
#endif
#ifndef J9_JIT_DLL_NAME
#define J9_JIT_DLL_NAME "j9jit${uma.buildinfo.version.major}${uma.buildinfo.version.minor}"
#endif
#ifndef J9_JVMTI_DLL_NAME
#define J9_JVMTI_DLL_NAME "j9jvmti${uma.buildinfo.version.major}${uma.buildinfo.version.minor}"
#endif
#ifndef J9_VERBOSE_DLL_NAME
#define J9_VERBOSE_DLL_NAME "j9vrb${uma.buildinfo.version.major}${uma.buildinfo.version.minor}"
#endif
#ifndef J9_PORT_DLL_NAME
#define J9_PORT_DLL_NAME "j9prt${uma.buildinfo.version.major}${uma.buildinfo.version.minor}"
#endif
#ifndef J9_MAX_DLL_NAME
#define J9_MAX_DLL_NAME "jclmax_${uma.buildinfo.version.major}${uma.buildinfo.version.minor}"
#endif
#ifndef J9_RAS_TRACE_DLL_NAME
#define J9_RAS_TRACE_DLL_NAME "j9trc${uma.buildinfo.version.major}${uma.buildinfo.version.minor}"
#endif
#ifndef J9_JEXTRACT_DLL_NAME
#define J9_JEXTRACT_DLL_NAME "j9jextract"
#endif
#ifndef J9_JCL_CLEAR_DLL_NAME
#define J9_JCL_CLEAR_DLL_NAME "jclclear_${uma.buildinfo.version.major}${uma.buildinfo.version.minor}"
#endif
#ifndef J9_JCL_HS60_DLL_NAME
#define J9_JCL_HS60_DLL_NAME "jclhs60_${uma.buildinfo.version.major}${uma.buildinfo.version.minor}"
#endif
#ifndef J9_ZIP_DLL_NAME
#define J9_ZIP_DLL_NAME "j9zlib${uma.buildinfo.version.major}${uma.buildinfo.version.minor}"
#endif

/* This is an old DLL that should be removed later. */
#define J9_AOT_DEBUG_DLL_NAME "bogus old dll"

#ifdef __cplusplus
}
#endif

#endif /* J9LIB_H */
