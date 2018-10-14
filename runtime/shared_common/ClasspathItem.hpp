/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

#ifndef CLASSPATHITEM_HPP_INCLUDED
#define CLASSPATHITEM_HPP_INCLUDED

/* @ddr_namespace: default */
#include "j9.h"
#include "j9protos.h"

/*
 * A ClasspathEntryItem is the data structure for representing a 
 * classpath entry in the shared cache.
 * These are created internally by ClasspathItem when addItem() is called.
 */
class ClasspathEntryItem 
{
protected:
	void *operator new(size_t size, void *memoryPtr) { return memoryPtr; };

public:
    typedef char* BlockPtr;

	UDATA protocol;
	I_64 timestamp;
	UDATA flags;
	UDATA pathLen;
	UDATA locationPathLen;

	/* 
	 * Construct new ClasspathEntryItem
	 * 
	 * Parameters:
	 *   path			Path of classpath entry
	 *   pathlen		Path length is required
	 *   protocol		Protocol of classpath entry	
	 *   memForConstructor		Memory to allocate instance into
	 */
	static ClasspathEntryItem* newInstance(const char* path, U_16 pathlen, UDATA protocol, ClasspathEntryItem* memForConstructor);

	/*
	 * Returns bytes needed in memory to serialize this ClasspathEntryItem
	 * with appropriate padding.
	 */
	U_32 getSizeNeeded(void) const;

	/*
	 * Serializes this ClasspathEntryItem to address "block".
	 * Assumes that enough memory exists at address given.
	 * Returns pointer to next free byte at end of the block written.
	 * 
	 * Parameters:
	 *   block			Address to serialize to
	 */
	BlockPtr writeToAddress(BlockPtr block);

	/*
	 * Returns the string path of the classpath entry
	 * If object in memory, returns private path field.
	 * If object in cache, returns path in cache.
	 * 
	 * Parameters:
	 *   pathLen			Path length of the path returned
	 */
	const char* getPath(U_16* pathLen) const;
	
	/*
	 * Returns the same string path as getPath() except for a nested jar.
	 * If the ClasspathEntryItem is a nested jar, getPath() returns a path string including the entry inside the external jar,
	 * while getLocation() returns a path string including the external jar only.
	 * e.g. For /path/A.jar!/lib/B.jar, getPath() returns "/path/A.jar!/lib/B.jar", getLocation() returns "/path/A.jar"
	 *
	 * If object in memory, returns private path field.
	 * If object in cache, returns path in cache.
	 *
	 * Parameters:
	 *   locationPathLen	Path length of the path returned
	 */
	const char* getLocation(U_16* locationPathLen) const;

	/*
	 * Get a hashcode for this cpei.
	 * 
	 * Parameters:
	 *   vmFunctionTable		A VM function table
	 */
	UDATA hash(J9InternalVMFunctions* vmFunctionTable);

	/*
	 * Clean up resources
	 */
	void cleanup(void);
	
private:
	char* path;
	UDATA hashValue;

	IDATA initialize(const char* path, U_16 pathLen, UDATA protocol);
};

/*
 * A ClasspathItem is the data structure for representing a classpath in the
 * shared cache. It has a number of useful functions for comparison and serialization.
 * A ClasspathItem is made up of a number of ClasspathEntryItems (see above).
 * 
 * ClasspathItems are organized and managed by the SH_ClasspathManager.
 */
class ClasspathItem
{
protected:
	void *operator new(size_t size, void *memoryPtr) { return memoryPtr; };

public:
    typedef char* BlockPtr;
    
	/* Public vars are ones we care about in the cache and locally */
	U_16 type;
	U_16 flags;
	IDATA itemsAdded;
	UDATA hashValue;
	IDATA firstDirIndex;
	
	/*
	 * Construct new ClasspathItem
	 * 
	 * Parameters:
	 *   vm					A Java VM
	 *   entries			Expected number of ClasspathEntryItems
	 * 						(Allocates memory for them ahead of time)
	 *   memForConstructor		Memory to build instance into
	 */
	static ClasspathItem* newInstance(J9JavaVM* vm, IDATA entries, IDATA fromHelperID, U_16 cpType, ClasspathItem* memForConstructor);

	/*
	 * Returns memory bytes the constructor requires to build what it needs
	 */
	static UDATA getRequiredConstrBytes(UDATA entries);

	/*
	 * Adds a classpath entry to the ClasspathItem, which constructs
	 * a ClasspathEntryItem and stores it internally.
	 * Returns number of items currently added.
	 * 
	 * (Note: Does not dynamically grow "entries" space allocated in constructor
	 *  so can only addItem() until getItemsAdded()=="entries". After that,
	 *  addItem() returns -1).
	 * 
	 * Parameters:
	 *   vmFunctionTable		A VM function table
	 *   path				Path of classpath entry
	 *   protocol			Protocol of classpath entry (use PROTO_X defines)
	 */
	IDATA addItem(J9InternalVMFunctions* vmFunctionTable, const char* path, U_16 pathLen, UDATA protocol);
	
