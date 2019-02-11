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

#include "dmpsup.h"
#include "j9.h"
#include "j9dmpnls.h"
#include "j2sever.h"
#include "HeapIteratorAPI.h"

struct J9RASDumpdumpStats {
	J9MM_IterateRegionDescriptor *regionDescriptor;
	j9object_t lastObject;
	UDATA nArrays;
	UDATA nClasses;
	UDATA nNulls;
	UDATA nObjects;
	UDATA nPrimitives;
	UDATA nReferences;
	UDATA nTotal;
};

typedef struct J9RASHeapdumpContext {
	J9JavaVM *vm;
	J9RASdumpContext *context;
	J9RASdumpAgent *agent;
	IDATA fd;
	struct J9RASDumpdumpStats stats;
	char label[J9_MAX_DUMP_PATH]; /* filename passed in, including %id on realtime */
	char filename[J9_MAX_DUMP_PATH]; /* generated filename once tokens expanded */
} J9RASHeapdumpContext;

#define allClassesStartDo(vm, state, loader) \
	vm->internalVMFunctions->allClassesStartDo(state, vm, loader)

#define allClassesNextDo(vm, state) \
	vm->internalVMFunctions->allClassesNextDo(state)

#define allClassesEndDo(vm, state) \
	vm->internalVMFunctions->allClassesEndDo(state)

static UDATA openHeapdumpFile(J9RASHeapdumpContext *ctx, const char *label);
static void closeHeapdumpFile(J9RASHeapdumpContext *ctx);
static void writeMultipleHeapdumps(J9RASHeapdumpContext *ctx);
static void writeSingleHeapdump(J9RASHeapdumpContext *ctx);
static void writeVersion(J9RASHeapdumpContext *ctx);
static void writeClasses(J9RASHeapdumpContext *ctx);
static void writeTotals(J9RASHeapdumpContext *ctx);
static void writeObject(J9RASHeapdumpContext *ctx, j9object_t obj);
static void writeReference(J9RASHeapdumpContext *ctx, j9object_t obj, j9object_t ref);
static void print(J9RASHeapdumpContext *ctx, const char *format, ...);
static void printType(J9RASHeapdumpContext *ctx, j9object_t obj);
static jvmtiIterationControl hdClassicMultiHeapIteratorCallback(J9JavaVM *vm, J9MM_IterateHeapDescriptor *heapDescriptor, void *userData);
static jvmtiIterationControl hdClassicMultiSpaceIteratorCallback(J9JavaVM *vm, J9MM_IterateSpaceDescriptor *spaceDescriptor, void *userData);
static jvmtiIterationControl hdClassicHeapIteratorCallback(J9JavaVM *vm, J9MM_IterateHeapDescriptor *heapDescriptor, void *userData);
static jvmtiIterationControl hdClassicSpaceIteratorCallback(J9JavaVM *vm, J9MM_IterateSpaceDescriptor *spaceDescriptor, void *userData);
static jvmtiIterationControl hdClassicRegionIteratorCallback(J9JavaVM *vm, J9MM_IterateRegionDescriptor *regionDescriptor, void *userData);
static jvmtiIterationControl hdClassicObjectIteratorCallback(J9JavaVM *vm, J9MM_IterateObjectDescriptor *objectDesc, void *userData);
static jvmtiIterationControl hdClassicObjectRefIteratorCallback(J9JavaVM *javaVM, J9MM_IterateObjectDescriptor *objectDesc, J9MM_IterateObjectRefDescriptor *refDesc, void *userData);
void writeClassicHeapdump(const char *label, J9RASdumpContext *context, J9RASdumpAgent *agent);

void
writeClassicHeapdump(const char *label, J9RASdumpContext *context, J9RASdumpAgent *agent)
{
	J9RASHeapdumpContext ctx;
	UDATA len = strlen(label);
	
	memset(&ctx, 0, sizeof(ctx));
	ctx.vm = context->javaVM;
	ctx.context = context;
	ctx.agent = agent;

	strncpy(ctx.label, label, sizeof(ctx.label));
	
	/* Re-label if necessary */
	if (len >= 4 && strcmp(&ctx.label[len - 4], ".phd") == 0) {
		strcpy(&ctx.label[len - 4], ".txt");
	}




	if (agent->requestMask & J9RAS_DUMP_DO_MULTIPLE_HEAPS) {
		writeMultipleHeapdumps(&ctx);
	} else {
		writeSingleHeapdump(&ctx);
	}
}

