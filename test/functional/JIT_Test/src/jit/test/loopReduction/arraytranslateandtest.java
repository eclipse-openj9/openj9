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

public class arraytranslateandtest {
   private static byte data[] = new byte[1024];

   public void testSimpleByteSwitch(Context c) {
      for (int i = 0; i < data.length; ++i) {
         data[i] = ' ';
      }
      data[data.length / 2] = 'a';
      int alpha = 0;
      int period = 0;
      int unk = 0;
      for (int iters = 1; iters <= c.iterations()*10; ++iters) {
         for (int i = 0; i < data.length; ++i) {
            switch (data[i]) {
            case ' ':
               break;
            case '.':
               ++period;
               break;
            case 'a':
               ++alpha;
               break;
            default:
               ++unk;
               break;
            }
         }
         if (c.verify())
            if (alpha != iters || period != 0) {
               c.printerr("Wrong number of matches on iteration " + iters);
               break;
            }
      }
   }

   public void testFindByteSwitch(Context c) {
      byte lessThan = '<';
      for (int i = 0; i < data.length; ++i) {
         data[i] = ' ';
      }
      data[data.length / 2] = lessThan;
      for (int iters = 1; iters <= c.iterations() * 10; ++iters) {
         int currentOffset = 0;
         while (currentOffset < data.length) {
            if (data[currentOffset] == lessThan)
               break;
            ++currentOffset;
         }
         if (c.verify())
            if (currentOffset != data.length / 2) {
               c.printerr("Wrong number of matches on iteration " + iters);
               break;
            }
      }
   }

   public void testBoundedFindByteSwitch(Context c) {
      byte lessThan = (byte) '<';
      for (int i = 0; i < data.length; ++i) {
         data[i] = (byte) ' ';
      }
      data[(data.length / 2) + 1] = lessThan;
      for (int iters = 1; iters <= c.iterations() * 10; ++iters) {
         int i;
         for (i = 0; i < data.length; ++i) {
            if (data[i] == lessThan)
               break;
         }
         if (c.verify())
            if (i >= data.length || data[i] != lessThan) {
               c.printerr("Wrong number of matches on iteration " + iters + " offset " + i);
               break;
            }
      }
   }
}
