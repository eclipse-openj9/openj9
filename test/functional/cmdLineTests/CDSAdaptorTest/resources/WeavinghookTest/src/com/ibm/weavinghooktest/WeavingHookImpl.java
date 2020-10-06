package com.ibm.weavinghooktest;

import org.osgi.framework.Bundle;
import org.osgi.framework.BundleContext;
import org.osgi.framework.hooks.weaving.WeavingHook;
import org.osgi.framework.hooks.weaving.WovenClass;

public class WeavingHookImpl implements WeavingHook {

	public static final String TEST_BUNDLE_SYMBOLIC_NAME = "com.ibm.testbundle";
	
	BundleContext context;
	
	WeavingHookImpl(BundleContext context) {
		this.context = context;
	}
	
	/**
	 * Replaces class bytes of com.ibm.testbundle.SomeMessageV1 loaded as part of bundle com.ibm.testbundle
	 * with that of com.ibm.testbundle.SomeMessageV2.
	 */
	public void weave(WovenClass wovenClass) {
		String className = wovenClass.getClassName();
		if (className.equals("com.ibm.testbundle.SomeMessageV1")) {
			Bundle testBundle = null;
			Bundle[] bundles = context.getBundles();
			for (int i = 0; i < bundles.length; i++) {
				String symbolicName = bundles[i].getSymbolicName();
				if (symbolicName.equals(TEST_BUNDLE_SYMBOLIC_NAME)) {
					testBundle = bundles[i];
					break;
				}
			}
			byte[] newBytes = Util.getClassBytes(testBundle, "com.ibm.testbundle.SomeMessageV2");
			if (newBytes != null) {
				newBytes = Util.replaceClassNames(newBytes, "SomeMessageV1", "SomeMessageV2");
				if (newBytes != null) {
					wovenClass.setBytes(newBytes);
				}
			}
		}
	}
}
