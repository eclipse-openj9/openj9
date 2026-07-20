/*******************************************************************************
 * Copyright IBM Corp. and others 2026
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "ctest.h"
#include "cassume_api.h"

void
verifyJ9JavaVMAlignment(void)
{
	PORT_ACCESS_FROM_PORT(cTestPortLib);
	j9tty_printf(PORTLIB, "Verifying J9JavaVM field alignment\n");

	j9_assume(offsetof(J9JavaVM, internalVMFunctions) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, javaVM) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, reserved1_identifier) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, reserved2_library) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, portLibrary) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, j2seVersion) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, zipCachePool) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, vmInterface) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, dynamicLoadClassAllocationIncrement) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, dynamicLoadBuffers) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jimageIntf) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, vmArgsArray) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, dllLoadTable) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, j2seRootDirectory) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, j9libvmDirectory) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, systemProperties) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, systemPropertiesMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, javaHome) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, cInterpreter) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, bytecodeLoop) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, cudaGlobals) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, sharedClassConfig) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, bootstrapClassPath) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, runtimeFlags) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, extendedRuntimeFlags) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, extendedRuntimeFlags2) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, extendedRuntimeFlags3) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, zeroOptions) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, hotFieldClassInfoPool) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, hotFieldClassInfoPoolMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, globalHotFieldPoolMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, systemClassLoader) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, sigFlags) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, vmLocalStorageFunctions) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, unsafeMemoryTrackingMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, unsafeMemoryListHead) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, pathSeparator) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, linkPrevious) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, linkNext) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, internalVMLabels) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, memoryManagerFunctions) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, memorySegments) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, objectMemorySegments) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, classMemorySegments) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, stackSize) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, ramClassAllocationIncrement) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, ramClassSub4GAllocationIncrement) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, romClassAllocationIncrement) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, memoryMax) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, directByteBufferMemoryMax) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, exclusiveAccessMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jniGlobalReferences) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jniWeakGlobalReferences) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, vmThreadListMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, selectorHashTable) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, classLoaderBlocks) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, voidReflectClass) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, booleanReflectClass) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, charReflectClass) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, floatReflectClass) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, doubleReflectClass) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, byteReflectClass) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, shortReflectClass) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, intReflectClass) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, longReflectClass) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, booleanArrayClass) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, charArrayClass) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, floatArrayClass) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, doubleArrayClass) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, byteArrayClass) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, shortArrayClass) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, intArrayClass) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, longArrayClass) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jclConstantPool) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, mainThread) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, deadThreadList) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, exclusiveAccessState) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, classTableMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, anonClassCount) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, totalThreadCount) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, daemonThreadCount) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, accumulatedThreadCount) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, peakThreadCount) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, finalizeMainThread) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, finalizeMainMonitor) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, processReferenceMonitor) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, processReferenceActive) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, finalizeMainFlags) % sizeof(IDATA), 0);
	j9_assume(offsetof(J9JavaVM, exclusiveAccessResponseCount) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, destroyVMState) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, segmentMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jniFrameMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, verboseLevel) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, finalizeFlags) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, rsOverflow) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, maxStackUse) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, maxCStackUse) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, extensionClassLoader) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, applicationClassLoader) % sizeof(void*), 0);
#if JAVA_SPEC_VERSION < 24
	j9_assume(offsetof(J9JavaVM, doPrivilegedMethodID1) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, doPrivilegedMethodID2) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, doPrivilegedWithContextMethodID1) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, doPrivilegedWithContextMethodID2) % sizeof(UDATA), 0);
#endif /* JAVA_SPEC_VERSION < 24 */
	j9_assume(offsetof(J9JavaVM, defaultMemorySpace) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, systemThreadGroupRef) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, classLoaderBlocksMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, methodHandleCompileCount) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, jitConfig) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, tenureAge) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, classLoadingStackPool) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, finalizeRunFinalizationMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, finalizeRunFinalizationCount) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, vmLocalStorage) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, exitHook) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, abortHook) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, finalizeForceClassLoaderUnloadCount) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, classLoaderAllocationCount) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, scvTenureRatioHigh) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, scvTenureRatioLow) % sizeof(void*), 0);
