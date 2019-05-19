/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#if !defined(VMHELPERS_HPP_)
#define VMHELPERS_HPP_

#include "j9.h"
#include "j9protos.h"
#include "j9consts.h"
#include "rommeth.h"
#include "j9vmconstantpool.h"
#include "j9modifiers_api.h"
#include "j9cp.h"
#include "ute.h"
#include "ObjectAllocationAPI.hpp"

typedef enum {
	J9_BCLOOP_SEND_TARGET_INITIAL_STATIC = 0,
	J9_BCLOOP_SEND_TARGET_INITIAL_SPECIAL,
	J9_BCLOOP_SEND_TARGET_INITIAL_VIRTUAL,
#if defined(J9VM_OPT_VALHALLA_NESTMATES)
	J9_BCLOOP_SEND_TARGET_INVOKE_PRIVATE,
#endif /* J9VM_OPT_VALHALLA_NESTMATES */
	J9_BCLOOP_SEND_TARGET_UNSATISFIED_OR_ABSTRACT,
	J9_BCLOOP_SEND_TARGET_DEFAULT_CONFLICT,
	J9_BCLOOP_SEND_TARGET_COUNT_NON_SYNC,
	J9_BCLOOP_SEND_TARGET_NON_SYNC,
	J9_BCLOOP_SEND_TARGET_COUNT_SYNC,
	J9_BCLOOP_SEND_TARGET_SYNC,
	J9_BCLOOP_SEND_TARGET_COUNT_SYNC_STATIC,
	J9_BCLOOP_SEND_TARGET_SYNC_STATIC,
	J9_BCLOOP_SEND_TARGET_COUNT_OBJ_CTOR,
	J9_BCLOOP_SEND_TARGET_OBJ_CTOR,
	J9_BCLOOP_SEND_TARGET_COUNT_LARGE,
	J9_BCLOOP_SEND_TARGET_LARGE,
	J9_BCLOOP_SEND_TARGET_COUNT_EMPTY_OBJ_CTOR,
	J9_BCLOOP_SEND_TARGET_EMPTY_OBJ_CTOR,
	J9_BCLOOP_SEND_TARGET_COUNT_ZEROING,
	J9_BCLOOP_SEND_TARGET_ZEROING,
	J9_BCLOOP_SEND_TARGET_BIND_NATIVE,
	J9_BCLOOP_SEND_TARGET_COUNT_AND_SEND_JNI_NATIVE,
	J9_BCLOOP_SEND_TARGET_COMPILE_JNI_NATIVE,
	J9_BCLOOP_SEND_TARGET_RUN_JNI_NATIVE,
	J9_BCLOOP_SEND_TARGET_I2J_TRANSITION,
	J9_BCLOOP_SEND_TARGET_INL_OBJECT_GET_CLASS,
	J9_BCLOOP_SEND_TARGET_INL_CLASS_IS_ASSIGNABLE_FROM,
	J9_BCLOOP_SEND_TARGET_INL_CLASS_IS_ARRAY,
	J9_BCLOOP_SEND_TARGET_INL_CLASS_IS_PRIMITIVE,
	J9_BCLOOP_SEND_TARGET_INL_CLASS_GET_MODIFIERS_IMPL,
	J9_BCLOOP_SEND_TARGET_INL_CLASS_GET_COMPONENT_TYPE,
	J9_BCLOOP_SEND_TARGET_INL_THREAD_CURRENT_THREAD,
	J9_BCLOOP_SEND_TARGET_INL_STRING_INTERN,
	J9_BCLOOP_SEND_TARGET_INL_SYSTEM_ARRAYCOPY,
	J9_BCLOOP_SEND_TARGET_INL_SYSTEM_CURRENT_TIME_MILLIS,
	J9_BCLOOP_SEND_TARGET_INL_SYSTEM_NANO_TIME,
	J9_BCLOOP_SEND_TARGET_INL_INTERNALS_GET_SUPERCLASS,
	J9_BCLOOP_SEND_TARGET_INL_INTERNALS_IDENTITY_HASH_CODE,
	J9_BCLOOP_SEND_TARGET_INL_THROWABLE_FILL_IN_STACK_TRACE,
	J9_BCLOOP_SEND_TARGET_INL_INTERNALS_NEWINSTANCEIMPL,
	J9_BCLOOP_SEND_TARGET_INL_INTERNALS_PRIMITIVE_CLONE,
	J9_BCLOOP_SEND_TARGET_INL_REFERENCE_GETIMPL,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETBYTE_NATIVE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETBYTE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETBYTE_VOLATILE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTBYTE_NATIVE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTBYTE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTBYTE_VOLATILE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETBOOLEAN,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETBOOLEAN_VOLATILE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTBOOLEAN,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTBOOLEAN_VOLATILE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETSHORT_NATIVE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETSHORT,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETSHORT_VOLATILE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTSHORT_NATIVE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTSHORT,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTSHORT_VOLATILE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETCHAR_NATIVE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETCHAR,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETCHAR_VOLATILE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTCHAR_NATIVE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTCHAR,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTCHAR_VOLATILE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETINT_NATIVE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETINT,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETINT_VOLATILE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTINT_NATIVE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTINT,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTINT_VOLATILE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETFLOAT_NATIVE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETFLOAT,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETFLOAT_VOLATILE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTFLOAT_NATIVE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTFLOAT,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTFLOAT_VOLATILE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETLONG_NATIVE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETLONG,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETLONG_VOLATILE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTLONG_NATIVE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTLONG,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTLONG_VOLATILE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETDOUBLE_NATIVE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETDOUBLE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETDOUBLE_VOLATILE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTDOUBLE_NATIVE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTDOUBLE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTDOUBLE_VOLATILE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETOBJECT,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETOBJECT_VOLATILE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTOBJECT,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTOBJECT_VOLATILE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_GETADDRESS,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_PUTADDRESS,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_ADDRESS_SIZE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_ARRAY_BASE_OFFSET,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_ARRAY_INDEX_SCALE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_LOAD_FENCE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_STORE_FENCE,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_COMPAREANDSWAPOBJECT,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_COMPAREANDSWAPLONG,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_COMPAREANDSWAPINT,
	J9_BCLOOP_SEND_TARGET_INL_INTERNALS_GET_INTERFACES,
	J9_BCLOOP_SEND_TARGET_INL_ARRAY_NEW_ARRAY_IMPL,
	J9_BCLOOP_SEND_TARGET_INL_CLASSLOADER_FIND_LOADED_CLASS_IMPL,
	J9_BCLOOP_SEND_TARGET_INL_VM_FIND_CLASS_OR_NULL,
	J9_BCLOOP_SEND_TARGET_CLASS_FORNAMEIMPL,
	J9_BCLOOP_SEND_TARGET_INL_THREAD_INTERRUPTED,
	J9_BCLOOP_SEND_TARGET_INL_VM_GET_CP_INDEX_IMPL,
	J9_BCLOOP_SEND_TARGET_INL_VM_GET_STACK_CLASS_LOADER,
	J9_BCLOOP_SEND_TARGET_INL_OBJECT_NOTIFY_ALL,
	J9_BCLOOP_SEND_TARGET_INL_OBJECT_NOTIFY,
	J9_BCLOOP_SEND_TARGET_INL_CLASS_IS_INSTANCE,
	J9_BCLOOP_SEND_TARGET_INL_CLASS_GET_SIMPLE_NAME_IMPL,
	J9_BCLOOP_SEND_TARGET_INL_VM_INITIALIZE_CLASS_LOADER,
	J9_BCLOOP_SEND_TARGET_INL_VM_GET_CLASS_PATH_ENTRY_TYPE,
	J9_BCLOOP_SEND_TARGET_INL_VM_IS_BOOTSTRAP_CLASS_LOADER,
	J9_BCLOOP_SEND_TARGET_INL_UNSAFE_ALLOCATE_INSTANCE,
	J9_BCLOOP_SEND_TARGET_INL_INTERNALS_PREPARE_CLASS_IMPL,
	J9_BCLOOP_SEND_TARGET_INL_ATTACHMENT_LOADAGENTLIBRARYIMPL,
	J9_BCLOOP_SEND_TARGET_INL_VM_GETSTACKCLASS,
	J9_BCLOOP_SEND_TARGET_INL_THREAD_SLEEP,
	J9_BCLOOP_SEND_TARGET_INL_OBJECT_WAIT,
	J9_BCLOOP_SEND_TARGET_INL_CLASSLOADER_LOADLIBRARYWITHPATH,
	J9_BCLOOP_SEND_TARGET_INL_THREAD_ISINTERRUPTEDIMPL,
	J9_BCLOOP_SEND_TARGET_INL_CLASSLOADER_INITIALIZEANONCLASSLOADER,
	J9_BCLOOP_SEND_TARGET_VARHANDLE,
	J9_BCLOOP_SEND_TARGET_INL_THREAD_ON_SPIN_WAIT,
	J9_BCLOOP_SEND_TARGET_OUT_OF_LINE_INL,
	J9_BCLOOP_SEND_TARGET_CLASS_ARRAY_TYPE_IMPL,
} VM_SendTarget;