	/*
	 * Return ClasspathEntryItem at given index
	 * Returns NULL if index is out of bounds
	 * 
	 * Parameters:
	 *   i					Index of classpath entry to return
	 */
	ClasspathEntryItem* itemAt(I_16 i) const;
	
	/*
	 * Static helper function for comparing two ClasspathEntryItems.
	 * Returns true if items are the same path and protocol, but does not
	 * compare timestamps for equality. Returns false otherwise.
	 * (Note: test and compare can be pointers to ClasspathEntryItems
	 *  either in cache or in memory)
	 * 
	 * Parameters:
	 *   vmFunctionTable		A VM function table
	 *   test				ClasspathEntryItem to test
	 *   compareTo			ClasspathEntryItem to compare test to
	 */
	static bool compare(J9InternalVMFunctions* vmFunctionTable, ClasspathEntryItem* test, ClasspathEntryItem* compareTo);
	
	/*
	 * Static helper function for comparing two ClasspathItems.
	 * Returns true if classpaths contain the same classpath entries
	 * (tested by other compare function) in the same order.
	 * Returns false otherwise.
	 * (Note: test and compare can be pointers to ClasspathItems
	 *  either in cache or in memory)
	 * WARNING: This function can be expensive for long classpaths!
	 * 
	 * Parameters:
	 *   vmFunctionTable		A VM function table
	 *   test				ClasspathItem to test
	 *   compareTo			ClasspathItem to compare test to
	 */
	static bool compare(J9InternalVMFunctions* vmFunctionTable, ClasspathItem* test, ClasspathItem* compareTo);

	/*
	 * Finds the index of a classpath entry in a given classpath
	 * Searches whole classpath. Returns -1 if not found.
	 * (Note: If you know that the result must be before a certain index
	 *  use other find function as this will search up to that index)
	 * 
	 * Parameters:
	 *   vmFunctionTable		A VM function table
	 *   test				Classpath entry to search for
	 */
	I_16 find(J9InternalVMFunctions* vmFunctionTable, ClasspathEntryItem* test) const;
	
	/*
	 * Finds the index of a classpath entry in a given classpath
	 * Searches classpath up to and including stopAtIndex.
	 * Returns -1 if not found.
	 * 
	 * Parameters:
	 *   vmFunctionTable		A VM function table
	 *   test				Classpath entry to search for
	 *   stopAtIndex		Index to stop searching at
	 */
	I_16 find(J9InternalVMFunctions* vmFunctionTable, ClasspathEntryItem* test, I_16 stopAtIndex) const;

	/*
	 * Returns max number of ClasspathEntryItems
	 */
	IDATA getMaxItems(void) const;

	/*
	 * Returns number of ClasspathEntryItems added
	 */
	I_16 getItemsAdded(void) const;

	/*
	 * Returns bytes needed in memory to serialize this ClasspathItem
	 * with appropriate padding.
	 */
	U_32 getSizeNeeded(void) const;
	
	/*
	 * Returns ID of the Helper API that this classpath belongs to
	 */
	IDATA getHelperID(void) const;

	/*
	 * Returns index of first entry in classpath which is a timestampable directory.
	 * Otherwise returns -1.
	 */
	IDATA getFirstDirIndex(void) const;

	/*
	 * Serializes this ClasspathItem to address "block".
	 * Assumes that enough memory exists at address given.
	 * Returns 0 if success and -1 if failed.
	 * 
	 * Parameters:
	 *   block			Address to serialize to
	 */
	IDATA writeToAddress(BlockPtr block);

	/* 
     * Returns hashcode calculated from classpath entry strings
     * Not guaranteed to be unique.
     */
	UDATA getHashCode(void) const;

	/* 
     * Returns type of ClasspathItem
     */
	UDATA getType(void) const;

	/*
	 * Returns true if the ClasspathItem is in the cache. False if local.
	 */
	bool isInCache(void) const;

	/* Field used for optimizing timestamp checking */
	void setJarsLockedToIndex(I_16 i);

	/* Field used for optimizing timestamp checking */
	I_16 getJarsLockedToIndex(void) const;

	/*
	 * Clean up resources
	 */
	void cleanup(void);
	
private:

	/* Private options are ones only used locally */
	IDATA entries;
	ClasspathEntryItem** items;
	J9PortLibrary* portlib;
	IDATA helperID;
	IDATA jarsLockedToIndex;

	ClasspathEntryItem** initializeItems(IDATA entries_);

	void initialize(J9JavaVM* vm, IDATA entries, IDATA fromHelperID, U_16 cpType, BlockPtr memForConstructor);
};

#endif /* CLASSPATHITEM_HPP_INCLUDED */