#if defined(J9VM_ENV_SHARED_LIBS_USE_GLOBAL_TABLE) || defined(J9VM_ENV_CALL_VIA_TABLE)
	j9_assume(offsetof(J9JavaVM, jitTOC) % sizeof(UDATA), 0);
#endif /* defined(J9VM_ENV_SHARED_LIBS_USE_GLOBAL_TABLE) || defined(J9VM_ENV_CALL_VIA_TABLE) */
	j9_assume(offsetof(J9JavaVM, stackWalkCount) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, aotRuntimeInitMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, aotFindAndInitializeMethodEntryPoint) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, aotInitializeJxeEntryPoint) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, freeAotRuntimeInfo) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, threadDllHandle) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, arrayROMClasses) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, bytecodeVerificationData) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, defaultOSStackSize) % sizeof(UDATA), 0);
#if defined(J9VM_ENV_SHARED_LIBS_USE_GLOBAL_TABLE) || defined(J9VM_ENV_CALL_VIA_TABLE)
	j9_assume(offsetof(J9JavaVM, magicLinkageValue) % sizeof(UDATA), 0);
#endif /* defined(J9VM_ENV_SHARED_LIBS_USE_GLOBAL_TABLE) || defined(J9VM_ENV_CALL_VIA_TABLE) */
	j9_assume(offsetof(J9JavaVM, hookVMEvent) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jniFunctionTable) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jniSendTarget) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, thrMaxSpins1BeforeBlocking) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, thrMaxSpins2BeforeBlocking) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, thrMaxYieldsBeforeBlocking) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, thrMaxTryEnterSpins1BeforeBlocking) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, thrMaxTryEnterSpins2BeforeBlocking) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, thrMaxTryEnterYieldsBeforeBlocking) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, thrNestedSpinning) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, thrTryEnterNestedSpinning) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, thrDeflationPolicy) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, gcOptions) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, unhookVMEvent) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, classLoadingMaxStack) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, callInReturnPC) % sizeof(void*), 0);
#if defined(J9VM_OPT_METHOD_HANDLE)
	j9_assume(offsetof(J9JavaVM, impdep1PC) % sizeof(void*), 0);
#endif /* defined(J9VM_OPT_METHOD_HANDLE) */
	j9_assume(offsetof(J9JavaVM, jitExceptionHandlerSearch) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, walkStackFrames) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, walkFrame) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jitWalkStackFrames) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jitGetOwnedObjectMonitors) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jniArrayCacheMaxSize) % sizeof(UDATA), 0);
#if defined(J9VM_ENV_SHARED_LIBS_USE_GLOBAL_TABLE) || defined(J9VM_ENV_CALL_VIA_TABLE)
	j9_assume(offsetof(J9JavaVM, jclTOC) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, hookTOC) % sizeof(UDATA), 0);
#endif /* defined(J9VM_ENV_SHARED_LIBS_USE_GLOBAL_TABLE) || defined(J9VM_ENV_CALL_VIA_TABLE) */
	j9_assume(offsetof(J9JavaVM, initialStackSize) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, verboseStruct) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, codertOldAboutToBootstrap) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, codertOldVMShutdown) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jitOldAboutToBootstrap) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jitOldVMShutdown) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, gcExtensions) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, gcAllocationType) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, gcWriteBarrierType) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, gcReadBarrierType) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, gcPolicy) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, J9SigQuitShutdown) % sizeof(void*), 0);
#if defined(J9VM_INTERP_SIG_USR2)
	j9_assume(offsetof(J9JavaVM, J9SigUsr2Shutdown) % sizeof(void*), 0);