typedef enum {
	VH_GET = 0,
	VH_SET,
	VH_GET_VOLATILE,
	VH_SET_VOLATILE,
	VH_GET_OPAQUE,
	VH_SET_OPAQUE,
	VH_GET_ACQUIRE,
	VH_SET_RELEASE,
	VH_COMPARE_AND_SET,
	VH_COMPARE_AND_EXCHANGE,
	VH_COMPARE_AND_EXCHANGE_ACQUIRE,
	VH_COMPARE_AND_EXCHANGE_RELEASE,
	VH_WEAK_COMPARE_AND_SET,
	VH_WEAK_COMPARE_AND_SET_ACQUIRE,
	VH_WEAK_COMPARE_AND_SET_RELEASE,
	VH_WEAK_COMPARE_AND_SET_PLAIN,
	VH_GET_AND_SET,
	VH_GET_AND_SET_ACQUIRE,
	VH_GET_AND_SET_RELEASE,
	VH_GET_AND_ADD,
	VH_GET_AND_ADD_ACQUIRE,
	VH_GET_AND_ADD_RELEASE,
	VH_GET_AND_BITWISE_AND,
	VH_GET_AND_BITWISE_AND_ACQUIRE,
	VH_GET_AND_BITWISE_AND_RELEASE,
	VH_GET_AND_BITWISE_OR,
	VH_GET_AND_BITWISE_OR_ACQUIRE,
	VH_GET_AND_BITWISE_OR_RELEASE,
	VH_GET_AND_BITWISE_XOR,
	VH_GET_AND_BITWISE_XOR_ACQUIRE,
	VH_GET_AND_BITWISE_XOR_RELEASE,
} VM_VARHANDLE_ACCESS_MODES;

	typedef enum {
		ffiSuccess,
		ffiFailed,
		ffiOOM
	} FFI_Return;

#define	J9_BCLOOP_SEND_TARGET_IS_C(mra) TRUE
#define J9_BCLOOP_DECODE_SEND_TARGET(mra) ((UDATA)(mra))
#define J9_BCLOOP_ENCODE_SEND_TARGET(num) ((void *)(num))
#define J9_VH_ENCODE_ACCESS_MODE(num) ((void *)((num << 1) + 1))
#define J9_VH_DECODE_ACCESS_MODE(mra) ((UDATA)(mra) >> 1)

class VM_VMHelpers
{
/*
 * Data members
 */
private:

protected:

public:

/*
 * Function members
 */
private:

protected:

public:

	/**
	 * Calculates the maximum stack size of a method when running bytecoded.
	 *
	 * @param[in] romMethod the J9ROMMethod for which to compute the size
	 * @param[in] stackFrameSize the size in bytes of the fixed stack frame, default to sizeof(J9SFStackFrame)
	 *
	 * @returns the maximum possible number of slots used by the method
	 */
	static VMINLINE UDATA
	calculateStackUse(J9ROMMethod *romMethod, UDATA stackFrameSize = sizeof(J9SFStackFrame))
	{
		UDATA stackUse = J9_MAX_STACK_FROM_ROM_METHOD(romMethod);
		stackUse += romMethod->tempCount;
		stackUse += (stackFrameSize / sizeof(UDATA));
		if (romMethod->modifiers & J9AccSynchronized) {
			stackUse += 1;
		} else if (J9ROMMETHOD_IS_NON_EMPTY_OBJECT_CONSTRUCTOR(romMethod)) {
			/* Non-empty java.lang.Object.<init> has one hidden temp to hold a copy of the receiver */
			stackUse += 1;
		}
		return stackUse;
	}

	/**
	 * Fetches the class depth of a J9Class.
	 *
	 * @param clazz[in] the class from which to fetch the depth
	 *
	 * @returns the class depth
	 */
	static VMINLINE UDATA
	getClassDepth(J9Class *clazz)
	{
		return J9CLASS_DEPTH(clazz);
	}

	/**
	 * Fetches the immediate superclass of a J9Class.
	 *
	 * @param clazz[in] the class from which to fetch the superclass
	 *
	 * @returns the superclass, or NULL if clazz is a hierarchy root
	 */
	static VMINLINE J9Class*
	getSuperclass(J9Class *clazz)
	{
		return clazz->superclasses[getClassDepth(clazz) - 1];
	}


	/**
	 * Checks if an instance field reference in the VM constant pool has been resolved.
	 *
	 * @param vm[in] the J9JavaVM
	 * @param cpIndex[in] the index into the constant pool
	 *
	 * @returns true if resolved, false if not
	 */
	static VMINLINE bool
	vmConstantPoolFieldIsResolved(J9JavaVM *vm, UDATA cpIndex)
	{
		return 0 != J9VMCONSTANTPOOL_FIELDREF_AT(vm, cpIndex)->flags;
	}

	/**
	 * Fetch the Interpreter-to-JIT compiled entry point from the methodhandle if one exists
	 *
	 * @param vm[in] the J9JavaVM
	 * @param currentThread[in] the current J9VMThread
	 * @param methodHandle[in] the MethodHandle object to check
	 *
	 * @returns the compiled i2j entrypoint or null.
	 */
	static VMINLINE void*
	methodHandleCompiledEntryPoint(J9JavaVM *vm, J9VMThread *currentThread, j9object_t methodHandle)
	{
		void *compiledEntryPoint = NULL;
		if (J9_EXTENDED_RUNTIME_I2J_MH_TRANSITION_ENABLED == (vm->extendedRuntimeFlags & J9_EXTENDED_RUNTIME_I2J_MH_TRANSITION_ENABLED)) {
			j9object_t thunks = J9VMJAVALANGINVOKEMETHODHANDLE_THUNKS(currentThread, methodHandle);
			I_64 i2jEntry = J9VMJAVALANGINVOKETHUNKTUPLE_I2JINVOKEEXACTTHUNK(currentThread, thunks);
			compiledEntryPoint = (void *)(UDATA)i2jEntry;
		}
		return compiledEntryPoint;
	}

