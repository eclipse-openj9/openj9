package j9vm.test.romclasscreation;

/*******************************************************************************
 * Copyright (c) 2010, 2019 IBM Corp. and others
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

import java.io.*;

/**
 * This test checks that the VM removes the orphan ROM class
 * from the romClassOrphans hash table once the RAM class has
 * been loaded successfully. It uses a custom class loader to
 * first load a class whose superclass is not found, which should
 * produce an orphan ROM class, followed by allowing the class
 * loader to find the parent class and trying to load the class
 * again, which should succeed and remove the ROM class from the
 * orphan table.
 * 
 * @author Alexei Svitkine
 *
 */
public class OrphanROMHashTableDeletionTest {
	/** Empty class that has ParentOfOrphanClass as parent. It's class bytes are in ORPHAN_CLASS_BYTES. */
	public static final String ORPHAN_CLASS = "OrphanClass";
	/** Empty class is the parent of ParentOfOrphanClass. It's class bytes are in ORPHAN_CLASS_PARENT_BYTES. */
	public static final String ORPHAN_CLASS_PARENT = "ParentOfOrphanClass";

	public static void main(String[] args) {
		new OrphanROMHashTableDeletionTest().test();
	}

	private static class OrphanTestClassLoader extends ClassLoader {
		private boolean shouldLoadParent = false;

		public void setShouldLoadParent(boolean shouldLoadParent) {
			this.shouldLoadParent = shouldLoadParent;
		}

		protected synchronized Class loadClass(String className, boolean resolve) throws ClassNotFoundException {
			Class klass = null;

			if (className.equals(ORPHAN_CLASS)) {
				klass = defineClass(className, ORPHAN_CLASS_BYTES, 0, ORPHAN_CLASS_BYTES.length);
			} else if (shouldLoadParent && className.equals(ORPHAN_CLASS_PARENT)) {
				klass = defineClass(className, ORPHAN_CLASS_PARENT_BYTES, 0, ORPHAN_CLASS_PARENT_BYTES.length);
			} else {
				return super.loadClass(className, resolve);
			}

			if (resolve) {
				resolveClass(klass);
			}

			return klass;
		}
	}

	private void test() {
		OrphanTestClassLoader classLoader = new OrphanTestClassLoader();	

		/* Try loading the parent class. Expect failure - it should not be on the classpath. */
		try {
			Class.forName(ORPHAN_CLASS_PARENT, true, classLoader);
			System.err.println("Error: Class.forName(" + ORPHAN_CLASS_PARENT + ") should not have succeeded!");
			return;
		} catch (java.lang.ClassNotFoundException exc) {
		} catch (java.lang.NoClassDefFoundError err) {
		}

		/* Try loading the child class. Expect failure - it's parent class is not on the classpath. */
		try {
			Class.forName(ORPHAN_CLASS, true, classLoader);
			System.err.println("Error: Class.forName(" + ORPHAN_CLASS + ") should not have succeeded!");
			return;
		} catch (java.lang.ClassNotFoundException exc) {
		} catch (java.lang.NoClassDefFoundError err) {
		}

		/* Make the parent class loadable. */
		classLoader.setShouldLoadParent(true);

		/* Try loading the child class now - it should succeed. */
		try {
			Class.forName(ORPHAN_CLASS, true, classLoader);
		} catch (Throwable t) {
			System.err.println("Error: Class.forName(" + ORPHAN_CLASS + ") should not have failed!");
			t.printStackTrace();
		}
	}

