/*******************************************************************************
 * Copyright (c) 2016, 2021 IBM Corp. and others
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
package com.ibm.dump.tests.memorycategories;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.java.JavaRuntimeMemoryCategory;

/**
 * Test class that will check all JavaRuntimes inside an Image 
 * and checks the state of the JavaRuntimeMemoryCategories.
 * 
 * Encapsulates knowledge of which memory categories are expected to be present,
 * and what constitutes sanity for a 
 * 
 * @author andhall
 *
 */
public final class DTFJMemoryCategoryTest 
{
	/* List of memory categories we expect to have in all good dumps with default options */
	public static final List<String> EXPECTED_MEMORY_CATEGORIES;
	
	static {
		List<String> localList = new ArrayList<String>();
		
		localList.add("JRE");
		localList.add("JRE/VM");
		localList.add("JRE/VM/Classes");
		localList.add("JRE/VM/Memory Manager (GC)");
		localList.add("JRE/VM/Memory Manager (GC)/Java Heap");
		localList.add("JRE/VM/Threads");
		localList.add("JRE/VM/Threads/Java Stack");
		localList.add("JRE/VM/Threads/Native Stack");
		localList.add("JRE/VM/Port Library");
		localList.add("JRE/Class Libraries");
		/* Note: this test expects to be run against dumps from JIT-On VMs */
		localList.add("JRE/JIT");
		localList.add("JRE/JIT/JIT Code Cache");
		localList.add("JRE/JIT/JIT Data Cache");
		
		EXPECTED_MEMORY_CATEGORIES = Collections.unmodifiableList(localList);
	}
	
	private final Image image;
	private final List<String> failureReasons = new LinkedList<String>();
	
	public DTFJMemoryCategoryTest(Image image)
	{
		this.image = image;
	}

	/* Does the address space -> process -> Java runtime walk.
	 * We expect this test to be run on simple, good dumps. Any CorruptData is a test failure.
	 */
	private void walkJavaRuntimes() 
	{
		boolean javaRuntimeFound = false;
		Iterator<?> addressSpaceIt = image.getAddressSpaces();
		
		while (addressSpaceIt.hasNext()) {
			Object addressSpaceObj = addressSpaceIt.next();
			
			if (addressSpaceObj instanceof ImageAddressSpace) {
				ImageAddressSpace addressSpace = (ImageAddressSpace) addressSpaceObj;
				
				Iterator<?> processIt = addressSpace.getProcesses();
				
				while( processIt.hasNext() ) {
					Object processObj = processIt.next();
					
					if (processObj instanceof ImageProcess) {
						ImageProcess process = (ImageProcess) processObj;
						
						Iterator<?> runtimeIt = process.getRuntimes();
						
						while (runtimeIt.hasNext()) {
							Object runtimeObj = runtimeIt.next();
							
							if (runtimeObj instanceof JavaRuntime) {
								JavaRuntime runtime = (JavaRuntime) runtimeObj;
								
								testJavaRuntime(runtime);
								javaRuntimeFound = true;
							} else {
								recordFailure("Corrupt JavaRuntime object : " + runtimeObj + ", type " + runtimeObj.getClass());
							}
						}
						
					} else {
						recordFailure("Corrupt ImageProcess object : " + processObj + ", type " + processObj.getClass());
					}
				}
			} else {
				recordFailure("Corrupt ImageAddressSpace object : " + addressSpaceObj + ", type " + addressSpaceObj.getClass());
			}
		}
		
		if (! javaRuntimeFound) {
			recordFailure("No JavaRuntime objects found in dump");
		}
	}

	/**
	 * Test method run on each JavaRuntime found in the dump.
	 * @param runtime
	 */
	private void testJavaRuntime(JavaRuntime runtime) 
	{
		boolean sane = sanityCheckMemoryCategories(runtime);
		
		if (sane) {
			checkMemoryCategoryDetails(runtime);
		} else {
			recordFailure("MemoryCategory sanity check failed on runtime " + runtime + ". See other messages for details");
		}
	}

