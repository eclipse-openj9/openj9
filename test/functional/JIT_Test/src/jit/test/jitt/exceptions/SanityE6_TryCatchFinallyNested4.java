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
public class SanityE6_TryCatchFinallyNested4 extends jit.test.jitt.Test {

	private static Logger logger = Logger.getLogger(SanityE6_TryCatchFinallyNested4.class);
	static class BlewIt extends Exception{}

private int tstExceptionNest4(int pi) throws BlewIt {
int mi = 23;
try
 {mi = mi + 5;
  try
   {mi = mi + 45;
	if (pi == 48)
	  throw new BlewIt();
   } 
   catch (BlewIt r) {mi = mi + 19;
	      if (pi == 48) throw new BlewIt();
	     }
   finally {//mi += 200;
	        //if (pi == 48) throw new BlewIt(); 
	       };
   mi += 100;
 }
 catch (BlewIt r) {//mi = mi + 36;// logger.debug("Blew2");
	    //if (pi==48) throw new BlewIt();
	    }
 finally {mi = mi + 46;
	      try {//mi = mi +19;
		       //if (pi == 48)
		       //  throw new BlewIt();
		       //mi = mi + 27;
	      }
	      //catch (BlewIt r) {mi = mi + 16;}
	      catch (Exception e) {mi = mi + 17;}    
	      finally {mi = mi + 57;}
	      
		  }
return mi;
}

	@Test
	public void testSanityE6_TryCatchFinallyNested4() {
	int j=4;
		 try  // cause resolve Special
	       {tstExceptionNest4(48); 
		   }//causes JITed rtn to throw blew it
	        catch (BlewIt bcc ) 
	          {
		       j = 1;
		      };
		 try // force routine to JIT
	       {for (int i = 0; i < 100; i++)   
		      tstExceptionNest4(52);
		   }
	        catch (BlewIt bcc ) 
	          {j = 3;
		      };
		 j = 99;
		 for (int i = 0; i < 5; i++)
		  try
	        {tstExceptionNest4(48);
		    } //throw blewIt
	        catch (BlewIt bcc ) 
	          {j = 0;
		      };    		    
	}
}
