/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

package com.ibm.jvmti.tests.nestMatesRedefinition;

import org.objectweb.asm.Attribute;
import org.objectweb.asm.ByteVector;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Label;

/*
 * Nestmates attributes are a future java feature and, as such, the ASM6 API
 * does not provide built-in methods for them. However, ASM6 allows the
 * subclassing and use of its Attribute class. This can be leveraged to create
 * a NestHostAttribute in order to write nest host attribute data.
 * 
 * Documentation on the ASM6 attribute class can be found here:
 * http://asm.ow2.org/asm60/javadoc/user/org/objectweb/asm/Attribute.html
 * Nestmates utilities are expected to be available in ASM7.
 * 
 * Nest hosts attributes have the format:
 *		NestHost_attribute { 
 *			u2 attribute_name_index; 
 *			u4 attribute_length;
 *			u2 host_class_index;
 *		}
 *
 * Note that the spec explicitly disallows having more than one nestmates
 * attribute (nest host attribute or nest members attribute) but that use
 * of this workaround permits defining multiple nest attributes for a
 * class.
 */
final class NestHostAttribute extends Attribute {
	private String nestHost;

	public Label[] getLabels(){
		return null;
	}
	
	public NestHostAttribute(String nestTop){
		super("NestHost");
		this.nestHost = nestTop;
	}
	
	public boolean isCodeAttribute(){
		return false;
	}

	public boolean isUnknown(){
		return true;
	}
	
	public ByteVector write(ClassWriter cw, byte[] code, int len, int maxStack, int maxLocals){
		int topclass_index = cw.newClass(nestHost);		
		ByteVector b = new ByteVector();
		b.putShort(topclass_index);
		return b;
	}
}
