dnl Copyright IBM Corp. and others 2001
dnl
dnl This program and the accompanying materials are made available under
dnl the terms of the Eclipse Public License 2.0 which accompanies this
dnl distribution and is available at https://www.eclipse.org/legal/epl-2.0/
dnl or the Apache License, Version 2.0 which accompanies this distribution and
dnl is available at https://www.apache.org/licenses/LICENSE-2.0.
dnl
dnl This Source Code may also be made available under the following
dnl Secondary Licenses when the conditions for such availability set
dnl forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
dnl General Public License, version 2 with the GNU Classpath
dnl Exception [1] and GNU General Public License, version 2 with the
dnl OpenJDK Assembly Exception [2].
dnl
dnl [1] https://www.gnu.org/software/classpath/license.html
dnl [2] https://openjdk.org/legal/assembly-exception.html
dnl
dnl SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
dnl
dnl (name, cc, decorate, ret, args...)
_X(jio_vfprintf,,false,int,FILE *stream, const char *format, va_list args)
_X(jio_vsnprintf,,false,int,char *str, int n, const char *format, va_list args)
_IF([defined(J9ZOS390)],
	[_X(JNI_a2e_vsprintf,,false,jint,char *target, const char *format, va_list args)])
_X(JVM_Accept,JNICALL,true,jint,jint descriptor, struct sockaddr *address, int *length)
_X(JVM_ActiveProcessorCount,JNICALL,true,jint,void)
_X(JVM_AllocateNewArray,JNICALL,true,jobject,JNIEnv *env, jclass caller, jclass current, jint length)
_X(JVM_AllocateNewObject,JNICALL,true,jobject,JNIEnv *env, jclass caller, jclass current, jclass init)
_X(JVM_Available,JNICALL,true,jint,jint descriptor, jlong *bytes)
_X(JVM_ClassDepth,JNICALL,true,jint,JNIEnv *env, jstring name)
_X(JVM_ClassLoaderDepth,JNICALL,true,jint,JNIEnv *env)
_X(JVM_Close,JNICALL,true,jint,jint descriptor)
_X(JVM_Connect,JNICALL,true,jint,jint descriptor, const struct sockaddr *address, int length)
_IF([JAVA_SPEC_VERSION < 26],
	[_X(JVM_ConstantPoolGetClassAt,JNICALL,true,jclass,JNIEnv *env, jobject anObject, jobject constantPool, jint index)],
	[_X(JVM_ConstantPoolGetClassAt,JNICALL,true,jclass,JNIEnv *env, jobject constantPool, jint index)])
_IF([JAVA_SPEC_VERSION < 26],
	[_X(JVM_ConstantPoolGetClassAtIfLoaded,JNICALL,true,jclass,JNIEnv *env, jobject anObject, jobject constantPool, jint index)],
	[_X(JVM_ConstantPoolGetClassAtIfLoaded,JNICALL,true,jclass,JNIEnv *env, jobject constantPool, jint index)])
_IF([(11 <= JAVA_SPEC_VERSION) && (JAVA_SPEC_VERSION < 26)],
	[_X(JVM_ConstantPoolGetClassRefIndexAt,JNICALL,false,jint,JNIEnv *env, jobject arg1, jobject arg2, jint arg3)])
_IF([JAVA_SPEC_VERSION >= 26],
	[_X(JVM_ConstantPoolGetClassRefIndexAt,JNICALL,false,jint,JNIEnv *env, jobject arg1, jint arg2)])
_IF([JAVA_SPEC_VERSION < 26],
	[_X(JVM_ConstantPoolGetDoubleAt,JNICALL,true,jdouble,JNIEnv *env, jobject anObject, jobject constantPool, jint index)],
	[_X(JVM_ConstantPoolGetDoubleAt,JNICALL,true,jdouble,JNIEnv *env, jobject constantPool, jint index)])
_IF([JAVA_SPEC_VERSION < 26],
	[_X(JVM_ConstantPoolGetFieldAt,JNICALL,true,jobject,JNIEnv *env, jobject anObject, jobject constantPool, jint index)],
	[_X(JVM_ConstantPoolGetFieldAt,JNICALL,true,jobject,JNIEnv *env, jobject constantPool, jint index)])
_IF([JAVA_SPEC_VERSION < 26],
	[_X(JVM_ConstantPoolGetFieldAtIfLoaded,JNICALL,true,jobject,JNIEnv *env, jobject anObject, jobject constantPool, jint index)],
	[_X(JVM_ConstantPoolGetFieldAtIfLoaded,JNICALL,true,jobject,JNIEnv *env, jobject constantPool, jint index)])
_IF([JAVA_SPEC_VERSION < 26],
	[_X(JVM_ConstantPoolGetFloatAt,JNICALL,true,jfloat,JNIEnv *env, jobject anObject, jobject constantPool, jint index)],
	[_X(JVM_ConstantPoolGetFloatAt,JNICALL,true,jfloat,JNIEnv *env, jobject constantPool, jint index)])
_IF([JAVA_SPEC_VERSION < 26],
	[_X(JVM_ConstantPoolGetIntAt,JNICALL,true,jint,JNIEnv *env, jobject anObject, jobject constantPool, jint index)],
	[_X(JVM_ConstantPoolGetIntAt,JNICALL,true,jint,JNIEnv *env, jobject constantPool, jint index)])
_IF([JAVA_SPEC_VERSION < 26],
	[_X(JVM_ConstantPoolGetLongAt,JNICALL,true,jlong,JNIEnv *env, jobject anObject, jobject constantPool, jint index)],
	[_X(JVM_ConstantPoolGetLongAt,JNICALL,true,jlong,JNIEnv *env, jobject constantPool, jint index)])
_IF([JAVA_SPEC_VERSION < 26],
	[_X(JVM_ConstantPoolGetMemberRefInfoAt,JNICALL,true,jobjectArray,JNIEnv *env, jobject anObject, jobject constantPool, jint index)],
	[_X(JVM_ConstantPoolGetMemberRefInfoAt,JNICALL,true,jobjectArray,JNIEnv *env, jobject constantPool, jint index)])