	/**
	 * @brief Return whether an exception is pending or not
	 * 
	 * @param currentThread the J9VMThread to check
	 * @return true if an exception is pending, otherwise false.
	 */
	static VMINLINE bool
	exceptionPending(J9VMThread *const currentThread)
	{
		return NULL != currentThread->currentException;
	}

	/**
	 * Checks whether a class can be instantiated normally (i.e. via the new bytecode).
	 * Abstract, interface, base type and array classes cannot.
	 *
	 * @param j9clazz[in] the J9Class to query
	 *
	 * @returns true if instantiable, false if not
	 */
	static VMINLINE bool
	classCanBeInstantiated(J9Class *j9clazz)
	{
		J9ROMClass *romClass = j9clazz->romClass;
		return !J9ROMCLASS_IS_ABSTRACT(romClass) && !J9ROMCLASS_IS_INTERFACE(romClass) && !J9ROMCLASS_IS_PRIMITIVE_TYPE(romClass) && !J9ROMCLASS_IS_ARRAY(romClass);
	}

	/**
	 * Checks whether the class must be initialized before using it.
	 *
	 * Classes which have been successfully initialized or are in the process
	 * of being initialized by the current thread do not need to be initialized.
	 *
	 * @param currentThread[in] the current J9VMThread
	 * @param j9clazz[in] the J9Class to query
	 *
	 * @returns true if the class requires initialization, false if not
	 */
	static VMINLINE bool
	classRequiresInitialization(J9VMThread *currentThread, J9Class *j9clazz)
	{
		bool requiresInitialization = true;
		UDATA initStatus = j9clazz->initializeStatus;
		if ((J9ClassInitSucceeded == initStatus) || (((UDATA)currentThread) == initStatus)) {
			requiresInitialization = false;
		}
		return requiresInitialization;
	}

	/**
	 * Returns whether or not the out of line GC access barrier code must be called.
	 *
	 * @param vm[in] the J9JavaVM
	 *
	 * @returns true if the call must be made, false if an inline path may be taken
	 */
	static VMINLINE bool
	mustCallWriteAccessBarrier(J9JavaVM *vm)
	{
#if defined(J9VM_GC_ALWAYS_CALL_OBJECT_ACCESS_BARRIER)
		return true;
#else
		return J9_GC_WRITE_BARRIER_TYPE_ALWAYS == vm->gcWriteBarrierType;
#endif
	}

	/**
	 * Checks whether the given class was loaded by the bootstrap class loader.
	 *
	 * @param vm[in] the J9JavaVM
	 * @param j9clazz[in] the J9Class to query
	 *
	 * @returns true if the class was loaded by the bootstrap class loader, false if not
	 */
	static VMINLINE bool
	classIsBootstrap(J9JavaVM *vm, J9Class *j9clazz)
	{
		return j9clazz->classLoader == vm->systemClassLoader;
	}

	/**
	 * Returns the most current version of the given class (a newer version may exist due to HCR).
	 *
	 * @param j9clazz[in] the J9Class to query
	 *
	 * @returns the most current version of the class
	 */
	static VMINLINE J9Class*
	currentClass(J9Class *j9clazz)
	{
		if (J9_IS_CLASS_OBSOLETE(j9clazz)) {
			j9clazz = j9clazz->arrayClass;
		}
		return j9clazz;
	}

	/**
	 * Push an object, assuming there is a special frame on top of stack.
	 *
	 * @param currentThread[in] the current J9VMThread
	 * @param object[in] the object to preserve
	 */
	static VMINLINE void
	pushObjectInSpecialFrame(J9VMThread *currentThread, j9object_t object)
	{
		UDATA *sp = currentThread->sp - 1;
		currentThread->sp = sp;
		currentThread->literals = (J9Method*)((UDATA)currentThread->literals + sizeof(object));
		*(j9object_t*)sp = object;
	}

	/**
	 * Pop an object, assuming there is a special frame on top of stack.
	 *
	 * @param currentThread[in] the current J9VMThread
	 *
	 * @returns the object popped from the stack
	 */
	static VMINLINE j9object_t
	popObjectInSpecialFrame(J9VMThread *currentThread)
	{
		UDATA *sp = currentThread->sp;
		j9object_t object = *(j9object_t*)sp;
		currentThread->sp = sp + 1;
		currentThread->literals = (J9Method*)((UDATA)currentThread->literals - sizeof(object));
		return object;
	}

	/**
	 * Perform any special GC processing on a new object.
	 *
	 * @param currentThread[in] the current J9VMThread
	 */
	static VMINLINE void
	checkIfFinalizeObject(J9VMThread *currentThread, j9object_t object)
	{
		J9Class *objectClass = J9OBJECT_CLAZZ(currentThread, object);
		UDATA classFlags = J9CLASS_FLAGS(objectClass) & J9_JAVA_CLASS_FINALIZER_CHECK_MASK;
		if (0 != classFlags) {
			if (classFlags & J9AccClassFinalizeNeeded) {
				currentThread->javaVM->memoryManagerFunctions->finalizeObjectCreated(currentThread, object);
			}
			if (classFlags & J9AccClassOwnableSynchronizer) {
				currentThread->javaVM->memoryManagerFunctions->ownableSynchronizerObjectCreated(currentThread, object);
			}
		}
	}

	/**
	 * Determine if an object is an instance of an array class.
	 *
	 * @param currentThread[in] the current J9VMThread
	 * @param object[in] the object to query
	 *
	 * @returns true if the object is an array, false if ont
	 */
	static VMINLINE bool
	objectIsArray(J9VMThread *currentThread, j9object_t object)
	{
		J9Class *objectClass = J9OBJECT_CLAZZ(currentThread, object);
		return J9CLASS_IS_ARRAY(objectClass);
	}

	/**
	 * Determine if a class is identical to or a superclass of another.
	 *
	 * @param superClass[in] the superclass
	 * @param subClass[in] the subclass
	 *
	 * @returns true if superClass is identical to or a superclass of subClass, false if not
	 */
	static VMINLINE bool
	isSameOrSuperclass(J9Class *superClass, J9Class *subClass)
	{
		bool isSubclass = true;
		if (subClass != superClass) {
			UDATA superClassDepth = getClassDepth(superClass);
			UDATA subClassDepth = getClassDepth(subClass);
			if ((subClassDepth <= superClassDepth) || (subClass->superclasses[superClassDepth] != superClass)) {
				isSubclass = false;
			}
		}
		return isSubclass;
	}

