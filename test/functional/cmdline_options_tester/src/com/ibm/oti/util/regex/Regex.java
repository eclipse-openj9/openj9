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

public class Regex {
	private ModifiedGroup[] _groups;
	private boolean _caseSensitive;

	public Regex( String regex, boolean caseSensitive ) throws MalformedRegexException {
		_caseSensitive = caseSensitive;
		ArrayList al = new ArrayList();
		
		boolean escaped = false;
		StringBuffer buf = new StringBuffer();
		Group curGroup = null;
		for (int i = 0; i < regex.length(); i++) {
			char c = regex.charAt(i);
			if (escaped) {
				escaped = false;
				switch (c) {
					case 'd':
					case 'D':
					case 's':
					case 'S':
					case 'w':
					case 'W':
						if (buf.length() > 0) {
							curGroup = new TextGroup( buf.toString(), caseSensitive );
							buf.setLength(0);
						}
						if (curGroup != null) {
							al.add( new ModifiedGroup( curGroup ) );
							curGroup = null;
						}
						String charGroup = (Character.isUpperCase(c) ? "^" : "");
						if (Character.toLowerCase(c) == 'd') {
							charGroup += "0-9";
						} else if (Character.toLowerCase(c) == 's') {
							charGroup += " \t\n\f\r" + '\u000B';
						} else {
							charGroup += "a-zA-Z_0-9";
						}
						curGroup = new CharacterGroup( charGroup, caseSensitive );
						break;
					default:
						if (curGroup != null) {
							al.add( new ModifiedGroup( curGroup ) );
							curGroup = null;
						}
						int ch;
						switch (c) {
							case '\\':
								ch = '\\';
								break;
							case 't':
								ch = '\t';
								break;
							case 'n':
								ch = '\n';
								break;
							case 'r':
								ch = '\r';
								break;
							case 'f':
								ch = '\f';
								break;
							case 'a':
								ch = '\u0007';
								break;
							case 'e':
								ch = '\u001B';
								break;
							case 'x':
								try {
									ch = Integer.parseInt( regex.substring( i + 1, i + 3 ), 16 );
									i += 2;
								} catch (Exception e) {
									throw new MalformedRegexException( "The \\x at character " + i + " must be followed by two hex digits" );
								}
								break;
							case 'u':
								try {
									ch = Integer.parseInt( regex.substring( i + 1, i + 5), 16 );
									i += 4;
								} catch (Exception e) {
									throw new MalformedRegexException( "The \\u at character " + i + " must be followed by four hex digits" );
								}
								break;
							default:
								ch = c;
								break;
						}
						buf.append( (char)ch );
						break;
				}
				continue;
			}

			switch (c) {
				case '\\':
					escaped = true;
					break;
				case '+':
				case '*':
				case '?':
					if (buf.length() > 1) {
						al.add( new ModifiedGroup( new TextGroup( buf.substring(0, buf.length() - 1), caseSensitive ) ) );
						buf.delete(0, buf.length() - 1);
					}
					if (buf.length() > 0) {
						curGroup = new CharacterGroup( buf.toString(), caseSensitive );
						buf.setLength( 0 );
					}
					if (curGroup == null) {
						throw new MalformedRegexException( "Unexpected modifier " + c + " at character " + (i+1) );
					}
					al.add( new ModifiedGroup( curGroup, c ) );
					curGroup = null;
					break;
				case '.':
					if (buf.length() > 0) {
						curGroup = new TextGroup( buf.toString(), caseSensitive );
						buf.setLength( 0 );
					}
					if (curGroup != null) {
						al.add( new ModifiedGroup( curGroup ) );
						curGroup = null;
					}
					curGroup = new CharacterGroup( "^\n", caseSensitive );
					break;
				default:
					if (curGroup != null) {
						al.add( new ModifiedGroup( curGroup ) );
						curGroup = null;
					}
					buf.append( c );
					break;
			}
		}
		if (buf.length() > 0) {
			curGroup = new TextGroup( buf.toString(), caseSensitive );
		}
		if (curGroup != null) {
			al.add( new ModifiedGroup( curGroup ) );
		}
		
		_groups = (ModifiedGroup[])al.toArray( new ModifiedGroup[ al.size() ] );
	}
	
	public boolean matches( String text ) {
		TreeSet strings = new TreeSet();
		strings.add( text );
		for (int i = 0; i < _groups.length; i++) {
			TreeSet nextSet = new TreeSet();
			Iterator it = strings.iterator();
			while (it.hasNext()) {
				nextSet.addAll( _groups[i].consume( (String)it.next() ) );
			}
			strings = nextSet;
		}
		return (strings.contains(""));
	}
}
