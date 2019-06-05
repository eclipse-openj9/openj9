package org.openj9.test.java.security;

/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

import java.security.spec.AlgorithmParameterSpec;
import java.util.Random;
import javax.crypto.Cipher;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.SecretKeySpec;
import java.security.InvalidKeyException;

import org.testng.log4testng.Logger;
import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity" })
public class OpenSSLInvalidGCMKeySizeTest {

    static Logger logger = Logger.getLogger(OpenSSLInvalidGCMKeySizeTest.class);

    @Test
    public static void testGCM() {
        final int len = 1024;
        final int tagLen = 16;
        // 15 bytes is an illegal key size
        final int key_size = 15;
        byte[] skey_bytes = new byte[key_size/8];
        SecretKeySpec skey = new SecretKeySpec(skey_bytes, "AES");
        byte[] data = new byte[len];
        byte[] tagBuffer = new byte[len + tagLen];
        byte[] iv = new byte[12];

        try {
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            AlgorithmParameterSpec iva = new GCMParameterSpec(tagLen * 8, iv);
            cipher.init(Cipher.ENCRYPT_MODE, skey, iva);
            cipher.doFinal(data, 0, data.length, tagBuffer);
            Assert.fail("InvalidKeyException is expected to be thrown");

        } catch (InvalidKeyException e) {
            logger.debug("InvalidKeyException thrown as expected");
            logger.debug(e);
        } catch (Exception e) {
            logger.error(e);
            Assert.fail("Unexpected exception thrown");
        }
    }
}
