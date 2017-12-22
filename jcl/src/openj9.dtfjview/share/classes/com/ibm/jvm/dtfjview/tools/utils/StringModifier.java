/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2012, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.tools.utils;


/**
 * This class is to be used together with pre-match handler, match handler 
 * and post-match handler. It receives strings from the OutputStreamModifier 
 * and delegates them to the above handlers according to the matching status.
 * <p>
 * @author Manqing Li, IBM.
 *
 */
public class StringModifier implements IStringModifier {

	public StringModifier(IPrematchHandle prematchHandle, IMatchHandle matchHandle, IPostmatchHandle postmatchHandle) {
		this.prematchHandle = prematchHandle;
		this.matchHandle = matchHandle;
		this.postmatchHandle = postmatchHandle;
		this.matched = false;
	}

	public String modify(String s) {
		boolean becomeMatched = matchHandle.matches(s);
		if (false == matched && false == becomeMatched) {
			prematchHandle.process(s);
			return "";
		} else if (false == matched && true == becomeMatched) {
			matched = true;
			postmatchHandle.justMatched();
			return prematchHandle.release() + matchHandle.process(s); 
		} else if (true == matched && false == becomeMatched) {
			return postmatchHandle.process(s);
		} else { // matched == true && becomeMatched == true
			postmatchHandle.justMatched();
			return matchHandle.process(s);
		}
	}
	private IPrematchHandle prematchHandle;
	private IMatchHandle matchHandle;
	private IPostmatchHandle postmatchHandle;
	private boolean matched;
}
