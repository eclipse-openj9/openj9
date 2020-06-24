/*******************************************************************************
 * Copyright (c) 2005, 2020 IBM Corp. and others
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

import org.testng.AssertJUnit;
import java.util.Collection;
import java.util.Map;

import javax.management.MBeanAttributeInfo;

public class AllManagementTests {

	public static void validateAttributeInfo(MBeanAttributeInfo info, Collection<String> ignoredAttributes,
			Map<String, AttributeData> attribs) {
		String attrName = info.getName();

		if (ignoredAttributes.contains(attrName)) {
			return;
		}

		String msg = "Invalid attribute " + attrName;
		AttributeData example = attribs.get(attrName);

		AssertJUnit.assertNotNull(msg, example);
		AssertJUnit.assertEquals(msg + " (type)", example.type, info.getType());
		AssertJUnit.assertEquals(msg + " (readable)", example.readable, info.isReadable());
		AssertJUnit.assertEquals(msg + " (writable)", example.writable, info.isWritable());
		AssertJUnit.assertEquals(msg + " (isAccessor)", example.isAccessor, info.isIs());
	}

}
