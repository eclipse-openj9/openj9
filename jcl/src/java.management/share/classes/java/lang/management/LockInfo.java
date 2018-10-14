/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2008, 2018 IBM Corp. and others
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
package java.lang.management;

import javax.management.openmbean.CompositeData;

import com.ibm.java.lang.management.internal.ManagementUtils;

/**
 * This class represents information about locked objects.
 *
 * @since 1.6
 */
public class LockInfo {

	private final String className;

	private final int identityHashCode;

	/*[IF]*/
	@SuppressWarnings("unused") /* Used only by native code. */
	/*[ENDIF]*/
	private LockInfo(Object object) {
		this(object.getClass().getName(), System.identityHashCode(object));
	}

	/**
	 * Creates a new <code>LockInfo</code> instance.
	 *
	 * @param className
	 *            the name (including the package prefix) of the associated lock
	 *            object's class
	 * @param identityHashCode
	 *            the value of the associated lock object's identity hash code.
	 *            This amounts to the result of calling
	 *            {@link System#identityHashCode(Object)} with the lock object
	 *            as the sole argument.
	 * @throws NullPointerException
	 *             if <code>className</code> is <code>null</code>
	 */
	public LockInfo(String className, int identityHashCode) {
		super();
		if (className == null) {
			/*[MSG "K0600", "className cannot be null"]*/
			throw new NullPointerException(com.ibm.oti.util.Msg.getString("K0600")); //$NON-NLS-1$
		}
		this.className = className;
		this.identityHashCode = identityHashCode;
	}

	/**
	 * Returns the name of the lock object's class in fully qualified form (i.e.
	 * including the package prefix).
	 *
	 * @return the associated lock object's class name
	 */
	public String getClassName() {
		return className;
	}

	/**
	 * Returns the value of the associated lock object's identity hash code
	 *
	 * @return the identity hash code of the lock object
	 */
	public int getIdentityHashCode() {
		return identityHashCode;
	}

	/**
	 * Returns a {@code LockInfo} object represented by the given
	 * {@code CompositeData}. The given {@code CompositeData} must contain the
	 * following attributes: <blockquote>
	 * <table>
	 * <caption>The attributes and the types the given CompositeData contains</caption>
	 * <tr>
	 * <th style="float:left">Attribute Name</th>
	 * <th style="float:left">Type</th>
	 * </tr>
	 * <tr>
	 * <td>className</td>
	 * <td><code>java.lang.String</code></td>
	 * </tr>
	 * <tr>
	 * <td>identityHashCode</td>
	 * <td><code>java.lang.Integer</code></td>
	 * </tr>
	 * </table>
	 * </blockquote>
	 *
	 * @param compositeData
	 *            {@code CompositeData} representing a {@code LockInfo}
	 * @throws IllegalArgumentException
	 *             if {@code compositeData} does not represent a {@code LockInfo} with the
	 *             attributes described above.
	 * @return a {@code LockInfo} object represented by {@code compositeData} if {@code compositeData}
	 *         is not {@code null}; {@code null} otherwise.
	 *
	 * @since 1.8
	 */
	public static LockInfo from(CompositeData compositeData) {
		LockInfo element = null;

		if (compositeData != null) {
			// Verify the element
			ManagementUtils.verifyFieldNumber(compositeData, 2);
			String[] attributeNames = { "className", "identityHashCode" }; //$NON-NLS-1$ //$NON-NLS-2$
			ManagementUtils.verifyFieldNames(compositeData, attributeNames);
			String[] attributeTypes = { "java.lang.String", "java.lang.Integer" }; //$NON-NLS-1$ //$NON-NLS-2$
			ManagementUtils.verifyFieldTypes(compositeData, attributeNames, attributeTypes);

			// Get hold of the values from the data object to use in the
			// creation of a new LockInfo.
			Object[] attributeVals = compositeData.getAll(attributeNames);
			String className = (String) attributeVals[0];
			int idHashCode = ((Integer) attributeVals[1]).intValue();

			element = new LockInfo(className, idHashCode);
		}

		return element;
	}

	/**
	 * Provides callers with a string value that represents the associated lock.
	 * The string will hold both the name of the lock object's class and it's
	 * identity hash code expressed as an unsigned hexadecimal. i.e.<br>
	 * <p>
	 * {@link #getClassName()} &nbsp;+&nbsp;&#64;&nbsp;+&nbsp;Integer.toHexString({@link #getIdentityHashCode()})
	 * </p>
	 *
	 * @return a string containing the key details of the lock
	 */
	@Override
	public String toString() {
		StringBuilder sb = new StringBuilder();
		sb.append(className);
		sb.append('@');
		sb.append(Integer.toHexString(identityHashCode));
		return sb.toString();
	}

}
