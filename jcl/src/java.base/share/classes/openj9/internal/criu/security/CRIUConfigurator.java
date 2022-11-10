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

import sun.security.action.GetPropertyAction;
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
			GetPropertyAction.privilegedGetProperty("enable.j9internal.checkpoint.security.api.debug", "false"));

	/**
	 * Removes the usual security providers and adds the CRIU security provider.
	 *
	 * @param props the system properties
	 */
	public static void setCRIUSecMode(Properties props) {
		systemProps = props;

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

		if (debug) {
			System.out.println("CRIUSEC added and all other security providers removed.");
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
				System.out.println("CRIUSEC removed.");
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
		}
	}
}
