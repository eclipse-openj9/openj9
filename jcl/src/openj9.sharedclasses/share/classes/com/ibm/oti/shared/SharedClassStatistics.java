/*[INCLUDE-IF SharedClasses]*/
/*
 * Copyright IBM Corp. and others 1998
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package com.ibm.oti.shared;

import java.io.File;

/**
 * SharedClassStatistics provides static functions that report basic cache statistics.
 */
public class SharedClassStatistics {

	/**
	 * Returns the size of the shared cache that the JVM is currently connected to.
	 *
	 * @return the total size in bytes
	 */
	public static long maxSizeBytes() {
		return maxSizeBytesImpl();
	}

	/**
	 * Returns the free space of the shared cache that the JVM is currently connected to.
	 *
	 * @return the free space in bytes
	 */
	public static long freeSpaceBytes() {
		return freeSpaceBytesImpl();
	}

	/**
	 * Returns the soft limit in bytes for the available space of the cache that the JVM is currently connected to.
	 *
	 * @return the soft max size or cache size in bytes if it is not set
	 */
	public static long softmxBytes() {
		return softmxBytesImpl();
	}

	/**
	 * Returns the minimum space reserved for AOT data of the cache that the JVM is currently
	 * connected to.
	 *
	 * @return the minimum shared classes cache space reserved for AOT data in bytes or -1 if it is not set
	 */
	public static long minAotBytes() {
		return minAotBytesImpl();
	}

	/**
	 * Returns the maximum space allowed for AOT data of the cache that the JVM is currently
	 * connected to.
	 *
	 * @return the maximum shared classes cache space allowed for AOT data in bytes or -1 if it is not set
	 */
	public static long maxAotBytes() {
		return maxAotBytesImpl();
	}

	/**
	 * Returns the minimum space reserved for JIT data of the cache that the JVM is currently
	 * connected to.
	 *
	 * @return the minimum shared classes cache space reserved for JIT data in bytes or -1 if it is not set
	 */
	public static long minJitDataBytes() {
		return minJitDataBytesImpl();
	}

	/**
	 * Returns the maximum space allowed for JIT data of the cache that the JVM is currently
	 * connected to.
	 *
	 * @return the maximum shared classes cache space allowed for JIT data in bytes  or -1 if it is not set
	 */
	public static long maxJitDataBytes() {
		return maxJitDataBytesImpl();
	}

	/**
	 * Returns the SysV shmem nattch value for a non-persistent top level cache.
	 *
	 * <p>On Windows or for persistent caches, -1 is returned. Depending on the platform
	 * and OS, such as z/OS, the value indicates the number of JVMs attached to the cache.
	 *
	 * @return the SysV shmem nattch value for a top level non-persistent cache, or -1 if it cannot be determined
	 */
	public static long numberAttached() {
		return numberAttachedImpl();
	}

	/**
	 * Returns the full cache path.
	 *
	 * @return the full cache path, or NULL if it cannot be determined
	 */
	public static String cachePath() {
		return cachePathImpl();
	}

	/**
	 * Returns the cache name.
	 *
	 * @return the cache name, or NULL if it cannot be determined
	 */
	public static String cacheName() {
		String cachePath = cachePathImpl();
		if (null != cachePath) {
			String fileName = new File(cachePath).getName();
			int prefixIndex = fileName.indexOf('_');
			int tailIndex = fileName.lastIndexOf('_');
			if (tailIndex > prefixIndex) {
				String cacheName = fileName.substring(prefixIndex + 1, tailIndex);
				if ((prefixIndex > 0) && (fileName.charAt(prefixIndex - 1) != 'P')) {
					/* Remove "memory_" from the non-persistent cache name. */
					int typeIndex = cacheName.indexOf('_');
					if (typeIndex > -1) {
						cacheName = cacheName.substring(typeIndex + 1);
					}
				}
				return cacheName;
			}
		}
		return null;
	}

	/**
	 * Returns the cache directory.
	 *
	 * @return the cache directory, or NULL if it cannot be determined
	 */
	public static String cacheDir() {
		String cachePath = cachePathImpl();
		if (null != cachePath) {
			return new File(cachePath).getParent();
		}
		return null;
	}

	private static native String cachePathImpl();
	private static native long freeSpaceBytesImpl();
	private static native long maxAotBytesImpl();
	private static native long maxJitDataBytesImpl();
	private static native long maxSizeBytesImpl();
	private static native long minAotBytesImpl();
	private static native long minJitDataBytesImpl();
	private static native long numberAttachedImpl();
	private static native long softmxBytesImpl();

	/*[IF]*/
	/*
	 * This explicit constructor is equivalent to the one implicitly defined
	 * in earlier versions. It was probably never intended to be public nor
	 * was it likely intended to be used, but rather than break any existing
	 * uses, we simply mark it deprecated.
	 */
	/*[ENDIF]*/
	/**
	 * Constructs a new instance of this class.
	 */
	@Deprecated
	public SharedClassStatistics() {
		super();
	}

}
