/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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
package org.openj9.test.lworld;

import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;
import java.lang.reflect.Array;

public class DDRValueTypeTest {
    public static void main(String[] args) {
        try {
            createAndCheckValueType();
        } catch (Throwable e) {
            System.out.println("Exception caught!");
            e.printStackTrace();
        }
    }

    private static void createAndCheckValueType() throws Throwable {
        // Setup required classes
        ValueTypeTests.testCreateValueInt();
        ValueTypeTests.testCreateValueFloat();
        ValueTypeTests.testCreatePoint2D();
        ValueTypeTests.testCreateFlattenedLine2D();
        ValueTypeTests.testCreateTriangle2D();
        
        // Create value types and check object
        Class assortedValueWithSingleAlignmentClass = ValueTypeGenerator.generateValueClass("AssortedValueWithSingleAlignment", ValueTypeTests.typeWithSingleAlignmentFields);

        MethodHandle makeAssortedValueWithSingleAlignment = MethodHandles.lookup().findStatic(assortedValueWithSingleAlignmentClass,
            "makeValueGeneric", MethodType.methodType(Object.class, Object.class,
                        Object.class, Object.class, Object.class, Object.class, Object.class));

        Object[] altFields = {
            ValueTypeTests.defaultTrianglePositionsNew, 
            ValueTypeTests.defaultPointPositions2, 
            ValueTypeTests.defaultLinePositions2, 
            ValueTypeTests.defaultIntNew, 
            ValueTypeTests.defaultFloatNew, 
            ValueTypeTests.defaultTrianglePositionsNew
        };
        Object assortedValueWithSingleAlignment = ValueTypeTests.createAssorted(makeAssortedValueWithSingleAlignment, ValueTypeTests.typeWithSingleAlignmentFields);
        Object assortedValueWithSingleAlignmentAlt = ValueTypeTests.createAssorted(makeAssortedValueWithSingleAlignment, ValueTypeTests.typeWithSingleAlignmentFields, altFields);
    
        Object valArray = Array.newInstance(assortedValueWithSingleAlignmentClass, 2);
        Array.set(valArray, 0, assortedValueWithSingleAlignment);
        Array.set(valArray, 1, assortedValueWithSingleAlignmentAlt);

        ValueTypeTests.checkObject(assortedValueWithSingleAlignment, assortedValueWithSingleAlignmentAlt, valArray);
    }
}
