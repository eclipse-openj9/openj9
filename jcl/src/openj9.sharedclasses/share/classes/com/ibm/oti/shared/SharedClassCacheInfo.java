/*[INCLUDE-IF SharedClasses]*/
package com.ibm.oti.shared;

/*******************************************************************************
 * Copyright (c) 2010, 2019 IBM Corp. and others
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

/**
 * SharedClassCacheInfo stores information about a shared class cache and
 * provides methods to retrieve that information.
 * <p>
 */
public class SharedClassCacheInfo {
	
	/**
	 * Specifies a Java 5 cache. 
	 */
	static final public int JVMLEVEL_JAVA5 = 1;
	/**
	 * Specifies a Java 6 cache. 
	 */
	static final public int JVMLEVEL_JAVA6 = 2;
	/**
	 * Specifies a Java 7 cache. 
	 */
	static final public int JVMLEVEL_JAVA7 = 3;
	/**
	 * Specifies a Java 8 cache. 
	 */
	static final public int JVMLEVEL_JAVA8 = 4;
	/*[IF Sidecar19-SE]*/
	/**
	 * Specifies a Java 9 cache.
	 */
	static final public int JVMLEVEL_JAVA9 = 5;
	/*[ENDIF] Sidecar19-SE */
	/**
	 * Specifies a 32-bit cache.
	 */
	static final public int ADDRESS_MODE_32 = 32;
	
	/**
	 * Specifies a 64-bit cache.
	 */
	static final public int ADDRESS_MODE_64 = 64;
	/**
	 * Specifies a compressedRefs cache.
	 */
	static final public int COMPRESSED_REFS = 1;
	/**
	 * Specifies a non-compressedRefs cache.
	 */
	static final public int NON_COMPRESSED_REFS = 0;
	/**
	 * The compressedRefs mode is unknown for the cache.
	 */
	static final public int COMPRESSED_REFS_UNKNOWN = -1;
	
	private String name;
	private boolean isCompatible;
	private boolean isPersistent;
	private int osShmid;
	private int osSemid;
	private long lastDetach;
	private int modLevel;
	private int addrMode;
	private boolean isCorrupt;
	private long cacheSize;
	private long freeBytes;
	private int cacheType;
	private long softMaxBytes;
	
	/**
	 * Gets the cache name for the shared class cache.
	 *
	 * @return		Name of the shared class cache.
	 */
	public String getCacheName() {
		return name;
	}
	
	/**
	 * Checks the compatibility of the shared class cache with this JVM. 
	 *
	 * @return		true if cache is compatible with this JVM, false otherwise.
	 */
	public boolean isCacheCompatible() {
		return isCompatible;
	}
	
	/**
	 * Checks if the shared class cache is persistent.
	 *
	 * @deprecated Use getCacheType() instead.
	 *
	 * @return		true if cache is persistent, false otherwise.
	 */
	@Deprecated
	public boolean isCachePersistent() {
		return isPersistent;
	}
	
	/**
	 * Check the type of the shared class cache.
	 *
	 * @return		Either {@link SharedClassUtilities#PERSISTENT},
	 * 				{@link SharedClassUtilities#NONPERSISTENT} or
	 * 				{@link SharedClassUtilities#SNAPSHOT}
	 */
	public int getCacheType() {
		return cacheType;
	}
	
	/**
	 * Gets the OS shared memory ID associated with the shared class cache. 
	 *
	 * @return		A valid value if cache is non-persistent and shared memory id is available, else -1. 
	 */
	public int getOSshmid() {
		return osShmid;
	}
	
	/**
	 * Gets the OS semaphore ID associated with the shared class cache. 
	 *
	 * @return		A valid value if cache is non-persistent and semaphore id is available, else -1. 
	 */
	public int getOSsemid() {
		return osSemid;
	}
	
	/**
	 * Gets the time when the shared class cache was last detached.
	 * 
	 * @return		Date, or null if last detach time is not available.
	 */
	public java.util.Date getLastDetach() {
		if (-1 == lastDetach) {
			return null;
		} else {
			return new java.util.Date(lastDetach);
		}
	}
	
