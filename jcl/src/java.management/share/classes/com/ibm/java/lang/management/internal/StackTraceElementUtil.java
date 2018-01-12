/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2016, 2017 IBM Corp. and others
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
package com.ibm.java.lang.management.internal;

import java.lang.ref.SoftReference;

import javax.management.openmbean.CompositeData;
import javax.management.openmbean.CompositeDataSupport;
import javax.management.openmbean.CompositeType;
import javax.management.openmbean.OpenDataException;
import javax.management.openmbean.OpenType;
import javax.management.openmbean.SimpleType;

/**
 * Support for the {@link StackTraceElement} class. 
 */
public final class StackTraceElementUtil {

	private static CompositeType compositeType;

	/**
	 * Convenience method that returns a {@link StackTraceElement} created from
	 * the corresponding <code>CompositeData</code> argument.
	 *
	 * @param stackTraceCD
	 *            a <code>CompositeData</code> that wraps a
	 *            <code>StackTraceElement</code>
	 * @return a <code>StackTraceElement</code> object built using the data
	 *         discovered in the <code>stackTraceCD</code>.
	 * @throws IllegalArgumentException
	 *             if the <code>stackTraceCD</code> does not correspond to a
	 *             <code>StackTraceElement</code> with the following
	 *             attributes:
	 *             <ul>
/*[IF Sidecar19-SE]
	 *             <li><code>moduleName</code>(<code>java.lang.String</code>)
	 *             <li><code>moduleVersion</code>(<code>java.lang.String</code>)
	 *             <li><code>classLoaderName</code>(<code>java.lang.String</code>)
/*[ENDIF]
	 *             <li><code>className</code>(<code>java.lang.String</code>)
	 *             <li><code>methodName</code>(
	 *             <code>java.lang.String</code>)
	 *             <li><code>fileName</code>(<code>java.lang.String</code>)
	 *             <li><code>lineNumber</code> (<code>java.lang.Integer</code>)
	 *             <li><code>nativeMethod</code> (<code>java.lang.Boolean</code>)
	 *             </ul>
	 */
	public static StackTraceElement from(CompositeData stackTraceCD) {
		return fromArray(new CompositeData[] { stackTraceCD })[0];
	}

	private static SoftReference<String[]> methodNameCache = null;
	private static SoftReference<String[]> returnTypeCache = null;
	/* 
	 * the following three methods, fromArray, toCompositeData, and getCompositeType 
	 * must respect the ordering for the methods 
	 */
	private static String[] getMethodNames() {
		String[] methodNames = null;
		if (null != methodNameCache) {
			methodNames = methodNameCache.get();
		}
		if (null == methodNames) {
			methodNames =new String[] {"className",  //$NON-NLS-1$ 
					"methodName", //$NON-NLS-1$ 
					"fileName",  //$NON-NLS-1$ 
					"lineNumber",  //$NON-NLS-1$ 
					"nativeMethod", //$NON-NLS-1$ 
					/*[IF Sidecar19-SE]*/
					"moduleName",  //$NON-NLS-1$ 
					"moduleVersion",  //$NON-NLS-1$ 
					"classLoaderName" //$NON-NLS-1$ 
					/*[ENDIF]*/
			}; 
			methodNameCache = new SoftReference<>(methodNames);
		}
		return methodNames;
	}

	private static String[] getMethodReturnTypeNames() {
		String[] returnTypes = null;
		if (null != returnTypeCache) {
			returnTypes = returnTypeCache.get();
		}
		if (null == returnTypes) { 
			returnTypes = new String[] {
				"java.lang.String", //$NON-NLS-1$
				"java.lang.String", //$NON-NLS-1$
				"java.lang.String", //$NON-NLS-1$
				"java.lang.Integer", //$NON-NLS-1$
				"java.lang.Boolean", //$NON-NLS-1$
				/*[IF Sidecar19-SE]*/
				"java.lang.String", //$NON-NLS-1$
				"java.lang.String", //$NON-NLS-1$
				"java.lang.String" //$NON-NLS-1$
				/*[ENDIF]*/
				};
		}
		returnTypeCache = new SoftReference<>(returnTypes);
		return returnTypes;
	}

	private static Object[] getValues(StackTraceElement element) {
		Object[] values = {
				element.getClassName(), 
				element.getMethodName(),
				element.getFileName(), 
				Integer.valueOf(element.getLineNumber()),
				Boolean.valueOf(element.isNativeMethod()),
				/*[IF Sidecar19-SE]*/
				element.getModuleName(), 
				element.getModuleVersion(), 
				element.getClassLoaderName()
				/*[ENDIF]*/
				};
		return values;
	}

