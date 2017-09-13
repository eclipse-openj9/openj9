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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package com.ibm.oti.VMCPTool;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.StringTokenizer;

import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

public abstract class PrimaryItem {
	protected final Alias primary;
	private final Alias[] aliases;
	
	protected abstract String cMacroName();
	
	protected PrimaryItem(Element e, String nodeName, Alias.Factory factory) {
		primary = factory.alias(e, null);
		aliases = aliases(e, nodeName, primary, factory);
	}
	
	private static String[] attribute(Element e, String name) {
		StringTokenizer tok = new StringTokenizer(e.getAttribute(name), ",");
		if (tok.hasMoreTokens()) {
			String[] attributes = new String[tok.countTokens()];
			for (int i = 0; tok.hasMoreTokens(); i++) {
				attributes[i] = tok.nextToken();
			}
			return attributes;
		} else {
			return null;
		}
	}
	
	protected static String[] jcl(Element e, Alias proto) {
		String[] jcls = attribute(e, "jcl");
		if (jcls == null) {
			jcls = proto != null ? proto.jcl : new String[0];
		}
		return jcls;
	}
	
	protected static String[] flags(Element e, Alias proto) {
		String[] flags = attribute(e, "flags");
		if (flags == null) {
			flags = proto != null ? proto.flags : new String[0];
		}
		return flags;
	}
	
	protected static String attribute(Element e, String name, String ifAbsent) {
		return e.hasAttribute(name) ? e.getAttribute(name) : ifAbsent;
	}
	
	protected static class Alias {
		static interface Factory {
			Alias alias(Element e, Alias proto);
		}
		
		final String[] jcl;
		final String[] flags;
		static final Alias DEFAULT = new Alias(null, null);
		
		Alias(String[] jcl, String[] flags) {
			this.jcl = jcl;
			this.flags = flags;
		}

		protected boolean isIncluded() {
			return false;
		}

		void writeSecondaryItems(ConstantPoolStream ds) {}
		
		void write(ConstantPoolStream ds) {
			ds.writeInt(0);
			ds.writeInt(0);
		}

		private boolean hasJCL(String jcl) {
			for (String s : this.jcl) {
				if (s.equals(jcl)) {
					return true;
				}
			}
			return false;
		}
		
		private boolean hasFlag(Set<String> flags) {
			for (String s : this.flags) {
				if (flags.contains(s)) {
					return true;
				}
			}
			return false;
		}
	}
	
	protected static class AliasWithClass extends Alias {
		final ClassRef classRef;
		private boolean classIncluded;
		
		AliasWithClass(String[] jcl, String[] flags, ClassRef classRef) {
			super(jcl, flags);
			this.classRef = classRef;
		}
		
		boolean checkClassForWriteSecondary(ConstantPoolStream ds) {
			if (!classIncluded) {
				super.writeSecondaryItems(ds);
			}
			return classIncluded;
		}

		boolean checkClassForWrite(ConstantPoolStream ds) {
			classIncluded = classRef.alias(ds).isIncluded();
			if (!classIncluded) {
				super.write(ds);
			}
			return classIncluded;
		}
	}
	
	private static Alias[] aliases(Element e, String nodeName, Alias proto, Alias.Factory factory) {
		NodeList nodes = e.getChildNodes();
		List<Alias> list = new ArrayList<Alias>();
		for (int i = 0; i < nodes.getLength(); i++) {
			Node node = nodes.item(i);
			if (node.getNodeType() == Node.ELEMENT_NODE && nodeName.equals(node.getNodeName())) {
				list.add(factory.alias((Element) node, proto));
			}
		}
		return list.toArray(new Alias[list.size()]);
	}
	
	Alias alias(ConstantPoolStream ds) {
		// Look for an alias that explicitly supports the JCL and a flag
		for (Alias alias : aliases) {
			if (alias.hasJCL(ds.jcl) && alias.hasFlag(ds.flags)) {
				return alias;
			}
		}
		
		// Look for an alias that explicitly supports either the JCL or a flag
		for (Alias alias : aliases) {
			// Check if the alias explicitly supports the JCL and has no flags
			if (alias.hasJCL(ds.jcl) && alias.flags.length == 0) {
				// Check that the primary alias supports a flag
				if (primary.hasFlag(ds.flags) || primary.flags.length == 0) {
					return alias;
				}
			}
			// Check if the alias implicitly supports the JCL and has a flag
			if (alias.jcl.length == 0 && alias.hasFlag(ds.flags)) {
				// Check that the primary alias supports the JCL
				if (primary.hasJCL(ds.jcl) || primary.jcl.length == 0) {
					return alias;
				}
			}
		}
		
		// Check that the primary alias supports the JCL and flags
		if (primary.hasJCL(ds.jcl) || primary.jcl.length == 0) {
			if (primary.hasFlag(ds.flags) || primary.flags.length == 0) {
				// Look for an alias that implicitly supports the JCL and flags
				for (Alias alias : aliases) {
					if (alias.jcl.length == 0 && alias.flags.length == 0) {
						return alias;
					}
				}

				return primary;
			}
		}

		// Return a default alias
		return Alias.DEFAULT;
	}
	
	public void write(ConstantPoolStream ds) {
		alias(ds).write(ds);
	}
	
	public void writeSecondaryItems(ConstantPoolStream ds) {
		alias(ds).writeSecondaryItems(ds);
	}

	public void writeMacros(ConstantPool pool, PrintWriter out) {
		out.println();
		out.println("#define J9VMCONSTANTPOOL_"	+ cMacroName() + " " + pool.getIndex(this));
	}
	
	public String commentText() {
		return getClass().getName();
	}

}
