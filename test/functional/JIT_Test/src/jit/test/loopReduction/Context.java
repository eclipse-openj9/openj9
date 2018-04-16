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

import java.util.Iterator;
import java.util.Vector;

public class Context {
   private boolean errors;
   private boolean help;
   private boolean timing;

   private int iterations;

   private boolean verify;

   private long endTime;

   private long startTime;

   private boolean verbose;

   private Vector restrictedMethod = new Vector();

   public Context() {
   }

   public Context(String args[]) {
      if (args.length >= 1) {
         int i = 0;
         while (i < args.length && args[i].startsWith("-")) {
            if (args[i].equals("-verify")) {
               verify = true;
            } else if (args[i].equals("-timing")) {
               timing = true;
            } else if (args[i].equals("-verbose")) {
               verbose = true;
            }
            else if (args[i].equals("-help")) {
               help = true;
               verbose = true;
            }
            ++i;
         }
         while (i < args.length) {
            restrictedMethod.add(args[i]);
            ++i;
         }
      } else {
         restrictedMethod = null;
      }
   }

   public boolean verification() {
      return verify;
   }

   public void enableVerification() {
      verify = true;
   }

   public boolean timed() {
      return timing;
   }

   public void enableTiming() {
      timing = true;
   }

   public boolean verbose() {
      return verbose;
   }

   public void makeVerbose() {
      verbose = true;
   }

   public void println(String string) {
      if (verbose) {
         System.out.println(string);
      }
   }

   public void printerr(String string) {
      if (verbose) {
         System.err.println(string);
      }
      errors = true;
   }

   public int iterations() {
      return iterations;
   }

   public int setIterations(int iterations) {
      return this.iterations = iterations;
   }

   public void makeQuiet() {
      verbose = false;
   }

   public boolean restricted(String string) {
      
      if (restrictedMethod == null || restrictedMethod.isEmpty()) {
         return false;
      }
      Iterator i = restrictedMethod.iterator();
      boolean exclude = false;
      
      while (i.hasNext()) {
         String method = (String) i.next();
         if (method.startsWith("!") || method.startsWith("^")) {
            method = method.substring(1, method.length());
            if (string.indexOf(method) != -1) {
               exclude = true;
               break;
            }
         }
      }
      i = restrictedMethod.iterator();
      boolean include = false;
      while (i.hasNext()) {
         String method = (String) i.next();
         if (string.indexOf(method) != -1) {
            include = true;
            break;
         }
      }
      if (include && exclude) {
         return exclude;
      }
      else if (include) {
         return false;
      }
      else if (exclude) {
         return true;
      }
      return true;
   }

   public void start() {
      startTime = System.currentTimeMillis();
   }

   public void end() {
      endTime = System.currentTimeMillis();
      if (verbose && timing) {
         long time = (endTime - startTime);
         System.out.println("Test took " + time + " millis");
      }
   }

   public boolean verify() {
      return verify;
   }

   public boolean hadErrors() {
      return errors;
   }

   public boolean help() {
      return help;
   }

   public void printHelp() {
      println("Main [-verify] [-verbose] [-timing] [-help] [method list]*");
      println(" Defaults to noverify, noverbose, timing and running all tests");
      println(" -verify indicates that each test should run it's self-verification");
      println(" -timing indicates that a warm-up and timing phase should be used for performance analysis");
      println(" -verbose indicates that timing information should be printed out");
      println(" -help indicates to print this help info");
      println(" One or more method patterns can be specified, indicating what should be tested");
      println(" If the method is prefixed with ! or ^ it indicates that method pattern should be excluded from the test");
      println("Example: Main -verify -verbose J2SE !J2SEByteCompare");
      println("  Run all methods that have J2SE in them except methods that have J2SEByteCompare in them");
   }
}
