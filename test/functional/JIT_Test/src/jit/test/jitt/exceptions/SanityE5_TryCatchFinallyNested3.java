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
package jit.test.jitt.exceptions;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity","component.jit" })
public class SanityE5_TryCatchFinallyNested3 extends jit.test.jitt.Test {

	private static Logger logger = Logger.getLogger(SanityE5_TryCatchFinallyNested3.class);
	static class BlewIt extends Exception{}

private int tstExceptionNest3(int pi) throws BlewIt {
int mi = 323;
try
 {mi = mi + 45;
  try
   {mi = mi + 545;
	if (pi == 648)
	  throw new BlewIt();
   } 
   catch (BlewIt r) {mi = mi + 1019; logger.debug("Blew");
	      if (pi == 648) throw new BlewIt();
	     }
   finally {mi += 7200;};
   mi += 8100;
 }
 catch (BlewIt r) {mi = mi + 936; logger.debug("Blew2");
	    //if (pi==648) throw new BlewIt(); till JIT ->interpreted works
	    }
 finally {mi = mi + 246;
	      try {mi = mi +319;
		       if (pi == 648)
		         throw new BlewIt();
		       mi = mi + 427;
	      }
	      catch (BlewIt r) {mi = mi + 516;}
	      catch (Exception e) {mi = mi + 617;}    
	      finally {mi = mi + 757;}
	      		  }
return mi;
}


	@Test
	public void testSanityE5_TryCatchFinallyNested3() {
		int j=4;
		 try  // cause resolve Special
	       {tstExceptionNest3(648); 
		   }//causes JITed rtn to throw blew it
	        catch (BlewIt bcc ) 
	          {
		       j = 1;
		      };
		 try // force routine to JIT
	       {for (int i = 0; i < sJitThreshold; i++)   
		      tstExceptionNest3(52);
		   }
	        catch (BlewIt bcc ) 
	          {j = 3;
		      };
		 j = 99;
		 for (int i = 0; i < 5; i++)
		  try
	        {
	        tstExceptionNest3(648);
		    } //throw blewIt
	        catch (BlewIt bcc ) 
	          {j = 0;
		      };    			  
	
	}
}
