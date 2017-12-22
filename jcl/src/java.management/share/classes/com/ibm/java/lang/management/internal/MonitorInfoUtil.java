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

import java.lang.management.MonitorInfo;

import javax.management.openmbean.CompositeData;
import javax.management.openmbean.CompositeDataSupport;
import javax.management.openmbean.CompositeType;
import javax.management.openmbean.OpenDataException;
import javax.management.openmbean.OpenType;
import javax.management.openmbean.SimpleType;

/**
 * Support for the {@link MonitorInfo} class.
 */
public final class MonitorInfoUtil {

	private static CompositeType compositeType;

	/**
	 * Returns an array of {@link MonitorInfo} whose elements have been created
	 * from the corresponding elements of the <code>monitorInfos</code> argument.
	 *
	 * @param monitorInfos
	 *            an array of {@link CompositeData} objects, each one
	 *            representing a <code>MonitorInfo</code>
	 * @return an array of <code>MonitorInfo</code> objects built using the
	 *         data discovered in the corresponding elements of
	 *         <code>monitorInfos</code>
	 * @throws IllegalArgumentException
	 *             if any of the elements of <code>monitorInfos</code>
	 *             do not correspond to a <code>MonitorInfo</code> with the
	 *             following attributes:
	 *             <ul>
	 *             <li><code>lockedStackFrame</code>(<code>javax.management.openmbean.CompositeData</code>)
	 *             <li><code>lockedStackDepth</code>(
	 *             <code>java.lang.Integer</code>)
	 *             </ul>
	 *             The <code>lockedStackFrame</code> attribute must correspond
	 *             to a <code>java.lang.StackTraceElement</code> which has the
	 *             following attributes:
	 *             <ul>
	/*[IF Sidecar19-SE]
	 *             <li><code>moduleName</code>(<code>java.lang.String</code>)
	 *             <li><code>moduleVersion</code>(<code>java.lang.String</code>)
	/*[ENDIF]
	 *             <li><code>className</code> (<code>java.lang.String</code>)
	 *             <li><code>methodName</code> (<code>java.lang.String</code>)
	 *             <li><code>fileName</code> (<code>java.lang.String</code>)
	 *             <li><code>lineNumber</code> (<code>java.lang.Integer</code>)
	 *             <li><code>nativeMethod</code> (<code>java.lang.Boolean</code>)
	 *             </ul>
	 */
	public static MonitorInfo[] fromArray(CompositeData[] monitorInfos) {
		MonitorInfo[] result = null;

		if (monitorInfos != null) {
			String[] attributeNames = { "className", "identityHashCode", //$NON-NLS-1$ //$NON-NLS-2$
					"lockedStackFrame", "lockedStackDepth" }; //$NON-NLS-1$ //$NON-NLS-2$
			String[] attributeTypes = { "java.lang.String", "java.lang.Integer", //$NON-NLS-1$ //$NON-NLS-2$
					CompositeData.class.getName(), "java.lang.Integer" }; //$NON-NLS-1$
			int length = monitorInfos.length;

			result = new MonitorInfo[length];

			for (int i = 0; i < length; ++i) {
				CompositeData data = monitorInfos[i];

				// verify the element
				ManagementUtils.verifyFieldNumber(data, 4);
				ManagementUtils.verifyFieldNames(data, attributeNames);
				ManagementUtils.verifyFieldTypes(data, attributeNames, attributeTypes);

				result[i] = MonitorInfo.from(data);
			}
		}

		return result;
	}

	/**
	 * @return an instance of {@link CompositeType} for the {@link MonitorInfo} class
	 */
	public static CompositeType getCompositeType() {
		if (compositeType == null) {
			try {
				String[] names = { "className", "identityHashCode", //$NON-NLS-1$ //$NON-NLS-2$
						"lockedStackFrame", "lockedStackDepth" }; //$NON-NLS-1$ //$NON-NLS-2$
				String[] descs = { "className", "identityHashCode", //$NON-NLS-1$ //$NON-NLS-2$
						"lockedStackFrame", "lockedStackDepth" }; //$NON-NLS-1$ //$NON-NLS-2$
				OpenType<?>[] types = { SimpleType.STRING, SimpleType.INTEGER,
						StackTraceElementUtil.getCompositeType(),
						SimpleType.INTEGER };
				compositeType = new CompositeType(
						MonitorInfo.class.getName(),
						MonitorInfo.class.getName(),
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
	 * @param info a <code>MonitorInfo</code> object
	 * @return a new <code>CompositeData</code> instance that represents the supplied <code>info</code> object
	 */
	public static CompositeData toCompositeData(MonitorInfo info) {
		CompositeData result = null;

		if (info != null) {
			CompositeType type = getCompositeType();
			String[] names = { "className", "identityHashCode", //$NON-NLS-1$ //$NON-NLS-2$
					"lockedStackFrame", "lockedStackDepth" }; //$NON-NLS-1$ //$NON-NLS-2$
			Object[] values = {
					info.getClassName(),
					Integer.valueOf(info.getIdentityHashCode()),
					StackTraceElementUtil.toCompositeData(info.getLockedStackFrame()),
					Integer.valueOf(info.getLockedStackDepth()) };

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

	private MonitorInfoUtil() {
		super();
	}

}
