/*
 * Copyright IBM Corp. and others 2019
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package org.openj9.test.lworld;

import jdk.internal.value.ValueClass;
import static org.openj9.test.lworld.ValueTypeTestClasses.*;

public class DDRBackfillLayoutTest {
	public static void main(String[] args) {
		try {
			createAndCheckValueType();
		} catch (Throwable e) {
			e.printStackTrace();
		}
	}

	private static void createAndCheckValueType() throws Throwable {
		// Setup required classes
		ValueTypeTests.testCreateValueInt();
		ValueTypeTests.testCreateValueLong();
		ValueTypeTests.testCreateValueObject();
		ValueTypeTests.testCreatePoint2D();
		ValueTypeTests.testCreateFlattenedLine2D();
		ValueTypeTests.testCreateTriangle2D();
		ValueTypeTests.testCreateLayoutsWithPrimitives();
		ValueTypeTests.testLayoutsWithPrimitives();
		ValueTypeTests.testCreateFlatLayoutsWithValueTypes();
		ValueTypeTests.testFlatLayoutsWithValueTypes();
		ValueTypeTests.testFlatLayoutsWithRecursiveLongs();

		Object flatSingleBackfillInstance =  ValueTypeTests.makeFlatSingleBackfillClass.invoke(ValueTypeTests.makeValueLong.invoke(ValueTypeTests.defaultLong), ValueTypeTests.makeValueObject.invoke(ValueTypeTests.defaultObject), ValueTypeTests.makeValueInt.invoke(ValueTypeTests.defaultInt));
		Object objectBackfillInstance = ValueTypeTests.makeFlatObjectBackfillClass.invoke(ValueTypeTests.makeValueLong.invoke(ValueTypeTests.defaultLong), ValueTypeTests.makeValueObject.invoke(ValueTypeTests.defaultObject));
		Object flatUnAlignedSingleBackfillInstance = ValueTypeTests.makeFlatUnAlignedSingleBackfillClass.invoke(ValueTypeTests.makeValueLong.invoke(ValueTypeTests.defaultLong), ValueTypeTests.makeFlatUnAlignedSingleClass.invoke(ValueTypeTests.makeValueInt.invoke(ValueTypeTests.defaultInt), ValueTypeTests.makeValueInt.invoke(ValueTypeTests.defaultIntNew)), ValueTypeTests.makeValueObject.invoke(ValueTypeTests.defaultObject));
		Object flatUnAlignedSingleBackfill2Instance = ValueTypeTests.makeFlatUnAlignedSingleBackfillClass2.invoke(ValueTypeTests.makeValueLong.invoke(ValueTypeTests.defaultLong), ValueTypeTests.makeFlatUnAlignedSingleClass.invoke(ValueTypeTests.makeValueInt.invoke(ValueTypeTests.defaultInt), ValueTypeTests.makeValueInt.invoke(ValueTypeTests.defaultIntNew)), ValueTypeTests.makeFlatUnAlignedSingleClass.invoke(ValueTypeTests.makeValueInt.invoke(ValueTypeTests.defaultInt), ValueTypeTests.makeValueInt.invoke(ValueTypeTests.defaultIntNew)));
		Object flatUnAlignedObjectBackfillInstance = ValueTypeTests.makeFlatUnAlignedObjectBackfillClass.invoke(ValueTypeTests.makeFlatUnAlignedObjectClass.invoke(ValueTypeTests.makeValueObject.invoke(ValueTypeTests.defaultObject), ValueTypeTests.makeValueObject.invoke(ValueTypeTests.defaultObjectNew)), ValueTypeTests.makeFlatUnAlignedObjectClass.invoke(ValueTypeTests.makeValueObject.invoke(ValueTypeTests.defaultObject), ValueTypeTests.makeValueObject.invoke(ValueTypeTests.defaultObjectNew)), ValueTypeTests.makeValueLong.invoke(ValueTypeTests.defaultLong));
		Object flatUnAlignedObjectBackfill2Instance = ValueTypeTests.makeFlatUnAlignedObjectBackfillClass2.invoke(ValueTypeTests.makeValueObject.invoke(ValueTypeTests.defaultObject), ValueTypeTests.makeFlatUnAlignedObjectClass.invoke(ValueTypeTests.makeValueObject.invoke(ValueTypeTests.defaultObject), ValueTypeTests.makeValueObject.invoke(ValueTypeTests.defaultObjectNew)), ValueTypeTests.makeValueLong.invoke(ValueTypeTests.defaultLong));
		Object singleBackfillInstance = ValueTypeTests.makeSingleBackfillClass.invoke(ValueTypeTests.defaultLong, ValueTypeTests.defaultObject, ValueTypeTests.defaultInt);
		Object objectBackfillInstance2 = ValueTypeTests.makeObjectBackfillClass.invoke(ValueTypeTests.defaultLong, ValueTypeTests.defaultObject);

		ValueTypeDoubleLong doubleLongInstance = new ValueTypeDoubleLong(new ValueTypeLong(ValueTypeTests.defaultLong), ValueTypeTests.defaultLongNew);
		ValueTypeQuadLong quadLongInstance = new ValueTypeQuadLong(doubleLongInstance, new ValueTypeLong(ValueTypeTests.defaultLongNew2), ValueTypeTests.defaultLongNew3);
		ValueTypeDoubleQuadLong doubleQuadLongInstance = new ValueTypeDoubleQuadLong(quadLongInstance, doubleLongInstance, new ValueTypeLong(ValueTypeTests.defaultLongNew4), ValueTypeTests.defaultLongNew5);

		Object[] flatUnAlignedSingleBackfill2Array = ValueClass.newNullRestrictedArray(ValueTypeTests.flatUnAlignedSingleBackfillClass2, 3);
		flatUnAlignedSingleBackfill2Array[1] = flatUnAlignedSingleBackfill2Instance;

		Object[] quadLongArray = ValueClass.newNullRestrictedArray(ValueTypeQuadLong.class, 3);
		quadLongArray[1] = quadLongInstance;

		ValueTypeTests.checkObject(flatSingleBackfillInstance,
				objectBackfillInstance,
				flatUnAlignedSingleBackfillInstance,
				flatUnAlignedSingleBackfill2Instance,
				flatUnAlignedObjectBackfillInstance,
				flatUnAlignedObjectBackfill2Instance,
				singleBackfillInstance,
				objectBackfillInstance2,
				doubleLongInstance,
				quadLongInstance,
				doubleQuadLongInstance,
				flatUnAlignedSingleBackfill2Array,
				quadLongArray);
	}
}
