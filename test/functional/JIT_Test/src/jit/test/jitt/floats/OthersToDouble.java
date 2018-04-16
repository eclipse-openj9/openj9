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
public class OthersToDouble extends jit.test.jitt.Test
{
	@Test
	public void testOthersToDouble()
	{
		double value = 0;

		for (int j = 0; j < sJitThreshold; j++)
		{
			value = tstD2D();    
			       if (value != 5.7d)
				       Assert.fail("OthersToDouble->run(): Bad result for test #1");

			value = tstI2D();    
			       if (value != 5.0d)
				       Assert.fail("OthersToDouble->run(): Bad result for test #2");

			value = tstS2D();    
			
			       if (value != 5.0d)
				       Assert.fail("OthersToDouble->run(): Bad result for test #3");

			value = tstB2D();    
			
			       if (value != 5.0d)
				       Assert.fail("OthersToDouble->run(): Bad result for test #4");

			value = tstC2D();    

			       if (value != 306.0d)
				       Assert.fail("OthersToDouble->run(): Bad result for test #5");
	
			value = tstL2D();    
			       if (value != 5.0d)
				       Assert.fail("OthersToDouble->run(): Bad result for test #6");

		}

	}


	private double tstD2D() 
	{	
		double a1 = 9.9d; 
		double d1 = (double) a1;
		
		double a2 = -7.5d;
		double d2 = (double) a2;
		
		double a3 = 0;
		double d3 = (double) a3;
		
		double d4 = (double) 3.3d;
		
		return d1+d2+d3+d4;		
	
	}

	private double tstI2D() 
	{	
		int a1 = 9; 
		double d1 = (double) a1;
		
		int a2 = -7;
		double d2 = (double) a2;
		
		int a3 = 0;
		double d3 = (double) a3;
		
		double d4 = (double) 3;
		
		return d1+d2+d3+d4;		
	}

	private double tstS2D()
	{	
		short a1 = 9;
		double d1 = (double) a1;
		
		short a2 = -7;
		double d2 = (double) a2;
		
		short a3 = 0;
		double d3 = (double) a3;
		
		double d4 = (double) 3;
		
		return (d1+d2+d3+d4);
	
	}

	private double tstB2D()
	{	
		byte a1 = 9; 
		double d1 = (double) a1;
		
		byte a2 = -7;
		double d2 = (double) a2;
		
		byte a3 = 0;
		double d3 = (double) a3;
		
		double d4 = (double) 3;
		
		return (d1+d2+d3+d4);
	
	}

	private double tstC2D()
	{	
		char a1 = 0x01;
		double d1 = (double) a1;
		
		char a2 = 0xfe;
		double d2 = (double) a2;
		
		char a3 = 0x00;
		double d3 = (double) a3;
		
		double d4 = (double) '3';
		
		return (d1+d2+d3+d4);
	
	}

	private double tstL2D()
	{
		long a1 = 9;
		double d1 = (double) a1;
		
		long a2 = -7;
		double d2 = (double) a2;
		
		long a3 = 0;
		double d3 = (double) a3;
		
		double d4 = (double) 3L;
		
		return d1+d2+d3+d4;
	}

}
