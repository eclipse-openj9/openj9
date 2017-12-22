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

import static org.junit.Assert.*;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Iterator;
import java.util.List;
import java.util.Properties;
import java.util.logging.Logger;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.j9ddr.util.IteratorHelpers;
import com.ibm.j9ddr.util.IteratorHelpers.IteratorFilter;
import com.ibm.j9ddr.view.dtfj.comparators.CorruptDataComparator;
import com.ibm.j9ddr.view.dtfj.test.DTFJUnitTest.InvalidObjectException;

public abstract class DTFJComparator {
	public static final int ALL_MEMBERS = 0xFFFFFFFF;
	protected static Logger log = Logger.getLogger("j9ddr.view.dtfj.test");
	
	private class InvokeResult {
		Object ddrObject;
		Object jextractObject;
		boolean shouldTest;
		
		 InvokeResult(Object ddrResult, Object jextractResult, boolean shouldTest) {
			this.ddrObject = ddrResult;
			this.jextractObject = jextractResult;
			if((ddrResult == null) && (jextractResult == null)) {
				this.shouldTest = false;		//both results are null
			} else {
				this.shouldTest = shouldTest;
			}
		}
	}
	
	public abstract void testEquals(Object ddrObject, Object jextractObject, int members);
	
	private void testProperties(Properties ddrProperties, Properties jextractProperties) {
		assertNotNull("ddr Properties should not be null", ddrProperties);
		assertEquals(jextractProperties.size(), ddrProperties.size());
		assertArrayEquals(jextractProperties.keySet().toArray(), ddrProperties.keySet().toArray());
		assertArrayEquals(jextractProperties.values().toArray(), ddrProperties.values().toArray());
	}
	
	@SuppressWarnings("unchecked")
	public final void testComparatorIteratorEquals(List<Object> ddrObjects, List<Object> jextractObjects, String methodName, Class elementClass, Comparator<Object> order) {
		assertEquals(jextractObjects.size(), ddrObjects.size());
		for (int i = 0; i < ddrObjects.size(); i++) {
			testComparatorIteratorEquals(ddrObjects.get(i), jextractObjects.get(i), methodName, elementClass, order, getDefaultMask());
		}
	}
	
	@SuppressWarnings("unchecked")
	public final void testComparatorIteratorEquals(List<Object> ddrObjects, List<Object> jextractObjects, String methodName, Class elementClass, Comparator<Object> order, int members) {
		assertEquals(jextractObjects.size(), ddrObjects.size());
		for (int i = 0; i < ddrObjects.size(); i++) {
			testComparatorIteratorEquals(ddrObjects.get(i), jextractObjects.get(i), methodName, elementClass, order, members);
		}
	}
	
	@SuppressWarnings("unchecked")
	public final void testComparatorIteratorEquals(Object ddrObject, Object jextractObject, String methodName, Class elementClass, Comparator<Object> order, int members, IteratorFilter ddrFilter, IteratorFilter jextractFilter) {
		InvokeResult invokeResult = invokeMethod(ddrObject, jextractObject, methodName, new Class[0], new Object[0]);
		if (invokeResult.shouldTest) {
			if (!(invokeResult.ddrObject instanceof Iterator)) {
				throw new IllegalArgumentException("Use testComparatorEquals to test methods that do NOT return Iterator");
			}
			
			Object[] ddrObjects = null;
			Object[] jextractObjects = null;
			
			try {
				ddrObjects = getList(elementClass, (Iterator) invokeResult.ddrObject, ddrFilter).toArray();
				jextractObjects = getList(elementClass, (Iterator) invokeResult.jextractObject, jextractFilter).toArray();
			} catch (InvalidObjectException e) {
				fail(String.format("%s.%s(): %s", ddrObject.getClass().getSimpleName(), methodName, e.getMessage()));
			}

			// TODO: Evaluation of error string may cause error.
			assertEquals(String.format("%s.%s() returned iterators of different size for %s", ddrObject.getClass().getSimpleName(), methodName, ddrObject), jextractObjects.length, ddrObjects.length);
			
			if (order != null) {
				Arrays.sort(ddrObjects, order);
				Arrays.sort(jextractObjects, order);
			}
			
			for (int i = 0; i < ddrObjects.length; i++) {
				if (!assertEqualsIfBothObjectsCorrupt(jextractObjects[i], ddrObjects[i])) {
					testEquals(ddrObjects[i], jextractObjects[i], members);
				}
			}
		}
	}
	
