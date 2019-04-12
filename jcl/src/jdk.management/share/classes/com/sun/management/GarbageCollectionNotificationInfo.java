/*[INCLUDE-IF Sidecar18-SE]*/
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
package com.sun.management;

import javax.management.openmbean.CompositeData;
import javax.management.openmbean.CompositeDataView;
import javax.management.openmbean.CompositeType;

import com.ibm.java.lang.management.internal.ManagementUtils;
import com.sun.management.internal.GarbageCollectionNotificationInfoUtil;

/**
 * Information about a garbage collection.
 *
 * <p>
 * A garbage collection notification is emitted by {@link GarbageCollectorMXBean}
 * when the Java virtual machine completes a garbage collection action.
 * The notification emitted will contain the garbage collection notification
 * information about the status of the memory:
 * <ul>
 *   <li>The name of the garbage collector used perform the collection.</li>
 *   <li>The action performed by the garbage collector.</li>
 *   <li>The cause of the garbage collection action.</li>
 *   <li>A {@link GcInfo} object containing some statistics about the GC cycle
 *        (start time, end time) and the memory usage before and after
 *        the GC cycle.</li>
 * </ul>
 *
 * <p>
 * A {@link CompositeData CompositeData} representing
 * the {@code GarbageCollectionNotificationInfo} object
 * is stored in the
 * {@linkplain javax.management.Notification#setUserData userdata}
 * of a {@linkplain javax.management.Notification notification}.
 * The {@link #from from} method is provided to convert from
 * a {@code CompositeData} to a {@code GarbageCollectionNotificationInfo}
 * object. For example:
 *
 * <blockquote><pre>
 *      Notification notif;
 *
 *      // receive the notification emitted by a GarbageCollectorMXBean and save in notif
 *      ...
 *
 *      String notifType = notif.getType();
 *      if (notifType.equals(GarbageCollectionNotificationInfo.GARBAGE_COLLECTION_NOTIFICATION)) {
 *          // retrieve the garbage collection notification information
 *          CompositeData cd = (CompositeData) notif.getUserData();
 *          GarbageCollectionNotificationInfo info = GarbageCollectionNotificationInfo.from(cd);
 *          ...
 *      }
 * </pre></blockquote>
 *
 * <p>
 * The type of the notification emitted by a {@code GarbageCollectorMXBean} is:
 * <ul>
 * <li>A {@linkplain #GARBAGE_COLLECTION_NOTIFICATION garbage collection notification}.
 *     <br>Used by every notification emitted by the garbage collector, the details about
 *         the notification are provided in the {@linkplain #getGcAction action} String.
 * </li>
 * </ul>
 **/
public class GarbageCollectionNotificationInfo implements CompositeDataView {

	/**
	 * Notification type denoting that the Java virtual machine has completed a garbage collection cycle.
	 * This notification is emitted by a GarbageCollectorMXBean. The value of this notification type is
	 * "com.ibm.lang.management.gc.notification" which matches RI naming for compatibility.
	 */
	public static final String GARBAGE_COLLECTION_NOTIFICATION = "com.sun.management.gc.notification"; //$NON-NLS-1$

	/**
	 * Comment for <code>gcName</code>
	 */
	private final String gcName;

	/**
	 * Comment for <code>gcAction</code>
	 */
	private final String gcAction;

	/**
	 * Comment for <code>gcCause</code>
	 */
	private final String gcCause;

	/**
	 * Comment for <code>gcInfo</code>
	 */
	private final GcInfo gcInfo;

	private CompositeData cdata;
	
	private CompositeData getCompositeData() {
		if (null == cdata) {
			cdata = GarbageCollectionNotificationInfoUtil.toCompositeData(this);
		}
		return cdata;
	}

	private void setCompositeData(CompositeData cd) {
		cdata = cd;
	}
	
	/**
	 * Creates a new <code>GarbageCollectionNotificationInfo</code> instance.
	 * 
	 * @param gcName
	 *			   the name of the garbage collector used to perform the collection
	 * @param gcAction
	 *            the action of the performed by the garbage collector
	 * @param gcCause
	 *            the cause the garbage collection
	 * @param gcInfo  
	 * 			  a GcInfo object providing statistics about the GC cycle
	 * 
	 */
    public GarbageCollectionNotificationInfo(String gcName, String gcAction, String gcCause, GcInfo gcInfo) {
		super();
		this.gcName = gcName;
		this.gcAction = gcAction;
		this.gcCause = gcCause;
		this.gcInfo = gcInfo;
	}

