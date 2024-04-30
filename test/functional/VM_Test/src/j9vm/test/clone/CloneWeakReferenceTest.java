/*
 * Copyright IBM Corp. and others 2001
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package j9vm.test.clone;

import java.lang.ref.WeakReference;
import org.openj9.test.util.VersionCheck;

/**
 * Test case to ensure that the VM is properly cloning weak references.
 * 
 * In order for the test case to pass, the referent of the cloned weak
 * reference must be the same referent in the original weak reference.  Also,
 * after garbage collection, the referent in each weak reference should be
 * cleared (e.g. reference.get() == null).
 * 
 * Please refer to defect 136236 for past history of this problem.
 * 
 * @author Jonathan Ball (jon_ball@ca.ibm.com)
 */
public class CloneWeakReferenceTest {
	
	public static void main(String[] args) throws CloneNotSupportedException {	
		Object referent = new String("foobar");
		
		SubWeakReference reference = new SubWeakReference(referent);

		try {
			SubWeakReference clone = (SubWeakReference) reference.clone();
			// fail if CloneNotSupportedException is not thrown
			throw new RuntimeException(
					"CloneNotSupportedException is expected, but it is not thrown for JDK version: "
							+ VersionCheck.major());
		} catch (CloneNotSupportedException e) {
			// CloneNotSupportedException is expected
		}
	}

	static class SubWeakReference extends WeakReference implements Cloneable {

		public SubWeakReference(Object referent) {
			super(referent);
		}

		public Object clone() throws CloneNotSupportedException {
			return super.clone();
		}
	}
}
