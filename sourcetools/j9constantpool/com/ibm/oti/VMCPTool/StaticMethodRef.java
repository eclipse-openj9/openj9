/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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

public class StaticMethodRef extends PrimaryItem implements Constants {
	private static class Alias extends PrimaryItem.AliasWithClass {
		final NameAndSignature nas;
		
		Alias(String[] jcl, String[] flags, ClassRef classRef, NameAndSignature nas) {
			super(jcl, flags, classRef);
			this.nas = nas;
		}

		void writeSecondaryItems(ConstantPoolStream ds) {
			if (checkClassForWriteSecondary(ds)) {
				ds.writeSecondaryItem(nas);
			}
		}
		
		void write(ConstantPoolStream ds) {
			if (checkClassForWrite(ds)) {
				ds.alignTo(4);
				ds.markStaticMethod();
				ds.writeInt( ds.getIndex(classRef) );
				ds.writeInt( ds.getOffset(nas) - ds.getOffset() );
			}
		}
	}
	
	private static class Factory implements Alias.Factory {
		private Map classes;
		private ClassRef classRef;

		Factory(Map classes) {
			this.classes = classes;
			this.classRef = null;
		}
		
		public PrimaryItem.Alias alias(Element e, PrimaryItem.Alias proto) {
			Alias p = (Alias) proto;
			return new Alias(
				jcl(e, p),
				flags(e, p),
				classRef(e),
				new NameAndSignature(
						attribute(e, "name", p != null ? p.nas.name.data : ""),
						attribute(e, "signature", p != null ? p.nas.signature.data : "")));
		}
		
		private ClassRef classRef(Element e) {
			String name = attribute(e, "class", null);
			if (name == null) {
				return classRef;
			}
			if (classRef == null) {
				classRef = (ClassRef) classes.get(name);
			}
			return (ClassRef) classes.get(name);
		}
	}

	public StaticMethodRef(Element e, Map classes) {
		super(e, STATICMETHODREF, new Factory(classes));
	}
	
	protected String cMacroName() {
		return ((Alias) primary).classRef.cMacroName() + "_" + ((Alias) primary).nas.name.data.toUpperCase();
	}
	
	public void writeMacros(ConstantPool pool, PrintWriter out) {
		super.writeMacros(pool, out);
		String macroName = cMacroName();
		out.println("#define J9VM" + macroName + "_REF(vm) J9VMCONSTANTPOOL_STATICMETHODREF_AT(vm, J9VMCONSTANTPOOL_" + macroName + ")");
		out.println("#define J9VM" + macroName + "_METHOD(vm) J9VMCONSTANTPOOL_STATICMETHOD_AT(vm, J9VMCONSTANTPOOL_" + macroName + ")");
	}
	
	public String commentText() {
		Alias alias = (Alias) primary;
		return "StaticMethodRef[" + alias.classRef.getClassName() + "." + alias.nas.name.data +  alias.nas.signature.data + "]";
	}	
	
}
