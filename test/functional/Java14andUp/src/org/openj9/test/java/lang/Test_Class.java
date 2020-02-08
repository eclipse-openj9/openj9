package org.openj9.test.java.lang;

/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

import org.testng.AssertJUnit;

/*
 * Test JCL additions to java.lang.Class from JEP 359: Records preview.
 * 
 * New methods include:
 * - boolean isRecord()
 */

 @Test(groups = { "level.sanity" })
 public class Test_Class {
    private static final Logger logger = Logger.getLogger(Test_Class.class);

    /* Test classes and records */
    class TestNotARecord {}
    record TestRecordEmpty() {}

    @Test
    public void test_isRecord() {
        AssertJUnit.assertTrue("TestRecordEmpty is a record", TestRecordEmpty.class.isRecord());
        AssertJUnit.assertTrue("TestNotARecord is not a record", !TestNotARecord.class.isRecord());
    }
 }
 