	private void checkMemoryCategoryDetails(JavaRuntime runtime) 
	{
		for (String path : EXPECTED_MEMORY_CATEGORIES) {
			JavaRuntimeMemoryCategory category = findCategoryPath(runtime, path);
			
			if (category == null) {
				recordFailure("Dump missing expected memory category " + path);
			}
			
			/* None of the expected memory categories should have 0 counters */
			try {
				if (category.getDeepAllocations() <= 0) {
					recordFailure("Expected memory category " + path + " has <=0 deepAllocations: " + category.getDeepAllocations());
				}
				
				if (category.getDeepBytes() <= 0) {
					recordFailure("Expected memory category " + path + " has <=0 deepBytes: " + category.getDeepBytes());
				}
			} catch (CorruptDataException ex) {
				recordFailure("CorruptDataException checking allocations/bytes from category " + path);
			}
		}
	}
	
	/**
	 * Finds a memory category based on a path
	 * @param runtime Java runtime to walk categories from
	 * @param path Qualified path to category e.g. JRE/Memory Manager (GC)/Java Heap
	 * @return JavaRuntimeMemoryCategory object, or null if not found
	 */
	private JavaRuntimeMemoryCategory findCategoryPath(JavaRuntime runtime, String path)
	{
		// Strip any leading or trailing '/' 
		path = path.replaceAll("^/+", "");
		path = path.replaceAll("/+$", "");
		
		try {
			return findCategoryPath(runtime.getMemoryCategories(), path);
		} catch (DataUnavailable e) {
			throw new Error("Unexpected DataUnavailable - this should have been caught in the sanity check");
		}
	}

	private JavaRuntimeMemoryCategory findCategoryPath(Iterator<?> categoriesIt, String path) 
	{
		int nextSeparatorIndex = path.indexOf('/');
		
		String matchName = null;
		if (nextSeparatorIndex != -1) {
			matchName = path.substring(0, nextSeparatorIndex);
			path = path.substring(nextSeparatorIndex + 1);
		} else {
			matchName = path;
			path = "";
		}
		
		while (categoriesIt.hasNext()) {
			Object categoryObj = categoriesIt.next();
			
			if (categoryObj instanceof JavaRuntimeMemoryCategory) {
				JavaRuntimeMemoryCategory category = (JavaRuntimeMemoryCategory) categoryObj;
				
				try {
					if (category.getName().equals(matchName)) {
						if (path.length() == 0) {
							return category;
						} else {
							return findCategoryPath(category.getChildren(), path);
						}
					}
				} catch (CorruptDataException e) {
					recordFailure("CorruptDataException getting name from category " + category, e);
				}
				
			} else {
				/* Should have been caught in the sanity check */
				throw new Error("Unexpected bad memory category object: " + categoryObj + " type: " + categoryObj.getClass() + " - this should have been caught in the sanity check");
			}
		}
		return null;
	}

	/**
	 * Verifies that we can get memory categories from the runtime, and that
	 * there aren't any loops or repeated nodes.
	 */
	private boolean sanityCheckMemoryCategories(JavaRuntime runtime) 
	{
		try {
			Iterator<?> memoryCategoryIt = runtime.getMemoryCategories();
			boolean memoryCategoryFound = false;
			Set<JavaRuntimeMemoryCategory> knownSet = new HashSet<JavaRuntimeMemoryCategory>();
			
			while ( memoryCategoryIt.hasNext() ) {
				Object memoryCategoryObj = memoryCategoryIt.next();
				
				if (memoryCategoryObj instanceof JavaRuntimeMemoryCategory) {
					JavaRuntimeMemoryCategory category = (JavaRuntimeMemoryCategory) memoryCategoryObj;
					memoryCategoryFound = true;
					
					if (! sanityWalkCategory(knownSet, category) ) {
						return false;
					}
				} else {
					recordFailure("Corrupt JavaRuntimeMemoryCategory returned from runtime " + runtime + "; Object = " + memoryCategoryObj + ", type =  " + memoryCategoryObj.getClass());
				}
			}
			
			if (! memoryCategoryFound) {
				recordFailure("No memory categories iterated for runtime " + runtime);
				return false;
			}
			return true;
			
		} catch (DataUnavailable e) {
			recordFailure("Runtime " + runtime + " has no memory categories available");
			return false;
		}
	}