	/* Child .class file as a byte array. */
	private static final byte[] ORPHAN_CLASS_BYTES = new byte[] {
		(byte) 0xca, (byte) 0xfe, (byte) 0xba, (byte) 0xbe, 
		(byte) 0x00, (byte) 0x00, (byte) 0x00, (byte) 0x32, 
		(byte) 0x00, (byte) 0x0d, (byte) 0x0a, (byte) 0x00, 
		(byte) 0x03, (byte) 0x00, (byte) 0x0a, (byte) 0x07, 
		(byte) 0x00, (byte) 0x0b, (byte) 0x07, (byte) 0x00, 
		(byte) 0x0c, (byte) 0x01, (byte) 0x00, (byte) 0x06, 
		(byte) 0x3c, (byte) 0x69, (byte) 0x6e, (byte) 0x69, 
		(byte) 0x74, (byte) 0x3e, (byte) 0x01, (byte) 0x00, 
		(byte) 0x03, (byte) 0x28, (byte) 0x29, (byte) 0x56, 
		(byte) 0x01, (byte) 0x00, (byte) 0x04, (byte) 0x43, 
		(byte) 0x6f, (byte) 0x64, (byte) 0x65, (byte) 0x01, 
		(byte) 0x00, (byte) 0x0f, (byte) 0x4c, (byte) 0x69, 
		(byte) 0x6e, (byte) 0x65, (byte) 0x4e, (byte) 0x75, 
		(byte) 0x6d, (byte) 0x62, (byte) 0x65, (byte) 0x72, 
		(byte) 0x54, (byte) 0x61, (byte) 0x62, (byte) 0x6c, 
		(byte) 0x65, (byte) 0x01, (byte) 0x00, (byte) 0x0a, 
		(byte) 0x53, (byte) 0x6f, (byte) 0x75, (byte) 0x72, 
		(byte) 0x63, (byte) 0x65, (byte) 0x46, (byte) 0x69, 
		(byte) 0x6c, (byte) 0x65, (byte) 0x01, (byte) 0x00, 
		(byte) 0x10, (byte) 0x4f, (byte) 0x72, (byte) 0x70, 
		(byte) 0x68, (byte) 0x61, (byte) 0x6e, (byte) 0x43, 
		(byte) 0x6c, (byte) 0x61, (byte) 0x73, (byte) 0x73, 
		(byte) 0x2e, (byte) 0x6a, (byte) 0x61, (byte) 0x76, 
		(byte) 0x61, (byte) 0x0c, (byte) 0x00, (byte) 0x04, 
		(byte) 0x00, (byte) 0x05, (byte) 0x01, (byte) 0x00, 
		(byte) 0x0b, (byte) 0x4f, (byte) 0x72, (byte) 0x70, 
		(byte) 0x68, (byte) 0x61, (byte) 0x6e, (byte) 0x43, 
		(byte) 0x6c, (byte) 0x61, (byte) 0x73, (byte) 0x73, 
		(byte) 0x01, (byte) 0x00, (byte) 0x13, (byte) 0x50, 
		(byte) 0x61, (byte) 0x72, (byte) 0x65, (byte) 0x6e, 
		(byte) 0x74, (byte) 0x4f, (byte) 0x66, (byte) 0x4f, 
		(byte) 0x72, (byte) 0x70, (byte) 0x68, (byte) 0x61, 
		(byte) 0x6e, (byte) 0x43, (byte) 0x6c, (byte) 0x61, 
		(byte) 0x73, (byte) 0x73, (byte) 0x00, (byte) 0x21, 
		(byte) 0x00, (byte) 0x02, (byte) 0x00, (byte) 0x03, 
		(byte) 0x00, (byte) 0x00, (byte) 0x00, (byte) 0x00, 
		(byte) 0x00, (byte) 0x01, (byte) 0x00, (byte) 0x01, 
		(byte) 0x00, (byte) 0x04, (byte) 0x00, (byte) 0x05, 
		(byte) 0x00, (byte) 0x01, (byte) 0x00, (byte) 0x06, 
		(byte) 0x00, (byte) 0x00, (byte) 0x00, (byte) 0x1d, 
		(byte) 0x00, (byte) 0x01, (byte) 0x00, (byte) 0x01, 
		(byte) 0x00, (byte) 0x00, (byte) 0x00, (byte) 0x05, 
		(byte) 0x2a, (byte) 0xb7, (byte) 0x00, (byte) 0x01, 
		(byte) 0xb1, (byte) 0x00, (byte) 0x00, (byte) 0x00, 
		(byte) 0x01, (byte) 0x00, (byte) 0x07, (byte) 0x00, 
		(byte) 0x00, (byte) 0x00, (byte) 0x06, (byte) 0x00, 
		(byte) 0x01, (byte) 0x00, (byte) 0x00, (byte) 0x00, 
		(byte) 0x01, (byte) 0x00, (byte) 0x01, (byte) 0x00, 
		(byte) 0x08, (byte) 0x00, (byte) 0x00, (byte) 0x00, 
		(byte) 0x02, (byte) 0x00, (byte) 0x09
	};

