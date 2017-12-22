/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.view.dtfj.test;

import static org.junit.Assert.fail;
import static org.junit.Assert.assertTrue;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Comparator;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Ignore;
import org.junit.Test;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaHeap;
import com.ibm.dtfj.java.JavaMethod;
import com.ibm.dtfj.java.JavaMonitor;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaReference;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.java.JavaThread;
import com.ibm.j9ddr.util.IteratorHelpers.IteratorFilter;
import com.ibm.j9ddr.view.dtfj.comparators.JavaThreadComparator;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageProcess;

public class JavaRuntimeTest extends DTFJUnitTest {

	public static final Comparator<Object> COMPILED_METHODS_SORT_ORDER = new Comparator<Object>() {
		public int compare(Object o1, Object o2) {
			try {
				JavaMethod method1 = (JavaMethod) o1;
				JavaMethod method2 = (JavaMethod) o2;
				
				ImagePointer id1 = method1.getDeclaringClass().getID();
				String result1 = String.valueOf(id1.getAddress());
				result1 += id1.getAddressSpace().toString();
				result1 += method1.getName();
				result1 += method1.getSignature();
				
				ImagePointer id2 = method2.getDeclaringClass().getID();
				String result2 = String.valueOf(id2.getAddress());
				result2 += id2.getAddressSpace().toString();
				result2 += method2.getName();
				result2 += method2.getSignature();
				
				return result1.compareTo(result2);
			} catch (CorruptDataException e) {
				fail("Problem setting up test");
				return 0;
			} catch (DataUnavailable e) {
				fail("Problem setting up test");
				return 0;
			}
		}
	};
	
	@BeforeClass
	public static void classSetup() {
		init();
	}
	
	@Before
	public void setup() {
		initTestObjects();
	}
	
	protected void loadTestObjects(JavaRuntime ddrRuntime, List<Object> ddrObjects, JavaRuntime jextractRuntime, List<Object> jextractObjects) {
		ddrObjects.add(ddrRuntime);
		jextractObjects.add(jextractRuntime);
	}
	
	@Test
	public void equalsTest() {
		equalsTest(ddrTestObjects, ddrClonedObjects, jextractTestObjects, true);
	}
	
	@Test
	public void hashCodeTest() {
		hashCodeTest(ddrTestObjects, ddrClonedObjects);
	}
	
	@Test
	public void getCompiledMethodsTest() {
		javaMethodComparator.testComparatorIteratorEquals(ddrTestObjects, jextractTestObjects, "getCompiledMethods", JavaMethod.class, COMPILED_METHODS_SORT_ORDER);
	}
	
