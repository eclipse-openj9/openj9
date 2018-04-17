/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
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

package org.openj9.test.contendedfields;

import org.testng.log4testng.Logger;

	@SuppressWarnings("nls")
public class Crypto  extends SuperCrypto  {
	static final long serialVersionUID = 99;
	public java.math.BigInteger x;
	public static final java.lang.String z = "z";
	protected static Logger logger = Logger.getLogger(ContendedFieldsTests.class);

	public static void main(String args[]) {
		Crypto myCrypto = new Crypto();
		myCrypto.algid = null;
		myCrypto.attributes = null;
		myCrypto.key = null;
		myCrypto.provider = null;
		myCrypto.x = null;
		logger.debug("Finished main()");
	}
}
