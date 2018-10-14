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

import java.lang.reflect.Method;

public class loopRep
   {
      public static LRProps lrp;
      public static LRProps warmupLR;
      public static void main(String [] args)
         {
            lrp = new LRProps(16, "test");
            warmupLR = new LRProps(1, "warm");
            String [] className = {"While", "doWhile"};
            for (int klass = 0; klass < className.length; klass++)
               {
                  System.out.println("Processing - "+className[klass]);
                  Class c = null;
                  try 
                     {
                        c = Class.forName(className[klass]);
                     } 
                  catch (ClassNotFoundException e)
                     {
                        System.out.println("Class not found - "+className[klass]+" skipping...");
                        continue;
                     }
                  Method[] m = null;
                  try
                     {
                        m = c.newInstance().getClass().getMethods();
                     }
                  catch (Exception e)
                     {
                        System.out.println("unable to obtain methods for class - "+className[klass]);
                        continue;
                     }
         
                  Object [] warmupLRobj = new Object[1];
                  warmupLRobj[0] = warmupLR;
                  Object [] lrpobj = new Object[1];
                  lrpobj[0] = lrp;
                  for (int i = 0; i < m.length; i++)
                     {
                        if (m[i].getName().startsWith("test"))
                           {
                              try
                                 {
                                    System.out.println("invoke method to warmup - "+m[i].getName());
                                    for (int k = 0; k < lrp.iterations; k++)
                                       m[i].invoke(c.newInstance(), warmupLRobj);
                                    System.gc();
                                    System.out.println("invoke real method - "+m[i].getName());
                                    m[i].invoke(c.newInstance(), lrpobj);
                                 }
                              catch (Exception e)
                                 {
                                    try 
                                       {
                                          System.out.println("unable to invoke method - "+m[i].getName());
                                          continue;
                                       }
                                    catch (Exception se)
                                       {
                                          System.out.println("cannot print method name exception - "+se.toString());
                                          continue;
                                       } 
                                 }
                           }
                     }   
               }
         } 
   }

class LRProps
   {
      public LRProps(int iters, String s)
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

class While
   {
      public static final int limit = 18097;
      public static byte [] byteArr = new byte[limit];
      public static char [] charArr = new char[limit];
      public While()
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
      public void test001(LRProps lrp)
         {
            int k = 0xff;
            for (int i = lrp.iterations; i >= 0; i--)
               {
                  k = k & sub001(); 
               }
            // escape
            lrp.joker = k;
         }
      private int sub001()
         {
            int r = 0xff;
      
            for (int k = byteArr.length - 1; k > 0; k--)
               {
                  char c = (char)(byteArr[k] & 0xff);
                  if (byteArr[k] == 0x0b) // rare
                     {
                        charArr[k] = (char)(byteArr[k] + 0x0c);
                     }
                  else
                     charArr[k] = c;
                  r = r ^ charArr[k];
               }
            return r;
         }

      public boolean test002(LRProps lrp)
         {
            // looper
            char [] a = new char[limit];
            for (int j = 0; j < lrp.iterations; j++)
               {
                  int i = -1;
                  while (i < limit)
                     {
                        i++;
                        if (byteArr[i] == 0x0b)
                           {
                              a[i] = (char)(byteArr[i] + 0x0c);
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