	public final void testComparatorIteratorEquals(Object ddrObject, Object jextractObject, String methodName, Class elementClass, Comparator<Object> order, int members) {
		testComparatorIteratorEquals(ddrObject, jextractObject, methodName, elementClass, order, members, null, null);
	}
	
	/**
	 * Calls methodName on ddrObject and jextractObject.
	 * asserts that if one object throws either a DataUnavailable or CorruptData then the other object
	 * must throw the same exception.
	 * 
	 * Otherwise calls DTFJComparator.test(Object ddrObject, Object jextractObject) on the results 
	 * 
	 * @param ddrObject
	 * @param jextractObject
	 * @param methodName
	 */
	
	@SuppressWarnings("unchecked")
	public final void testComparatorIteratorEquals(List<Object> ddrObjects, List<Object> jextractObjects, String methodName, Class elementClass, int members) {
		testComparatorIteratorEquals(ddrObjects, jextractObjects, methodName, elementClass, null, members);
	}
	
	@SuppressWarnings("unchecked")
	public final void testComparatorIteratorEquals(Object ddrObject, Object jextractObject, String methodName, Class elementClass, int members) {
		testComparatorIteratorEquals(ddrObject, jextractObject, methodName, elementClass, null, members);
	}
	
	@SuppressWarnings("unchecked")
	public final void testComparatorIteratorEquals(List<Object> ddrObjects, List<Object> jextractObjects, String methodName, Class elementClass) {
		assertEquals(jextractObjects.size(), ddrObjects.size());
		for (int i = 0; i < ddrObjects.size(); i++) {
			testComparatorIteratorEquals(ddrObjects.get(i), jextractObjects.get(i), methodName, elementClass, getDefaultMask());
		}
	}

	@SuppressWarnings("unchecked")
	public final void testComparatorIteratorEquals(Object ddrObject, Object jextractObject, String methodName, Class elementClass) {
		testComparatorIteratorEquals(ddrObject, jextractObject, methodName, elementClass, getDefaultMask());
	}
	
	public final void testComparatorEquals(List<Object> ddrObjects, List<Object> jextractObjects, String methodName) {
		assertEquals(jextractObjects.size(), ddrObjects.size());
		for (int i = 0; i < ddrObjects.size(); i++) {
			testComparatorEquals(ddrObjects.get(i), jextractObjects.get(i), methodName, getDefaultMask());
		}
	}
	
	public final void testComparatorEquals(Object ddrObject, Object jextractObject, String methodName) {
		testComparatorEquals(ddrObject, jextractObject, methodName, getDefaultMask());
	}
	
	@SuppressWarnings("unchecked")
	public final void testComparatorEquals(Object ddrObject, Object jextractObject, String methodName, Class[] paramTypes, Object[] args) {
		testComparatorEquals(ddrObject, jextractObject, methodName, getDefaultMask(), paramTypes, args);
	}
	
	@SuppressWarnings("unchecked")
	public final void testComparatorEquals(List<Object> ddrObjects, List<Object> jextractObjects, String methodName, Class[] paramTypes, Object[] args) {
		testComparatorEquals(ddrObjects, jextractObjects, methodName, getDefaultMask(), paramTypes, args);
	}
	
	public final void testComparatorEquals(List<Object> ddrObjects, List<Object> jextractObjects, String methodName, int members) {
		testComparatorEquals(ddrObjects, jextractObjects, methodName, members, new Class[0], new Object[0]);
	}
	
	@SuppressWarnings("unchecked")
	public final void testComparatorEquals(List<Object> ddrObjects, List<Object> jextractObjects, String methodName, int members, Class[] paramTypes, Object[] args) {
		assertEquals(jextractObjects.size(), ddrObjects.size());
		for (int i = 0; i < ddrObjects.size(); i++) {
			testComparatorEquals(ddrObjects.get(i), jextractObjects.get(i), methodName, members, paramTypes, args);
		}
	}
	
	public final void testComparatorEquals(Object ddrObject, Object jextractObject, String methodName, int members) {
		testComparatorEquals(ddrObject, jextractObject, methodName, members, new Class[0], new Object[0]);
	}
	
