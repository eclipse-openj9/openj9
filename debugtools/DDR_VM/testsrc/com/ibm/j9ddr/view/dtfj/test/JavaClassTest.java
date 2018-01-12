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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.Iterator;
import java.util.List;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaField;
import com.ibm.dtfj.java.JavaMethod;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaReference;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.java.JavaStackFrame;
import com.ibm.j9ddr.view.dtfj.comparators.JavaClassComparator;
import com.ibm.j9ddr.view.dtfj.comparators.JavaFieldComparator;
import com.ibm.j9ddr.view.dtfj.comparators.StringComparator;

public class JavaClassTest extends DTFJUnitTest {

	public static final Comparator<Object> DECLARED_FIELDS_SORT_ORDER = new Comparator<Object>() {
		public int compare(Object o1, Object o2) {
			try {
				return ((JavaField) o1).getName().compareTo(((JavaField) o2).getName());
			} catch (CorruptDataException e) {
				fail("Test Setup Error");
				return 0;
			}
		}
	};
	
	public static final Comparator<Object> CONSTANCT_POOL_REFERENCES_SORT_ORDER = new Comparator<Object>() {
		public int compare(Object o1, Object o2) {
			return new Long(((JavaObject) o1).getID().getAddress()).compareTo(new Long(((JavaObject) o2).getID().getAddress()));
		}
	};
	