#endif /* defined(J9VM_INTERP_SIG_USR2) */
	j9_assume(offsetof(J9JavaVM, globalEventFlags) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, sidecarInterruptFunction) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, reflectFunctions) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, bindNativeMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, sidecarExitHook) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, sidecarExitFunctions) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, monitorTables) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, monitorTableCount) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, monitorTableMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, monitorTableList) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, monitorTableListPool) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, thrStaggerStep) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, thrStaggerMax) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, thrStagger) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, EsJNIFunctions) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, j9ras) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, j9rasDumpFunctions) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, floatJITExitInterpreter) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, doubleJITExitInterpreter) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, sigquitToFileDir) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, initializeSlotsOnTLHAllocate) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, stackWalkVerboseLevel) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, whackedPointerCounter) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, j9rasGlobalStorage) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jclFlags) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, jclSysPropBuffer) % sizeof(void*), 0);
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	j9_assume(offsetof(J9JavaVM, javaOffloadSwitchOnFunc) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, javaOffloadSwitchOffFunc) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, javaOffloadSwitchOnWithMethodFunc) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, javaOffloadSwitchOffWithMethodFunc) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, javaOffloadSwitchJDBCWithMethodFunc) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, javaOffloadSwitchOnAllowSubtasksWithMethodFunc) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, javaOffloadSwitchOnWithReasonFunc) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, javaOffloadSwitchOnAllowSubtasksWithReasonFunc) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, javaOffloadSwitchOnDisableSubtasksWithReasonFunc) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, javaOffloadSwitchOffWithReasonFunc) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, javaOffloadSwitchOnNoEnvWithReasonFunc) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, javaOffloadSwitchOffNoEnvWithReasonFunc) % sizeof(void*), 0);
#endif /* defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT) */
	j9_assume(offsetof(J9JavaVM, maxInvariantLocalTableNodeCount) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, jvmExtensionInterface) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, srMethodAccessor) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, srConstructorAccessor) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jlrMethodInvoke) % sizeof(void*), 0);
#if JAVA_SPEC_VERSION >= 18
	j9_assume(offsetof(J9JavaVM, jlrMethodInvokeMH) % sizeof(void*), 0);
#endif /* JAVA_SPEC_VERSION >= 18 */
	j9_assume(offsetof(J9JavaVM, jliMethodHandleInvokeWithArgs) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jliMethodHandleInvokeWithArgsList) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jliArgumentHelper) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, verboseStackDump) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, java_nio_Buffer) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, sun_nio_ch_DirectBuffer) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, java_nio_Buffer_address) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, java_nio_Buffer_capacity) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jvmtiData) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, hookInterface) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, requiredDebugAttributes) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, stackSizeIncrement) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, managementData) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, setVerboseState) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, verboseStateMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, sharedClassPreinitConfig) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, runtimeFlagsMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, extendedMethodFlagsMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, java_nio_DirectByteBuffer) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, java_nio_DirectByteBuffer_init) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, heapBase) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, heapTop) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, asyncEventMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, asyncEventHandlers) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, classLoadingConstraints) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, vTableScratch) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, vTableScratchSize) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, classUnloadMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, isClassUnloadMutexHeldForRedefinition) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, java2J9ThreadPriorityMap) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, j9Thread2JavaPriorityMap) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, priorityAsyncEventDispatch) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, priorityAsyncEventDispatchNH) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, priorityTimerDispatch) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, priorityTimerDispatchNH) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, priorityMetronomeAlarm) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, priorityMetronomeTrace) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, priorityJitSample) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, priorityJitCompile) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, priorityPosixSignalDispatch) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, priorityPosixSignalDispatchNH) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, priorityRealtimePriorityShift) % sizeof(IDATA), 0);
	j9_assume(offsetof(J9JavaVM, checkJNIData) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, j9rasdumpGlobalStorage) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, contendedLoadTable) % sizeof(void*), 0);
#if defined(OMR_GC_COMPRESSED_POINTERS)
	j9_assume(offsetof(J9JavaVM, compressedPointersShift) % sizeof(UDATA), 0);
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
#if defined(OMR_GC_CONCURRENT_SCAVENGER) && defined(J9VM_ARCH_S390)
	j9_assume(offsetof(J9JavaVM, invokeJ9ReadBarrier) % sizeof(void*), 0);
