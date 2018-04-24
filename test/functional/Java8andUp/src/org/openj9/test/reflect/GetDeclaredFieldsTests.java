/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
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
package org.openj9.test.reflect;

import org.openj9.resources.reflect.B;

import java.lang.reflect.Field;
import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;

public class GetDeclaredFieldsTests {

    @Test(groups = { "level.sanity" })
    public void testGetDeclaredFields() {
        try {
            B.class.newInstance();
        } catch (ExceptionInInitializerError e) {
        } catch (Exception e) {
        } finally {
            Field[] declaredFields = null;
            try {
                declaredFields = B.class.getDeclaredFields();
            } catch (Exception e) {
                e.printStackTrace();
                Assert.fail("unexpected exception: " + e);
            }
            AssertJUnit.assertNotNull("declared fields returned null", declaredFields);
            int expectedFieldsListSize = 1;
            int declaredFieldsListSize = declaredFields.length;
            AssertJUnit.assertEquals("wrong declared fields list size", expectedFieldsListSize, declaredFieldsListSize);
            String expectedField = "public int org.openj9.resources.reflect.B.x";
            String actualFieldSignature = declaredFields[0].toString();
            AssertJUnit.assertEquals("wrong signatures for declared fields", expectedField, actualFieldSignature);
        }
    }

}

