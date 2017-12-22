/*******************************************************************************
 * Copyright (c) 1999, 2017 IBM Corp. and others
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
package com.ibm.jpp.om;

import java.io.FileNotFoundException;
import java.util.HashMap;
import java.util.Map;

import com.ibm.jpp.xml.XMLException;

/**
 * Represents a meta-registry to manage all the ConfigurationRegistries associated
 * with configurations
 */
public class MetaRegistry {

	private static Map<String, ConfigurationRegistry> registries = new HashMap<>();

	/**
	 * Returns the registry associated with the given source root and XML file.  If the
	 * registry cannot be found a new one will be created and it will populated with the
	 * configurations from the given XML file.
	 *
	 * @param       baseDir     the registry's base directory
	 * @param       srcRoot     the registry's source root path
	 * @param       xml         the registry's parent XML
	 * @return      the configuration registry requested
	 *
	 * @throws      FileNotFoundException   if the xml file could not be found
	 * @throws      XMLException    if there are errors in parsing the XML file
	 */
	public static ConfigurationRegistry getRegistry(String baseDir, String srcRoot, String xml) throws FileNotFoundException, XMLException {
		ConfigurationRegistry registry = registries.get(srcRoot);

		if (registry == null) {
			registry = new ConfigurationRegistry(baseDir, srcRoot);
			registry.registerXMLSet(xml);

			// Add registry to meta-registry by srcRoot/name
			registries.put(srcRoot, registry);
		}

		return registry;
	}

	/**
	 * Returns all of the configuration registries available.
	 *
	 * @return      the configuration registries
	 */
	public static ConfigurationRegistry[] getRegistries() {
		int size = registries.size();
		return (size > 0) ? registries.values().toArray(new ConfigurationRegistry[size]) : null;
	}

	/**
	 * Clears the meta registry.
	 */
	public static void clear() {
		registries.clear();
	}
}
