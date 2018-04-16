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
package jit.test.jitt.floats;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class OthersToFloat extends jit.test.jitt.Test
{
	@Test
	public void testOthersToFloat()
	{
		float value = 0;

		for (int j = 0; j < sJitThreshold; j++)
		{
			value = tstD2F();
			    if (value != 5.7f)
				    Assert.fail("OthersToFloat->run(): Bad result for test #1");

			value = tstI2F();
			    if (value != 5.0f)
				    Assert.fail("OthersToFloat->run(): Bad result for test #2");

			value = tstS2F();
			
			    if (value != 5.0f)
				    Assert.fail("OthersToFloat->run(): Bad result for test #3");

			value = tstB2F();
			
			    if (value != 5.0f)
				    Assert.fail("OthersToFloat->run(): Bad result for test #4");

			value = tstC2F();

			    if (value != 306.0f)
				    Assert.fail("OthersToFloat->run(): Bad result for test #5");
	
			value = tstL2F();
			    if (value != 5.0f)
				    Assert.fail("OthersToFloat->run(): Bad result for test #6");

		}

	}


	private float tstD2F() 
	{	
		double a1 = 9.9d; 
		float f1 = (float) a1;
		
		double a2 = -7.5d;
		float f2 = (float) a2;
		
		double a3 = 0;
		float f3 = (float) a3;
		
		float f4 = (float) 3.3d;
		
		return f1+f2+f3+f4;		
	
	}

	private float tstI2F() 
	{	
		int a1 = 9; 
		float f1 = (float) a1;
		
		int a2 = -7;
		float f2 = (float) a2;
		
		int a3 = 0;
		float f3 = (float) a3;
		
		float f4 = (float) 3;
		
		return f1+f2+f3+f4;		
	}

	private float tstS2F()
	{	
		short a1 = 9;
		float f1 = (float) a1;
		
		short a2 = -7;
		float f2 = (float) a2;
		
		short a3 = 0;
		float f3 = (float) a3;
		
		float f4 = (float) 3;
		
		return (f1+f2+f3+f4);
	
	}

	private float tstB2F()
	{	
		byte a1 = 9; 
		float f1 = (float) a1;
		
		byte a2 = -7;
		float f2 = (float) a2;
		
		byte a3 = 0;
		float f3 = (float) a3;
		
		float f4 = (float) 3;
		
		return (f1+f2+f3+f4);
	
	}

	private float tstC2F()
	{	
		char a1 = 0x01;
		float f1 = (float) a1;
		
		char a2 = 0xfe;
		float f2 = (float) a2;
		
		char a3 = 0x00;
		float f3 = (float) a3;
		
		float f4 = (float) '3';
		
		return (f1+f2+f3+f4);
	
	}

	private float tstL2F()
	{
		long a1 = 9;
		float f1 = (float) a1;
		
		long a2 = -7;
		float f2 = (float) a2;
		
		long a3 = 0;
		float f3 = (float) a3;
		
		float f4 = (float) 3L;
		
		return f1+f2+f3+f4;
	}

}