	/* Parent .class file as a byte array. */
	private static final byte[] ORPHAN_CLASS_PARENT_BYTES = new byte[] {
		(byte) 0xca, (byte) 0xfe, (byte) 0xba, (byte) 0xbe, 
		(byte) 0x00, (byte) 0x00, (byte) 0x00, (byte) 0x32, 
		(byte) 0x00, (byte) 0x0d, (byte) 0x0a, (byte) 0x00, 
		(byte) 0x03, (byte) 0x00, (byte) 0x0a, (byte) 0x07, 
		(byte) 0x00, (byte) 0x0b, (byte) 0x07, (byte) 0x00, 
		(byte) 0x0c, (byte) 0x01, (byte) 0x00, (byte) 0x06, 
		(byte) 0x3c, (byte) 0x69, (byte) 0x6e, (byte) 0x69, 
		(byte) 0x74, (byte) 0x3e, (byte) 0x01, (byte) 0x00, 
		(byte) 0x03, (byte) 0x28, (byte) 0x29, (byte) 0x56, 
		(byte) 0x01, (byte) 0x00, (byte) 0x04, (byte) 0x43, 
		(byte) 0x6f, (byte) 0x64, (byte) 0x65, (byte) 0x01, 
		(byte) 0x00, (byte) 0x0f, (byte) 0x4c, (byte) 0x69, 
		(byte) 0x6e, (byte) 0x65, (byte) 0x4e, (byte) 0x75, 
		(byte) 0x6d, (byte) 0x62, (byte) 0x65, (byte) 0x72, 
		(byte) 0x54, (byte) 0x61, (byte) 0x62, (byte) 0x6c, 
		(byte) 0x65, (byte) 0x01, (byte) 0x00, (byte) 0x0a, 
		(byte) 0x53, (byte) 0x6f, (byte) 0x75, (byte) 0x72, 
		(byte) 0x63, (byte) 0x65, (byte) 0x46, (byte) 0x69, 
		(byte) 0x6c, (byte) 0x65, (byte) 0x01, (byte) 0x00, 
		(byte) 0x18, (byte) 0x50, (byte) 0x61, (byte) 0x72, 
		(byte) 0x65, (byte) 0x6e, (byte) 0x74, (byte) 0x4f, 
		(byte) 0x66, (byte) 0x4f, (byte) 0x72, (byte) 0x70, 
		(byte) 0x68, (byte) 0x61, (byte) 0x6e, (byte) 0x43, 
		(byte) 0x6c, (byte) 0x61, (byte) 0x73, (byte) 0x73, 
		(byte) 0x2e, (byte) 0x6a, (byte) 0x61, (byte) 0x76, 
		(byte) 0x61, (byte) 0x0c, (byte) 0x00, (byte) 0x04, 
		(byte) 0x00, (byte) 0x05, (byte) 0x01, (byte) 0x00, 
		(byte) 0x13, (byte) 0x50, (byte) 0x61, (byte) 0x72, 
		(byte) 0x65, (byte) 0x6e, (byte) 0x74, (byte) 0x4f, 
		(byte) 0x66, (byte) 0x4f, (byte) 0x72, (byte) 0x70, 
		(byte) 0x68, (byte) 0x61, (byte) 0x6e, (byte) 0x43, 
		(byte) 0x6c, (byte) 0x61, (byte) 0x73, (byte) 0x73, 
		(byte) 0x01, (byte) 0x00, (byte) 0x10, (byte) 0x6a, 
		(byte) 0x61, (byte) 0x76, (byte) 0x61, (byte) 0x2f, 
		(byte) 0x6c, (byte) 0x61, (byte) 0x6e, (byte) 0x67, 
		(byte) 0x2f, (byte) 0x4f, (byte) 0x62, (byte) 0x6a, 
		(byte) 0x65, (byte) 0x63, (byte) 0x74, (byte) 0x00, 
		(byte) 0x21, (byte) 0x00, (byte) 0x02, (byte) 0x00, 
		(byte) 0x03, (byte) 0x00, (byte) 0x00, (byte) 0x00, 
		(byte) 0x00, (byte) 0x00, (byte) 0x01, (byte) 0x00, 
		(byte) 0x01, (byte) 0x00, (byte) 0x04, (byte) 0x00, 
		(byte) 0x05, (byte) 0x00, (byte) 0x01, (byte) 0x00, 
		(byte) 0x06, (byte) 0x00, (byte) 0x00, (byte) 0x00, 
		(byte) 0x1d, (byte) 0x00, (byte) 0x01, (byte) 0x00, 
		(byte) 0x01, (byte) 0x00, (byte) 0x00, (byte) 0x00, 
		(byte) 0x05, (byte) 0x2a, (byte) 0xb7, (byte) 0x00, 
		(byte) 0x01, (byte) 0xb1, (byte) 0x00, (byte) 0x00, 
		(byte) 0x00, (byte) 0x01, (byte) 0x00, (byte) 0x07, 
		(byte) 0x00, (byte) 0x00, (byte) 0x00, (byte) 0x06, 
		(byte) 0x00, (byte) 0x01, (byte) 0x00, (byte) 0x00, 
		(byte) 0x00, (byte) 0x01, (byte) 0x00, (byte) 0x01, 
		(byte) 0x00, (byte) 0x08, (byte) 0x00, (byte) 0x00, 
		(byte) 0x00, (byte) 0x02, (byte) 0x00, (byte) 0x09
	};
}
