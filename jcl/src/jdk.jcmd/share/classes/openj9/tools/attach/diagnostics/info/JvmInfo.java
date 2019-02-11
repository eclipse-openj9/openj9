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

package openj9.tools.attach.diagnostics.info;

import java.io.IOException;
import java.util.Set;

import javax.management.openmbean.ArrayType;
import javax.management.openmbean.CompositeData;
import javax.management.openmbean.CompositeDataSupport;
import javax.management.openmbean.CompositeType;
import javax.management.openmbean.OpenDataException;
import javax.management.openmbean.OpenType;
import javax.management.openmbean.SimpleType;

import com.ibm.java.lang.management.internal.LockInfoUtil;
import com.ibm.java.lang.management.internal.MonitorInfoUtil;
import com.ibm.java.lang.management.internal.StackTraceElementUtil;

import openj9.tools.attach.diagnostics.base.DiagnosticProperties;

/**
 * Methods for converting between properties and CompositeTypes
 *
 */
public class JvmInfo {
	protected static final String ARRAY_SIZE = "array_size"; //$NON-NLS-1$

	protected static void addFields(DiagnosticProperties props, String keyPrefix, CompositeType compType,
			CompositeData compData) {
		for (String k : compType.keySet()) {
			String keyString = keyPrefix + k;
			Object value = compData.get(k);
			OpenType<?> valueType = compType.getType(k);

			boolean valuePresent = (null != value);
			if (JvmInfo.isSimpleType(valueType)) {
				if (valuePresent) {
					props.put(keyString, value.toString());
				}
			} else {
				props.put(keyString, valuePresent);
				if (valuePresent) {
					if (valueType.isArray()) {
						ArrayType<?> valueArrayType = (ArrayType<?>) valueType;
						CompositeType elementType = getCompositeType(valueArrayType.getElementOpenType());
						CompositeData[] valueArray = (CompositeData[]) value;
						int arrayLength = valueArray.length;
						props.put(keyString + ARRAY_SIZE, arrayLength);
						for (int i = 0; i < arrayLength; ++i) {
							String newPrefix = keyString + '.' + Integer.toString(i) + '.';
							addFields(props, newPrefix, elementType, valueArray[i]);
						}
					} else {
						CompositeType newCompType = getCompositeType(valueType);
						String newPrefix = keyString + '.';
						CompositeData newCompData = (CompositeData) value;
						addFields(props, newPrefix, newCompType, newCompData);
					}
				}
			}
		}
	}

	/**
	 * Get the CompositeType object based on a name
	 * 
	 * @param typeIdentifier Name of the type
	 * @return CompositeType for that type; null if the type is unrecognized.
	 */
	protected static CompositeType getCompositeType(OpenType<?> typeIdentifier) {
		CompositeType result = null;

		String typeName = typeIdentifier.getTypeName();
		switch (typeName) {
		case "java.lang.management.LockInfo": //$NON-NLS-1$
			result = LockInfoUtil.getCompositeType();
			break;
		case "java.lang.management.MonitorInfo": //$NON-NLS-1$
			result = MonitorInfoUtil.getCompositeType();
			break;
		case "java.lang.StackTraceElement": //$NON-NLS-1$
			result = StackTraceElementUtil.getCompositeType();
			break;
		default:
			break;
		}
		return result;
	}

	/**
	 * Convert a Properties into a CompositeData object
	 * 
	 * @param props          Source of the data
	 * @param propertyPrefix Used to filter the data
	 * @param compType       type description
	 * @return Filled-in CompositeData object
	 * @throws IOException if the properties are invalid
	 */
	protected static CompositeData extractFields(DiagnosticProperties props, String propertyPrefix,
			CompositeType compType) throws IOException {
		Set<String> keys = compType.keySet();
		int numKeys = keys.size();
		String[] itemNames = keys.toArray(new String[numKeys]);
		Object[] itemValues = new Object[numKeys];
		try {
			for (int cursor = 0; cursor < numKeys; ++cursor) {
				String k = itemNames[cursor];
				String keyString = propertyPrefix + k;
				Object value = null;
				OpenType<?> valueType = compType.getType(k);
				if (JvmInfo.isSimpleType(valueType)) {
					value = props.getSimple(valueType.getTypeName(), keyString);
				} else {
					boolean valuePresent = props.getBoolean(keyString);
					if (valuePresent) {
						if (valueType.isArray()) {
							int arrayLength = props.getInt(keyString + ARRAY_SIZE);
							ArrayType<?> valueArrayType = (ArrayType<?>) valueType;
							CompositeType elementType = getCompositeType(valueArrayType.getElementOpenType());
							CompositeData[] valueArray = new CompositeData[arrayLength];
							for (int i = 0; i < arrayLength; ++i) {
								String newPrefix = keyString + '.' + Integer.toString(i) + '.';
								CompositeData compData = extractFields(props, newPrefix, elementType);
								valueArray[i] = compData;
							}
							value = valueArray;
						} else { /* composed type */
							CompositeType newCompType = getCompositeType(valueType);
							String newPrefix = keyString + '.';
							CompositeData compData = extractFields(props, newPrefix, newCompType);
							value = compData;
						}
					}
				}
				itemValues[cursor] = value;
			}
			CompositeDataSupport result = new CompositeDataSupport(compType, itemNames, itemValues);
			return result;
		} catch (NumberFormatException | OpenDataException e) {
			throw new IOException(e);
		}
	}

	static boolean isSimpleType(OpenType<?> valueType) {
		return SimpleType.class.isAssignableFrom(valueType.getClass());
	}
}