static UDATA
openHeapdumpFile(J9RASHeapdumpContext *ctx, const char *label)
{
	PORT_ACCESS_FROM_JAVAVM(ctx->vm);
	
	memset(&(ctx->stats), 0, sizeof(ctx->stats));

	j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_REQUESTING_DUMP_STR, "Heap", label);
	
	ctx->fd = j9file_open(label, EsOpenWrite | EsOpenCreate | EsOpenTruncate | EsOpenCreateNoTag, 0666);
	if (ctx->fd == -1) {
		j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_DMP_ERROR_IN_DUMP_STR, "Heap", label);
		return 1;
	}
	
	return 0;
}

static void
closeHeapdumpFile(J9RASHeapdumpContext *ctx)
{
	PORT_ACCESS_FROM_JAVAVM(ctx->vm);
	
	if (ctx->fd != -1) {
		j9file_close(ctx->fd);
		ctx->fd = -1;
		j9nls_printf(PORTLIB, J9NLS_INFO | J9NLS_STDERR, J9NLS_DMP_WRITTEN_DUMP_STR, "Heap", ctx->filename);
	} else {
		j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_STDERR, J9NLS_DMP_ERROR_IN_DUMP_STR, "Heap", ctx->filename);
	}
}

static void
writeMultipleHeapdumps(J9RASHeapdumpContext *ctx)
{
	PORT_ACCESS_FROM_JAVAVM(ctx->vm);
	ctx->vm->memoryManagerFunctions->j9mm_iterate_heaps(ctx->vm, PORTLIB, 0, hdClassicMultiHeapIteratorCallback, ctx);
}

static void
writeSingleHeapdump(J9RASHeapdumpContext *ctx)
{
	PORT_ACCESS_FROM_JAVAVM(ctx->vm);
	
	strncpy(ctx->filename, ctx->label, sizeof(ctx->filename));
	
	if (openHeapdumpFile(ctx, ctx->filename)) {
		return;
	}

	writeVersion(ctx);
	ctx->vm->memoryManagerFunctions->j9mm_iterate_heaps(ctx->vm, PORTLIB, 0, hdClassicHeapIteratorCallback, ctx);
	writeClasses(ctx);
	writeTotals(ctx);
	
	closeHeapdumpFile(ctx);
}