	/**
	 * Determine if a class is castable to another.  If updateCache is true, the current thread
	 * must have VM access (or otherwise be blocking the GC) as writes back to classes which might
	 * be being unloaded could occur.
	 *
	 * @param instanceClass[in] the class being cast from
	 * @param castClass[in] the class being cast to
	 * @param updateCache[in] whether or not to write back to the cache (default true)
	 *
	 * @returns true if instanceClass may be cast to castClass, false if not
	 */
	static VMINLINE bool
	inlineCheckCast(J9Class *instanceClass, J9Class *castClass, bool updateCache = true)
	{
		J9Class *initialInstanceClass = instanceClass;
		J9Class *initialCastClass = castClass;
		bool didRetry = false;
retry:
		bool castable = true;
		/* start with the trivial case - do not cache successful equality check to avoid cache pollution */
		if (!isSameOrSuperclass(castClass, instanceClass)) {
			J9ITable *iTable = NULL;
			/* The cache is a single slot containing the last class to which the class was cast
			 * (low bit tagged for a failed cast, clear for a successful cast). The cache is only
			 * updated on non-trivial successful casts - casting a class to itself or a superclass
			 * is not cached because that case can be covered very quickly in the fast inline path.
			 */
			UDATA cachedValue = instanceClass->castClassCache;
			if (castClass == (J9Class *)(cachedValue & ~(UDATA)1)) {
				if (cachedValue & 1) {
					castable = false;
				}
				goto done;
			}
			if (J9ROMCLASS_IS_INTERFACE(castClass->romClass)) {
				/***** casting to an interface - do itable check */
				iTable = (J9ITable*)instanceClass->lastITable;
				if (iTable->interfaceClass == castClass) {
					goto cacheCastable;
				}
				iTable = (J9ITable*)instanceClass->iTable;
				while (NULL != iTable) {
					if (iTable->interfaceClass == castClass) {
						if (updateCache) {
							instanceClass->lastITable = iTable;
						}
cacheCastable:
						if (updateCache) {
							instanceClass->castClassCache = (UDATA)castClass;
						}
						goto done;
					}
					iTable = iTable->next;
				}
			} else if (J9CLASS_IS_ARRAY(castClass)) {
				/* the instanceClass must be an array to continue */
				if (J9CLASS_IS_ARRAY(instanceClass)) {
					/* castClass is an array - if base type, they must be the same primitive type */
					UDATA castArity = ((J9ArrayClass*)castClass)->arity;
					J9Class *castClassLeafComponent = ((J9ArrayClass*)castClass)->leafComponentType;
					if (J9CLASS_IS_MIXED(castClassLeafComponent)) {
						UDATA instanceArity = ((J9ArrayClass*)instanceClass)->arity;
						if (instanceArity > castArity) {
							J9Class *workingInstanceClass = instanceClass;
							/* strip out the extra layers of [ and find what is left */
							while (castArity > 0) {
								workingInstanceClass = ((J9ArrayClass*)workingInstanceClass)->componentType;
								castArity -= 1;
							}
							/* try again with the arities stripped down to minimum */
							instanceClass = workingInstanceClass;
							castClass = castClassLeafComponent;
							didRetry = true;
							goto retry;
						} else {
							/* check the [[O -> [[O case.  Don't allow [[I -> [[O */
							if (instanceArity == castArity) {
								J9Class *instanceClassLeafComponent = ((J9ArrayClass*)instanceClass)->leafComponentType;
								if (J9CLASS_IS_MIXED(instanceClassLeafComponent)) {
									/* we know arities are the same, so skip directly to the terminal case */
									instanceClass = instanceClassLeafComponent;
									castClass = castClassLeafComponent;
									didRetry = true;
									goto retry;
								}
							}
							/* else the arity of the instance wasn't high enough, so we fail */
						}
					}
					/* else we have a base type castClass and it isn't equal (from the trivial case at the top), so it can't be valid so we fail */
				}
			}
			/* fallthrough cases are all invalid casts that should be cached */
			if (updateCache) {
				instanceClass->castClassCache = (UDATA)castClass | 1;
			}
			castable = false;
		}
done:
		if (didRetry && updateCache) {
			if (castable) {
				initialInstanceClass->castClassCache = (UDATA)initialCastClass;
			} else {
				initialInstanceClass->castClassCache = (UDATA)initialCastClass | 1;
			}
		}
		return castable;
	}

	/**
	 * Set an exception pending in the current thread.
	 * Note this does not generate the "systhrow" dump event.
	 *
	 * @param currentThread[in] the current J9VMThread
	 * @param exception[in] the Throwable instance
	 */
	static VMINLINE void
	setExceptionPending(J9VMThread *currentThread, j9object_t exception)
	{
		currentThread->currentException = exception;
		currentThread->privateFlags |= J9_PRIVATE_FLAGS_REPORT_EXCEPTION_THROW;
	}

	/**
	 * Clear any pending exception in the current thread.
	 * Note this does not generate the "catch" dump event.
	 *
	 * @param currentThread[in] the current J9VMThread
	 */
	static VMINLINE void
	clearException(J9VMThread *currentThread)
	{
		currentThread->currentException = NULL;
		currentThread->privateFlags &= ~(UDATA)J9_PRIVATE_FLAGS_REPORT_EXCEPTION_THROW;
	}

	/**
	 * Checks whether a stack grow is required on the current thread
	 * for a given actual or potential stack pointer value.
	 *
	 * @param currentThread[in] the current J9VMThread
	 * @param checkSP[in] the sp value to check
	 *
	 * @returns true if a grow is required, false if not
	 */
	static VMINLINE bool
	shouldGrowForSP(J9VMThread *currentThread, UDATA *checkSP)
	{
		bool checkRequired = false;
		if (checkSP < currentThread->stackOverflowMark2) {
			checkRequired = true;
			if (J9_ARE_ANY_BITS_SET(currentThread->privateFlags, J9_PRIVATE_FLAGS_STACK_OVERFLOW)) {
				UDATA *absoluteLimit = (UDATA*)((UDATA)currentThread->stackObject + J9_STACK_OVERFLOW_RESERVED_SIZE);
				if (checkSP >= absoluteLimit) {
					checkRequired = false;
				}
			}
		}
		return checkRequired;
	}

	/**
	 * Checks whether an async message is pending on the current thread.
	 *
	 * @param currentThread[in] the current J9VMThread
	 *
	 * @returns true if an async is pending, false if not
	 */
	static VMINLINE bool
	asyncMessagePending(J9VMThread *currentThread)
	{
		return (UDATA*)J9_EVENT_SOM_VALUE == currentThread->stackOverflowMark;
	}

	/**
	 * Indicate that an async message is pending on the a thread.
	 *
	 * @param vmThread[in] the J9VMThread
	 */
	static VMINLINE void
	indicateAsyncMessagePending(J9VMThread *vmThread)
	{
		/* Ensure any flag updates are written before the SOM */
		VM_AtomicSupport::writeBarrier();
		vmThread->stackOverflowMark = (UDATA*)J9_EVENT_SOM_VALUE;
	}

	/**
	 * Checks for async messages on the current thread and
	 * returns on of the follow action codes:
	 *
	 * 		J9_CHECK_ASYNC_NO_ACTION
	 * 			- nothing needs to be done
	 * 		J9_CHECK_ASYNC_THROW_EXCEPTION
	 * 			- throw the current exception immediately
	 * 		J9_CHECK_ASYNC_POP_FRAMES
	 * 			- pop the current frame immediately
	 * 			- this cannot happen if either of the top two methods on the stack are native
	 *
	 * Do not call this function if an exception is already pending.
	 *
	 * @param currentThread[in] the current J9VMThread
	 *
	 * @returns the async action code
	 */
	static VMINLINE UDATA
	checkAsyncMessages(J9VMThread *currentThread)
	{
		UDATA action = J9_CHECK_ASYNC_NO_ACTION;
		if (asyncMessagePending(currentThread)) {
			action = J9_VM_FUNCTION(currentThread, javaCheckAsyncMessages)(currentThread, TRUE);
		}
		return action;
	}

