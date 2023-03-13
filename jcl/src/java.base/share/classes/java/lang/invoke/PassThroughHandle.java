/*[INCLUDE-IF Sidecar17 & !OPENJDK_METHODHANDLES]*/
/*******************************************************************************
 * Copyright IBM Corp. and others 2009
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package java.lang.invoke;

/*[IF JAVA_SPEC_VERSION >= 15]*/
import java.util.List;
/*[ENDIF] JAVA_SPEC_VERSION >= 15 */

/*
 * Special handle that in the interpreter calls the equivalent's implementation.
 * Provides ability to have JIT specific subtypes without requiring individual
 * interpreter targets.
 */
abstract class PassThroughHandle extends MethodHandle {
	final MethodHandle equivalent;

	/**
	 * Create a new PassThroughHandle that will call 'equivalent' MethodHandle
	 * when invoked in the interpreter.
	 * 
	 * @param equivalent the equivalent methodhandle, usually using the collect-operate-spread pattern
	 * @param name subclass name
	 * @param thunkArg extra thunkArg used in computeThunks.  
	 */
	PassThroughHandle(MethodHandle equivalent, Object thunkArg) {
		super(equivalent.type, KIND_PASSTHROUGH, thunkArg);
		this.equivalent = equivalent;
	}
	
	/**
	 * Helper constructor.  Calls {@link #PassThroughHandle(MethodHandle, Object)} with null thunkArg
	 */
	PassThroughHandle(MethodHandle equivalent) {
		this(equivalent, null);
	}

	/**
	 * Helper constructor for cloneWithNewType, copys relevant fields to the new object
	 */
	PassThroughHandle(PassThroughHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
		this.equivalent = originalHandle.equivalent;
	}
	
/*[IF JAVA_SPEC_VERSION >= 15]*/
	@Override
	boolean addRelatedMHs(List<MethodHandle> relatedMHs) {
		relatedMHs.add(equivalent);
		return true;
	}
/*[ENDIF] JAVA_SPEC_VERSION >= 15 */

}
