/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2011, 2016 IBM Corp. and others
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

import com.ibm.oti.util.Msg;

final class SpreadHandle extends MethodHandle {
	@VMCONSTANTPOOL_FIELD
 	private final MethodHandle next;
	@VMCONSTANTPOOL_FIELD
	private final Class<?> arrayClass;
	@VMCONSTANTPOOL_FIELD
	private final int spreadCount;
	@VMCONSTANTPOOL_FIELD
	private final int spreadPosition;  /* The starting position of spread arguments */
	
	protected SpreadHandle(MethodHandle next, MethodType collectType, Class<?> arrayClass, int spreadCount, int spreadPosition) {
		super(collectType, KIND_SPREAD, infoAffectingThunks(arrayClass, spreadCount, spreadPosition));
		this.arrayClass = arrayClass;
		this.spreadCount = spreadCount;
		this.spreadPosition = spreadPosition;
		this.next = next;
	}

	SpreadHandle(SpreadHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
		this.next = originalHandle.next;
		this.arrayClass = originalHandle.arrayClass;
		this.spreadCount = originalHandle.spreadCount;
		this.spreadPosition = originalHandle.spreadPosition;
	}

	// {{{ JIT support

	private static final ThunkTable _thunkTable = new ThunkTable();
	protected final ThunkTable thunkTable(){ return _thunkTable; }

	private static Object[] infoAffectingThunks(Class<?> arrayClass, int spreadCount, int spreadPosition) {
		// The number and types of values to spread affect jitted thunks
		Object[] result = { arrayClass, spreadCount, spreadPosition};
		return result;
	}

	protected final ThunkTuple computeThunks(Object info) {
		return thunkTable().get(new ThunkKeyWithObjectArray(ThunkKey.computeThunkableType(type()), (Object[])info));
	}

	private static native int numArgsToPassThrough();
	private static native int numArgsToSpread();
	private static native int spreadStart();
	private static native int numArgsAfterSpreadArray();
	private static native Object arrayArg(int argPlaceholder);

	// Can't expand placeholders in calls to virtual methods, so here's a static method that calls Class.cast.
	private static void checkCast(Class<?> c, Object q) {
		c.cast(q);
	}

	private static void checkArray(Object spreadArg, int spreadCount) throws IllegalArgumentException, ArrayIndexOutOfBoundsException {
		if (spreadArg == null) {
			if (spreadCount != 0) {
				/*[MSG "K05d1", "cannot have null spread argument unless spreadCount is 0"]*/
				throw new IllegalArgumentException(Msg.getString("K05d1")); //$NON-NLS-1$
			}
		} else if (spreadCount != java.lang.reflect.Array.getLength(spreadArg)) {
			/*[MSG "K05d2", "expected '{0}' sized array; encountered '{1}' sized array"]*/
			throw new IllegalArgumentException(Msg.getString("K05d2", spreadCount, java.lang.reflect.Array.getLength(spreadArg))); //$NON-NLS-1$
		}
	}

	@FrameIteratorSkip
	private final int invokeExact_thunkArchetype_X(int argPlaceholder) {
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(next);
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		checkCast(arrayClass, arrayArg(argPlaceholder));
		checkArray(arrayArg(argPlaceholder), spreadCount);
		return ILGenMacros.invokeExact_X(next, ILGenMacros.placeholder(
			ILGenMacros.firstN(spreadStart(), argPlaceholder),
			ILGenMacros.arrayElements(arrayArg(argPlaceholder), 0, numArgsToSpread()),
			ILGenMacros.lastN(numArgsAfterSpreadArray(), argPlaceholder))
			);
	}

	// }}} JIT support

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new SpreadHandle(this, newType);
	}

	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof SpreadHandle) {
			((SpreadHandle)right).compareWithSpread(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithSpread(SpreadHandle left, Comparator c) {
		c.compareStructuralParameter(left.arrayClass, this.arrayClass);
		c.compareStructuralParameter(left.spreadCount, this.spreadCount);
		c.compareStructuralParameter(left.spreadPosition, this.spreadPosition);
		c.compareChildHandle(left.next, this.next);
	}

}

