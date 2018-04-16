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
package jit.test.jitt.misc;

import org.testng.log4testng.Logger;

public class S {
private static Logger logger = Logger.getLogger(S.class);
/**
 * S constructor comment.
 */
public S() {
	super();
}
static int getIn() {
	int index = 0;
	int numin = 0;
	byte inputBytes[] = {9,9,9,9,9};
	try {
		numin = System.in.read(inputBytes,0,4);
		
	} catch (java.io.IOException e) {
		numin = 0;
	}
	if (numin == 3)
	  	index = (int)inputBytes[0];
	  else
	  	index = (int)inputBytes[0]*256 + (int)inputBytes[1];
	//logger.debug("Digits Received=" + numin + "\n" + "Test Index=" + index + "\n");
	return index;
}
void newMethod() {
}
static void printString(String inString) {
//	logger.debug(inString);
}
void printStringV(String inString) {
//	logger.debug(inString);
}
public static int tstArithValidatorIntSMeth(int expectedResult,int actualResult,int location,int rtnValue)
 {int i,j;
  i = tstHide(1); //=1
  j = tstHide(0); //=0
  if (expectedResult != actualResult)
	{//eventually print out here
	// logger.error("Failed Validation Code Is " + location + "--expectedResult-- " + expectedResult + "--actualResult-- " + actualResult);
	 return i/j;
	};
  return rtnValue;
 } 
public static int tstHide(int i)
	 
 {return i;
 } 
public static boolean tstJited(int i)
{
 if (i > 2500) return true;
 return false;
}
public static long tstLArithValidatorIntSMeth(long expectedResult,long actualResult,int location,long rtnValue)
 {long i,j;
  i = tstLHide(1l); //=1
  j = tstLHide(0l); //=0
  if (expectedResult != actualResult)
	{//eventually print out here
	// logger.error("Failed Validation Code Is " location) + "--expectedResult-- " + expectedResult + "--actualResult-- " + actualResult);
	 return i/j;
	};
  return rtnValue;
 } 
public static long tstLHide(long i)
	 
 {return i;
 } 
}
