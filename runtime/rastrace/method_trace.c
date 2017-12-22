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

#include "rommeth.h"

#define  _UTE_STATIC_
#include "j9rastrace.h"

#include "ut_j9trc.h"
#undef UT_MODULE_LOADED
#undef UT_MODULE_UNLOADED
#include "ut_mt.h"

static void hookRAMClassLoad(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void traceMethodArgInt (J9VMThread *thr, UDATA* arg0EA, char* cursor, UDATA length, char* type); 
static void traceMethodArgDouble (J9VMThread *thr, UDATA* arg0EA, char* cursor, UDATA length);
static void traceMethodExit (J9VMThread *thr, J9Method *method, UDATA isCompiled, void* returnValuePtr, UDATA doParameters);
static void traceMethodExitX (J9VMThread *thr, J9Method *method, UDATA isCompiled, void* exceptionPtr, UDATA doParameters);
static void traceMethodArgFloat (J9VMThread *thr, UDATA* arg0EA, char* cursor, UDATA length); 
static void traceMethodArgObject (J9VMThread *thr, UDATA* arg0EA, char* cursor, UDATA length);
static char* traceMethodArguments (J9VMThread* thr, J9UTF8* signature, UDATA* arg0EA, char* buf, const char* endOfBuf); 
static char* traceMethodReturnVal(J9VMThread* thr, J9UTF8* signature, void* returnValuePtr, char* buf, const char* endOfBuf);
static void traceMethodArgBoolean (J9VMThread *thr, UDATA* arg0EA, char* cursor, UDATA length); 
static void traceMethodEnter (J9VMThread *thr, J9Method *method, void *receiverAddress, UDATA isCompiled, UDATA doParameters);
static void traceMethodArgLong (J9VMThread *thr, UDATA* arg0EA, char* cursor, UDATA length);
static U_8 checkMethod (J9VMThread *thr, J9Method *method);

/**************************************************************************
 * name        - matchMethod
 * description - Checks to see if the method that has been loaded matches
 *                               an entry for either class or method name.
 * parameters  - Pointer to either a class or method name.
 *                               Pointer to class or method name loaded.
 * returns     - true or false
 *************************************************************************/

BOOLEAN
matchMethod(RasMethodTable * methodTable, J9Method *method)
{
	J9UTF8 * className;
	J9UTF8 * methodName;

	if (methodTable->methodName == NULL) {
		if (methodTable->className == NULL) {
			/*
			 *  Any class, any method
			 */
			return TRUE;
		} else {
			/*
			 *  Any method, specific class(es)
			 */
			className = J9ROMCLASS_CLASSNAME(J9_CLASS_FROM_METHOD(method)->romClass);
			return wildcardMatch(
				methodTable->classMatchFlag, 
				(const char *)J9UTF8_DATA(methodTable->className), J9UTF8_LENGTH(methodTable->className),
				(const char *)J9UTF8_DATA(className), J9UTF8_LENGTH(className));
		}
	}
	if (methodTable->className == NULL) {
		/*
		 * Specific method, any class
		 */

		methodName = J9ROMMETHOD_GET_NAME(J9_CLASS_FROM_METHOD(method)->romClass, J9_ROM_METHOD_FROM_RAM_METHOD(method));
		return wildcardMatch(
			methodTable->methodMatchFlag,
			(const char *)J9UTF8_DATA(methodTable->methodName), J9UTF8_LENGTH(methodTable->methodName),
			(const char *)J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodName));
	} else {
		/*
		 *  Specific method(s), specific class(es)
		 */

		methodName = J9ROMMETHOD_GET_NAME(J9_CLASS_FROM_METHOD(method)->romClass, J9_ROM_METHOD_FROM_RAM_METHOD(method));
		if (wildcardMatch(
			methodTable->methodMatchFlag,
			(const char *)J9UTF8_DATA(methodTable->methodName), J9UTF8_LENGTH(methodTable->methodName),
			(const char *)J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodName)))
		{
			className = J9ROMCLASS_CLASSNAME(J9_CLASS_FROM_METHOD(method)->romClass);
			return wildcardMatch(
				methodTable->classMatchFlag,
				(const char *)J9UTF8_DATA(methodTable->className), J9UTF8_LENGTH(methodTable->className),
				(const char *)J9UTF8_DATA(className), J9UTF8_LENGTH(className));
		}
	}
	return FALSE;
}

