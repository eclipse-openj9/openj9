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
package jit.test.stringconcat;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;

@Test(groups = { "level.sanity","component.jit" })
public class StringConcatTest {
	
	private final String DEFAULT_VALID_PATTERN = "5 of us are going to buy 1 orange each";
	
	private final String DEFAULT_VALID_PATTERN_WITH_NEGATIVE = "-5 is smaller than -1 to a sane person";
	
	private final String DEFAULT_VALID_PATTERN_WITH_NULL1 = "-5 is bigger than 1null to a less than sane person";
	
	private final String DEFAULT_VALID_PATTERN_WITH_NULL2 = "-5 is bigger than -1 to a less than sane person null";
	
	private final String INVALID_PATTERN_ISIS = "1 of us is going to sleep at 2 am";
	
	private final String INVALID_PATTERN_SISS = "I wrote 2 cliches either side of 1 good verse";
	
	private final String INVALID_PATTERN_ISISSS = "1 side of the moon is good enough for 1 earth full of moonstruck ants";
	
	private final String INVALID_PATTERN_ISISSI = "1 side of the moon is good enough for 1 earth full of moonstruck ants but 1";
	
	private final String INVALID_PATTERN_ISISSISS = "1 side of the moon is good enough for 1 earth full of moonstruck ants but 1 disagrees with this";
	
	private final String INVALID_PATTERN_ISISSISC = "1 side of the moon is good enough for 1 earth full of moonstruck ants but 1 disagrees with this";
	
	private final String INVALID_PATTERN_SIS = "I am 1 therefore I am many";
	
	private final String INVALID_PATTERN_SC = "I think therefore I am";
	
	public void testDefaultPattern(){
		String str = buildDefaultStr();
		str = buildDefaultStr();
		AssertJUnit.assertEquals(DEFAULT_VALID_PATTERN, str);
	}
	
	private String buildDefaultStr(){
		return new Integer(5).toString() + " of us are going to buy " + new Integer(1).toString() + " orange " + "each";
	}
		
	public void testDefaultPattern_With_Negative_Integer(){
		String str = buildDefaultPattern_With_Negative_Integer();
		str = buildDefaultPattern_With_Negative_Integer();
		AssertJUnit.assertEquals(DEFAULT_VALID_PATTERN_WITH_NEGATIVE, str);
	}
	
	private String buildDefaultPattern_With_Negative_Integer(){
		return new Integer(-5).toString() + " is smaller than " + new Integer(-1).toString() + " to " + "a sane person";
	}
		
	public void testNullStr_Middle(){
		String str = buildNullStr_Middle();
		str = buildNullStr_Middle();
		AssertJUnit.assertEquals(DEFAULT_VALID_PATTERN_WITH_NULL1, str);
	}
	
	private String buildNullStr_Middle(){
		return new Integer(-5).toString() + " is bigger than " + new Integer(1).toString() + null + " to a less than sane person";
	}
		
	public void testNullStr_End(){
		String str = buildNullStr_End();
		str = buildNullStr_End();
		AssertJUnit.assertEquals(DEFAULT_VALID_PATTERN_WITH_NULL2, str);
	}
	
	private String buildNullStr_End(){
		return new Integer(-5).toString() + " is bigger than " + new Integer(-1).toString() + " to a less than sane person " + null;
	}
		
	public void testAutoboxedInteger(){
		String str = buildAutoboxedIntegerStr();
		str = buildAutoboxedIntegerStr();
		AssertJUnit.assertEquals(DEFAULT_VALID_PATTERN, str);
	}
	
	private String buildAutoboxedIntegerStr(){
		return 5 + " of us are going to buy " + 1 + " orange " + "each";
	}
	
	public void testDefaultPattern_With_Integer_ValueOf(){
		String str = buildDefaultPattern_With_Integer_ValueOf();
		str = buildDefaultPattern_With_Integer_ValueOf();
		AssertJUnit.assertEquals(DEFAULT_VALID_PATTERN, str);
	}
	
	private String buildDefaultPattern_With_Integer_ValueOf(){
		return Integer.valueOf(5) + " of us are going to buy " + Integer.valueOf(1) + " orange " + "each";
	}
	
	public void testInvalidPattern_SISS(){
		String str = buildInvalidPattern_SISS();
		str = buildInvalidPattern_SISS();
		AssertJUnit.assertEquals(INVALID_PATTERN_SISS,str);
	}
	
	private String buildInvalidPattern_SISS(){
		return "I wrote " + new Integer(2).toString() + " cliches either side of " + new Integer(1).toString() + " good verse";
	}
	