_IF([JAVA_SPEC_VERSION < 26],
	[_X(JVM_ConstantPoolGetMethodAt,JNICALL,true,jobject,JNIEnv *env, jobject anObject, jobject constantPool, jint index)],
	[_X(JVM_ConstantPoolGetMethodAt,JNICALL,true,jobject,JNIEnv *env, jobject constantPool, jint index)])
_IF([JAVA_SPEC_VERSION < 26],
	[_X(JVM_ConstantPoolGetMethodAtIfLoaded,JNICALL,true,jobject,JNIEnv *env, jobject anObject, jobject constantPool, jint index)],
	[_X(JVM_ConstantPoolGetMethodAtIfLoaded,JNICALL,true,jobject,JNIEnv *env, jobject constantPool, jint index)])
_IF([(11 <= JAVA_SPEC_VERSION) && (JAVA_SPEC_VERSION < 26)],
	[_X(JVM_ConstantPoolGetNameAndTypeRefIndexAt,JNICALL,false,jint,JNIEnv *env, jobject arg1, jobject arg2, jint arg3)])
_IF([JAVA_SPEC_VERSION >= 26],
	[_X(JVM_ConstantPoolGetNameAndTypeRefIndexAt,JNICALL,false,jint,JNIEnv *env, jobject arg1, jint arg2)])
_IF([(11 <= JAVA_SPEC_VERSION) && (JAVA_SPEC_VERSION < 26)],
	[_X(JVM_ConstantPoolGetNameAndTypeRefInfoAt,JNICALL,false,jobjectArray,JNIEnv *env, jobject arg1, jobject arg2, jint arg3)])
_IF([JAVA_SPEC_VERSION >= 26],
	[_X(JVM_ConstantPoolGetNameAndTypeRefInfoAt,JNICALL,false,jobjectArray,JNIEnv *env, jobject arg1, jint arg2)])
_IF([JAVA_SPEC_VERSION < 26],
	[_X(JVM_ConstantPoolGetSize,JNICALL,true,jint,JNIEnv *env, jobject anObject, jobject constantPool)],
	[_X(JVM_ConstantPoolGetSize,JNICALL,true,jint,JNIEnv *env, jobject constantPool)])
_IF([JAVA_SPEC_VERSION < 26],
	[_X(JVM_ConstantPoolGetStringAt,JNICALL,true,jstring,JNIEnv *env, jobject anObject, jobject constantPool, jint index)],
	[_X(JVM_ConstantPoolGetStringAt,JNICALL,true,jstring,JNIEnv *env, jobject constantPool, jint index)])
_IF([(11 <= JAVA_SPEC_VERSION) && (JAVA_SPEC_VERSION < 26)],
	[_X(JVM_ConstantPoolGetTagAt,JNICALL,false,jbyte,JNIEnv *env, jobject arg1, jobject arg2, jint arg3)])
_IF([JAVA_SPEC_VERSION >= 26],
	[_X(JVM_ConstantPoolGetTagAt,JNICALL,false,jbyte,JNIEnv *env, jobject arg2, jint arg3)])
_IF([JAVA_SPEC_VERSION < 26],
	[_X(JVM_ConstantPoolGetUTF8At,JNICALL,true,jstring,JNIEnv *env, jobject anObject, jobject constantPool, jint index)],
	[_X(JVM_ConstantPoolGetUTF8At,JNICALL,true,jstring,JNIEnv *env, jobject constantPool, jint index)])
_X(JVM_CurrentClassLoader,JNICALL,true,jobject,JNIEnv *env)
_X(JVM_CurrentLoadedClass,JNICALL,true,jclass,JNIEnv *env)
_X(JVM_CurrentTimeMillis,JNICALL,true,jlong,JNIEnv *env, jint unused1)
_X(JVM_DefineClassWithSource,JNICALL,true,jclass,JNIEnv *env, const char *className, jobject classLoader, const jbyte *classArray, jsize length, jobject domain, const char *source)
_X(JVM_DumpThreads,JNICALL,true,jobjectArray,JNIEnv *env, jclass thread, jobjectArray threadArray)
_X(JVM_ExpandFdTable,JNICALL,true,jint,jint fd)
_X(JVM_FindLibraryEntry,JNICALL,true,void *,void *handle, const char *functionName)
_X(JVM_FindSignal,JNICALL,true,jint,const char *sigName)
_X(JVM_FreeMemory,JNICALL,true,jlong,void)
_X(JVM_GC,JNICALL,true,void,void)
_X(JVM_GCNoCompact,JNICALL,true,void,void)
_X(JVM_GetAllThreads,JNICALL,true,jobjectArray,JNIEnv *env, jclass aClass)
_IF([JAVA_SPEC_VERSION < 11],
	[_X(JVM_GetCallerClass,JNICALL,true,jobject,JNIEnv *env, jint depth)],
	[_X(JVM_GetCallerClass,JNICALL,true,jobject,JNIEnv *env)])
_IF([JAVA_SPEC_VERSION < 26],
	[_X(JVM_GetClassAccessFlags,JNICALL,true,jint,JNIEnv *env, jclass clazzRef)])
_X(JVM_GetClassAnnotations,JNICALL,true,jbyteArray,JNIEnv *env, jclass target)
_X(JVM_GetClassConstantPool,JNICALL,true,jobject,JNIEnv *env, jclass target)
_IF([JAVA_SPEC_VERSION < 24],
	[_X(JVM_GetClassContext,JNICALL,true,jobject,JNIEnv *env)])
_X(JVM_GetClassLoader,JNICALL,true,jobject,JNIEnv *env, jobject obj)
_IF([JAVA_SPEC_VERSION < 11],
	[_X(JVM_GetClassName,JNICALL,true,jstring, JNIEnv *env, jclass theClass)],
	[_X(JVM_InitClassName,JNICALL,true,jstring, JNIEnv *env, jclass theClass)])
_X(JVM_GetClassSignature,JNICALL,true,jstring,JNIEnv *env, jclass target)
_X(JVM_GetEnclosingMethodInfo,JNICALL,true,jobjectArray,JNIEnv *env, jclass theClass)
_IF([JAVA_SPEC_VERSION < 17],
	[_X(JVM_GetInterfaceVersion,JNICALL,true,jint,void)])
