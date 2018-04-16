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
package jit.test.tr.BigDecimal;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import java.math.BigDecimal;
import java.io.*;

@Test(groups = { "level.sanity","component.jit" })
public class BDSerializationTest {
   @Test
   public void testSerialization1() throws IOException, ClassNotFoundException {
      BigDecimal a = new BigDecimal("350000");
      BigDecimal b = new BigDecimal("8930.00");
      BigDecimal c1 = a.subtract(b);
      FileOutputStream fos = new FileOutputStream("t.tmp");
      ObjectOutputStream oos = new ObjectOutputStream(fos);
      oos.writeObject(c1);
      oos.close();
      fos.close();
      FileInputStream fis = new FileInputStream("t.tmp");
      ObjectInputStream ois = new ObjectInputStream(fis);
      BigDecimal c2 = (BigDecimal) ois.readObject();
      ois.close();
      fis.close();
      AssertJUnit.assertEquals("testSerialization1", c1, c2);
   }
}
