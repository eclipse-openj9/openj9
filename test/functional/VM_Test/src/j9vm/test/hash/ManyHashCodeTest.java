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
 * Created on Jul 9, 2007
 *
 * To change the template for this generated file go to
 * Window>Preferences>Java>Code Generation>Code and Comments
 */
package j9vm.test.hash;

import java.util.Iterator;
import java.util.Vector;

public class ManyHashCodeTest extends HashCodeTestParent {

	public ManyHashCodeTest(int mode) {
		super(mode);
	}


	static class TestClass {
		int oldHashCode = hashCode();
	}
	
	/**
	 * @param args
	 */
	public static void main(String[] args) {
		for (int i = 0; i < MODES.length; i++) {
			new ManyHashCodeTest(MODES[i]).test();
		}
	}

	
	public void test() {
		/* test that many objects' hash codes remain stable  */
		Vector v = new Vector();
		
		for (int i = 0; i < 100000; i++) {
			/* create a garbage object */
			new TestClass();

			/* create a retained object */
			v.add(new TestClass());
		}
		
		gc();
		
		/* test that the hash codes are stable */
		for (Iterator iterator = v.iterator(); iterator.hasNext();) {
			TestClass obj = (TestClass)iterator.next();
			int newHashCode = obj.hashCode();
			if (obj.oldHashCode != newHashCode) {
				throw new Error("Hash code changed: " + obj.oldHashCode + " != " + newHashCode);
			}
		}
	}
}
