/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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
/*
 * Created on Jun 13, 2007
 *
 * To change the template for this generated file go to
 * Window>Preferences>Java>Code Generation>Code and Comments
 */
package j9vm.test.ref;

import java.lang.ref.SoftReference;
import java.lang.reflect.Field;

/**
 * 
 * @author pburka
 *
 * Test that multiple SoftReferences age at the same rate.
 * The only way to test this reliably is to reach into the
 * private "age" field. If the implementation of SoftReferences
 * changes then this test may become invalid.
 */
public class SoftReferenceAgingTest {

	private SoftReference r1;
	private SoftReference r2;
	
	public static void main(String[] args) throws Throwable {
		new SoftReferenceAgingTest().test();
	}
	
	private void test() throws Throwable {
		Field age = SoftReference.class.getDeclaredField("age");
		age.setAccessible(true);
		
		/* reset both ages in case a GC occurred between creating the two refs */
		r1.get();
		r2.get();
		
		int prevAge = -1;
		for (int i = 0; i < 70; i++) {
			int age1 = age.getInt(r1);
			int age2 = age.getInt(r2);
			
			if (age1 != age2) {
				throw new Error("SoftReference1.age (" + age1 + ") not equal to SoftReference2.age (" + age2 + ")");
			} else if (age1 <= prevAge) {
				throw new Error("SoftReference age failed to increase following System.gc()");
			}
			
			System.gc();
		}
	}
	
	private SoftReferenceAgingTest() {
		Object o = new Object();
		r1 = new SoftReference(o);
		r2 = new SoftReference(o);	
	}
}