#endif /* defined(OMR_GC_CONCURRENT_SCAVENGER) && defined(J9VM_ARCH_S390) */
	j9_assume(offsetof(J9JavaVM, objectAlignmentInBytes) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, objectAlignmentShift) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, initialMethods) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, fieldIndexTable) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, fieldIndexThreshold) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, fieldIndexMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, localMapFunction) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, realtimeHeapMapBasePageRounded) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, realtimeHeapMapBits) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, realtimeSizeClasses) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, nativeMethodBindTable) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, mapMemoryBuffer) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, mapMemoryResultsBuffer) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, mapMemoryBufferSize) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, mapMemoryBufferMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jclCacheMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, arrayletLeafSize) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, arrayletLeafLogSize) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, contiguousIndexableHeaderSize) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, discontiguousIndexableHeaderSize) % sizeof(UDATA), 0);
#if defined(J9VM_ENV_DATA64)
	j9_assume(offsetof(J9JavaVM, isIndexableDataAddrPresent) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, indexableObjectLayout) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, isIndexableDualHeaderShapeEnabled) % sizeof(U_32), 0);
#endif /* defined(J9VM_ENV_DATA64) */
	j9_assume(offsetof(J9JavaVM, exclusiveVMAccessQueueHead) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, exclusiveVMAccessQueueTail) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, statisticsMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, nextStatistic) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, loadAgentLibraryOnAttach) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, isAgentLibraryLoaded) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, attachContext) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, hotSwapCount) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, zombieThreadCount) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, hiddenInstanceFields) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, hiddenInstanceFieldsMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, hiddenLockwordFieldShape) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, hiddenFinalizeLinkFieldShape) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, modulePointerOffset) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, jniCriticalLock) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jniCriticalResponseCount) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, sharedInvariantInternTable) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, sharedCacheAPI) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, lockwordMode) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, lockwordExceptions) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, sidecarClearInterruptFunction) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, phase) % sizeof(UDATA), 0);
#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)
	j9_assume(offsetof(J9JavaVM, leConditionConvertedToJavaException) % sizeof(UDATA), 0);
#endif /* defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT) */
	j9_assume(offsetof(J9JavaVM, originalSIGPIPESignalAction) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, finalizeWorkerData) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, heapOOMStringRef) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, strCompEnabled) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, identityHashData) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, minimumSuperclassArraySize) % sizeof(UDATA), 0);
#if defined(J9VM_JIT_RUNTIME_INSTRUMENTATION)
	j9_assume(offsetof(J9JavaVM, jitRIHandlerKey) % sizeof(IDATA), 0);
#endif /* defined(J9VM_JIT_RUNTIME_INSTRUMENTATION) */
	j9_assume(offsetof(J9JavaVM, osrGlobalBufferSize) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, osrGlobalBuffer) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, osrGlobalBufferLock) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, collectJitPrivateThreadData) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, alternateJitDir) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, debugField1) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, segregatedAllocationCacheSize) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, omrVM) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, omrRuntime) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, vmThreadSize) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, nativeLibraryMonitor) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, freePreviousClassLoaders) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, anonClassLoader) % sizeof(void*), 0);
#if JAVA_SPEC_VERSION < 24
	j9_assume(offsetof(J9JavaVM, doPrivilegedWithContextPermissionMethodID1) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, doPrivilegedWithContextPermissionMethodID2) % sizeof(UDATA), 0);
#endif /* JAVA_SPEC_VERSION < 24 */
	j9_assume(offsetof(J9JavaVM, nativeLibrariesLoadMethodID) % sizeof(UDATA), 0);
#if defined(J9VM_INTERP_CUSTOM_SPIN_OPTIONS)
	j9_assume(offsetof(J9JavaVM, customSpinOptions) % sizeof(void*), 0);
#endif /* defined(J9VM_INTERP_CUSTOM_SPIN_OPTIONS) */
	j9_assume(offsetof(J9JavaVM, romMethodSortThreshold) % sizeof(UDATA), 0);
#if defined(J9VM_THR_ASYNC_NAME_UPDATE)
	j9_assume(offsetof(J9JavaVM, threadNameHandlerKey) % sizeof(IDATA), 0);
#endif /* defined(J9VM_THR_ASYNC_NAME_UPDATE) */
	j9_assume(offsetof(J9JavaVM, decompileName) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, classLoaderModuleAndLocationMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, modularityPool) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, javaBaseModule) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, unnamedModuleForSystemLoader) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, modulesPathEntry) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jimModules) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, addReads) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, addExports) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, addOpens) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, addUses) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, addProvides) % sizeof(void*), 0);
