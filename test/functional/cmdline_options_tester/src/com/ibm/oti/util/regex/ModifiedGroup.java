/*******************************************************************************
 * Copyright (c) 2004, 2018 IBM Corp. and others
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
package com.ibm.oti.util.regex;

import java.util.*;

public class ModifiedGroup {
	private static final boolean DEBUG = false;

	public static final int MODIFIER_ZERO_MORE = 42; // ascii for *
	public static final int MODIFIER_ONE_MORE = 43;	// ascii for +
	public static final int MODIFIER_GREEDY = 63; // ascii for ?
	
	private Group _group;
	private int _modifier;
	
	public ModifiedGroup( Group group ) {
		this( group, ' ' );
	}
	
	public ModifiedGroup( Group group, char modifier ) {
		_group = group;
		_modifier = modifier;
		if (DEBUG) System.out.println("ModifiedGroup::ModifiedGroup(" + group + "," + modifier + ")");
	}
	
	public ArrayList consume( String text ) {
		ArrayList al = new ArrayList();
		
		int c;
		switch (_modifier) {
			case MODIFIER_ONE_MORE:
				if ((c = _group.consumes(text)) <= 0) {
					break;
				}
				text = text.substring(c);
				// fall thru
			case MODIFIER_ZERO_MORE:
				if (DEBUG) System.out.println("ModifiedGroup::consume::1 adding " + text);
				al.add(text);
				while ((c = _group.consumes(text)) > 0) {
					text = text.substring(c);
					if (DEBUG) System.out.println("ModifiedGroup::consume::2 adding " + text);
					al.add(text);
				}
				break;
			case MODIFIER_GREEDY:
				while ((c = _group.consumes(text)) > 0) {
					text = text.substring(c);
				}
				if (DEBUG) System.out.println("ModifiedGroup::consume::3 adding " + text);
				al.add(text);
				break;
			default:
				c = _group.consumes(text);
				if (c > 0) {
					text = text.substring(c);
					if (DEBUG) System.out.println("ModifiedGroup::consume::4 adding " + text);
					al.add( text );
				}
				break;
		}
		
		return al;
	}
}