	@SuppressWarnings("unchecked")
	public final void testComparatorEquals(Object ddrObject, Object jextractObject, String methodName, int members, Class[] paramTypes, Object[] args) {
		InvokeResult invokeResult = invokeMethod(ddrObject, jextractObject, methodName, paramTypes, args);
		if (invokeResult.shouldTest) {
			if (invokeResult.ddrObject instanceof Iterator) {
				throw new IllegalArgumentException(String.format("%s.%s() returned an Iterator.  Use testComparatorIteratorEquals()", ddrObject.getClass().getSimpleName(), methodName));
			}
			testEquals(invokeResult.ddrObject, invokeResult.jextractObject, members);
		}
	}
	
	public final void testPropertiesEquals(Object ddrObject, Object jextractObject, String methodName) {
		InvokeResult invokeResult = invokeMethod(ddrObject, jextractObject, methodName, new Class[0], new Object[0]);
		if (invokeResult.shouldTest) {
			testProperties((Properties) invokeResult.ddrObject, (Properties) invokeResult.jextractObject); 
		}
	}
	
	public final void testPropertiesEquals(List<Object> ddrObjects, List<Object> jextractObjects, String methodName) {
		for (int i = 0; i < ddrObjects.size(); i++) {
			testPropertiesEquals(ddrObjects.get(i), jextractObjects.get(i), methodName);
		}
	}
	
	public final void testJavaEquals(List<Object> ddrObjects, List<Object> jextractObjects, String methodName) {
		testJavaEquals(ddrObjects, jextractObjects, methodName, new Class[0], new Object[0]);
	}
	
	@SuppressWarnings("unchecked")
	public final void testJavaEquals(List<Object> ddrObjects, List<Object> jextractObjects, String methodName, Class[] paramTypes, Object[] args) {
		assertNotNull("DDR object should not be null", ddrObjects);
		assertNotNull("JExtract object should not be null", jextractObjects);
		assertEquals("Lists are not the same size", jextractObjects.size(),  ddrObjects.size());
		for (int i = 0; i < ddrObjects.size(); i++) {
			testJavaEquals(ddrObjects.get(i), jextractObjects.get(i), methodName, paramTypes, args);
		}
	}
	
	public final void testJavaEquals(Object ddrObject, Object jextractObject, String methodName) {
		testJavaEquals(ddrObject, jextractObject, methodName, new Class[0], new Object[0]);
	}
	
	@SuppressWarnings("unchecked")
	public final void testJavaEquals(Object ddrObject, Object jextractObject, String methodName, Class[] paramTypes, Object[] args) {
		assertNotNull("DDR object should not be null", ddrObject);
		assertNotNull("JExtract object should not be null", jextractObject);
		InvokeResult invokeResult = invokeMethod(ddrObject, jextractObject, methodName, paramTypes, args);
		if (invokeResult.shouldTest) {
			assertEquals(String.format("%s.%s() does not return the same result", ddrObject.getClass().getSimpleName(), methodName), invokeResult.jextractObject, invokeResult.ddrObject);
		}
	}
	
