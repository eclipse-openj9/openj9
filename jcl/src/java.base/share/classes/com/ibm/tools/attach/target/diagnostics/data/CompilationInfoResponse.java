/*[INCLUDE-IF Sidecar16]*/
package com.ibm.tools.attach.target.diagnostics.data;

import java.io.Serializable;
import java.lang.management.CompilationMXBean;

/**
 * Information from {@link CompilationMXBean}
 */
public class CompilationInfoResponse implements Serializable {
	private static final long serialVersionUID = 1L;
	private final long compilationTime;
	
	/**
	 * Constructor
	 * @param compilationTime information from {@link CompilationMXBean#getTotalCompilationTime()}
	 */
	public CompilationInfoResponse(long compilationTime) {
		super();
		this.compilationTime = compilationTime;
	}

	/**
	 * @return {@link CompilationMXBean#getTotalCompilationTime()}
	 */
	public long getCompilationTime() {
		return compilationTime;
	}
	
}
