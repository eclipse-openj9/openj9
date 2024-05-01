/*
 * Copyright IBM Corp. and others 2022
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

package jit.test.recognizedMethod;
import org.testng.AssertJUnit;
import org.testng.annotations.Test;

public class TestJavaIntegerAndLongToString {

    @Test(groups = {"level.sanity"}, invocationCount=2)
    public void test_java_lang_Integer_toString() {
	// Base case tests
	AssertJUnit.assertTrue(Integer.toString(0).equals("0"));
	AssertJUnit.assertTrue(Integer.toString(1).equals("1"));
	AssertJUnit.assertTrue(Integer.toString(-1).equals("-1"));

	// Integer.Max tests
	AssertJUnit.assertTrue(Integer.toString(2).equals("2"));
	AssertJUnit.assertTrue(Integer.toString(21).equals("21"));
	AssertJUnit.assertTrue(Integer.toString(214).equals("214"));
	AssertJUnit.assertTrue(Integer.toString(2147).equals("2147"));
	AssertJUnit.assertTrue(Integer.toString(21474).equals("21474"));
	AssertJUnit.assertTrue(Integer.toString(214748).equals("214748"));
	AssertJUnit.assertTrue(Integer.toString(2147483).equals("2147483"));
	AssertJUnit.assertTrue(Integer.toString(21474836).equals("21474836"));
	AssertJUnit.assertTrue(Integer.toString(214748364).equals("214748364"));
	AssertJUnit.assertTrue(Integer.toString(2147483647).equals("2147483647")); // INT.MAX

	//Integer.Min tests
	AssertJUnit.assertTrue(Integer.toString(-2).equals("-2"));
	AssertJUnit.assertTrue(Integer.toString(-21).equals("-21"));
	AssertJUnit.assertTrue(Integer.toString(-214).equals("-214"));
	AssertJUnit.assertTrue(Integer.toString(-2147).equals("-2147"));
	AssertJUnit.assertTrue(Integer.toString(-21474).equals("-21474"));
	AssertJUnit.assertTrue(Integer.toString(-214748).equals("-214748"));
	AssertJUnit.assertTrue(Integer.toString(-2147483).equals("-2147483"));
	AssertJUnit.assertTrue(Integer.toString(-21474836).equals("-21474836"));
	AssertJUnit.assertTrue(Integer.toString(-214748364).equals("-214748364"));
	AssertJUnit.assertTrue(Integer.toString(-2147483648).equals("-2147483648")); // INT.MIN
    }

    public void test_java_lang_Long_toString() {
	    // Base case tests
	    AssertJUnit.assertTrue(Long.toString(0).equals("0"));
	    AssertJUnit.assertTrue(Long.toString(1).equals("1"));
	    AssertJUnit.assertTrue(Long.toString(-1).equals("-1"));

	    // Long.Max tests 9,223,372,036,854,775,807
	    AssertJUnit.assertTrue(Long.toString(9L).equals("9"));
	    AssertJUnit.assertTrue(Long.toString(92L).equals("92"));
	    AssertJUnit.assertTrue(Long.toString(922L).equals("922"));
	    AssertJUnit.assertTrue(Long.toString(9223L).equals("9223"));
	    AssertJUnit.assertTrue(Long.toString(92233L).equals("92233"));
	    AssertJUnit.assertTrue(Long.toString(922337L).equals("922337"));
	    AssertJUnit.assertTrue(Long.toString(9223372L).equals("9223372"));
	    AssertJUnit.assertTrue(Long.toString(92233720L).equals("92233720"));
	    AssertJUnit.assertTrue(Long.toString(922337203L).equals("922337203"));
	    AssertJUnit.assertTrue(Long.toString(9223372036L).equals("9223372036"));
	    AssertJUnit.assertTrue(Long.toString(92233720368L).equals("92233720368"));
	    AssertJUnit.assertTrue(Long.toString(922337203685L).equals("922337203685"));
	    AssertJUnit.assertTrue(Long.toString(9223372036854L).equals("9223372036854"));
	    AssertJUnit.assertTrue(Long.toString(92233720368547L).equals("92233720368547"));
	    AssertJUnit.assertTrue(Long.toString(922337203685477L).equals("922337203685477"));
	    AssertJUnit.assertTrue(Long.toString(9223372036854775L).equals("9223372036854775"));
	    AssertJUnit.assertTrue(Long.toString(92233720368547758L).equals("92233720368547758"));
	    AssertJUnit.assertTrue(Long.toString(922337203685477580L).equals("922337203685477580"));
	    AssertJUnit.assertTrue(Long.toString(9223372036854775807L).equals("9223372036854775807"));

	    //Long.Min tests
	    AssertJUnit.assertTrue(Long.toString(-9L).equals("-9"));
	    AssertJUnit.assertTrue(Long.toString(-92L).equals("-92"));
	    AssertJUnit.assertTrue(Long.toString(-922L).equals("-922"));
	    AssertJUnit.assertTrue(Long.toString(-9223L).equals("-9223"));
	    AssertJUnit.assertTrue(Long.toString(-92233L).equals("-92233"));
	    AssertJUnit.assertTrue(Long.toString(-922337L).equals("-922337"));
	    AssertJUnit.assertTrue(Long.toString(-9223372L).equals("-9223372"));
	    AssertJUnit.assertTrue(Long.toString(-92233720L).equals("-92233720"));
	    AssertJUnit.assertTrue(Long.toString(-922337203L).equals("-922337203"));
	    AssertJUnit.assertTrue(Long.toString(-9223372036L).equals("-9223372036"));
	    AssertJUnit.assertTrue(Long.toString(-92233720368L).equals("-92233720368"));
	    AssertJUnit.assertTrue(Long.toString(-922337203685L).equals("-922337203685"));
	    AssertJUnit.assertTrue(Long.toString(-9223372036854L).equals("-9223372036854"));
	    AssertJUnit.assertTrue(Long.toString(-92233720368547L).equals("-92233720368547"));
	    AssertJUnit.assertTrue(Long.toString(-922337203685477L).equals("-922337203685477"));
	    AssertJUnit.assertTrue(Long.toString(-9223372036854775L).equals("-9223372036854775"));
	    AssertJUnit.assertTrue(Long.toString(-92233720368547758L).equals("-92233720368547758"));
	    AssertJUnit.assertTrue(Long.toString(-922337203685477580L).equals("-922337203685477580"));
	    AssertJUnit.assertTrue(Long.toString(-9223372036854775808L).equals("-9223372036854775808"));
    }
}
