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
 * @ingroup GC_Base
 */

#if !defined(CLASSUNLOADMANAGER_HPP_)
#define CLASSUNLOADMANAGER_HPP_

#include "j9.h"
#include "j9cfg.h"


#include "BaseNonVirtual.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensions.hpp"

/* forward declarations to avoid cyclic include dependencies */
class MM_GCExtensions;
class MM_GlobalCollector;
class MM_HeapMap;
class MM_ClassUnloadStats;

class MM_ClassLoaderManager : public MM_BaseNonVirtual
{
friend class GC_ClassLoaderLinkedListIterator;
	
public:
protected:
private:
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	omrthread_monitor_t _undeadSegmentListMonitor;
	J9MemorySegment *_firstUndeadSegment;
	UDATA _undeadSegmentsTotalSize;
	UDATA _lastUnloadNumOfClassLoaders;  /**< number of class loaders last seen during a dynamic class unloading pass */
	UDATA _lastUnloadNumOfAnonymousClasses; /**< number of anonymous classes last seen during a dynamic class unloading pass */
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	MM_GlobalCollector *_globalCollector; /**< Pointer to the global collector.  Used for yielding */
	J9ClassLoader *_classLoaders; /**< Linked list of classloaders */
	MM_GCExtensions *_extensions; /**< GC extensions structure */
	J9JavaVM *_javaVM; /**< Pointer to the Java VM */
	omrthread_monitor_t _classLoaderListMonitor; /**< monitor that controls modification of the classLoader linked list */

public:
	static MM_ClassLoaderManager *newInstance(MM_EnvironmentBase *env, MM_GlobalCollector *globalCollector); 
	void kill(MM_EnvironmentBase *env);
	
	MM_ClassLoaderManager(MM_EnvironmentBase *env, MM_GlobalCollector *globalCollector) :
		MM_BaseNonVirtual()
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
		,_undeadSegmentListMonitor(NULL)
		,_firstUndeadSegment(NULL)
		,_undeadSegmentsTotalSize(0)
		,_lastUnloadNumOfClassLoaders(0)
		,_lastUnloadNumOfAnonymousClasses(0)
#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
		,_globalCollector(globalCollector)
		,_classLoaders(NULL)
		,_extensions(MM_GCExtensions::getExtensions(env))
		,_javaVM((J9JavaVM *)env->getOmrVM()->_language_vm)
		,_classLoaderListMonitor(NULL)
	{
		_typeId = __FUNCTION__;
	}

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	/**
	 * Adds the list of memory segments to the internal queue to be reclaimed later
	 * @param listRoot The root of the segments to be added (follow nextSegmentInClassLoader link to traverse)
	 */
	void enqueueUndeadClassSegments(J9MemorySegment *listRoot);
	
	/**
	 * Flushes the cached list of segments by calling the VM's freeMemorySegment method
	 * @param env The environment
	 */
	void flushUndeadSegments(MM_EnvironmentBase *env);
	
	/**
	 * Returns the total amount of memory (in bytes) which would be reclaimed if the buffer were to be flushed
	 */
	UDATA reclaimableMemory() { return _undeadSegmentsTotalSize; }
	
	/**
	 * Returns the number of class loaders last seen during a dynamic class unloading pass
	 */
	UDATA getLastUnloadNumOfClassLoaders() { return _lastUnloadNumOfClassLoaders; }

	/**
	 * Set the number of class loaders last seen during a dynamic class unloading pass,
	 * (i.e. the number of classloaders currently loaded)
	 */
	void setLastUnloadNumOfClassLoaders();
	
	/**
	 * Returns the number of anonymous classes last seen during a dynamic class unloading pass
	 */
	UDATA getLastUnloadNumOfAnonymousClasses() { return _lastUnloadNumOfAnonymousClasses; }

	/**
	 * Set the number of anonymous classes last seen during a dynamic class unloading pass,
	 * (i.e. the number of anonymous classes currently loaded)
	 */
	void setLastUnloadNumOfAnonymousClasses();

	/**
	 * Perform initial cleanup for classloader unloading.  The current thread has exclusive access.
	 * The J9AccClassDying bit is set and J9HOOK_VM_CLASS_UNLOAD is triggered for each class that will be unloaded.
	 * The J9_GC_CLASS_LOADER_DEAD bit is set for each class loader that will be unloaded.
	 * J9HOOK_VM_CLASSES_UNLOAD is triggered if any classes will be unloaded.
	 * 
	 * @param env[in] the master GC thread
	 * @param classLoaderUnloadList[in] the linked list of loaders to unload, connected through the unloadLink field
	 * @param markMap[in] the markMap to use to test for class loader and classes liveness
	 * @param classUnloadStats[out] returns the class unloading statistics for classes about to be unloaded
	 */
	void cleanUpClassLoadersStart(MM_EnvironmentBase *env, J9ClassLoader* classLoaderUnloadList, MM_HeapMap *markMap, MM_ClassUnloadStats *classUnloadStats);
	
