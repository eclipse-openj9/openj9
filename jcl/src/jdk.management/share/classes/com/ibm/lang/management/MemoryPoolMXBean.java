/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2005, 2017 IBM Corp. and others
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
package com.ibm.lang.management;

import java.lang.management.MemoryUsage;

/**
 * The OpenJ9 interface for managing and monitoring the virtual machine's memory pools.
 *
 * The following list describes 4 common behavior changes for {@link MemoryPoolMXBean}. 
 * You can revert to the earlier implementation of {@link MemoryPoolMXBean} by setting the 
 * <b>-XX:+HeapManagementMXBeanCompatibility</b> Java command line option.
 * 
 * <h3>1. More detailed heap memory pools can be obtained by calling {@link java.lang.management.ManagementFactory#getMemoryPoolMXBeans}</h3>
 * The following names are reported for heap memory pools, listed by garbage collection policy:
 * <br><br>
 *	For <b>-Xgcpolicy:gencon</b>
 *	<ul>
 *		<li><b>nursery-allocate</b>
 *		<li><b>nursery-survivor</b>
 *		<li><b>tenured-LOA</b>
 *		<li><b>tenured-SOA</b>
 *	</ul>
 * <br>
 *	For <b>-Xgcpolicy:optthruput</b> and <b>-Xgcpolicy:optavgpause</b>
 *	<ul>
 *		<li><b>tenured-LOA</b>
 *		<li><b>tenured-SOA</b>
 *	</ul>
 * <br>
 *	For <b>-Xgcpolicy:balanced</b>
 *	<ul>
 *		<li><b>balanced-reserved</b>
 *		<li><b>balanced-eden</b>
 *		<li><b>balanced-survivor</b>
 *		<li><b>balanced-old</b>
 *	</ul>
 * <br>
 *	For <b>-Xgcpolicy:metronome</b>
 *	<ul>
 *		<li><b>JavaHeap</b>
 *	</ul>
 *
 * <br><p>
 * If you set the <b>-XX:+HeapManagementMXBeanCompatibility</b> option to turn on compatibility with earlier versions of the VM,
 * information about heap memory pools is reported in the older format.
 * <br>
 * The following name is reported for the heap memory pool for all garbage collection policies in the old format:
 * <ul>
 * 	<li><b>Java heap</b>
 * </ul>
 * 
 * <h3>2. Memory Usage</h3>
 * Memory usage for each heap memory pool can be retrieved by using {@link java.lang.management.MemoryPoolMXBean#getUsage} or {@link java.lang.management.MemoryPoolMXBean#getCollectionUsage}.
 * In some cases the total sum of memory usage of all heap memory pools is more than the maximum heap size. 
 * This irregularity can be caused if data for each pool is collected between garbage collection cycles,
 * where objects have been moved or reclaimed.
 * If you want to collect memory usage data that is synchronized across the memory pools, use the 
 * {@link com.sun.management.GarbageCollectionNotificationInfo} or {@link com.sun.management.GarbageCollectorMXBean#getLastGcInfo} extensions.
 * 
 * <h3>3. Usage Threshold ({@link java.lang.management.MemoryPoolMXBean#getUsageThreshold}, {@link java.lang.management.MemoryPoolMXBean#setUsageThreshold}, {@link java.lang.management.MemoryPoolMXBean#isUsageThresholdExceeded})</h3>
 * The usage threshold attribute is designed for monitoring the increasing trend of memory usage and incurs only a low overhead.
 * This attribute is not appropriate for some memory pools.
 * Use the {@link java.lang.management.MemoryPoolMXBean#isUsageThresholdSupported} method to determine 
 * if this functionality is supported by the memory pool to avoid an unexpected {@link java.lang.UnsupportedOperationException}.
 * <br>
 * The following names are reported for heap memory pools that support the usage threshold attribute:
 * <ul>
 *	<li><b>JavaHeap</b>
 *	<li><b>tenured</b>
 *	<li><b>tenured-LOA</b>
 *	<li><b>tenured-SOA</b>
 *	<li><b>balanced-survivor</b>
 *	<li><b>balanced-old</b>
 * </ul>
 * 
 * <h3>4. Collection Usage Threshold ({@link java.lang.management.MemoryPoolMXBean#getCollectionUsageThreshold}, {@link java.lang.management.MemoryPoolMXBean#setCollectionUsageThreshold}, {@link java.lang.management.MemoryPoolMXBean#isCollectionUsageThresholdExceeded})</h3>
 * The collection usage threshold is a manageable attribute that is applicable only to some garbage-collected memory pools. 
 * This attribute reports the amount of memory taken up by objects that are still in use after a garbage collection cycle. 
 * Use the {@link java.lang.management.MemoryPoolMXBean#isCollectionUsageThresholdSupported} method to determine 
 * if this functionality is supported by the memory pool to avoid an unexpected {@link java.lang.UnsupportedOperationException}.
 * <br>
 * The following names are reported for heap memory pools that support the collection usage threshold attribute:
 * <ul>
 *	<li><b>JavaHeap</b>
 *	<li><b>tenured</b>
 *	<li><b>tenured-LOA</b>
 *	<li><b>tenured-SOA</b>
 *	<li><b>nursery-allocate</b>
 *	<li><b>balanced-eden</b>
 *	<li><b>balanced-survivor</b>
 *	<li><b>balanced-old</b>
 * </ul>
 * <br>
 * <br>
 * @since 1.5
 */
public interface MemoryPoolMXBean extends java.lang.management.MemoryPoolMXBean {

	/**
	 * If supported by the virtual machine, returns a {@link java.lang.management.MemoryUsage} which
	 * encapsulates this memory pool's memory usage <em>before</em> the most
	 * recent run of the garbage collector. No garbage collection will be
	 * actually occur as a result of this method getting called.
	 * <p>
	 * The method will return a <code>null</code> if the virtual machine does
	 * not support this type of functionality.
	 * </p>
	 * <h2>MBeanServer access:</h2>
	 * The return value will be mapped to a
	 * {@link javax.management.openmbean.CompositeData} with attributes as
	 * specified in {@link java.lang.management.MemoryUsage}.
	 * 
	 * @return a {@link java.lang.management.MemoryUsage} containing the usage details for the memory
	 *         pool just before the most recent collection occurred.
	 */
	public MemoryUsage getPreCollectionUsage();

}
