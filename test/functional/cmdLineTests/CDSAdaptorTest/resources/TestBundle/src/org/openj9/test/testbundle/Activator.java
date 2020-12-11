/*******************************************************************************
 * Copyright (c) 2001, 2020 IBM Corp. and others
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

package org.openj9.test.testbundle;

import org.osgi.framework.Bundle;
import org.osgi.framework.BundleActivator;
import org.osgi.framework.BundleContext;

public class Activator implements BundleActivator {

	public static final String WEAVING_HOOK_BUNDLE_SYMBOLIC_NAME = "org.openj9.test.weavinghooktest";
	
	private static BundleContext context;
	
	static BundleContext getContext() {
		return context;
	}

	/*
	 * (non-Javadoc)
	 * @see org.osgi.framework.BundleActivator#start(org.osgi.framework.BundleContext)
	 */
	public void start(BundleContext bundleContext) throws Exception {
		Activator.context = bundleContext;
		
		Bundle[] bundleList = bundleContext.getBundles();
		Bundle weavingHookBundle = null;
		/* If weaving hook bundle is installed and not yet started, 
		 * start it now before accessing SomeMessageV1 class 
		 */ 
		for (int i = 0; i < bundleList.length; i++) {
			String symbolicName = bundleList[i].getSymbolicName();
			if (symbolicName.equals(WEAVING_HOOK_BUNDLE_SYMBOLIC_NAME)) {
				weavingHookBundle = bundleList[i];
				break;
			}
		}
		if ((weavingHookBundle != null) && (weavingHookBundle.getState() == Bundle.RESOLVED)) {
			weavingHookBundle.start();
		}
		
		SomeMessageV1 obj = new SomeMessageV1();
		obj.printMessage();
	}

	/*
	 * (non-Javadoc)
	 * @see org.osgi.framework.BundleActivator#stop(org.osgi.framework.BundleContext)
	 */
	public void stop(BundleContext bundleContext) throws Exception {
		Activator.context = null;
	}

}