	private boolean sanityWalkCategory(Set<JavaRuntimeMemoryCategory> knownSet, JavaRuntimeMemoryCategory currentCategory) 
	{
		String currentCategoryName;
		try {
			currentCategoryName = currentCategory.getName();
		} catch (CorruptDataException e1) {
			recordFailure("CorruptDataException trying to read name of category " + currentCategory, e1);
			currentCategoryName = "<CORRUPT>";
		}
		
		if (knownSet.contains(currentCategory)) {
			recordFailure("Loop detected in JavaRuntimeMemoryCategory tree. Found " + currentCategory + " at least twice. Name = " + currentCategoryName);
			return false;
		}
		
		knownSet.add(currentCategory);
		
		Iterator<?> childIt = null;
		try {
			childIt = currentCategory.getChildren();
		} catch (CorruptDataException e) {
			recordFailure("CorruptDataException trying to get children of category " + currentCategoryName, e);
			//Even though we can't walk children - we haven't found a loop or any really dangerous damage
			//so we'll call this sane
			return true;
		}
		
		try {
			long deepBytes = currentCategory.getDeepBytes();
			if (deepBytes < 0) {
				recordFailure("Category " + currentCategoryName + " has negative deepBytes: " + deepBytes);
				return false;
			}
			
			long shallowBytes = currentCategory.getShallowBytes();
			if (shallowBytes < 0) {
				recordFailure("Category " + currentCategoryName + " has negative shallowBytes: " + shallowBytes);
				return false;
			}
			
			long deepAllocations = currentCategory.getDeepAllocations();
			if (deepAllocations < 0) {
				recordFailure("Category " + currentCategoryName + " has negative deepAllocations: " + deepAllocations);
				return false;
			}
			
			long shallowAllocations = currentCategory.getShallowAllocations();
			if (shallowAllocations < 0) {
				recordFailure("Category " + currentCategoryName + " has negative shallowAllocations: " + shallowAllocations);
				return false;
			}
			
		} catch (CorruptDataException ex) {
			recordFailure("CorruptDataException thrown trying to introspect category " + currentCategoryName, ex);
			return false;
		}
		
		try {
			currentCategory.getMemorySections(false);
			currentCategory.getMemorySections(true);
		} catch (CorruptDataException e) {
			recordFailure("CorruptDataException thrown trying to getMemorySections from " + currentCategoryName, e);
			return false;
		} catch (DataUnavailable e) {
			//This is permissable
		}
		
		while (childIt.hasNext()) {
			Object childCategoryObj = childIt.next();
			
			if (childCategoryObj instanceof JavaRuntimeMemoryCategory) {
				JavaRuntimeMemoryCategory childCategory = (JavaRuntimeMemoryCategory) childCategoryObj;
				
				if (! sanityWalkCategory(knownSet, childCategory) ) {
					return false;
				}
			} else {
				recordFailure("Unrecognised child category " + childCategoryObj + " type: " + childCategoryObj.getClass() + " returned as child of category " + currentCategoryName);
				return false;
			}
		}
		
		return true;
	}

	public boolean runTest() {
		resetFailureInfo();
		
		try {
			walkJavaRuntimes();
		} catch (Throwable t) {
			recordFailure("Throwable caught in runTest()", t);
		}
			
		return testPassed();
	}

	private void recordFailure(String message) 
	{
		failureReasons.add(message);
	}
	
	private void recordFailure(String message, Throwable t) 
	{
		StringWriter writer = new StringWriter();
		PrintWriter pw = new PrintWriter(writer);
		
		pw.println(message);
		t.printStackTrace(pw);
		
		failureReasons.add(writer.toString());
	}

	private boolean testPassed()
	{
		return failureReasons.size() == 0;
	}

	private void resetFailureInfo() 
	{
		failureReasons.clear();
	}

	public void printFailures() 
	{
		for (String failure : failureReasons) {
			System.err.println("* " + failure);
		}
	}

}
