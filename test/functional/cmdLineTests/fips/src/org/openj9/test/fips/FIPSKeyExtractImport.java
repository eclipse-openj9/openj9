/*
 * Copyright IBM Corp. and others 2022
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package org.openj9.test.fips;

import java.math.BigInteger;

import java.security.interfaces.RSAPrivateCrtKey;
import java.security.interfaces.RSAPublicKey;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.NoSuchAlgorithmException;
import java.security.NoSuchProviderException;
import java.security.spec.RSAPublicKeySpec;
import java.security.SecureRandom;
import java.util.Base64;

import javax.crypto.Cipher;
import javax.crypto.NoSuchPaddingException;
import javax.crypto.spec.IvParameterSpec;
import javax.crypto.spec.SecretKeySpec;
import javax.crypto.SecretKey;
import javax.crypto.SecretKeyFactory;
import javax.crypto.spec.DESedeKeySpec;

public class FIPSKeyExtractImport {
    private static final String key =        "78BCC9D7998EF944C69AE4E066376CE1";
    private static final String initVector = "ij093kj4309d43jf";

    public static void main(String[] args) throws NoSuchAlgorithmException, NoSuchProviderException {
        String test = args[0];
        switch(test) {
        case "encrypt":
            encrypt("TEST");
            break;
        case "extractkey":
            extractkey();
            break;
        default:
            throw new RuntimeException("incorrect parameters");
        }
    }

    public static String encrypt(String value) {
        try {
            IvParameterSpec iv = new IvParameterSpec(initVector.getBytes("UTF-8"));
            SecretKeySpec skeySpec = new SecretKeySpec(key.getBytes("UTF-8"), "AES");

            Cipher cipher = Cipher.getInstance("AES/CBC/PKCS5PADDING", "SunPKCS11-NSS-FIPS");
            cipher.init(Cipher.ENCRYPT_MODE, skeySpec, iv);

            byte[] encrypted = cipher.doFinal(value.getBytes());
            System.out.println("Import secret key in FIPS mode test COMPLETED");
            return Base64.getEncoder().encodeToString(encrypted);
        } catch (NoSuchAlgorithmException | NoSuchPaddingException | NoSuchProviderException e) {
            System.out.println("Import secret key in FIPS mode test FAILED");
            System.out.println(e.toString());
        } catch (Exception ex) {
            System.out.println("Import secret key in FIPS mode test FAILED");
            ex.printStackTrace();
        }
        return null;
    }

    public static String extractkey() {
        try {
            KeyPair pair = null;
            KeyPairGenerator keyGen = null;
            SecureRandom random = null;

            keyGen = KeyPairGenerator.getInstance("RSA");

            random = new SecureRandom();
            int len = 512;
            keyGen.initialize(len * 8, random);

            pair = keyGen.generateKeyPair();
            RSAPublicKey rsaPubKey = (RSAPublicKey) pair.getPublic();
            RSAPrivateCrtKey rsaPrivKey = (RSAPrivateCrtKey) pair.getPrivate();

            BigInteger pe = rsaPrivKey.getPrivateExponent();
            System.out.println(pe);
            System.out.println("Extract RSA keys in FIPS mode test COMPLETED");
            return "";
        } catch (Exception ex) {
            System.out.println("Extract RSA keys in FIPS mode test FAILED");
            ex.printStackTrace();
        }
        return null;
    }
}
