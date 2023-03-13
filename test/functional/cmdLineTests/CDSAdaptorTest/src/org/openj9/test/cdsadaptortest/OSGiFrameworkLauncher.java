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

package org.openj9.test.cdsadaptortest;

import org.osgi.framework.*;
import org.osgi.framework.launch.*;
import org.osgi.framework.Constants;
import java.util.HashMap;
import java.util.ArrayList;
import java.io.File;
import java.util.List;
import java.util.Iterator;
import java.util.ServiceLoader;

/**
 * This class is used to lauch OSGi framework and to install bundles.
 */
class OSGiFrameworkLauncher {
	private String frameworkBundleLocation;

	private Framework framework;
	private BundleContext fwBundleContext;

	private HashMap<String, String> osgiPropertyMap = new HashMap<>();

	OSGiFrameworkLauncher(String bundleLocation) {
		frameworkBundleLocation = bundleLocation;
	}

	/**
	 * Launches Equinox framework.
	 * 
	 * @param map
	 *            A map of properties to be used while launching the framework.
	 *            Can be null.
	 */
	public Framework launchFramework(HashMap<String, String> map)
			throws Exception {
		if (map != null) {
			osgiPropertyMap = map;
		}
		
		/* Constants.FRAMEWORK_STORAGE_CLEAN is used to clear OSGi cache */
		osgiPropertyMap.put(Constants.FRAMEWORK_STORAGE_CLEAN, "onFirstInit");

		/* Following is a standard way to launch OSGi framework using Java Service Provider Configuration.
		 * It requires framework implementation (in this case org.eclipse.osgi bundle) to be on the classpath.  
		 */
		ServiceLoader<FrameworkFactory> sl = ServiceLoader
				.load(FrameworkFactory.class);
		Iterator<FrameworkFactory> it = sl.iterator();
		if (it.hasNext()) {
			framework = it.next().newFramework(osgiPropertyMap);
			if (framework != null) {
				framework.init();
				fwBundleContext = framework.getBundleContext();
				if (installBundles(frameworkBundleLocation, null) == null) {
					System.err
							.println("Error: Failed to install bundles required by the framework.");
				} else {
					framework.start();
					return framework;
				}
			} else {
				System.err.println("Error: Failed to create new framework");
			}
		} else {
			System.err
					.println("Error: Failed to load framework factory via service loader");
		}
		return null;
	}

	public void stopFramework() throws Exception {
		framework.stop();
		framework.waitForStop(0);
	}

	/**
	 * 
	 * @param bundleStoreDir
	 *            Location from where bundles are to be installed.
	 * @param excludeList
	 *            List of bundles not to be installed. Bundles are specified by
	 *            their symbolic name. Pass null if no bundle is to be skipped.
	 * @return List of bundles installed properly. Returns null if any of the
	 *         bundle in <code>bundleStoreDir<code> is not installed.
	 * @throws Exception
	 */
	public List<Bundle> installBundles(String bundleStoreDir,
			List<String> excludeList) throws Exception {
		List<Bundle> bundleList = null;
		File storeDir = new File(bundleStoreDir);
		if (storeDir.isDirectory()) {
			bundleList = new ArrayList<Bundle>();
			File[] bundleFiles = storeDir.listFiles();
			System.out.println("Installing bundles in dir: " + bundleStoreDir);
			for (int i = 0; i < bundleFiles.length; i++) {
				if (bundleFiles[i].getName().endsWith(".jar")) {
					if (excludeList != null) {
						/* Check if current bundle needs to be excluded */
						boolean excluded = false;
						for (String excludeBundle : excludeList) {
							if (bundleFiles[i].getName().startsWith(
									excludeBundle)) {
								System.out.println("Bundle skipped: "
										+ bundleFiles[i].toURI().toString());
								excluded = true;
								break;
							}
						}
						if (excluded) {
							continue;
						}
					}
					System.out.println("Bundle to be installed: "
							+ bundleFiles[i].toURI().toString());
					Bundle bundle = fwBundleContext
							.installBundle(bundleFiles[i].toURI().toString());
					if (bundle != null) {
						bundleList.add(bundle);
					} else {
						/*
						 * Return null on failure to install a bundle. Test will
						 * most probably fail if the required bundles are not
						 * installed.
						 */
						System.out.println("Error: Failed to install bundle: "
								+ bundleFiles[i].getName());
						return null;
					}
				}
			}
		} else {
			System.out.println("Error: " + bundleStoreDir
					+ " is not a directory");
		}
		return bundleList;
	}
}
