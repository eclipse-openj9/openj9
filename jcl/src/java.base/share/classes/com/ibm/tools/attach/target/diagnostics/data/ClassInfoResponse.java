/*[INCLUDE-IF Sidecar16]*/
package com.ibm.tools.attach.target.diagnostics.data;

import java.io.Serializable;
import java.lang.management.ClassLoadingMXBean;

/**
 * Information from {@link ClassLoadingMXBean} 
 */
public class ClassInfoResponse implements Serializable {
	private static final long serialVersionUID = 1L;

	private final int loadedClassCount;
	private final long unloadedClassCount;
	private final long totalLoadedClassCount;
	
	/**
	 * Constructor
	 * @param loadedClassCount information from {@link ClassLoadingMXBean#getLoadedClassCount()}
	 * @param unloadedClassCount information from {@link ClassLoadingMXBean#getUnloadedClassCount()}
	 * @param totalLoadedClassCount information from {@link ClassLoadingMXBean#getTotalLoadedClassCount()}
	 */
	public ClassInfoResponse(int loadedClassCount, long unloadedClassCount, long totalLoadedClassCount) {
		super();
		this.loadedClassCount = loadedClassCount;
		this.unloadedClassCount = unloadedClassCount;
		this.totalLoadedClassCount = totalLoadedClassCount;
	}

	/**
	 * @return {@link ClassLoadingMXBean#getLoadedClassCount()}
	 */
	public int getLoadedClassCount() {
		return loadedClassCount;
	}

	/**
	 * @return {@link ClassLoadingMXBean#getUnloadedClassCount()}
	 */
	public long getUnloadedClassCount() {
		return unloadedClassCount;
	}

	/**
	 * @return {@link ClassLoadingMXBean#getTotalLoadedClassCount()}
	 */
	public long getTotalLoadedClassCount() {
		return totalLoadedClassCount;
	}
}
