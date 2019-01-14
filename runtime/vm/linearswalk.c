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

#include "j9.h"
#include "linearswalk.h"
#include "omrlinkedlist.h"
#include "stackwalk.h"
#include "rommeth.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>



static const char * getFrameName(J9SWFrame * frame, char * name);
static const char * getFrameTypeName(UDATA frameType);
static const char * getResolveFrameType(UDATA resolveFrameType);
static char * lswStrDup(J9SlotWalker * slotWalker, char * str);
static IDATA getSlotIndex(J9StackWalkState * walkState, UDATA * addr);
static IDATA lswSanityCheck(J9JavaVM * vm, J9StackWalkState * walkState);
static void lswPrintf(struct J9PortLibrary *portLibrary, const char *format, ...);

#define LSW_ACCESS_FROM_WALKSTATE(walkState) J9SlotWalker *slotWalker = (J9SlotWalker *) (walkState)->linearSlotWalker
#define LSW_ENABLED(walkState) do { \
									if (NULL == (walkState)->linearSlotWalker) {\
										return SW_ERROR_NONE;\
									} \
								} while(0) 

#ifdef LINUX
static const char COL_RESET[] = "\x1b[0m";
static const char CYAN[]    = "\x1b[36m";
#else
static const char COL_RESET[] = "";
static const char CYAN[]    = ""; 
#endif


static UDATA
lswAnnotationHash(void *key, void *userData)
{
	J9SWSlotAnnotation * entry = (J9SWSlotAnnotation *) key;

	return (UDATA) entry->slotIndex;
}

static UDATA
lswAnnotationHashEqual(void *leftKey, void *rightKey, void *userData)
{
	J9SWSlotAnnotation *left_k = leftKey;
	J9SWSlotAnnotation *right_k = rightKey;

	return (left_k->slotIndex == right_k->slotIndex);
}




IDATA
lswInitialize(J9JavaVM * vm, J9StackWalkState * walkState)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA			rc;
	J9SlotWalker	*slotWalker = NULL;
	J9SWSlot		*slots = NULL;
	J9HashTable		*hashTable = NULL;
	J9Pool			*stringPool = NULL;
	J9Pool			*annotationPool = NULL;
	UDATA			slotArraySize;


	/* Grab a hashtable to store frame annotations. Indexed by slot number. */

	hashTable = hashTableNew(OMRPORT_FROM_J9PORT(PORTLIB), J9_GET_CALLSITE(), 16, sizeof(J9SWSlotAnnotation), 0, 0,
				OMRMEM_CATEGORY_VM, lswAnnotationHash, lswAnnotationHashEqual, NULL, NULL);
	if (NULL == hashTable) {
		rc =  SW_ERROR_HASHTABLE_NOMEM;
		goto err;
	}

	/* Pool of annotations for the annotations hashtable */

	annotationPool = pool_new(sizeof(J9SWFrame), 0, 0, POOL_ALWAYS_KEEP_SORTED, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(vm->portLibrary));
	if (NULL == annotationPool) {
		rc = SW_ERROR_ANNOTATION_POOL_NOMEM;
		goto err;
	}

	/* Pool of string buffers to be parcelled out for small strings. */

	stringPool = pool_new(LSW_STRING_POOL_MAX, 0, 0, POOL_ALWAYS_KEEP_SORTED, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(vm->portLibrary));
	if (NULL == stringPool) {
		rc = SW_ERROR_STRING_POOL_NOMEM;
		goto err;
	}

	slotWalker = j9mem_allocate_memory(sizeof(J9SlotWalker), OMRMEM_CATEGORY_VM);
	if (NULL == slotWalker) {
		rc = SW_ERROR_NOMEM;
		goto err;
	}
	memset(slotWalker, 0x00, sizeof(J9SlotWalker));


	slotWalker->sp = REMOTE_ADDR(walkState->walkSP);
	slotWalker->stackBottom = walkState->walkThread->stackObject->end;
	slotArraySize = sizeof(J9SWSlot) * (slotWalker->stackBottom - slotWalker->sp);

	slots = j9mem_allocate_memory(slotArraySize, OMRMEM_CATEGORY_VM);
	if (NULL == slots) {
		rc = SW_ERROR_NOMEM;
		goto err;
	}	
	memset(slots, 0x00, slotArraySize);


	slotWalker->slots = slots;
	slotWalker->stringPool = stringPool;
	slotWalker->slotAnnotations = hashTable;
	slotWalker->annotationPool = annotationPool;

	walkState->linearSlotWalker = slotWalker;

	return SW_ERROR_NONE;