static jvmtiIterationControl
hdClassicMultiHeapIteratorCallback(J9JavaVM *vm, J9MM_IterateHeapDescriptor *heapDescriptor, void *userData)
{
	vm->memoryManagerFunctions->j9mm_iterate_spaces(vm, vm->portLibrary, heapDescriptor, 0, hdClassicMultiSpaceIteratorCallback, userData);
	return JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
hdClassicMultiSpaceIteratorCallback(J9JavaVM *vm, J9MM_IterateSpaceDescriptor *spaceDescriptor, void *userData)
{
	J9RASHeapdumpContext *ctx = (J9RASHeapdumpContext *) userData;
	char insert[64];
	char *to = ctx->filename;
	char *cursor = ctx->label;
	char *end = cursor + strlen(ctx->label);
	PORT_ACCESS_FROM_JAVAVM(ctx->vm);
	
	memset(&(ctx->filename), 0, sizeof(ctx->filename));

	j9str_printf(PORTLIB, insert, sizeof(insert), "%s%.*zX", spaceDescriptor->name, sizeof(UDATA) * 2, spaceDescriptor->id);
	
	/* Generate the filename to be written to by substituting "%id" for insert */
	while (cursor < end) {
		if (cursor == strstr(cursor, "%id")) {
			/* found the %id insertion point */
			strcpy(to, insert);
			to += strlen(insert);
			cursor += 3;
		} else {
			*to++ = *cursor++;
		}
	}
	
	if (openHeapdumpFile(ctx, ctx->filename)) {
		return JVMTI_ITERATION_CONTINUE;
	}
	
	writeVersion(ctx);
	vm->memoryManagerFunctions->j9mm_iterate_regions(vm, vm->portLibrary, spaceDescriptor, 0, hdClassicRegionIteratorCallback, userData);
	writeClasses(ctx);
	writeTotals(ctx);
	
	closeHeapdumpFile(ctx);
	return JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
hdClassicHeapIteratorCallback(J9JavaVM *vm, J9MM_IterateHeapDescriptor *heapDescriptor, void *userData)
{
	vm->memoryManagerFunctions->j9mm_iterate_spaces(vm, vm->portLibrary, heapDescriptor, 0, hdClassicSpaceIteratorCallback, userData);
	return JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
hdClassicSpaceIteratorCallback(J9JavaVM *vm, J9MM_IterateSpaceDescriptor *spaceDescriptor, void *userData)
{
	vm->memoryManagerFunctions->j9mm_iterate_regions(vm, vm->portLibrary, spaceDescriptor, 0, hdClassicRegionIteratorCallback, userData);
	return JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
hdClassicRegionIteratorCallback(J9JavaVM *vm, J9MM_IterateRegionDescriptor *regionDescriptor, void *userData)
{
	J9RASHeapdumpContext *ctx = (J9RASHeapdumpContext *) userData;
	ctx->stats.regionDescriptor = regionDescriptor;
	
	vm->memoryManagerFunctions->j9mm_iterate_region_objects(vm, vm->portLibrary, regionDescriptor, 0, hdClassicObjectIteratorCallback, userData);
	return JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
hdClassicObjectIteratorCallback(J9JavaVM *vm, J9MM_IterateObjectDescriptor *objectDesc, void *userData)
{
	J9RASHeapdumpContext *ctx = (J9RASHeapdumpContext *) userData;
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (J9VM_IS_INITIALIZED_HEAPCLASS_VM(vm, objectDesc->object)) {
		/* Heap classes are handled by a separate walk */
		return JVMTI_ITERATION_CONTINUE;
	}



	writeObject(ctx, objectDesc->object);

	vm->memoryManagerFunctions->j9mm_iterate_object_slots(vm, PORTLIB, objectDesc, 0, hdClassicObjectRefIteratorCallback, userData);

	return JVMTI_ITERATION_CONTINUE;
}

static jvmtiIterationControl
hdClassicObjectRefIteratorCallback(J9JavaVM *vm, J9MM_IterateObjectDescriptor *objectDesc, J9MM_IterateObjectRefDescriptor *refDesc, void *userData)
{
	J9RASHeapdumpContext *ctx = (J9RASHeapdumpContext*)userData;

	j9object_t obj = objectDesc->object;
	j9object_t ref = refDesc->object;

	writeReference(ctx, obj, ref);

	return JVMTI_ITERATION_CONTINUE;
}

static void
writeVersion(J9RASHeapdumpContext *ctx)
{
	J9JavaVM *vm = ctx->vm;
	const char *formatString = "// Version: %s";

	if( vm->j9ras->serviceLevel != NULL ) {
		print(ctx,formatString,  vm->j9ras->serviceLevel);
	}
}

static void
writeClasses(J9RASHeapdumpContext *ctx)
{
	J9JavaVM *vm = ctx->vm;
	
	J9ClassWalkState state;
	PORT_ACCESS_FROM_JAVAVM(vm);
	
	J9Class *clazz = allClassesStartDo(vm, &state, NULL);
	
	while (clazz) {
		/* Ignore redefined and dying classes */
		if (!(J9CLASS_FLAGS(clazz) & J9AccClassHotSwappedOut)
		 && !(J9CLASS_FLAGS(clazz) & J9AccClassDying)) {
			j9object_t currentObject = J9VM_J9CLASS_TO_HEAPCLASS(clazz);
			
			if (J9VM_IS_INITIALIZED_HEAPCLASS_VM(vm, currentObject)) {
				J9MM_IterateObjectDescriptor objectDescriptor;
				j9object_t* staticReferencePointer = (j9object_t*)(clazz->ramStatics);
				int staticReferenceCount = clazz->romClass->objectStaticCount;
				int j = 0;
				BOOLEAN writeClass = TRUE;



				if (writeClass) {
					writeObject(ctx, currentObject);

					vm->memoryManagerFunctions->j9mm_initialize_object_descriptor(vm, &objectDescriptor, currentObject);
					vm->memoryManagerFunctions->j9mm_iterate_object_slots(vm, PORTLIB, &objectDescriptor, 0, hdClassicObjectRefIteratorCallback, ctx);

					/* Write the static references */
					for (j = 0; j < staticReferenceCount; j++) {
						writeReference(ctx, currentObject, staticReferencePointer[j]);
					}
				}
			}
		}
		clazz = allClassesNextDo(vm, &state);
	}
	allClassesEndDo(vm, &state);
}

static void
writeTotals(J9RASHeapdumpContext *ctx)
{
	print(ctx, "\n");
	print(ctx, "// Breakdown - Classes: %zu, Objects: %zu, ObjectArrays: %zu, PrimitiveArrays: %zu\n", ctx->stats.nClasses, ctx->stats.nObjects, ctx->stats.nArrays, ctx->stats.nPrimitives);
	print(ctx, "// EOF:  Total 'Objects',Refs(null) : %zu,%zu(%zu)\n", ctx->stats.nTotal, ctx->stats.nReferences, ctx->stats.nNulls);
}

static void
writeObject(J9RASHeapdumpContext *ctx, j9object_t obj)
{
	J9JavaVM *vm = ctx->vm;
	UDATA size;

	if (obj != ctx->stats.lastObject) {
		if (obj) {
			size = vm->memoryManagerFunctions->j9gc_get_object_size_in_bytes(vm, obj);

			print(ctx, "\n0x%p [%zu] ", obj, size);
			printType(ctx, obj);
			print(ctx, "\n\t");
			ctx->stats.nTotal++;
		}
		ctx->stats.lastObject = obj;
	}
}

static void
writeReference(J9RASHeapdumpContext *ctx, j9object_t obj, j9object_t ref)
{
	if (obj != ctx->stats.lastObject) {
		writeObject(ctx, obj);
	}
	
	if (ref) {
		print(ctx, "0x%p ", ref);
	} else {
		ctx->stats.nNulls++;
	}
	ctx->stats.nReferences++;
}

static void
printType(J9RASHeapdumpContext *ctx, j9object_t obj)
{
	J9JavaVM *vm = ctx->vm;
	UDATA tally = 0;
	J9Class *clazz;

	if (J9VM_IS_INITIALIZED_HEAPCLASS_VM(vm, obj)) {
		clazz = J9VM_J9CLASS_FROM_HEAPCLASS_VM(vm, obj);
		print(ctx, "CLS ");
		ctx->stats.nClasses++;
	} else {
		clazz = J9OBJECT_CLAZZ_VM(vm, obj);
		print(ctx, "OBJ ");
		tally = 1;
	}

	if (J9ROMCLASS_IS_ARRAY(clazz->romClass)) {
		J9ArrayClass *array = (J9ArrayClass *) clazz;
		J9ROMClass *leafType;
		J9Class *leafClass;
		UDATA n;

		for (n = array->arity; n > 1; n--) {
			print(ctx, "[");
		}

		leafClass = array->leafComponentType;
		print(ctx, "%.*s", J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(leafClass->arrayClass->romClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(leafClass->arrayClass->romClass)));

		leafType = leafClass->romClass;
		if (J9ROMCLASS_IS_PRIMITIVE_TYPE(leafType)) {
			ctx->stats.nPrimitives += tally;
		} else {
			print(ctx, "%.*s;", J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(leafType)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(leafType)));
			ctx->stats.nArrays += tally;
		}
	} else {
		print(ctx, "%.*s", J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(clazz->romClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(clazz->romClass)));
		ctx->stats.nObjects += tally;
	}
}

static void
print(J9RASHeapdumpContext *ctx, const char *format, ...)
{
	va_list args;
	PORT_ACCESS_FROM_JAVAVM(ctx->vm);
	
	va_start(args, format);
	j9file_vprintf(ctx->fd, format, args);
	va_end(args);
}
