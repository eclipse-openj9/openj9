/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2009, 2019 IBM Corp. and others
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
 * WrongMethodTypeException is thrown to indicate an attempt to invoke a MethodHandle with the wrong MethodType.
 * This exception can also be thrown when adapting a MethodHandle in a way that is incompatible with its MethodType.
 * 
 * @author		OTI
 * @version		initial
 * @since		1.7
 */
@VMCONSTANTPOOL_CLASS
public class WrongMethodTypeException extends RuntimeException {
	
	/**
	 * Serialized version ID
	 */
	private static final long serialVersionUID = 292L;

	/**
	 * Construct a WrongMethodTypeException.
	 */
	public WrongMethodTypeException() {
		super();
	}
	
	/**
	 * Construct a WrongMethodTypeException with the supplied message.
	 *
	 * @param message The message for the exception
	 */
	public WrongMethodTypeException(String message){
		super(message);
	}
	
	static WrongMethodTypeException newWrongMethodTypeException(MethodType oldType, MethodType newType) {
		/*[MSG "K0632", "{0} is not compatible with {1}"]*/
		return new WrongMethodTypeException(com.ibm.oti.util.Msg.getString("K0632", newType, oldType)); //$NON-NLS-1$
	}
	
	WrongMethodTypeException(String message, Throwable throwable) {
		super(message, throwable);
	}
}

