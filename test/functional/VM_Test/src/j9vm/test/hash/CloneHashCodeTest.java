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

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/*
 * Test that clones objects don't have the same hash code as the
 * object they were cloned from. Although this can happen occasionally
 * it shouldn't happen regularly. 
 */
public class CloneHashCodeTest extends HashCodeTestParent {

	public CloneHashCodeTest(int mode) {
		super(mode);
	}


	static class ZeroSlots implements Cloneable {
		public Object clone() { 
			try {
				return super.clone();  
			} catch (CloneNotSupportedException e) {
				throw new Error(e);
			}
		}
		
	}

	static class OneSlot extends ZeroSlots {
		int slot1;
	}

	static class TwoSlots extends OneSlot {
		int slot2;
	}

	static class ThreeSlots extends TwoSlots {
		int slot3;
	}

	static class FourSlots extends ThreeSlots {
		int slot4;
	}

	
	public static void main(String[] args) {
		for (int i = 0; i < MODES.length; i++) {
			new CloneHashCodeTest(MODES[i]).test();	
		}
	}

	
	public void test() {
		gc();
	
		/* test that an object's hash code doesn't get cloned */
		Cloneable[] objects = {
				new ZeroSlots(),
				new OneSlot(),
				new TwoSlots(),
				new ThreeSlots(),
				new FourSlots(),
				new boolean[0],
				new byte[0],
				new char[0],
				new short[0],
				new int[0],
				new float[0],
				new long[0],
				new double[0],
				new Object[0]
		};
		
		int[] hashCodes = new int[objects.length];
		for (int i = 0; i < objects.length; i++) {
			hashCodes[i] = objects[i].hashCode();
		}

		Object[] clonesBeforeGC = new Object[objects.length];
		for (int i = 0; i < objects.length; i++) {
			clonesBeforeGC[i] = doClone(objects[i]);
		}
		
		int[] cloneHashCodes = new int[objects.length];
		for (int i = 0; i < objects.length; i++) {
			cloneHashCodes[i] = clonesBeforeGC[i].hashCode();
		}
		
		gc();

		for (int i = 0; i < objects.length; i++) {
			int newHashCode = objects[i].hashCode();
			if (hashCodes[i] != newHashCode) {
				throw new Error("Hash code for " + objects[i] + " changed: " + hashCodes[i] + " != " + newHashCode);
			}
			
			newHashCode = clonesBeforeGC[i].hashCode();
			if (cloneHashCodes[i] != newHashCode) {
				throw new Error("Hash code for cloned object " + clonesBeforeGC[i] + " changed: " + cloneHashCodes[i] + " != " + newHashCode);
			}
		}
	}
	
	private Cloneable doClone(Cloneable obj) {
		if (obj != null) {
			if (obj instanceof boolean[]) {
				return (Cloneable)((boolean[])obj).clone();
			} else if (obj instanceof byte[]) {
				return (Cloneable)((byte[])obj).clone();
			} else if (obj instanceof char[]) {
				return (Cloneable)((char[])obj).clone();
			} else if (obj instanceof short[]) {
				return (Cloneable)((short[])obj).clone();
			} else if (obj instanceof int[]) {
				return (Cloneable)((int[])obj).clone();
			} else if (obj instanceof float[]) {
				return (Cloneable)((float[])obj).clone();
			} else if (obj instanceof long[]) {
				return (Cloneable)((long[])obj).clone();
			} else if (obj instanceof double[]) {
				return (Cloneable)((double[])obj).clone();
			} else if (obj instanceof Object[]) {
				return (Cloneable)((Object[])obj).clone();
			} else {
				try {
					Method clone = obj.getClass().getMethod("clone", null);
					return (Cloneable)clone.invoke(obj, null);
				} catch (NoSuchMethodException e) {
					throw new Error(e);
				} catch (IllegalAccessException e) {
					throw new Error(e);
				} catch (InvocationTargetException e) {
					throw new Error(e);
				}
			}
		}
		return null;
	}
	
}