/**************************************************************************
 * name        - checkMethod
 * description - Checks to see if the class / method matches
 *               the trace spec and thus need to be traced.
 * parameters  - Pointer to the current thread
 *               Pointer to the method
 * returns     - none
 *************************************************************************/
static
U_8 checkMethod(J9VMThread *thr, J9Method *method)
{
	RasMethodTable *methodTable;
	U_8 flag = J9_RAS_METHOD_SEEN;

	Trc_trcengine_checkMethod(thr);

	methodTable = ((RasGlobalStorage *)thr->javaVM->j9rasGlobalStorage)->traceMethodTable;

	/*
	 * Check through the method table for a match
	 */

	while(methodTable != NULL) {

		/* Try and match the class / method names. */

		if (matchMethod(methodTable, method)) {

			if(methodTable->includeFlag == TRUE) {
				flag |= J9_RAS_METHOD_TRACING;
				/* Check if input and return values need to be traced */

				if(methodTable->traceInputRetVals == TRUE) {
					flag |= J9_RAS_METHOD_TRACE_ARGS;
				}
			} else {
				/* Disable all method trace for this method */

				flag &= ~(J9_RAS_METHOD_TRACING | J9_RAS_METHOD_TRACE_ARGS);
			}
		}

		/* Move to the next method table entry. */

		methodTable = methodTable->next;
	}
	return flag;
}

/**************************************************************************
 * name        - traceMethodEnter
 * description - Collects the data to be traced for a method and issues the
 *               appropriate tracepoints for the MT component.
 * parameters  - thread, J9Method pointer, pointer to 'this', entry/exit flag, isCompiled flag
 *               and doParameters flag.
 * returns     - none
 *************************************************************************/

static void 
traceMethodEnter(J9VMThread *thr, J9Method *method, void *receiverAddress, UDATA isCompiled, UDATA doParameters)
{
	J9UTF8* className = J9ROMCLASS_CLASSNAME(J9_CLASS_FROM_METHOD(method)->romClass);
	J9UTF8* methodName = J9ROMMETHOD_GET_NAME(J9_CLASS_FROM_METHOD(method)->romClass, J9_ROM_METHOD_FROM_RAM_METHOD(method));
	J9UTF8* methodSignature = J9ROMMETHOD_GET_SIGNATURE(J9_CLASS_FROM_METHOD(method)->romClass, J9_ROM_METHOD_FROM_RAM_METHOD(method));
	UDATA modifiers = J9_ROM_METHOD_FROM_RAM_METHOD(method)->modifiers;

	if (isCompiled) {
		if (modifiers & J9AccStatic) {
			Trc_MethodEntryCS(thr, J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodSignature), J9UTF8_DATA(methodSignature));
		} else {
			j9object_t receiver = *(j9object_t*)receiverAddress;
			Trc_MethodEntryC(thr, J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodSignature), J9UTF8_DATA(methodSignature), receiver);
		}
	} else {
		char buf[1024];

		if (modifiers & J9AccStatic) {
			if (modifiers & J9AccNative) {
				Trc_MethodEntryNS(thr, J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodSignature), J9UTF8_DATA(methodSignature));
			} else {
				Trc_MethodEntryS(thr, J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodSignature), J9UTF8_DATA(methodSignature));
			}
			if (doParameters) {
				Trc_MethodArgumentsS(thr, traceMethodArguments(thr, methodSignature, (UDATA*)receiverAddress, buf, buf + sizeof(buf)));
			}
		} else {
			j9object_t receiver = *(j9object_t*)receiverAddress;
			J9Class* receiverClazz = J9OBJECT_CLAZZ(thr, receiver);
			J9UTF8* receiverClassName = J9ROMCLASS_CLASSNAME(receiverClazz->romClass);

			if (modifiers & J9AccNative) {
				Trc_MethodEntryN(thr, J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodSignature), J9UTF8_DATA(methodSignature), receiver);
			} else {
				Trc_MethodEntry(thr, J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodSignature), J9UTF8_DATA(methodSignature), receiver);
			}

			if (doParameters) {
				Trc_MethodArguments(
						thr, 
						(U_32)J9UTF8_LENGTH(receiverClassName),
						J9UTF8_DATA(receiverClassName),
						receiver,
						traceMethodArguments(thr, methodSignature, (UDATA*)receiverAddress - 1, buf, buf + sizeof(buf)));
			}
		}
	}
}

