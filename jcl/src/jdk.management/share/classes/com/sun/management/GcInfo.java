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

import java.lang.management.MemoryUsage;
import java.util.Collection;
import java.util.HashMap;
import java.util.Map;

import javax.management.openmbean.CompositeData;
import javax.management.openmbean.CompositeDataView;
import javax.management.openmbean.CompositeType;
import javax.management.openmbean.TabularData;

import com.ibm.java.lang.management.internal.ManagementUtils;
import com.sun.management.internal.GcInfoUtil;

/**
 * Garbage collection information.  It contains the following
 * information for one garbage collection as well as GC-specific
 * attributes:
 * <blockquote>
 * <ul>
 *   <li>Start time</li>
 *   <li>End time</li>
 *   <li>Duration</li>
 *   <li>Memory usage before the collection starts</li>
 *   <li>Memory usage after the collection ends</li>
 * </ul>
 * </blockquote>
 * 
 * @since   1.9
 */
public class GcInfo implements CompositeData, CompositeDataView {

	/**
	 * Comment for <code>index</code>
	 */
	private final long index;

	/**
     * Comment for <code>startTime</code>
     */
	private final long startTime;
	
    /**
     * Comment for <code>endTime</code>
     */
	private final long endTime;
	
    /**
     * Comment for <code>usageBeforeGc</code>
     */
	private final Map<String, MemoryUsage> usageBeforeGc;
	
    /**
     * Comment for <code>usageAfterGc</code>
     */
	private final Map<String, MemoryUsage> usageAfterGc;

	private CompositeData cdata;
	
	private CompositeData getCompositeData() {
		if (null == cdata) {
			cdata = GcInfoUtil.toCompositeData(this);
		}
		return cdata;
	}
	
	private void setCompositeData(CompositeData cd) {
		cdata = cd;
	}
    /**
     * Creates a new <code>GcInfo</code> instance.
     * 
     * @param index
     * 			  the identifier of this garbage collection which is the number of collections that this collector has done
     * @param startTime
     * 			  the start time of the collection in milliseconds since the Java virtual machine was started.
     * @param endTime
     * 			  the end time of the collection in milliseconds since the Java virtual machine was started.
     * @param usageBeforeGc
     * 			  the memory usage of all memory pools at the beginning of this GC.
     * @param usageAfterGc
     * 			  the memory usage of all memory pools at the end of this GC.
     * 
     */
	private GcInfo(long index, long startTime, long endTime, Map<String, MemoryUsage> usageBeforeGc, Map<String, MemoryUsage> usageAfterGc) {
		super();
		this.index = index;
		this.startTime = startTime;
		this.endTime = endTime;
		this.usageBeforeGc = usageBeforeGc;
		this.usageAfterGc = usageAfterGc;
	}
	
	/**
	 *  @return the identifier of this garbage collection which is
	 *  the number of collections that this collector has done.
	 */
	public long getId() {
		return index;
	}

	/**
	 * Returns the start time of this GC in milliseconds
	 * since the Java virtual machine was started.
	 *
	 * @return the start time of this GC.
	 */
	public long getStartTime() {
		return this.startTime;
	}

	/**
	 * Returns the end time of this GC in milliseconds
	 * since the Java virtual machine was started.
	 *
	 * @return the end time of this GC.
	 */
	public long getEndTime() {
		return this.endTime;
	}

	/**
	 * Returns the elapsed time of this GC in milliseconds.
	 *
	 * @return the elapsed time of this GC in milliseconds.
	 */
	public long getDuration() {
		return this.endTime - this.startTime;

	}

	/**
	 * Returns the memory usage of all memory pools
	 * at the beginning of this GC.
	 * This method returns
	 * a <code>Map</code> of the name of a memory pool
	 * to the memory usage of the corresponding
	 * memory pool before GC starts.
	 *
	 * @return a <code>Map</code> of memory pool names to the memory
	 * usage of a memory pool before GC starts.
	 */
	public Map<String, MemoryUsage> getMemoryUsageBeforeGc() {
		return this.usageBeforeGc;
	}

	/**
	 * Returns the memory usage of all memory pools
	 * at the end of this GC.
	 * This method returns
	 * a <code>Map</code> of the name of a memory pool
	 * to the memory usage of the corresponding
	 * memory pool when GC finishes.
	 *
	 * @return a <code>Map</code> of memory pool names to the memory
	 * usage of a memory pool when GC finishes.
	 */
	public Map<String, MemoryUsage> getMemoryUsageAfterGc() {
		return this.usageAfterGc;
	}

