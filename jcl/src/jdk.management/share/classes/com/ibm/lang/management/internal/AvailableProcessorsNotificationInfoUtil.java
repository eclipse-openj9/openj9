/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2016, 2019 IBM Corp. and others
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
package com.ibm.lang.management.internal;

import javax.management.openmbean.CompositeData;
import javax.management.openmbean.CompositeDataSupport;
import javax.management.openmbean.CompositeType;
import javax.management.openmbean.OpenDataException;
import javax.management.openmbean.OpenType;
import javax.management.openmbean.SimpleType;

import com.ibm.java.lang.management.internal.ManagementUtils;
import com.ibm.lang.management.AvailableProcessorsNotificationInfo;

/**
 * Support for the {@link AvailableProcessorsNotificationInfo} class.
 */
public final class AvailableProcessorsNotificationInfoUtil {

	private static CompositeType compositeType;

	/**
	 * @return an instance of {@link CompositeType} for the {@link AvailableProcessorsNotificationInfo} class
	 */
	private static CompositeType getCompositeType() {
		if (compositeType == null) {
			String[] names = { "newAvailableProcessors" }; //$NON-NLS-1$
			String[] descs = { "newAvailableProcessors" }; //$NON-NLS-1$
			OpenType<?>[] types = { SimpleType.INTEGER };

			try {
				compositeType = new CompositeType(
						AvailableProcessorsNotificationInfo.class.getName(),
						AvailableProcessorsNotificationInfo.class.getName(),
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
	 * @param info a {@link AvailableProcessorsNotificationInfo} object
	 * @return a {@link CompositeData} object that represents the supplied <code>info</code> object
	 */
	public static CompositeData toCompositeData(AvailableProcessorsNotificationInfo info) {
		CompositeData result = null;

		if (info != null) {
			CompositeType type = getCompositeType();
			String[] names = { "newAvailableProcessors" }; //$NON-NLS-1$
			Object[] values = { Integer.valueOf(info.getNewAvailableProcessors()) };

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

	private AvailableProcessorsNotificationInfoUtil() {
		super();
	}

}