	/**
	 * Determine if an immediate async is pending on the current thread.
	 *
	 * @param currentThread[in] the current J9VMThread
	 *
	 * @returns true if an immediate async is pending, false if not
	 */
	static VMINLINE bool
	immediateAsyncPending(J9VMThread *currentThread)
	{
		return (0 != (currentThread->publicFlags & J9_PUBLIC_FLAGS_POP_FRAMES_INTERRUPT));
	}

	/**
	 * Perform a non-instrumentable allocation of a non-indexable class.
	 * If inline allocation fails, the out of line allocator will be called.
	 * This function assumes that the stack and live values are in a valid state for GC.
	 *
	 * If allocation fails, a heap OutOfMemory error is set pending on the thread.
	 *
	 * @param currentThread[in] the current J9VMThread
	 * @param objectAllocate[in] instance of MM_ObjectAllocationAPI created on the current thread
	 * @param j9clazz[in] the non-indexable J9Class to instantiate
	 * @param initializeSlots[in] whether or not to initialize the slots (default true)
	 * @param memoryBarrier[in] whether or not to issue a write barrier (default true)
	 *
	 * @returns the new object, or NULL if allocation failed
	 */
	static VMINLINE j9object_t
	allocateObject(J9VMThread *currentThread, MM_ObjectAllocationAPI *objectAllocate, J9Class *clazz, bool initializeSlots = true, bool memoryBarrier = true)
	{
		j9object_t instance = objectAllocate->inlineAllocateObject(currentThread, clazz, initializeSlots, memoryBarrier);
		if (NULL == instance) {
			instance = currentThread->javaVM->memoryManagerFunctions->J9AllocateObject(currentThread, clazz, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
			if (J9_UNEXPECTED(NULL == instance)) {
				setHeapOutOfMemoryError(currentThread);
			}
		}
		return instance;
	}

	/**
	 * Perform a non-instrumentable allocation of an indexable class.
	 * If inline allocation fails, the out of line allocator will be called.
	 * This function assumes that the stack and live values are in a valid state for GC.
	 *
	 * If allocation fails, a heap OutOfMemory error is set pending on the thread.
	 *
	 * @param currentThread[in] the current J9VMThread
	 * @param objectAllocate[in] instance of MM_ObjectAllocationAPI created on the current thread
	 * @param arrayClass[in] the indexable J9Class to instantiate
	 * @param size[in] the desired size of the array
	 * @param initializeSlots[in] whether or not to initialize the slots (default true)
	 * @param memoryBarrier[in] whether or not to issue a write barrier (default true)
	 * @param sizeCheck[in] whether or not to perform the maximum size check (default true)
	 *
	 * @returns the new object, or NULL if allocation failed
	 */
	static VMINLINE j9object_t
	allocateIndexableObject(J9VMThread *currentThread, MM_ObjectAllocationAPI *objectAllocate, J9Class *arrayClass, U_32 size, bool initializeSlots = true, bool memoryBarrier = true, bool sizeCheck = true)
	{
		j9object_t instance = objectAllocate->inlineAllocateIndexableObject(currentThread, arrayClass, size, initializeSlots, memoryBarrier, sizeCheck);
		if (NULL == instance) {
			instance = currentThread->javaVM->memoryManagerFunctions->J9AllocateIndexableObject(currentThread, arrayClass, size, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
			if (J9_UNEXPECTED(NULL == instance)) {
				setHeapOutOfMemoryError(currentThread);
			}
		}
		return instance;
	}

	/**
	 * Determine if a J9Method has been compiled by the JIT.
	 *
	 * @param method[in] the J9Method to query
	 *
	 * @returns true if the method has been compiled, false if not
	 */
	static VMINLINE bool
	methodHasBeenCompiled(J9Method *method)
	{
		return J9_ARE_NO_BITS_SET((UDATA)method->extra, J9_STARTPC_NOT_TRANSLATED);
	}

	/**
	 * Determine the number of UTF8 bytes required to encode a unicode character.
	 *
	 * @param unicode[in] the unicode character
	 *
	 * @returns the number of bytes required to encode the character as UTF8
	 */
	static VMINLINE UDATA
	encodedUTF8Length(U_16 unicode)
	{
		UDATA length = 1;
		if ((unicode >= 0x01) && (unicode <= 0x7F)) {
			/* Written this way to match encodeUTF */
		} else if ((unicode == 0) || ((unicode >= 0x80) && (unicode <= 0x7FF))) {
			length = 2;
		} else {
			length = 3;
		}
		return length;
	}

	/**
	 * Determine the number of UTF8 bytes required to encode a unicode character.
	 *
	 * @param unicode[in] the unicode character
	 *
	 * @returns the number of bytes required to encode the character as UTF8
	 */
	static VMINLINE UDATA
	encodedUTF8LengthI8(I_8 unicode)
	{
		return (unicode >= 0x01) && (unicode <= 0x7F) ? 1 : 2;
	}

	/**
	 * Encode the Unicode character.
	 *
	 * Encodes the input Unicode character and stores it into utfChars.
	 *
	 * @param[in] unicode The unicode character
	 * @param[in,out] utfChars buffer for UTF8 character
	 *
	 * @return Size of encoding (1,2,3) on success, 0 on failure
	 */
	static VMINLINE UDATA
	encodeUTF8Char(U_16 unicode, U_8 *utfChars)
	{
		UDATA length = 1;
		if ((unicode >= 0x01) && (unicode <= 0x7F)) {
			utfChars[0] = (U_8)unicode;
		} else if ((unicode == 0) || ((unicode >= 0x80) && (unicode <= 0x7FF))) {
			utfChars[0] = (U_8)(((unicode >>6 ) & 0x1F) | 0xC0);
			utfChars[1] = (U_8)((unicode & 0x3F) | 0x80);
			length = 2;
		} else {
			utfChars[0] = (U_8)(((unicode >> 12) & 0x0F) | 0xE0);
			utfChars[1] = (U_8)(((unicode >> 6 ) & 0x3F) | 0x80);
			utfChars[2] = (U_8)((unicode & 0x3F) | 0x80);
			length = 3;
		}
		return length;
	}

	/**
	 * Encode the Unicode character.
	 *
	 * Encodes the input Unicode character and stores it into utfChars.
	 *
	 * @param[in] unicode The unicode character
	 * @param[in,out] utfChars buffer for UTF8 character
	 *
	 * @return Size of encoding (1,2,3) on success, 0 on failure
	 */
	static VMINLINE UDATA
	encodeUTF8CharI8(I_8 unicode, U_8 *utfChars)
	{
		UDATA length = 1;
		if ((unicode >= 0x01) && (unicode <= 0x7F)) {
			utfChars[0] = (U_8)unicode;
		} else {
			utfChars[0] = (U_8)(((unicode >>6 ) & 0x1F) | 0xC0);
			utfChars[1] = (U_8)((unicode & 0x3F) | 0x80);
			length = 2;
		}
		return length;
	}

	 /**
	 * Encode the Unicode character.
	 *
	 * Encodes the input Unicode character and stores it into utfChars.
	 *
	 * @param[in] unicode The unicode character
	 * @param[in,out] utfChars buffer for UTF8 character
	 * @param[in] bytesRemaining available space in result buffer
	 *
	 * @return Size of encoding (1,2,3) on success, 0 on failure
	 *
	 * @note Don't write more than bytesRemaining characters
	 */
	static VMINLINE UDATA
	encodeUTF8CharN(U_16 unicode, U_8 *utfChars, UDATA bytesRemaining)
	{
		if (unicode >= 0x01 && unicode <= 0x7f) {
			if (bytesRemaining < 1) {
				return 0;
			}
			*utfChars = (U_8)unicode;
			return 1;
		} else if (unicode == 0 || (unicode >= 0x80 && unicode <= 0x7ff)) {
			if (bytesRemaining < 2) {
				return 0;
			}
			*utfChars++ = (U_8)(((unicode >> 6) & 0x1f) | 0xc0);
			*utfChars = (U_8)((unicode & 0x3f) | 0x80);
			return 2;
		} else if (unicode >= 0x800) {
			if (bytesRemaining < 3) {
				return 0;
			}
			*utfChars++ = (U_8)(((unicode >> 12) & 0x0f) | 0xe0);
			*utfChars++ = (U_8)(((unicode >> 6) & 0x3f) | 0x80);
			*utfChars = (U_8)((unicode & 0x3f) | 0x80);
			return 3;
		} else {
			return 0;
		}
	}

	/**
	 * Decode a single unicode character from a possibly-invalid UTF8 byte stream.
	 * This function assumes that bytesRemaining is not zero.
	 *
	 * @param input[in] the UTF8 data
	 * @param result[out] the location into which to write the unicode character
	 * @param bytesRemaining[in] the number of bytes remaining in the UTF8 stream
	 *
	 * @returns the number of bytes consumed from the UTF8 stream, 0 if the UTF data is invalid
	 */
	static VMINLINE UDATA
	decodeUTF8CharN(const U_8 *input, U_16 *result, UDATA bytesRemaining)
	{
		U_8 c = input[0];
		U_16 unicode = (U_16)c;
		UDATA consumed = 0;
		/* 0 is not legal as a single-byte encoding */
		if (0 != c) {
			if (0 == (c & 0x80)) {
				/* one byte encoding */
				consumed = 1;
			} else if (0xC0 == (c & 0xE0)) {
				/* two byte encoding */
				if (bytesRemaining >= 2) {
					unicode = (((U_16)c & 0x1F) << 6);
					c = input[1];
					unicode += ((U_16)c & 0x3F);
					if (0x80 == (c & 0xC0)) {
						consumed = 2;
					}
				}
			} else if (0xE0 == (c & 0xF0)) {
				/* three byte encoding */
				if (bytesRemaining >= 3) {
					unicode = (((U_16)c & 0x0F) << 12);
					c = input[1];
					unicode += (((U_16)c & 0x3F) << 6);
					if (0x80 == (c & 0xC0)) {
						c = input[2];
						unicode += ((U_16)c & 0x3F);
						if (0x80 == (c & 0xC0)) {
							consumed = 3;
						}
					}
				}
			}
		}
		*result = unicode;
		return consumed;
	}

	/**
	 * Decode a single unicode character from a valid UTF8 byte stream.
	 *
	 * @param input[in] the UTF8 data
	 * @param result[out] the location into which to write the unicode character
	 *
	 * @returns the number of bytes consumed from the UTF8 stream
	 */
	static VMINLINE UDATA
	decodeUTF8Char(const U_8 *input, U_16 *result)
	{
		U_8 c = input[0];
		U_16 unicode = (U_16)c;
		UDATA consumed = 1;
		if (0 != (c & 0x80)) {
			if (0xC0 == (c & 0xE0)) {
				/* two byte encoding */
				unicode = (((U_16)c & 0x1F) << 6);
				c = input[1];
				unicode += ((U_16)c & 0x3F);
				consumed = 2;
			} else if (0xE0 == (c & 0xF0)) {
				/* three byte encoding */
				unicode = (((U_16)c & 0x0F) << 12);
				c = input[1];
				unicode += (((U_16)c & 0x3F) << 6);
				c = input[2];
				unicode += ((U_16)c & 0x3F);
				consumed = 3;
			}
		}
		*result = unicode;
		return consumed;
	}

	/**
	 * Determine the JIT to JIT start address by skipping over the interpreter
	 * pre-prologue at the interpreter to JIT start address.
	 *
	 * @param jitStartAddress[in] the interpreter to JIT start address
	 *
	 * @returns the JIT to JIT start address
	 */
	static VMINLINE void*
	jitToJitStartAddress(void *jitStartAddress)
	{
#if defined(J9SW_ARGUMENT_REGISTER_COUNT)
		jitStartAddress = (void*)((UDATA)jitStartAddress + (((U_32*)jitStartAddress)[-1] >> 16));
#endif /* J9SW_ARGUMENT_REGISTER_COUNT */
		return jitStartAddress;
	}

	/**
	 * Determine if a method is being traced via method trace.
	 *
	 * @param vm[in] the J9JavaVM
	 * @param method[in] the method
	 *
	 * @returns true if the method is being traced, false if not
	 */
	static VMINLINE bool
	methodBeingTraced(J9JavaVM *vm, J9Method *method)
	{
		bool rc = false;
		if (J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_METHOD_TRACE_ENABLED)) {
			U_8 *pmethodflags = fetchMethodExtendedFlagsPointer(method);
			if (J9_RAS_IS_METHOD_TRACED(*pmethodflags)) {
				rc = true;
			}
		}
		return rc;
	}

	/**
	 * Determine if a RAM instance field ref is resolved.
	 *
	 * @param flags[in] field from the ref
	 * @param valueOffset[in] field from the ref
	 *
	 * @returns true if resolved, false if not
	 */
	static VMINLINE bool
	instanceFieldRefIsResolved(UDATA flags, UDATA valueOffset)
	{
		/* In a resolved field, flags will have the J9FieldFlagResolved bit set, thus
		 * having a higher value than any valid valueOffset.
		 *
		 * This check avoids the need for a barrier, as it will only succeed if flags
		 * and valueOffset have both been updated. It is crucial that we do not treat
		 * a field ref as resolved if only one of the two values has been set (by
		 * another thread that is in the middle of a resolve).
		 */
		return (flags > valueOffset);
	}

	/**
	 * Determine if a RAM static field ref is resolved.
	 *
	 * @param flagsAndClass[in] field from the ref
	 * @param valueOffset[in] field from the ref
	 *
	 * @returns true if resolved, false if not
	 */
	static VMINLINE bool
	staticFieldRefIsResolved(IDATA flagsAndClass, UDATA valueOffset)
	{
		/* In an unresolved static fieldref, the valueOffset will be -1 or flagsAndClass will be <= 0.
		 * If the fieldref was resolved as an instance fieldref, the high bit of flagsAndClass will be
		 * set, so it will be < 0 and will be treated as an unresolved static fieldref.
		 *
		 * Since instruction re-ordering may result in us reading an updated valueOffset but
		 * a stale flagsAndClass, we check that both fields have been updated. It is crucial
		 * that we do not use a stale flagsAndClass with non-zero value, as doing so may cause the
		 * the StaticFieldRefDouble bit check to succeed when it shouldn't.
		 */
		return ((UDATA)-1 != valueOffset) && (flagsAndClass > 0);
	}

	/**
	 * Get the field address from a resolved RAM static field ref.
	 *
	 * @param flagsAndClass[in] field from the ref
	 * @param staticAddress[in] field from the ref
	 *
	 * @returns the field address
	 */
	static VMINLINE void*
	staticFieldAddressFromResolvedRef(IDATA flagsAndClass, UDATA staticAddress)
	{
		J9Class *clazz = (J9Class*)((UDATA)flagsAndClass << J9_REQUIRED_CLASS_SHIFT);
		staticAddress &= ~((UDATA)1 << ((8 * sizeof(UDATA)) - 1));
		staticAddress += (UDATA)clazz->ramStatics;
		return (void*)staticAddress;
	}

	/**
	 * Perform the zaap offload switch (if necessary) before making a JNI
	 * native call.  The JNI native method frame must be on stack, and
	 * currentThread must be up-to-date.
	 *
	 * @param[in] currentThread the current J9VMThread
	 */
	static VMINLINE void
	beforeJNICall(J9VMThread *currentThread)
	{
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
		J9JavaVM *vm = currentThread->javaVM;
		J9VMEntryLocalStorage *els = currentThread->entryLocalStorage;
		/* check if java offload mode is enabled */
		if (NULL != vm->javaOffloadSwitchOffWithMethodFunc) {
			/* save old state */
			els->savedJavaOffloadState = currentThread->javaOffloadState;
			/* check if we need to change state */
			J9SFJNINativeMethodFrame *nativeMethodFrame = (J9SFJNINativeMethodFrame*)((UDATA)currentThread->sp + (UDATA)currentThread->literals);
			J9Method *method = nativeMethodFrame->method;
			if (J9_ARE_ANY_BITS_SET((UDATA)method->constantPool, J9_STARTPC_NATIVE_REQUIRES_SWITCHING)) {
				/* zero the state and switch java offload mode OFF */
				currentThread->javaOffloadState = 0;
				/* check if the class requires lazy switching (for JDBC) or normal switching */
				J9Class *methodClass = J9_CLASS_FROM_METHOD(method);
				if (J9_ARE_ANY_BITS_SET(J9CLASS_FLAGS(methodClass), J9AccClassHasJDBCNatives)) {
					vm->javaOffloadSwitchJDBCWithMethodFunc(currentThread, method);
				} else {
					vm->javaOffloadSwitchOffWithMethodFunc(currentThread, method);
				}
			}
		}
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */
	}

	/**
	 * Perform the zaap offload switch (if necessary) upon return from a JNI
	 * native call.  The JNI native method frame must be on stack, and
	 * currentThread must be up-to-date.
	 *
	 * @param[in] currentThread the current J9VMThread
	 */
	static VMINLINE void
	afterJNICall(J9VMThread *currentThread)
	{
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
		J9JavaVM *vm = currentThread->javaVM;
		J9VMEntryLocalStorage *els = currentThread->entryLocalStorage;
		/* check if java offload mode is enabled */
		if (NULL != vm->javaOffloadSwitchOnWithMethodFunc) {
			/* check if we need to change state */
			if (0 == currentThread->javaOffloadState) {
				if (0 != els->savedJavaOffloadState) {
					/* if yes, call the offload switch ON and restore our state from ELS */
					J9SFJNINativeMethodFrame *nativeMethodFrame = (J9SFJNINativeMethodFrame*)((UDATA)currentThread->sp + (UDATA)currentThread->literals);
					J9Method *method = nativeMethodFrame->method;
					vm->javaOffloadSwitchOnWithMethodFunc(currentThread, method);
					currentThread->javaOffloadState = els->savedJavaOffloadState;
				}
			}
		}
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */
	}

	static VMINLINE bool
	hasDefaultConflictSendTarget(J9Method *method)
	{
		const void * conflictRunAddress = J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_DEFAULT_CONFLICT);
		return conflictRunAddress == method->methodRunAddress;
	}

