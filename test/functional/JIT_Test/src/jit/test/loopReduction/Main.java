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
package jit.test.loopReduction;

import java.lang.reflect.Method;

public class Main {
   public static Context warmUpContext;

   public static Context context;

   public static void main(String[] args) {
      context = new Context(args);
      if (context.help()) {
         context.printHelp();
         return;
      }
      String hardware = System.getProperty("os.arch");
      int perfRating = 2000; // rough guide to speed of hardware for reasonable testing
      if (hardware.equalsIgnoreCase("x86")) {
         perfRating = 4000;
      }
      else if (hardware.equalsIgnoreCase("s390")) {
         perfRating = 1500;
      }
      if (context.timed()) {
         context.setIterations(10*perfRating);
      } else {
         context.setIterations(perfRating);
      }
      warmUpContext = new Context(args);
      warmUpContext.makeQuiet();
      warmUpContext.setIterations(1);
      String[] className = { "jit.test.loopReduction.idiomTests", "jit.test.loopReduction.bytearraywrite", "jit.test.loopReduction.arraycompare", "jit.test.loopReduction.arraycopy", "jit.test.loopReduction.arrayset", "jit.test.loopReduction.arraytranslateandtest" };
      for (int clazz = 0; clazz < className.length; ++clazz) {
         context.println("Process Class " + className[clazz]);
         Class c = null;
         try {
            if (clazz == 0) {
               c = Class.forName("java.nio.ByteBuffer");
               c = Class.forName("java.nio.ByteOrder");
            }

            c = Class.forName(className[clazz]);
         } catch (ClassNotFoundException cnfe) {
            System.err.println("Not able to find class " + className[clazz] + ". Class skipped");
            continue;
         } catch (NoClassDefFoundError ncdfe) {
            System.err.println("Not able to find class " + className[clazz] + ". Class skipped");
            continue;
         }
         Method[] method = null;
         try {
            method = c.newInstance().getClass().getMethods();
         } catch (Exception e) {
            System.err.println("Not able to get methods for class " + className[clazz]);
            continue;
         }
         for (int i = 0; i < method.length; i++) {
            Object[] warmUpParms = new Object[1];
            warmUpParms[0] = warmUpContext;
            Object[] testParms = new Object[1];
            testParms[0] = context;
            if (method[i].getName().startsWith("test") && !context.restricted(method[i].getName())) {
               try {
                  context.println("Warm Up Method " + method[i].getName());
                  for (int iters = 0; iters < context.iterations(); ++iters) {
                     method[i].invoke(c.newInstance(), warmUpParms);
                  }
                  System.gc();
                  context.println("Test Method " + method[i].getName());
                  context.start();
                  method[i].invoke(c.newInstance(), testParms);
                  context.end();
               } catch (Exception e) {
                  try {
                     e.printStackTrace();
                     System.err.println(e);
                     System.err.println("Not able to invoke method " + method[i].getName() + ". Method invocation skipped");
                     continue;
                  } catch (Exception ee) {
                     System.err.println("Error encountered printing error message for method " + method[i]);
                     continue;
                  }
               }
            }
         }
      }
      if (context.hadErrors()) {
         System.err.println("Errors encountered testing");
      } else {
         System.out.println("SUCCESSFUL - LoopReduction");
      }
   }

}
