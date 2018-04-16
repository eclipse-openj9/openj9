/*******************************************************************************
 * Copyright (c) 2004, 2018 IBM Corp. and others
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
// loop replicator sample testcase
package jit.test.tr.loopReplicator;

import java.io.*;

public class Main 
   {
   public int iterations;
   public long vowCount;
   public long newlineCount;
   public static int N = 10000; 
   
   public Main() 
      {
      }
   public Main(int iters)
      {
      if (iters < 0) 
         iterations = -1*iters;
      iterations = iters;
      }
   
   public static void main(String [] argv)
      {
      if (argv.length < 2)
         {
         System.out.println("Usage: java Main file iters");
         System.out.println("Recommended setting iters = 10 [to get to scorching]");
         System.exit(1);
         }
      File f = new File(argv[0]);
      Main m = new Main(Integer.parseInt(argv[1]));
      try 
         {
         ///boolean success = f.createNewFile();
         RandomAccessFile raf = new RandomAccessFile(f, "r");
         long remFilePointer = raf.getFilePointer(); // orig file position
         for (int i = 0; i < m.iterations; i++)
            {
            m.testProps(raf);
            raf.seek(remFilePointer); // restore the pointer for next iteration
            }
         System.out.println("Number of vowels: "+m.vowCount);
         System.out.println("Number of newlines: "+m.newlineCount);
         }
      catch (Exception e)
         {
         ///if (e instanceof FileNotFoundException)
         ///   System.out.println("Error opening the file "+argv[0]+" !");
         System.out.println(e.toString()); 
         }
      }
   
   public void testProps(RandomAccessFile raf) throws IOException
      {
      int c;
      boolean val = true;
      long vowKeep = 32768;
      long newlineKeep = 65536;
      int counter = 0;
      
      while ((c = raf.read()) != -1 && val)
         {
         // here's a block before the switch
         // split control
         if (++counter < (0.9*N))
            {
            switch (c)
               {
               case 'e':
               case 'a':
               case 'i':
               case 'o':
               case 'u':
                       vowCount++; // common case
                       break;
               case '\n':
                       newlineCount++; // rare case
                       break;
               default:
                       // here are some more multiple blocks
                       vowKeep++;
                       newlineKeep--;
                       if (newlineKeep < 0)
                          newlineKeep = - newlineKeep;
                       break;
               }
            // let's get blocks following switch 
            if ((vowCount%2) == 0)
               {
               vowKeep = vowKeep + vowCount;
               newlineKeep = newlineKeep + newlineCount;
               }
            else
               {
               vowKeep++;
               newlineKeep++;
               }
            }
         else
            {
            vowKeep--;
            newlineKeep--;
            if (counter >= N) 
               counter = 0;
            }
         // join 
         if ((newlineCount%2) == 0)
            {
            newlineKeep--;
            vowKeep++;
            }
         }
      
      // lets get more blocks in the method
      if ((vowCount%2) == 0)
         {
         vowKeep = vowCount;
         newlineKeep = newlineCount;
         val = true;
         }
      else 
         {
         vowKeep = vowKeep + vowCount;
         newlineKeep = newlineKeep + newlineCount;
         val = false;
         }
      if (val)
         printMe(false, true, vowKeep);
      else
         printMe(false, true, newlineKeep);
      }

   public static void printMe(boolean b, boolean val, long field) 
      {
      if (b)
         {
         if (val)
            System.out.print("its even ");
         else
            System.out.print("its odd ");
         System.out.println(field);
         }
      }
   }

/*
 * test cases
 */
/* test scenarios
 * test code to determine blocks following the switch block -
 * a. loop with just a switch ie. while <cond> { switch }
 * b. while <cond> { <some code> switch }
 * c. while <cond> { <some code> switch <single block> }
 * d. while <code> { <some code> switch <cases with multiple blocks> <multiple blocks> }
 */
