/*[INCLUDE-IF CRIU_SUPPORT]*/
/*******************************************************************************
 * Copyright (c) 2022, 2022 IBM Corp. and others
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

package openj9.internal.criu.security;

import java.lang.reflect.Method;
import java.security.Security;
import java.util.HashMap;
import java.util.Map;
import java.util.Properties;
import java.util.*;
import java.security.Provider;
import java.security.Provider.Service;

import sun.security.action.GetPropertyAction;
import sun.security.jca.ProviderList;
import sun.security.jca.Providers;
import openj9.internal.criu.CRIUSECProvider;

/**
 * Configures the security providers when in CRIU mode.
 */
public final class CRIUConfigurator {
	private static Properties systemProps;
	/** Stores the old security providers (position, name). */
	private static final HashMap<String, String> oldProviders = new HashMap<>();
	/** Tracing for CRIUSEC. */
	private static final boolean debug = Boolean.parseBoolean(
		 GetPropertyAction.privilegedGetProperty("enable.j9internal.checkpoint.security.api.debug", "false"));
	private static HashMap<String, Set<String>> cachedAlgorithms = new HashMap<>();
	private static boolean isCacheEligible = false;

	/**
	 * Removes the usual security providers and adds the CRIU security provider.
	 *
	 * @param props the system properties
	 */
	public static void setCRIUSecMode(Properties props) {
		systemProps = props;
		
		// Get all configured security providers and save all unique services and their associated algorithms
		// into a cache of type HashMap. When cache has been populated remove all the providers we loaded.
		Provider[] providerList = Security.getProviders();
		for (Provider provider: providerList) {
			if (debug) {
				System.out.println("Name: "  + provider.getName());
				System.out.println("Information:\n" + provider.getInfo());
			}
			// If the SunPKCS11 provider is configured we do not want to populate an algorithm cache. The
			// reason being that the algorithms associated with the SunPKCS11 provider are based upon the 
			// reported mechanisms available in the systems PKCS11 interface. For CRIU we will avoid caching 
			// all the various algorithms and their services since the cache will not be accurate on a 
			// CRIU restore on a different system where we have potentially different PKCS11 mechanisms configured.
			if (provider.getName().equalsIgnoreCase("SunPKCS11")) {
				if (provider.isConfigured()) {
					if (debug) {
						System.out.println("SunPKCS11 provider is configured. Disable algorithm caching.");
					}
					cachedAlgorithms.clear();
					break;
				} else {
					if (debug) {
						System.out.println("SunPKCS11 provider is not configured. Allow algorithm caching.");
					}
				}
			}
			Set<Service> serviceList = provider.getServices();
			for (Service service: serviceList) {
				String serviceName = service.getType();
				if (cachedAlgorithms.get(serviceName) == null) {
					Set<String> algs = Security.getAlgorithms(serviceName);
					cachedAlgorithms.put(serviceName, algs);
					if (debug) {
						System.out.println("Cached algorithms with service name <" + serviceName + ">:\n" + algs);
					}
				}
			}
		}

		//Remove all providers.
		for (Provider provider: providerList) {
			Security.removeProvider(provider.getName());
		}
		
		// Save the old security.provider key such that when we later perform a CRIU restore we know what providers
		// to restore.
		for (Map.Entry<Object, Object> entry : props.entrySet()) {
			String key = (String) entry.getKey();
			if (key.startsWith("security.provider.")) {
				oldProviders.put(key, (String) entry.getValue());
			}
		}
		for (String provider : oldProviders.keySet()) {
			props.remove(provider);
		}
		props.put("security.provider.1", "openj9.internal.criu.CRIUSECProvider");
		Security.insertProviderAt(new CRIUSECProvider(),1);
		if (debug) {
			System.out.println("CRIUSEC provider added and all other security providers removed.");
		}
	}

	/**
	 * Removes the CRIU security provider and adds the usual security providers
	 * back.
	 */
	public static void setCRIURestoreMode() {
		if (null != systemProps) {
			Security.removeProvider("CRIUSEC");
			// Note that CRIUSEC was set as security.provider.1 in the method
			// setCRIUSecMode, which is called before this method.
			systemProps.remove("security.provider.1");
			if (debug) {
				System.out.println("CRIUSEC provider removed.");
			}
			for (Map.Entry<String, String> entry : oldProviders.entrySet()) {
				systemProps.put(entry.getKey(), entry.getValue());
			}
			try {
				// Invoke the fromSecurityProperties method from the ProviderList class.
				Class<?> runnable = Class.forName("sun.security.jca.ProviderList", true,
						ClassLoader.getSystemClassLoader());
				Method readProperties = runnable.getDeclaredMethod("fromSecurityProperties");
				readProperties.setAccessible(true);
				ProviderList providerList = (ProviderList) readProperties.invoke(null);
				Providers.setProviderList(providerList);
			} catch (Exception e) {
				System.out.println(e.toString());
			}
			if (debug) {
				for (String provider : oldProviders.values()) {
					System.out.println(provider + " restored.");
				}
			}
			// If any algorithms are available from when we took a checkpoint in our algorithm cache
			// we can now allow callers to get values from this cache.
			if (cachedAlgorithms.size() > 0) {
				if (debug) {
					System.out.println("Algorithm cache enabled.");
				}
			    isCacheEligible = true;
			}
		}
	}

	/**
	 * Gets algorithms from the checkpointed cache of algorithms.
	 * 
	 * @param serviceName
	 */
	public static Set<String> getAlgorithms(String serviceName) {
		if ((serviceName == null) || (serviceName.isEmpty())) {
			return Collections.emptySet();
		}

		if (debug) {
			System.out.println("Returned cached algorithms for service <" + serviceName + ">: " + cachedAlgorithms.get(serviceName));
		}
		return cachedAlgorithms.get(serviceName);
	}

	/**
	 * Checks if the algorithm cache populated during a checkpoint is available. This cache is
	 * available when there is data in the cache and we have completed a CRIU restore. The cache
	 * may also become invalid when any security providers were inserted or removed from the provider list
	 * as dictated by the logic in the Security class.
	 * @return boolean value true when the cache is available, false otherwise.
	 */
	public static boolean isCachedAlgorithmsPresentAndReady() {
		return (cachedAlgorithms.size() > 0) && isCacheEligible;
	}

	/**
	 * Invalidates the CRIU algorithm cache.
	 */
	public static void invalidateAlgorithmCache() {
		if (debug) {
			System.out.println("Algorithm cache set to disabled.");
		}

		isCacheEligible = false;
 	}
}