/*******************************************************************************
 * Copyright (c) 2015, 2019 IBM Corp. and others
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

import static org.openj9.test.contendedfields.FieldUtilities.FINALIZE_LINK_SIZE;
import static org.openj9.test.contendedfields.FieldUtilities.LOCKWORD_SIZE;
import static org.openj9.test.contendedfields.FieldUtilities.REFERENCE_SIZE;

import java.lang.management.ManagementFactory;
import java.lang.ref.SoftReference;
import java.lang.ref.WeakReference;
import java.lang.reflect.Method;
import java.math.BigInteger;

import org.testng.annotations.AfterMethod;
import org.testng.annotations.BeforeMethod;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.openj9.test.contendedfields.TestClasses.*;

@Test(groups = { "level.extended" })
public class ContendedFieldsTests {
	private static boolean jep142Restricted = true;
	private static int paddingIncrement;
	private static boolean vmConfigInitialized = false;
	private static int CACHE_LINE_SIZE = -1;
	private static final  int SINGLE_SIZE = 4;
	protected String testName;
	protected static Logger logger = Logger.getLogger(ContendedFieldsTests.class);

	@SuppressWarnings("nls")
	static void getVMConfig() { /* ideally we should have an mxbean to get this information */
		if (vmConfigInitialized) {
			return;
		}
		CACHE_LINE_SIZE = 64;
		String osArch = System.getProperty("os.arch");
		if (osArch.startsWith("ppc")) {
			CACHE_LINE_SIZE = 128;
		} else if (osArch.startsWith("s390")) {
			CACHE_LINE_SIZE = 256;			
		} else if (osArch.startsWith("amd64") || osArch.startsWith("x86")) {
			CACHE_LINE_SIZE = 64;			
		}
		jep142Restricted = true;
		for (String vmarg: ManagementFactory.getRuntimeMXBean().getInputArguments()) {
			switch (vmarg) {
			case "-XX:-RestrictContended": 
				jep142Restricted = false;
				break;
			case "-XX:+RestrictContended":
			case "-XX:-ContendedFields":
				jep142Restricted = true;
				break;				
			}
		}
		paddingIncrement = jep142Restricted ? 0 : CACHE_LINE_SIZE;
		logger.debug("architecture = " + osArch + " reference size = "+REFERENCE_SIZE+" cache line size = " + CACHE_LINE_SIZE+" object alignment="+FieldUtilities.OBJECT_ALIGNMENT+" contended fields "+(jep142Restricted? "": "un")+"restricted");
		logger.debug("=============================================================================");
		vmConfigInitialized = true;
	}


	/**
	 * Run with both:
	 * -Xlockword:none,mode=all
	 * -Xlockword:none,mode=minimizeFootprint
	 * 
	 */

	@AfterMethod
	protected void tearDown() throws Exception {
		System.gc();
		System.runFinalization(); /* make sure the finalizer links work */
	}

	@BeforeMethod
	protected void setUp(Method testMethod) throws Exception {
		getVMConfig();
		testName = testMethod.getName();
		logger.debug("starting " + testName);
	}

	public void testPlainObject() {
		Object testObject = new Object();
		FieldUtilities.checkObjectSize(testObject, LOCKWORD_SIZE, 0);
		FieldUtilities.checkFieldAccesses(testObject);
	}

	public void testString() {
		Object testObject = new String();
		FieldUtilities.checkObjectSize(testObject, 0, 0);
		FieldUtilities.checkFieldAccesses(testObject, false);
	}

	public void testMultipleField() {
		Object testObjects[] = {new ClassWithOneInt(), new ClassWithTwoInt(), new ClassWithThreeInt(), new ClassWithFourInt()};
		for (Object testObject: testObjects) {
			FieldUtilities.checkObjectSize(testObject, LOCKWORD_SIZE, 0);
			FieldUtilities.checkFieldAccesses(testObject);
		}
	}

	public void testBigContendedClass() {
		Object testObjects[] = {new ClassWithManyFields(), new ContendedSubclassOfClassWithManyFields(), new ContendedSubclassWithmanyFieldsOfClassWithManyFields()};
		for (Object testObject: testObjects) {
			FieldUtilities.checkObjectSize(testObject, LOCKWORD_SIZE, 0);
			FieldUtilities.checkFieldAccesses(testObject);
		}
	}

	public void testFinalizableMultipleField() {
		Object testObject = new MultipleFieldFinalizableClass();
		FieldUtilities.checkFieldAccesses(testObject, false);
		FieldUtilities.checkObjectSize(testObject, LOCKWORD_SIZE +FINALIZE_LINK_SIZE, 0);
	}

	public void testBigInteger() {
		Object testObject = new BigInteger("42");
		FieldUtilities.checkFieldAccesses(testObject, false);
		FieldUtilities.checkObjectSize(testObject, LOCKWORD_SIZE, 0);
	}

	public void testCrypto() {
		Object testObjects[] = new Crypto[4];
		for (int i = 0; i < testObjects.length; ++i) {
			testObjects[i] = new Crypto();
		}
		for (int i = 0; i < testObjects.length; ++i) {
			FieldUtilities.checkFieldAccesses(testObjects[i], false);
			FieldUtilities.checkObjectSize(testObjects[i], LOCKWORD_SIZE+SINGLE_SIZE, 0); /* two hidden fields */
		}
	}

	public void testMultipleContField() {
		Object testObjects[] = {new MultipleContFieldClass(), new OneContFieldClass(),
				new TwoContFieldClass(), new ThreeContFieldClass()};
		for (Object testObject: testObjects) {
			FieldUtilities.checkObjectSize(testObject, LOCKWORD_SIZE, 0);
			FieldUtilities.checkFieldAccesses(testObject);
		}
	}

	public void testMultipleFieldContClass() {
		Object testObject = new MultipleFieldContClass();
		FieldUtilities.checkObjectSize(testObject, LOCKWORD_SIZE, paddingIncrement);
		FieldUtilities.checkFieldAccesses(testObject);
	}

	public void testMultipleContFieldContClass() {
		Object testObject = new MultipleContFieldContClass();
		FieldUtilities.checkObjectSize(testObject, LOCKWORD_SIZE, paddingIncrement);
		FieldUtilities.checkFieldAccesses(testObject);
	}

	public void testFinalizableMultipleFieldContClass() {
		Object testObject = new FinalizableMultipleFieldContClass();
		FieldUtilities.checkObjectSize(testObject, LOCKWORD_SIZE + FINALIZE_LINK_SIZE, paddingIncrement);
		FieldUtilities.checkFieldAccesses(testObject);
	}


	public void testContentionGroups() {
		Object testObjects[] = {new ContGroupClass(), new ContGroupsClass()};
		for (Object testObject: testObjects) {
			FieldUtilities.checkObjectSize(testObject, LOCKWORD_SIZE, 0);
			FieldUtilities.checkFieldAccesses(testObject);
		}
	}

	public void testContentionGroupsAndClass() {
		Object testObjects[] = {new ContGroupContClass(), new ContGroupsContClass()};
		for (Object testObject: testObjects) {
			FieldUtilities.checkObjectSize(testObject, LOCKWORD_SIZE, paddingIncrement);
			FieldUtilities.checkFieldAccesses(testObject);
		}
	}

	public void testFinalizableSubclassOFinalizable() {
		Object testObjects[] = {new SubclassOfSingleFieldFinalizableClass(), new SubclassOfDoubleFieldFinalizableClass()};
		for (Object testObject: testObjects) {
			FieldUtilities.checkObjectSize(testObject,  LOCKWORD_SIZE + FINALIZE_LINK_SIZE, 0);
			FieldUtilities.checkFieldAccesses(testObject);
		}
	}

	public void testNonFinalizableSubclassOFinalizable() {
		Object testObjects[] = {new NonFinalizableSubclassOfSingleFieldFinalizableClass(), new NonFinalizableSubclassOfDoubleFieldFinalizableClass()};
		for (Object testObject: testObjects) {
			FieldUtilities.checkObjectSize(testObject,  LOCKWORD_SIZE + FINALIZE_LINK_SIZE, 0); /* all subclasses of finalizable classes are finalizable, even if the finalize method is empty */
			FieldUtilities.checkFieldAccesses(testObject);
		}
	}

	public void testFinalizableSubclassOfNonFinalizable() {
		Object testObjects[] = {new FinalizableSubclassOfSingleFieldClass(), new FinalizableSubclassOfDoubleFieldClass()};
		for (Object testObject: testObjects) {
			FieldUtilities.checkObjectSize(testObject,  LOCKWORD_SIZE + FINALIZE_LINK_SIZE, 0);
			FieldUtilities.checkFieldAccesses(testObject);
		}
	}

	public void testFinalizableSubclassOfFinalizable() {
		Object testObjects[] = {new FinalizableSubclassOfSingleFieldFinalizableClass(), new FinalizableSubclassOfDoubleFieldFinalizableClass()};
		for (Object testObject: testObjects) {
			FieldUtilities.checkObjectSize(testObject,  LOCKWORD_SIZE + FINALIZE_LINK_SIZE, 0);
			FieldUtilities.checkFieldAccesses(testObject);
		}
	}

	public void testSubclassOfBackfillableAndNonBackfillable() {
		Object testObjects[] = {new SubclassOfSingleFieldClass(), new SubclassOfDoubleFieldClass()};
		for (Object testObject: testObjects) {
			FieldUtilities.checkObjectSize(testObject, LOCKWORD_SIZE, 0);
			FieldUtilities.checkFieldAccesses(testObject);
		}
	}

	public void testReferenceClasses() {
		Object testObjects[];

		{
			Object testReferent = new Object();
			Object temp[] = {new WeakReference<Object>(testReferent), new SoftReference<Object>(testReferent), new OneDoubleReferenceClass(testReferent), new OneSingleReferenceClass(testReferent), new OneDoubleOneSingleReferenceClass(testReferent)};
			testObjects = temp;
			System.gc();
			System.runFinalization();
		}

		for (Object testObject: testObjects) {
			FieldUtilities.checkObjectSize(testObject, LOCKWORD_SIZE + REFERENCE_SIZE, 0);
			FieldUtilities.checkFieldAccesses(testObject);
		}
	}

	public void testFinalizableReferenceClasses() {
		Object testObjects[];

		{
			Object testReferent = new Object();
			Object temp[] = {new OneDoubleFinalizableReferenceClass(testReferent), new OneDoubleOneSingleFinalizableReferenceClass(testReferent)};
			testObjects = temp;
			System.gc();
			System.runFinalization();
		}
		for (Object testObject: testObjects) {
			FieldUtilities.checkObjectSize(testObject, LOCKWORD_SIZE + FINALIZE_LINK_SIZE +  REFERENCE_SIZE, 0);
			FieldUtilities.checkFieldAccesses(testObject);
		}
	}

	public void testSubclassWithLockwordOfClassWithNoLockword() {
		SubclassWithLockwordOfClassWithNoLockword testObject = new SubclassWithLockwordOfClassWithNoLockword();
		FieldUtilities.checkObjectSize(testObject, LOCKWORD_SIZE, 0);
		FieldUtilities.checkFieldAccesses(testObject);
	}
}
