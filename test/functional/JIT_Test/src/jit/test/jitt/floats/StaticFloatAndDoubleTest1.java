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
public class StaticFloatAndDoubleTest1 extends jit.test.jitt.Test
{
	@Test
	public void testStaticFloatAndDoubleTest1()
	{
		float value = 0;

		for (int j = 0; j < sJitThreshold; j++)
		{
			value = tstStatic1();   
			        if (value != 9.0f)
				        Assert.fail("StaticFloatAndDoubleTest1->run(): Bad result for test #1");

			value = tstI2F();   
			        if (value != -7.0f)
				        Assert.fail("StaticFloatAndDoubleTest1->run(): Bad result for test #2");

			value = tstS2F();   
			
			        if (value != -7.0f)
				        Assert.fail("StaticFloatAndDoubleTest1->run(): Bad result for test #3");

			value = tstB2F();   
			
			        if (value != 0.0f)
				        Assert.fail("StaticFloatAndDoubleTest1->run(): Bad result for test #4");

			value = tstC2F();   

			        if (value != 254.0f)
				        Assert.fail("StaticFloatAndDoubleTest1->run(): Bad result for test #5");
	
			value = tstL2F();   
			        if (value != 3.0f)
				        Assert.fail("StaticFloatAndDoubleTest1->run(): Bad result for test #6");

		}

	}


	private float tstStatic1() 
	{	
		double a1 = 9.0d; 
		float f1 = (float) a1;
		
		return f1;		
	}

	private float tstI2F() 
	{	
		
		int a2 = -7;
		float f2 = (float) a2;
				
		return f2;		
	}

	private float tstS2F()
	{			
		short a2 = -7;
		float f2 = (float) a2;
				
		return f2;
	
	}

	private float tstB2F()
	{			
		byte a3 = 0;
		float f3 = (float) a3;
				
		return f3;
	
	}

	private float tstC2F()
	{			
		char a2 = 0xfe;
		float f2 = (float) a2;
		
		return f2;
	
	}

	private float tstL2F()
	{		
		float f4 = (float) 3L;
		
		return f4;
	}

}