_X(JVM_GetLastErrorString,JNICALL,true,jint,char *buffer, jint length)
_X(JVM_GetManagement,JNICALL,true,void *,jint version)
_X(JVM_GetPortLibrary,JNICALL,true,struct J9PortLibrary *,void)
_X(JVM_GetSystemPackage,JNICALL,true,jstring,JNIEnv *env, jstring pkgName)
_X(JVM_GetSystemPackages,JNICALL,true,jobject,JNIEnv *env)
_X(JVM_GetThreadInterruptEvent,JNICALL,true,void *,void)
_X(JVM_Halt,JNICALL,true,void,jint exitCode)
_X(JVM_InitializeSocketLibrary,JNICALL,true,jint,void)
_X(JVM_InvokeMethod,JNICALL,true,jobject,JNIEnv *env, jobject method, jobject obj, jobjectArray args)
_IF([defined(J9VM_ZOS_3164_INTEROPERABILITY) && (JAVA_SPEC_VERSION >= 17)],
	_X(JVM_Invoke31BitJNI_OnXLoad,JNICALL,true,jint,JavaVM *vm,void *handle,jboolean isOnLoad,void *reserved))
_X(JVM_IsNaN,JNICALL,true,jboolean,jdouble dbl)
_X(JVM_LatestUserDefinedLoader,JNICALL,true,jobject,JNIEnv *env)
_X(JVM_Listen,JNICALL,true,jint,jint descriptor, jint count)
_IF([JAVA_SPEC_VERSION < 11],
	_X(JVM_LoadLibrary,JNICALL,true,void *,const char *libName),
	_X(JVM_LoadLibrary,JNICALL,true,void *,const char *libName, jboolean throwOnFailure))
_X(JVM_Lseek,JNICALL,true,jlong,jint descriptor, jlong bytesToSeek, jint origin)
_X(JVM_MaxMemory,JNICALL,true,jlong,void)
_X(JVM_MaxObjectInspectionAge,JNICALL,true,jlong,void)
_X(JVM_MonitorNotify,JNICALL,true,void,JNIEnv *env, jobject anObject)
_X(JVM_MonitorNotifyAll,JNICALL,true,void,JNIEnv *env, jobject anObject)
_X(JVM_MonitorWait,JNICALL,true,void,JNIEnv *env, jobject anObject, jlong timeout)
_X(JVM_NanoTime,JNICALL,true,jlong,JNIEnv *env, jclass aClass)
_X(JVM_NativePath,JNICALL,true,char *,char *path)
_X(JVM_NewInstanceFromConstructor,JNICALL,true,jobject,JNIEnv *env, jobject c, jobjectArray args)
_X(JVM_Open,JNICALL,true,jint,const char *filename, jint flags, jint mode)
_X(JVM_RaiseSignal,JNICALL,true,jboolean,jint sigNum)
_X(JVM_RawMonitorCreate,JNICALL,true,void *,void)
_X(JVM_RawMonitorDestroy,JNICALL,true,void,void *deadMonitor)
_X(JVM_RawMonitorEnter,JNICALL,true,jint,void *monitor)
_X(JVM_RawMonitorExit,JNICALL,true,void,void *monitor)
_X(JVM_Read,JNICALL,true,jint,jint descriptor, char *buffer, jint bytesToRead)
_X(JVM_Recv,JNICALL,true,jint,jint descriptor, char *buffer, jint length, jint flags)
_X(JVM_RecvFrom,JNICALL,true,jint,jint descriptor, char *buffer, jint length, jint flags, struct sockaddr *fromAddr, int *fromLength)
_X(JVM_RegisterSignal,JNICALL,true,void *,jint sigNum, void *handler)
_X(JVM_Send,JNICALL,true,jint,jint descriptor, const char *buffer, jint numBytes, jint flags)
_X(JVM_SendTo,JNICALL,true,jint,jint descriptor, const char *buffer, jint length, jint flags, const struct sockaddr *toAddr, int toLength)
_X(JVM_SetLength,JNICALL,true,jint,jint fd, jlong length)
_X(JVM_Sleep,JNICALL,true,void,JNIEnv *env, jclass thread, jlong timeout)
_X(JVM_Socket,JNICALL,true,jint,jint domain, jint type, jint protocol)
_X(JVM_SocketAvailable,JNICALL,true,jint,jint descriptor, jint *result)
_X(JVM_SocketClose,JNICALL,true,jint,jint descriptor)
_X(JVM_Startup,JNICALL,true,jint,JavaVM *vm, JNIEnv *env)
_IF([JAVA_SPEC_VERSION < 22],
	[_X(JVM_SupportsCX8,JNICALL,true,jboolean,void)])
_X(JVM_Sync,JNICALL,true,jint,jint descriptor)
_X(JVM_Timeout,JNICALL,true,jint,jint descriptor, jint timeout)
_X(JVM_TotalMemory,JNICALL,true,jlong,void)
_X(JVM_TraceInstructions,JNICALL,true,void,jboolean on)
_X(JVM_TraceMethodCalls,JNICALL,true,void,jboolean on)
_X(JVM_UcsOpen,JNICALL,true,jint,const jchar *filename, jint flags, jint mode)
_IF([defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT) && (JAVA_SPEC_VERSION >= 17)],
	[_X(JVM_ValidateJNILibrary,JNICALL,true,void *,const char *libName, void *handle, jboolean isStatic)])
_X(JVM_Write,JNICALL,true,jint,jint descriptor, const char *buffer, jint length)
_IF([defined(J9ZOS390)],
	[_X(NewStringPlatform,,false,jint,JNIEnv *env, const char *instr, jstring *outstr, const char *encoding)])