err:

	if (annotationPool) {
		pool_kill(annotationPool);
	}

	if (stringPool) {
		pool_kill(stringPool);
	}

	j9mem_free_memory(hashTable);
	j9mem_free_memory(slotWalker);

	return rc; 
}

IDATA
lswCleanup(J9JavaVM * vm, J9StackWalkState * walkState)
{
	LSW_ACCESS_FROM_WALKSTATE(walkState);

	LSW_ENABLED(walkState);

	hashTableFree(slotWalker->slotAnnotations);
	pool_kill(slotWalker->annotationPool);
	pool_kill(slotWalker->stringPool);
	slotWalker->frameList = NULL;

	return SW_ERROR_NONE;
}


IDATA
lswFrameNew(J9JavaVM * vm, J9StackWalkState * walkState, UDATA frameType)
{
	J9SWFrame * frame;
	LSW_ACCESS_FROM_WALKSTATE(walkState);

	LSW_ENABLED(walkState);

	if (frameType == LSW_FRAME_TYPE_END_OF_STACK) {
		return SW_ERROR_NONE;
	}

	frame = pool_newElement(slotWalker->annotationPool);
	if (NULL == frame) {
		return SW_ERROR_ANNOTATION_POOL_NOMEM;
	}

	frame->linkNext = NULL;
	frame->linkPrevious = NULL;
	frame->method = NULL;
	frame->number = walkState->framesWalked;
	frame->type = frameType;

	J9_LINKED_LIST_ADD_LAST(slotWalker->frameList, frame);


	if (frame->type == LSW_FRAME_TYPE_JIT_INLINE) {
		/* JIT inline frame do not actually consume any slots but we want to 
		   treat them as "frames" for printing purposes */

		if (frame == slotWalker->frameList) {
			frame->topSlot = slotWalker->sp;
			frame->bottomSlot = slotWalker->sp;
		} else {
			frame->topSlot = frame->linkPrevious->bottomSlot;
			frame->bottomSlot = frame->topSlot;
		}

	} else {

		/* The bottom slot is always set to arg0EA */

		frame->bottomSlot = REMOTE_ADDR(walkState->arg0EA);

		/* Figure out the topSlot of this frame based on previous frames arg0EA 
		   or SP if we are looking at the first/top frame */

		if (frame == slotWalker->frameList) {
			frame->topSlot = slotWalker->sp;
		} else {
			frame->topSlot = frame->linkPrevious->bottomSlot + 1;
		}
	}


	slotWalker->frameCount++;
	slotWalker->currentFrame = frame;

	return SW_ERROR_NONE;
}


