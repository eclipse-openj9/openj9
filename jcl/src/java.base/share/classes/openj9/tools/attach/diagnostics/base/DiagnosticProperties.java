/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

package openj9.tools.attach.diagnostics.base;

import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.Comparator;
import java.util.Properties;

/**
 * Augments Properties with convenience methods to add ints, booleans, and
 * longs.
 *
 */
public class DiagnosticProperties {
	private final Properties baseProperties;

	private static final String JAVA_LANG_STRING = "java.lang.String"; //$NON-NLS-1$

	/**
	 * Set this to "true" to enable verbose mode
	 */
	public static final String DEBUG_PROPERTY = "openj9.tools.attach.diagnostics.debug"; //$NON-NLS-1$

	/**
	 * For development use only
	 */
	public static boolean isDebug = Boolean.getBoolean(DEBUG_PROPERTY);

	/**
	 * @param props Properties object received from the target.
	 */
	public DiagnosticProperties(Properties props) {
		baseProperties = props;
	}

	/**
	 * Create an empty list of properties.
	 */
	public DiagnosticProperties() {
		baseProperties = new Properties();
	}

	/**
	 * Add a property with an integer value.
	 * 
	 * @param key   property name
	 * @param value property value
	 */
	public void put(String key, int value) {
		baseProperties.setProperty(key, Integer.toString(value));
	}

	/**
	 * Add a property with a String value.
	 * 
	 * @param key   property name
	 * @param value property value
	 */
	public void put(String key, String value) {
		baseProperties.setProperty(key, value);
	}

	/**
	 * Add a property with an long value.
	 * 
	 * @param key   property name
	 * @param value property value
	 */
	public void put(String key, long value) {
		baseProperties.setProperty(key, Long.toString(value));
	}

	/**
	 * Add a property with a boolean value.
	 * 
	 * @param key   property name
	 * @param value property value
	 */
	public void put(String key, boolean value) {
		baseProperties.setProperty(key, Boolean.toString(value));
	}

	private void checkExists(String key) throws IOException {
		if (!containsField(key)) {
			throw new IOException("key " + key + " not found"); //$NON-NLS-1$ //$NON-NLS-2$
		}
	}

	/**
	 * Test if the given property is present.
	 * 
	 * @param key property name
	 * @return true if the property name is found
	 */
	public boolean containsField(String key) {
		return baseProperties.containsKey(key);
	}

	/**
	 * Retrieve the value of a property and convert it to an int.
	 * 
	 * @param key name of the property
	 * @return value of the property as an int
	 * @throws NumberFormatException if the value is not a string representing a
	 *                               number.
	 * @throws IOException           if the property is missing
	 */
	public int getInt(String key) throws NumberFormatException, IOException {
		checkExists(key);
		return Integer.parseInt(baseProperties.getProperty(key));
	}

	/**
	 * Retrieve the value of a property and convert it to an long.
	 * 
	 * @param key name of the property
	 * @return value of the property as a long
	 * @throws NumberFormatException if the value is not a string representing a
	 *                               number.
	 * @throws IOException           if the property is missing
	 */
	public long getLong(String key) throws NumberFormatException, IOException {
		checkExists(key);
		return Long.parseLong(baseProperties.getProperty(key));
	}

	/**
	 * Retrieve the value of a property and convert it to a boolean.
	 * 
	 * @param key name of the property
	 * @return value of the property as a boolean
	 * @throws IOException if the property is missing
	 */
	public boolean getBoolean(String key) throws IOException {
		checkExists(key);
		return Boolean.parseBoolean(baseProperties.getProperty(key));
	}

	/**
	 * Return a property value for the given key.
	 * 
	 * @param key property name
	 * @return property value or null if the property is not found
	 */
	public String getPropertyOrNull(String key) {
		return baseProperties.getProperty(key);
	}

	/**
	 * Retrieve a value of a simple type, i.e. boxed scalar type, from a specified
	 * property. Return null if the type is not recognized, the value is absent, or the
	 * value is the empty string (unless the type is String).
	 * 
	 * @param typeName Fully qualified name of the type
	 * @param key      Name of the property containing the value
	 * @return object containing the value or null on error.
	 * @throws NumberFormatException if the property is malformed
	 */
	public Object getSimple(String typeName, String key) throws NumberFormatException {
		Object value = null;
		String valueString = baseProperties.getProperty(key);
		if ((null != valueString) && 
				(JAVA_LANG_STRING.equals(typeName) || !valueString.isEmpty())) {
			switch (typeName) {
			case "java.lang.Boolean": //$NON-NLS-1$
				value = Boolean.valueOf(valueString);
				break;
			case "java.lang.Character": //$NON-NLS-1$
				value = Character.valueOf(valueString.charAt(0));
				break;
			case "java.lang.Byte": //$NON-NLS-1$
				value = Byte.valueOf(valueString);
				break;
			case "java.lang.Short": //$NON-NLS-1$
				value = Short.valueOf(valueString);
				break;
			case "java.lang.Integer": //$NON-NLS-1$
				value = Integer.valueOf(valueString);
				break;
			case "java.lang.Long": //$NON-NLS-1$
				value = Long.valueOf(valueString);
				break;
			case "java.lang.Float": //$NON-NLS-1$
				value = Float.valueOf(valueString);
				break;
			case "java.lang.Double": //$NON-NLS-1$
				value = Double.valueOf(valueString);
				break;
			case JAVA_LANG_STRING:
				value = valueString;
				break;
			default:
				break;
			}
		}
		return value;
	}

	/**
	 * Print a DiagnosticProperties object.
	 * 
	 * @param msg   Message to print before properties. May be null.
	 * @param props DiagnosticProperties object to dump
	 */
	public static void dumpPropertiesIfDebug(String msg, DiagnosticProperties props) {
		dumpPropertiesIfDebug(msg, props.baseProperties);
	}

	/**
	 * Print a list of properties in sorted order if debugging is enabled.
	 * 
	 * @param msg   Message to print before properties. May be null.
	 * @param props Properties object to dump
	 */
	public static void dumpPropertiesIfDebug(String msg, final Properties props) {
		if (DiagnosticProperties.isDebug) {
			if (null != msg) {
				System.err.println(msg);
			}
			if (null != props) {
				final String result = printProperties(props);
				System.err.print(result);
			}
		}
	}

	/**
	 * Print a Properties object in sorted key order.
	 * 
	 * @param props object to dump
	 * @return String representation of props
	 */
	public static String printProperties(final Properties props) {
		final StringWriter buff = new StringWriter(1000);
		try (PrintWriter buffWriter = new PrintWriter(buff)) {
			props.entrySet().stream().sorted(Comparator.comparing(e -> e.getKey().toString())).forEach(theEntry -> {
				buffWriter.print(theEntry.getKey());
				buffWriter.print("="); //$NON-NLS-1$
				buffWriter.println((String) theEntry.getValue());
			});
		}
		final String result = buff.toString();
		return result;
	}

	/**
	 * Return the underlying properties object by reference.
	 * 
	 * @return Properties object
	 */
	public Properties toProperties() {
		return baseProperties;
	}
}