	/**
	 * Perform final cleanup for classloader unloading.  The current thread has exclusive access.
	 *
	 * Any classes in the subclass hierarchy which are identified as dying are removed.
	 * 
	 * RAM segments in the J9ClassLoaders in unloadLink are changed to UNDEAD segments and added
	 * to reclaimedSegments.  ROM segments in the J9ClassLoaders in the unloadLink are freed.
	 * 
	 * All J9ClassLoaders in unloadLink are freed.
	 * 
	 * @note Shared libraries associated with dead classloaders are NOT unloaded by this function.  Such
	 * classloaders should not be on the unloadLink list.
	 *
	 * @param env[in] the current thread
	 * @param unloadLink a list of non-finalizable dead classloaders, linked by J9ClassLoader::unloadLink
	 *
	 * @return the count of classes unloaded
	 */
	void cleanUpClassLoadersEnd(MM_EnvironmentBase *env, J9ClassLoader* unloadLink);
	
	/**
	 * Frees all the ROMClass segments in the list reachable from segment following nextSegmentInClassLoader and
	 * sets all RAMClass segments to UNDEADClass segments and prepends them to the reclaimedSegments list, by
	 * reference, linked via nextSegmentInClassLoader
	 */
	void cleanUpSegmentsAlongClassLoaderLink(J9JavaVM *javaVM, J9MemorySegment *segment, J9MemorySegment **reclaimedSegments);
	
	/**
	 * Remove the specified class from its subclass traversal list.
	 * The class is moved into a trivial list consisting of itself.
	 * @param env[in] the current thread
	 * @param clazzPtr[in] the class to remove 
	 */
	void removeFromSubclassHierarchy(MM_EnvironmentBase *env, J9Class *clazzPtr);

	/**
	 * Perform generic clean up for a list of class loaders to unload.
	 * @param env[in] the current thread
	 * @param classLoader[in] the list of class loaders to clean up
	 * @param reclaimedSegments[out] a linked list of memory segments to be reclaimed by cleanUpClassLoadersEnd
	 * @param unloadLink[out] a linked list of class loaders to be reclaimed by cleanUpClassLoadersEnd
	 * @param finalizationRequired[out] set to true if the finalize thread must be started, unmodified otherwise
	 */
	void cleanUpClassLoaders(MM_EnvironmentBase *env, J9ClassLoader *classLoadersUnloadedList, J9MemorySegment** reclaimedSegments, J9ClassLoader ** unloadLink, volatile bool* finalizationRequired);

	/**
	 * Attempt to enter the class unload mutex if it is uncontended.
	 * @param env[in] the current thread
	 * @return true on success, false if the JIT has the mutex locked
	 */
	bool tryEnterClassUnloadMutex(MM_EnvironmentBase *env);

	/**
	 * Enter the class unload mutex, waiting for the JIT to release it if necessary.
	 * @param env[in] the current thread
	 * @return the time, in microseconds, the GC was forced to wait
	 */
	U_64 enterClassUnloadMutex(MM_EnvironmentBase *env);

	/**
	 * Release the class unload mutex.
	 * @param env[in] the current thread
	 */
	void exitClassUnloadMutex(MM_EnvironmentBase *env);
	
	/**
	 * Using a mark map to identify liveness, build a linked list of class loaders to unload.
	 * @param env[in] the current thread
	 * @param markMap[in] the markMap to use to test for class loader liveness
	 * @param classUnloadStats[out] returns the class unloading statistics with class loaders visited
	 * @return the head of a linked list of class loaders to be unloaded
	 */
	J9ClassLoader *identifyClassLoadersToUnload(MM_EnvironmentBase *env, MM_HeapMap *markMap, MM_ClassUnloadStats *classUnloadStats);

	/**
	 * Clean up memory segments in anonymous classloader
	 * @param env[in] the current thread
	 * @param reclaimedSegments[out] a linked list of memory segments to be reclaimed by cleanUpClassLoadersEnd
	 */
	void cleanUpSegmentsInAnonymousClassLoader(MM_EnvironmentBase *env, J9MemorySegment **reclaimedSegments);

#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */
	
	/**
	 * Answer true if class unloading should be attempted, or false if insufficient class loading
	 * activity has occurred
	 */
	bool isTimeForClassUnloading(MM_EnvironmentBase *env);

	void unlinkClassLoader(J9ClassLoader *classLoader);
	void linkClassLoader(J9ClassLoader *classLoader);
	
protected:
	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);
private:

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	/**
	 * Scan classloader for dying classes and add them to the list
	 * @param env[in] the current thread
	 * @param classLoader[in] the list of class loaders to clean up
	 * @param markMap[in] the markMap to use to test for class loader liveness
	 * @param setAll[in] bool if true if all classes must be set dying, if false unmarked classes only
	 * @param classUnloadListStart[in] root of list dying classes should be added to
	 * @param classUnloadCountOut[out] number of classes dying added to the list
	 * @return new root to list of dying classes
	 */
	J9Class *addDyingClassesToList(MM_EnvironmentBase *env, J9ClassLoader * classLoader, MM_HeapMap *markMap, bool setAll, J9Class *classUnloadListStart, UDATA *classUnloadCountOut);

#endif /* J9VM_GC_DYNAMIC_CLASS_UNLOADING */

};

#endif /* CLASSUNLOADMANAGER_HPP_ */
