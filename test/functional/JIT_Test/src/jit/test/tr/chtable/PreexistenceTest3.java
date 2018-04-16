/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
package jit.test.tr.chtable;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import java.lang.reflect.*;
import org.testng.log4testng.Logger;

// import some good class hierarchies
import java.io.*;
import java.util.*;
import java.util.zip.*;

@Test(groups = { "level.sanity","component.jit" })
public class PreexistenceTest3 {

   private static Logger logger = Logger.getLogger(PreexistenceTest3.class);
   //ctor
      
   static final class LongObjectIterator implements Iterator {
      private long cursor;
      private long end;
		
      public LongObjectIterator(long start, long end) {
         this.cursor = start;
	 this.end = end;
	 }
		
      public boolean hasNext() {
         return end > cursor;
	 }
		
      public Object next() {
         return new Long(cursor++);
	 }

      public void remove() {
	 throw new UnsupportedOperationException();
	 }
      }


	static final class LongWrapper {
		private long value;

		public LongWrapper(long value) {
			this.value = value;
		}

		public Long getValue() {
			return new Long(value);
		}
	}

    static final class LongWrapperIterator implements Iterator {
       private long cursor;
       private long end;
		
       public LongWrapperIterator(long start, long end) {
          this.cursor = start+100;
	  this.end = end+100;
	  }
		
       public boolean hasNext() {
          return end > cursor;
	  }
		
       public Object next() {
          return new LongWrapper(cursor++);
	  }

       public void remove() {
          throw new UnsupportedOperationException();
	  }
       }

	@Test
	public void driver() {
           long start = 0; //Long.parseLong(args[0]);
	   long end = 1000; //Long.parseLong(args[1]);
		
           boolean result = false;
	   for (int i = 0; i < 30; i++) {
              logger.debug("Iteration " + i);
	      result = bench(start, end);
              if (!result)
                 break;
	      }
 
        AssertJUnit.assertTrue(result);
	}

	private boolean bench(long start, long end) {

		System.gc();
		//time = System.currentTimeMillis();
		long result1 = test2(new LongObjectIterator(start, end));
		//logger.debug("\tLongObjectIterator: " + (System.currentTimeMillis() - time));
                
		System.gc();
		//time = System.currentTimeMillis();
		test3(new LongWrapperIterator(start, end));
		//logger.debug("\tLongWrapperIterator: " + (System.currentTimeMillis() - time));

        	System.gc();
		//time = System.currentTimeMillis();
		long result2 = test2(new LongObjectIterator(start, end));
		//logger.debug("\tLongObjectIterator: " + (System.currentTimeMillis() - time));
                if (result2 == result1)
                   return true;
                return false;
	}
	
	
	/*
	 * Test the performance of a standard Iterator interface which returns java.lang.Longs
	 * (1 object)
	 */
	private static long test2(Iterator iter) {
		long result = 0;
		while (iter.hasNext()) {
			result ^= ((Long)iter.next()).longValue();
		}
		
		return result;
	}
	
	/*
	 * Test the performance of a standard Iterator interface which returns objects 
	 * which wrap java.lang.Longs
	 * (2 objects)
	 */
	private static long test3(Iterator iter) {
		long result = 0;
		
		while (iter.hasNext()) {
			result ^= ((LongWrapper)iter.next()).getValue().longValue();
		}
		
		return result;
	}
   }
