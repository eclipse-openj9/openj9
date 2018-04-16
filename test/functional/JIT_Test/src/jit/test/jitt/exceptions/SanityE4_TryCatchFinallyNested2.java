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
public class SanityE4_TryCatchFinallyNested2 extends jit.test.jitt.Test {

	private static Logger logger = Logger.getLogger(SanityE4_TryCatchFinallyNested2.class);
	static class BlewIt extends Exception {}
	static class BooBoo extends Exception {}
	
private int tstExceptionNest2(int pi) throws BlewIt  {
int mi = 23;
try
 {
  try
   {mi = 245;
	if (pi == 548)
	  throw new BlewIt();
   } 
   catch (BlewIt r) {mi = mi + 419; 
	      //if (pi == 548) throw new BooBoo(); till Kevin/Roo gets facility to handle thrown object
	     }
   finally {mi += 6200;
	        if (pi == 548) throw new BooBoo(); 
	       };
   mi += 8100;
 }
 catch (BooBoo r) {mi = mi + 936; 	    //if (pi==48) throw new BlewIt(); till JIT -> interpreted works
	    }
 finally {mi = mi + 1046;
	     }
return mi; 
}
	
	@Test
	public void testSanityE4_TryCatchFinallyNested2() {
		int j=4;
	
		 try  // cause resolve Special
	       {tstExceptionNest2(548); 
		   }//causes JITed rtn to throw blew it
	        catch (BlewIt bcc ) 
	          {
		       j = 1;
		      }
		 try // force routine to JIT
	       {for (int i = 0; i < sJitThreshold; i++)   
		      tstExceptionNest2(52);
		   }
	        catch (BlewIt bcc ) 
	          {j = 3;
		      }
	        //catch (BooBoo bcc)
	         // {j = 2;
		     //  logger.debug("Error Driver(interpreted) caught BooBoo \n");
		     //  break;
		    //  };
		 j = 99;
		 for (int i = 0; i < 5; i++)
		  try
	        {tstExceptionNest2(548);
		    } //throw blewIt
	        catch (BlewIt bcc ) 
	          {j = 0;
			  }
	       // catch (BooBoo bcc)
	        //  {j = 2;
		     //  logger.debug("Error Driver(interpreted) caught BooBoo \n");
		     //  break;
		    //  };
	}

}
