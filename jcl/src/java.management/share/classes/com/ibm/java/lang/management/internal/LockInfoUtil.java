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

import java.lang.management.LockInfo;

import javax.management.openmbean.CompositeData;
import javax.management.openmbean.CompositeDataSupport;
import javax.management.openmbean.CompositeType;
import javax.management.openmbean.OpenDataException;
import javax.management.openmbean.OpenType;
import javax.management.openmbean.SimpleType;

/**
 * Support for the {@link LockInfo} class.
 */
public final class LockInfoUtil {

	private static CompositeType compositeType;

	/**
	 * Returns an array of {@link LockInfo} whose elements have been created from
	 * the corresponding elements of the <code>lockInfosCDArray</code> argument.
	 *
	 * @param lockInfos
	 *     an array of {@link CompositeData} objects, each one
	 *     representing a {@link LockInfo}
	 * @return an array of {@link LockInfo} objects built using the data
	 *     discovered in the corresponding elements of <code>lockInfosCDArray</code>
	 * @throws IllegalArgumentException
	 *     if any of the elements of <code>lockInfosCDArray</code> do not correspond
	 *     to a {@link LockInfo} with the following attributes:
	 *     <ul>
	 *     <li><code>className</code>(<code>java.lang.String</code>)</li>
	 *     <li><code>identityHashCode</code>(<code>java.lang.Integer</code>)</li>
	 *     </ul>
	 */
	public static LockInfo[] fromArray(CompositeData[] lockInfos) {
		LockInfo[] result = null;

		if (lockInfos != null) {
			int length = lockInfos.length;

			result = new LockInfo[length];

			for (int i = 0; i < length; ++i) {
				result[i] = LockInfo.from(lockInfos[i]);
			}
		}

		return result;
	}

	/**
	 * @return an instance of {@link CompositeType} for the {@link LockInfo} class
	 */
	public static CompositeType getCompositeType() {
		if (compositeType == null) {
			try {
				String[] names = { "className", "identityHashCode" }; //$NON-NLS-1$ //$NON-NLS-2$
				String[] descs = { "className", "identityHashCode" }; //$NON-NLS-1$ //$NON-NLS-2$
				OpenType<?>[] types = { SimpleType.STRING, SimpleType.INTEGER };

				compositeType = new CompositeType(
						LockInfo.class.getName(),
						LockInfo.class.getName(),
						names,
						descs,
						types);
			} catch (OpenDataException e) {
				if (ManagementUtils.VERBOSE_MODE) {
					e.printStackTrace(System.err);
				}
			}
		}

		return compositeType;
	}

	/**
	 * @param info a {@link LockInfo} object
	 * @return a new {@link CompositeData} instance that represents the supplied <code>info</code> object
	 */
	public static CompositeData toCompositeData(LockInfo info) {
		CompositeData result = null;

		if (info != null) {
			CompositeType type = getCompositeType();
			String[] names = { "className", "identityHashCode" }; //$NON-NLS-1$ //$NON-NLS-2$
			Object[] values = { info.getClassName(), Integer.valueOf(info.getIdentityHashCode()) };

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

	private LockInfoUtil() {
		super();
	}

}