/**************************************************************************
 * name        - traceMethodExit
 * description - Collects the data to be traced for a method when it returns
 *               and issues the appropriate tracepoints for the MT component.
 * parameters  - thread, J9Method pointer, isCompiled flag, pointer to the
 *               return value and whether parameters and return values are
 *               being formatted.
 * returns     - none
 *************************************************************************/
static void 
traceMethodExit(J9VMThread *thr, J9Method *method, UDATA isCompiled, void* returnValuePtr, UDATA doParameters)
{
	J9UTF8* className = J9ROMCLASS_CLASSNAME(J9_CLASS_FROM_METHOD(method)->romClass);
	J9UTF8* methodName = J9ROMMETHOD_GET_NAME(J9_CLASS_FROM_METHOD(method)->romClass, J9_ROM_METHOD_FROM_RAM_METHOD(method));
	J9UTF8* methodSignature = J9ROMMETHOD_GET_SIGNATURE(J9_CLASS_FROM_METHOD(method)->romClass, J9_ROM_METHOD_FROM_RAM_METHOD(method));
	UDATA modifiers = J9_ROM_METHOD_FROM_RAM_METHOD(method)->modifiers;

	if (isCompiled) {
		if (modifiers & J9AccStatic) {
			Trc_MethodExitCS(thr, J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodSignature), J9UTF8_DATA(methodSignature));
		} else {
			Trc_MethodExitC(thr, J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodSignature), J9UTF8_DATA(methodSignature));
		}
	} else {
		if (modifiers & J9AccNative) {
			if (modifiers & J9AccStatic) {
				Trc_MethodExitNS(thr, J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodSignature), J9UTF8_DATA(methodSignature));
			} else {
				Trc_MethodExitN(thr, J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodSignature), J9UTF8_DATA(methodSignature));
			}
		} else {
			if (modifiers & J9AccStatic) {
				Trc_MethodExitS(thr, J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodSignature), J9UTF8_DATA(methodSignature));
			} else {
				Trc_MethodExit(thr, J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodSignature), J9UTF8_DATA(methodSignature));
			}
		}
	}

	if( doParameters ) {
		char buf[1024];
		/* Don't trace the void return type. */
		if( traceMethodReturnVal(thr, methodSignature, returnValuePtr, buf, buf + sizeof(buf)) ) {
			Trc_MethodReturn( thr, buf );
		};
	}
}

/**************************************************************************
 * name        - traceMethodExitX
 * description - Collects the data to be traced for a method when it returns
 *               after an exception is thrown and issues the appropriate tracepoints
 *               for the MT component.
 * parameters  - thread, J9Method pointer, isCompiled flag, pointer to the exception
 *               and whether parameters and return values are being formatted.
 * returns     - none
 *************************************************************************/