#if JAVA_SPEC_VERSION >= 19
	j9_assume(offsetof(J9JavaVM, vThreadInterrupt) % sizeof(void*), 0);
#endif /* JAVA_SPEC_VERSION >= 19 */
	j9_assume(offsetof(J9JavaVM, addModulesCount) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, safePointState) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, safePointResponseCount) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, alreadyHaveExclusive) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, vmRuntimeStateListener) % sizeof(void*), 0);
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH)
#if defined(J9UNIX) || defined(AIXPPC)
	j9_assume(offsetof(J9JavaVM, exclusiveGuardPage) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, flushMutex) % sizeof(void*), 0);
#elif defined(WIN32)
	j9_assume(offsetof(J9JavaVM, flushFunction) % sizeof(void*), 0);
#endif /* defined(WIN32) */
#endif /* defined(J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH) */
	j9_assume(offsetof(J9JavaVM, constantDynamicMutex) % sizeof(void*), 0);
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
	j9_assume(offsetof(J9JavaVM, valueFlatteningThreshold) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, valueTypeVerificationMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, valueTypeVerificationStackPool) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, verificationMaxStack) % sizeof(UDATA), 0);
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
	j9_assume(offsetof(J9JavaVM, dCacheLineSize) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, cpuCacheWritebackCapabilities) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, enableGlobalLockReservation) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, reservedTransitionThreshold) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, reservedAbsoluteThreshold) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, minimumReservedRatio) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, cancelAbsoluteThreshold) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, minimumLearningRatio) % sizeof(U_32), 0);
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
	j9_assume(offsetof(J9JavaVM, vmindexOffset) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, vmtargetOffset) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, mutableCallSiteInvalidationCookieOffset) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, jitVMEntryKeepAliveOffset) % sizeof(UDATA), 0);
#endif /* defined(J9VM_OPT_OPENJDK_METHODHANDLE) */
#if defined(J9VM_ZOS_3164_INTEROPERABILITY)
	j9_assume(offsetof(J9JavaVM, javaVM31) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, javaVM31PadTo8) % sizeof(U_32), 0);
#endif /* defined(J9VM_ZOS_3164_INTEROPERABILITY) */
#if defined(J9VM_OPT_CRIU_SUPPORT)
	j9_assume(offsetof(J9JavaVM, checkpointState) % sizeof(void*), 0);
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
#if JAVA_SPEC_VERSION >= 16
	j9_assume(offsetof(J9JavaVM, cifNativeCalloutDataCache) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, cifNativeCalloutDataCacheMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, cifArgumentTypesCache) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, cifArgumentTypesCacheMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, thunkHeapHead) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, thunkHeapListMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, layoutStrFFITypeTable) % sizeof(void*), 0);
#endif /* JAVA_SPEC_VERSION >= 16 */
	j9_assume(offsetof(J9JavaVM, ensureHashedClasses) % sizeof(void*), 0);
#if JAVA_SPEC_VERSION >= 19
	j9_assume(offsetof(J9JavaVM, nextTID) % sizeof(U_64), 0);
	j9_assume(offsetof(J9JavaVM, virtualThreadInspectorCountOffset) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, internalSuspendStateOffset) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, tlsOffset) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, tlsFinalizers) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, tlsFinalizersMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, tlsPool) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, tlsPoolMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, vthreadGroup) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, continuationT2Cache) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, continuationT1Size) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, continuationT2Size) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, t1CacheHit) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, t2CacheHit) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, cacheMiss) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, t2store) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, cacheFree) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, totalContinuationStackSize) % sizeof(U_64), 0);
#if defined(J9VM_PROF_CONTINUATION_ALLOCATION)
	j9_assume(offsetof(J9JavaVM, avgCacheLookupTime) % sizeof(I_64), 0);
	j9_assume(offsetof(J9JavaVM, fastAlloc) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, slowAlloc) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, fastAllocAvgTime) % sizeof(I_64), 0);
	j9_assume(offsetof(J9JavaVM, slowAllocAvgTime) % sizeof(I_64), 0);
	j9_assume(offsetof(J9JavaVM, avgCacheFreeTime) % sizeof(I_64), 0);
