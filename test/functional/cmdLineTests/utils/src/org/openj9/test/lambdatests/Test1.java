/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

package org.openj9.test.lambdatests;

import java.util.Comparator;

/* for 0 as the input, 2 lambda classes are being run to check whether they get stored in the cache,
 * for 1 as the input, only the second lambda class from the previous case is being run to check whether it is still being used from the cache or not even if the index numbers differ,
 * for 2 as the input, 10 lambda classes are being run,
 * for 3 as the input, the lambda classes in the previous case except the first lambda class are being run to check whether the last class is still being used from the cache even
 * though it's index number changed from 10 to 9, which makes a size difference,
 * for 4 as the input, a lambda class is being run, then 2 other lambda classes in another file is being run, then another lambda class is being run back in the current file to check
 * whether we are still doing everything correctly even if there's another file involved.
 */

public class Test1 {

    public static String str1 = new String("testing123");
    
    public static void main(String[] args) {
        
        int input = 0;
        boolean inputError = false;

        if (args.length > 0) {
            try {
                input = Integer.parseInt(args[0]);
            } catch (NumberFormatException localNumberFormatException) {
                inputError = true;
            }
        }
        if ((inputError | input < 0 | input > 4)) {
            System.out.println("Invalid argument");
            input = 0;
        }

        if (input < 2) {
            if (0 == input) {
                Comparator<String> stringComparatorLambda = (String o1, String o2) -> {
                    return o1.compareTo(o2) + o2.compareTo(o1) + 15 + (int)Math.sqrt(122324);
                };
                int lambdaComparison = stringComparatorLambda.compare("world", "hello");
                System.out.println(lambdaComparison);
            }
    		
            Comparator<String> stringComparatorLambda2 = (String o1, String o2) -> {
                return o1.compareTo(o2) + 42 + (int)Math.toRadians(95402.134);
            };
            int lambdaComparison2 = stringComparatorLambda2.compare("qqqsdasfds", "dsfkopvwqp");
            System.out.println(lambdaComparison2);
    	} else if (input < 4) {
            
            int x = 99;
            
            if (2 == input) {
                Comparator<String> stringComparatorLambda = (String o1, String o2) -> {
                    return str1.compareTo(o1) + o1.compareTo(o2) + o2.compareTo(o1) + 15 + x;
                };
                int lambdaComparison = stringComparatorLambda.compare("world", "hello");
                System.out.println(lambdaComparison);
            }
            
            Comparator<String> stringComparatorLambda2 = (String o1, String o2) -> {
                return str1.compareTo(o1)+ o1.compareTo(o2) + o2.compareTo(o1) + 42 + x;
            };
            int lambdaComparison2 = stringComparatorLambda2.compare("qqqsdasfds", "dsfkopvwqp");
            System.out.println(lambdaComparison2);
    		
    		Comparator<String> stringComparatorLambda3 = (String o1, String o2) -> {
                return str1.compareTo(o1) + o1.compareTo(o2) + o2.compareTo(o1) + 15 + x;
            };
            int lambdaComparison3 = stringComparatorLambda3.compare("ojusoqm", "qiwpkd");
            System.out.println(lambdaComparison3);

            Comparator<String> stringComparatorLambda4 = (String o1, String o2) -> {
                return str1.compareTo(o1) + o1.compareTo(o2) + o2.compareTo(o1) + 15 + x;
            };
            int lambdaComparison4 = stringComparatorLambda4.compare("skxonqwi", "mcxkqwj");
            System.out.println(lambdaComparison4);

            Comparator<String> stringComparatorLambda5 = (String o1, String o2) -> {
                return str1.compareTo(o1) + o1.compareTo(o2) + o2.compareTo(o1) + 15 + x;
            };
            int lambdaComparison5 = stringComparatorLambda5.compare("qwjnd", "sdmka");
            System.out.println(lambdaComparison5);

            Comparator<String> stringComparatorLambda6 = (String o1, String o2) -> {
                return str1.compareTo(o1) + o1.compareTo(o2) + o2.compareTo(o1) + 15 + x;
            };
            int lambdaComparison6 = stringComparatorLambda6.compare("fdosofofsdkfe", "pdsfklaspl");
            System.out.println(lambdaComparison6);

            Comparator<String> stringComparatorLambda7 = (String o1, String o2) -> {
                return str1.compareTo(o1) + o1.compareTo(o2) + o2.compareTo(o1) + 15 + x;
            };
            int lambdaComparison7 = stringComparatorLambda7.compare("ksadmnfkas", "sdfjfs");
            System.out.println(lambdaComparison7);

            Comparator<String> stringComparatorLambda8 = (String o1, String o2) -> {
                return str1.compareTo(o1) + o1.compareTo(o2) + o2.compareTo(o1) + 15 + x;
            };
            int lambdaComparison8 = stringComparatorLambda8.compare("sdfjfsc", "zcxmlapq");
            System.out.println(lambdaComparison8);

            Comparator<String> stringComparatorLambda9 = (String o1, String o2) -> {
                return str1.compareTo(o1) + o1.compareTo(o2) + o2.compareTo(o1) + 15 + x;
            };
            int lambdaComparison9 = stringComparatorLambda9.compare("sdkfaq[]", "asjdfnvniww");
            System.out.println(lambdaComparison9);

            int y = 12345;
            Comparator<String> stringComparatorLambda10 = (String o1, String o2) -> {
                return 2 + y + o2.compareTo(o1) + o1.compareTo(str1);
            };
            int lambdaComparison10 = stringComparatorLambda10.compare("###", "!!!");
            System.out.println(lambdaComparison10);
    	} else {
            int x = 99;
            Comparator<String> stringComparatorLambda = (String o1, String o2) -> {
                return str1.compareTo(o1) + o1.compareTo(o2) + o2.compareTo(o1) + 15 + x;
            };
            int lambdaComparison = stringComparatorLambda.compare("world", "hello");
            System.out.println(lambdaComparison);

            Test2 tobj = new Test2();
            tobj.func();
            
            Comparator<String> stringComparatorLambda2 = (String o1, String o2) -> {
                return str1.compareTo(o1) + o1.compareTo(o2) + o2.compareTo(o1) + 42 + x;
            };
            int lambdaComparison2 = stringComparatorLambda2.compare("qqqsdasfds", "dsfkopvwqp");
            System.out.println(lambdaComparison2);
        }
        
        System.out.println("Lambda test done!");
    }
}
