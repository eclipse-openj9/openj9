/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
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

package org.openj9.test.contendedfields;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import org.openj9.test.util.JavaAgent;
import org.testng.log4testng.Logger;

import com.ibm.oti.vm.VM;
import static org.testng.AssertJUnit.assertTrue;
import static org.testng.AssertJUnit.assertEquals;
import static org.testng.Assert.fail;
import static org.openj9.test.util.StringPrintStream.logStackTrace;

public class FieldUtilities  {
	protected static Logger logger = Logger.getLogger(ContendedFieldsTests.class);

	static {
		try {
			System.loadLibrary("j9ben");
		} catch (UnsatisfiedLinkError e) {}
	}

	public static void checkSize(Object testObject, long lowerLimit, long upperLimit) {
		long actualSize = JavaAgent.getObjectSize(testObject);
		String errorMessage = testObject.getClass().getName()+" expect "+lowerLimit+"<="+actualSize+"<="+upperLimit;
		logger.debug(errorMessage);

		assertTrue(errorMessage, (lowerLimit <= actualSize) && (actualSize <= upperLimit));
	}

	public static void checkObjectSize(Object testObject, int hiddenFieldsSize, int padding) {
		final Class<? extends Object> testClass = testObject.getClass();
		long fieldsSize = calculateFieldsSize(testClass) + REFERENCE_SIZE /* header */ + hiddenFieldsSize;
		fieldsSize = Math.max(16, fieldsSize); /* minimum object size */
		fieldsSize = ((fieldsSize + OBJECT_ALIGNMENT - 1)/OBJECT_ALIGNMENT) * OBJECT_ALIGNMENT;
		if (padding > 0) { /* round up to a multiple of the cache size */
			fieldsSize = ((fieldsSize + padding - 1)/padding) * padding;
		}

		long actualSize = JavaAgent.getObjectSize(testObject);
		long difference = actualSize - fieldsSize;
		final String msg = testClass.getName()+" calculated size "+fieldsSize+" actual size "+actualSize +" difference "+(difference);
		logger.debug(msg);
		if (difference != padding) { /* the actual may be one cache line size too big */
			assertTrue(msg, Math.abs(difference) == 0);
		}
	}

	
	static int REFERENCE_SIZE = getReferenceSize();
	static int OBJECT_ALIGNMENT = getObjectAlignmentInBytes();
	static int LOCKWORD_SIZE=REFERENCE_SIZE;
	static int FINALIZE_LINK_SIZE=REFERENCE_SIZE;
	public static void checkFieldAccesses(Object testObject) {
		checkFieldAccesses( testObject,  true);
	}

	private static int getReferenceSize() {		
		return VM.FJ9OBJECT_SIZE;
	}
	
	private static native int getObjectAlignmentInBytes();

	public static void checkFieldAccesses(Object testObject, boolean strict) {
		Class testClass = testObject.getClass();
		logger.debug("Checking class "+testClass.getName());
		final int intValue = 1234567898;
		final long longValue = 1234567898765432101L;
		int fieldCount = 0;
		for (Field f: testClass.getFields()) {
			try {
				String typeName = f.getType().getSimpleName();
				switch (typeName) {
				case "int": f.setInt(testObject, intValue + fieldCount);
				break;
				case "long": f.setLong(testObject, longValue + fieldCount);
				break;
				case "Object": {
					f.set(testObject, new Integer(fieldCount));
				}
				break;
				default: if (strict) {
					fail("unhandled type "+typeName+" on "+f.getName());
				}
	
				}
			} catch (IllegalAccessException e) {
				if (strict) {
					logStackTrace(e, logger);
					fail("IllegalAccessException on "+f.getName());
				}
			}
			++fieldCount;
		}
		
		synchronized (testObject) { /* force use of lockword */
			fieldCount = 0;
		}
		
		for (Field f: testClass.getFields()) {
			try {
				logger.debug("Checking "+f.getName());
				String typeName = f.getType().getSimpleName();
				switch (typeName) {
				case "int": assertEquals("error on field "+f.getName(), intValue + fieldCount, f.getInt(testObject));
				break;
				case "long": f.setLong(testObject, longValue + fieldCount);
				break;
				case "Object": {
					Object o = f.get(testObject);
					int actual = ((Integer) o).intValue();
					assertEquals("error on field "+f.getName(), fieldCount, actual);
				}
				break;
				default: if (strict) {
					fail("unhandled type "+typeName+" on "+f.getName());
				}
	
				}
			} catch (IllegalAccessException e) {
				if (strict) {
					logStackTrace(e, logger);
					fail("IllegalAccessException on "+f.getName());
				}
			}
			++fieldCount;
	
		}
	}

	static int verbose = 1;

	/**
	 * @return number of bytes used by the declared fields
	 */
	public static int calculateFieldsSize(Class testClass) {
		int numBytes = 0;
		for (Field f: testClass.getDeclaredFields()) {
			f.setAccessible(true);
			if (0 != (f.getModifiers() & Modifier.STATIC)) {
				continue;
			}
			String typeName = f.getType().getName();
			switch (typeName) {
			case "byte": 
			case "char": 
			case "float": 
			case "int": 
			case "short": 
			case "boolean": 
				numBytes += 4;
				break;
			case "double": 
			case "long": 
				numBytes += 8;
				break;
			default: 
				numBytes += REFERENCE_SIZE;
			}
		}
		Class superClass = testClass.getSuperclass();
		if (null != superClass) {
			numBytes += calculateFieldsSize(superClass);
		}
		return numBytes;
	}
}
