/*******************************************************************************
 * Copyright (c) 2004, 2019 IBM Corp. and others
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
package com.ibm.oti.VMCPTool;

import java.io.PrintWriter;
import java.util.Map;

import org.w3c.dom.Element;

public class StaticFieldRef extends FieldRef implements Constants {
	private static class Alias extends FieldRef.Alias {
		Alias(VersionRange[] versions, String[] flags, ClassRef classRef, NameAndSignature nas, String cast) {
			super(versions, flags, classRef, nas, cast);
		}

		void write(ConstantPoolStream ds) {
			if (checkClassForWrite(ds)) {
				ds.alignTo(4);
				ds.markStaticField();
				ds.writeInt( ds.getIndex(classRef) );
				ds.writeInt( ds.getOffset(nas) - ds.getOffset() );
			}
		}
	}
	
	private static class Factory extends FieldRef.Factory {

		Factory(Map classes) {
			super(classes);
		}
		
		public PrimaryItem.Alias alias(Element e, PrimaryItem.Alias proto) {
			Alias p = (Alias)proto;
			return new Alias(
				versions(e, p),
				flags(e, p),
				classRef(e),
				new NameAndSignature(
						attribute(e, "name", p != null ? p.nas.name.data : ""),
						attribute(e, "signature", p != null ? p.nas.signature.data : "")),
				attribute(e, "cast", p != null ? p.cast : ""));
		}
		
	}

	public StaticFieldRef(Element e, Map classes) {
		super(e, FIELDALIAS, new Factory(classes));
	}
		
	public void writeMacros(ConstantPool pool, PrintWriter out) {
		superWriteMacros(pool, out);
		String type = fieldType();
		String cast = ((FieldRef.Alias) primary).cast;
		String castTo = cast.length() == 0 ? "" : "(" + cast + ")";
		String macroName = cMacroName();
		String fieldOffset = "J9VMCONSTANTPOOL_STATICFIELD_ADDRESS(J9VMTHREAD_JAVAVM(vmThread), J9VMCONSTANTPOOL_" + macroName + ")";
		
		out.println("#define J9VM" + macroName + "_ADDRESS(vmThread) " + fieldOffset);
		out.println("#define J9VM" + macroName + "(vmThread, clazz) ((void)0, \\");
		out.println("\t" + castTo + "J9STATIC_" + type + "_LOAD(vmThread, clazz, J9VM" + macroName + "_ADDRESS(vmThread)))");
		out.println("#define J9VM" + cSetterMacroName() + "(vmThread, clazz, value) ((void)0, \\");
		out.println("\tJ9STATIC_" + type + "_STORE(vmThread, clazz, J9VM" + macroName + "_ADDRESS(vmThread), (value)))");
		
		/* Generate a second set of macros that take a J9JavaVM parameter instead of a J9VMThread */
		
		fieldOffset = "J9VMCONSTANTPOOL_STATICFIELD_ADDRESS_VM(javaVM, J9VMCONSTANTPOOL_" + macroName + ")";
		
		out.println("#define J9VM" + macroName + "_ADDRESS_VM(javaVM) " + fieldOffset);
		out.println("#define J9VM" + macroName + "_VM(javaVM, clazz) ((void)0, \\");
		out.println("\t" + castTo + "J9STATIC_" + type + "_LOAD_VM(javaVM, clazz, J9VM" + macroName + "_ADDRESS_VM(javaVM)))");
		out.println("#define J9VM" + cSetterMacroName() + "_VM(javaVM, clazz, value) ((void)0, \\");
		out.println("\tJ9STATIC_" + type + "_STORE_VM(javaVM, clazz, J9VM" + macroName + "_ADDRESS_VM(javaVM), (value)))");
		
	}
}