	@SuppressWarnings("unchecked")
	private InvokeResult invokeMethod(Object ddrObject, Object jextractObject, String methodName, Class[] paramTypes, Object[] args) {
		
		if (assertEqualsIfBothObjectsCorrupt(jextractObject, ddrObject)) {
			return new InvokeResult(null, null, false);
		}
		
		boolean ddrDataUnavailable = false;
		boolean jextractDataUnavailable = false;
		boolean ddrCorruptData = false;
		boolean jextractCorruptData = false;
		boolean ddrUnsupportedOperation = false;
		@SuppressWarnings("unused")
		boolean jextractUnsupportedOperation = false;
		Object ddrResult = false;
		Object jextractResult = false;
		
		Method method;
		Throwable ddrException = null;
		Throwable jextractException = null;
		
		try {
			method = ddrObject.getClass().getMethod(methodName, paramTypes);
			method.setAccessible(true);
			ddrResult = method.invoke(ddrObject, args);
		} catch (SecurityException e) {
			e.printStackTrace();
			fail(String.format("Could not find method %s.%s()", ddrObject.getClass().getName(), methodName));
		} catch (NoSuchMethodException e) {
			e.printStackTrace();
			fail(String.format("Could not find method %s.%s()", ddrObject.getClass().getName(), methodName));
		} catch (IllegalArgumentException e) {
			e.printStackTrace();
			fail(String.format("Could not find method %s.%s()", ddrObject.getClass().getName(), methodName));
		} catch (IllegalAccessException e) {
			e.printStackTrace();
			fail(String.format("Could not find method %s.%s()", ddrObject.getClass().getName(), methodName));
		} catch (InvocationTargetException e) {
			if (e.getCause() instanceof DataUnavailable) {
//				e.getCause().printStackTrace();
				ddrException = e.getCause();
				ddrDataUnavailable = true;
			} else if (e.getCause() instanceof CorruptDataException) {
//				e.getCause().printStackTrace();
				ddrException = e.getCause();
				ddrCorruptData = true;
			} else {
				fail(String.format("Unexpected exception %s from method %s.%s()", e.getCause(), ddrObject.getClass().getName(), methodName));
			}
		}
		
		try {
			method = jextractObject.getClass().getMethod(methodName, paramTypes);
			method.setAccessible(true);
			jextractResult = method.invoke(jextractObject, args);
		} catch (SecurityException e) {
			e.printStackTrace();
			fail(String.format("Could not find method %s.%s()", jextractObject.getClass().getName(), methodName));
		} catch (NoSuchMethodException e) {
			e.printStackTrace();
			fail(String.format("Could not find method %s.%s()", jextractObject.getClass().getName(), methodName));
		} catch (IllegalArgumentException e) {
			e.printStackTrace();
			fail(String.format("Could not find method %s.%s()", jextractObject.getClass().getName(), methodName));
		} catch (IllegalAccessException e) {
			e.printStackTrace();
			fail(String.format("Could not find method %s.%s()", jextractObject.getClass().getName(), methodName));
		} catch (InvocationTargetException e) {
			if (e.getCause() instanceof DataUnavailable) {
				jextractException = e.getCause();
				jextractDataUnavailable = true;
			} else if (e.getCause() instanceof CorruptDataException) {
				jextractException = e.getCause();
				jextractCorruptData = true;
			} else {
				fail(String.format("Unexpected exception %s from method %s.%s()", e.getCause(), ddrObject.getClass().getName(), methodName));
			}
		}
		
		if (ddrDataUnavailable && ! jextractDataUnavailable) {
			if (ddrException != null) {
				ddrException.printStackTrace();
			}
			
			if (jextractException != null) {
				jextractException.printStackTrace();
			}
			
			fail(String.format("%s.%s returned DataUnavailable. JExtract did not.", ddrObject.getClass().getSimpleName(),methodName));
		}
		
		if(jextractCorruptData && !ddrCorruptData) {
			//in this case DDR found data and jextract threw a corrupt data exception, so mark it as do not test
			return new InvokeResult(ddrResult, jextractResult, false);			
		} else {
		
			assertEquals(String.format("%s.%s() returned CorruptData for %s", ddrObject.getClass().getSimpleName(), methodName, ddrCorruptData ? "DDR" : "JEXTRACT"), jextractCorruptData, ddrCorruptData);
		
			return new InvokeResult(ddrResult, jextractResult, !jextractDataUnavailable && !ddrDataUnavailable && !ddrCorruptData && !ddrUnsupportedOperation);
		}
	}
	
	public int getDefaultMask() {
		return ALL_MEMBERS;
	}
	
	@SuppressWarnings("unchecked")
	public static List<Object> getList(Class<?> clazz, Iterator iter, IteratorFilter filter) throws InvalidObjectException {
		ArrayList<Object> result = new ArrayList<Object>();
		if(null != filter) {
			iter = IteratorHelpers.filterIterator(iter, filter);
		}
		Object obj = null;
		while (iter.hasNext()) {
			obj = iter.next();
			if((clazz.isInstance(obj)) || (obj instanceof CorruptData)) {
				result.add(obj);
			} else {
				throw new InvalidObjectException(String.format("Expected %s but was %s", clazz.getSimpleName(), obj.getClass().getSimpleName()));
			} 
		}
			
		return result;
	}
	
	@SuppressWarnings("unchecked")
	public static List<Object> getList(Class<?> clazz, Iterator iter) throws InvalidObjectException {
		return getList(clazz, iter, null);
	}

	// If one object is corrupt then both objects MUST be corrupt and both objects MUST be equal
	// Returns true if both objects where corrupt, false otherwise.
	private static boolean assertEqualsIfBothObjectsCorrupt(Object a, Object b) {
		if (a instanceof CorruptData && b instanceof CorruptData) {
			CorruptDataComparator comp = new CorruptDataComparator();
			comp.testEquals(b, a, comp.getDefaultMask());
			return true;
		}
		
		assertFalse("This object should have been corrupt", a instanceof CorruptData);
		assertFalse("This object should have been corrupt", b instanceof CorruptData);
		return false;
	}
}
