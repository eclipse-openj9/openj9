/*******************************************************************************
 * Copyright (c) 2005, 2019 IBM Corp. and others
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

import org.testng.annotations.AfterClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeClass;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.management.MemoryUsage;

import javax.management.openmbean.CompositeData;
import javax.management.openmbean.CompositeDataSupport;
import javax.management.openmbean.CompositeType;
import javax.management.openmbean.OpenDataException;
import javax.management.openmbean.OpenType;
import javax.management.openmbean.SimpleType;

// This class is not public API.
import com.ibm.java.lang.management.internal.MemoryUsageUtil;

@Test(groups = { "level.sanity" })
public class TestMemoryUsage {

	private static Logger logger = Logger.getLogger(TestMemoryUsage.class);

	private MemoryUsage goodMU;

	private static final long INIT_VAL = 1024;

	private static final long USED_VAL = 2 * 1024;

	private static final long COMMITTED_VAL = 5 * 1024;

	private static final long MAX_VAL = 10 * 1024;

	private static final String TO_STRING_VAL = "init = 1024(1K) used = 2048(2K) committed = 5120(5K) max = 10240(10K)";

	@BeforeClass
	protected void setUp() throws Exception {
		goodMU = new MemoryUsage(INIT_VAL, USED_VAL, COMMITTED_VAL, MAX_VAL);
		logger.info("Starting TestMemoryUsage tests ...");
	}

	@AfterClass
	protected void tearDown() throws Exception {
		// do nothing
	}

	/**
	 *
	 */
	@Test
	public void testGetCommitted() {
		AssertJUnit.assertEquals(COMMITTED_VAL, goodMU.getCommitted());
	}

	/**
	 *
	 */
	@Test
	public void testGetInit() {
		AssertJUnit.assertEquals(INIT_VAL, goodMU.getInit());
	}

	/**
	 *
	 */
	@Test
	public void testGetMax() {
		AssertJUnit.assertEquals(MAX_VAL, goodMU.getMax());
	}

	/**
	 *
	 */
	@Test
	public void testGetUsed() {
		AssertJUnit.assertEquals(USED_VAL, goodMU.getUsed());
	}

	/*
	 * Class under test for String toString()
	 */
	/**
	 *
	 */
	@Test
	public void testToString() {
		AssertJUnit.assertEquals(TO_STRING_VAL, goodMU.toString());
	}

	/**
	 *
	 */
	@Test
	public void testFrom() {
		CompositeData cd = MemoryUsageUtil.toCompositeData(goodMU);
		AssertJUnit.assertNotNull(cd);
		MemoryUsage mu = MemoryUsage.from(cd);
		AssertJUnit.assertNotNull(mu);

		// Verify that the MemoryUsage contains what we *think* it contains.
		AssertJUnit.assertEquals(1024, mu.getInit());
		AssertJUnit.assertEquals(5 * 1024, mu.getCommitted());
		AssertJUnit.assertEquals(10 * 1024, mu.getMax());
		AssertJUnit.assertEquals(2 * 1024, mu.getUsed());
	}

	/**
	 *
	 */
	@Test
	public void testFromWithBadInput() {
		CompositeData cd = createBadCompositeDataObject();
		AssertJUnit.assertNotNull(cd);
		try {
			MemoryUsage mu = MemoryUsage.from(cd);
			Assert.fail("Method should have thrown IllegalArgumentException");
		} catch (IllegalArgumentException e) {
			// expected
		}
	}

	/**
	 *
	 */
	@Test
	public void testConstructionFailsForBadInputs() {
		// First, check an init value less than -1...
		try {
			MemoryUsage mu1 = new MemoryUsage(-3, USED_VAL, COMMITTED_VAL, MAX_VAL);
			Assert.fail("Constructor should have thrown IllegalArgumentException");
		} catch (IllegalArgumentException e) {
			// expected
		}

		// Check max value less than -1...
		try {
			MemoryUsage mu1 = new MemoryUsage(INIT_VAL, USED_VAL, COMMITTED_VAL, -27);
			Assert.fail("Constructor should have thrown IllegalArgumentException");
		} catch (IllegalArgumentException e) {
			// expected
		}

		// Check negative used ...
		try {
			MemoryUsage mu1 = new MemoryUsage(INIT_VAL, -100, COMMITTED_VAL, MAX_VAL);
			Assert.fail("Constructor should have thrown IllegalArgumentException");
		} catch (IllegalArgumentException e) {
			// expected
		}

		// Check negative committed ...
		try {
			MemoryUsage mu1 = new MemoryUsage(INIT_VAL, USED_VAL, -32, MAX_VAL);
			Assert.fail("Constructor should have thrown IllegalArgumentException");
		} catch (IllegalArgumentException e) {
			// expected
		}

		// Check used larger than committed ...
		try {
			MemoryUsage mu1 = new MemoryUsage(INIT_VAL, USED_VAL * 1000, COMMITTED_VAL, MAX_VAL);
			Assert.fail("Constructor should have thrown IllegalArgumentException");
		} catch (IllegalArgumentException e) {
			// expected
		}

		// Check that if max has been defined, committed is not larger ...
		try {
			MemoryUsage mu1 = new MemoryUsage(INIT_VAL, USED_VAL, COMMITTED_VAL * 1000, MAX_VAL);
			Assert.fail("Constructor should have thrown IllegalArgumentException");
		} catch (IllegalArgumentException e) {
			// expected
		}

		// Check that if max has *not* been defined, committed can be larger ...
		try {
			MemoryUsage mu1 = new MemoryUsage(INIT_VAL, USED_VAL, COMMITTED_VAL * 1000, -1);
		} catch (IllegalArgumentException e) {
			Assert.fail("Constructor should not have thrown IllegalArgumentException");
		}
	}

	private static CompositeData createBadCompositeDataObject() {
		CompositeData result = null;
		String[] names = { "init", "max", "incorrect", "committed" };
		Object[] values = { new Long(100), new Long(1000), new Long(150), new Long(500) };
		CompositeType cType = createBadCompositeTypeObject();
		try {
			result = new CompositeDataSupport(cType, names, values);
		} catch (OpenDataException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		return result;
	}

	private static CompositeType createBadCompositeTypeObject() {
		CompositeType result = null;
		String[] typeNames = { "init", "incorrect", "committed", "max" };
		String[] typeDescs = { "init", "incorrect", "committed", "max" };
		OpenType[] typeTypes = { SimpleType.LONG, SimpleType.LONG, SimpleType.LONG, SimpleType.LONG };
		try {
			result = new CompositeType(MemoryUsage.class.getName(), MemoryUsage.class.getName(), typeNames, typeDescs,
					typeTypes);
		} catch (OpenDataException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		return result;
	}

}
