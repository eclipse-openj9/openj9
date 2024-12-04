/*[INCLUDE-IF CRIU_SUPPORT]*/
/*
 * Copyright IBM Corp. and others 2022
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */

package openj9.internal.criu.security;

import java.lang.reflect.Method;
import java.security.Provider;
import java.security.Provider.Service;
import java.security.Security;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Properties;
import java.util.Set;
import java.util.TreeMap;

import openj9.internal.criu.CRIUSECProvider;
import sun.security.jca.ProviderList;
import sun.security.jca.Providers;

/**
 * Configures the security providers when in CRIU mode.
 */
public final class CRIUConfigurator {
	private static Properties systemProps;
	/** Stores the old security providers (position, name). */
	private static final HashMap<String, String> oldProviders = new HashMap<>();
	/** Tracing for CRIUSEC. */
	private static final boolean debug = Boolean.parseBoolean(
			/*[IF JAVA_SPEC_VERSION < 24]*/
			sun.security.action.GetPropertyAction.privilegedGetProperty
			/*[ELSE] JAVA_SPEC_VERSION < 24
			System.getProperty
			/*[ENDIF] JAVA_SPEC_VERSION < 24 */
					("enable.j9internal.checkpoint.security.api.debug", "false"));
	private static final Map<String, Set<String>> cachedAlgorithms = new TreeMap<>(String.CASE_INSENSITIVE_ORDER);
	private static boolean isCacheEligible;

	/**
	 * Removes the usual security providers and adds the CRIU security provider.
	 *
	 * @param props the system properties
	 */
	public static void setCRIUSecMode(Properties props) {
		systemProps = props;

		// Get all configured security providers and save all unique services and their associated algorithms
		// into a cache of type Map. When cache has been populated remove all the providers we loaded.
		Provider[] providerList = Security.getProviders();
		for (Provider provider: providerList) {
			if (debug) {
				System.out.format("Name: %s%nInformation:%n%s%n", provider.getName(), provider.getInfo());
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
			for (Service service : serviceList) {
				String serviceName = service.getType();
				if (cachedAlgorithms.get(serviceName) == null) {
					Set<String> algs = Security.getAlgorithms(serviceName);
					cachedAlgorithms.put(serviceName, algs);
					if (debug) {
						System.out.format("Cached algorithms with service name <%s>:%n%s%n", serviceName, algs);
					}
				}
			}
		}

		// Remove all providers.
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
		Security.insertProviderAt(new CRIUSECProvider(), 1);
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
			if (!cachedAlgorithms.isEmpty()) {
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
	 * @param serviceName the name of the Java cryptographic
	 * service (e.g., Signature, MessageDigest, Cipher, Mac, KeyStore).
	 * Note: this parameter is case-insensitive.
	 *
	 * @return a Set of Strings containing the names of all available
	 * algorithms or types for the specified Java cryptographic service
	 * or an empty set if no provider supports the specified service.
	 */
	public static Set<String> getAlgorithms(String serviceName) {
		if ((serviceName == null) || serviceName.isEmpty()) {
			if (debug) {
				System.out.println("Return empty collection since service name is empty or null.");
			}
			return Collections.emptySet();
		}

		Set<String> result = cachedAlgorithms.get(serviceName);
		if (result == null) {
			if (debug) {
				System.out.format("Return empty collection for service <%s> since it was not found in the cache.", serviceName);
			}
			return Collections.emptySet();
		}

		if (debug) {
			System.out.format("Return cached algorithms for service <%s>:%n%s%n", serviceName, result);
		}
		return Collections.unmodifiableSet(result);
	}

	/**
	 * Checks if the algorithm cache populated during a checkpoint is available. This cache is
	 * available when there is data in the cache and we have completed a CRIU restore. The cache
	 * may also become invalid when any security providers were inserted or removed from the provider list
	 * as dictated by the logic in the Security class.
	 * @return boolean value true when the cache is available, false otherwise.
	 */
	public static boolean isCachedAlgorithmsPresentAndReady() {
		return isCacheEligible && !cachedAlgorithms.isEmpty();
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
