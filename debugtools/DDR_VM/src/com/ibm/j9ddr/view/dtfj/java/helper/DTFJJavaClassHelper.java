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
package com.ibm.j9ddr.view.dtfj.java.helper;

import java.util.Iterator;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaField;
import com.ibm.dtfj.java.JavaObject;

/**
 * A helper class containing static methods that are used by implementations of DTFJJavaClass
 * 
 */

public class DTFJJavaClassHelper {
	private static final String PROTECTION_DOMAIN_FIELD_NAME = "protectionDomain";
	
	public static JavaObject getProtectionDomain(JavaClass clazz) throws CorruptDataException, DataUnavailable, MemoryAccessException {
		JavaField _protectionDomainField = getField(clazz.getObject().getJavaClass(), PROTECTION_DOMAIN_FIELD_NAME);
		if(_protectionDomainField == null) {
			throw new DataUnavailable("Protection domain information is not available");
		}
		return (JavaObject) _protectionDomainField.get(clazz.getObject());
	}
	
	public static JavaField getField(JavaClass clazz, String name) throws CorruptDataException {
		Iterator<?> fieldsIt = clazz.getDeclaredFields();
		while(fieldsIt.hasNext()) {
			Object potential = fieldsIt.next();
			
			if(potential instanceof JavaField) {
				JavaField field = (JavaField) potential;
				
				if(field.getName().equals(name)) {
					return field;
				}
			}
		}
		return null;
	}
}