	static VMINLINE bool
	threadIsInterruptedImpl(J9VMThread *currentThread, j9object_t threadObject)
	{
		bool result = false;
		/* If the thread is alive, ask the OS thread.  Otherwise, answer false. */
		if (J9VMJAVALANGTHREAD_STARTED(currentThread, threadObject)) {
			J9VMThread *targetThread = J9VMJAVALANGTHREAD_THREADREF(currentThread, threadObject);
			if (NULL != targetThread) {
				if (omrthread_interrupted(targetThread->osThread)) {
					result = true;
				}
			}
		}
		return result;
	}

	static VMINLINE jobject
	createLocalRef(JNIEnv *env, j9object_t object)
	{
		j9object_t* ref = NULL;

		if (object != NULL) {
			J9VMThread *vmThread = (J9VMThread*)env;
			J9SFJNINativeMethodFrame* frame = (J9SFJNINativeMethodFrame*)((U_8*)vmThread->sp + (UDATA)vmThread->literals);
			if ((0 == (frame->specialFrameFlags & J9_SSF_CALL_OUT_FRAME_ALLOC)) && ((UDATA)vmThread->literals < (J9_SSF_CO_REF_SLOT_CNT * sizeof(UDATA)))) {
				vmThread->literals = (J9Method*)((UDATA)vmThread->literals + sizeof(UDATA));
#ifdef J9VM_INTERP_GROWABLE_STACKS
				frame->specialFrameFlags += 1;
#endif
				ref = (j9object_t*)--(vmThread->sp);	/* predecrement sp */
				*ref = object;
			} else {
				ref = (j9object_t*)J9_VM_FUNCTION(vmThread, j9jni_createLocalRef)(env, object);
			}
		}

		return (jobject)ref;
	}
	
