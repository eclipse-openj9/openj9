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

class CharacterGroup implements Group {
	private String _chars;
	private boolean _inverted;
	private boolean _caseSensitive;
	
	public CharacterGroup( String text, boolean caseSensitive ) {
		_inverted = (text.charAt(0) == '^');
		_chars = (_inverted ? text.substring(1) : text);
		_caseSensitive = caseSensitive;
		if (! caseSensitive) {
			_chars = _chars.toLowerCase();
		}
		
		int dash = -1;
		do {
			dash = _chars.indexOf( '-', dash + 1);
			if (dash > 0 && dash < _chars.length() - 1 && _chars.charAt(dash - 1) != '\\') {
				char first = _chars.charAt(dash - 1);
				char last = _chars.charAt(dash + 1);
				StringBuffer set = new StringBuffer();
				if (first > last) {
					for (char c = last; c <= first; c++) {
						set.append(c);
					}
				} else {
					for (char c = first; c <= last; c++) {
						set.append(c);
					}
				}
				_chars = _chars.substring(0, dash - 1) + set.toString() + _chars.substring( dash + 2 );
				dash += set.length() - 1;
			}
		} while (dash >= 0);
	}
	
	public int consumes(String text) {
		if (text == null || text.length() == 0) {
			return 0;
		}
		char c = text.charAt(0);
		if (! _caseSensitive) {
			c = Character.toLowerCase(c);
		}
		int index = _chars.indexOf(c);
		return (_inverted ? index < 0 : index >= 0) ? 1 : 0;
	}
	
	public String toString() {
		return '[' + (_inverted ? "^" : "") + _chars + "]";
	}
}
