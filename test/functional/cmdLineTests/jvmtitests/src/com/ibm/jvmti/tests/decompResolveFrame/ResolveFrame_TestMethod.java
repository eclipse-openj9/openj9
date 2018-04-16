/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package com.ibm.jvmti.tests.decompResolveFrame;

public class ResolveFrame_TestMethod 
{

    public boolean testMethod(double d1, Integer i1, long l1, float f1, float f2, float f3,
                              float f4, float f5, float f6, float f7, float f8, float f9, float f10, float f11, float f12, float f13) 
    {
     
        Integer r_i1 = new Integer(1234);
        double  r_d1 = 2863311530.0;
        long    r_l1 = 1234567;

        float   r_f1 = (float)   1.0000001;
        float   r_f2 = (float)   2.0000002;
        float   r_f3 = (float)   3.0000003;
        float   r_f4 = (float)   4.0000005;
        float   r_f5 = (float)   5.0000006;
        float   r_f6 = (float)   6.0000007;
        float   r_f7 = (float)   7.0000008;
        float   r_f8 = (float)   8.0000009;
        float   r_f9 = (float)   9.0000011;
        float   r_f10 = (float) 10.0000012;
        float   r_f11 = (float) 11.0000013;
        float   r_f12 = (float) 12.0000014;
        float   r_f13 = (float) 13.0000015; 
    	
        
    	if ((r_i1.intValue() != i1.intValue()) ||
    		(r_d1 != d1) || (r_l1 != l1 ) || (r_f1 != f1) || 
    		(r_f2 != f2 ) || (r_f3 != f3) || (r_f4 != f4)  || (r_f5 != f5) || 
    		(r_f6 != f6 ) || (r_f7 != f7) || (r_f8 != f8 ) || (r_f9 != f9) || 
    		(r_f10 != f10) || (r_f11 != f11) || (r_f12 != f12) || (r_f13 != f13)) 
    	{    	
    		System.out.println("\n\nReceived vs Expected " + d1 +  "   " + r_d1 +
    				  		   "\n\t int:    " + i1 +  "   " + r_i1 + 
                                                   "\n\t long:   1 " + l1 + "   " + r_l1 +
                                                   "\n\t float:  1 " + f1 + "   " + r_f1 +
                                                   "\n\t float:  2 " + f2 + "   " + r_f2 +
                                                   "\n\t float:  3 " + f3 + "   " + r_f3 +
                                                   "\n\t float:  4 " + f4 + "   " + r_f4 +
                                                   "\n\t float:  5 " + f5 + "   " + r_f5 +
                                                   "\n\t float:  6 " + f6 + "   " + r_f6 +
                                                   "\n\t float:  7 " + f7 + "   " + r_f7 +
                                                   "\n\t float:  8 " + f8 + "   " + r_f8 +
                                                   "\n\t float:  9 " + f9 + "   " + r_f9 +
                                                   "\n\t float: 10 " + f10 + "   " + r_f10 +
                                                   "\n\t float: 11 " + f11 + "   " + r_f11 +
                                                   "\n\t float: 12 " + f12 + "   " + r_f12 +
                                                   "\n\t float: 13 " + f13 +  "   " + r_f13 +"\n");
    		return false;
    	}
    
    	return true;
   } 
}
