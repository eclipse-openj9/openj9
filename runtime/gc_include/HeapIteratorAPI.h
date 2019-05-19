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


/**
 * @file
 * @ingroup GC_Include
 */

#if !defined(HEAPITERATORAPI_H_)
#define HEAPITERATORAPI_H_

/* @ddr_namespace: default */
#include "j9.h"
#include "j9modron.h"
#include "RootScannerTypes.h"
#include "jvmti.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Scan Flags
 */
#define SCAN_CLASSES 0x00001
#define SCAN_VM_CLASS_SLOTS  0x00002
#define SCAN_CLASS_LOADERS  0x00004
#define SCAN_THREADS  0x00008
#define SCAN_FINALIZABLE_OBJECTS  0x00010
#define SCAN_JNI_GLOBAL  0x00020
#define SCAN_STRING_TABLE 0x00040
#define SCAN_UNUSED_1 0x00080
#define SCAN_UNUSED_2 0x00100
#define SCAN_UNUSED_3 0x00200
#define SCAN_UNFINALIZABLE 0x00400
#define SCAN_MONITORS 0x00800
#define SCAN_JNI_WEAK 0x01000
#define SCAN_DEBUGGER 0x02000
#define SCAN_DEBUGGER_CLASS_REF 0x04000
#define SCAN_REMEBERED_SET 0x08000
#define SCAN_JVMTI_OBJECT_TAG_TABLE	0x10000
#define SCAN_OWNABLE_SYNCHRONIZER 0x20000
#define SCAN_ALL 0x3FFFF

#define HEAP_ROOT_SLOT_DESCRIPTOR_OBJECT 0
#define HEAP_ROOT_SLOT_DESCRIPTOR_CLASS 1

typedef enum RootScannerEntityReachability {
	RootScannerEntityReachability_None = 0,
	RootScannerEntityReachability_Strong,
	RootScannerEntityReachability_Weak
} RootScannerEntityReachability;

/**
 * Iterator directives (placeholders for now)
 */
typedef enum J9MM_IteratorHeapType {
	j9mm_iterator_heap_type_all = 0,
	/* ensure 4-byte enum */
	j9mm_iterator_heap_type_max = 0x1000000
} J9MM_IteratorHeapType;

typedef enum J9MM_IteratorSpaceType {
	j9mm_iterator_space_type_all = 0,
	/* ensure 4-byte enum */
	j9mm_iterator_space_type_max = 0x1000000
} J9MM_IteratorSpaceType;

typedef enum J9MM_IteratorRegionType {
	j9mm_iterator_region_type_all = 0,
	/* ensure 4-byte enum */
	j9mm_iterator_region_type_max = 0x1000000
} J9MM_IteratorRegionType;

typedef enum J9MM_IteratorObjectType {
	j9mm_iterator_object_type_all = 0,
	/* ensure 4-byte enum */
	j9mm_iterator_object_type_max = 0x1000000
} J9MM_IteratorObjectType;

typedef enum J9MM_IteratorObjectRefType {
	j9mm_iterator_object_ref_type_object = 1,
	j9mm_iterator_object_ref_type_arraylet_leaf = 2,
	/* ensure 4-byte enum */
	j9mm_iterator_object_ref_type_max = 0x1000000
} J9MM_IteratorObjectRefType;

typedef enum J9MM_IteratorFlags {
	j9mm_iterator_flag_none = 0,
	j9mm_iterator_flag_include_holes = 1, /**< Indicates that holes (unused portions of the region) should be included in the object iterators */
	j9mm_iterator_flag_include_arraylet_leaves = 2, /**< Indicates that arraylet leaf pointers should be included in the object ref iterators */
	j9mm_iterator_flag_exclude_null_refs = 4, /**< Indicates that NULL pointers should be excluded in the object ref iterators */
	j9mm_iterator_flag_regions_read_only = 8, /**< Indicates that it is read only request (no TLH flush and further heap walk) */
	j9mm_iterator_flag_max = 0x1000000
} J9MM_IteratorFlags;

/**
 * Descriptor declarations (what is handed to the user functions).  None of the strings for identifiers are carved in stone.
 * The structure and its contents are only guaranteed for the lifetime of the user function call.
 */
typedef struct J9MM_IterateHeapDescriptor {
	const char *name; /**< Name of the Heap */
	UDATA id; /**< Unique identifier */
} J9MM_IterateHeapDescriptor;

