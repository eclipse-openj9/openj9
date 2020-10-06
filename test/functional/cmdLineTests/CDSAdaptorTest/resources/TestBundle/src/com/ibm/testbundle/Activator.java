package com.ibm.testbundle;

import org.osgi.framework.Bundle;
import org.osgi.framework.BundleActivator;
import org.osgi.framework.BundleContext;

public class Activator implements BundleActivator {

	public static final String WEAVING_HOOK_BUNDLE_SYMBOLIC_NAME = "com.ibm.weavinghooktest";
	
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