#endif /* defined(J9VM_PROF_CONTINUATION_ALLOCATION) */
#endif /* JAVA_SPEC_VERSION >= 19 */
#if JAVA_SPEC_VERSION >= 22
	j9_assume(offsetof(J9JavaVM, closeScopeCountOffset) % sizeof(UDATA), 0);
#endif /* JAVA_SPEC_VERSION >= 22 */
#if defined(J9VM_OPT_CRIU_SUPPORT)
	j9_assume(offsetof(J9JavaVM, delayedLockingOperationsMutex) % sizeof(void*), 0);
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
	j9_assume(offsetof(J9JavaVM, compatibilityFlags) % sizeof(U_32), 0);
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
	j9_assume(offsetof(J9JavaVM, memberNameListsMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, memberNameListNodePool) % sizeof(void*), 0);
#endif /* defined(J9VM_OPT_OPENJDK_METHODHANDLE) */
#if defined(J9VM_OPT_JFR)
	j9_assume(offsetof(J9JavaVM, jfrState) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jfrBuffer) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jfrBufferMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jfrSamplerMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jfrSamplerThread) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, jfrSamplerState) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, jfrAsyncKey) % sizeof(IDATA), 0);
	j9_assume(offsetof(J9JavaVM, jfrThreadCPULoadAsyncKey) % sizeof(IDATA), 0);
	j9_assume(offsetof(J9JavaVM, isJFRExcludedOffset) % sizeof(UDATA), 0);
#if JAVA_SPEC_VERSION >= 17
	j9_assume(offsetof(J9JavaVM, jfrThreadAllocationStatisticsAsyncKey) % sizeof(IDATA), 0);
#endif /* JAVA_SPEC_VERSION >= 17 */
#endif /* defined(J9VM_OPT_JFR) */
	j9_assume(offsetof(J9JavaVM, unsafeIndexableHeaderSize) % sizeof(UDATA), 0);
#if defined(J9VM_OPT_SNAPSHOTS)
	j9_assume(offsetof(J9JavaVM, vmSnapshotImplPortLibrary) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, vmSnapshotFilePath) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, rcpCacheMutex) % sizeof(void*), 0);
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
#if defined(J9VM_OPT_JFR)
	j9_assume(offsetof(J9JavaVM, loadedClassCount) % sizeof(U_32), 0);
#endif /* defined(J9VM_OPT_JFR) */
#if JAVA_SPEC_VERSION >= 24
	j9_assume(offsetof(J9JavaVM, blockedContinuations) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, blockedVirtualThreadsMutex) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, pendingBlockedVirtualThreadsNotify) % sizeof(U_32), 0);
	j9_assume(offsetof(J9JavaVM, unblockerWaitTime) % sizeof(I_64), 0);
#endif /* JAVA_SPEC_VERSION >= 24 */
	j9_assume(offsetof(J9JavaVM, disclaimableRAMSegmentCount) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, disclaimableROMSegmentCount) % sizeof(UDATA), 0);
#if defined(OMR_THR_YIELD_ALG)
	j9_assume(offsetof(J9JavaVM, cpuUtilCalcThread) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, cpuUtilCacheInterval) % sizeof(UDATA), 0);
	j9_assume(offsetof(J9JavaVM, prevProcCPUTime) % sizeof(I_64), 0);
	j9_assume(offsetof(J9JavaVM, prevProcTimestamp) % sizeof(I_64), 0);
	j9_assume(offsetof(J9JavaVM, cpuUtilCacheMutex) % sizeof(void*), 0);
#endif /* defined(OMR_THR_YIELD_ALG) */
	j9_assume(offsetof(J9JavaVM, defaultPageSize) % sizeof(UDATA), 0);
#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
	j9_assume(offsetof(J9JavaVM, constRefArrayPool) % sizeof(void*), 0);
	j9_assume(offsetof(J9JavaVM, constRefsMutex) % sizeof(void*), 0);
#endif /* defined(J9VM_OPT_OPENJDK_METHODHANDLE) */
	j9_assume(offsetof(J9JavaVM, jitArtifactMonitor) % sizeof(void*), 0);
}