IDATA
lswRecord(J9StackWalkState * walkState, UDATA recordType, void * recordValue)
{
	IDATA slotIndex;
	J9SWSlot * slot;
	J9HashTable * slotAnntHT;
	J9SWSlotAnnotation entry;
	J9SWFrame * frame;
	LSW_ACCESS_FROM_WALKSTATE(walkState);
	
	LSW_ENABLED(walkState);

	frame = slotWalker->currentFrame;
	slotAnntHT = slotWalker->slotAnnotations;

	switch (recordType) {

		case LSW_TYPE_FRAME_TYPE:
			frame->type = (UDATA) recordValue;
			break;

		case LSW_TYPE_FRAME_NAME:
			frame->name = lswStrDup(slotWalker, recordValue);
			break;

		case LSW_TYPE_JIT_BP:
			slotIndex = getSlotIndex(walkState, recordValue);
			slot = &slotWalker->slots[slotIndex];
			slot->data = READ_UDATA((UDATA *) recordValue);
			slot->name = lswStrDup(slotWalker, "Return PC");

			/* Fall through to LSW_TYPE_BP to record the BP location */

		case LSW_TYPE_BP:
			entry.slotIndex = getSlotIndex(walkState, recordValue);
			entry.description = (char *) lswStrDup(slotWalker, "BP");
			hashTableAdd(slotAnntHT, &entry);
			break;

		case LSW_TYPE_FIXED_SLOTS: {
			UDATA * slotPtr = recordValue;
			slotIndex = getSlotIndex(walkState, recordValue);
			slot = &slotWalker->slots[slotIndex];

			slot->data = READ_UDATA(slotPtr);
			slot->name = lswStrDup(slotWalker, "SavedA0");

			slot--;
			slotPtr--;
			slot->data = READ_UDATA(slotPtr);
			slot->name = lswStrDup(slotWalker, "SavedPC");

			slot--;
			slotPtr--;
			slot->data = READ_UDATA(slotPtr);
			slot->type = LSW_TYPE_METHOD;
			slot->name = lswStrDup(slotWalker, "SavedMethod");

			break;
		}

		case LSW_TYPE_UNWIND_SP:
			if (recordValue) { 			
				entry.slotIndex = getSlotIndex(walkState, recordValue);
				entry.description = (char *) lswStrDup(slotWalker, "USP");
				hashTableAdd(slotAnntHT, &entry);
			}
			break;

		case LSW_TYPE_FRAME_BOTTOM:
			frame->bottomSlot = REMOTE_ADDR(recordValue);
			break;

		case LSW_TYPE_ARG_COUNT:
			frame->argCount = (UDATA) recordValue;
			break;

		case LSW_TYPE_METHOD:
			frame->method = (J9Method *) recordValue;
			break;

		case LSW_TYPE_RESOLVE_FRAME_TYPE:
			frame->ftd.resolve.type = (UDATA) recordValue;
			break;

		case LSW_TYPE_JIT_FRAME_INFO:
			frame->pc = (UDATA *) walkState->pc;
			frame->cp = (UDATA *) REMOTE_ADDR(walkState->constantPool);
			frame->ftd.jit.jitInfo = REMOTE_ADDR(walkState->jitInfo);
			frame->ftd.jit.bytecodeIndex = walkState->bytecodePCOffset;
			frame->ftd.jit.inlineDepth = walkState->inlineDepth;
			frame->ftd.jit.pcOffset = walkState->pc - (U_8 *) walkState->method->extra;
			break;

		case LSW_TYPE_FRAME_INFO:
			frame->pc = (UDATA *) walkState->pc;
			frame->cp = REMOTE_ADDR(walkState->constantPool);
			frame->frameFlags = walkState->frameFlags;
			frame->literals = (UDATA) walkState->literals;
			break;

		case LSW_TYPE_ELS:
			frame->ftd.jniCallIn.els = (UDATA) recordValue;
			break;

		default:
			abort();
	}

	return SW_ERROR_NONE;
}


IDATA
lswRecordSlot(J9StackWalkState * walkState, const void * slotAddress, UDATA slotType, const char *format, ...)
{
	IDATA slotIndex;
	J9SWSlot * slot;
	char buf[LSW_STRING_MAX];
	LSW_ACCESS_FROM_WALKSTATE(walkState);
	va_list args;
	UDATA * slotAddressUDATA = (UDATA *)slotAddress;

	LSW_ENABLED(walkState);

	slotIndex = getSlotIndex(walkState, slotAddressUDATA);
	if (slotIndex == -1) {
		return SW_ERROR_NONE;
	}

	slot = &slotWalker->slots[slotIndex];

	slot->data = READ_UDATA(slotAddressUDATA);
	slot->type = slotType;

	va_start(args, format);
	vsnprintf(buf, LSW_STRING_MAX, format, args);
	va_end(args);

	slot->name = lswStrDup(slotWalker, buf);

	return SW_ERROR_NONE;
}