	/**
	 * @return the name of the garbage collector used to perform the collection.
	 */
	public String getGcName() {
		return this.gcName;
	}

	/**
	 * @return the the action of the performed by the garbage collector
	 */
	public String getGcAction() {
		return this.gcAction;
	}

	/**
	 * @return the cause of the garbage collection
	 */
	public String getGcCause() {
		return this.gcCause;
	}

	/**
	 * @return the GC information related to the last garbage collection
	 */
	public GcInfo getGcInfo() {
		return gcInfo;
	}

	/**
	 * Returns a {@code GarbageCollectionNotificationInfo} object represented by the
	 * given {@code CompositeData}.
	 * The given {@code CompositeData} must contain the following attributes:
	 * <blockquote>
	 * <table border summary="">
	 * <tr>
	 *   <th align=left>Attribute Name</th>
	 *   <th align=left>Type</th>
	 * </tr>
	 * <tr>
	 *   <td>gcName</td>
	 *   <td>{@code java.lang.String}</td>
	 * </tr>
	 * <tr>
	 *   <td>gcAction</td>
	 *   <td>{@code java.lang.String}</td>
	 * </tr>
	 * <tr>
	 *   <td>gcCause</td>
	 *   <td>{@code java.lang.String}</td>
	 * </tr>
	 * <tr>
	 *   <td>gcInfo</td>
	 *   <td>{@code com.ibm.lang.management.GcInfo}</td>
	 * </tr>
	 * </table>
	 * </blockquote>
	 *
	 * @param cd {@code CompositeData} representing a
	 *     {@code GarbageCollectionNotificationInfo}
	 *
	 * @throws IllegalArgumentException if {@code cd} does not
	 *     represent a {@code GarbageCollectionNotificationInfo} object
	 *
	 * @return a {@code GarbageCollectionNotificationInfo} object represented
	 *     by {@code cd} if {@code cd} is not {@code null};
	 *     {@code null} otherwise
	 */
	public static GarbageCollectionNotificationInfo from(CompositeData cd) {
		GarbageCollectionNotificationInfo result = null;

        if (cd != null) {
            /* Does cd meet the necessary criteria to create a new
			 * GarbageCollectionNotificationInfo?
			 * If not then exit on an IllegalArgumentException.
			 */
			ManagementUtils.verifyFieldNumber(cd, 4);
			String[] attributeNames = { "gcName", "gcAction", "gcCause", "gcInfo" }; //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$
			ManagementUtils.verifyFieldNames(cd, attributeNames);
			String[] attributeTypes = { "java.lang.String", //$NON-NLS-1$
					"java.lang.String", //$NON-NLS-1$
					"java.lang.String", //$NON-NLS-1$
					CompositeData.class.getName() };
			ManagementUtils.verifyFieldTypes(cd, attributeNames, attributeTypes); 

			/* Extract the values of the attributes and use them to construct
			 * a new GarbageCollectionNotificationInfo.
			 */
			Object[] attributeVals = cd.getAll(attributeNames);
			String gcNameVal = (String) attributeVals[0];
			String gcActionVal = (String) attributeVals[1];
			String gcCauseVal = (String) attributeVals[2];
			GcInfo gcInfoVal = GcInfo.from((CompositeData) attributeVals[3]);

			result = new GarbageCollectionNotificationInfo(gcNameVal, gcActionVal, gcCauseVal, gcInfoVal);
			result.setCompositeData(cd);
		}

		return result;
	}

	/* Implementation of the CompositeDataView interface */
	
	/**
	 * <p>Return the {@code CompositeData} representation of this
	 * {@code GarbageCollectionNotificationInfo}.  
	 *
	 * @param ct the {@code CompositeType} that the caller expects.
	 * This parameter is ignored and can be null.
	 *
	 * @return the {@code CompositeData} representation.
	*/
	@Override
	public CompositeData toCompositeData(CompositeType ct) {
		return getCompositeData();
	}
}