static void
traceMethodExitX(J9VMThread *thr, J9Method *method, UDATA isCompiled, void* exceptionPtr,UDATA doParameters)
{
	J9UTF8* className = J9ROMCLASS_CLASSNAME(J9_CLASS_FROM_METHOD(method)->romClass);
	J9UTF8* methodName = J9ROMMETHOD_GET_NAME(J9_CLASS_FROM_METHOD(method)->romClass, J9_ROM_METHOD_FROM_RAM_METHOD(method));
	J9UTF8* methodSignature = J9ROMMETHOD_GET_SIGNATURE(J9_CLASS_FROM_METHOD(method)->romClass, J9_ROM_METHOD_FROM_RAM_METHOD(method));
	UDATA modifiers = J9_ROM_METHOD_FROM_RAM_METHOD(method)->modifiers;

	if (isCompiled) {
		if (modifiers & J9AccStatic) {
			Trc_MethodExitCXS(thr, J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodSignature), J9UTF8_DATA(methodSignature));
		} else {
			Trc_MethodExitCX(thr, J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodSignature), J9UTF8_DATA(methodSignature));
		}
	} else {
		if (modifiers & J9AccNative) {
			if (modifiers & J9AccStatic) {
				Trc_MethodExitNXS(thr, J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodSignature), J9UTF8_DATA(methodSignature));
			} else {
				Trc_MethodExitNX(thr, J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodSignature), J9UTF8_DATA(methodSignature));
			}
		} else {
			if (modifiers & J9AccStatic) {
				Trc_MethodExitXS(thr, J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodSignature), J9UTF8_DATA(methodSignature));
			} else {
				Trc_MethodExitX(thr, J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodSignature), J9UTF8_DATA(methodSignature));
			}
		}
	}

	if( doParameters ) {
		char buf[1024];
		traceMethodArgObject(thr, (UDATA*)&exceptionPtr, buf, sizeof(buf) );
		Trc_MethodException(thr, buf);
	}

}

static void
hookRAMClassLoad(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMInternalClassLoadEvent* event = (J9VMInternalClassLoadEvent *)eventData;
	J9VMThread* thr = event->currentThread;
	J9JavaVM *vm = thr->javaVM;
	J9Class* clazz = event->clazz;
	J9ROMClass* romClass = clazz->romClass;
	U_32 i;

	J9Method * method = clazz->ramMethods;
	for (i = 0; i < romClass->romMethodCount; i++) {
		U_8 *mtFlag = fetchMethodExtendedFlagsPointer(method);
		setExtendedMethodFlags(vm, mtFlag, (checkMethod(thr, method) | rasSetTriggerTrace(thr, method) ) );
		method++;
	}
}

/* External entry point for method trace, exposed via J9UtServerInterface. */
void
trcTraceMethodEnter(J9VMThread* thr, J9Method* method, void *receiverAddress, UDATA methodType)
{
	/* Disabled until VM/JIT work for Story 51842 is done and method
	 * entry/exit hooks can be disabled. Can be uncommented for testing by
	 * VM/JIT but if you don't disable the entry exit hooks in
	 * enableMethodTraceHooks you will see all the method trace points twice.*/
	U_8 *mtFlag = fetchMethodExtendedFlagsPointer(method);

	if ( 0 != (*mtFlag & J9_RAS_METHOD_TRIGGERING) ) {
		rasTriggerMethod(thr, method, TRUE, BEFORE_TRACEPOINT);
	}

	if ( 0 != (*mtFlag & J9_RAS_METHOD_TRACING) ) {
		UDATA doParameters = *mtFlag & J9_RAS_METHOD_TRACE_ARGS;
		traceMethodEnter(thr, method, receiverAddress, methodType, doParameters);
	}

	if ( 0 != (*mtFlag & J9_RAS_METHOD_TRIGGERING) ) {
		rasTriggerMethod(thr, method, TRUE, AFTER_TRACEPOINT);
	}

}

/* External entry point for method trace, exposed via J9UtServerInterface. */
void
trcTraceMethodExit(J9VMThread* thr, J9Method* method,  void* exceptionPtr, void *returnValuePtr, UDATA methodType)
{
	/* Disabled until VM/JIT work for Story 51842 is done and method
	 * entry/exit hooks can be disabled. Can be uncommented for testing by
	 * VM/JIT but if you don't disable the entry exit hooks in
	 * enableMethodTraceHooks you will see all the method trace points twice.*/
	U_8 *mtFlag = fetchMethodExtendedFlagsPointer(method);

	if ( 0 != (*mtFlag & J9_RAS_METHOD_TRIGGERING) ) {
		rasTriggerMethod(thr, method, FALSE, BEFORE_TRACEPOINT);
	}

	if ( 0 != (*mtFlag & J9_RAS_METHOD_TRACING) ) {
		UDATA doParameters = *mtFlag & J9_RAS_METHOD_TRACE_ARGS;
		if( exceptionPtr ) {
			traceMethodExitX(thr, method, methodType, exceptionPtr, doParameters);
		} else {
			traceMethodExit(thr, method, methodType, returnValuePtr, doParameters);
		}
	}

	if ( 0 != (*mtFlag & J9_RAS_METHOD_TRIGGERING) ) {
		rasTriggerMethod(thr, method, FALSE, AFTER_TRACEPOINT);
	}

}

