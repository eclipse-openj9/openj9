/*
 * Copyright IBM Corp. and others 2024
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package org.openj9.test.java.lang.management;

import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import jdk.crac.management.CRaCMXBean;
import org.testng.annotations.Test;
import org.testng.Assert;

/**
 * A class for testing CRaCMXBean.
 */
@Test(groups = { "level.sanity" })
public class TestCRaCMXBean {
	private final static CRaCMXBean cracMXBean = CRaCMXBean.getCRaCMXBean();

	@Test
	private void testCRaCMXBeanGetUptimeSinceRestore() {
		Assert.assertEquals(-1, cracMXBean.getUptimeSinceRestore(), "CRaCMXBean.getUptimeSinceRestore() is not -1");
	}

	@Test
	private void testCRaCMXBeanGetRestoreTime() {
		Assert.assertEquals(-1, cracMXBean.getRestoreTime(), "CRaCMXBean.getRestoreTime() is not -1");
	}

	@Test
	private void testCRaCMXBeanGetCRaCMXBean() {
		Assert.assertNotNull(CRaCMXBean.getCRaCMXBean(), "CRaCMXBean.getCRaCMXBean() is null");
	}

	@Test
	private void testCRaCMXBeanGetObjectName() {
		Assert.assertNotNull(cracMXBean.getObjectName(), "CRaCMXBean.getObjectName() is null");
		Assert.assertEquals(
			CRaCMXBean.CRAC_MXBEAN_NAME,
			cracMXBean.getObjectName().toString(),
			"CRaCMXBean.getObjectName().toString() is not CRaCMXBean.CRAC_MXBEAN_NAME");
	}

	@Test
	private void testCRaCMXBeanGetObjectNameThrowsInternalError() {
		String malformedCRaCMXBeanName = "InvalidObjectName!@#";
		CRaCMXBean testCRaCMXBean = new TestCRaCMXBeanImpl(malformedCRaCMXBeanName);
		try {
			ObjectName objectName = testCRaCMXBean.getObjectName();
			throw new AssertionError("CRaCMXBean.getObjectName() did not throw InternalError");
		} catch (InternalError e) {
		}
	}

	private static class TestCRaCMXBeanImpl implements CRaCMXBean {
		private final String malformedCRaCMXBeanName;

		public TestCRaCMXBeanImpl(String malformedCRaCMXBeanName) {
			this.malformedCRaCMXBeanName = malformedCRaCMXBeanName;
		}

		@Override
		public long getUptimeSinceRestore() {
			return -1;
		}

		@Override
		public long getRestoreTime() {
			return -1;
		}

		@Override
		public ObjectName getObjectName() {
			try {
				return ObjectName.getInstance(malformedCRaCMXBeanName);
			} catch (MalformedObjectNameException e) {
				throw new InternalError(e);
			}
		}
	}
}
