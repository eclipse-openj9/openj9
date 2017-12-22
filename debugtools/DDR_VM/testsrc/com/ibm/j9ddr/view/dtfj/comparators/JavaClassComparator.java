/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.view.dtfj.comparators;

import static org.junit.Assert.fail;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaField;
import com.ibm.dtfj.java.JavaMethod;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaReference;
import com.ibm.j9ddr.view.dtfj.test.DTFJComparator;

public class JavaClassComparator extends DTFJComparator {

	public static final int CLASS_LOADER = 1;
	public static final int COMPONENT_TYPE = 2;
	public static final int CONSTANT_POOL_REFERENCES = 4;
	public static final int DECLARED_FIELDS = 8;
	public static final int DECLARED_METHODS = 16;
	public static final int ID = 32;
	public static final int INTERFACES = 64;
	public static final int MODIFIERS = 128;
	public static final int NAME = 256;
	public static final int OBJECT = 512;
	public static final int REFERENCES = 1024;
	public static final int SUPERCLASS = 2048;
	public static final int IS_ARRAY = 4096;
	
	public int getDefaultMask() {
		return ID | NAME;
	}
	
	// getClassLoader()
	// getComponentType()
	// getConstantPoolReferences()
	// getDeclaredFields()
	// getDeclaredMethods()
	// getID()
	// getInterfaces()
	// getModifiers()
	// getName()
	// getObject()
	// getReferences()
	// getSuperclass()
	// isArray()
	public void testEquals(Object ddrObject, Object jextractObject, int members) {
		JavaClass ddrJavaClass = (JavaClass) ddrObject;
		JavaClass jextractJavaClass = (JavaClass) jextractObject;
		
		JavaObjectComparator javaObjectComparator = new JavaObjectComparator();
		
		// getClassLoader()
		if ((CLASS_LOADER & members) != 0)
			new JavaClassLoaderComparator().testComparatorEquals(ddrJavaClass, jextractJavaClass, "getClassLoader");
		
		// isArray()
		if ((IS_ARRAY & members) != 0)
			testJavaEquals(ddrJavaClass, jextractJavaClass, "isArray");
		
		// getComponentType()
		if ((COMPONENT_TYPE & members) != 0) {
			try {
				if (ddrJavaClass.isArray()) {
					new JavaClassComparator().testComparatorEquals(ddrJavaClass, jextractJavaClass, "getComponentType");
				}
			} catch (CorruptDataException e) {
				fail("Problem comparing JavaClass");
			}
		}
		
		// getConstantPoolReferences()
		if ((CONSTANT_POOL_REFERENCES & members) != 0)
			javaObjectComparator.testComparatorIteratorEquals(ddrJavaClass, jextractJavaClass, "getConstantPoolReferences", JavaObject.class);

		// getDeclaredFields
		if ((DECLARED_FIELDS & members) != 0)
			new JavaFieldComparator().testComparatorIteratorEquals(ddrJavaClass, jextractJavaClass, "getDeclaredFields", JavaField.class);
		
		// getDeclaredMethods
		if ((DECLARED_METHODS & members) != 0)
			new JavaMethodComparator().testComparatorIteratorEquals(ddrJavaClass, jextractJavaClass, "getDeclaredMethods", JavaMethod.class);
		
		// getID()
		if ((ID & members) != 0)
			new ImagePointerComparator().testComparatorEquals(ddrJavaClass, jextractJavaClass, "getID");
		
		// getInterfaces()
		if ((INTERFACES & members) != 0)
			testComparatorIteratorEquals(ddrJavaClass, jextractJavaClass, "getInterfaces", String.class);
		
		// getModifiers()
		if ((MODIFIERS & members) != 0)
			testJavaEquals(ddrJavaClass, jextractJavaClass, "getModifiers");
		
		// getName()
		if ((NAME & members) != 0) {
			testJavaEquals(ddrJavaClass, jextractJavaClass, "getName");
		}
		
		// getObject()
		if ((OBJECT & members) != 0)
			javaObjectComparator.testJavaEquals(ddrJavaClass, jextractJavaClass, "getObject");
		
		// getReferneces()
		if ((REFERENCES & members) != 0)
			new JavaReferenceComparator().testComparatorIteratorEquals(ddrJavaClass, jextractJavaClass, "getReferneces", JavaReference.class);
		
		// getSuperclass()
		if ((SUPERCLASS & members) != 0)
			testComparatorEquals(ddrJavaClass, jextractJavaClass, "getSuperclass");
	
	}
}
