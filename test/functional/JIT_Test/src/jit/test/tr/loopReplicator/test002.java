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
package jit.test.tr.loopReplicator;

public class test002
   {
      public static LRprops lrp;
      public static final int limit = 18097;
      public static byte [] byteArr = new byte[limit];
      
      public test002()
         {
            // init
            for (int j = 0; j < byteArr.length; j++)
               {
                  int l = (j%250);
                  if (l == 0)
                     byteArr[j] = 0x0a;
                  else
                     byteArr[j] = 0x0b;
               } 
         }
      public static void main (String [] argv)
         {
            lrp = new LRprops(10, "test");
            test002 t2 = new test002();
            for (int i = 0; i < lrp.iterations; i++)
               t2.subtest002(lrp);
         }
      private boolean subtest002(LRprops lrp)
         {
            // looper
            char [] a = new char[2*limit];
            for (int j = 0; j < lrp.iterations; j++)
               {
                  int i = 0;
                  while (i < limit)
                     {
                        if (byteArr[i] == 0x0b)
                           {
                              a[i] = (char)(byteArr[i] + 0x0c);
                              i++;
                           }
                        else 
                           {
                              int type = lrp.joker;
                              switch(type)
                                 {
                                    default:
                                       return false;
                                    case 1:
                                       i += 2;
                                       break;
                                    case 2:
                                       i += 3;
                                       break;
                                 }
                           }
                     }
               }
            if (a[limit-1] != 0x0b)
               return false;
            return true;
         }
   }
class LRprops
   {
      public LRprops(int iters, String s)
         {
            iterations = 1 << iters;
            name = s;
            joker = 1;
         }

      public String getName() { return name; }
      public int iterations;
      private String name;
      // meaning depends on the test 
      public int joker;
   }