typedef struct J9MM_IterateSpaceDescriptor {
	const char *name; /**< Name of the space */
	UDATA id; /**< Unique identifier */
	UDATA classPointerOffset; /**< Offset from the beginning of an object where the class pointer resides */
	UDATA classPointerSize; /**< Size of the class pointer, in bytes */
	UDATA fobjectPointerScale; /**< Multiplier for compressed pointer decompression */
	UDATA fobjectPointerDisplacement; /**< Displacement to add for compressed pointer decompression */
	UDATA fobjectSize; /**< sizeof(fj9object_t) */
	void* memorySpace; /**< the memory space struct associated with this space, or NULL if none. This is temporary. */
} J9MM_IterateSpaceDescriptor;

typedef struct J9MM_IterateRegionDescriptor {
	const char *name; /**< Name of the Heap */
	UDATA id; /**< Unique identifier */
	UDATA objectAlignment; /**< Object alignment for this region */
	UDATA objectMinimumSize; /**< Object minimum size for this region */
	const void *regionStart; /**< Address of the start of the region */
	UDATA regionSize; /**< The size (in bytes) of the region */
} J9MM_IterateRegionDescriptor;

typedef struct J9MM_IterateObjectDescriptor {
	UDATA id; /**< Unique identifier */
	UDATA size; /**< Size in bytes that the object (or hole) consumes */
	j9object_t object; /**< Pointer to the actual J9Object (or hole).  This is temporary */
	UDATA isObject; /**< TRUE if this is an object, or FALSE if it's a hole */
} J9MM_IterateObjectDescriptor;

typedef struct J9MM_IterateObjectRefDescriptor {
	UDATA id; /**< Unique identifier */
	j9object_t object; /**< Pointer to the actual j9object_t.  If this value is modified it is stored back into the field once the callback returns */
	const fj9object_t *fieldAddress; /**< The address of the field within the object */
	J9MM_IteratorObjectRefType type; /**< The type of reference */
} J9MM_IterateObjectRefDescriptor;

typedef struct J9MM_HeapRootSlotDescriptor {
	UDATA slotType; /**< Type of slot */
	UDATA scanType; /**< What is being scanned, class or object */
	UDATA slotReachability; /**< Reachability of slot */
} J9MM_HeapRootSlotDescriptor;

typedef jvmtiIterationControl (*rootIteratorCallBackFunc)(void* ptr, J9MM_HeapRootSlotDescriptor *rootDesc, void *userData);

/**
 * Walk the roots within a heap, call user provided function
 * @param flags The flags describing the walk (unused currently)
 * @param func The function to call on each heap descriptor.
 * @param userData Pointer to storage for userData.
 */
jvmtiIterationControl
j9mm_iterate_roots(J9JavaVM *javaVM, J9PortLibrary *portLibrary, UDATA flags, rootIteratorCallBackFunc callBackFunc, void *userData);

/**
 * Walk all heaps, call user provided function.
 *
 * The caller must have VM access.
 *
 * This function may acquire locks during its execution to protect the list of heaps.
 * These locks may be held while the callback function is executed.
 *
 * @param flags The flags describing the walk (unused currently)
 * @param func The function to call on each heap descriptor.
 * @param userData Pointer to storage for userData.
 */
jvmtiIterationControl
j9mm_iterate_heaps(J9JavaVM *vm, J9PortLibrary *portLibrary, UDATA flags, jvmtiIterationControl (*func)(J9JavaVM *vm, J9MM_IterateHeapDescriptor *heapDesc, void *userData), void *userData);

/**
 * Walk all spaces contained in the heap, call user provided function.
 *
 * The caller must have VM access.
 *
 * This function may acquire locks during its execution to protect the list of spaces.
 * These locks may be held while the callback function is executed.
 *
 * @param flags The flags describing the walk (unused currently)
 * @param func The function to call on each heap descriptor.
 * @param userData Pointer to storage for userData.
 */
jvmtiIterationControl
j9mm_iterate_spaces(J9JavaVM *vm, J9PortLibrary *portLibrary, J9MM_IterateHeapDescriptor *heap, UDATA flags, jvmtiIterationControl (*func)(J9JavaVM *vm, J9MM_IterateSpaceDescriptor *spaceDesc, void *userData), void *userData);

/**
 * Walk all regions for the given space, call user provided function.
 *
 * The caller must have VM access.
 *
 * This function may acquire locks during its execution to protect the list of regions.
 * These locks may be held while the callback function is executed.
 *
 * @param space The descriptor for the space that should be walked
 * @param flags The flags describing the walk (unused currently)
 * @param func The function to call on each region descriptor.
 * @param userData Pointer to storage for userData.
 */
jvmtiIterationControl
j9mm_iterate_regions(J9JavaVM *vm, J9PortLibrary *portLibrary, J9MM_IterateSpaceDescriptor *space, UDATA flags, jvmtiIterationControl (*func)(J9JavaVM *vm, J9MM_IterateRegionDescriptor *regionDesc, void *userData), void *userData);

