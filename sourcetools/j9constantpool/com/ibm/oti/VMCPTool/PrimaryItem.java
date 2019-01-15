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
import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.StringTokenizer;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

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
			for (int i = 0; tok.hasMoreTokens(); ++i) {
				attributes[i] = tok.nextToken();
			}
			return attributes;
		} else {
			return null;
		}
	}

	protected static VersionRange[] versions(Element e, Alias proto) {
		VersionRange[] ranges;
		String[] versions = attribute(e, "versions");

		if (versions != null) {
			int length = versions.length;

			ranges = new VersionRange[length];

			for (int i = 0; i < length; ++i) {
				ranges[i] = VersionRange.parse(versions[i]);
			}
		} else if (proto != null) {
			ranges = proto.versions;
		} else {
			ranges = VersionRange.ALL;
		}

		return ranges;
	}

	private static final String[] NO_FLAGS = new String[0];

	protected static String[] flags(Element e, Alias proto) {
		String[] flags = attribute(e, "flags");
		if (flags == null) {
			flags = proto != null ? proto.flags : NO_FLAGS;
		}
		return flags;
	}

	protected static String attribute(Element e, String name, String ifAbsent) {
		return e.hasAttribute(name) ? e.getAttribute(name) : ifAbsent;
	}

	protected static final class VersionRange {

		protected static final VersionRange[] ALL = new VersionRange[] { new VersionRange(0, Integer.MAX_VALUE) };

		private static final Pattern PATTERN = Pattern.compile("(\\d+)(-(\\d*))?");

		protected static VersionRange parse(String rangeText) {
			Matcher matcher = PATTERN.matcher(rangeText);

			if (matcher.matches()) {
				try {
					int min = Integer.parseInt(matcher.group(1));
					String maxText = matcher.group(3);
					int max;

					if (maxText == null) {
						max = min;
					} else if (maxText.isEmpty()) {
						max = Integer.MAX_VALUE;
					} else {
						max = Integer.parseInt(maxText);
					}

					return new VersionRange(min, max);
				} catch (NumberFormatException e) {
					// throw IllegalArgumentException below
				}
			}

			throw new IllegalArgumentException();
		}

		private final int min;

		private final int max;

		private VersionRange(int min, int max) {
			super();
			this.min = min;
			this.max = max;
		}

		protected boolean includes(int version) {
			return min <= version && version <= max;
		}

	}

	protected static class Alias {

		static interface Factory {
			Alias alias(Element e, Alias proto);
		}

		final VersionRange[] versions;
		final String[] flags;
		static final Alias DEFAULT = new Alias(null, null);

		Alias(VersionRange[] versions, String[] flags) {
			this.versions = versions;
			this.flags = flags;
		}

		protected boolean isIncluded() {
			return false;
		}

		void writeSecondaryItems(ConstantPoolStream ds) {
			// do nothing
		}

		void write(ConstantPoolStream ds) {
			ds.writeInt(0);
			ds.writeInt(0);
		}

		private boolean matchesVersion(int version) {
			for (VersionRange range : this.versions) {
				if (range.includes(version)) {
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
		
		AliasWithClass(VersionRange[] versions, String[] flags, ClassRef classRef) {
			super(versions, flags);
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
		// Look for an alias that explicitly supports the version and a flag
		for (Alias alias : aliases) {
			if (alias.matchesVersion(ds.version) && alias.hasFlag(ds.flags)) {
				return alias;
			}
		}

		// Look for an alias that explicitly supports either the version or a flag
		for (Alias alias : aliases) {
			// Check if the alias explicitly supports the version and has no flags
			if (alias.matchesVersion(ds.version) && alias.flags.length == 0) {
				// Check that the primary alias supports a flag
				if (primary.hasFlag(ds.flags) || primary.flags.length == 0) {
					return alias;
				}
			}
			// Check if the alias implicitly supports the version and has a flag
			if (alias.versions.length == 0 && alias.hasFlag(ds.flags)) {
				// Check that the primary alias supports the version
				if (primary.matchesVersion(ds.version) || primary.versions.length == 0) {
					return alias;
				}
			}
		}

		// Check that the primary alias supports the version and flags
		if (primary.matchesVersion(ds.version) || primary.versions.length == 0) {
			if (primary.hasFlag(ds.flags) || primary.flags.length == 0) {
				// Look for an alias that implicitly supports the version and flags
				for (Alias alias : aliases) {
					if (alias.versions.length == 0 && alias.flags.length == 0) {
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
		out.println("#define J9VMCONSTANTPOOL_" + cMacroName() + " " + pool.getIndex(this));
	}

	public String commentText() {
		return getClass().getName();
	}

}