	/**
	 * Returns an array of {@link StackTraceElement} whose elements have been
	 * created from the corresponding elements of the
	 * <code>stackTraceDataVal</code> argument.
	 *
	 * @param stackTraceDataVal
	 *            an array of {@link CompositeData} objects, each one
	 *            representing a <code>StackTraceElement</code>.
	 * @return an array of <code>StackTraceElement</code> objects built using
	 *         the data discovered in the corresponding elements of
	 *         <code>stackTraceDataVal</code>.
	 * @throws IllegalArgumentException
	 *             if any of the elements of <code>stackTraceDataVal</code> do
	 *             not correspond to a <code>StackTraceElement</code> with the
	 *             following attributes:
	 *             <ul>
/*[IF Sidecar19-SE]
	 *             <li><code>moduleName</code>(<code>java.lang.String</code>)
	 *             <li><code>moduleVersion</code>(<code>java.lang.String</code>)
	 *             <li><code>classLoaderName</code>(<code>java.lang.String</code>)
/*[ENDIF]
	 *             <li><code>className</code>(<code>java.lang.String</code>)
	 *             <li><code>methodName</code>(
	 *             <code>java.lang.String</code>)
	 *             <li><code>fileName</code>(<code>java.lang.String</code>)
	 *             <li><code>lineNumber</code> (<code>java.lang.Integer</code>)
	 *             <li><code>nativeMethod</code> (<code>java.lang.Boolean</code>)
	 *             </ul>
	 */
	public static StackTraceElement[] fromArray(CompositeData[] stackTraceDataVal) {
		// Bail out early on null input.
		if (stackTraceDataVal == null) {
			return null;
		}

		String[] attributeNames = getMethodNames();
		String[] attributeTypes = getMethodReturnTypeNames();

		int length = stackTraceDataVal.length;
		StackTraceElement[] result = new StackTraceElement[length];

		for (int i = 0; i < length; ++i) {
			CompositeData data = stackTraceDataVal[i];

			if (data == null) {
				result[i] = null;
			} else {
				// verify the element
				ManagementUtils.verifyFieldNumber(data, attributeNames.length);
				ManagementUtils.verifyFieldNames(data, attributeNames);
				ManagementUtils.verifyFieldTypes(data, attributeNames, attributeTypes);

				// Get hold of the values from the data object to use in the
				// creation of a new StackTraceElement.
				Object[] attributeVals = data.getAll(attributeNames);

				String classNameVal = (String) attributeVals[0];
				String methodNameVal = (String) attributeVals[1];
				String fileNameVal = (String) attributeVals[2];
				int lineNumberVal = ((Integer) attributeVals[3]).intValue();
				@SuppressWarnings("unused")
				boolean nativeMethodVal = ((Boolean) attributeVals[4]).booleanValue();
				/*[IF Sidecar19-SE]*/
				String moduleNameVal = (String) attributeVals[5];
				String moduleVersionVal = (String) attributeVals[6];
				String classLoaderNameVal = (String) attributeVals[7];
				/*[ENDIF]*/
				StackTraceElement element = new StackTraceElement(
						/*[IF Sidecar19-SE]*/
						moduleNameVal, 
						moduleVersionVal,
						classLoaderNameVal,
						/*[ENDIF]*/
						classNameVal, methodNameVal, fileNameVal, lineNumberVal
						);
				result[i] = element;
			}
		}

		return result;
	}

	/**
	 * @return an instance of {@link CompositeType} for the {@link StackTraceElement} class
	 */
	public static CompositeType getCompositeType() {
		if (compositeType == null) {
			String[] names = getMethodNames();
			String[] descs = getMethodNames();
			OpenType<?>[] types = {
					SimpleType.STRING, SimpleType.STRING, SimpleType.STRING,
					SimpleType.INTEGER, SimpleType.BOOLEAN ,
					/*[IF Sidecar19-SE]*/
					SimpleType.STRING, SimpleType.STRING,SimpleType.STRING,
					/*[ENDIF]*/
					};
		
			try {
				compositeType = new CompositeType(
						StackTraceElement.class.getName(),
						StackTraceElement.class.getName(),
						names, descs, types);
			} catch (OpenDataException e) {
				if (ManagementUtils.VERBOSE_MODE) {
					e.printStackTrace(System.err);
				}
			}
		}

		return compositeType;
	}

	/**
	 * @param element a {@link StackTraceElement} object
	 * @return a {@link CompositeData} object that represents the supplied <code>element</code> object
	 */
	public static CompositeData toCompositeData(StackTraceElement element) {
		CompositeData result = null;

		if (element != null) {
			CompositeType type = StackTraceElementUtil.getCompositeType();
			String[] names = getMethodNames();
			Object[] values = getValues(element);

			try {
				result = new CompositeDataSupport(type, names, values);
			} catch (OpenDataException e) {
				if (ManagementUtils.VERBOSE_MODE) {
					e.printStackTrace(System.err);
				}
			}
		}

		return result;
	}

	private StackTraceElementUtil() {
		super();
	}

}
