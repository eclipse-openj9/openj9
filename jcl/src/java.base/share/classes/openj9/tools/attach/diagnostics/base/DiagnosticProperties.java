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
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.Comparator;
import java.util.Properties;
import com.ibm.tools.attach.target.IPC;

/**
 * Augments Properties with convenience methods to add ints, booleans, and
 * longs.
 *
 */
public class DiagnosticProperties {
	private final Properties baseProperties;

	/**
	 * Master prefix for property keys
	 */
	public static final String OPENJ9_DIAGNOSTICS_PREFIX = "openj9_diagnostics."; //$NON-NLS-1$
	/**
	 * Use for commands which return a single string
	 */
	public static final String DIAGNOSTICS_STRING_RESULT = OPENJ9_DIAGNOSTICS_PREFIX + "string_result"; //$NON-NLS-1$

	private static final String JAVA_LANG_STRING = "java.lang.String"; //$NON-NLS-1$

	/**
	 * Set this to "true" to enable verbose mode
	 */
	public static final String DEBUG_PROPERTY = "openj9.tools.attach.diagnostics.debug"; //$NON-NLS-1$

	/**
	 * For development use only
	 */
	public static boolean isDebug;

	static {
		AccessController.doPrivileged((PrivilegedAction<Void>) () -> {
			isDebug = Boolean.getBoolean(DEBUG_PROPERTY);
			return null;
		});
	}
	
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
	 * @return property value as a String
	 * @throws IOException if the property is missing
	 */
	public String getString(String key) throws IOException {
		checkExists(key);
		return baseProperties.getProperty(key);
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
		if (isDebug) {
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
		StringWriter buff = new StringWriter(1000);
		try (PrintWriter buffWriter = new PrintWriter(buff)) {
			props.entrySet().stream().sorted(Comparator.comparing(e -> e.getKey().toString())).forEach(theEntry -> {
				buffWriter.print(theEntry.getKey());
				buffWriter.print("="); //$NON-NLS-1$
				buffWriter.println((String) theEntry.getValue());
			});
		}
		return buff.toString();
	}
	
	/**
	 * Print the result string of a command that produces a single string. Print an
	 * error message if the command resulted in an error.
	 * 
	 * @return String result or error message
	 */
	public String printStringResult() {
		StringWriter buff = new StringWriter(1000);
		try (PrintWriter buffWriter = new PrintWriter(buff)) {
			if (Boolean.parseBoolean(getPropertyOrNull(IPC.PROPERTY_DIAGNOSTICS_ERROR))) {
				String errorType = getPropertyOrNull(IPC.PROPERTY_DIAGNOSTICS_ERRORTYPE);
				if (null == errorType) {
					errorType = "No error type available"; //$NON-NLS-1$
				}
				buffWriter.printf("Error: %s%n", errorType); //$NON-NLS-1$
				String msg = getPropertyOrNull(IPC.PROPERTY_DIAGNOSTICS_ERRORMSG);
				if (null != msg) {
					buffWriter.println(msg);
				}
			} else {
				buffWriter.println(getString(DIAGNOSTICS_STRING_RESULT));
			}
		} catch (IOException e) {
			buff = new StringWriter();
			buff.append("Error parsing properties: "); //$NON-NLS-1$
			buff.append(e.toString());
			buff.append(System.lineSeparator());
		}
		return buff.toString();
	}

	/**
	 * Create a properties file to hold a single string.
	 * 
	 * @param text text of the string
	 * @return DiagnosticProperties object
	 */
	public static DiagnosticProperties makeStringResult(String text) {
		DiagnosticProperties props = makeCommandSucceeded();
		props.put(DIAGNOSTICS_STRING_RESULT, text);
		return props;
	}

	/**
	 * Return the underlying properties object by reference.
	 * 
	 * @return Properties object
	 */
	public Properties toProperties() {
		return baseProperties;
	}

	/**
	 * Report successful execution of a command
	 * @return DiagnosticProperties object
	 */
	public static DiagnosticProperties makeCommandSucceeded() {
		DiagnosticProperties props = makeStatusProperties(false, null);
		props.put(DIAGNOSTICS_STRING_RESULT, "Command succeeded"); //$NON-NLS-1$
		return props; // $NON-NLS-1$
	}

	/**
	 * Report an error in the command
	 * @param message error message
	 * @return DiagnosticProperties object
	 */
	public static DiagnosticProperties makeErrorProperties(String message) {
		return makeStatusProperties(true, message); // $NON-NLS-1$
	}

	/**
	 * Encode information about an exception into properties.
	 * 
	 * @param e Exception object
	 * @return Properties object
	 */
	public static Properties makeExceptionProperties(Exception e) {
		Properties props = new Properties();
		props.put(IPC.PROPERTY_DIAGNOSTICS_ERROR, Boolean.toString(true));
		props.put(IPC.PROPERTY_DIAGNOSTICS_ERRORTYPE, e.getClass().getName());
		String msg = e.getMessage();
		if (null == msg) {
			msg = String.format("Error at %s", e.getStackTrace()[0].toString()); //$NON-NLS-1$
		}
		props.put(IPC.PROPERTY_DIAGNOSTICS_ERRORMSG, msg);
		return props;
	}

	/**
	 * Report the status of a command execution.
	 * 
	 * @param error true if the command failed
	 * @param msg   status message
	 * @return DiagnosticProperties object
	 */
	public static DiagnosticProperties makeStatusProperties(boolean error, String msg) {
		DiagnosticProperties props = new DiagnosticProperties();
		props.put(IPC.PROPERTY_DIAGNOSTICS_ERROR, Boolean.toString(error));
		props.put(IPC.PROPERTY_DIAGNOSTICS_ERRORTYPE, "Error in command"); //$NON-NLS-1$
		if (null != msg) {
			props.put(IPC.PROPERTY_DIAGNOSTICS_ERRORMSG, msg);
		}
		return props;
	}

}
