/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2009, 2016 IBM Corp. and others
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

/* CollectHandle is a MethodHandle subclass used to call another MethodHandle.  
 * It accepts the incoming arguments and collects the requested number
 * of them into an array of type 'T'.
 * <p>
 * Return types can NOT be adapted by this handle.
 * <p>
 * Can't pre-allocate the collect array as its not thread-safe - same handle
 * can be used in multiple threads or collected args array can be modified
 * down the call chain.
 */
final class CollectHandle extends MethodHandle {
	@VMCONSTANTPOOL_FIELD
	final MethodHandle next;
	@VMCONSTANTPOOL_FIELD
	final int collectArraySize; /* Size of the collect array */
	@VMCONSTANTPOOL_FIELD
	final int collectPosition; /* The starting position of arguments to collect */
	
	CollectHandle(MethodHandle next, int collectArraySize, int collectPosition) {
		super(collectMethodType(next.type(), collectArraySize, collectPosition), KIND_COLLECT, new int[]{collectArraySize, collectPosition});
		this.collectPosition = collectPosition;
		this.collectArraySize = collectArraySize;
		this.next = next;
	}
	
	CollectHandle(CollectHandle original, MethodType newType) {
		super(original, newType);
		this.collectPosition = original.collectPosition;
		this.collectArraySize = original.collectArraySize;
		this.next = original.next;
	}

	private static final MethodType collectMethodType(MethodType type, int collectArraySize, int collectPosition) {
		int parameterCount = type.parameterCount();
		if (0 == parameterCount) {
			/*[MSG "K05ca", "The argument of MethodType at '{0}' must be an array class"]*/
			throw new IllegalArgumentException(Msg.getString("K05ca", collectPosition)); //$NON-NLS-1$
		}
		// Ensure the class at the specified position is an array
		Class<?> arrayComponent = type.parameterType(collectPosition).getComponentType();
		if (null == arrayComponent) {
			/*[MSG "K05ca", "The argument of MethodType at '{0}' must be an array class"]*/
			throw new IllegalArgumentException(Msg.getString("K05ca", collectPosition)); //$NON-NLS-1$
		}
		// Change the T[] into a 'T'
		MethodType newType = type.changeParameterType(collectPosition, arrayComponent);
		
		// Add necessary additional 'T' to the type
		if (0 == collectArraySize) {
			newType = newType.dropParameterTypes(collectPosition , collectPosition + 1);
		} else if (collectArraySize > 1) {
			Class<?>[] classes = new Class[collectArraySize - 1];
			int arrayLength = classes.length;
			for (int j = 0; j < arrayLength; j++) {
				classes[j] = arrayComponent;
			}
			newType = newType.insertParameterTypes(collectPosition + 1, classes);
		}
		return newType;
	}

	@Override
	MethodHandle cloneWithNewType(MethodType newType) {
		return new CollectHandle(this, newType);
	}

	// {{{ JIT support

	private static final ThunkTable _thunkTable = new ThunkTable();
	protected final ThunkTable thunkTable(){ return _thunkTable; }
	protected final ThunkTuple computeThunks(Object arg) {
		int[] collectArguments = (int[]) arg;
		return thunkTable().get(new ThunkKeyWithIntArray(ThunkKey.computeThunkableType(type()), collectArguments));
	}

	private final Object allocateArray() {
		return java.lang.reflect.Array.newInstance(
			next.type().parameterType(collectPosition).getComponentType(),
			collectArraySize);
	}

	private static native int numArgsToCollect();
	private static native int collectionStart();
	private static native int numArgsAfterCollectArray();

	@FrameIteratorSkip
	private final int invokeExact_thunkArchetype_X(int argPlaceholder) throws Throwable {
		if (ILGenMacros.isShareableThunk()) {
			undoCustomizationLogic(next);
		}
		if (!ILGenMacros.isCustomThunk()) {
			doCustomizationLogic();
		}
		ILGenMacros.populateArray(
			ILGenMacros.push(allocateArray()),
			ILGenMacros.middleN(collectionStart(), numArgsToCollect(), argPlaceholder));
		return ILGenMacros.invokeExact_X(
			next, 
			ILGenMacros.placeholder(
				ILGenMacros.firstN(collectionStart(), argPlaceholder),
				ILGenMacros.pop_L(),
				ILGenMacros.lastN(numArgsAfterCollectArray(), argPlaceholder)));
	}

	// }}} JIT support

	final void compareWith(MethodHandle right, Comparator c) {
		if (right instanceof CollectHandle) {
			((CollectHandle)right).compareWithCollect(this, c);
		} else {
			c.fail();
		}
	}

	final void compareWithCollect(CollectHandle left, Comparator c) {
		c.compareStructuralParameter(left.collectArraySize, this.collectArraySize);
		c.compareStructuralParameter(left.collectPosition, this.collectPosition);
		c.compareChildHandle(left.next, this.next);
	}
}

