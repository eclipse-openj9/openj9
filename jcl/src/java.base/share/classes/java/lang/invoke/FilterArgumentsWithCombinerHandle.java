/*[INCLUDE-IF Java12]*/
/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be madpe available under the following
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

/* 
 * Pseudocode example:
 * handle: D original(A, B, C)
 * filterPosition: 1
 * preprocessor:  B filter(B, A)
 * argumentIndices: {1, 0}
 * 
 * resulting handle:
 * D result(A a, B b, C c) {
 * 	B e = filter(b, a)
 * 	return original(a, e, c)
 * }
 */
final class FilterArgumentsWithCombinerHandle extends MethodHandle {
    protected final MethodHandle next;
    protected final MethodHandle combiner;
    private final int filterPosition;
    private final int[] argumentIndices;

    FilterArgumentsWithCombinerHandle(MethodHandle next, int filterPosition, MethodHandle combiner, int... argumentIndices) {
        super(next.type, KIND_FILTERARGUMENTS_WITHCOMBINER, infoAffectingThunks(combiner.type(), filterPosition, argumentIndices));
        this.next = next;
        this.combiner = combiner;
        this.filterPosition = filterPosition;
        this.argumentIndices = argumentIndices;
    }

    FilterArgumentsWithCombinerHandle(FilterArgumentsWithCombinerHandle originalHandle, MethodType newType) {
        super(originalHandle, newType);
        this.next = originalHandle.next;
        this.combiner = originalHandle.combiner;
        this.filterPosition = originalHandle.filterPosition;
        this.argumentIndices = originalHandle.argumentIndices;
    }

    static FilterArgumentsWithCombinerHandle get(MethodHandle next, int filterPosition, MethodHandle combiner, int... argumentIndices) {
        return new FilterArgumentsWithCombinerHandle(next, filterPosition, combiner, argumentIndices);
    }

    @Override
    MethodHandle cloneWithNewType(MethodType newType) {
        return new FilterArgumentsWithCombinerHandle(this, newType);
    }

    private static Object[] infoAffectingThunks(MethodType combinerType, int filterPosition, int...argumentIndices) {
        MethodType thunkableType = ThunkKey.computeThunkableType(combinerType);
        Object[] result = {thunkableType, filterPosition, argumentIndices};
        return result;
    }

    private static final ThunkTable _thunkTable = new ThunkTable();
    @Override
    protected ThunkTable thunkTable(){ return _thunkTable; }
    
    @Override
    protected final ThunkTuple computeThunks(Object info) {
        return thunkTable().get(new ThunkKeyWithObjectArray(ThunkKey.computeThunkableType(type()), (Object[])info));
    }

    private static native int filterPosition();
    private static native int argumentIndices();
    /* create placeholder for combiner based on argumentIndices the original handle's arguments */
    private static native int argumentsForCombiner(int indice, int argPlaceholder);
    /* number of arguments remaining after the filtered argument */
    private static native int numSuffixArgs();

    @FrameIteratorSkip
    private final int invokeExact_thunkArchetype_X(int argPlaceholder) {
        if (ILGenMacros.isShareableThunk()) {
            undoCustomizationLogic(combiner, next);
        }
        if (!ILGenMacros.isCustomThunk()) {
            doCustomizationLogic();
        }

        return ILGenMacros.invokeExact_X(next, ILGenMacros.placeholder(
            ILGenMacros.firstN(filterPosition(), argPlaceholder), 
            ILGenMacros.invokeExact(combiner, argumentsForCombiner(argumentIndices(), argPlaceholder)),
            ILGenMacros.lastN(numSuffixArgs(), argPlaceholder)));
    }

    @Override
    final void compareWith(MethodHandle right, Comparator c) {
        if (right instanceof FilterArgumentsWithCombinerHandle) {
            ((FilterArgumentsWithCombinerHandle)right).compareWithFilterArgumentsWithCombiner(this, c);
        } else {
            c.fail();
        }
    }
    
    final void compareWithFilterArgumentsWithCombiner(FilterArgumentsWithCombinerHandle left, Comparator c) {
        c.compareChildHandle(left.next, this.next);
        c.compareChildHandle(left.combiner, this.combiner);
        c.compareStructuralParameter(left.filterPosition, this.filterPosition);
        c.compareStructuralParameter(left.argumentIndices, this.argumentIndices);
    }
}
