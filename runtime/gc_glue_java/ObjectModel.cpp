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

#include <string.h>

#include "ObjectModel.hpp"
#include "GCExtensionsBase.hpp"
#include "ModronAssertions.h"

bool
GC_ObjectModel::initialize(MM_GCExtensionsBase *extensions)
{
	bool result = true;
	_javaVM = (J9JavaVM *)extensions->getOmrVM()->_language_vm;
	
	_mixedObjectModel = &(extensions->mixedObjectModel);
	_indexableObjectModel = &(extensions->indexableObjectModel);

#if defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS)
	getObjectModelDelegate()->setCompressObjectReferences(extensions->compressObjectReferences());
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS) */
	getObjectModelDelegate()->setMixedObjectModel(_mixedObjectModel);
	getObjectModelDelegate()->setArrayObjectModel(_indexableObjectModel);

	_classClass = NULL;
	_classLoaderClass = NULL;
	_atomicMarkableReferenceClass = NULL;
	
	J9HookInterface **vmHookInterface = _javaVM->internalVMFunctions->getVMHookInterface(_javaVM);
	if (NULL == vmHookInterface) {
		result = false;
	} else if ((*vmHookInterface)->J9HookRegisterWithCallSite(vmHookInterface, J9HOOK_VM_INTERNAL_CLASS_LOAD, internalClassLoadHook, OMR_GET_CALLSITE(), this)) {
		result = false;
	} else if ((*vmHookInterface)->J9HookRegisterWithCallSite(vmHookInterface, J9HOOK_VM_CLASSES_REDEFINED, classesRedefinedHook, OMR_GET_CALLSITE(), this)) {
		result = false;
	}
	
	return result;
}

void
GC_ObjectModel::tearDown(MM_GCExtensionsBase *extensions)
{
	J9HookInterface **vmHookInterface = _javaVM->internalVMFunctions->getVMHookInterface(_javaVM);
	if (NULL != vmHookInterface) {
		(*vmHookInterface)->J9HookUnregister(vmHookInterface, J9HOOK_VM_INTERNAL_CLASS_LOAD, internalClassLoadHook, this);
		(*vmHookInterface)->J9HookUnregister(vmHookInterface, J9HOOK_VM_CLASSES_REDEFINED, classesRedefinedHook, this);
	}
}

GC_ObjectModel::ScanType 
GC_ObjectModel::getSpecialClassScanType(J9Class *objectClazz)
{
	ScanType result = SCAN_MIXED_OBJECT;

	/* 
	 * check if the object is an instance of or an instance or subclass of one of the SPECIAL classes.
	 * Note that these could still be uninitialized objects (no corresponding VM structs)
	 */
	if (objectClazz == _classClass) {
		/* no need to check subclasses of java.lang.Class, since it's final */
		result = SCAN_CLASS_OBJECT;
	} else if ((NULL != _classLoaderClass) && isSameOrSuperClassOf(_classLoaderClass, objectClazz)) {
		result = SCAN_CLASSLOADER_OBJECT;
	} else if ((NULL != _atomicMarkableReferenceClass) && isSameOrSuperClassOf(_atomicMarkableReferenceClass, objectClazz)) {
		result = SCAN_ATOMIC_MARKABLE_REFERENCE_OBJECT;
	} else {
		/* some unrecognized special class? */
		result = SCAN_INVALID_OBJECT;
	}
	
	return result;
}

void
GC_ObjectModel::printClassDetails(MM_ForwardedHeader *forwardedHeader) { 
    J9Class *clazz = getPreservedClass(forwardedHeader);
    J9UTF8* name = J9ROMCLASS_CLASSNAME(clazz->romClass);
    printf("class name is %.*s \n",J9UTF8_LENGTH(name), J9UTF8_DATA(name));
}


