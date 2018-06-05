/*******************************************************************************
 * Copyright (c) 2005, 2018 IBM Corp. and others
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

package org.openj9.test.java.lang.management;

import org.openj9.test.util.VersionCheck;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeClass;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.management.MonitorInfo;

import javax.management.openmbean.CompositeData;
import javax.management.openmbean.CompositeDataSupport;
import javax.management.openmbean.CompositeType;
import javax.management.openmbean.OpenDataException;
import javax.management.openmbean.OpenType;
import javax.management.openmbean.SimpleType;

@Test(groups = { "level.sanity" })
public class TestMonitorInfo {

	private static Logger logger = Logger.getLogger(TestMonitorInfo.class);

	protected MonitorInfo goodMI;

	protected Object lock;

	protected int idHashCode;

	protected String lockName;

	protected int stackDepth;

	protected StackTraceElement stackFrame;

	protected final static String GOOD_CD_CLASSNAME = "java.lang.StringBuilder";

	protected final static int GOOD_CD_IDHASHCODE = System.identityHashCode(new StringBuilder());

	protected final static int GOOD_CD_STACKDEPTH = 29;

	@BeforeClass
	protected void setUp() throws Exception {
		lock = new Object();
		idHashCode = System.identityHashCode(lock);
		lockName = lock.getClass().getName();
		stackDepth = 42;
		stackFrame = new StackTraceElement("apple.orange.Banana", "doSomething", "Banana.java", 100);
		goodMI = new MonitorInfo(lock.getClass().getName(), System.identityHashCode(lock), stackDepth, stackFrame);
		logger.info("Starting TestMonitorInfo tests ...");
	}

	/*
	 * Test method for 'java.lang.management.MonitorInfo.MonitorInfo(String,
	 * int, int, StackTraceElement)'
	 */
	@Test
	public void testMonitorInfo() {
		AssertJUnit.assertEquals(lockName, goodMI.getClassName());
		AssertJUnit.assertEquals(idHashCode, goodMI.getIdentityHashCode());
		AssertJUnit.assertEquals(stackDepth, goodMI.getLockedStackDepth());

		// Null class name should throw a NPE ...
		try {
			MonitorInfo mi = new MonitorInfo(null, System.identityHashCode(null), stackDepth, stackFrame);
			Assert.fail("Should have thrown NPE");
		} catch (NullPointerException e) {
			// expected
		}

		// Null stack frame should throw a IllegalArgumentException ...
		try {
			logger.info("hi");
			MonitorInfo mi = new MonitorInfo(lock.getClass().getName(), System.identityHashCode(lock), stackDepth,
					null);
			Assert.fail("Should have thrown IllegalArgumentException");
		} catch (IllegalArgumentException e) {
			// expected
		}
	}

	/*
	 * Test method for 'java.lang.management.MonitorInfo.getLockedStackDepth()'
	 */
	@Test
	public void testGetLockedStackDepth() {
		AssertJUnit.assertEquals(stackDepth, goodMI.getLockedStackDepth());

		// Negative stack trace depth is allowed ...
		MonitorInfo mi = new MonitorInfo(lock.getClass().getName(), System.identityHashCode(lock), -100, null);
	}

	/*
	 * Test method for 'java.lang.management.MonitorInfo.getLockedStackFrame()'
	 */
	@Test
	public void testGetLockedStackFrame() {
		AssertJUnit.assertEquals(stackFrame, goodMI.getLockedStackFrame());
	}

	/*
	 * Test method for 'java.lang.management.MonitorInfo.from(CompositeData)'
	 */
	@Test
	public void testFrom() {
		CompositeData cd = createGoodCompositeData();
		AssertJUnit.assertNotNull(cd);
		MonitorInfo mi = MonitorInfo.from(cd);

		// Verify the recovered MonitorInfo. First, the inherited state and
		// behaviour...
		AssertJUnit.assertEquals(GOOD_CD_CLASSNAME, mi.getClassName());
		AssertJUnit.assertEquals(GOOD_CD_IDHASHCODE, mi.getIdentityHashCode());
		AssertJUnit.assertEquals(GOOD_CD_CLASSNAME + "@" + Integer.toHexString(GOOD_CD_IDHASHCODE), mi.toString());

		// Verify the MonitorInfo state
		AssertJUnit.assertEquals(GOOD_CD_STACKDEPTH, mi.getLockedStackDepth());
		StackTraceElement st = mi.getLockedStackFrame();
		AssertJUnit.assertNotNull(st);
		AssertJUnit.assertEquals("Blobby.java", st.getFileName());
		AssertJUnit.assertEquals("foo.bar.Blobby", st.getClassName());
		AssertJUnit.assertEquals(2100, st.getLineNumber());
		AssertJUnit.assertEquals("takeOverWorld", st.getMethodName());
	}

	/*
	 * Test method for 'java.lang.management.MonitorInfo.from(CompositeData)'
	 */
	@Test
	public void testFrom_WithNullData() throws Exception {
		AssertJUnit.assertNull(MonitorInfo.from(null));
	}

	/*
	 * Test method for 'java.lang.management.MonitorInfo.from(CompositeData)'
	 */
	/*
	public void testFrom_WithCDContainingNullClassName() throws Exception {
	    CompositeData cd = createCompositeDataContainingNullClassName();
	    AssertJUnit.assertNotNull(cd);
	    MonitorInfo mi = MonitorInfo.from(cd);
	    AssertJUnit.assertNull(mi.getClassName());
	    assertEquals(GOOD_CD_IDHASHCODE, mi.getIdentityHashCode());
	    assertEquals(GOOD_CD_STACKDEPTH, mi.getLockedStackDepth());
	    assertEquals(null + "@" + Integer.toHexString(GOOD_CD_IDHASHCODE), mi
	            .toString());
	    StackTraceElement st = mi.getLockedStackFrame();
	    AssertJUnit.assertNotNull(st);
	    assertEquals("Blobby.java", st.getFileName());
	    assertEquals("foo.bar.Blobby", st.getClassName());
	    assertEquals(2100, st.getLineNumber());
	    assertEquals("takeOverWorld", st.getMethodName());
	}
	*/

	/*
	 * Test method for 'java.lang.management.MonitorInfo.from(CompositeData)'
	 */
	@Test
	public void testFrom_WithCDContainingNullStackFrame() throws Exception {
		CompositeData cd = createCompositeDataContainingNullStackFrame();
		AssertJUnit.assertNotNull(cd);
		try {
			MonitorInfo mi = MonitorInfo.from(cd);
			Assert.fail("Should have thrown IllegalArgumentException");
		} catch (IllegalArgumentException e) {
			// expected
			AssertJUnit.assertEquals("Parameter stackDepth is 29 but stackFrame is null", e.getMessage());
		}
	}

	/*
	 * Test method for 'java.lang.management.MonitorInfo.from(CompositeData)'
	 */
	@Test
	public void testFrom_WithBadData() throws Exception {
		CompositeData badCd = createBadCompositeData();
		AssertJUnit.assertNotNull(badCd);
		try {
			MonitorInfo li = MonitorInfo.from(badCd);
			Assert.fail("Should have thrown an IllegalArgumentException");
		} catch (IllegalArgumentException e) {
			// Expected
		}
	}

	/**
	 * Returns a new <code>CompositeData</code> instance which has been
	 * intentionally constructed with attributes that <i>should</i> prevent the
	 * creation of a new <code>MonitorInfo</code>.
	 *
	 * @return a new <code>CompositeData</code> instance representing a
	 *         <code>MonitorInfo</code>.
	 */
	private CompositeData createBadCompositeData() {
		CompositeData result = null;
		String[] names = { "className", "identityHashCode", "lockedStackFrame", "lockedStackDepth" };
		// Intentionally mixing up the order in which the values are entered
		// i.e. lockedStackDepth and lockedStackFrame have been switched around
		Object[] values = {
				/* className */new String(GOOD_CD_CLASSNAME),
				/* identityHashCode */new Integer(GOOD_CD_IDHASHCODE),
				/* lockedStackDepth */new Integer(GOOD_CD_STACKDEPTH),
				/* lockedStackFrame */createGoodStackTraceElementCompositeData() };
		CompositeType cType = createBadMonitorInfoCompositeTypeObject();
		try {
			result = new CompositeDataSupport(cType, names, values);
		} catch (OpenDataException e) {
			e.printStackTrace();
		}
		return result;
	}

	/**
	 * Returns a new <code>CompositeType</code> instance which has been
	 * intentionally constructed with attributes that <i>should</i> prevent the
	 * creation of a new <code>MonitorInfo</code>.
	 *
	 * @return <code>CompositeType</code> for use when wrapping up
	 *         <code>MonitorInfo</code> objects in <code>CompositeData</code>s.
	 */
	private CompositeType createBadMonitorInfoCompositeTypeObject() {
		CompositeType result = null;
		try {
			String[] typeNames = { "className", "identityHashCode", "lockedStackFrame", "lockedStackDepth" };
			String[] typeDescs = { "className", "identityHashCode", "lockedStackFrame", "lockedStackDepth" };
			// Intentionally mixing up the order in which the values are entered
			// i.e. lockedStackDepth and lockedStackFrame have been switched
			// around
			OpenType[] typeTypes = {
					SimpleType.STRING,
					SimpleType.INTEGER,
					SimpleType.INTEGER,
					createStackTraceElementCompositeTypeObject() };
			result = new CompositeType(MonitorInfo.class.getName(), MonitorInfo.class.getName(), typeNames, typeDescs,
					typeTypes);
		} catch (OpenDataException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		return result;
	}

	private CompositeData createCompositeDataContainingNullStackFrame() {
		CompositeData result = null;
		String[] names = { "className", "identityHashCode", "lockedStackFrame", "lockedStackDepth" };
		Object[] values = {
				/* className */new String(GOOD_CD_CLASSNAME),
				/* identityHashCode */new Integer(GOOD_CD_IDHASHCODE),
				/* lockedStackFrame */null,
				/* lockedStackDepth */new Integer(GOOD_CD_STACKDEPTH) };
		CompositeType cType = createGoodMonitorInfoCompositeType();
		try {
			result = new CompositeDataSupport(cType, names, values);
		} catch (OpenDataException e) {
			e.printStackTrace();
		}
		return result;
	}

	private CompositeData createCompositeDataContainingNullClassName() {
		CompositeData result = null;
		String[] names = { "className", "identityHashCode", "lockedStackFrame", "lockedStackDepth" };
		Object[] values = {
				/* className */null,
				/* identityHashCode */new Integer(GOOD_CD_IDHASHCODE),
				/* lockedStackFrame */createGoodStackTraceElementCompositeData(),
				/* lockedStackDepth */new Integer(GOOD_CD_STACKDEPTH) };
		CompositeType cType = createGoodMonitorInfoCompositeType();
		try {
			result = new CompositeDataSupport(cType, names, values);
		} catch (OpenDataException e) {
			e.printStackTrace();
		}
		return result;
	}

	/*
	 * Test method for 'java.lang.management.MonitorInfo.getClassName()'
	 */
	@Test
	public void testGetClassName() {
		MonitorInfo mi = new MonitorInfo("foo.Bar", 0, stackDepth, stackFrame);
		AssertJUnit.assertEquals("foo.Bar", mi.getClassName());
		// Verify that case matters
		AssertJUnit.assertTrue(!mi.getClassName().equals("foo.bar"));

		// Even an empty string is allowed for the class name
		mi = new MonitorInfo("", 0, stackDepth, stackFrame);
		AssertJUnit.assertEquals("", mi.getClassName());

	}

	/*
	 * Test method for 'java.lang.management.MonitorInfo.getIdentityHashCode()'
	 */
	@Test
	public void testGetIdentityHashCode() {
		MonitorInfo mi = new MonitorInfo("foo.Bar", 4882, stackDepth, stackFrame);
		AssertJUnit.assertEquals(4882, mi.getIdentityHashCode());

		// Negative identity hash codes are allowed
		mi = new MonitorInfo("foo.Bar", -222, stackDepth, stackFrame);
		AssertJUnit.assertEquals(-222, mi.getIdentityHashCode());
	}

	/*
	 * Test method for 'java.lang.management.MonitorInfo.toString()'
	 */
	@Test
	public void testToString() {
		AssertJUnit.assertEquals(lockName + "@" + Integer.toHexString(idHashCode), goodMI.toString());
	}

	/**
	 * @return a new <code>CompositeData</code> instance representing a
	 *         <code>MonitorInfo</code>.
	 */
	static CompositeData createGoodCompositeData() {
		CompositeData result = null;
		String[] names = { "className", "identityHashCode", "lockedStackFrame", "lockedStackDepth" };
		Object[] values = {
				/* className */new String(GOOD_CD_CLASSNAME),
				/* identityHashCode */new Integer(GOOD_CD_IDHASHCODE),
				/* lockedStackFrame */createGoodStackTraceElementCompositeData(),
				/* lockedStackDepth */new Integer(GOOD_CD_STACKDEPTH) };
		CompositeType cType = createGoodMonitorInfoCompositeType();
		try {
			result = new CompositeDataSupport(cType, names, values);
		} catch (OpenDataException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		return result;
	}

	/**
	 * @return new <code>CompositeData</code> representing a
	 *         <code>StackTraceElement</code>.
	 */
	public static CompositeData createGoodStackTraceElementCompositeData() {
		CompositeData result = null;
		CompositeType cType = createStackTraceElementCompositeTypeObject();
		String[] names;
		Object[] values;
		if (VersionCheck.major() >= 9) {
			String[] namesTmp = TestMisc.getJava9STEMethodNames();
			Object[] valuesTmp = {
					new String("foo.bar.Blobby"),
					new String("takeOverWorld"),
					new String("Blobby.java"),
					new Integer(2100),
					new Boolean(false),
					new String("myModuleName"),
					new String("myModuleVersion"),
					new String("myClassLoaderName") };
			names = namesTmp;
			values = valuesTmp;
		} else {
			String[] namesTmp = { "className", "methodName", "fileName", "lineNumber", "nativeMethod" };
			Object[] valuesTmp = {
					new String("foo.bar.Blobby"),
					new String("takeOverWorld"),
					new String("Blobby.java"),
					new Integer(2100),
					new Boolean(false) };
			names = namesTmp;
			values = valuesTmp;
		}
		try {
			result = new CompositeDataSupport(cType, names, values);
		} catch (OpenDataException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		return result;
	}

	private static CompositeType createStackTraceElementCompositeTypeObject() {
		CompositeType result = null;
		String[] typeNames;
		String[] typeDescs;
		OpenType[] typeTypes;
		if (VersionCheck.major() >= 9) {
			String[] typeNamesTmp = TestMisc.getJava9STEMethodNames();
			String[] typeDescsTmp = TestMisc.getJava9STEMethodNames();
			OpenType[] typeTypesTmp = {
					SimpleType.STRING,
					SimpleType.STRING,
					SimpleType.STRING,
					SimpleType.INTEGER,
					SimpleType.BOOLEAN,
					SimpleType.STRING,
					SimpleType.STRING,
					SimpleType.STRING };
			typeNames = typeNamesTmp;
			typeDescs = typeDescsTmp;
			typeTypes = typeTypesTmp;
		} else {
			String[] typeNamesTmp = { "className", "methodName", "fileName", "lineNumber", "nativeMethod" };
			String[] typeDescsTmp = { "className", "methodName", "fileName", "lineNumber", "nativeMethod" };
			OpenType[] typeTypesTmp = {
					SimpleType.STRING,
					SimpleType.STRING,
					SimpleType.STRING,
					SimpleType.INTEGER,
					SimpleType.BOOLEAN };
			typeNames = typeNamesTmp;
			typeDescs = typeDescsTmp;
			typeTypes = typeTypesTmp;
		}
		try {
			result = new CompositeType(StackTraceElement.class.getName(), StackTraceElement.class.getName(), typeNames,
					typeDescs, typeTypes);
		} catch (OpenDataException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		return result;
	}

	/**
	 * @return <code>CompositeType</code> for use when wrapping up
	 *         <code>MonitorInfo</code> objects in <code>CompositeData</code>s.
	 */
	static CompositeType createGoodMonitorInfoCompositeType() {
		CompositeType result = null;
		try {
			String[] typeNames = { "className", "identityHashCode", "lockedStackFrame", "lockedStackDepth" };
			String[] typeDescs = { "className", "identityHashCode", "lockedStackFrame", "lockedStackDepth" };
			OpenType[] typeTypes = {
					SimpleType.STRING,
					SimpleType.INTEGER,
					createStackTraceElementCompositeTypeObject(),
					SimpleType.INTEGER };
			result = new CompositeType(MonitorInfo.class.getName(), MonitorInfo.class.getName(), typeNames, typeDescs,
					typeTypes);
		} catch (OpenDataException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		return result;
	}

}
