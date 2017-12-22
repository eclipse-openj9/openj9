/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2007, 2017 IBM Corp. and others
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
import com.ibm.java.lang.management.internal.StackTraceElementUtil;

/**
 * This class represents information about objects locked via
 * a synchronized method or block.
 *
 * @since 1.6
 */
public class MonitorInfo extends LockInfo {

	private final int stackDepth;

	private final StackTraceElement stackFrame;

	/**
	 * Creates a new <code>MonitorInfo</code> instance.
	 *
	 * @param className
	 *            the name (including the package prefix) of the associated
	 *            monitor lock object's class
	 * @param identityHashCode
	 *            the value of the associated monitor lock object's identity
	 *            hash code. This amounts to the result of calling
	 *            {@link System#identityHashCode(Object)} with the monitor lock
	 *            object as the sole argument.
	 * @param stackDepth
	 *            the number of frames deep in the stack where the locking of
	 *            the monitor took place
	 * @param stackFrame
	 *            the complete stack frame at which the locking of the monitor
	 *            occurred
	 * @throws IllegalArgumentException
	 *             if either of the following two conditions apply:
	 *             <ul>
	 *             <li>the supplied <code>stackFrame</code> is non-<code>null</code>
	 *             yet the value of <code>stackDepth</code> is less than zero
	 *             <li>the supplied <code>stackFrame</code> is
	 *             <code>null</code> yet the value of <code>stackDepth</code>
	 *             is zero or greater
	 *             </ul>
	 */
	public MonitorInfo(String className, int identityHashCode, int stackDepth, StackTraceElement stackFrame) {
		super(className, identityHashCode);
		if ((stackFrame == null && stackDepth >= 0) || (stackFrame != null && stackDepth < 0)) {
			String arg;
			if (stackFrame == null) {
				/*[MSG "K0610", "null"]*/
				arg = com.ibm.oti.util.Msg.getString("K0610"); //$NON-NLS-1$
			} else {
				/*[MSG "K0611", "not null"]*/
				arg = com.ibm.oti.util.Msg.getString("K0611"); //$NON-NLS-1$
			}
			/*[MSG "K060F", "Parameter stackDepth is {0} but stackFrame is {1}"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K060F", stackDepth, arg)); //$NON-NLS-1$
		}
		this.stackDepth = stackDepth;
		this.stackFrame = stackFrame;
	}

	private MonitorInfo(Object object, int stackDepth, StackTraceElement stackFrame) {
		super(object.getClass().getName(), System.identityHashCode(object));
		this.stackDepth = stackDepth;
		this.stackFrame = stackFrame;
	}

	/**
	 * Returns an integer which is the number of frames deep into the stack
	 * where the monitor locking took place.
	 *
	 * @return the number of frames into the stack trace at which point the
	 *         monitor object locking too place
	 */
	public int getLockedStackDepth() {
		return stackDepth;
	}

	/**
	 * The complete {@link StackTraceElement} in which the monitor was locked.
	 *
	 * @return the <code>StackTraceElement</code> in which the associated
	 *         monitor was locked
	 */
	public StackTraceElement getLockedStackFrame() {
		return stackFrame;
	}

	/**
	 * Receives a {@link CompositeData} representing a <code>MonitorInfo</code>
	 * object and attempts to return the root <code>MonitorInfo</code>
	 * instance.
	 *
	 * @param cd
	 *            a <code>CompositeData</code> that represents a
	 *            <code>MonitorInfo</code>.
	 * @return if <code>cd</code> is non- <code>null</code>, returns a new
	 *         instance of <code>MonitorInfo</code>. If <code>cd</code> is
	 *         <code>null</code>, returns <code>null</code>.
	 * @throws IllegalArgumentException
	 *             if argument <code>cd</code> does not correspond to a
	 *             <code>MonitorInfo</code> with the expected attributes of a
	 *             {@link LockInfo}&nbsp;-&nbsp;<code>className</code>(<code>java.lang.String</code>)
	 *             and <code>identityHashCode</code>(<code>java.lang.Integer</code>)&nbsp;-&nbsp;
	 *             plus the following:
	 *             <ul>
	 *             <li><code>lockedStackFrame</code>(<code>javax.management.openmbean.CompositeData</code>)
	 *             <li><code>lockedStackDepth</code>(
	 *             <code>java.lang.Integer</code>)
	 *             </ul>
	 *             The <code>lockedStackFrame</code> attribute must correspond
	 *             to a <code>java.lang.StackTraceElement</code> which has the
	 *             following attributes:
	 *             <ul>
/*[IF Sidecar19-SE]
	 *             <li><code>moduleName</code>(<code>java.lang.String</code>)
	 *             <li><code>moduleVersion</code>(<code>java.lang.String</code>)
/*[ENDIF]
	 *             <li><code>className</code> (<code>java.lang.String</code>)
	 *             <li><code>methodName</code> (<code>java.lang.String</code>)
	 *             <li><code>fileName</code> (<code>java.lang.String</code>)
	 *             <li><code>lineNumber</code> (<code>java.lang.Integer</code>)
	 *             <li><code>nativeMethod</code> (<code>java.lang.Boolean</code>)
	 *             </ul>
	 */
	public static MonitorInfo from(CompositeData cd) {
		MonitorInfo result = null;

		if (cd != null) {
			// Does cd meet the necessary criteria to create a new MonitorInfo?
			// If not then exit on an IllegalArgumentException.
			ManagementUtils.verifyFieldNumber(cd, 4);
			String[] attributeNames = { "className", "identityHashCode", //$NON-NLS-1$ //$NON-NLS-2$
					"lockedStackFrame", "lockedStackDepth" }; //$NON-NLS-1$ //$NON-NLS-2$
			ManagementUtils.verifyFieldNames(cd, attributeNames);
			String[] attributeTypes = { "java.lang.String", "java.lang.Integer", //$NON-NLS-1$ //$NON-NLS-2$
					CompositeData.class.getName(), "java.lang.Integer" }; //$NON-NLS-1$
			ManagementUtils.verifyFieldTypes(cd, attributeNames, attributeTypes);

			// Extract the values of the attributes and use them to construct
			// a new MonitorInfo.
			Object[] attributeVals = cd.getAll(attributeNames);
			String classNameVal = (String) attributeVals[0];
			int idHashCodeVal = ((Integer) attributeVals[1]).intValue();
			CompositeData lockedStackFrameCDVal = (CompositeData) attributeVals[2];
			StackTraceElement lockedStackFrameVal = StackTraceElementUtil.from(lockedStackFrameCDVal);
			int lockedStackDepthVal = ((Integer) attributeVals[3]).intValue();
			result = new MonitorInfo(classNameVal, idHashCodeVal, lockedStackDepthVal, lockedStackFrameVal);
		}

		return result;
	}

}
