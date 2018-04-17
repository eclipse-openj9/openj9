/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2011, 2018 IBM Corp. and others
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
 * A ConstantCallSite is permanently bound to its initial target MethodHandle.  
 * Any call to {@link #setTarget(MethodHandle)} will result in an UnsupportedOperationException.
 * 
 * @since 1.7
 */
public class ConstantCallSite extends CallSite {
	private final MethodHandle target; 

	/**
	 * Create a ConstantCallSite with a target MethodHandle that cannot change.
	 * 
	 * @param permanentTarget - the target MethodHandle to permanently associate with this CallSite.
	 */
	public ConstantCallSite(MethodHandle permanentTarget) {
		super(permanentTarget.type());
		// .type() call ensures non-null
		target = permanentTarget;
	}
	
	/**
	 * Create a ConstantCallSite and assign the hook MethodHandle's result to its permanent target.
	 * The hook MethodHandle is invoked as though by (@link MethodHandle#invoke(this)) and must return a MethodHandle that will be installed 
	 * as the ConstantCallSite's target.
	 * <p>
	 * The hook MethodHandle is required if the ConstantCallSite's target needs to have access to the ConstantCallSite instance.  This is an
	 * action that user code cannot perform on its own.
	 * <p>
	 * The hook must return a MethodHandle that is exactly of type <i>targetType</i>.
	 * <p>
	 * Until the result of the hook has been installed in the ConstantCallSite, any call to getTarget() or dynamicInvoker() will throw an 
	 * IllegalStateException.  It is always valid to call type().
	 * 
	 * @param targetType - the type of the ConstantCallSite's target
	 * @param hook - the hook handle, with signature (ConstantCallSite)MethodHandle
	 * @throws Throwable anything thrown by the hook.
	 * @throws WrongMethodTypeException if the hook has the wrong signature or returns a MethodHandle with the wrong signature
	 * @throws NullPointerException if the hook is null or returns null
	 * @throws ClassCastException if the result of the hook is not a MethodHandle
	 */
	protected ConstantCallSite(MethodType targetType, MethodHandle hook) throws Throwable, WrongMethodTypeException, NullPointerException, ClassCastException {
		super(targetType);
		MethodHandle handle = null;
		if (hook != null) {
			handle = (MethodHandle) hook.invoke(this);
		}
		handle.getClass(); // Throw NPE if null
		if (handle.type != targetType) {
			throw WrongMethodTypeException.newWrongMethodTypeException(targetType, handle.type);
		}
		target = handle;
	}
	
	/**
	 * Return the target MethodHandle of this CallSite.
	 * @throws IllegalStateException - if the target has not yet been assigned in the ConstantCallSite constructor
	 */
	@Override
	public final MethodHandle dynamicInvoker() throws IllegalStateException {
		return getTarget();
	}

	/**
	 * Return the target MethodHandle of this CallSite.
	 * The target is defined as though it where a final field.
	 * 
	 * @throws IllegalStateException - if the target has not yet been assigned in the ConstantCallSite constructor
	 */
	@Override
	public final MethodHandle getTarget() throws IllegalStateException {
		if (target == null) {
			throw new IllegalStateException();
		}
		return target;
	}

	/**
	 * Throws UnsupportedOperationException as a ConstantCallSite is permanently 
	 * bound to its initial target MethodHandle.
	 */
	@Override
	public final void setTarget(MethodHandle newTarget) {
		throw new UnsupportedOperationException();
	}
}

