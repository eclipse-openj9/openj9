/*******************************************************************************
 * Licensed Materials - Property of IBM
 * "Restricted Materials of IBM"
 * 
 * (c) Copyright IBM Corp. 2017 All Rights Reserved
 * 
 * US Government Users Restricted Rights - Use, duplication or disclosure
 * restricted by GSA ADP Schedule Contract with IBM Corp.
 ********************************************************************************/

#include "j9.h"
#include "j9protos.h"
#include "ut_j9vm.h"
#ifdef J9VM_OPT_PANAMA
#include "NativeCalloutHelpers.h"

static UDATA NativeCalloutDataHashFn(void *key, void *userData);
static UDATA NativeCalloutDataHashEqualFn(void *tableNode, void *queryNode, void *userData);

static UDATA
NativeCalloutDataHashFn(void *key, void *userData)
{
	J9NativeCalloutData *entry = (J9NativeCalloutData*) key;

	return (UDATA)entry;
}

static UDATA
NativeCalloutDataHashEqualFn(void *tableNode, void *queryNode, void *userData)
{
	J9NativeCalloutData *tableNodePatchPath = (J9NativeCalloutData *)tableNode;
	J9NativeCalloutData *queryNodePatchPath = (J9NativeCalloutData *)queryNode;

	return (tableNodePatchPath == queryNodePatchPath);
}

J9HashTable *
hashNativeCalloutDataNew(J9JavaVM *javaVM, U_32 initialSize)
{
	U_32 flags = J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION;

	return hashTableNew(OMRPORT_FROM_J9PORT(javaVM->portLibrary), J9_GET_CALLSITE(), initialSize, sizeof(J9NativeCalloutData), sizeof(char *), flags, J9MEM_CATEGORY_MODULES, NativeCalloutDataHashFn, NativeCalloutDataHashEqualFn, NULL, javaVM);
}
#endif /* J9VM_OPT_PANAMA */