	static VMINLINE U_32
	lookupVarHandleMethodTypeCacheIndex(J9ROMClass *romClass, UDATA cpIndex)
	{
		U_16 *varHandleMethodTypeLookupTable = NNSRP_GET(romClass->varHandleMethodTypeLookupTable, U_16*);
		U_32 high = romClass->varHandleMethodTypeCount - 1;
		U_32 index = high / 2;
		U_32 low = 0;

		while (varHandleMethodTypeLookupTable[index] != cpIndex) {
			if (varHandleMethodTypeLookupTable[index] > cpIndex) {
				high = index - 1;
			} else {
				low = index + 1;
			}
			index = (high + low) / 2;
		}

		return index;
	}

	/**

	 * Check if two MethodTypes are compatible ignoring the last argument of the second MethodType.
	 * Given two MethodTypes:  (A B C)R & (A B C D)R, ensure that the return types match, that the second
	 * MT is 1 argument longer, and that the other arguments match.
	 * This is useful for VarHandle invokes which have a trailing VarHandle argument on the MH being invoked.
	 *
	 * @param currentThread The current J9VMThread
	 * @param shorterMT The first MethodType object, expected to have one less argument.  Must not be null.
	 * @param longerMT The second MethodTYpe object, expected to have one more argument.  Must not be null.
	 * @return whether the comparisons succeeds or not.
	 */
	static bool
	doMethodTypesMatchIgnoringLastArgument(J9VMThread *currentThread, j9object_t shorterMT, j9object_t longerMT)
	{
		bool matched = false;
		j9object_t shorterMTReturnType = J9VMJAVALANGINVOKEMETHODTYPE_RETURNTYPE(currentThread, shorterMT);
		j9object_t longerMTReturnType = J9VMJAVALANGINVOKEMETHODTYPE_RETURNTYPE(currentThread, longerMT);

		if (shorterMTReturnType == longerMTReturnType) {
			j9object_t shorterMTArguments = J9VMJAVALANGINVOKEMETHODTYPE_ARGUMENTS(currentThread, shorterMT);
			j9object_t longerMTArguments = J9VMJAVALANGINVOKEMETHODTYPE_ARGUMENTS(currentThread, longerMT);
			I_32 longerMTArgumentsLength = (I_32)J9INDEXABLEOBJECT_SIZE(currentThread, longerMTArguments);

			if ((longerMTArgumentsLength - 1) == (I_32)J9INDEXABLEOBJECT_SIZE(currentThread, shorterMTArguments)) {
				for (I_32 i = 0; i < longerMTArgumentsLength - 1; i++) {
					if (J9JAVAARRAYOFOBJECT_LOAD(currentThread, shorterMTArguments, i) != J9JAVAARRAYOFOBJECT_LOAD(currentThread, longerMTArguments, i)) {
						goto exit;
					}
				}
				matched = true;
			}
		}
exit:
		return matched;
	}

