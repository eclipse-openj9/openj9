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

import org.w3c.dom.Element;

public class ClassRef extends PrimaryItem implements Constants {
	private static class Alias extends PrimaryItem.Alias {
		final J9UTF8 name;
		
		Alias(VersionRange[] versions, String[] flags, J9UTF8 name) {
			super(versions, flags);
			this.name = name;
		}

		protected boolean isIncluded() {
			return true;
		}

		void writeSecondaryItems(ConstantPoolStream ds) {
			ds.writeSecondaryItem(name);
		}

		void write(ConstantPoolStream ds) {
			ds.alignTo(4);
			ds.markClass();
			ds.writeInt( ds.getOffset(name) - ds.getOffset() );

			Main.JCLRuntimeFlag flag;
			if (flags.length == 0) {
				flag = Main.getRuntimeFlag("default");
			} else {
				if (flags.length > 1) {
					System.err.println("Error: tool cannot handle complex flag expressions");
					System.exit(-1);
				}
				flag = Main.getRuntimeFlag( flags[0] );
			}
			ds.writeInt(flag.value);
		}
	}

	public ClassRef(Element e) {
		super(e, CLASSALIAS, new Alias.Factory() {
			public PrimaryItem.Alias alias(Element e, PrimaryItem.Alias proto) {
				Alias p = (Alias) proto;
				return new Alias(ClassRef.versions(e, p), ClassRef.flags(e, p),
						new J9UTF8(ClassRef.attribute(e, "name", p != null ? p.name.data : "")));
			}
		});
	}
	
	protected String cMacroName() {
		char[] buf = ((Alias) primary).name.data.toCharArray();
		int j = 0;

		for (int i = 0; i < buf.length; i++) {
			if (buf[i] != '/' && buf[i] != '$') {
				buf[j++] = buf[i];
			}
		}
		return new String(buf, 0, j).toUpperCase();
	}

	public void writeMacros(ConstantPool pool, PrintWriter out) {	
		super.writeMacros(pool, out);
		
		String macroName = cMacroName();
		out.println("#define J9VM" + macroName + "(vm) J9VMCONSTANTPOOL_CLASS_AT(vm, J9VMCONSTANTPOOL_" + macroName + ")");
		out.println("#define J9VM" + macroName + "_OR_NULL(vm) (J9VMCONSTANTPOOL_CLASSREF_AT(vm, J9VMCONSTANTPOOL_" + macroName + ")->value)");
	}
	
	public String getClassName() {
		return ((Alias) primary).name.data;
	}

	public String commentText() {
		return "ClassRef[" + getClassName() + "]";
	}
	
}
