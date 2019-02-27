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

public class FieldRef extends PrimaryItem implements Constants {
	protected static class Alias extends PrimaryItem.AliasWithClass {
		final NameAndSignature nas;
		final String cast;

		Alias(VersionRange[] versions, String[] flags, ClassRef classRef, NameAndSignature nas, String cast) {
			super(versions, flags, classRef);
			this.nas = nas;
			this.cast = cast;
		}

		void writeSecondaryItems(ConstantPoolStream ds) {
			if (checkClassForWriteSecondary(ds)) {
				ds.writeSecondaryItem(nas);
			}
		}

		void write(ConstantPoolStream ds) {
			if (checkClassForWrite(ds)) {
				ds.alignTo(4);
				ds.markInstanceField();
				ds.writeInt( ds.getIndex(classRef) );
				ds.writeInt( ds.getOffset(nas) - ds.getOffset() );
			}
		}
	}
	
	protected static class Factory implements Alias.Factory {
		private Map classes;
		private ClassRef classRef;

		Factory(Map classes) {
			this.classes = classes;
			this.classRef = null;
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
		
		protected ClassRef classRef(Element e) {
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

	public FieldRef(Element e, Map classes) {
		super(e, FIELDALIAS, new Factory(classes));
	}
	
	protected FieldRef(Element e, String nodeName, com.ibm.oti.VMCPTool.PrimaryItem.Alias.Factory factory) {
		super(e, nodeName, factory);
	}
	
	protected String cMacroName() {
		return ((Alias) primary).classRef.cMacroName() + "_" + ((Alias) primary).nas.name.data.toUpperCase();
	}
	
	protected String cSetterMacroName() {
		return ((Alias) primary).classRef.cMacroName() + "_SET_" + ((Alias) primary).nas.name.data.toUpperCase();
	}
	
	protected String fieldType() {
		// helpers are:
		//	j9gc_objaccess_mixedObjectReadI32
		//	j9gc_objaccess_mixedObjectReadU32
		//	j9gc_objaccess_mixedObjectReadI64
		//	j9gc_objaccess_mixedObjectReadU64
		//	j9gc_objaccess_mixedObjectReadObject
		//	j9gc_objaccess_mixedObjectReadAddress
		//	j9gc_objaccess_mixedObjectStoreI32
		//	j9gc_objaccess_mixedObjectStoreU32
		//	j9gc_objaccess_mixedObjectStoreI64
		//	j9gc_objaccess_mixedObjectStoreU64
		//	j9gc_objaccess_mixedObjectStoreObject
		//	j9gc_objaccess_mixedObjectStoreAddress
		//	j9gc_objaccess_mixedObjectStoreU64Split
		
		// Arrays and objects take precedence over cast to support pointer compression
		switch (((Alias) primary).nas.signature.data.charAt(0)) {
		case '[':
		case 'L':
			return "OBJECT";
		default:
			// Do nothing
		}
		
		// The cast then has first dibs to determine the field type
		String cast = ((Alias) primary).cast;
		if (cast.length() > 0) {
			if (cast.indexOf('*') >= 0) {
				return "ADDRESS";
			} else if ("I_64".equals(cast)) {
				return "I64";
			} else if ("I_32".equals(cast)) {
				return "I32";
			} else if ("U_64".equals(cast)) {
				return "U64";
			} else if ("U_32".equals(cast)) {
				return "U32";
			} else if ("UDATA".equals(cast)) {
				return "UDATA";
			} else {
				throw new UnsupportedOperationException("Unrecognized cast: " + cast);
			}
		}
		
		// Otherwise just determine it from the signature
		switch (((Alias) primary).nas.signature.data.charAt(0)) {
		case '[':
		case 'L':
			return "OBJECT";
		case 'J':
			return "I64";
		case 'D':
			throw new UnsupportedOperationException("Double fields not supported by memory manager functions");
		case 'F':
			throw new UnsupportedOperationException("Float fields not supported by memory manager functions");
		default:
			return "U32";
		}
	}
	
	public void writeMacros(ConstantPool pool, PrintWriter out) {
		super.writeMacros(pool, out);
		String type = fieldType();
		String cast = ((Alias) primary).cast;
		String castTo = cast.length() == 0 ? "" : "(" + cast + ")";
		String macroName = cMacroName();
		String fieldOffset = "J9VMCONSTANTPOOL_FIELD_OFFSET(J9VMTHREAD_JAVAVM(vmThread), J9VMCONSTANTPOOL_" + macroName + ")";
		if (type.equals("ADDRESS")) {
			fieldOffset = "J9VMCONSTANTPOOL_ADDRESS_OFFSET(J9VMTHREAD_JAVAVM(vmThread), J9VMCONSTANTPOOL_" + macroName + ")";
		}
		
		out.println("#define J9VM" + macroName + "_OFFSET(vmThread) " + fieldOffset);
		out.println("#define J9VM" + macroName + "(vmThread, object) ((void)0, \\");
		out.println("\t" + castTo + "J9OBJECT_" + type + "_LOAD(vmThread, object, J9VM" + macroName + "_OFFSET(vmThread)))");
		out.println("#define J9VM" + cSetterMacroName() + "(vmThread, object, value) ((void)0, \\");
		out.println("\tJ9OBJECT_" + type + "_STORE(vmThread, object, J9VM" + macroName + "_OFFSET(vmThread), (value)))");
		
		/* Generate a second set of macros that take a J9JavaVM parameter instead of a J9VMThread */
		
		fieldOffset = "J9VMCONSTANTPOOL_FIELD_OFFSET(javaVM, J9VMCONSTANTPOOL_" + macroName + ")";
		if (type.equals("ADDRESS")) {
			fieldOffset = "J9VMCONSTANTPOOL_ADDRESS_OFFSET(javaVM, J9VMCONSTANTPOOL_" + macroName + ")";
		}
		
		out.println("#define J9VM" + macroName + "_OFFSET_VM(javaVM) " + fieldOffset);
		out.println("#define J9VM" + macroName + "_VM(javaVM, object) ((void)0, \\");
		out.println("\t" + castTo + "J9OBJECT_" + type + "_LOAD_VM(javaVM, object, J9VM" + macroName + "_OFFSET_VM(javaVM)))");
		out.println("#define J9VM" + cSetterMacroName() + "_VM(javaVM, object, value) ((void)0, \\");
		out.println("\tJ9OBJECT_" + type + "_STORE_VM(javaVM, object, J9VM" + macroName + "_OFFSET_VM(javaVM), (value)))");
		
	}
	
	protected void superWriteMacros(ConstantPool pool, PrintWriter out) {
		super.writeMacros(pool, out);
	}
	
	public String commentText() {
		Alias alias = (Alias) primary;
		return "FieldRef[" + alias.classRef.getClassName() + "." + alias.nas.name.data + " " + alias.nas.signature.data + "]";
	}	
}
