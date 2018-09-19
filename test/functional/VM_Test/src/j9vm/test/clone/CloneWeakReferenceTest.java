/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

		if (VersionCheck.major() >= 11) {
			try {
				SubWeakReference clone = (SubWeakReference) reference.clone();
				// fail if CloneNotSupportedException is not thrown
				throw new RuntimeException(
						"CloneNotSupportedException is expected, but it is not thrown for JDK version: "
								+ VersionCheck.major());
			} catch (CloneNotSupportedException e) {
				// CloneNotSupportedException is expected for JDK version >= 11
			}
		} else {
			SubWeakReference clone = (SubWeakReference) reference.clone();
			
			if (clone.get() == referent) {
				System.out.println("original and cloned weak references using the same referent (expected)");
			} else {			
				throw new RuntimeException("original and cloned weak references are not using the same referent!");
			}
			
			referent = null;
			
			System.gc();
			
			/* At this point the weak references may not have been cleared.  In metronome, if a GC cycle
			 * is ongoing when System.gc is called, the cycle will be finished. This GC is a 
			 * snapshot-at-the-beginning collector.  If the GC cycle was started BEFORE referent was set
			 * to null, then String object will not be collected and the weak references will not be
			 * cleared.
			 */

			if (reference.get() != clone.get()) {
				throw new RuntimeException("Referents not identical. reference.get(): " + reference.get() + " clone.get(): " + clone.get());
			}
			
			System.gc();
			
			if (reference.get() != null) {
				throw new RuntimeException("Reference referent not cleared: " + reference + " " + reference.get());
			}
			
			if (clone.get() != null) {
				throw new RuntimeException("Clone referent not cleared: " + clone + " " + clone.get());
			}
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
