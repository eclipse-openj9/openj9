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
import java.util.HashMap;
import java.util.ArrayList;
import java.io.File;
import java.util.List;
import java.util.Iterator;
import java.util.ServiceLoader;

/**
 * This class is used to lauch OSGi framework and to install bundles.
 * 
 * @author administrator
 *
 */
class OSGiFrameworkLauncher {
	private String frameworkBundleLocation;

	private Framework framework;
	private BundleContext fwBundleContext;

	private HashMap<String, String> osgiPropertyMap = null;

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
		
		/* osgi.clean is used to clear OSGi cache */
		System.setProperty("osgi.clean", "true");
		System.setProperty("osgi.hook.configurators.include",
				"com.ibm.cds.CDSHookConfigurator");

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