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
package org.openj9.test.invoker.util;

import java.util.Hashtable;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.MethodVisitor;

public class InterfaceGenerator extends ClassGenerator {

	public InterfaceGenerator(String directory, String pkgName) {
		super(directory, pkgName);
	}

	public InterfaceGenerator(String directory, String pkgName, Hashtable<String, Object> characteristics) {
		super(directory, pkgName, characteristics);
	}

	@Override
	public byte[] getClassData() throws Exception {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;

		className = packageName + "/" + INTERFACE_NAME;

		cw.visit(V1_8, ACC_PUBLIC + ACC_ABSTRACT + ACC_INTERFACE, className, null, "java/lang/Object", null);
		{
			boolean hasDefaultMethod = false;
			boolean hasStaticMethod = false;

			if (characteristics.get(INTF_HAS_DEFAULT_METHOD) != null) {
				hasDefaultMethod = (Boolean)characteristics.get(INTF_HAS_DEFAULT_METHOD);
			}
			if (characteristics.get(INTF_HAS_STATIC_METHOD) != null) {
				hasStaticMethod = (Boolean)characteristics.get(INTF_HAS_STATIC_METHOD);
			}

			int access = ACC_PUBLIC;
			if (!hasDefaultMethod && !hasStaticMethod) {
				access += ACC_ABSTRACT;
			}
			if (hasStaticMethod) {
				access += ACC_STATIC;
			}

			mv = cw.visitMethod(access, INTF_METHOD_NAME, INTF_METHOD_SIG, null, null);
			if (hasDefaultMethod || hasStaticMethod) {
				String retValue = null;
				String version = (String)characteristics.get(VERSION);
				mv.visitCode();
				if (hasDefaultMethod) {
					if (version == null) {
						retValue = "Default " + INTERFACE_NAME + "." + INTF_METHOD_NAME;
					} else {
						retValue = "Default " + INTERFACE_NAME + "_" + version + "." + INTF_METHOD_NAME;
					}
				} else {
					if (version == null) {
						retValue = "Static " + INTERFACE_NAME + "." + INTF_METHOD_NAME;
					} else {
						retValue = "Static " + INTERFACE_NAME + "_" + version + "." + INTF_METHOD_NAME;
					}
				}
				mv.visitLdcInsn(retValue);
				mv.visitInsn(ARETURN);
				mv.visitMaxs(1, 1);
			}
			mv.visitEnd();
		}
		cw.visitEnd();
		data = cw.toByteArray();
		writeToFile();

		return data;
	}

}
