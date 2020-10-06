/*******************************************************************************
 * Licensed Materials - Property of IBM
 * "Restricted Materials of IBM"
 *
 * (c) Copyright IBM Corp. 2001, 2012 All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or disclosure
 * restricted by GSA ADP Schedule Contract with IBM Corp.
 *******************************************************************************/
package com.ibm.cdsadaptortest;

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
 * 
 * @author administrator
 * 
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
			excludeList.add("com.ibm.weavinghooktest");
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