	@SuppressWarnings("unchecked")
	@Test
	public void getHeapRootsTest() {
		boolean result = true;
		String version = ((J9DDRImageProcess) ddrProcess).getVersion();
		IteratorFilter ddrFilter = null;
		
		if(version.equals("26")) {
			ddrFilter = new IteratorFilter()
			{
				//vm26 has a StringTable Cache that the root scanner scans but jextract(HeapIteratorAPI) does not
				public boolean accept(Object obj) {
					JavaReference ref = (JavaReference) obj;
					if(ref.getDescription().equals("StringCacheTable")) {
						return false;
					}
					return true;
				}	
			};
		}
		
		if(version.equals("23")) {
			ddrFilter = new IteratorFilter()
			{
				boolean foundFirstContrivedThread = false;
				
				public boolean accept(Object obj) {

					try{
						JavaReference ref = (JavaReference) obj;
						Object ddrHeapRoot = ref.getTarget();
						
						// JExtract has a bug which causes it to miss the following:
						// An instance of java.lang.OutOfMemoryError attached to each Thread
						// An instance of the threads current Exception if it exists.
						
						// So this test will fail if:
						//    JExtract detects a reference that DDR does not
						//    DDR detects a reference that JExtract does not AND that reference is not an Exception on Thread
						
						if (ref.getRootType() == JavaReference.HEAP_ROOT_THREAD) {
							if (ddrHeapRoot instanceof JavaObject) {
								JavaObject javaObject = (JavaObject) ddrHeapRoot;
								String name = javaObject.getJavaClass().getName();
								if (
									name.endsWith("Error") || 
									name.endsWith("Exception") ||
									name.equals("com/ibm/dtfj/tck/tests/javaruntime/TestJavaMonitor_ObjectMonitors$MonitorClass") ||
									name.equals("com/ibm/dtfj/tck/harness/Configure$DumpThread")
								) {
									// Ignore these since JExtract can't find them.
									return false;
								}
								
								if (name.equals("com/ibm/dtfj/tck/tests/javaruntime/TestJavaThread_getStackFrames$ContrivedThread")) {
									if (foundFirstContrivedThread) {
										return false;
									} else {
										foundFirstContrivedThread = true;
									}
								}
							}
						}
						return true;
					} catch (DataUnavailable e) {
						fail("bail");
						return false;
					} catch (CorruptDataException e) {
						fail("bail");
						return false;
					}
				}
			};
		}
		
		for (int i = 0; i < ddrTestObjects.size(); i++) {
			JavaRuntime localDDRRuntime = (JavaRuntime) ddrTestObjects.get(i);
			JavaRuntime localJextractRuntime = (JavaRuntime) jextractTestObjects.get(i);
			
			Object[] ddrHeapRootsArray = null;
			Object[] jextractHeapRootsArray = null;
			
			int threadCount = 0;
			Iterator titer = localDDRRuntime.getThreads();
			while (titer.hasNext()) {
				threadCount++;
				titer.next();
			}
			
			System.out.println("Thread count: " + threadCount);
			
			try {
				ddrHeapRootsArray = DTFJComparator.getList(Object.class, localDDRRuntime.getHeapRoots(), ddrFilter).toArray();
				jextractHeapRootsArray = DTFJComparator.getList(Object.class, localJextractRuntime.getHeapRoots(), null).toArray();
			} catch (InvalidObjectException e1) {
				fail("Test setup failure");
			}
			
			Map<Long, List<Object>> map = new HashMap<Long, List<Object>>();
			
			for (int j = 0; j < ddrHeapRootsArray.length; j++) {
				long key = 0;
				
				JavaReference ref = (JavaReference) ddrHeapRootsArray[j];
				Object ddrHeapRoot = null;
				try {
					ddrHeapRoot = ref.getTarget();
				} catch (DataUnavailable e) {
					fail("bail");
				} catch (CorruptDataException e) {
					fail("bail");
				}
				
				if (ddrHeapRoot instanceof JavaObject) {
					key = ((JavaObject) ddrHeapRoot).getID().getAddress();
				} else if (ddrHeapRoot instanceof JavaClass) {
					key = ((JavaClass) ddrHeapRoot).getID().getAddress();
				} else {
					fail("bail");
				}
				
				List bucket = map.get(key);
				if (bucket == null) {
					bucket = new ArrayList<Object>();
					map.put(key, bucket);
				}
				
				bucket.add(ddrHeapRoot);
			}
			
			for (int j = 0; j < jextractHeapRootsArray.length; j++) {
				JavaReference ref = (JavaReference) jextractHeapRootsArray[j];
				Object heapRoot = null;
				String name = null;	
				long key = 0;
				
				try {
					heapRoot = ref.getTarget();
									
					if (heapRoot instanceof JavaObject) {
						key = ((JavaObject) heapRoot).getID().getAddress();
						name = "Instance of " + ((JavaObject) heapRoot).getJavaClass().getName();
					} else if (heapRoot instanceof JavaClass) {
						key = ((JavaClass) heapRoot).getID().getAddress();
						name = ((JavaClass) heapRoot).getName();
					} else {
						fail("bail");
					}
				} catch (DataUnavailable e) {
					fail("bail");
				} catch (CorruptDataException e) {
					fail("bail");
				}
				
				List bucket = map.get(key);
				
				if (bucket == null) {
					System.out.println(String.format("Jextract found %s at address: %s.  Element #: %s that ddr does not know about", name, Long.toHexString(key), j));
					result = false;
					continue;
				}
				
				if (bucket.isEmpty()) {
					fail("bail");
				}
				
				bucket.remove(0);
				
				if (bucket.isEmpty()) {
					map.remove(key);
				}
			}
			
			// Whats left.
			
			int count = 0;
			Collection<List<Object>> remains = map.values();
			if (remains.size() > 0) {
				System.out.println("");
				System.out.println("DDR found the following that JExtract did not know about");
			}
			
			Iterator<List<Object>> bucketIter = remains.iterator();
			while (bucketIter.hasNext()) {
				List<Object> bucket = bucketIter.next();
				Iterator<Object> contentsIter = bucket.iterator();
				while (contentsIter.hasNext()) {
					Object obj = contentsIter.next();
					System.out.println(String.format("%s) %s", ++count, obj));
				}
			}
		}
		
		assertTrue(result);
	}
	
	@Test
	public void getHeapsTest() {
		Iterator<Object> iter = ((JavaRuntime) ddrTestObjects.get(0)).getHeaps();
		while (iter.hasNext()) {
			JavaHeap heap = (JavaHeap) iter.next();
			Iterator<Object> objectIter = heap.getObjects();
			int count = 0;
			while (objectIter.hasNext()) {
//				JavaObject javaObject = (JavaObject) objectIter.next();
//				System.out.println(javaObject.get);
				count++;
				objectIter.next();
			}
			System.out.println(heap.getName() + ": " + count);
		}
		javaHeapComparator.testComparatorIteratorEquals(ddrTestObjects, jextractTestObjects, "getHeaps", JavaHeap.class);
	}
	
	@Test
	public void getJavaClassLoadersTest() {
		javaClassLoaderComparator.testComparatorIteratorEquals(ddrTestObjects, jextractTestObjects, "getJavaClassLoaders", JavaClassLoader.class);
	}
	
	@Test
	public void getJavaVMTest() {
		imagePointerComparator.testComparatorEquals(ddrTestObjects, jextractTestObjects, "getJavaVM");
	}
	
	@Test
	public void getJavaVMInitArgsTest() {
		javaVMInitArgsComparator.testComparatorEquals(ddrTestObjects, jextractTestObjects, "getJavaVMInitArgs");
	}
	
	@Test
	public void getMonitorsTest() {
		javaMonitorComparator.testComparatorIteratorEquals(ddrTestObjects, jextractTestObjects, "getMonitors", JavaMonitor.class, null);
	}
	
	@Ignore
	@Test
	public void getObjectAtAddressTest() {
		fail("Implement Test");
	}
	
	@Test
	public void getThreadsTest() {
		javaThreadComparator.testComparatorIteratorEquals(ddrTestObjects, jextractTestObjects, "getThreads", JavaThread.class, JavaThreadComparator.JNI_ENV);
	}
	
	@Ignore
	@Test
	public void getTraceBufferTest() {
		fail("Implement Test");
	}
}
