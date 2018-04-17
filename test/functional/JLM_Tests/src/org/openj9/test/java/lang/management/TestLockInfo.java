/*******************************************************************************
 * Copyright (c) 2013, 2018 IBM Corp. and others
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

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeClass;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.management.LockInfo;

import javax.management.openmbean.CompositeData;
import javax.management.openmbean.CompositeDataSupport;
import javax.management.openmbean.CompositeType;
import javax.management.openmbean.OpenDataException;
import javax.management.openmbean.OpenType;
import javax.management.openmbean.SimpleType;

@Test(groups = { "level.sanity" })
public class TestLockInfo {

	private static Logger logger = Logger.getLogger(TestLockInfo.class);

	protected LockInfo goodLI;

	protected Object lock;

	protected int idHashCode;

	protected String lockName;

	protected final static String GOOD_CD_CLASSNAME = "java.lang.StringBuilder";

	protected final static int GOOD_CD_IDHASHCODE = System.identityHashCode(new StringBuilder());

	@BeforeClass
	protected void setUp() throws Exception {
		lock = new Object();
		idHashCode = System.identityHashCode(lock);
		lockName = lock.getClass().getName();
		goodLI = new LockInfo(lock.getClass().getName(), System.identityHashCode(lock));
		logger.info("Starting TestLockInfo tests ...");
	}

	/*
	 * Test method for 'java.lang.management.LockInfo.LockInfo(String, int)'
	 */
	@Test
	public void testLockInfo() throws Exception {
		AssertJUnit.assertEquals(lockName, goodLI.getClassName());
		AssertJUnit.assertEquals(idHashCode, goodLI.getIdentityHashCode());

		// The bad case ...
		try {
			LockInfo li = new LockInfo(null, System.identityHashCode(null));
			Assert.fail("Should have thrown NPE");
		} catch (NullPointerException e) {
			// expected
		}
	}

	/*
	 * Test method for 'java.lang.management.LockInfo.getClassName()'
	 */
	@Test
	public void testGetClassName() {
		LockInfo li = new LockInfo("foo.Bar", 0);
		AssertJUnit.assertEquals("foo.Bar", li.getClassName());
		// Verify that case matters
		AssertJUnit.assertTrue(!li.getClassName().equals("foo.bar"));

		// Even an empty string is allowed for the class name
		li = new LockInfo("", 0);
		AssertJUnit.assertEquals("", li.getClassName());
	}

	/*
	 * Test method for 'java.lang.management.LockInfo.getIdentityHashCode()'
	 */
	@Test
	public void testGetIdentityHashCode() {
		LockInfo li = new LockInfo("foo.Bar", 42);
		AssertJUnit.assertEquals(42, li.getIdentityHashCode());

		// Negative identity hash codes are allowed
		li = new LockInfo("foo.Bar", -222);
		AssertJUnit.assertEquals(-222, li.getIdentityHashCode());
	}

	/*
	 * Test method for 'java.lang.management.LockInfo.toString()'
	 */
	@Test
	public void testToString() {
		AssertJUnit.assertEquals(lockName + "@" + Integer.toHexString(idHashCode), goodLI.toString());
	}

	/**
	 * @return a new <code>CompositeData</code> instance representing a
	 *         <code>LockInfo</code>.
	 */
	static CompositeData createGoodCompositeData() {
		CompositeData result = null;
		String[] names = { "className", "identityHashCode" };
		Object[] values = {
				/* className */new String(GOOD_CD_CLASSNAME),
				/* identityHashCode */new Integer(GOOD_CD_IDHASHCODE) };
		CompositeType cType = createGoodLockInfoCompositeType();
		try {
			result = new CompositeDataSupport(cType, names, values);
		} catch (OpenDataException e) {
			e.printStackTrace();
		}
		return result;
	}

	/**
	 * Returns a new <code>CompositeData</code> instance which has been
	 * intentionally constructed with attributes that <i>should</i> prevent the
	 * creation of a new <code>LockInfo</code>.
	 *
	 * @return a new <code>CompositeData</code> instance representing a
	 *         <code>LockInfo</code>.
	 */
	private static CompositeData createBadCompositeData() {
		CompositeData result = null;
		String[] names = { "className", "identityHashCode" };
		// intentionally mix up the order of the values...
		Object[] values = {
				/* className */new Integer(GOOD_CD_IDHASHCODE),
				/* identityHashCode */new String(GOOD_CD_CLASSNAME) };
		CompositeType cType = createBadLockInfoCompositeTypeObject();
		try {
			result = new CompositeDataSupport(cType, names, values);
		} catch (OpenDataException e) {
			e.printStackTrace();
		}
		return result;
	}

	/**
	 * Returns a new <code>CompositeData</code> instance which has been
	 * intentionally constructed with a null class name value that <i>should</i>
	 * prevent the creation of a new <code>LockInfo</code>.
	 *
	 * @return a new <code>CompositeData</code> instance representing a
	 *         <code>LockInfo</code>.
	 */
	private static CompositeData createCompositeDataContainingNullClassName() {
		CompositeData result = null;
		String[] names = { "className", "identityHashCode" };
		// intentionally mix up the order of the values...
		Object[] values = { /* className */null, /* identityHashCode */new Integer(GOOD_CD_IDHASHCODE) };
		CompositeType cType = createGoodLockInfoCompositeType();
		try {
			result = new CompositeDataSupport(cType, names, values);
		} catch (OpenDataException e) {
			e.printStackTrace();
		}
		return result;
	}

	/**
	 * @return <code>CompositeType</code> for use when wrapping up
	 *         <code>LockInfo</code> objects in <code>CompositeData</code>s.
	 */
	static CompositeType createGoodLockInfoCompositeType() {
		CompositeType result = null;
		try {
			String[] typeNames = { "className", "identityHashCode" };
			String[] typeDescs = { "className", "identityHashCode" };
			OpenType[] typeTypes = { SimpleType.STRING, SimpleType.INTEGER };
			result = new CompositeType(LockInfo.class.getName(), LockInfo.class.getName(), typeNames, typeDescs,
					typeTypes);
		} catch (OpenDataException e) {
			e.printStackTrace();
			Assert.fail("Unexpected exception occurred.");
		}
		return result;
	}

	/**
	 * Returns a new <code>CompositeType</code> instance which has been
	 * intentionally constructed with attributes that <i>should</i> prevent the
	 * creation of a new <code>LockInfo</code>.
	 *
	 * @return <code>CompositeType</code> for use when wrapping up
	 *         <code>LockInfo</code> objects in <code>CompositeData</code>s.
	 */
	private static CompositeType createBadLockInfoCompositeTypeObject() {
		CompositeType result = null;
		try {
			String[] typeNames = { "className", "identityHashCode" };
			String[] typeDescs = { "className", "identityHashCode" };
			// intentionally mix up the order of the values...
			OpenType[] typeTypes = { SimpleType.INTEGER, SimpleType.STRING };
			result = new CompositeType(LockInfo.class.getName(), LockInfo.class.getName(), typeNames, typeDescs,
					typeTypes);
		} catch (OpenDataException e) {
			e.printStackTrace();
		}
		return result;
	}
}
