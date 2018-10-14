/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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

@SuppressWarnings("unused")
public class SecurityClass {
	private static final long serialVersionUID = 0;
	private static boolean initOidTable = false;
	private static java.util.Map<java.lang.String, Object> oidTable = null;
	private java.lang.String provider;
	private Object algid;
	private java.security.AlgorithmParameters algParams;
	protected Object params;
	private static Object debug = null;
	private static java.lang.String className = null;
	private static final int[] MD2_data = null;
	private static final int[] MD5_data = null;
	private static final int[] SHA1_OIW_data = null;
	private static final int[] SHA256_data = null;
	private static final int[] SHA384_data = null;
	private static final int[] SHA512_data = null;
	private static final int[] SHA224_data = null;
	private static final int[] HmacSHA1_data = null;
	private static final int[] PasswordBasedMac_data = null;
	public static Object MD2_oid = null;
	public static Object MD5_oid = null;
	public static Object SHA_oid = null;
	public static Object SHA256_oid = null;
	public static Object SHA384_oid = null;
	public static Object SHA512_oid = null;
	public static Object SHA224_oid = null;
	public static Object HmacSHA1_oid = null;
	public static Object PasswordBasedMac_oid = null;
	private static final int[] DH_data = null;
	private static final int[] DH_PKIX_data = null;
	private static final int[] DSA_OIW_data = null;
	private static final int[] DSA_PKIX_data = null;
	private static final int[] RSA_data = null;
	private static final int[] RSAEncryption_data = null;
	public static Object DH_oid = null;
	public static Object DH_PKIX_oid = null;
	public static Object DSA_OIW_oid = null;
	public static Object DSA_oid = null;
	public static Object RSA_oid = null;
	public static Object RSAEncryption_oid = null;
	public static Object EC_oid = null;
	private static final int[] md2WithRSAEncryption_data = null;
	private static final int[] md5WithRSAEncryption_data = null;
	private static final int[] sha1WithRSAEncryption_data = null;
	private static final int[] sha1WithRSAEncryption_OIW_data = null;
	private static final int[] sha256WithRSAEncryption_data = null;
	private static final int[] sha384WithRSAEncryption_data = null;
	private static final int[] sha512WithRSAEncryption_data = null;
	private static final int[] sha224WithRSAEncryption_data = null;
	private static final int[] shaWithDSA_OIW_data = null;
	private static final int[] sha1WithDSA_OIW_data = null;
	private static final int[] sha224WithDSA_data = null;
	private static final int[] sha256WithDSA_data = null;
	private static final int[] dsaWithSHA1_PKIX_data = null;
	private static final int[] DESCBC_data = null;
	private static final int[] tripleDESCBC_data = null;
	private static final int[] RC2CBC_data = null;
	private static final int[] AES_data = null;
	private static final int[] AES128CBC_data = null;
	private static final int[] AES192CBC_data = null;
	private static final int[] AES256CBC_data = null;
	private static final int[] sha1WithECDSA_data = null;
	private static final int[] sha224WithECDSA_data = null;
	private static final int[] sha256WithECDSA_data = null;
	private static final int[] sha384WithECDSA_data = null;
	private static final int[] sha512WithECDSA_data = null;
	private static final int[] specifiedWithECDSA_data = null;
	private static final int[] RSAPSS_data = null;
	private static final int[] mgf1_data = null;
	private static final int[] ecoid_data = null;
	public static Object md2WithRSAEncryption_oid = null;
	public static Object md5WithRSAEncryption_oid = null;
	public static Object sha1WithRSAEncryption_oid = null;
	public static Object RSAPSS_oid = null;
	public static Object mgf1_oid = null;
	public static Object sha1WithRSAEncryption_OIW_oid = null;
	public static Object sha256WithRSAEncryption_oid = null;
	public static Object sha384WithRSAEncryption_oid = null;
	public static Object sha512WithRSAEncryption_oid = null;
	public static Object sha224WithRSAEncryption_oid = null;
	public static Object shaWithDSA_OIW_oid = null;
	public static Object sha1WithDSA_OIW_oid = null;
	public static Object sha1WithDSA_oid = null;
	public static Object sha256WithDSA_oid = null;
	public static Object sha224WithDSA_oid = null;
	public static Object DESCBC_oid = null;
	public static Object tripleDESCBC_oid = null;
	public static Object RC2CBC_oid = null;
	public static Object AES_oid = null;
	public static Object AES128CBC_oid = null;
	public static Object AES192CBC_oid = null;
	public static Object AES256CBC_oid = null;
	public static Object sha1WithECDSA_oid = null;
	public static Object sha224WithECDSA_oid = null;
	public static Object sha256WithECDSA_oid = null;
	public static Object sha384WithECDSA_oid = null;
	public static Object sha512WithECDSA_oid = null;
	public static Object specifiedWithECDSA_oid = null;

}