omr_error_t
enableMethodTraceHooks(J9JavaVM *vm)
{
	J9HookInterface** hook = vm->internalVMFunctions->getVMHookInterface(vm);

	/* Called from J9VMDLLMain, single threaded at startup so no
	 * need to protect with vm->runtimeFlagsMutex
	 */
	vm->extendedRuntimeFlags |= J9_EXTENDED_RUNTIME_METHOD_TRACE_ENABLED;

	if ((*hook)->J9HookRegisterWithCallSite(hook, J9HOOK_VM_INTERNAL_CLASS_LOAD, hookRAMClassLoad, OMR_GET_CALLSITE(), NULL)) {
		return OMR_ERROR_INTERNAL;
	}

	return OMR_ERROR_NONE;
}

/*
 * Trigger the appropriate tracepoints for an double-type method argument
 */
static void 
traceMethodArgDouble(J9VMThread *thr, UDATA* arg0EA, char* cursor, UDATA length)
{
	jdouble data;
	PORT_ACCESS_FROM_VMC(thr);

	/* read a potentially misaligned double value */
	memcpy(&data, arg0EA, sizeof(data));

	j9str_printf(PORTLIB, cursor, length, "(double)%f", data);
}

/*
 * Trigger the appropriate tracepoints for an float-type method argument
 */
static void 
traceMethodArgFloat(J9VMThread *thr, UDATA* arg0EA, char* cursor, UDATA length)
{
	jfloat data = *(jfloat*)arg0EA;
	PORT_ACCESS_FROM_VMC(thr);

	j9str_printf(PORTLIB, cursor, length, "(float)%f", data);
}

/*
 * Trigger the appropriate tracepoints for an int-type method argument
 */
static void 
traceMethodArgInt(J9VMThread *thr, UDATA* arg0EA, char* cursor, UDATA length, char* type)
{
	jint data = *(jint*)arg0EA;
	PORT_ACCESS_FROM_VMC(thr);

	j9str_printf(PORTLIB, cursor, length, "(%s)%d", type, data);
}

/*
 * Trigger the appropriate tracepoints for an long-type method argument
 */
static void 
traceMethodArgLong(J9VMThread *thr, UDATA* arg0EA, char* cursor, UDATA length)
{
	jlong data;
	PORT_ACCESS_FROM_VMC(thr);

	/* read a potentially misaligned long value */
	memcpy(&data, arg0EA, sizeof(data));

	j9str_printf(PORTLIB, cursor, length, "(long)%lld", data);
}

/*
 * Trigger the appropriate tracepoints for an object-type method argument
 */
static void 
traceMethodArgObject(J9VMThread *thr, UDATA* arg0EA, char* cursor, UDATA length)
{
	j9object_t object = *(j9object_t*)arg0EA;
	PORT_ACCESS_FROM_VMC(thr);

	if (object == NULL) {
		j9str_printf(PORTLIB, cursor, length, "null");
	} else {
		J9Class* clazz = J9OBJECT_CLAZZ(thr, object);
		J9ROMClass * romClass = clazz->romClass;
		J9UTF8* className = J9ROMCLASS_CLASSNAME(romClass);

		/* TODO: handle arrays */

		j9str_printf(PORTLIB, cursor, length, "%.*s@%p", (U_32)J9UTF8_LENGTH(className), J9UTF8_DATA(className), object);
	}
}