	/**
	 * Gets the JVM level for the shared class cache.  
	/*[IF Java10] 
	 * Starting from Java 10, the JVM LEVEL equals to the java version number on which the share class cache is created.
	/*[ENDIF]
	 *
	 * @return		A JVMLEVEL constant.
	 */					
	public int getCacheJVMLevel() {
		return modLevel;
	}
	
	/**
	 * Gets the address mode for the shared class cache. 
	 *
	 * @return		Either {@link SharedClassCacheInfo#ADDRESS_MODE_32} or
	 * 				{@link SharedClassCacheInfo#ADDRESS_MODE_64} 
	 */			
	public int getCacheAddressMode() {
		final int addrModeMask = 0xFFFF;
		return addrMode & addrModeMask;
	}
	
	/**
	 * Checks if the shared class cache is corrupt.
	 *
	 * @return		true if the cache is corrupt, false otherwise. 
	 */		
	public boolean isCacheCorrupt() {
		return isCorrupt;
	}
	
	/**
	 * Gets total usable shared class cache size. Returns -1 if cache is incompatible.
	 *
	 * @return		Number of usable bytes in cache. 
	 */		
	public long getCacheSize() {
		return cacheSize;
	}
	
	/**
	 * Gets the amount of free bytes in the shared class cache. Returns -1 if cache is incompatible.
	 *
	 * @return		long
	 */		
	public long getCacheFreeBytes() {
		return freeBytes;
	}
	
	/**
	 * Get the soft limit for available space in the cache in bytes. Returns -1 if cache is incompatible or cache size if it is not set.
	 *
	 * @return		long
	 */
	public long getCacheSoftMaxBytes() {
		return softMaxBytes;
	}
	
	/**
	 * Get the compressedRefs mode for the shared class cache. 
	 *
	 * @return		Either {@link SharedClassCacheInfo#COMPRESSED_REFS} or
	 * 				{@link SharedClassCacheInfo#NON_COMPRESSED_REFS} or
	 * 				{@link SharedClassCacheInfo#COMPRESSED_REFS_UNKNOWN}
	 */			
	public int getCacheCompressedRefsMode() {
		final int compressedRefsMask = 0x10000;
		final int noncompressedRefsMask = 0x20000;
		int ret = NON_COMPRESSED_REFS;
		
		if (compressedRefsMask == (addrMode & compressedRefsMask)) {
			ret = COMPRESSED_REFS;
		} else if (noncompressedRefsMask == (addrMode & noncompressedRefsMask)) {
			ret = NON_COMPRESSED_REFS;
		} else {
			ret = COMPRESSED_REFS_UNKNOWN;
		}
		return ret;
	}

	/**
	 * Constructor to create an object of SharedClassCacheInfo using the parameter specified.
	 *
	 * @param		name			Shared class cache name
	 * @param		isCompatible	Compatibility of the cache
	 * @param		isPersistent	Cache type
	 * @param		osShmid			OS shared memory id for the cache
	 * @param		osSemid			OS semaphore id for the cache
	 * @param		lastDetach		Time of last detach
	 * @param		modLevel		JVM level of the cache
	 * @param		addrMode		Address mode of the cache
	 * @param		isCorrupt
	 * @param		cacheSize		Size of cache in bytes
	 * @param		freeBytes		Free bytes in cache
	 * @param		cacheType		Cache type
	 * @param		softMaxBytes	Soft limit for the available space in bytes
	 */		
	SharedClassCacheInfo(String name,
						boolean isCompatible, 
						boolean isPersistent, 
						int osShmid, 
						int osSemid,
						long lastDetach, 
						int modLevel, 
						int addrMode, 
						boolean isCorrupt, 
						long cacheSize, 
						long freeBytes,
						int cacheType,
						long softMaxBytes)
	{
		this.name = name;
		this.isCompatible = isCompatible;
		this.isPersistent = isPersistent;
		this.osShmid = osShmid;
		this.osSemid = osSemid;
		this.lastDetach = lastDetach;
		this.modLevel = modLevel;
		this.addrMode = addrMode;
		this.isCorrupt = isCorrupt;
		this.cacheSize = cacheSize;
		this.freeBytes = freeBytes;
		this.cacheType = cacheType;
		this.softMaxBytes = softMaxBytes;
	}
}
