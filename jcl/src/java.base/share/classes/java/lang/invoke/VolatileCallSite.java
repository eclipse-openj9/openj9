/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2011, 2011 IBM Corp. and others
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
package java.lang.invoke;

/**
 * A VolatileCallSite acts as though its target MethodHandle were a volatile variable.  
 * This CallSite sub-class should be used if the changes to the target are frequent or if 
 * changes must be immediately observed by all threads, even if the {@link #setTarget(MethodHandle)}
 * occurs in a different thread.
 * <p>
 * Since VolatileCallSite's target is defined as though it were a volatile variable, there is no need
 * for a method like {@link MutableCallSite#syncAll(MutableCallSite[])} because every thread will always
 * see a consistent view of the MethodHandle target.
 *
 * @since 1.7
 */
public class VolatileCallSite extends CallSite {
	private volatile MethodHandle target;
	
	/**
	 * Create a VolatileCallSite with the same type as the volatileTarget 
	 * and the initial target set to volatileTarget.
	 * 
	 * @param volatileTarget - the target MethodHandle of the CallSite
	 * @throws NullPointerException - if the <i>volatileTarget</i> is null.
	 */
	public VolatileCallSite(MethodHandle volatileTarget) throws NullPointerException {
		// .type provides the NPE if volatileTarget null
		super(volatileTarget.type());
		target = volatileTarget;
	}
	
	/**
	 * Create a VolatileCallSite with the MethodType <i>type</i> and an
	 * initial target that throws IllegalStateException.
	 * 
	 * @param type - the permanent type of this CallSite.
	 * @throws NullPointerException - if the type is null.
	 */
	public VolatileCallSite(MethodType type) throws NullPointerException {
		super(type);
		// install a target that throws IllegalStateException
		target = CallSite.initialTarget(type);
	}
	
	@Override
	public final MethodHandle dynamicInvoker() {
		return new DynamicInvokerHandle(this);
	}

	/**
	 * The target MethodHandle is returned as though by a read of a volatile variable.
	 */
	@Override
	public final MethodHandle getTarget() {
		return target;
	}

	/**
	 * Set the CallSite's target to be <i>nextTarget</i>.
	 * The <i>nextTarget</i> MethodHandle must have the same type as the CallSite.
	 * This occurs as though by a write to a volatile variable.
	 */
	@Override
	public void setTarget(MethodHandle nextTarget) throws NullPointerException, WrongMethodTypeException {
		// newTarget.type provides NPE if null
		if (!type().equals(nextTarget.type())) {
			throw new WrongMethodTypeException();
		}
		target = nextTarget;
	}
	
}