	public static final Comparator<Object> REFERENCES_SORT_ORDER = new Comparator<Object>() {
		
		private String getTargetCompareString(Object obj) {
			JavaReference ref = (JavaReference) obj;
			
			Object targetObj = null;
			try {
				targetObj = ref.getTarget();
			} catch (DataUnavailable e) {
				fail("Test Setup Error");
				return "";
			} catch (CorruptDataException e) {
				fail("Test Setup Error");
				return "";
			}
			
			if (targetObj instanceof JavaObject) {
				JavaObject targetJavaObj = (JavaObject) targetObj;
				ImagePointer imagePointer = targetJavaObj.getID();
				
				String result = String.valueOf(imagePointer.getAddress());
				result = result + imagePointer.getAddressSpace().toString();
				return result;
			} 
			
			if (targetObj instanceof JavaClass) {
				JavaClass targetJavaClass = (JavaClass) targetObj;
				ImagePointer imagePointer = targetJavaClass.getID();
				
				String result = String.valueOf(imagePointer.getAddress());
				result = result + imagePointer.getAddressSpace().toString();
				return result;
			} 
			fail("Test Setup Error");
			return "";
		}
		
		// TODO: Despite documentation JavaRefernece.getSource() can also return a JavaRuntime. 
		private String getSourceCompareString(Object obj) {
			JavaReference ref = (JavaReference) obj;
			
			Object sourceObj = null;
			try {
				sourceObj = ref.getSource();
			} catch (DataUnavailable e) {
				fail("Test Setup Error");
				return "";
			} catch (CorruptDataException e) {
				fail("Test Setup Error");
				return "";
			}
			
			if (sourceObj == null) {
				return "null";
			}
			
			if (sourceObj instanceof JavaObject) {
				JavaObject targetJavaObj = (JavaObject) sourceObj;
				ImagePointer imagePointer = targetJavaObj.getID();
				
				String result = String.valueOf(imagePointer.getAddress());
				result = result + imagePointer.getAddressSpace().toString();
				return result;
			} 
			
			if (sourceObj instanceof JavaClass) {
				JavaClass targetJavaClass = (JavaClass) sourceObj;
				ImagePointer imagePointer = targetJavaClass.getID();
				
				String result = String.valueOf(imagePointer.getAddress());
				result = result + imagePointer.getAddressSpace().toString();
				return result;
			}
			
			if (sourceObj instanceof JavaStackFrame) {
				JavaStackFrame targetJavaStackFrame = (JavaStackFrame) sourceObj;
				ImagePointer imagePointer;
				try {
					imagePointer = targetJavaStackFrame.getBasePointer();
				} catch (CorruptDataException e) {
					fail("Test Setup Error");
					return "";
				}
				
				String result = String.valueOf(imagePointer.getAddress());
				result = result + imagePointer.getAddressSpace().toString();
				return result;
			}
			
			if (sourceObj instanceof JavaRuntime) {
				JavaRuntime targetJavaRuntime = (JavaRuntime) sourceObj;
			
				
				ImagePointer imagePointer;
				try {
					imagePointer = 	targetJavaRuntime.getJavaVM();
				} catch (CorruptDataException e) {
					fail("Test Setup Error");
					return "";
				}
				
				String result = String.valueOf(imagePointer.getAddress());
				result = result + imagePointer.getAddressSpace().toString();
				return result;
			}
			
			fail("Test Setup Error");
			return "";
		}
		
		public int compare(Object o1, Object o2) {
			String result1 = "";
			result1 += getSourceCompareString(o1);
			result1 += getTargetCompareString(o1);
			
			String result2 = "";
			result2 += getSourceCompareString(o2);
			result2 += getTargetCompareString(o2);
			
			return result1.compareTo(result2);
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
	
	protected void loadTestObjects(JavaRuntime ddrRuntime, List<Object> ddrObjects, JavaRuntime jextractRuntime, List<Object> jextractObjects)
	{
		List<Object> ddrClassLoaders = new ArrayList<Object>();
		List<Object> jextractClassLoaders = new ArrayList<Object>();
		
		fillLists(ddrClassLoaders, ddrRuntime.getJavaClassLoaders(), jextractClassLoaders, jextractRuntime.getJavaClassLoaders(), null);

		for (int i = 0; i < ddrClassLoaders.size(); i++) {
			JavaClassLoader ddrClassLoader = (JavaClassLoader) ddrClassLoaders.get(i);
			JavaClassLoader jextractClassLoader = (JavaClassLoader) jextractClassLoaders.get(i);
			
			fillLists(ddrObjects, ddrClassLoader.getDefinedClasses(), jextractObjects, jextractClassLoader.getDefinedClasses(), null);
		}
	}
	
	@Test
	public void equalsTest() {
		equalsTest(ddrTestObjects, ddrClonedObjects, jextractTestObjects);
	}
	
	@Test
	public void hashCodeTest() {
		hashCodeTest(ddrTestObjects, ddrClonedObjects);
	}
	
	@Test
	public void getNameTest() {
		javaClassComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getName"); 
	}
	
	@Test
	public void getClassLoaderTest() {
		javaClassLoaderComparator.testComparatorEquals(ddrTestObjects, jextractTestObjects, "getClassLoader");
	}
	
	@Test
	public void getComponentTypeTest() {
		for (int i = 0; i < ddrTestObjects.size(); i++) {
			JavaClass ddrClass = (JavaClass) ddrTestObjects.get(i);
			JavaClass jextractClass = (JavaClass) jextractTestObjects.get(i);
			
			boolean isArray = false;
			try {
				assertEquals(jextractClass.isArray(), ddrClass.isArray());
				isArray = ddrClass.isArray();
			} catch (CorruptDataException e1) {
				fail("Problem with test setup");
			}
			
			if (isArray) {
				javaClassComparator.testComparatorEquals(ddrClass, jextractClass, "getComponentType"); 
			} else {
				JavaClass result = null;
				try {
					result = ddrClass.getComponentType();
				} catch (IllegalArgumentException e) {
					// Expected exception
				} catch (CorruptDataException e) {
					fail("getcComponentType threw unexpected CorruptDataException");
				}
				
				try {
					assertNull(String.format("%s is not an array and should have thrown an IllegalArgumentException", ddrClass.getName()), result);
				} catch (CorruptDataException e) {
					fail("getcComponentType threw unexpected CorruptDataException");
				}
			}
		}
	}
	
	@Test
	public void anyClassIsArray() {
		boolean foundArray = false;
		for (int i = 0; i < ddrTestObjects.size(); i++) {
			JavaClass ddrClass = (JavaClass) ddrTestObjects.get(i);
			try {
				if (ddrClass.isArray()) {
					foundArray = true;
					break;
				}
			} catch (CorruptDataException e) {
				// Do nothing ... not testing this aspect
			}
		}
		assertTrue("No class under test was an array", foundArray);
	}
	
	@Test
	public void getConstantPoolReferencesTest() {
		javaObjectComparator.testComparatorIteratorEquals(ddrTestObjects, jextractTestObjects, "getConstantPoolReferences", JavaObject.class, CONSTANCT_POOL_REFERENCES_SORT_ORDER);
	}
	
	@Test
	public void getDeclaredFieldsTest() {
		javaFieldComparator.testComparatorIteratorEquals(ddrTestObjects, jextractTestObjects, "getDeclaredFields", JavaField.class, DECLARED_FIELDS_SORT_ORDER, JavaFieldComparator.NAME);
	}
	
	@Test
	public void getDeclaredMethodsTest() {
		javaMethodComparator.testComparatorIteratorEquals(ddrTestObjects, jextractTestObjects, "getDeclaredMethods", JavaMethod.class);
	}
	
	@Test
	public void getIDTest() {
		imagePointerComparator.testComparatorEquals(ddrTestObjects, jextractTestObjects, "getID");
	}
	
	@Test
	public void getInterfacesTest() {
		new StringComparator().testComparatorIteratorEquals(ddrTestObjects, jextractTestObjects, "getInterfaces", String.class, null, JavaClassComparator.INTERFACES);
	}
	
	@Test
	public void anyClassHasMoreThanOneInterface() {
		boolean foundMultipleInterfaces = false;
		
		for (int i = 0; i < ddrTestObjects.size(); i++) {
			Iterator<Object> iter = ddrTestObjects.iterator();
			int count = 0;
			while (iter.hasNext() && count < 2) {
				iter.next();
				count++;
			}
			
			if (count == 2) {
				foundMultipleInterfaces = true;
				break;
			}
		}
		assertTrue("No class tested had more than one interface", foundMultipleInterfaces);
	}
	
	@Test
	public void getModifiersTest() {
		javaClassComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getModifiers");
	}
	
	@Test
	public void getObjectTest() {
		javaObjectComparator.testComparatorEquals(ddrTestObjects, jextractTestObjects, "getObject");
	}
	
	@Test
	public void getReferencesTest() {
		javaReferenceComparator.testComparatorIteratorEquals(ddrTestObjects, jextractTestObjects, "getReferences", JavaReference.class, REFERENCES_SORT_ORDER);
	}
	
	@Test
	public void isArrayTest() {
		javaClassComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "isArray");
	}
}