_X(post_block,,false,int,void)
_X(pre_block,,false,int,pre_block_t buf)
_X(JVM_ZipHook,JNICALL,true,void,JNIEnv *env, const char *filename, jint newState)
_X(JVM_RawAllocate,JNICALL,true,void *,size_t size, const char *callsite)
_X(JVM_RawRealloc,JNICALL,true,void *,void *ptr, size_t size, const char *callsite)
_X(JVM_RawCalloc,JNICALL,true,void *,size_t nmemb, size_t size, const char *callsite)
_X(JVM_RawAllocateInCategory,JNICALL,true,void *,size_t size, const char *callsite, jint category)
_X(JVM_RawReallocInCategory,JNICALL,true,void *,void *ptr, size_t size, const char *callsite, jint category)
_X(JVM_RawCallocInCategory,JNICALL,true,void *,size_t nmemb, size_t size, const char *callsite, jint category)
_X(JVM_RawFree,JNICALL,true,void,void *ptr)
_X(JVM_ArrayCopy,JNICALL,true,void,JNIEnv *env, jclass ignored, jobject src, jint src_pos, jobject dst, jint dst_pos, jint length)
_X(JVM_AssertionStatusDirectives,JNICALL,true,jobject,jint arg0, jint arg1)
_X(JVM_Clone,JNICALL,true,jobject,jint arg0, jint arg1)
_X(JVM_CompileClass,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_CompileClasses,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_CompilerCommand,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_CountStackFrames,JNICALL,true,jobject,jint arg0, jint arg1)
_X(JVM_CurrentThread,JNICALL,true,jobject,JNIEnv *env, jclass java_lang_Thread)
_X(JVM_DesiredAssertionStatus,JNICALL,true,jboolean,JNIEnv *env, jobject arg1, jobject arg2)
_X(JVM_DisableCompiler,JNICALL,true,jobject,jint arg0, jint arg1)
_IF([(JAVA_SPEC_VERSION == 8) && defined(WIN32)],
	[_X(JVM_DoPrivileged,JNICALL,true,jobject,JNIEnv *env, jobject java_security_AccessController, jobject action, jboolean unknown, jboolean isExceptionAction)])
_X(JVM_EnableCompiler,JNICALL,true,jobject,jint arg0, jint arg1)
_X(JVM_FillInStackTrace,JNICALL,true,void,JNIEnv *env, jobject throwable)
_X(JVM_FindClassFromClassLoader,JNICALL,true,jobject,JNIEnv *env, char *className, jboolean init, jobject classLoader, jboolean throwError)
_X(JVM_FindClassFromBootLoader,JNICALL,false,jobject,JNIEnv *env, char *className)
_X(JVM_FindLoadedClass,JNICALL,true,jobject,JNIEnv *env, jobject classLoader, jobject className)
_X(JVM_FindPrimitiveClass,JNICALL,true,jobject,JNIEnv *env, char *className)
_X(JVM_GetArrayElement,JNICALL,true,jobject,JNIEnv *env, jobject arr, jint index)
_X(JVM_GetArrayLength,JNICALL,true,jint,JNIEnv *env, jobject arr)
_X(JVM_GetClassDeclaredConstructors,JNICALL,true,jobject,JNIEnv *env, jclass clazz, jboolean unknown)
_X(JVM_GetClassDeclaredFields,JNICALL,true,jobject,JNIEnv *env, jobject clazz, jint arg2)
_X(JVM_GetClassDeclaredMethods,JNICALL,true,jobject,JNIEnv *env, jobject clazz, jboolean unknown)
_X(JVM_GetClassInterfaces,JNICALL,true,jobject,jint arg0, jint arg1)
_IF([JAVA_SPEC_VERSION < 25],
	[_X(JVM_GetClassModifiers,JNICALL,true,jint,JNIEnv *env, jclass clazz)])
_X(JVM_GetClassSigners,JNICALL,true,jobject,jint arg0, jint arg1)
_X(JVM_GetComponentType,JNICALL,true,jobject,JNIEnv *env, jclass cls)
_X(JVM_GetDeclaredClasses,JNICALL,true,jobject,jint arg0, jint arg1)
_X(JVM_GetDeclaringClass,JNICALL,true,jobject,jint arg0, jint arg1)
_IF([JAVA_SPEC_VERSION < 24],
	[_X(JVM_GetInheritedAccessControlContext,JNICALL,true,jobject,jint arg0, jint arg1)])
_X(JVM_GetPrimitiveArrayElement,JNICALL,true,jvalue,JNIEnv *env, jobject arr, jint index, jint wCode)
_IF([JAVA_SPEC_VERSION < 25],
	[_X(JVM_GetProtectionDomain,JNICALL,true,jobject,jint arg0, jint arg1)])
_IF([JAVA_SPEC_VERSION < 24],
	[_X(JVM_GetStackAccessControlContext,JNICALL,true,jobject,JNIEnv *env, jclass java_security_AccessController)])
_X(JVM_GetStackTraceDepth,JNICALL,true,jint,JNIEnv *env, jobject throwable)
_X(JVM_GetStackTraceElement,JNICALL,true,jobject,JNIEnv *env, jobject throwable, jint index)
_X(JVM_HoldsLock,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_IHashCode,JNICALL,true,jint,JNIEnv *env, jobject obj)
_X(JVM_InitProperties,JNICALL,true,jobject,JNIEnv *env, jobject properties)
_X(JVM_InternString,JNICALL,true,jstring,JNIEnv *env, jstring str)
_X(JVM_Interrupt,JNICALL,true,jobject,jint arg0, jint arg1)
_IF([JAVA_SPEC_VERSION < 25],
	[_X(JVM_IsArrayClass,JNICALL,true,jboolean,JNIEnv *env, jclass clazz)])
_X(JVM_IsInterface,JNICALL,true,jboolean,JNIEnv *env, jclass clazz)
_X(JVM_IsInterrupted,JNICALL,true,jboolean,JNIEnv *env, jobject thread, jboolean unknown)
_IF([JAVA_SPEC_VERSION < 25],
	[_X(JVM_IsPrimitiveClass,JNICALL,true,jboolean,JNIEnv *env, jclass clazz)])
_X(JVM_IsSupportedJNIVersion,JNICALL,true,jboolean,jint jniVersion)
_IF([JAVA_SPEC_VERSION < 17],
	[_X(JVM_IsThreadAlive,JNICALL,true,jboolean,JNIEnv *env, jobject targetThread)])