/**
 * Walk all objects for the given region, call user provided function.
 *
 * The caller must have exclusive VM access.
 *
 * @param region The descriptor for the region that should be walked
 * @param flags The flags describing the walk (0 or j9mm_iterator_flag_include_holes)
 * @param func The function to call on each object descriptor.
 * @param userData Pointer to storage for userData.
 */
jvmtiIterationControl
j9mm_iterate_region_objects(J9JavaVM *vm, J9PortLibrary *portLibrary, J9MM_IterateRegionDescriptor *region, UDATA flags, jvmtiIterationControl (*func)(J9JavaVM *vm, J9MM_IterateObjectDescriptor *objectDesc, void *userData), void *userData);

/**
 * Walk all object slots for the given object, call user provided function.
 * @param object The descriptor for the object that should be walked
 * @param flags The flags describing the walk (0 or any combination of j9mm_iterator_flag_include_arraylet_leaves and j9mm_iterator_flag_exclude_null_refs)
 * @param func The function to call on each object descriptor.
 * @param userData Pointer to storage for userData.
 */
jvmtiIterationControl
j9mm_iterate_object_slots(J9JavaVM *javaVM, J9PortLibrary *portLibrary, J9MM_IterateObjectDescriptor *object, UDATA flags, jvmtiIterationControl (*func)(J9JavaVM *javaVM, J9MM_IterateObjectDescriptor *objectDesc, J9MM_IterateObjectRefDescriptor *refDesc, void *userData), void *userData);

/**
 * Provide the arraylet identification bitmask
 * @return arrayletPageSize
 * @return offset
 * @return width
 * @return mask
 * @return result
 * @return 0 on success, non-0 on failure.
 */
UDATA
j9mm_arraylet_identification(J9JavaVM *javaVM, UDATA* arrayletLeafSize, UDATA *offset, UDATA *width, UDATA *mask, UDATA *result);

/**
 * Initialize a descriptor for the specified object.
 * This descriptor may subsequently be used with j9mm_iterate_object_slots or other iterator APIs.
 *
 * @return descriptor The descriptor to be initialized
 * @param object The object to store into the descriptor
 */
void
j9mm_initialize_object_descriptor(J9JavaVM *javaVM, J9MM_IterateObjectDescriptor *descriptor, j9object_t object);

/**
 * Walk all objects for the given VM, call user provided function.
 * @param flags The flags describing the walk (0 or j9mm_iterator_flag_include_holes)
 * @param func The function to call on each object descriptor.
 * @param userData Pointer to storage for userData.
 */
jvmtiIterationControl
j9mm_iterate_all_objects(J9JavaVM *vn, J9PortLibrary *portLibrary, UDATA flags, jvmtiIterationControl (*func)(J9JavaVM *vm, J9MM_IterateObjectDescriptor *object, void *userData), void *userData);

/**
 * Walk all ownable synchronizer object, call user provided function.
 * @param flags The flags describing the walk (unused currently)
 * @param func The function to call on each object descriptor.
 * @param userData Pointer to storage for userData.
 * @return return 0 on successfully iterating entire list, return user provided function call if it did not return JVMTI_ITERATION_CONTINUE
 */
jvmtiIterationControl
j9mm_iterate_all_ownable_synchronizer_objects(J9VMThread *vmThread, J9PortLibrary *portLibrary, UDATA flags, jvmtiIterationControl (*func)(J9VMThread *vmThread, J9MM_IterateObjectDescriptor *object, void *userData), void *userData);

/**
 * Shortcut specific for Segregated heap to find the page the pointer belongs to
 * This is instead of iterating pages, which may be very time consuming.
 * @param objectDescriptor Structure containing pointer to an object
 * @param regionDesc Out parameter initialized by the method if the region is found
 */
UDATA
j9mm_find_region_for_pointer(J9JavaVM* javaVM, void *pointer, J9MM_IterateRegionDescriptor *regionDesc);

/**
 * Convert the memory that is currently represented as an object to dark matter.
 * This routine is typically used by GC tools such as gccheck and tgc to ensure that only valid (i.e. reachable)
 * objects remain on the heap after a sweep. A 'dead' object may contain dangling references to heap addresses
 * which no longer contain walkable objects. The problem is solved by converting the invalid objects to dark matter.
 *
 * @param[in] javaVM the JavaVM
 * @param[in] region the region in which the object exists
 * @param[in] objectDesc the object to be abandoned
 * @return 0 on success, non-0 on failure.
 */
UDATA
j9mm_abandon_object(J9JavaVM *javaVM, J9MM_IterateRegionDescriptor *region, J9MM_IterateObjectDescriptor *objectDesc);

#ifdef __cplusplus
} /* extern "C" { */
#endif /* __cplusplus */

#endif /* HEAPITERATORAPI_H_ */