static char* 
traceMethodArguments(J9VMThread* thr, J9UTF8* signature, UDATA* arg0EA, char* buf, const char* endOfBuf)
{
	char* cursor = buf;
	U_8* sigChar = J9UTF8_DATA(signature);

	buf[0] = '\0';

	while (*(++sigChar) != ')') {
		switch (*sigChar) {
		case '[':
		case 'L':
			traceMethodArgObject(thr, arg0EA--, cursor, endOfBuf - cursor);
			while (*sigChar == '[') {
				sigChar++;
			}
			if (*sigChar == 'L' ) {
				while (*sigChar != ';') {
					sigChar++;
				}
			}
			break;
		case 'J':
			arg0EA--;
			traceMethodArgLong(thr, arg0EA--, cursor, endOfBuf - cursor);
			break;
		case 'D':
			arg0EA--;
			traceMethodArgDouble(thr, arg0EA--, cursor, endOfBuf - cursor);
			break;
		case 'F':
			traceMethodArgFloat(thr, arg0EA--, cursor, endOfBuf - cursor);
			break;
		case 'B':
			traceMethodArgInt(thr, arg0EA--, cursor, endOfBuf - cursor, "byte");
			break;
		case 'C':
			traceMethodArgInt(thr, arg0EA--, cursor, endOfBuf - cursor, "char");
			break;
		case 'I':
			traceMethodArgInt(thr, arg0EA--, cursor, endOfBuf - cursor, "int");
			break;
		case 'S':
			traceMethodArgInt(thr, arg0EA--, cursor, endOfBuf - cursor, "short");
			break;
		case 'Z':
			traceMethodArgBoolean(thr, arg0EA--, cursor, endOfBuf - cursor);
			break;
		default:
			return "ERROR";
		}

		/* advance the cursor to the new end of the buffer */
		cursor = cursor + strlen(cursor);

		/* replace the terminating NUL with a comma if there's room */
		if (cursor < endOfBuf - 1) {
			*cursor++ = ',';
		} else {
			break;
		}
	}

	if (cursor == endOfBuf - 1) {
		/* insert an ellipsis if the buffer was truncated (assumes the buffer is at least 3 chars long) */
		*--cursor = '.';
		*--cursor = '.';
		*--cursor = '.';
	} else {
		/* remove the final comma */
		*--cursor = '\0';
	}

	return buf;
}

static char*
traceMethodReturnVal(J9VMThread* thr, J9UTF8* signature, void* returnValuePtr, char* buf, const char* endOfBuf)
{
	char* cursor = buf;
	U_8* sigChar = J9UTF8_DATA(signature);

	buf[0] = '\0';

	/* Ignore most of the signature until the return type. */
	while (*(++sigChar) != ')') {
		continue;
	}
	switch (*(++sigChar)) {
	case '[':
	case 'L':
		traceMethodArgObject(thr, returnValuePtr, cursor, endOfBuf - cursor);
		break;
	case 'J':
		traceMethodArgLong(thr, returnValuePtr, cursor, endOfBuf - cursor);
		break;
	case 'D':
		traceMethodArgDouble(thr, returnValuePtr, cursor, endOfBuf - cursor);
		break;
	case 'F':
		traceMethodArgFloat(thr, returnValuePtr, cursor, endOfBuf - cursor);
		break;
	case 'B':
		traceMethodArgInt(thr, returnValuePtr, cursor, endOfBuf - cursor, "byte");
		break;
	case 'C':
		traceMethodArgInt(thr, returnValuePtr, cursor, endOfBuf - cursor, "char");
		break;
	case 'I':
		traceMethodArgInt(thr, returnValuePtr, cursor, endOfBuf - cursor, "int");
		break;
	case 'S':
		traceMethodArgInt(thr, returnValuePtr, cursor, endOfBuf - cursor, "short");
		break;
	case 'Z':
		traceMethodArgBoolean(thr, returnValuePtr, cursor, endOfBuf - cursor);
		break;
	case 'V':
		return NULL;
		/* break; */
	default:
		return "ERROR";
	}

	/* advance the cursor to the end of the string */
	cursor = cursor + strlen(cursor);

	if (cursor == endOfBuf - 1) {
		/* insert an ellipsis if the string was truncated (the buffer is 1024 chars long) */
		*--cursor = '.';
		*--cursor = '.';
		*--cursor = '.';
	}

	return buf;
}

/*
 * Trigger the appropriate tracepoints for an boolean-type method argument
 */
static void 
traceMethodArgBoolean(J9VMThread *thr, UDATA* arg0EA, char* cursor, UDATA length)
{
	jint data = *(jint*)arg0EA;
	PORT_ACCESS_FROM_VMC(thr);

	j9str_printf(PORTLIB, cursor, length, data ? "true" : "false");
}