static char *
lswDescribeObject(J9VMThread * vmThread, char * dataPtr, J9Class ** class, J9Object * object)
{
	J9JavaVM *vm = vmThread->javaVM;
	if (J9VMJAVALANGCLASS_OR_NULL(vm) == *class) {
#ifdef J9VM_OUT_OF_PROCESS
		/* Do not use J9VM_J9CLASS_FROM_HEAPCLASS, it will try to dbg read the local vm structure again */
		*class = (J9Class *) J9OBJECT_ADDRESS_LOAD(vm, object,
												   J9VMCONSTANTPOOL_ADDRESS_OFFSET(vm, J9VMCONSTANTPOOL_JAVALANGCLASS_VMREF));
		*class = READ_CLASS(*class);
		dataPtr += sprintf(dataPtr, "jlC: ");
#else
		/* Heap class, get the class */
		*class = J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, object);
		dataPtr += sprintf(dataPtr, "jlC: "); 
#endif
	} else {
		*class = READ_CLASS(*class);
		dataPtr += sprintf(dataPtr, "obj: ");
	} 

	return dataPtr; 
}


static void
lswDescribeSlot(J9VMThread * vmThread, J9StackWalkState * walkState, J9SWSlot * slot, char * data, UDATA dataSize)
{	
	J9Class * class; 
	J9Object * object; 
	J9UTF8 * className;
	UDATA copySize;
	char *dataPtr = data;

	data[0] = 0;

	switch (slot->type) {

		case LSW_TYPE_INDIRECT_O_SLOT:
		case LSW_TYPE_O_SLOT:
			if (slot->data) {
			
				if (slot->type == LSW_TYPE_INDIRECT_O_SLOT) {
					object = (J9Object *) READ_UDATA((UDATA *) (slot->data & ~1));
					dataPtr += sprintf(dataPtr, "%p -> ", object);
				} else {
					object = (J9Object *) slot->data;
				}
				
				object = (J9Object *) READ_OBJECT(object);
				class = J9OBJECT_CLAZZ(vmThread, object);
				
				dataPtr = lswDescribeObject(vmThread, dataPtr, &class, object);

				className = J9ROMCLASS_CLASSNAME(class->romClass);
				copySize = (J9UTF8_LENGTH(className) <= dataSize) ? J9UTF8_LENGTH(className) : LSW_STRING_MAX - 1;
				memcpy(dataPtr, J9UTF8_DATA(className), copySize);
				dataPtr[copySize] = 0;
			}
			break;
	}

	return;
}