	/**
	 * Check if the current method is a method invocation
	 * @param vm
	 * @param method ramMethod
	 * @param currentThread
	 * @param currentClass class of the current method
	 * @return true if the method is a reflection method
	 */
	static VMINLINE bool
	isReflectionMethod( J9VMThread* currentThread, J9Method* method)
	{
		J9Class* currentClass = J9_CLASS_FROM_METHOD(method);
		J9JavaVM* vm = currentThread->javaVM;
		return ((method == vm->jlrMethodInvoke)
				|| (method == vm->jliMethodHandleInvokeWithArgs)
				|| (method == vm->jliMethodHandleInvokeWithArgsList)
				|| (vm->srMethodAccessor
						&& VM_VMHelpers::isSameOrSuperclass(
								J9VM_J9CLASS_FROM_JCLASS(currentThread,
										vm->srMethodAccessor), currentClass)));
	}

	/**
	 * Test if the current method is hidden, e.g. a lambda helper method.
	 *
	 * @param method ramMethod
	 * @param currentClass class of the current method
	 * @return true if the method is a hidden method
	 */
	static VMINLINE bool
	isHiddenMethod(J9Method* method)
	{
		bool isHidden = false;
		J9Class* currentClass = J9_CLASS_FROM_METHOD(method);
		if (J9_ARE_ANY_BITS_SET(currentClass->classFlags, J9ClassIsAnonymous)) {
			/* lambda helper method */
			isHidden = true;
		} else {
			J9ROMMethod* romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
			if (J9_ARE_ANY_BITS_SET(romMethod->modifiers, J9AccMethodFrameIteratorSkip)) {
				/* Methods with @FrameIteratorSkip annotation, such as JIT thunk */
				isHidden = true;
			}
		}
		return isHidden;
	}

	/**
	 * @brief Converts the type of the return value to the return type
	 * @param returnType[in] The type of the return value
	 * @param returnStorage[in] The pointer to the return value
	 */
	static VMINLINE void
	convertJNIReturnValue(U_8 returnType, UDATA* returnStorage)
	{
		switch (returnType) {
		case J9NtcObject:
			/* The native method return hook expects to find:
			 * 		returnValue = j9object_t
			 * 		returnValue2 = jobject for returnValue
			 * Any code that releases VMAccess must protect the value of returnValue
			 */
			*(returnStorage + 1) = *returnStorage;
			if (0 != *returnStorage) {
				*returnStorage = *(UDATA*)(*returnStorage);
			}
			break;
		case J9NtcBoolean:
		{
			U_32 returnValue = (U_32)*returnStorage;
			U_8 * returnAddress = (U_8 *)&returnValue;
#ifdef J9VM_ENV_LITTLE_ENDIAN
			*returnStorage = (UDATA)(0 != returnAddress[0]);
#else
			*returnStorage = (UDATA)(0 != returnAddress[3]);
#endif /*J9VM_ENV_LITTLE_ENDIAN */
		}
			break;
		case J9NtcByte:
			*returnStorage = (UDATA)(IDATA)(I_8)*returnStorage;
			break;
		case J9NtcChar:
			*returnStorage = (UDATA)(U_16)*returnStorage;
			break;
		case J9NtcShort:
			*returnStorage = (UDATA)(IDATA)(I_16)*returnStorage;
			break;
		case J9NtcInt:
			*returnStorage = (UDATA)(IDATA)(I_32)*returnStorage;
			break;
		case J9NtcFloat:
			*returnStorage = (UDATA)*(U_32*)returnStorage;
			break;
		case J9NtcVoid:
			/* Fall through is intentional */
		case J9NtcLong:
			/* Fall through is intentional */
		case J9NtcDouble:
			/* Fall through is intentional */
		case J9NtcClass:
			break;
		}
	}

	/**
	 * @brief Notify JIT upon the first modification of a final field, a stack frame should be build before calling this method to make the stack walkable
	 * @param currentThread
	 * @param fieldClass The declaring class of the final field
	 */
	static VMINLINE void
	reportFinalFieldModified(J9VMThread* currentThread, J9Class* fieldClass)
	{
		/** Only report modifications after class initialization
		 *  Since final field write is allowed during class init process,
		 *  JIT will not start to trust final field values until the class has completed initialization
		 */
		if (J9_ARE_NO_BITS_SET(fieldClass->classFlags, J9ClassHasIllegalFinalFieldModifications) && (J9ClassInitSucceeded == fieldClass->initializeStatus)) {
			J9JavaVM* vm = currentThread->javaVM;
			if (J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_OSR_SAFE_POINT)) {
				J9InternalVMFunctions* vmFuncs = vm->internalVMFunctions;
				vmFuncs->acquireSafePointVMAccess(currentThread);
				/* check class flag again after acquiring VM access */
				if (J9_ARE_NO_BITS_SET(fieldClass->classFlags, J9ClassHasIllegalFinalFieldModifications)) {
					J9JITConfig* jitConfig = vm->jitConfig;
					if (NULL != jitConfig) {
						jitConfig->jitIllegalFinalFieldModification(currentThread, fieldClass);
					}
				}
				vmFuncs->releaseSafePointVMAccess(currentThread);
			}
		}
	}

	/**
	 * Find the J9SFJNINativeMethodFrame representing the current native.
	 *
	 * @param currentThread[in] the current J9VMThread
	 *
	 * @returns the native method frame
	 */
	static VMINLINE J9SFJNINativeMethodFrame*
	findNativeMethodFrame(J9VMThread *currentThread)
	{
		return (J9SFJNINativeMethodFrame*)((UDATA)currentThread->sp + (UDATA)currentThread->literals);
	}

	/**
	 * Checks whether a ROM method is <clinit> or <init>
	 *
	 * @param romMethod[in] the J9ROMMethod to test
	 * @param isStatic[in] true to check for <clinit>, false to check for <init>
	 *
	 * @returns true if the method is a constructor, false if not
	 */
	static VMINLINE bool
	romMethodIsInitializer(J9ROMMethod *romMethod, bool isStatic)
	{
		U_8 *name = J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod));
		/* No method may have an empty name, so reading the first byte is always
		 * legal. The verifier only allows <clinit> or <init> to start with "<",
		 * so reading the second byte is legal if the first byte is "<".
		 */
		return ('<' == name[0]) && ((isStatic ? 'c' : 'i') == name[1]);
	}

	/**
	 * Checks whether a class enforces the final field setting rules.
	 * Classes which are class file version 53 or above enforce the rule
	 * unless the class was defined in a way that exempts it from validation.
	 *
	 * @param ramClass[in] the J9Class to test
	 *
	 * @returns true if the class enforces the rules, false if not
	 */
	static VMINLINE bool
	ramClassChecksFinalStores(J9Class *ramClass)
	{
		return (!J9CLASS_IS_EXEMPT_FROM_VALIDATION(ramClass))
			&& (ramClass->romClass->majorVersion >= 53);
	}

};

#endif /* VMHELPERS_HPP_ */
