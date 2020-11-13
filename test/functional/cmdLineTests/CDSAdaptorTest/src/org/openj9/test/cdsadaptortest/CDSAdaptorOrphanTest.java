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

package org.openj9.test.cdsadaptortest;

import org.osgi.framework.*;
import org.osgi.framework.launch.*;

import java.util.ArrayList;
import java.util.List;

/**
 * Usage: 
 * 		CDSAdaptorOrphanTest <arguments> 
 * Following arguments are required:
 * 		-frameworkBundleLocation <location>   - location to install bundles required by the framework 
 * 		-testBundleLocation <location>        - location to install bundles required by the test 
 * Optional arguments: 
 * 		-ignoreWeavingHookBundle              - if present, do not install bundle that performs weaving
 */
class CDSAdaptorOrphanTest {

	String frameworkBundleLocation, testBundleLocation;
	boolean ignoreWeavingHookBundle;

	public static void main(String args[]) throws Exception {
		CDSAdaptorOrphanTest obj = new CDSAdaptorOrphanTest();
		if (obj.parseArgs(args) == true) {
			obj.startTest();
		}
		return;
	}

	public void startTest() throws Exception {
		OSGiFrameworkLauncher launcher = new OSGiFrameworkLauncher(
				frameworkBundleLocation);
		Framework fw = launcher.launchFramework(null);
		if (fw != null) {
			installTestBundles(launcher, testBundleLocation);
			launcher.stopFramework();
		}
		return;
	}
	
	public void printUsage() {
		System.out.println("Usage:\n\tCDSAdaptorOrphanTest -frameworkBundleLocation <loc> -testBundleLocation <loc> [-ignoreWeavingHookBundle]");
		System.out.println("\n\tSee cdsadaptortest.xml for an example");
		return;
	}

	public boolean parseArgs(String args[]) {
		for (int i = 0; i < args.length; i++) {
			if (args[i].equalsIgnoreCase("-frameworkBundleLocation")) {
				frameworkBundleLocation = args[i + 1];
			}
			if (args[i].equalsIgnoreCase("-testBundleLocation")) {
				testBundleLocation = args[i + 1];
			}
			if (args[i].equalsIgnoreCase("-ignoreWeavingHookBundle")) {
				ignoreWeavingHookBundle = true;
			}
		}
		if ((frameworkBundleLocation == null) || (testBundleLocation == null)) {
			printUsage();
			return false;
		}
		return true;
	}

	public void installTestBundles(OSGiFrameworkLauncher launcher,
			String testBundleLocation) throws Exception {
		List<String> excludeList = null;
		if (ignoreWeavingHookBundle) {
			excludeList = new ArrayList<String>();
			excludeList.add("org.openj9.test.weavinghooktest");
		}
		List<Bundle> bundleList = launcher.installBundles(testBundleLocation,
				excludeList);
		if ((bundleList != null) && (bundleList.size() != 0)) {
			for (Bundle bundle : bundleList) {
				bundle.start(0);
			}
		}
	}
}