void
GC_ObjectModel::internalClassLoadHook(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMInternalClassLoadEvent *classLoadEvent = (J9VMInternalClassLoadEvent*)eventData;
	GC_ObjectModel *objectModel = (GC_ObjectModel*)userData;
	J9VMThread *vmThread = classLoadEvent->currentThread;
	J9Class *clazz = classLoadEvent->clazz;
	J9ROMClass *romClass = clazz->romClass;
	J9UTF8* className = J9ROMCLASS_CLASSNAME(romClass);
	printf("ALLOMCLASSES: class name is %.*s ;\n",J9UTF8_LENGTH(className), J9UTF8_DATA(className)); 
	/* we're only interested in bootstrap classes */
	if (clazz->classLoader == vmThread->javaVM->systemClassLoader) {		
		const char * const atomicMarkableReference = "java/util/concurrent/atomic/AtomicMarkableReference";
		const char * const javaLangClassLoader = "java/lang/ClassLoader";
		const char * const javaLangClass = "java/lang/Class";
		const char * const abstractOwnableSynchronizer = "java/util/concurrent/locks/AbstractOwnableSynchronizer";

		if (0 == compareUTF8Length(J9UTF8_DATA(className), J9UTF8_LENGTH(className), (U_8*)atomicMarkableReference, strlen(atomicMarkableReference))) {
			clazz->classDepthAndFlags |= J9AccClassGCSpecial;
			objectModel->_atomicMarkableReferenceClass = clazz;
		} else if (0 == compareUTF8Length(J9UTF8_DATA(className), J9UTF8_LENGTH(className), (U_8*)javaLangClassLoader, strlen(javaLangClassLoader))) {
			clazz->classDepthAndFlags |= J9AccClassGCSpecial;
			objectModel->_classLoaderClass = clazz;
		} else if (0 == compareUTF8Length(J9UTF8_DATA(className), J9UTF8_LENGTH(className), (U_8*)javaLangClass, strlen(javaLangClass))) {
			clazz->classDepthAndFlags |= J9AccClassGCSpecial;
			objectModel->_classClass = clazz;
		} else if (0 == compareUTF8Length(J9UTF8_DATA(className), J9UTF8_LENGTH(className), (U_8*)abstractOwnableSynchronizer, strlen(abstractOwnableSynchronizer))) {
			 clazz->classDepthAndFlags |= J9AccClassOwnableSynchronizer;
		}
		if(vmThread->javaVM->memoryManagerFunctions->j9gc_hot_reference_field_required(vmThread->javaVM) && J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(className), J9UTF8_LENGTH(className), "java/lang/String")) {
			const char * const fieldName = "value";
			const char * const fieldSig = "[C";				
			omrthread_monitor_enter(clazz->classLoader->hotFieldPoolMutex);
			clazz->hotFieldsInfo = (J9ClassHotFieldsInfo *)pool_newElement(clazz->classLoader->hotFieldClassInfoPool);

			if(NULL == clazz->hotFieldsInfo) {
				omrthread_monitor_exit(clazz->classLoader->hotFieldPoolMutex);
				printf("MODRON: Failed to allocate memory for hotFieldsInfo or hotfield pool\n");
				return; 
			} else {
				clazz->hotFieldsInfo->hotFieldOffset1 = (U_8)vmThread->javaVM->internalVMFunctions->instanceFieldOffset(vmThread, clazz, (U_8*)fieldName, strlen(fieldName), (U_8*)fieldSig, strlen(fieldSig), NULL, NULL, J9_LOOK_NO_JAVA) + (J9JAVAVM_OBJECT_HEADER_SIZE(javaVM)/J9JAVAVM_REFERENCE_SIZE(javaVM));;
				clazz->hotFieldsInfo->hotFieldOffset2 = U_8_MAX; 
				omrthread_monitor_exit(clazz->classLoader->hotFieldPoolMutex);
				printf("MODRON: AddFF newHotField to class STRING. Class:%p; hotFieldOffset:%u\n", clazz, clazz->hotFieldsInfo->hotFieldOffset1);
			}
		}
	}
}

void
GC_ObjectModel::classesRedefinedHook(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	GC_ObjectModel *objectModel = (GC_ObjectModel*)userData;

	/* update all J9Class pointers to their most current version */
	if (NULL != objectModel->_atomicMarkableReferenceClass) {
		objectModel->_atomicMarkableReferenceClass = J9_CURRENT_CLASS(objectModel->_atomicMarkableReferenceClass);
	}
	if (NULL != objectModel->_classLoaderClass) {
		objectModel->_classLoaderClass = J9_CURRENT_CLASS(objectModel->_classLoaderClass);
	}
	if (NULL != objectModel->_classClass) {
		objectModel->_classClass = J9_CURRENT_CLASS(objectModel->_classClass);
	}
}