static IDATA
lswPrintFrameSlots(J9VMThread * vmThread, J9StackWalkState * walkState, J9SWFrame * frame)
{
	UDATA i;
	IDATA topSlotIndex;
	UDATA slotCount;
	LSW_ACCESS_FROM_WALKSTATE(walkState);
	PORT_ACCESS_FROM_WALKSTATE(walkState);
	static const J9SWSlot unknownSlot = { 0xDEADBEEF, LSW_TYPE_UNKNOWN, "\x1b[31mFIXME\x1b[0m            " };
	char * stringBuf;

	if (frame->type == LSW_FRAME_TYPE_JIT_INLINE) {
		/* JIT Inline frames do not consume any slots */
		return SW_ERROR_NONE;
	}

	stringBuf = j9mem_allocate_memory(LSW_STRING_MAX, OMRMEM_CATEGORY_VM);
	if (NULL == stringBuf) {
		return SW_ERROR_NOMEM;
	}

	slotCount = frame->bottomSlot - frame->topSlot;
	topSlotIndex = getSlotIndex(walkState, frame->topSlot);

	for (i = 0; i <= slotCount; i++) {
		J9SWSlot *slot;
		J9SWSlotAnnotation entry;
		J9SWSlotAnnotation *annotation;

		entry.slotIndex = topSlotIndex + i;
		annotation = hashTableFind(slotWalker->slotAnnotations, &entry);

		slot = &slotWalker->slots[topSlotIndex + i];
		if (slot->data == 0 && NULL == slot->name) {
			slot = (J9SWSlot *) &unknownSlot;
			slot->data = READ_UDATA(frame->topSlot + i);
		}

		lswDescribeSlot(vmThread, walkState, slot, stringBuf, LSW_STRING_MAX);
#ifdef J9VM_ENV_DATA64
		lswPrintf(privatePortLibrary, "\t@%p [0x%016llx %-17s] %-4s %s\n", 
#else
		lswPrintf(privatePortLibrary, "\t@%p [0x%08lx %-17s] %-4s %s\n", 
#endif
			   frame->topSlot + i, 
			   slot->data, slot->name, 
			   annotation ? annotation->description : "",
			   stringBuf); 
	}

	j9mem_free_memory(stringBuf);

	return SW_ERROR_NONE;
}


IDATA
lswPrintFrames(J9VMThread * vmThread, J9StackWalkState * walkState)
{
	J9JavaVM *vm = vmThread->javaVM;
	LSW_ACCESS_FROM_WALKSTATE(walkState);
	J9SWFrame * frame;

	LSW_ENABLED(walkState);

	lswSanityCheck(vm, walkState);

	frame = J9_LINKED_LIST_START_DO(slotWalker->frameList); 
	while (frame) {
		char frameName[64];
		J9Method * method = READ_METHOD(frame->method);
		PORT_ACCESS_FROM_WALKSTATE(walkState);

		if (method) {
			J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
			J9UTF8 * name = J9ROMMETHOD_GET_NAME(J9_CLASS_FROM_METHOD(method)->romClass,romMethod);
			J9UTF8 * sig = J9ROMMETHOD_GET_SIGNATURE(J9_CLASS_FROM_METHOD(method)->romClass, romMethod);
			J9UTF8 * className = J9ROMCLASS_CLASSNAME(UNTAGGED_METHOD_CP(method)->ramClass->romClass);

			lswPrintf(privatePortLibrary, "[%s%-20s%s] j9method 0x%x  %s%.*s.%.*s%.*s%s\n", 
				   CYAN, getFrameName(frame, frameName), COL_RESET,
				   frame->method, CYAN,
				   (U_32) J9UTF8_LENGTH(className), J9UTF8_DATA(className), (U_32) J9UTF8_LENGTH(name), J9UTF8_DATA(name),
				   (U_32) J9UTF8_LENGTH(sig), J9UTF8_DATA(sig),
				   COL_RESET);
		} else {
			lswPrintf(privatePortLibrary, "[%s%-20s%s]\n", CYAN, getFrameName(frame, frameName), COL_RESET);
		}

		switch (frame->type) {

			case LSW_FRAME_TYPE_JIT:
			case LSW_FRAME_TYPE_JIT_INLINE:
				lswPrintf(privatePortLibrary, "\tpc = 0x%08lx  cp = 0x%08lx  jitinfo = 0x%08lx  bc index = %d  inlineDepth = %d  PC offset = 0x%lx\n", 
					   frame->pc, frame->cp, frame->ftd.jit.jitInfo, 
					   frame->ftd.jit.bytecodeIndex, frame->ftd.jit.inlineDepth, frame->ftd.jit.pcOffset);
				break;

			case LSW_FRAME_TYPE_JNI_NATIVE_METHOD:
				lswPrintf(privatePortLibrary, "\tpc (frame type) = 0x%08lx  cp = 0x%08lx  literals (obj bytes pushed) = 0x%08lx  flags = 0x%08lx\n", 
						  frame->pc, frame->cp, frame->literals, frame->frameFlags);
				break;

			case LSW_FRAME_TYPE_BYTECODE:
			case LSW_FRAME_TYPE_JNI_CALL_IN:
			default:
				lswPrintf(privatePortLibrary, "\tpc = 0x%08lx  cp = 0x%08lx  literals = 0x%08lx  flags = 0x%08lx\n", 
						  frame->pc, frame->cp, frame->literals, frame->frameFlags);
				break;
		}

		lswPrintFrameSlots(vmThread, walkState, frame);
		
		lswPrintf(privatePortLibrary, "\n");

		frame = J9_LINKED_LIST_NEXT_DO(slotWalker->frameList, frame);

	}

	return SW_ERROR_NONE;
}


static IDATA 
lswSanityCheck(J9JavaVM * vm, J9StackWalkState * walkState)
{
	LSW_ACCESS_FROM_WALKSTATE(walkState);
	J9SWFrame * frame;
	UDATA slotCount = 0;
	UDATA *lastBottomSlot = NULL;
	
	LSW_ENABLED(walkState);
	frame = J9_LINKED_LIST_START_DO(slotWalker->frameList);
	while (frame && (frame->type != LSW_FRAME_TYPE_END_OF_STACK)) {
		slotCount += (frame->bottomSlot - frame->topSlot) + 1;
		lastBottomSlot = frame->bottomSlot;
		frame = J9_LINKED_LIST_NEXT_DO(slotWalker->frameList, frame);
	}

	if (slotCount != (lastBottomSlot - slotWalker->sp + 1)) {
		PORT_ACCESS_FROM_WALKSTATE(walkState);
		lswPrintf(privatePortLibrary, "***********************************************************************\n");
		lswPrintf(privatePortLibrary, "slotCount = 0x%x  should be 0x%x\n", slotCount, lastBottomSlot - slotWalker->sp + 1);
		lswPrintf(privatePortLibrary, "***********************************************************************\n");
	}

	return SW_ERROR_NONE;
}


static const char *
getFrameName(J9SWFrame * frame, char * name)
{
	if (frame->type != LSW_FRAME_TYPE_JIT_RESOLVE) {
		return frame->name ? frame->name : (char *) getFrameTypeName(frame->type);
	} else {
		sprintf(name, "JIT Resolve (%s)", getResolveFrameType(frame->ftd.resolve.type));
	}

	return name;
}


static const char *
getFrameTypeName(UDATA frameType)
{
	/* Generic (undifferentiated) frame names. e.g. there are many jit resolve frame subtypes */

	switch (frameType) {
		case LSW_FRAME_TYPE_NATIVE_METHOD:
			return "Native Method";
		case LSW_FRAME_TYPE_JNI_NATIVE_METHOD:
			return "JNI Native Method";
		case LSW_FRAME_TYPE_JIT_JNI_CALLOUT:
			return "JIT JNI Callout";
		case LSW_FRAME_TYPE_METHOD:
			return "Method";
		case LSW_FRAME_TYPE_JIT_RESOLVE:
			return "JIT Resolve";
		case LSW_FRAME_TYPE_JIT:
			return "JIT";
		case LSW_FRAME_TYPE_JIT_INLINE:
			return "JIT Inline";
		case LSW_FRAME_TYPE_GENERIC_SPECIAL:
			return "Generic Special";
		case LSW_FRAME_TYPE_BYTECODE:
			return "Bytecode";
		case LSW_FRAME_TYPE_JNI_CALL_IN:
			return "JNI Call In";
		case LSW_FRAME_TYPE_METHODTYPE:
			return "JSR 292 MethodType";
		default:
			return "UNKNOWN";
	}

	return NULL;
}

static const char *
getResolveFrameType(UDATA resolveFrameType)
{
	switch (resolveFrameType) {
		case J9_STACK_FLAGS_JIT_GENERIC_RESOLVE:                return "Generic";
		case J9_STACK_FLAGS_JIT_STACK_OVERFLOW_RESOLVE_FRAME:   return "Stack overflow";
		case J9_STACK_FLAGS_JIT_DATA_RESOLVE:                   return "Data";
		case J9_STACK_FLAGS_JIT_RUNTIME_HELPER_RESOLVE:         return "Runtime helper";
		case J9_STACK_FLAGS_JIT_LOOKUP_RESOLVE:                 return "Interface lookup";
		case J9_STACK_FLAGS_JIT_INTERFACE_METHOD_RESOLVE:       return "Interface method";
		case J9_STACK_FLAGS_JIT_SPECIAL_METHOD_RESOLVE:         return "Special method";
		case J9_STACK_FLAGS_JIT_STATIC_METHOD_RESOLVE:          return "Static method";
		case J9_STACK_FLAGS_JIT_VIRTUAL_METHOD_RESOLVE:         return "Virtual method";
		case J9_STACK_FLAGS_JIT_RECOMPILATION_RESOLVE:          return "Recompilation";
		case J9_STACK_FLAGS_JIT_MONITOR_ENTER_RESOLVE:          return "Monitor enter";
		case J9_STACK_FLAGS_JIT_ALLOCATION_RESOLVE:             return "Allocation";
		case J9_STACK_FLAGS_JIT_BEFORE_ANEWARRAY_RESOLVE:       return "Before anewarray";
		case J9_STACK_FLAGS_JIT_BEFORE_MULTIANEWARRAY_RESOLVE:  return "Before multianewarray";
	}

	return "Unknown";
}

static IDATA
getSlotIndex(J9StackWalkState * walkState, UDATA * addr)
{		
	UDATA slotIndex;
	LSW_ACCESS_FROM_WALKSTATE(walkState);

	slotIndex = addr - slotWalker->sp;

	if (slotIndex > ((UDATA) slotWalker->stackBottom - (UDATA) slotWalker->sp)) {
		PORT_ACCESS_FROM_WALKSTATE(walkState);
		lswPrintf(privatePortLibrary, "OUT OF BOUNDS LSW SLOT ACCESS [addr=%p not between %p and %p]\n", addr, slotWalker->stackBottom, slotWalker->sp);
		return -1;
	}

	return slotIndex;
}



static IDATA
allocateStringPoolSlab(J9SlotWalker * slotWalker)
{
	slotWalker->stringSlab = pool_newElement(slotWalker->stringPool);
	if (NULL == slotWalker->stringSlab) {
		return SW_ERROR_STRING_POOL_NOMEM;
	}
	slotWalker->stringFree = slotWalker->stringSlab; 

	return SW_ERROR_NONE;
}

static char *
lswStrDup(J9SlotWalker * slotWalker, char * str)
{
	UDATA stringSize;
	char * stringFree;

	stringSize = strlen(str);

	if ((NULL == slotWalker->stringSlab) || (stringSize >= (UDATA)(slotWalker->stringSlab + LSW_STRING_POOL_MAX - slotWalker->stringFree))) {
		if (allocateStringPoolSlab(slotWalker) != SW_ERROR_NONE) {
			return NULL;
		}
	}

	stringFree = slotWalker->stringFree;
	slotWalker->stringFree += stringSize + 1; 
	strcpy(stringFree, str);

	return stringFree;
}


 
static void
lswPrintf(struct J9PortLibrary *privatePortLibrary, const char *format, ...)
{
	char buf[LSW_STRING_MAX];
	va_list args;

	va_start(args, format);
	vsnprintf(buf, LSW_STRING_MAX, format, args);
	va_end(args);

	j9tty_printf(privatePortLibrary, buf);
}


