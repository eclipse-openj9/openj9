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

import java.lang.reflect.Array;
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
		FlatSingleBackfill! flatSingleBackfillInstance =
			new FlatSingleBackfill(new ValueLong(defaultLong),
			new ValueObject(defaultObject), new ValueInt(defaultInt));
		FlatObjectBackfill! objectBackfillInstance =
			new FlatObjectBackfill(new ValueLong(defaultLong),
			new ValueObject(defaultObject));
		FlatUnAlignedSingleBackfill! flatUnAlignedSingleBackfillInstance =
			new FlatUnAlignedSingleBackfill(new ValueLong(defaultLong),
			new FlatUnAlignedSingle(new ValueInt(defaultInt),
			new ValueInt(defaultIntNew)), new ValueObject(defaultObject));
		FlatUnAlignedSingleBackfill2! flatUnAlignedSingleBackfill2Instance =
			new FlatUnAlignedSingleBackfill2(new ValueLong(defaultLong),
			new FlatUnAlignedSingle(new ValueInt(defaultInt),
			new ValueInt(defaultIntNew)),
			new FlatUnAlignedSingle(new ValueInt(defaultInt),
			new ValueInt(defaultIntNew)));
		FlatUnAlignedObjectBackfill! flatUnAlignedObjectBackfillInstance =
			new FlatUnAlignedObjectBackfill(new FlatUnAlignedObject(
				new ValueObject(defaultObject), new ValueObject(defaultObjectNew)),
				new FlatUnAlignedObject(new ValueObject(defaultObject),
				new ValueObject(defaultObjectNew)), new ValueLong(defaultLong));
		FlatUnAlignedObjectBackfill2! flatUnAlignedObjectBackfill2Instance =
			new FlatUnAlignedObjectBackfill2(new ValueObject(defaultObject),
			new FlatUnAlignedObject(new ValueObject(defaultObject),
			new ValueObject(defaultObjectNew)), new ValueLong(defaultLong));
		SingleBackfill! singleBackfillInstance =
			new SingleBackfill(defaultLong, defaultObject, defaultInt);
		ObjectBackfill! objectBackfillInstance2 =
			new ObjectBackfill(defaultLong, defaultObject);

		ValueTypeDoubleLong doubleLongInstance = new ValueTypeDoubleLong(
			new ValueLong(defaultLong), defaultLongNew);
		ValueTypeQuadLong quadLongInstance = new ValueTypeQuadLong(doubleLongInstance,
			new ValueLong(defaultLongNew2), defaultLongNew3);
		ValueTypeDoubleQuadLong doubleQuadLongInstance =
			new ValueTypeDoubleQuadLong(quadLongInstance, doubleLongInstance,
			new ValueLong(defaultLongNew4), defaultLongNew5);

		FlatUnAlignedSingleBackfill2![] flatUnAlignedSingleBackfill2Array =
			new FlatUnAlignedSingleBackfill2![3];
		flatUnAlignedSingleBackfill2Array[1] = flatUnAlignedSingleBackfill2Instance;

		ValueTypeQuadLong[] quadLongArray = new ValueTypeQuadLong[3];
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