	/**
	 * Returns a <code>GcInfo</code> object represented by the
	 * given <code>CompositeData</code>. The given
	 * <code>CompositeData</code> must contain
	 * all the following attributes:
	 *
	 * <blockquote>
	 * <table border summary="">
	 * <tr>
	 *   <th align=left>Attribute Name</th>
	 *   <th align=left>Type</th>
	 * </tr>
	 * <tr>
	 *   <td>index</td>
	 *   <td><code>java.lang.Long</code></td>
	 * </tr>
	 * <tr>
	 *   <td>startTime</td>
	 *   <td><code>java.lang.Long</code></td>
	 * </tr>
	 * <tr>
	 *   <td>endTime</td>
	 *   <td><code>java.lang.Long</code></td>
	 * </tr>
	 * <tr>
	 *   <td>memoryUsageBeforeGc</td>
	 *   <td><code>javax.management.openmbean.TabularData</code></td>
	 * </tr>
	 * <tr>
	 *   <td>memoryUsageAfterGc</td>
	 *   <td><code>javax.management.openmbean.TabularData</code></td>
	 * </tr>
	 * </table>
	 * </blockquote>
	 * @param cd <code>CompositeData</code> representing a <code>GcInfo</code>
	 *
	 * @throws IllegalArgumentException if <code>cd</code> does not
	 *   represent a <code>GcInfo</code> object with the attributes
	 *   described above.
	 *
	 * @return a <code>GcInfo</code> object represented by <code>cd</code>
	 * if <code>cd</code> is not <code>null</code>; <code>null</code> otherwise.
	 */
	public static GcInfo from(CompositeData cd) {
		GcInfo result = null;

		if (cd != null) {
            // Does cd meet the necessary criteria to create a new GcInfo?
            // If not then exit on an IllegalArgumentException.
            ManagementUtils.verifyFieldNumber(cd, 5);
            String[] attributeNames = { "index", "startTime", "endTime", "usageBeforeGc", "usageAfterGc" }; //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$ //$NON-NLS-5$
            ManagementUtils.verifyFieldNames(cd, attributeNames);
			String[] attributeTypes = { "java.lang.Long", //$NON-NLS-1$
										"java.lang.Long", //$NON-NLS-1$
										"java.lang.Long", //$NON-NLS-1$
										TabularData.class.getName(),
            							TabularData.class.getName()};
			ManagementUtils.verifyFieldTypes(cd, attributeNames, attributeTypes);

			// Extract the values of the attributes and use them to construct a new GcInfo.
			Object[] attributeVals = cd.getAll(attributeNames);
			long indexVal = ((Long) attributeVals[0]).longValue();
			long startTimeVal = ((Long) attributeVals[1]).longValue();
			long endTimeVal = ((Long) attributeVals[2]).longValue();
			Map<String, MemoryUsage> usageBeforeGcVal = convertTabularDataToMemoryUsageMap((TabularData) attributeVals[3]);
			Map<String, MemoryUsage> usageAfterGcVal = convertTabularDataToMemoryUsageMap((TabularData) attributeVals[4]);

			result = new GcInfo(indexVal, startTimeVal, endTimeVal, usageBeforeGcVal, usageAfterGcVal);
			result.setCompositeData(cd);
		}

		return result;
	}

	private static Map<String, MemoryUsage> convertTabularDataToMemoryUsageMap(TabularData td) {
		Map<String, MemoryUsage> result = new HashMap<>();

		for (CompositeData row : (Collection<CompositeData>) td.values()) {
			String keyVal = (String) row.get("key"); //$NON-NLS-1$
			MemoryUsage usageVal = MemoryUsage.from((CompositeData) row.get("value")); //$NON-NLS-1$
			result.put(keyVal, usageVal);
		}

		return result;
	}

	/* Implementation of the CompositeData interface */
	
	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean containsKey(String key) {
		return getCompositeData().containsKey(key);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean containsValue(Object value) {
		return getCompositeData().containsValue(value);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public boolean equals(Object obj) {
		return getCompositeData().equals(obj);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public Object get(String key) {
		return getCompositeData().get(key);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public Object[] getAll(String[] keys) {
		return getCompositeData().getAll(keys);
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public CompositeType getCompositeType() {
		return getCompositeData().getCompositeType();
	}
 
	/**
	 * {@inheritDoc}
	 */
	@Override
	public int hashCode() {
		return getCompositeData().hashCode();
	}
 
	/**
	 * {@inheritDoc}
	 */
	@Override
	public String toString() {
		return getCompositeData().toString();
	}

	/**
	 * {@inheritDoc}
	 */
	@Override
	public Collection<?> values() {
		return getCompositeData().values();
	}

	/* Implementation of the CompositeDataView interface */
	
	/**
	 * <p>Return the {@code CompositeData} representation of this
	 * {@code GcInfo}, including any GC-specific attributes.  The
	 * returned value will have at least all the attributes described
	 * in the {@link #from(CompositeData) from} method, plus optionally
	 * other attributes.
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