	public void testInvalidPattern_ISIS(){
		String str = buildInvalidPattern_ISIS();
		str = buildInvalidPattern_ISIS();
		AssertJUnit.assertEquals(INVALID_PATTERN_ISIS,str);
	}
	
	private String buildInvalidPattern_ISIS(){
		return new Integer(1).toString() + " of us is going to sleep at " + new Integer(2).toString() + " am";
	}
	
	public void testInvalidPattern_ISISISIS(){
		String str = buildInvalidPattern_ISIS();
		str += " " + buildInvalidPattern_ISIS();
		AssertJUnit.assertEquals(INVALID_PATTERN_ISIS + " " +INVALID_PATTERN_ISIS ,str);
	}
	
	 public void testInvalidPattern_ISISSS(){
		String str = buildInvalidPattern_ISISSS();
		str = buildInvalidPattern_ISISSS();
		AssertJUnit.assertEquals(INVALID_PATTERN_ISISSS,str);
	}
	
	private String buildInvalidPattern_ISISSS(){
		return  new Integer(1).toString() + " side of the moon is good enough for " + new Integer(1).toString() + " earth " + "full of " + "moonstruck ants";
	}
	
	public void testInvalidPattern_ISISSI(){
		String str = buildInvalidPattern_ISISSI();
		str = buildInvalidPattern_ISISSI();
		AssertJUnit.assertEquals(INVALID_PATTERN_ISISSI,str);
	}
	
	private String buildInvalidPattern_ISISSI(){
		return  new Integer(1).toString() + " side of the moon is good enough for " + new Integer(1).toString() + " earth " + "full of " + "moonstruck ants but " + new Integer(1);
	}
	
	public void testInvalidPattern_ISISSISS(){
		String str = buildInvalidPattern_ISISSISS();
		str = buildInvalidPattern_ISISSISS();
		AssertJUnit.assertEquals(INVALID_PATTERN_ISISSISS,str);
	}
	
	private String buildInvalidPattern_ISISSISS(){
		return  new Integer(1).toString() + " side of the moon is good enough for " + new Integer(1).toString() + " earth " + "full of " + "moonstruck ants but " + new Integer(1) + " disagrees with this";
	}
	
	public void testInvalidPattern_ISISSISC(){
		String str = buildInvalidPattern_ISISSISC();
		str = buildInvalidPattern_ISISSISC();
		AssertJUnit.assertEquals(INVALID_PATTERN_ISISSISC,str);
	}
	
	private String buildInvalidPattern_ISISSISC(){
		return  new Integer(1).toString() + " side of the moon is good enough for " + new Integer(1).toString() + " earth " + "full of " + "moonstruck ants but " + new Integer(1) + " disagrees with thi" + new Character('s');
	}
	
	public void testConstantString(){
		String str = buildConstantString();
		str = buildConstantString();
		AssertJUnit.assertEquals(DEFAULT_VALID_PATTERN,str);
	}
	
	private String buildConstantString(){
		final String s = new Integer(5).toString() + " of us are going to buy " + new Integer(1).toString() + " orange " + "each";
		return s;
	}
	
	public void testInvalidPattern_SIS_Stress(){
		String str = buildInvalidPattern_SIS_Stressfully(1);
		str = buildInvalidPattern_SIS_Stressfully(1000);
		AssertJUnit.assertEquals(INVALID_PATTERN_SIS,str);
		
	}
	
	private String buildInvalidPattern_SIS_Stressfully(int degree){
		String s = null;
		int c = 0 ; 
		do{
			s = "I am " + new Integer(1).toString() + " therefore I am many";
			c++;
		} while( c < degree );
		return s;
	}
	
	public void testInvalidPattern_SC_Stress(){
		String str = buildInvalidPattern_SC_Stressfully(1);
		str = buildInvalidPattern_SC_Stressfully(1000);
		AssertJUnit.assertEquals(INVALID_PATTERN_SC,str);
		
	}
	
	private String buildInvalidPattern_SC_Stressfully(int degree){
		String s = null;
		int c = 0 ; 
		do{
			s = "I think therefore I a" + new Character('m');
			c++;
		} while( c < degree );
		return s;
	}
	
	public void testInvalidPattern_ISISS_Stress(){
		String str = buildInvalidPattern_ISISS_Stressfully(1);
		str = buildInvalidPattern_ISISS_Stressfully(1000);
		AssertJUnit.assertEquals(DEFAULT_VALID_PATTERN,str);
		
	}
	
	private String buildInvalidPattern_ISISS_Stressfully(int degree){
		String s = null;
		int c = 0 ; 
		do{
			s = new Integer(5).toString() + " of us are going to buy " + new Integer(1).toString() + " orange " + "each";
			c++;
		} while( c < degree );
		return s;
	}
}
