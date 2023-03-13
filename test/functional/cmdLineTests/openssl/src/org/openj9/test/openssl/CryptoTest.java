/*******************************************************************************
 * Copyright IBM Corp. and others 2019
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

package org.openj9.test.openssl;

import java.util.Random;
import java.security.spec.AlgorithmParameterSpec;

import javax.crypto.Cipher;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;

public class CryptoTest {
	public static void main(String[] args) {
		try {
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            Random r = new Random(10);
            byte[] skey_bytes = new byte[16];
            r.nextBytes(skey_bytes);
            SecretKeySpec skey = new SecretKeySpec(skey_bytes, "AES");
            cipher.init(Cipher.ENCRYPT_MODE, skey);
			System.out.println("Crypto test COMPLETED");
		} catch (Exception e) {
			System.out.println("Crypto test FAILED");
			e.printStackTrace();
		}
	}
}