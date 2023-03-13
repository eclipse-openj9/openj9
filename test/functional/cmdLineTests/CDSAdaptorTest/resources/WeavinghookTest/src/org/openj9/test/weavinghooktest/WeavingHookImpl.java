/*******************************************************************************
 * Copyright IBM Corp. and others 2001
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

package org.openj9.test.weavinghooktest;

import org.osgi.framework.Bundle;
import org.osgi.framework.BundleContext;
import org.osgi.framework.hooks.weaving.WeavingHook;
import org.osgi.framework.hooks.weaving.WovenClass;

public class WeavingHookImpl implements WeavingHook {

	public static final String TEST_BUNDLE_SYMBOLIC_NAME = "org.openj9.test.testbundle";
	
	BundleContext context;
	
	WeavingHookImpl(BundleContext context) {
		this.context = context;
	}
	
	/**
	 * Replaces class bytes of org.openj9.test.testbundle.SomeMessageV1 loaded as part of bundle org.openj9.test.testbundle
	 * with that of org.openj9.test.testbundle.SomeMessageV2.
	 */
	public void weave(WovenClass wovenClass) {
		String className = wovenClass.getClassName();
		if (className.equals("org.openj9.test.testbundle.SomeMessageV1")) {
			Bundle testBundle = null;
			Bundle[] bundles = context.getBundles();
			for (int i = 0; i < bundles.length; i++) {
				String symbolicName = bundles[i].getSymbolicName();
				if (symbolicName.equals(TEST_BUNDLE_SYMBOLIC_NAME)) {
					testBundle = bundles[i];
					break;
				}
			}
			byte[] newBytes = Util.getClassBytes(testBundle, "org.openj9.test.testbundle.SomeMessageV2");
			if (newBytes != null) {
				newBytes = Util.replaceClassNames(newBytes, "SomeMessageV1", "SomeMessageV2");
				if (newBytes != null) {
					wovenClass.setBytes(newBytes);
				}
			}
		}
	}
}
