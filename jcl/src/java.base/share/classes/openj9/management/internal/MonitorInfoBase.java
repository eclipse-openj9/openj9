/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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
package openj9.management.internal;

/**
 * Container for monitor information for use by classes which have access only to the
 * java.base module.
 * This is typically wrapped in a MonitorInfo object.
 */
public class MonitorInfoBase extends LockInfoBase {
	private final int stackDepth;
	private final StackTraceElement stackFrame;

	/**
	 * @param classNameVal name, including the package prefix, of the object's class
	 * @param identityHashCodeVal object's identity hash code
	 * @param stackDepthVal the number of frames deep in the stack where the thread locked the monitor
	 * @param stackFrameVal complete stack frame at which the thread locked the monitor
	 */
	public MonitorInfoBase(String classNameVal, int identityHashCodeVal, int stackDepthVal, StackTraceElement stackFrameVal) {
		super(classNameVal, identityHashCodeVal);
		if (((stackFrameVal == null) && (stackDepthVal >= 0))
				|| ((stackFrameVal != null) && (stackDepthVal < 0))) {
			String arg;
			if (stackFrameVal == null) {
				/*[MSG "K0610", "null"]*/
				arg = com.ibm.oti.util.Msg.getString("K0610"); //$NON-NLS-1$
			} else {
				/*[MSG "K0611", "not null"]*/
				arg = com.ibm.oti.util.Msg.getString("K0611"); //$NON-NLS-1$
			}
			/*[MSG "K060F", "Parameter stackDepth is {0} but stackFrame is {1}"]*/
			throw new IllegalArgumentException(
					com.ibm.oti.util.Msg.getString("K060F", Integer.valueOf(stackDepthVal), arg)); //$NON-NLS-1$
		}
		stackDepth = stackDepthVal;
		stackFrame = stackFrameVal;
	}

	/**
	 * @return depth of the stack frame in which the object was locked
	 */
	public int getStackDepth() {
		return stackDepth;
	}

	/**
	 * @return StackTraceElement for the locking frame
	 */
	public StackTraceElement getStackFrame() {
		return stackFrame;
	}

}