_X(JVM_NewArray,JNICALL,true,jobject,JNIEnv *env, jclass componentType, jint dimension)
_X(JVM_NewMultiArray,JNICALL,true,jobject,JNIEnv *env, jclass eltClass, jintArray dim)
_X(JVM_ResolveClass,JNICALL,true,jobject,jint arg0, jint arg1)
_IF([JAVA_SPEC_VERSION < 20],
	[_X(JVM_ResumeThread,JNICALL,true,jobject,jint arg0, jint arg1)])
_X(JVM_SetArrayElement,JNICALL,true,void,JNIEnv *env, jobject array, jint index, jobject value)
_X(JVM_SetClassSigners,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_SetPrimitiveArrayElement,JNICALL,true,void,JNIEnv *env, jobject array, jint index, jvalue value, unsigned char vCode)
_X(JVM_SetProtectionDomain,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_SetThreadPriority,JNICALL,true,void,JNIEnv *env, jobject thread, jint priority)
_X(JVM_StartThread,JNICALL,true,void,JNIEnv *env, jobject newThread)
_IF([JAVA_SPEC_VERSION < 20],
	[_X(JVM_StopThread,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)])
_IF([JAVA_SPEC_VERSION < 20],
	[_X(JVM_SuspendThread,JNICALL,true,jobject,jint arg0, jint arg1)])
_IF([JAVA_SPEC_VERSION < 15],
	[_X(JVM_UnloadLibrary, JNICALL, true, jobject, jint arg0)],
	[_X(JVM_UnloadLibrary, JNICALL, true, void, void *handle)])
_X(JVM_Yield,JNICALL,true,jobject,jint arg0, jint arg1)
_X(JVM_SetSockOpt,JNICALL,true,jint,jint fd, int level, int optname, const char *optval, int optlen)
_X(JVM_GetSockOpt,JNICALL,true,jint,jint fd, int level, int optname, char *optval, int *optlen)
_X(JVM_SocketShutdown,JNICALL,true,jint,jint fd, jint howto)
_X(JVM_GetSockName,JNICALL,true,jint,jint fd, struct sockaddr *him, int *len)
_X(JVM_GetHostName,JNICALL,true,int,char *name, int namelen)
_X(JVM_GetMethodIxLocalsCount,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_GetCPMethodNameUTF,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_GetMethodIxExceptionTableEntry,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2, jint arg3, jint arg4)
_X(JVM_GetMethodIxExceptionTableLength,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_GetMethodIxMaxStack,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_GetMethodIxExceptionIndexes,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2, jint arg3)
_X(JVM_GetCPFieldSignatureUTF,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_GetClassMethodsCount,JNICALL,true,jobject,jint arg0, jint arg1)
_X(JVM_GetClassFieldsCount,JNICALL,true,jobject,jint arg0, jint arg1)
_X(JVM_GetClassCPTypes,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_GetClassCPEntriesCount,JNICALL,true,jobject,jint arg0, jint arg1)
_X(JVM_GetCPMethodSignatureUTF,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_GetCPFieldModifiers,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2, jint arg3)
_X(JVM_GetCPMethodModifiers,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2, jint arg3)
_X(JVM_IsSameClassPackage,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_GetCPMethodClassNameUTF,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_GetCPFieldClassNameUTF,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_GetCPClassNameUTF,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_GetMethodIxArgsSize,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_GetMethodIxModifiers,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_IsConstructorIx,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_GetMethodIxByteCodeLength,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_GetMethodIxByteCode,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2, jint arg3)
_X(JVM_GetFieldIxModifiers,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_FindClassFromClass,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2, jint arg3)
_X(JVM_GetClassNameUTF,JNICALL,true,jobject,jint arg0, jint arg1)
_X(JVM_GetMethodIxNameUTF,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_GetMethodIxSignatureUTF,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_GetMethodIxExceptionsCount,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_ReleaseUTF,JNICALL,true,jobject,jint arg0)
_IF([defined(J9ZOS390)],
	[_X(GetStringPlatform,,false,jint,JNIEnv *env, jstring instr, char *outstr, jint outlen, const char *encoding)])
_IF([defined(J9ZOS390)],
	[_X(GetStringPlatformLength,,false,jint,JNIEnv *env, jstring instr, jint *outlen, const char *encoding)])
