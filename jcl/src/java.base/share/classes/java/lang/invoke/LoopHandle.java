/*[INCLUDE-IF Sidecar19-SE]*/
/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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

final class LoopHandle extends PassThroughHandle {
	private final MethodHandle[][] handleClauses;
	
	protected LoopHandle(MethodHandle equivalent, MethodHandle[][] handleClauses) {
		super(equivalent, infoAffectingThunks(handleClauses[0][0].type.parameterCount()));
		this.handleClauses = handleClauses;
	}

	LoopHandle(LoopHandle loopHandle, MethodType newType) {
		super(loopHandle, newType);
		this.handleClauses = loopHandle.handleClauses;
	}

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new LoopHandle(this, newType);
	}

	public static LoopHandle get(MethodHandle equivalent, MethodHandle[][] handleClauses) {
		return new LoopHandle(equivalent, handleClauses);
	}

	// {{{ JIT support
	private static final ThunkTable _thunkTable = new ThunkTable();
	@Override
	protected final ThunkTable thunkTable(){ return _thunkTable; }

	
	@SuppressWarnings("boxing")
	private static Object infoAffectingThunks(int numLoopTargetArgs) {
		// The number of arguments passed to the loop target affects the code generated in the thunks
		return numLoopTargetArgs;
	}

	@Override
	@SuppressWarnings("boxing")
	protected final ThunkTuple computeThunks(Object arg) {
		int numLoopTargetArgs = (Integer)arg;
		return thunkTable().get(new ThunkKeyWithInt(ThunkKey.computeThunkableType(type()), numLoopTargetArgs));
	}
	
	/*
	 * The following method is not implemented by JIT team for now
	@FrameIteratorSkip
	private final int invokeExact_thunkArchetype_X(int argPlaceholder) {
	}
	*/
	
	// }}} JIT support

	@Override
	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof LoopHandle) {
			((LoopHandle)right).compareWithLoop(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithLoop(LoopHandle left, Comparator c) {
		c.compareStructuralParameter(left.handleClauses, this.handleClauses);
		
		int countOfClause = this.handleClauses.length;
		for (int clauseIndex = 0; clauseIndex < countOfClause; clauseIndex++) {
			MethodHandle[] leftCurrentClause = left.handleClauses[clauseIndex];
			MethodHandle[] thisCurrentClause = this.handleClauses[clauseIndex];
			for (int handleIndex = 0; handleIndex < 4; handleIndex++) {
				c.compareChildHandle(leftCurrentClause[handleIndex], thisCurrentClause[handleIndex]);
			}
		}
	}
}