_X(JVM_ExtendBootClassPath,JNICALL,true,void,JNIEnv *env, const char *path)
_X(JVM_Bind,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_IF([JAVA_SPEC_VERSION < 17],
	[_X(JVM_DTraceActivate,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2, jint arg3, jint arg4)])
_IF([JAVA_SPEC_VERSION < 17],
	[_X(JVM_DTraceDispose,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)])
_IF([JAVA_SPEC_VERSION < 17],
	[_X(JVM_DTraceGetVersion,JNICALL,true,jobject,jint arg0)])
_IF([JAVA_SPEC_VERSION < 17],
	[_X(JVM_DTraceIsProbeEnabled,JNICALL,true,jobject,jint arg0, jint arg1)])
_IF([JAVA_SPEC_VERSION < 17],
	[_X(JVM_DTraceIsSupported,JNICALL,true,jboolean,JNIEnv *env)])
_X(JVM_DefineClass,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2, jint arg3, jint arg4, jint arg5)
_X(JVM_DefineClassWithSourceCond,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2, jint arg3, jint arg4, jint arg5, jint arg6, jint arg7)
_X(JVM_EnqueueOperation,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2, jint arg3, jint arg4)
_X(JVM_GetCPFieldNameUTF,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_GetClassConstructor,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2, jint arg3)
_X(JVM_GetClassConstructors,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_GetClassField,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2, jint arg3)
_X(JVM_GetClassFields,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_GetClassMethod,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2, jint arg3, jint arg4)
_X(JVM_GetClassMethods,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_GetField,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_GetFieldAnnotations,JNICALL,true,jobject,jint arg0, jint arg1)
_X(JVM_GetMethodAnnotations,JNICALL,true,jobject,jint arg0, jint arg1)
_X(JVM_GetMethodDefaultAnnotationValue,JNICALL,true,jobject,jint arg0, jint arg1)
_X(JVM_GetMethodParameterAnnotations,JNICALL,true,jobject,jint arg0, jint arg1)
_X(JVM_GetClassTypeAnnotations,JNICALL,true,jbyteArray,JNIEnv *env, jobject cls)
_X(JVM_GetFieldTypeAnnotations,JNICALL,true,jbyteArray,JNIEnv *env, jobject field)
_X(JVM_GetMethodParameters,JNICALL,true,jobjectArray,JNIEnv *env, jobject method)
_X(JVM_GetMethodTypeAnnotations,JNICALL,true,jbyteArray,JNIEnv *env, jobject method)
_X(JVM_IsVMGeneratedMethodIx,JNICALL,true,jboolean,JNIEnv *env, jclass cb, jint index)
_X(JVM_GetTemporaryDirectory,JNICALL,false,jstring,JNIEnv *env)
_X(JVM_CopySwapMemory,JNICALL,true,void,JNIEnv *env, jobject srcObj, jlong srcOffset, jobject dstObj, jlong dstOffset, jlong size, jlong elemSize)
_X(JVM_GetPrimitiveField,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2, jint arg3)
_X(JVM_InitializeCompiler,JNICALL,true,jobject,jint arg0, jint arg1)
_X(JVM_IsSilentCompiler,JNICALL,true,jobject,jint arg0, jint arg1)
_X(JVM_LoadClass0,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2, jint arg3)
_X(JVM_NewInstance,JNICALL,true,jobject,jint arg0, jint arg1)
_X(JVM_PrintStackTrace,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2)
_X(JVM_SetField,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2, jint arg3)
_X(JVM_SetPrimitiveField,JNICALL,true,jobject,jint arg0, jint arg1, jint arg2, jint arg3, jint arg4, jint arg5)
_X(JVM_SetNativeThreadName,JNICALL,true,void,jint arg0, jobject arg1, jstring arg2)
_IF([(9 <= JAVA_SPEC_VERSION) && (JAVA_SPEC_VERSION < 15)],
	[_X(JVM_DefineModule,JNICALL,false,jobject,JNIEnv *env, jobject arg1, jboolean arg2, jstring arg3, jstring arg4, const char *const *arg5, jsize arg6)])
_IF([JAVA_SPEC_VERSION >= 15],
	[_X(JVM_DefineModule,JNICALL,false,jobject,JNIEnv *env, jobject arg1, jboolean arg2, jstring arg3, jstring arg4, jobjectArray arg5)])
_IF([(9 <= JAVA_SPEC_VERSION) && (JAVA_SPEC_VERSION < 15)],
	[_X(JVM_AddModuleExports,JNICALL,false,void,JNIEnv *env, jobject arg1, const char *arg2, jobject arg3)])
_IF([JAVA_SPEC_VERSION >= 15],
	[_X(JVM_AddModuleExports,JNICALL,false,void,JNIEnv *env, jobject arg1, jstring arg2, jobject arg3)])
_IF([(9 <= JAVA_SPEC_VERSION) && (JAVA_SPEC_VERSION < 15)],
	[_X(JVM_AddModuleExportsToAll,JNICALL,false,void,JNIEnv *env, jobject arg1, const char *arg2)])
_IF([JAVA_SPEC_VERSION >= 15],
	[_X(JVM_AddModuleExportsToAll,JNICALL,false,void,JNIEnv *env, jobject arg1, jstring arg2)])
_IF([JAVA_SPEC_VERSION >= 11],
	[_X(JVM_AddReadsModule,JNICALL,false,void,JNIEnv *env, jobject arg1, jobject arg2)])
_IF([JAVA_SPEC_VERSION >= 11],
	[_X(JVM_CanReadModule,JNICALL,false,jboolean,JNIEnv *env, jobject arg1, jobject arg2)])
_IF([JAVA_SPEC_VERSION >= 9],
	[_X(JVM_AddModulePackage,JNICALL,false,void,JNIEnv *env, jobject arg1, const char *arg2)])
_IF([(9 <= JAVA_SPEC_VERSION) && (JAVA_SPEC_VERSION < 15)],
	[_X(JVM_AddModuleExportsToAllUnnamed,JNICALL,false,void,JNIEnv *env, jobject arg1, const char *arg2)])
_IF([JAVA_SPEC_VERSION >= 15],
	[_X(JVM_AddModuleExportsToAllUnnamed,JNICALL,false,void,JNIEnv *env, jobject arg1, jstring arg2)])
_IF([JAVA_SPEC_VERSION >= 11],
	[_X(JVM_GetSimpleBinaryName,JNICALL,false,jstring,JNIEnv *env, jclass arg1)])
_IF([JAVA_SPEC_VERSION >= 11],
	[_X(JVM_SetMethodInfo,JNICALL,false,void,JNIEnv *env, jobject arg1)])
_IF([(11 <= JAVA_SPEC_VERSION) && (JAVA_SPEC_VERSION < 22)],
	[_X(JVM_MoreStackWalk,JNICALL,false,jint,JNIEnv *env, jobject arg1, jlong arg2, jlong arg3, jint arg4, jint arg5, jobjectArray arg6, jobjectArray arg7)])
_IF([JAVA_SPEC_VERSION >= 22],
	[_X(JVM_MoreStackWalk,JNICALL,false,jint,JNIEnv *env, jobject arg1, jint arg2, jlong arg3, jint arg4, jint arg5, jint arg6, jobjectArray arg7, jobjectArray arg8)])
_IF([JAVA_SPEC_VERSION >= 11],
	[_X(JVM_GetVmArguments,JNICALL,false,jobjectArray,JNIEnv *env)])
_IF([JAVA_SPEC_VERSION >= 11],
	[_X(JVM_FillStackFrames,JNICALL,false,void,JNIEnv *env, jclass arg1, jint arg2, jobjectArray arg3, jint arg4, jint arg5)])
_IF([JAVA_SPEC_VERSION >= 11],
	[_X(JVM_FindClassFromCaller,JNICALL,false,jclass,JNIEnv *env, const char *arg1, jboolean arg2, jobject arg3, jclass arg4)])
_IF([(11 <= JAVA_SPEC_VERSION) && (JAVA_SPEC_VERSION < 22)],
	[_X(JVM_CallStackWalk,JNICALL,false,jobject,JNIEnv *env, jobject arg1, jlong arg2, jint arg3, jint arg4, jint arg5, jobjectArray arg6, jobjectArray arg7)])
_IF([JAVA_SPEC_VERSION >= 22],
	[_X(JVM_CallStackWalk,JNICALL,false,jobject,JNIEnv *env, jobject arg1, jint arg2, jint arg3, jint arg4, jint arg5, jobjectArray arg6, jobjectArray arg7)])
_IF([JAVA_SPEC_VERSION >= 11],
	[_X(JVM_SetBootLoaderUnnamedModule,JNICALL,false,void,JNIEnv *env, jobject arg1)])
_IF([JAVA_SPEC_VERSION >= 11],
	[_X(JVM_ToStackTraceElement,JNICALL,false,void,JNIEnv *env, jobject arg1, jobject arg2)])
_IF([JAVA_SPEC_VERSION >= 11],
	[_X(JVM_GetStackTraceElements,JNICALL,false,void,JNIEnv *env, jobject arg1, jobjectArray arg2)])
_IF([JAVA_SPEC_VERSION >= 11],
	[_X(JVM_InitStackTraceElementArray,JNICALL,false,void,JNIEnv *env, jobjectArray arg1, jobject arg2)])
_IF([JAVA_SPEC_VERSION >= 11],
	[_X(JVM_InitStackTraceElement,JNICALL,false,void,JNIEnv *env, jobject arg1, jobject arg2)])
_IF([JAVA_SPEC_VERSION >= 11],
	[_X(JVM_GetAndClearReferencePendingList,JNICALL,false,jobject,JNIEnv *env)])
_IF([JAVA_SPEC_VERSION >= 11],
	[_X(JVM_HasReferencePendingList,JNICALL,false,jboolean,JNIEnv *env)])
_IF([JAVA_SPEC_VERSION >= 11],
	[_X(JVM_WaitForReferencePendingList,JNICALL,false,void,JNIEnv *env)])
_X(JVM_BeforeHalt,JNICALL,false,void,void)
_IF([JAVA_SPEC_VERSION >= 9],
	[_X(JVM_GetNanoTimeAdjustment,JNICALL,true,jlong,JNIEnv *env, jclass clazz, jlong offsetSeconds)])
_IF([JAVA_SPEC_VERSION >= 11],
	[_X(JVM_GetNestHost,JNICALL,false,jclass,JNIEnv *env,jclass clz)])
_IF([JAVA_SPEC_VERSION >= 11],
	[_X(JVM_GetNestMembers,JNICALL,false,jobjectArray,JNIEnv *env,jclass clz)])
_IF([JAVA_SPEC_VERSION >= 11],
	[_X(JVM_AreNestMates,JNICALL,false,jboolean,JNIEnv *env,jclass clzOne, jclass clzTwo)])
_IF([JAVA_SPEC_VERSION >= 11],
	[_X(JVM_InitializeFromArchive, JNICALL, false, void, JNIEnv *env, jclass clz)])
_IF([JAVA_SPEC_VERSION >= 14],
	[_X(JVM_GetExtendedNPEMessage, JNICALL, false, jstring, JNIEnv *env, jthrowable throwableObj)])
_IF([JAVA_SPEC_VERSION >= 16],
	[_X(JVM_GetRandomSeedForDumping, JNICALL, false, jlong)])
_IF([JAVA_SPEC_VERSION >= 15],
	[_X(JVM_RegisterLambdaProxyClassForArchiving, JNICALL, false, void, JNIEnv *env, jclass arg1, jstring arg2, jobject arg3, jobject arg4, jobject arg5, jobject arg6, jclass arg7)])
_IF([JAVA_SPEC_VERSION >= 16],
	[_X(JVM_LookupLambdaProxyClassFromArchive, JNICALL, false, jclass, JNIEnv *env, jclass arg1, jstring arg2, jobject arg3, jobject arg4, jobject arg5, jobject arg6)])
_IF([(15 <= JAVA_SPEC_VERSION) && (JAVA_SPEC_VERSION < 23)],
	[_X(JVM_IsCDSDumpingEnabled, JNICALL, false, jboolean, JNIEnv *env)])
_IF([(16 <= JAVA_SPEC_VERSION) && (JAVA_SPEC_VERSION < 23)],
	[_X(JVM_IsDumpingClassList, JNICALL, false, jboolean, JNIEnv *env)])
_IF([(16 <= JAVA_SPEC_VERSION) && (JAVA_SPEC_VERSION < 23)],
	[_X(JVM_IsSharingEnabled, JNICALL, false, jboolean, JNIEnv *env)])
_IF([JAVA_SPEC_VERSION >= 16],
	[_X(JVM_DefineArchivedModules, JNICALL, false, void, JNIEnv *env, jobject obj1, jobject obj2)])
_IF([JAVA_SPEC_VERSION >= 16],
	[_X(JVM_LogLambdaFormInvoker, JNICALL, false, void, JNIEnv *env, jstring str)])
_X(JVM_IsUseContainerSupport, JNICALL, false, jboolean, void)
_X(AsyncGetCallTrace, JNICALL, false, void, void *trace, jint depth, void *ucontext)
_IF([JAVA_SPEC_VERSION >= 17],
	[_X(JVM_DumpClassListToFile, JNICALL, false, void, JNIEnv *env, jstring str)])
_IF([JAVA_SPEC_VERSION >= 17],
	[_X(JVM_DumpDynamicArchive, JNICALL, false, void, JNIEnv *env, jstring str)])
_IF([JAVA_SPEC_VERSION >= 17],
	[_X(JVM_GetProperties, JNICALL, false, jobjectArray, JNIEnv *env)])
_IF([JAVA_SPEC_VERSION >= 18],
	[_X(JVM_IsFinalizationEnabled, JNICALL, false, jboolean, JNIEnv *env)])
_IF([JAVA_SPEC_VERSION >= 18],
	[_X(JVM_ReportFinalizationComplete, JNICALL, false, void, JNIEnv *env, jobject obj)])
_IF([JAVA_SPEC_VERSION >= 19],
	[_X(JVM_LoadZipLibrary, JNICALL, false, void *, void)])
_IF([JAVA_SPEC_VERSION >= 19],
	[_X(JVM_RegisterContinuationMethods, JNICALL, false, void, JNIEnv *env, jclass clz)])
_IF([JAVA_SPEC_VERSION >= 19],
	[_X(JVM_IsContinuationsSupported, JNICALL, false, void, void)])
_IF([JAVA_SPEC_VERSION >= 19],
	[_X(JVM_IsPreviewEnabled, JNICALL, false, void, void)])
_IF([JAVA_SPEC_VERSION >= 20],
	[_X(JVM_GetClassFileVersion, JNICALL, false, jint, JNIEnv *env, jclass cls)])
_IF([(20 <= JAVA_SPEC_VERSION) && (JAVA_SPEC_VERSION < 23)],
	[_X(JVM_VirtualThreadHideFrames, JNICALL, false, void, JNIEnv *env, jobject vthread, jboolean hide)])
_IF([(JAVA_SPEC_VERSION == 23) || defined(J9VM_OPT_VALHALLA_VALUE_TYPES)],
	[_X(JVM_VirtualThreadHideFrames, JNICALL, false, void, JNIEnv *env, jclass clz, jboolean hide)])
_IF([JAVA_SPEC_VERSION >= 21],
	[_X(JVM_IsForeignLinkerSupported, JNICALL, false, jboolean, void)])
_IF([JAVA_SPEC_VERSION >= 21],
	[_X(JVM_PrintWarningAtDynamicAgentLoad, JNICALL, false, jboolean, void)])
_IF([JAVA_SPEC_VERSION >= 21],
	[_X(JVM_VirtualThreadEnd, JNICALL, false, void, JNIEnv *env, jobject vthread)])
_IF([JAVA_SPEC_VERSION >= 21],
	[_X(JVM_VirtualThreadMount, JNICALL, false, void, JNIEnv *env, jobject vthread, jboolean hide)])
_IF([JAVA_SPEC_VERSION >= 21],
	[_X(JVM_VirtualThreadStart, JNICALL, false, void, JNIEnv *env, jobject vthread)])
_IF([JAVA_SPEC_VERSION >= 21],
	[_X(JVM_VirtualThreadUnmount, JNICALL, false, void, JNIEnv *env, jobject vthread, jboolean hide)])
_IF([defined(J9VM_OPT_VALHALLA_VALUE_TYPES)],
	[_X(JVM_CopyOfSpecialArray, JNICALL, false, jarray, JNIEnv *env, jarray orig, jint from, jint to)])
_IF([defined(J9VM_OPT_VALHALLA_VALUE_TYPES)],
	[_X(JVM_IsAtomicArray, JNICALL, false, jboolean, JNIEnv *env, jobject obj)])
_IF([defined(J9VM_OPT_VALHALLA_VALUE_TYPES)],
	[_X(JVM_IsFlatArray, JNICALL, false, jboolean, JNIEnv *env, jclass cls)])
_IF([defined(J9VM_OPT_VALHALLA_VALUE_TYPES)],
	[_X(JVM_IsNullRestrictedArray, JNICALL, false, jboolean, JNIEnv *env, jobject obj)])
_IF([defined(J9VM_OPT_VALHALLA_VALUE_TYPES)],
	[_X(JVM_IsValhallaEnabled, JNICALL, false, jboolean, void)])
_IF([defined(J9VM_OPT_VALHALLA_VALUE_TYPES)],
	[_X(JVM_NewNullableAtomicArray, JNICALL, false, jarray, JNIEnv *env, jclass cls, jint length)])
_IF([defined(J9VM_OPT_VALHALLA_VALUE_TYPES)],
	[_X(JVM_NewNullRestrictedAtomicArray, JNICALL, false, jarray, JNIEnv *env, jclass cls, jint length, jobject initialValue)])
_IF([defined(J9VM_OPT_VALHALLA_VALUE_TYPES)],
	[_X(JVM_NewNullRestrictedNonAtomicArray, JNICALL, false, jarray, JNIEnv *env, jclass elmClass, jint len, jobject initialValue)])
_IF([JAVA_SPEC_VERSION >= 22],
	[_X(JVM_ExpandStackFrameInfo, JNICALL, false, void, JNIEnv *env, jobject object)])
_IF([JAVA_SPEC_VERSION == 22],
	[_X(JVM_VirtualThreadDisableSuspend, JNICALL, false, void, JNIEnv *env, jobject vthread, jboolean enter)])
_IF([JAVA_SPEC_VERSION >= 23],
	[_X(JVM_VirtualThreadDisableSuspend, JNICALL, false, void, JNIEnv *env, jclass clz, jboolean enter)])
_IF([JAVA_SPEC_VERSION >= 23],
	[_X(JVM_GetCDSConfigStatus, JNICALL, false, jint, void)])
_IF([JAVA_SPEC_VERSION >= 21],
	[_X(JVM_IsContainerized, JNICALL, false, jboolean, void)])
_IF([JAVA_SPEC_VERSION >= 24],
	[_X(JVM_IsStaticallyLinked, JNICALL, false, jboolean, void)])
_IF([JAVA_SPEC_VERSION >= 24],
	[_X(JVM_VirtualThreadPinnedEvent, JNICALL, false, void, JNIEnv* env, jclass clazz, jstring op)])
_IF([JAVA_SPEC_VERSION >= 24],
	[_X(JVM_TakeVirtualThreadListToUnblock, JNICALL, false, jobject, JNIEnv* env, jclass ignored)])
_IF([JAVA_SPEC_VERSION >= 25],
	[_X(JVM_CreateThreadSnapshot, JNICALL, false, jobject, JNIEnv *env, jobject thread)])
_IF([JAVA_SPEC_VERSION >= 25],
	[_X(JVM_NeedsClassInitBarrierForCDS, JNICALL, false, jboolean, JNIEnv *env, jclass clazz)])
