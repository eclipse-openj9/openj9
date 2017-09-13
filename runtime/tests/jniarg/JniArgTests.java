/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

public class JniArgTests {

	final byte test_jbyte[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, };

	final short test_jshort[] = { 0x0101, 0x0102, 0x0103, 0x0104, 0x0105, 0x0106, 0x0107, 0x0108, 0x0109, 0x010A, 0x010B, 0x010C, 0x010D, 0x010E, 0x010F, 0x0110, };

	final int test_jint[] = { 0xBEEF0001, 0xBEEF0002, 0xBEEF0003, 0xBEEF0004, 0xBEEF0005, 0xBEEF0006, 0xBEEF0007, 0xBEEF0008, 0xBEEF0009, 0xBEEF000A, 0xBEEF000B, 0xBEEF000C, 0xBEEF000D, 0xBEEF000E, 0xBEEF000F, 0xBEEF0010, };

	final long test_jlong[] = {
		0xFEED0101FACE0001L,
		0xFEED0202FACE0002L,
		0xFEED0303FACE0003L,
		0xFEED0404FACE0004L,
		0xFEED0505FACE0005L,
		0xFEED0606FACE0006L,
		0xFEED0707FACE0007L,
		0xFEED0808FACE0008L,
		0xFEED0909FACE0009L,
		0xFEED0A0AFACE000AL,
		0xFEED0B0BFACE000BL,
		0xFEED0C0CFACE000CL,
		0xFEED0D0DFACE000DL,
		0xFEED0E0EFACE000EL,
		0xFEED0F0FFACE000FL,
		0xFEED1010FACE0010L,};

	final float test_jfloat[] = { 11.1f, 12.2f, 13.3f, 14.4f, 15.5f, 16.6f, 17.7f, 18.8f, 19.9f, 20.10f, 21.11f, 22.12f, 23.13f, 24.14f, 25.15f, 26.16f, };

	final double test_jdouble[] = { 11.1, 12.2, 13.3, 14.4, 15.5, 16.6, 17.7, 18.8, 19.9, 20.10, 21.11, 22.12, 23.13, 24.14, 25.15, 26.16, };

	public static void main(String[] args) {
		System.loadLibrary( "jniargtests" );
		JniArgTests jniArgTests = new JniArgTests();
		jniArgTests.testBlock();
		jniArgTests.summary();
	}

	void testBlock() {
		byte   retval_byte;
		short  retval_short;
		int    retval_int;
		long   retval_long;
		float  retval_float;
		double retval_double;



		retval_byte = nativeBBrB(  test_jbyte[1],  test_jbyte[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeBBrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeBBBBrB(  test_jbyte[1],  test_jbyte[2],  test_jbyte[3],  test_jbyte[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeBBBBrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeBSrB(  test_jbyte[1],  test_jshort[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeBSrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeBSBSrB(  test_jbyte[1],  test_jshort[2],  test_jbyte[3],  test_jshort[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeBSBSrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeBIrB(  test_jbyte[1],  test_jint[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeBIrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeBIBIrB(  test_jbyte[1],  test_jint[2],  test_jbyte[3],  test_jint[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeBIBIrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeBJrB(  test_jbyte[1],  test_jlong[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeBJrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeBJBJrB(  test_jbyte[1],  test_jlong[2],  test_jbyte[3],  test_jlong[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeBJBJrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeBFrB(  test_jbyte[1],  test_jfloat[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeBFrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeBFBFrB(  test_jbyte[1],  test_jfloat[2],  test_jbyte[3],  test_jfloat[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeBFBFrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeBDrB(  test_jbyte[1],  test_jdouble[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeBDrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeBDBDrB(  test_jbyte[1],  test_jdouble[2],  test_jbyte[3],  test_jdouble[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeBDBDrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeSBrB(  test_jshort[1],  test_jbyte[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeSBrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeSBSBrB(  test_jshort[1],  test_jbyte[2],  test_jshort[3],  test_jbyte[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeSBSBrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeSSrB(  test_jshort[1],  test_jshort[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeSSrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeSSSSrB(  test_jshort[1],  test_jshort[2],  test_jshort[3],  test_jshort[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeSSSSrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeSIrB(  test_jshort[1],  test_jint[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeSIrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeSISIrB(  test_jshort[1],  test_jint[2],  test_jshort[3],  test_jint[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeSISIrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeSJrB(  test_jshort[1],  test_jlong[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeSJrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeSJSJrB(  test_jshort[1],  test_jlong[2],  test_jshort[3],  test_jlong[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeSJSJrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeSFrB(  test_jshort[1],  test_jfloat[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeSFrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeSFSFrB(  test_jshort[1],  test_jfloat[2],  test_jshort[3],  test_jfloat[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeSFSFrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeSDrB(  test_jshort[1],  test_jdouble[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeSDrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeSDSDrB(  test_jshort[1],  test_jdouble[2],  test_jshort[3],  test_jdouble[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeSDSDrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeIBrB(  test_jint[1],  test_jbyte[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeIBrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeIBIBrB(  test_jint[1],  test_jbyte[2],  test_jint[3],  test_jbyte[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeIBIBrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeISrB(  test_jint[1],  test_jshort[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeISrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeISISrB(  test_jint[1],  test_jshort[2],  test_jint[3],  test_jshort[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeISISrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeIIrB(  test_jint[1],  test_jint[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeIIrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeIIIIrB(  test_jint[1],  test_jint[2],  test_jint[3],  test_jint[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeIIIIrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeIJrB(  test_jint[1],  test_jlong[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeIJrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeIJIJrB(  test_jint[1],  test_jlong[2],  test_jint[3],  test_jlong[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeIJIJrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeIFrB(  test_jint[1],  test_jfloat[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeIFrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeIFIFrB(  test_jint[1],  test_jfloat[2],  test_jint[3],  test_jfloat[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeIFIFrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeIDrB(  test_jint[1],  test_jdouble[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeIDrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeIDIDrB(  test_jint[1],  test_jdouble[2],  test_jint[3],  test_jdouble[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeIDIDrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeJBrB(  test_jlong[1],  test_jbyte[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeJBrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeJBJBrB(  test_jlong[1],  test_jbyte[2],  test_jlong[3],  test_jbyte[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeJBJBrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeJSrB(  test_jlong[1],  test_jshort[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeJSrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeJSJSrB(  test_jlong[1],  test_jshort[2],  test_jlong[3],  test_jshort[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeJSJSrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeJIrB(  test_jlong[1],  test_jint[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeJIrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeJIJIrB(  test_jlong[1],  test_jint[2],  test_jlong[3],  test_jint[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeJIJIrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeJJrB(  test_jlong[1],  test_jlong[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeJJrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeJJJJrB(  test_jlong[1],  test_jlong[2],  test_jlong[3],  test_jlong[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeJJJJrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeJFrB(  test_jlong[1],  test_jfloat[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeJFrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeJFJFrB(  test_jlong[1],  test_jfloat[2],  test_jlong[3],  test_jfloat[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeJFJFrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeJDrB(  test_jlong[1],  test_jdouble[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeJDrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeJDJDrB(  test_jlong[1],  test_jdouble[2],  test_jlong[3],  test_jdouble[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeJDJDrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeFBrB(  test_jfloat[1],  test_jbyte[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeFBrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeFBFBrB(  test_jfloat[1],  test_jbyte[2],  test_jfloat[3],  test_jbyte[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeFBFBrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeFSrB(  test_jfloat[1],  test_jshort[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeFSrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeFSFSrB(  test_jfloat[1],  test_jshort[2],  test_jfloat[3],  test_jshort[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeFSFSrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeFIrB(  test_jfloat[1],  test_jint[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeFIrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeFIFIrB(  test_jfloat[1],  test_jint[2],  test_jfloat[3],  test_jint[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeFIFIrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeFJrB(  test_jfloat[1],  test_jlong[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeFJrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeFJFJrB(  test_jfloat[1],  test_jlong[2],  test_jfloat[3],  test_jlong[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeFJFJrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeFFrB(  test_jfloat[1],  test_jfloat[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeFFrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeFFFFrB(  test_jfloat[1],  test_jfloat[2],  test_jfloat[3],  test_jfloat[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeFFFFrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeFDrB(  test_jfloat[1],  test_jdouble[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeFDrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeFDFDrB(  test_jfloat[1],  test_jdouble[2],  test_jfloat[3],  test_jdouble[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeFDFDrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeDBrB(  test_jdouble[1],  test_jbyte[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeDBrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeDBDBrB(  test_jdouble[1],  test_jbyte[2],  test_jdouble[3],  test_jbyte[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeDBDBrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeDSrB(  test_jdouble[1],  test_jshort[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeDSrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeDSDSrB(  test_jdouble[1],  test_jshort[2],  test_jdouble[3],  test_jshort[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeDSDSrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeDIrB(  test_jdouble[1],  test_jint[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeDIrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeDIDIrB(  test_jdouble[1],  test_jint[2],  test_jdouble[3],  test_jint[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeDIDIrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeDJrB(  test_jdouble[1],  test_jlong[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeDJrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeDJDJrB(  test_jdouble[1],  test_jlong[2],  test_jdouble[3],  test_jlong[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeDJDJrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeDFrB(  test_jdouble[1],  test_jfloat[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeDFrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeDFDFrB(  test_jdouble[1],  test_jfloat[2],  test_jdouble[3],  test_jfloat[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeDFDFrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeDDrB(  test_jdouble[1],  test_jdouble[2] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeDDrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_byte = nativeDDDDrB(  test_jdouble[1],  test_jdouble[2],  test_jdouble[3],  test_jdouble[4] );
		if ( retval_byte != test_jbyte[0] ) {
			logRetValError("Return value error:  nativeDDDDrB got: " + retval_byte + " expected: " + test_jbyte[0]);
		}


		retval_short = nativeBBrS(  test_jbyte[1],  test_jbyte[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeBBrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeBBBBrS(  test_jbyte[1],  test_jbyte[2],  test_jbyte[3],  test_jbyte[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeBBBBrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeBSrS(  test_jbyte[1],  test_jshort[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeBSrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeBSBSrS(  test_jbyte[1],  test_jshort[2],  test_jbyte[3],  test_jshort[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeBSBSrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeBIrS(  test_jbyte[1],  test_jint[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeBIrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeBIBIrS(  test_jbyte[1],  test_jint[2],  test_jbyte[3],  test_jint[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeBIBIrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeBJrS(  test_jbyte[1],  test_jlong[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeBJrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeBJBJrS(  test_jbyte[1],  test_jlong[2],  test_jbyte[3],  test_jlong[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeBJBJrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeBFrS(  test_jbyte[1],  test_jfloat[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeBFrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeBFBFrS(  test_jbyte[1],  test_jfloat[2],  test_jbyte[3],  test_jfloat[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeBFBFrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeBDrS(  test_jbyte[1],  test_jdouble[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeBDrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeBDBDrS(  test_jbyte[1],  test_jdouble[2],  test_jbyte[3],  test_jdouble[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeBDBDrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeSBrS(  test_jshort[1],  test_jbyte[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeSBrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeSBSBrS(  test_jshort[1],  test_jbyte[2],  test_jshort[3],  test_jbyte[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeSBSBrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeSSrS(  test_jshort[1],  test_jshort[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeSSrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeSSSSrS(  test_jshort[1],  test_jshort[2],  test_jshort[3],  test_jshort[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeSSSSrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeSIrS(  test_jshort[1],  test_jint[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeSIrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeSISIrS(  test_jshort[1],  test_jint[2],  test_jshort[3],  test_jint[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeSISIrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeSJrS(  test_jshort[1],  test_jlong[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeSJrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeSJSJrS(  test_jshort[1],  test_jlong[2],  test_jshort[3],  test_jlong[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeSJSJrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeSFrS(  test_jshort[1],  test_jfloat[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeSFrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeSFSFrS(  test_jshort[1],  test_jfloat[2],  test_jshort[3],  test_jfloat[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeSFSFrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeSDrS(  test_jshort[1],  test_jdouble[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeSDrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeSDSDrS(  test_jshort[1],  test_jdouble[2],  test_jshort[3],  test_jdouble[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeSDSDrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeIBrS(  test_jint[1],  test_jbyte[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeIBrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeIBIBrS(  test_jint[1],  test_jbyte[2],  test_jint[3],  test_jbyte[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeIBIBrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeISrS(  test_jint[1],  test_jshort[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeISrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeISISrS(  test_jint[1],  test_jshort[2],  test_jint[3],  test_jshort[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeISISrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeIIrS(  test_jint[1],  test_jint[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeIIrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeIIIIrS(  test_jint[1],  test_jint[2],  test_jint[3],  test_jint[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeIIIIrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeIJrS(  test_jint[1],  test_jlong[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeIJrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeIJIJrS(  test_jint[1],  test_jlong[2],  test_jint[3],  test_jlong[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeIJIJrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeIFrS(  test_jint[1],  test_jfloat[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeIFrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeIFIFrS(  test_jint[1],  test_jfloat[2],  test_jint[3],  test_jfloat[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeIFIFrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeIDrS(  test_jint[1],  test_jdouble[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeIDrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeIDIDrS(  test_jint[1],  test_jdouble[2],  test_jint[3],  test_jdouble[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeIDIDrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeJBrS(  test_jlong[1],  test_jbyte[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeJBrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeJBJBrS(  test_jlong[1],  test_jbyte[2],  test_jlong[3],  test_jbyte[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeJBJBrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeJSrS(  test_jlong[1],  test_jshort[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeJSrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeJSJSrS(  test_jlong[1],  test_jshort[2],  test_jlong[3],  test_jshort[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeJSJSrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeJIrS(  test_jlong[1],  test_jint[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeJIrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeJIJIrS(  test_jlong[1],  test_jint[2],  test_jlong[3],  test_jint[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeJIJIrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeJJrS(  test_jlong[1],  test_jlong[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeJJrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeJJJJrS(  test_jlong[1],  test_jlong[2],  test_jlong[3],  test_jlong[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeJJJJrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeJFrS(  test_jlong[1],  test_jfloat[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeJFrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeJFJFrS(  test_jlong[1],  test_jfloat[2],  test_jlong[3],  test_jfloat[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeJFJFrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeJDrS(  test_jlong[1],  test_jdouble[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeJDrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeJDJDrS(  test_jlong[1],  test_jdouble[2],  test_jlong[3],  test_jdouble[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeJDJDrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeFBrS(  test_jfloat[1],  test_jbyte[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeFBrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeFBFBrS(  test_jfloat[1],  test_jbyte[2],  test_jfloat[3],  test_jbyte[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeFBFBrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeFSrS(  test_jfloat[1],  test_jshort[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeFSrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeFSFSrS(  test_jfloat[1],  test_jshort[2],  test_jfloat[3],  test_jshort[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeFSFSrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeFIrS(  test_jfloat[1],  test_jint[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeFIrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeFIFIrS(  test_jfloat[1],  test_jint[2],  test_jfloat[3],  test_jint[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeFIFIrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeFJrS(  test_jfloat[1],  test_jlong[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeFJrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeFJFJrS(  test_jfloat[1],  test_jlong[2],  test_jfloat[3],  test_jlong[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeFJFJrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeFFrS(  test_jfloat[1],  test_jfloat[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeFFrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeFFFFrS(  test_jfloat[1],  test_jfloat[2],  test_jfloat[3],  test_jfloat[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeFFFFrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeFDrS(  test_jfloat[1],  test_jdouble[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeFDrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeFDFDrS(  test_jfloat[1],  test_jdouble[2],  test_jfloat[3],  test_jdouble[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeFDFDrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeDBrS(  test_jdouble[1],  test_jbyte[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeDBrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeDBDBrS(  test_jdouble[1],  test_jbyte[2],  test_jdouble[3],  test_jbyte[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeDBDBrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeDSrS(  test_jdouble[1],  test_jshort[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeDSrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeDSDSrS(  test_jdouble[1],  test_jshort[2],  test_jdouble[3],  test_jshort[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeDSDSrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeDIrS(  test_jdouble[1],  test_jint[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeDIrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeDIDIrS(  test_jdouble[1],  test_jint[2],  test_jdouble[3],  test_jint[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeDIDIrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeDJrS(  test_jdouble[1],  test_jlong[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeDJrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeDJDJrS(  test_jdouble[1],  test_jlong[2],  test_jdouble[3],  test_jlong[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeDJDJrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeDFrS(  test_jdouble[1],  test_jfloat[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeDFrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeDFDFrS(  test_jdouble[1],  test_jfloat[2],  test_jdouble[3],  test_jfloat[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeDFDFrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeDDrS(  test_jdouble[1],  test_jdouble[2] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeDDrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_short = nativeDDDDrS(  test_jdouble[1],  test_jdouble[2],  test_jdouble[3],  test_jdouble[4] );
		if ( retval_short != test_jshort[0] ) {
			logRetValError("Return value error:  nativeDDDDrS got: " + retval_short + " expected: " + test_jshort[0]);
		}


		retval_int = nativeBBrI(  test_jbyte[1],  test_jbyte[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeBBrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeBBBBrI(  test_jbyte[1],  test_jbyte[2],  test_jbyte[3],  test_jbyte[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeBBBBrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeBSrI(  test_jbyte[1],  test_jshort[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeBSrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeBSBSrI(  test_jbyte[1],  test_jshort[2],  test_jbyte[3],  test_jshort[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeBSBSrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeBIrI(  test_jbyte[1],  test_jint[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeBIrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeBIBIrI(  test_jbyte[1],  test_jint[2],  test_jbyte[3],  test_jint[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeBIBIrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeBJrI(  test_jbyte[1],  test_jlong[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeBJrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeBJBJrI(  test_jbyte[1],  test_jlong[2],  test_jbyte[3],  test_jlong[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeBJBJrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeBFrI(  test_jbyte[1],  test_jfloat[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeBFrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeBFBFrI(  test_jbyte[1],  test_jfloat[2],  test_jbyte[3],  test_jfloat[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeBFBFrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeBDrI(  test_jbyte[1],  test_jdouble[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeBDrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeBDBDrI(  test_jbyte[1],  test_jdouble[2],  test_jbyte[3],  test_jdouble[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeBDBDrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeSBrI(  test_jshort[1],  test_jbyte[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeSBrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeSBSBrI(  test_jshort[1],  test_jbyte[2],  test_jshort[3],  test_jbyte[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeSBSBrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeSSrI(  test_jshort[1],  test_jshort[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeSSrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeSSSSrI(  test_jshort[1],  test_jshort[2],  test_jshort[3],  test_jshort[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeSSSSrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeSIrI(  test_jshort[1],  test_jint[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeSIrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeSISIrI(  test_jshort[1],  test_jint[2],  test_jshort[3],  test_jint[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeSISIrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeSJrI(  test_jshort[1],  test_jlong[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeSJrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeSJSJrI(  test_jshort[1],  test_jlong[2],  test_jshort[3],  test_jlong[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeSJSJrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeSFrI(  test_jshort[1],  test_jfloat[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeSFrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeSFSFrI(  test_jshort[1],  test_jfloat[2],  test_jshort[3],  test_jfloat[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeSFSFrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeSDrI(  test_jshort[1],  test_jdouble[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeSDrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeSDSDrI(  test_jshort[1],  test_jdouble[2],  test_jshort[3],  test_jdouble[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeSDSDrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeIBrI(  test_jint[1],  test_jbyte[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeIBrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeIBIBrI(  test_jint[1],  test_jbyte[2],  test_jint[3],  test_jbyte[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeIBIBrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeISrI(  test_jint[1],  test_jshort[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeISrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeISISrI(  test_jint[1],  test_jshort[2],  test_jint[3],  test_jshort[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeISISrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeIIrI(  test_jint[1],  test_jint[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeIIrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeIIIIrI(  test_jint[1],  test_jint[2],  test_jint[3],  test_jint[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeIIIIrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeIJrI(  test_jint[1],  test_jlong[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeIJrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeIJIJrI(  test_jint[1],  test_jlong[2],  test_jint[3],  test_jlong[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeIJIJrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeIFrI(  test_jint[1],  test_jfloat[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeIFrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeIFIFrI(  test_jint[1],  test_jfloat[2],  test_jint[3],  test_jfloat[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeIFIFrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeIDrI(  test_jint[1],  test_jdouble[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeIDrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeIDIDrI(  test_jint[1],  test_jdouble[2],  test_jint[3],  test_jdouble[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeIDIDrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeJBrI(  test_jlong[1],  test_jbyte[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeJBrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeJBJBrI(  test_jlong[1],  test_jbyte[2],  test_jlong[3],  test_jbyte[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeJBJBrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeJSrI(  test_jlong[1],  test_jshort[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeJSrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeJSJSrI(  test_jlong[1],  test_jshort[2],  test_jlong[3],  test_jshort[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeJSJSrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeJIrI(  test_jlong[1],  test_jint[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeJIrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeJIJIrI(  test_jlong[1],  test_jint[2],  test_jlong[3],  test_jint[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeJIJIrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeJJrI(  test_jlong[1],  test_jlong[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeJJrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeJJJJrI(  test_jlong[1],  test_jlong[2],  test_jlong[3],  test_jlong[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeJJJJrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeJFrI(  test_jlong[1],  test_jfloat[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeJFrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeJFJFrI(  test_jlong[1],  test_jfloat[2],  test_jlong[3],  test_jfloat[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeJFJFrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeJDrI(  test_jlong[1],  test_jdouble[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeJDrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeJDJDrI(  test_jlong[1],  test_jdouble[2],  test_jlong[3],  test_jdouble[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeJDJDrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeFBrI(  test_jfloat[1],  test_jbyte[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeFBrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeFBFBrI(  test_jfloat[1],  test_jbyte[2],  test_jfloat[3],  test_jbyte[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeFBFBrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeFSrI(  test_jfloat[1],  test_jshort[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeFSrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeFSFSrI(  test_jfloat[1],  test_jshort[2],  test_jfloat[3],  test_jshort[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeFSFSrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeFIrI(  test_jfloat[1],  test_jint[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeFIrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeFIFIrI(  test_jfloat[1],  test_jint[2],  test_jfloat[3],  test_jint[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeFIFIrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeFJrI(  test_jfloat[1],  test_jlong[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeFJrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeFJFJrI(  test_jfloat[1],  test_jlong[2],  test_jfloat[3],  test_jlong[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeFJFJrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeFFrI(  test_jfloat[1],  test_jfloat[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeFFrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeFFFFrI(  test_jfloat[1],  test_jfloat[2],  test_jfloat[3],  test_jfloat[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeFFFFrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeFDrI(  test_jfloat[1],  test_jdouble[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeFDrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeFDFDrI(  test_jfloat[1],  test_jdouble[2],  test_jfloat[3],  test_jdouble[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeFDFDrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeDBrI(  test_jdouble[1],  test_jbyte[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeDBrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeDBDBrI(  test_jdouble[1],  test_jbyte[2],  test_jdouble[3],  test_jbyte[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeDBDBrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeDSrI(  test_jdouble[1],  test_jshort[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeDSrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeDSDSrI(  test_jdouble[1],  test_jshort[2],  test_jdouble[3],  test_jshort[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeDSDSrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeDIrI(  test_jdouble[1],  test_jint[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeDIrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeDIDIrI(  test_jdouble[1],  test_jint[2],  test_jdouble[3],  test_jint[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeDIDIrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeDJrI(  test_jdouble[1],  test_jlong[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeDJrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeDJDJrI(  test_jdouble[1],  test_jlong[2],  test_jdouble[3],  test_jlong[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeDJDJrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeDFrI(  test_jdouble[1],  test_jfloat[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeDFrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeDFDFrI(  test_jdouble[1],  test_jfloat[2],  test_jdouble[3],  test_jfloat[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeDFDFrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeDDrI(  test_jdouble[1],  test_jdouble[2] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeDDrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_int = nativeDDDDrI(  test_jdouble[1],  test_jdouble[2],  test_jdouble[3],  test_jdouble[4] );
		if ( retval_int != test_jint[0] ) {
			logRetValError("Return value error:  nativeDDDDrI got: " + retval_int + " expected: " + test_jint[0]);
		}


		retval_long = nativeBBrJ(  test_jbyte[1],  test_jbyte[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeBBrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeBBBBrJ(  test_jbyte[1],  test_jbyte[2],  test_jbyte[3],  test_jbyte[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeBBBBrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeBSrJ(  test_jbyte[1],  test_jshort[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeBSrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeBSBSrJ(  test_jbyte[1],  test_jshort[2],  test_jbyte[3],  test_jshort[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeBSBSrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeBIrJ(  test_jbyte[1],  test_jint[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeBIrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeBIBIrJ(  test_jbyte[1],  test_jint[2],  test_jbyte[3],  test_jint[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeBIBIrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeBJrJ(  test_jbyte[1],  test_jlong[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeBJrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeBJBJrJ(  test_jbyte[1],  test_jlong[2],  test_jbyte[3],  test_jlong[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeBJBJrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeBFrJ(  test_jbyte[1],  test_jfloat[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeBFrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeBFBFrJ(  test_jbyte[1],  test_jfloat[2],  test_jbyte[3],  test_jfloat[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeBFBFrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeBDrJ(  test_jbyte[1],  test_jdouble[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeBDrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeBDBDrJ(  test_jbyte[1],  test_jdouble[2],  test_jbyte[3],  test_jdouble[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeBDBDrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeSBrJ(  test_jshort[1],  test_jbyte[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeSBrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeSBSBrJ(  test_jshort[1],  test_jbyte[2],  test_jshort[3],  test_jbyte[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeSBSBrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeSSrJ(  test_jshort[1],  test_jshort[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeSSrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeSSSSrJ(  test_jshort[1],  test_jshort[2],  test_jshort[3],  test_jshort[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeSSSSrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeSIrJ(  test_jshort[1],  test_jint[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeSIrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeSISIrJ(  test_jshort[1],  test_jint[2],  test_jshort[3],  test_jint[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeSISIrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeSJrJ(  test_jshort[1],  test_jlong[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeSJrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeSJSJrJ(  test_jshort[1],  test_jlong[2],  test_jshort[3],  test_jlong[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeSJSJrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeSFrJ(  test_jshort[1],  test_jfloat[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeSFrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeSFSFrJ(  test_jshort[1],  test_jfloat[2],  test_jshort[3],  test_jfloat[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeSFSFrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeSDrJ(  test_jshort[1],  test_jdouble[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeSDrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeSDSDrJ(  test_jshort[1],  test_jdouble[2],  test_jshort[3],  test_jdouble[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeSDSDrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeIBrJ(  test_jint[1],  test_jbyte[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeIBrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeIBIBrJ(  test_jint[1],  test_jbyte[2],  test_jint[3],  test_jbyte[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeIBIBrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeISrJ(  test_jint[1],  test_jshort[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeISrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeISISrJ(  test_jint[1],  test_jshort[2],  test_jint[3],  test_jshort[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeISISrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeIIrJ(  test_jint[1],  test_jint[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeIIrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeIIIIrJ(  test_jint[1],  test_jint[2],  test_jint[3],  test_jint[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeIIIIrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeIJrJ(  test_jint[1],  test_jlong[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeIJrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeIJIJrJ(  test_jint[1],  test_jlong[2],  test_jint[3],  test_jlong[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeIJIJrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeIFrJ(  test_jint[1],  test_jfloat[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeIFrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeIFIFrJ(  test_jint[1],  test_jfloat[2],  test_jint[3],  test_jfloat[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeIFIFrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeIDrJ(  test_jint[1],  test_jdouble[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeIDrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeIDIDrJ(  test_jint[1],  test_jdouble[2],  test_jint[3],  test_jdouble[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeIDIDrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeJBrJ(  test_jlong[1],  test_jbyte[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeJBrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeJBJBrJ(  test_jlong[1],  test_jbyte[2],  test_jlong[3],  test_jbyte[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeJBJBrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeJSrJ(  test_jlong[1],  test_jshort[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeJSrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeJSJSrJ(  test_jlong[1],  test_jshort[2],  test_jlong[3],  test_jshort[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeJSJSrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeJIrJ(  test_jlong[1],  test_jint[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeJIrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeJIJIrJ(  test_jlong[1],  test_jint[2],  test_jlong[3],  test_jint[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeJIJIrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeJJrJ(  test_jlong[1],  test_jlong[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeJJrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeJJJJrJ(  test_jlong[1],  test_jlong[2],  test_jlong[3],  test_jlong[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeJJJJrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeJFrJ(  test_jlong[1],  test_jfloat[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeJFrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeJFJFrJ(  test_jlong[1],  test_jfloat[2],  test_jlong[3],  test_jfloat[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeJFJFrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeJDrJ(  test_jlong[1],  test_jdouble[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeJDrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeJDJDrJ(  test_jlong[1],  test_jdouble[2],  test_jlong[3],  test_jdouble[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeJDJDrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeFBrJ(  test_jfloat[1],  test_jbyte[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeFBrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeFBFBrJ(  test_jfloat[1],  test_jbyte[2],  test_jfloat[3],  test_jbyte[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeFBFBrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeFSrJ(  test_jfloat[1],  test_jshort[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeFSrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeFSFSrJ(  test_jfloat[1],  test_jshort[2],  test_jfloat[3],  test_jshort[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeFSFSrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeFIrJ(  test_jfloat[1],  test_jint[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeFIrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeFIFIrJ(  test_jfloat[1],  test_jint[2],  test_jfloat[3],  test_jint[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeFIFIrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeFJrJ(  test_jfloat[1],  test_jlong[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeFJrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeFJFJrJ(  test_jfloat[1],  test_jlong[2],  test_jfloat[3],  test_jlong[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeFJFJrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeFFrJ(  test_jfloat[1],  test_jfloat[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeFFrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeFFFFrJ(  test_jfloat[1],  test_jfloat[2],  test_jfloat[3],  test_jfloat[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeFFFFrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeFDrJ(  test_jfloat[1],  test_jdouble[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeFDrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeFDFDrJ(  test_jfloat[1],  test_jdouble[2],  test_jfloat[3],  test_jdouble[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeFDFDrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeDBrJ(  test_jdouble[1],  test_jbyte[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeDBrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeDBDBrJ(  test_jdouble[1],  test_jbyte[2],  test_jdouble[3],  test_jbyte[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeDBDBrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeDSrJ(  test_jdouble[1],  test_jshort[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeDSrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeDSDSrJ(  test_jdouble[1],  test_jshort[2],  test_jdouble[3],  test_jshort[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeDSDSrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeDIrJ(  test_jdouble[1],  test_jint[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeDIrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeDIDIrJ(  test_jdouble[1],  test_jint[2],  test_jdouble[3],  test_jint[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeDIDIrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeDJrJ(  test_jdouble[1],  test_jlong[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeDJrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeDJDJrJ(  test_jdouble[1],  test_jlong[2],  test_jdouble[3],  test_jlong[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeDJDJrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeDFrJ(  test_jdouble[1],  test_jfloat[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeDFrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeDFDFrJ(  test_jdouble[1],  test_jfloat[2],  test_jdouble[3],  test_jfloat[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeDFDFrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeDDrJ(  test_jdouble[1],  test_jdouble[2] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeDDrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeDDDDrJ(  test_jdouble[1],  test_jdouble[2],  test_jdouble[3],  test_jdouble[4] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeDDDDrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_float = nativeBBrF(  test_jbyte[1],  test_jbyte[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeBBrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeBBBBrF(  test_jbyte[1],  test_jbyte[2],  test_jbyte[3],  test_jbyte[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeBBBBrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeBSrF(  test_jbyte[1],  test_jshort[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeBSrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeBSBSrF(  test_jbyte[1],  test_jshort[2],  test_jbyte[3],  test_jshort[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeBSBSrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeBIrF(  test_jbyte[1],  test_jint[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeBIrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeBIBIrF(  test_jbyte[1],  test_jint[2],  test_jbyte[3],  test_jint[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeBIBIrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeBJrF(  test_jbyte[1],  test_jlong[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeBJrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeBJBJrF(  test_jbyte[1],  test_jlong[2],  test_jbyte[3],  test_jlong[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeBJBJrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeBFrF(  test_jbyte[1],  test_jfloat[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeBFrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeBFBFrF(  test_jbyte[1],  test_jfloat[2],  test_jbyte[3],  test_jfloat[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeBFBFrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeBDrF(  test_jbyte[1],  test_jdouble[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeBDrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeBDBDrF(  test_jbyte[1],  test_jdouble[2],  test_jbyte[3],  test_jdouble[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeBDBDrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeSBrF(  test_jshort[1],  test_jbyte[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeSBrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeSBSBrF(  test_jshort[1],  test_jbyte[2],  test_jshort[3],  test_jbyte[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeSBSBrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeSSrF(  test_jshort[1],  test_jshort[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeSSrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeSSSSrF(  test_jshort[1],  test_jshort[2],  test_jshort[3],  test_jshort[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeSSSSrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeSIrF(  test_jshort[1],  test_jint[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeSIrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeSISIrF(  test_jshort[1],  test_jint[2],  test_jshort[3],  test_jint[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeSISIrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeSJrF(  test_jshort[1],  test_jlong[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeSJrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeSJSJrF(  test_jshort[1],  test_jlong[2],  test_jshort[3],  test_jlong[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeSJSJrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeSFrF(  test_jshort[1],  test_jfloat[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeSFrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeSFSFrF(  test_jshort[1],  test_jfloat[2],  test_jshort[3],  test_jfloat[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeSFSFrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeSDrF(  test_jshort[1],  test_jdouble[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeSDrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeSDSDrF(  test_jshort[1],  test_jdouble[2],  test_jshort[3],  test_jdouble[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeSDSDrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeIBrF(  test_jint[1],  test_jbyte[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeIBrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeIBIBrF(  test_jint[1],  test_jbyte[2],  test_jint[3],  test_jbyte[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeIBIBrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeISrF(  test_jint[1],  test_jshort[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeISrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeISISrF(  test_jint[1],  test_jshort[2],  test_jint[3],  test_jshort[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeISISrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeIIrF(  test_jint[1],  test_jint[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeIIrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeIIIIrF(  test_jint[1],  test_jint[2],  test_jint[3],  test_jint[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeIIIIrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeIJrF(  test_jint[1],  test_jlong[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeIJrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeIJIJrF(  test_jint[1],  test_jlong[2],  test_jint[3],  test_jlong[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeIJIJrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeIFrF(  test_jint[1],  test_jfloat[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeIFrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeIFIFrF(  test_jint[1],  test_jfloat[2],  test_jint[3],  test_jfloat[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeIFIFrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeIDrF(  test_jint[1],  test_jdouble[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeIDrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeIDIDrF(  test_jint[1],  test_jdouble[2],  test_jint[3],  test_jdouble[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeIDIDrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeJBrF(  test_jlong[1],  test_jbyte[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeJBrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeJBJBrF(  test_jlong[1],  test_jbyte[2],  test_jlong[3],  test_jbyte[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeJBJBrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeJSrF(  test_jlong[1],  test_jshort[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeJSrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeJSJSrF(  test_jlong[1],  test_jshort[2],  test_jlong[3],  test_jshort[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeJSJSrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeJIrF(  test_jlong[1],  test_jint[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeJIrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeJIJIrF(  test_jlong[1],  test_jint[2],  test_jlong[3],  test_jint[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeJIJIrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeJJrF(  test_jlong[1],  test_jlong[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeJJrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeJJJJrF(  test_jlong[1],  test_jlong[2],  test_jlong[3],  test_jlong[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeJJJJrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeJFrF(  test_jlong[1],  test_jfloat[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeJFrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeJFJFrF(  test_jlong[1],  test_jfloat[2],  test_jlong[3],  test_jfloat[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeJFJFrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeJDrF(  test_jlong[1],  test_jdouble[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeJDrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeJDJDrF(  test_jlong[1],  test_jdouble[2],  test_jlong[3],  test_jdouble[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeJDJDrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeFBrF(  test_jfloat[1],  test_jbyte[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeFBrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeFBFBrF(  test_jfloat[1],  test_jbyte[2],  test_jfloat[3],  test_jbyte[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeFBFBrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeFSrF(  test_jfloat[1],  test_jshort[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeFSrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeFSFSrF(  test_jfloat[1],  test_jshort[2],  test_jfloat[3],  test_jshort[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeFSFSrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeFIrF(  test_jfloat[1],  test_jint[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeFIrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeFIFIrF(  test_jfloat[1],  test_jint[2],  test_jfloat[3],  test_jint[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeFIFIrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeFJrF(  test_jfloat[1],  test_jlong[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeFJrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeFJFJrF(  test_jfloat[1],  test_jlong[2],  test_jfloat[3],  test_jlong[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeFJFJrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeFFrF(  test_jfloat[1],  test_jfloat[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeFFrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeFFFFrF(  test_jfloat[1],  test_jfloat[2],  test_jfloat[3],  test_jfloat[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeFFFFrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeFDrF(  test_jfloat[1],  test_jdouble[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeFDrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeFDFDrF(  test_jfloat[1],  test_jdouble[2],  test_jfloat[3],  test_jdouble[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeFDFDrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeDBrF(  test_jdouble[1],  test_jbyte[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeDBrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeDBDBrF(  test_jdouble[1],  test_jbyte[2],  test_jdouble[3],  test_jbyte[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeDBDBrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeDSrF(  test_jdouble[1],  test_jshort[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeDSrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeDSDSrF(  test_jdouble[1],  test_jshort[2],  test_jdouble[3],  test_jshort[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeDSDSrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeDIrF(  test_jdouble[1],  test_jint[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeDIrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeDIDIrF(  test_jdouble[1],  test_jint[2],  test_jdouble[3],  test_jint[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeDIDIrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeDJrF(  test_jdouble[1],  test_jlong[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeDJrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeDJDJrF(  test_jdouble[1],  test_jlong[2],  test_jdouble[3],  test_jlong[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeDJDJrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeDFrF(  test_jdouble[1],  test_jfloat[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeDFrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeDFDFrF(  test_jdouble[1],  test_jfloat[2],  test_jdouble[3],  test_jfloat[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeDFDFrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeDDrF(  test_jdouble[1],  test_jdouble[2] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeDDrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_float = nativeDDDDrF(  test_jdouble[1],  test_jdouble[2],  test_jdouble[3],  test_jdouble[4] );
		if ( retval_float != test_jfloat[0] ) {
			logRetValError("Return value error:  nativeDDDDrF got: " + retval_float + " expected: " + test_jfloat[0]);
		}


		retval_double = nativeBBrD(  test_jbyte[1],  test_jbyte[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeBBrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeBBBBrD(  test_jbyte[1],  test_jbyte[2],  test_jbyte[3],  test_jbyte[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeBBBBrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeBSrD(  test_jbyte[1],  test_jshort[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeBSrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeBSBSrD(  test_jbyte[1],  test_jshort[2],  test_jbyte[3],  test_jshort[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeBSBSrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeBIrD(  test_jbyte[1],  test_jint[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeBIrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeBIBIrD(  test_jbyte[1],  test_jint[2],  test_jbyte[3],  test_jint[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeBIBIrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeBJrD(  test_jbyte[1],  test_jlong[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeBJrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeBJBJrD(  test_jbyte[1],  test_jlong[2],  test_jbyte[3],  test_jlong[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeBJBJrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeBFrD(  test_jbyte[1],  test_jfloat[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeBFrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeBFBFrD(  test_jbyte[1],  test_jfloat[2],  test_jbyte[3],  test_jfloat[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeBFBFrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeBDrD(  test_jbyte[1],  test_jdouble[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeBDrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeBDBDrD(  test_jbyte[1],  test_jdouble[2],  test_jbyte[3],  test_jdouble[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeBDBDrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeSBrD(  test_jshort[1],  test_jbyte[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeSBrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeSBSBrD(  test_jshort[1],  test_jbyte[2],  test_jshort[3],  test_jbyte[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeSBSBrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeSSrD(  test_jshort[1],  test_jshort[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeSSrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeSSSSrD(  test_jshort[1],  test_jshort[2],  test_jshort[3],  test_jshort[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeSSSSrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeSIrD(  test_jshort[1],  test_jint[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeSIrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeSISIrD(  test_jshort[1],  test_jint[2],  test_jshort[3],  test_jint[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeSISIrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeSJrD(  test_jshort[1],  test_jlong[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeSJrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeSJSJrD(  test_jshort[1],  test_jlong[2],  test_jshort[3],  test_jlong[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeSJSJrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeSFrD(  test_jshort[1],  test_jfloat[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeSFrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeSFSFrD(  test_jshort[1],  test_jfloat[2],  test_jshort[3],  test_jfloat[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeSFSFrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeSDrD(  test_jshort[1],  test_jdouble[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeSDrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeSDSDrD(  test_jshort[1],  test_jdouble[2],  test_jshort[3],  test_jdouble[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeSDSDrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeIBrD(  test_jint[1],  test_jbyte[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeIBrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeIBIBrD(  test_jint[1],  test_jbyte[2],  test_jint[3],  test_jbyte[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeIBIBrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeISrD(  test_jint[1],  test_jshort[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeISrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeISISrD(  test_jint[1],  test_jshort[2],  test_jint[3],  test_jshort[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeISISrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeIIrD(  test_jint[1],  test_jint[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeIIrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeIIIIrD(  test_jint[1],  test_jint[2],  test_jint[3],  test_jint[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeIIIIrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeIJrD(  test_jint[1],  test_jlong[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeIJrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeIJIJrD(  test_jint[1],  test_jlong[2],  test_jint[3],  test_jlong[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeIJIJrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeIFrD(  test_jint[1],  test_jfloat[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeIFrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeIFIFrD(  test_jint[1],  test_jfloat[2],  test_jint[3],  test_jfloat[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeIFIFrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeIDrD(  test_jint[1],  test_jdouble[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeIDrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeIDIDrD(  test_jint[1],  test_jdouble[2],  test_jint[3],  test_jdouble[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeIDIDrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeJBrD(  test_jlong[1],  test_jbyte[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeJBrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeJBJBrD(  test_jlong[1],  test_jbyte[2],  test_jlong[3],  test_jbyte[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeJBJBrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeJSrD(  test_jlong[1],  test_jshort[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeJSrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeJSJSrD(  test_jlong[1],  test_jshort[2],  test_jlong[3],  test_jshort[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeJSJSrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeJIrD(  test_jlong[1],  test_jint[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeJIrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeJIJIrD(  test_jlong[1],  test_jint[2],  test_jlong[3],  test_jint[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeJIJIrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeJJrD(  test_jlong[1],  test_jlong[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeJJrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeJJJJrD(  test_jlong[1],  test_jlong[2],  test_jlong[3],  test_jlong[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeJJJJrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeJFrD(  test_jlong[1],  test_jfloat[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeJFrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeJFJFrD(  test_jlong[1],  test_jfloat[2],  test_jlong[3],  test_jfloat[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeJFJFrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeJDrD(  test_jlong[1],  test_jdouble[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeJDrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeJDJDrD(  test_jlong[1],  test_jdouble[2],  test_jlong[3],  test_jdouble[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeJDJDrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeFBrD(  test_jfloat[1],  test_jbyte[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeFBrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeFBFBrD(  test_jfloat[1],  test_jbyte[2],  test_jfloat[3],  test_jbyte[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeFBFBrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeFSrD(  test_jfloat[1],  test_jshort[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeFSrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeFSFSrD(  test_jfloat[1],  test_jshort[2],  test_jfloat[3],  test_jshort[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeFSFSrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeFIrD(  test_jfloat[1],  test_jint[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeFIrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeFIFIrD(  test_jfloat[1],  test_jint[2],  test_jfloat[3],  test_jint[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeFIFIrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeFJrD(  test_jfloat[1],  test_jlong[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeFJrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeFJFJrD(  test_jfloat[1],  test_jlong[2],  test_jfloat[3],  test_jlong[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeFJFJrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeFFrD(  test_jfloat[1],  test_jfloat[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeFFrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeFFFFrD(  test_jfloat[1],  test_jfloat[2],  test_jfloat[3],  test_jfloat[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeFFFFrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeFDrD(  test_jfloat[1],  test_jdouble[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeFDrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeFDFDrD(  test_jfloat[1],  test_jdouble[2],  test_jfloat[3],  test_jdouble[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeFDFDrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeDBrD(  test_jdouble[1],  test_jbyte[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeDBrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeDBDBrD(  test_jdouble[1],  test_jbyte[2],  test_jdouble[3],  test_jbyte[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeDBDBrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeDSrD(  test_jdouble[1],  test_jshort[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeDSrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeDSDSrD(  test_jdouble[1],  test_jshort[2],  test_jdouble[3],  test_jshort[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeDSDSrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeDIrD(  test_jdouble[1],  test_jint[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeDIrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeDIDIrD(  test_jdouble[1],  test_jint[2],  test_jdouble[3],  test_jint[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeDIDIrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeDJrD(  test_jdouble[1],  test_jlong[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeDJrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeDJDJrD(  test_jdouble[1],  test_jlong[2],  test_jdouble[3],  test_jlong[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeDJDJrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeDFrD(  test_jdouble[1],  test_jfloat[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeDFrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeDFDFrD(  test_jdouble[1],  test_jfloat[2],  test_jdouble[3],  test_jfloat[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeDFDFrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeDDrD(  test_jdouble[1],  test_jdouble[2] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeDDrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		retval_double = nativeDDDDrD(  test_jdouble[1],  test_jdouble[2],  test_jdouble[3],  test_jdouble[4] );
		if ( retval_double != test_jdouble[0] ) {
			logRetValError("Return value error:  nativeDDDDrD got: " + retval_double + " expected: " + test_jdouble[0]);
		}


		try {
			retval_long = nativeIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIrJ(  test_jint[1],  test_jint[2],  test_jint[3],  test_jint[4],  test_jint[5],  test_jint[6],  test_jint[7],  test_jint[8],  test_jint[9],  test_jint[10],  test_jint[11],  test_jint[12],  test_jint[13],  test_jint[14],  test_jint[15],  test_jint[0],  test_jint[1],  test_jint[2],  test_jint[3],  test_jint[4],  test_jint[5],  test_jint[6],  test_jint[7],  test_jint[8],  test_jint[9],  test_jint[10],  test_jint[11],  test_jint[12],  test_jint[13],  test_jint[14],  test_jint[15],  test_jint[0],  test_jint[1],  test_jint[2],  test_jint[3],  test_jint[4],  test_jint[5],  test_jint[6],  test_jint[7],  test_jint[8],  test_jint[9],  test_jint[10],  test_jint[11],  test_jint[12],  test_jint[13],  test_jint[14],  test_jint[15],  test_jint[0],  test_jint[1],  test_jint[2],  test_jint[3],  test_jint[4],  test_jint[5],  test_jint[6],  test_jint[7],  test_jint[8],  test_jint[9],  test_jint[10],  test_jint[11],  test_jint[12],  test_jint[13],  test_jint[14],  test_jint[15],  test_jint[0],  test_jint[1],  test_jint[2],  test_jint[3],  test_jint[4],  test_jint[5],  test_jint[6],  test_jint[7],  test_jint[8],  test_jint[9],  test_jint[10],  test_jint[11],  test_jint[12],  test_jint[13],  test_jint[14],  test_jint[15],  test_jint[0],  test_jint[1],  test_jint[2],  test_jint[3],  test_jint[4],  test_jint[5],  test_jint[6],  test_jint[7],  test_jint[8],  test_jint[9],  test_jint[10],  test_jint[11],  test_jint[12],  test_jint[13],  test_jint[14],  test_jint[15],  test_jint[0],  test_jint[1],  test_jint[2],  test_jint[3],  test_jint[4],  test_jint[5],  test_jint[6],  test_jint[7],  test_jint[8],  test_jint[9],  test_jint[10],  test_jint[11],  test_jint[12],  test_jint[13],  test_jint[14],  test_jint[15],  test_jint[0],  test_jint[1],  test_jint[2],  test_jint[3],  test_jint[4],  test_jint[5],  test_jint[6],  test_jint[7],  test_jint[8],  test_jint[9],  test_jint[10],  test_jint[11],  test_jint[12],  test_jint[13],  test_jint[14],  test_jint[15],  test_jint[0],  test_jint[1],  test_jint[2],  test_jint[3],  test_jint[4],  test_jint[5],  test_jint[6],  test_jint[7],  test_jint[8],  test_jint[9],  test_jint[10],  test_jint[11],  test_jint[12],  test_jint[13],  test_jint[14],  test_jint[15],  test_jint[0],  test_jint[1],  test_jint[2],  test_jint[3],  test_jint[4],  test_jint[5],  test_jint[6],  test_jint[7],  test_jint[8],  test_jint[9],  test_jint[10],  test_jint[11],  test_jint[12],  test_jint[13],  test_jint[14],  test_jint[15],  test_jint[0],  test_jint[1],  test_jint[2],  test_jint[3],  test_jint[4],  test_jint[5],  test_jint[6],  test_jint[7],  test_jint[8],  test_jint[9],  test_jint[10],  test_jint[11],  test_jint[12],  test_jint[13],  test_jint[14],  test_jint[15],  test_jint[0],  test_jint[1],  test_jint[2],  test_jint[3],  test_jint[4],  test_jint[5],  test_jint[6],  test_jint[7],  test_jint[8],  test_jint[9],  test_jint[10],  test_jint[11],  test_jint[12],  test_jint[13],  test_jint[14],  test_jint[15],  test_jint[0],  test_jint[1],  test_jint[2],  test_jint[3],  test_jint[4],  test_jint[5],  test_jint[6],  test_jint[7],  test_jint[8],  test_jint[9],  test_jint[10],  test_jint[11],  test_jint[12],  test_jint[13],  test_jint[14],  test_jint[15],  test_jint[0],  test_jint[1],  test_jint[2],  test_jint[3],  test_jint[4],  test_jint[5],  test_jint[6],  test_jint[7],  test_jint[8],  test_jint[9],  test_jint[10],  test_jint[11],  test_jint[12],  test_jint[13],  test_jint[14],  test_jint[15],  test_jint[0],  test_jint[1],  test_jint[2],  test_jint[3],  test_jint[4],  test_jint[5],  test_jint[6],  test_jint[7],  test_jint[8],  test_jint[9],  test_jint[10],  test_jint[11],  test_jint[12],  test_jint[13],  test_jint[14],  test_jint[15],  test_jint[0],  test_jint[1],  test_jint[2],  test_jint[3],  test_jint[4],  test_jint[5],  test_jint[6],  test_jint[7],  test_jint[8],  test_jint[9],  test_jint[10],  test_jint[11],  test_jint[12],  test_jint[13],  test_jint[14] );
			if ( retval_long != test_jlong[0] ) {
				logRetValError("Return value error:  nativeIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIrJ got: " + retval_long + " expected: " + test_jlong[0]);
			}
	
	
			retval_int = nativeIJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJrI(  test_jint[1],  test_jlong[2],  test_jlong[3],  test_jlong[4],  test_jlong[5],  test_jlong[6],  test_jlong[7],  test_jlong[8],  test_jlong[9],  test_jlong[10],  test_jlong[11],  test_jlong[12],  test_jlong[13],  test_jlong[14],  test_jlong[15],  test_jlong[0],  test_jlong[1],  test_jlong[2],  test_jlong[3],  test_jlong[4],  test_jlong[5],  test_jlong[6],  test_jlong[7],  test_jlong[8],  test_jlong[9],  test_jlong[10],  test_jlong[11],  test_jlong[12],  test_jlong[13],  test_jlong[14],  test_jlong[15],  test_jlong[0],  test_jlong[1],  test_jlong[2],  test_jlong[3],  test_jlong[4],  test_jlong[5],  test_jlong[6],  test_jlong[7],  test_jlong[8],  test_jlong[9],  test_jlong[10],  test_jlong[11],  test_jlong[12],  test_jlong[13],  test_jlong[14],  test_jlong[15],  test_jlong[0],  test_jlong[1],  test_jlong[2],  test_jlong[3],  test_jlong[4],  test_jlong[5],  test_jlong[6],  test_jlong[7],  test_jlong[8],  test_jlong[9],  test_jlong[10],  test_jlong[11],  test_jlong[12],  test_jlong[13],  test_jlong[14],  test_jlong[15],  test_jlong[0],  test_jlong[1],  test_jlong[2],  test_jlong[3],  test_jlong[4],  test_jlong[5],  test_jlong[6],  test_jlong[7],  test_jlong[8],  test_jlong[9],  test_jlong[10],  test_jlong[11],  test_jlong[12],  test_jlong[13],  test_jlong[14],  test_jlong[15],  test_jlong[0],  test_jlong[1],  test_jlong[2],  test_jlong[3],  test_jlong[4],  test_jlong[5],  test_jlong[6],  test_jlong[7],  test_jlong[8],  test_jlong[9],  test_jlong[10],  test_jlong[11],  test_jlong[12],  test_jlong[13],  test_jlong[14],  test_jlong[15],  test_jlong[0],  test_jlong[1],  test_jlong[2],  test_jlong[3],  test_jlong[4],  test_jlong[5],  test_jlong[6],  test_jlong[7],  test_jlong[8],  test_jlong[9],  test_jlong[10],  test_jlong[11],  test_jlong[12],  test_jlong[13],  test_jlong[14],  test_jlong[15],  test_jlong[0],  test_jlong[1],  test_jlong[2],  test_jlong[3],  test_jlong[4],  test_jlong[5],  test_jlong[6],  test_jlong[7],  test_jlong[8],  test_jlong[9],  test_jlong[10],  test_jlong[11],  test_jlong[12],  test_jlong[13],  test_jlong[14],  test_jlong[15] );
			if ( retval_int != test_jint[0] ) {
				logRetValError("Return value error:  nativeIJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJrI got: " + retval_int + " expected: " + test_jint[0]);
			}
		} catch(UnsatisfiedLinkError e) {
			/* ZOS can't find the long symbol names */
		}


		retval_long = nativeJJJJJJJJJJJJrJ(  test_jlong[1],  test_jlong[2],  test_jlong[3],  test_jlong[4],  test_jlong[5],  test_jlong[6],  test_jlong[7],  test_jlong[8],  test_jlong[9],  test_jlong[10],  test_jlong[11],  test_jlong[12] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeJJJJJJJJJJJJrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeIJJJJJJJJJJJJrJ(  test_jint[1],  test_jlong[2],  test_jlong[3],  test_jlong[4],  test_jlong[5],  test_jlong[6],  test_jlong[7],  test_jlong[8],  test_jlong[9],  test_jlong[10],  test_jlong[11],  test_jlong[12],  test_jlong[13] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeIJJJJJJJJJJJJrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeIJIJIJIJIJIJrJ(  test_jint[1],  test_jlong[2],  test_jint[3],  test_jlong[4],  test_jint[5],  test_jlong[6],  test_jint[7],  test_jlong[8],  test_jint[9],  test_jlong[10],  test_jint[11],  test_jlong[12] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeIJIJIJIJIJIJrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeJIJIJIJIJIJIrJ(  test_jlong[1],  test_jint[2],  test_jlong[3],  test_jint[4],  test_jlong[5],  test_jint[6],  test_jlong[7],  test_jint[8],  test_jlong[9],  test_jint[10],  test_jlong[11],  test_jint[12] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeJIJIJIJIJIJIrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeFFFFFFFFFFFFrJ(  test_jfloat[1],  test_jfloat[2],  test_jfloat[3],  test_jfloat[4],  test_jfloat[5],  test_jfloat[6],  test_jfloat[7],  test_jfloat[8],  test_jfloat[9],  test_jfloat[10],  test_jfloat[11],  test_jfloat[12] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeFFFFFFFFFFFFrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeDDDDDDDDDDDDrJ(  test_jdouble[1],  test_jdouble[2],  test_jdouble[3],  test_jdouble[4],  test_jdouble[5],  test_jdouble[6],  test_jdouble[7],  test_jdouble[8],  test_jdouble[9],  test_jdouble[10],  test_jdouble[11],  test_jdouble[12] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeDDDDDDDDDDDDrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeFDFDFDFDFDFDrJ(  test_jfloat[1],  test_jdouble[2],  test_jfloat[3],  test_jdouble[4],  test_jfloat[5],  test_jdouble[6],  test_jfloat[7],  test_jdouble[8],  test_jfloat[9],  test_jdouble[10],  test_jfloat[11],  test_jdouble[12] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeFDFDFDFDFDFDrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeDFDFDFDFDFDFrJ(  test_jdouble[1],  test_jfloat[2],  test_jdouble[3],  test_jfloat[4],  test_jdouble[5],  test_jfloat[6],  test_jdouble[7],  test_jfloat[8],  test_jdouble[9],  test_jfloat[10],  test_jdouble[11],  test_jfloat[12] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeDFDFDFDFDFDFrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


		retval_long = nativeBBSSIJFDIFDFDFDBBSSIJFDrJ(  test_jbyte[1],  test_jbyte[2],  test_jshort[3],  test_jshort[4],  test_jint[5],  test_jlong[6],  test_jfloat[7],  test_jdouble[8],  test_jint[9],  test_jfloat[10],  test_jdouble[11],  test_jfloat[12],  test_jdouble[13],  test_jfloat[14],  test_jdouble[15],  test_jbyte[0],  test_jbyte[1],  test_jshort[2],  test_jshort[3],  test_jint[4],  test_jlong[5],  test_jfloat[6],  test_jdouble[7] );
		if ( retval_long != test_jlong[0] ) {
			logRetValError("Return value error:  nativeBBSSIJFDIFDFDFDBBSSIJFDrJ got: " + retval_long + " expected: " + test_jlong[0]);
		}


}

	native void logRetValError(String arg);

 native void summary();


	native byte nativeBBrB(byte arg1, byte arg2 );

	native byte nativeBBBBrB(byte arg1, byte arg2, byte arg3, byte arg4 );

	native byte nativeBSrB(byte arg1, short arg2 );

	native byte nativeBSBSrB(byte arg1, short arg2, byte arg3, short arg4 );

	native byte nativeBIrB(byte arg1, int arg2 );

	native byte nativeBIBIrB(byte arg1, int arg2, byte arg3, int arg4 );

	native byte nativeBJrB(byte arg1, long arg2 );

	native byte nativeBJBJrB(byte arg1, long arg2, byte arg3, long arg4 );

	native byte nativeBFrB(byte arg1, float arg2 );

	native byte nativeBFBFrB(byte arg1, float arg2, byte arg3, float arg4 );

	native byte nativeBDrB(byte arg1, double arg2 );

	native byte nativeBDBDrB(byte arg1, double arg2, byte arg3, double arg4 );

	native byte nativeSBrB(short arg1, byte arg2 );

	native byte nativeSBSBrB(short arg1, byte arg2, short arg3, byte arg4 );

	native byte nativeSSrB(short arg1, short arg2 );

	native byte nativeSSSSrB(short arg1, short arg2, short arg3, short arg4 );

	native byte nativeSIrB(short arg1, int arg2 );

	native byte nativeSISIrB(short arg1, int arg2, short arg3, int arg4 );

	native byte nativeSJrB(short arg1, long arg2 );

	native byte nativeSJSJrB(short arg1, long arg2, short arg3, long arg4 );

	native byte nativeSFrB(short arg1, float arg2 );

	native byte nativeSFSFrB(short arg1, float arg2, short arg3, float arg4 );

	native byte nativeSDrB(short arg1, double arg2 );

	native byte nativeSDSDrB(short arg1, double arg2, short arg3, double arg4 );

	native byte nativeIBrB(int arg1, byte arg2 );

	native byte nativeIBIBrB(int arg1, byte arg2, int arg3, byte arg4 );

	native byte nativeISrB(int arg1, short arg2 );

	native byte nativeISISrB(int arg1, short arg2, int arg3, short arg4 );

	native byte nativeIIrB(int arg1, int arg2 );

	native byte nativeIIIIrB(int arg1, int arg2, int arg3, int arg4 );

	native byte nativeIJrB(int arg1, long arg2 );

	native byte nativeIJIJrB(int arg1, long arg2, int arg3, long arg4 );

	native byte nativeIFrB(int arg1, float arg2 );

	native byte nativeIFIFrB(int arg1, float arg2, int arg3, float arg4 );

	native byte nativeIDrB(int arg1, double arg2 );

	native byte nativeIDIDrB(int arg1, double arg2, int arg3, double arg4 );

	native byte nativeJBrB(long arg1, byte arg2 );

	native byte nativeJBJBrB(long arg1, byte arg2, long arg3, byte arg4 );

	native byte nativeJSrB(long arg1, short arg2 );

	native byte nativeJSJSrB(long arg1, short arg2, long arg3, short arg4 );

	native byte nativeJIrB(long arg1, int arg2 );

	native byte nativeJIJIrB(long arg1, int arg2, long arg3, int arg4 );

	native byte nativeJJrB(long arg1, long arg2 );

	native byte nativeJJJJrB(long arg1, long arg2, long arg3, long arg4 );

	native byte nativeJFrB(long arg1, float arg2 );

	native byte nativeJFJFrB(long arg1, float arg2, long arg3, float arg4 );

	native byte nativeJDrB(long arg1, double arg2 );

	native byte nativeJDJDrB(long arg1, double arg2, long arg3, double arg4 );

	native byte nativeFBrB(float arg1, byte arg2 );

	native byte nativeFBFBrB(float arg1, byte arg2, float arg3, byte arg4 );

	native byte nativeFSrB(float arg1, short arg2 );

	native byte nativeFSFSrB(float arg1, short arg2, float arg3, short arg4 );

	native byte nativeFIrB(float arg1, int arg2 );

	native byte nativeFIFIrB(float arg1, int arg2, float arg3, int arg4 );

	native byte nativeFJrB(float arg1, long arg2 );

	native byte nativeFJFJrB(float arg1, long arg2, float arg3, long arg4 );

	native byte nativeFFrB(float arg1, float arg2 );

	native byte nativeFFFFrB(float arg1, float arg2, float arg3, float arg4 );

	native byte nativeFDrB(float arg1, double arg2 );

	native byte nativeFDFDrB(float arg1, double arg2, float arg3, double arg4 );

	native byte nativeDBrB(double arg1, byte arg2 );

	native byte nativeDBDBrB(double arg1, byte arg2, double arg3, byte arg4 );

	native byte nativeDSrB(double arg1, short arg2 );

	native byte nativeDSDSrB(double arg1, short arg2, double arg3, short arg4 );

	native byte nativeDIrB(double arg1, int arg2 );

	native byte nativeDIDIrB(double arg1, int arg2, double arg3, int arg4 );

	native byte nativeDJrB(double arg1, long arg2 );

	native byte nativeDJDJrB(double arg1, long arg2, double arg3, long arg4 );

	native byte nativeDFrB(double arg1, float arg2 );

	native byte nativeDFDFrB(double arg1, float arg2, double arg3, float arg4 );

	native byte nativeDDrB(double arg1, double arg2 );

	native byte nativeDDDDrB(double arg1, double arg2, double arg3, double arg4 );

	native short nativeBBrS(byte arg1, byte arg2 );

	native short nativeBBBBrS(byte arg1, byte arg2, byte arg3, byte arg4 );

	native short nativeBSrS(byte arg1, short arg2 );

	native short nativeBSBSrS(byte arg1, short arg2, byte arg3, short arg4 );

	native short nativeBIrS(byte arg1, int arg2 );

	native short nativeBIBIrS(byte arg1, int arg2, byte arg3, int arg4 );

	native short nativeBJrS(byte arg1, long arg2 );

	native short nativeBJBJrS(byte arg1, long arg2, byte arg3, long arg4 );

	native short nativeBFrS(byte arg1, float arg2 );

	native short nativeBFBFrS(byte arg1, float arg2, byte arg3, float arg4 );

	native short nativeBDrS(byte arg1, double arg2 );

	native short nativeBDBDrS(byte arg1, double arg2, byte arg3, double arg4 );

	native short nativeSBrS(short arg1, byte arg2 );

	native short nativeSBSBrS(short arg1, byte arg2, short arg3, byte arg4 );

	native short nativeSSrS(short arg1, short arg2 );

	native short nativeSSSSrS(short arg1, short arg2, short arg3, short arg4 );

	native short nativeSIrS(short arg1, int arg2 );

	native short nativeSISIrS(short arg1, int arg2, short arg3, int arg4 );

	native short nativeSJrS(short arg1, long arg2 );

	native short nativeSJSJrS(short arg1, long arg2, short arg3, long arg4 );

	native short nativeSFrS(short arg1, float arg2 );

	native short nativeSFSFrS(short arg1, float arg2, short arg3, float arg4 );

	native short nativeSDrS(short arg1, double arg2 );

	native short nativeSDSDrS(short arg1, double arg2, short arg3, double arg4 );

	native short nativeIBrS(int arg1, byte arg2 );

	native short nativeIBIBrS(int arg1, byte arg2, int arg3, byte arg4 );

	native short nativeISrS(int arg1, short arg2 );

	native short nativeISISrS(int arg1, short arg2, int arg3, short arg4 );

	native short nativeIIrS(int arg1, int arg2 );

	native short nativeIIIIrS(int arg1, int arg2, int arg3, int arg4 );

	native short nativeIJrS(int arg1, long arg2 );

	native short nativeIJIJrS(int arg1, long arg2, int arg3, long arg4 );

	native short nativeIFrS(int arg1, float arg2 );

	native short nativeIFIFrS(int arg1, float arg2, int arg3, float arg4 );

	native short nativeIDrS(int arg1, double arg2 );

	native short nativeIDIDrS(int arg1, double arg2, int arg3, double arg4 );

	native short nativeJBrS(long arg1, byte arg2 );

	native short nativeJBJBrS(long arg1, byte arg2, long arg3, byte arg4 );

	native short nativeJSrS(long arg1, short arg2 );

	native short nativeJSJSrS(long arg1, short arg2, long arg3, short arg4 );

	native short nativeJIrS(long arg1, int arg2 );

	native short nativeJIJIrS(long arg1, int arg2, long arg3, int arg4 );

	native short nativeJJrS(long arg1, long arg2 );

	native short nativeJJJJrS(long arg1, long arg2, long arg3, long arg4 );

	native short nativeJFrS(long arg1, float arg2 );

	native short nativeJFJFrS(long arg1, float arg2, long arg3, float arg4 );

	native short nativeJDrS(long arg1, double arg2 );

	native short nativeJDJDrS(long arg1, double arg2, long arg3, double arg4 );

	native short nativeFBrS(float arg1, byte arg2 );

	native short nativeFBFBrS(float arg1, byte arg2, float arg3, byte arg4 );

	native short nativeFSrS(float arg1, short arg2 );

	native short nativeFSFSrS(float arg1, short arg2, float arg3, short arg4 );

	native short nativeFIrS(float arg1, int arg2 );

	native short nativeFIFIrS(float arg1, int arg2, float arg3, int arg4 );

	native short nativeFJrS(float arg1, long arg2 );

	native short nativeFJFJrS(float arg1, long arg2, float arg3, long arg4 );

	native short nativeFFrS(float arg1, float arg2 );

	native short nativeFFFFrS(float arg1, float arg2, float arg3, float arg4 );

	native short nativeFDrS(float arg1, double arg2 );

	native short nativeFDFDrS(float arg1, double arg2, float arg3, double arg4 );

	native short nativeDBrS(double arg1, byte arg2 );

	native short nativeDBDBrS(double arg1, byte arg2, double arg3, byte arg4 );

	native short nativeDSrS(double arg1, short arg2 );

	native short nativeDSDSrS(double arg1, short arg2, double arg3, short arg4 );

	native short nativeDIrS(double arg1, int arg2 );

	native short nativeDIDIrS(double arg1, int arg2, double arg3, int arg4 );

	native short nativeDJrS(double arg1, long arg2 );

	native short nativeDJDJrS(double arg1, long arg2, double arg3, long arg4 );

	native short nativeDFrS(double arg1, float arg2 );

	native short nativeDFDFrS(double arg1, float arg2, double arg3, float arg4 );

	native short nativeDDrS(double arg1, double arg2 );

	native short nativeDDDDrS(double arg1, double arg2, double arg3, double arg4 );

	native int nativeBBrI(byte arg1, byte arg2 );

	native int nativeBBBBrI(byte arg1, byte arg2, byte arg3, byte arg4 );

	native int nativeBSrI(byte arg1, short arg2 );

	native int nativeBSBSrI(byte arg1, short arg2, byte arg3, short arg4 );

	native int nativeBIrI(byte arg1, int arg2 );

	native int nativeBIBIrI(byte arg1, int arg2, byte arg3, int arg4 );

	native int nativeBJrI(byte arg1, long arg2 );

	native int nativeBJBJrI(byte arg1, long arg2, byte arg3, long arg4 );

	native int nativeBFrI(byte arg1, float arg2 );

	native int nativeBFBFrI(byte arg1, float arg2, byte arg3, float arg4 );

	native int nativeBDrI(byte arg1, double arg2 );

	native int nativeBDBDrI(byte arg1, double arg2, byte arg3, double arg4 );

	native int nativeSBrI(short arg1, byte arg2 );

	native int nativeSBSBrI(short arg1, byte arg2, short arg3, byte arg4 );

	native int nativeSSrI(short arg1, short arg2 );

	native int nativeSSSSrI(short arg1, short arg2, short arg3, short arg4 );

	native int nativeSIrI(short arg1, int arg2 );

	native int nativeSISIrI(short arg1, int arg2, short arg3, int arg4 );

	native int nativeSJrI(short arg1, long arg2 );

	native int nativeSJSJrI(short arg1, long arg2, short arg3, long arg4 );

	native int nativeSFrI(short arg1, float arg2 );

	native int nativeSFSFrI(short arg1, float arg2, short arg3, float arg4 );

	native int nativeSDrI(short arg1, double arg2 );

	native int nativeSDSDrI(short arg1, double arg2, short arg3, double arg4 );

	native int nativeIBrI(int arg1, byte arg2 );

	native int nativeIBIBrI(int arg1, byte arg2, int arg3, byte arg4 );

	native int nativeISrI(int arg1, short arg2 );

	native int nativeISISrI(int arg1, short arg2, int arg3, short arg4 );

	native int nativeIIrI(int arg1, int arg2 );

	native int nativeIIIIrI(int arg1, int arg2, int arg3, int arg4 );

	native int nativeIJrI(int arg1, long arg2 );

	native int nativeIJIJrI(int arg1, long arg2, int arg3, long arg4 );

	native int nativeIFrI(int arg1, float arg2 );

	native int nativeIFIFrI(int arg1, float arg2, int arg3, float arg4 );

	native int nativeIDrI(int arg1, double arg2 );

	native int nativeIDIDrI(int arg1, double arg2, int arg3, double arg4 );

	native int nativeJBrI(long arg1, byte arg2 );

	native int nativeJBJBrI(long arg1, byte arg2, long arg3, byte arg4 );

	native int nativeJSrI(long arg1, short arg2 );

	native int nativeJSJSrI(long arg1, short arg2, long arg3, short arg4 );

	native int nativeJIrI(long arg1, int arg2 );

	native int nativeJIJIrI(long arg1, int arg2, long arg3, int arg4 );

	native int nativeJJrI(long arg1, long arg2 );

	native int nativeJJJJrI(long arg1, long arg2, long arg3, long arg4 );

	native int nativeJFrI(long arg1, float arg2 );

	native int nativeJFJFrI(long arg1, float arg2, long arg3, float arg4 );

	native int nativeJDrI(long arg1, double arg2 );

	native int nativeJDJDrI(long arg1, double arg2, long arg3, double arg4 );

	native int nativeFBrI(float arg1, byte arg2 );

	native int nativeFBFBrI(float arg1, byte arg2, float arg3, byte arg4 );

	native int nativeFSrI(float arg1, short arg2 );

	native int nativeFSFSrI(float arg1, short arg2, float arg3, short arg4 );

	native int nativeFIrI(float arg1, int arg2 );

	native int nativeFIFIrI(float arg1, int arg2, float arg3, int arg4 );

	native int nativeFJrI(float arg1, long arg2 );

	native int nativeFJFJrI(float arg1, long arg2, float arg3, long arg4 );

	native int nativeFFrI(float arg1, float arg2 );

	native int nativeFFFFrI(float arg1, float arg2, float arg3, float arg4 );

	native int nativeFDrI(float arg1, double arg2 );

	native int nativeFDFDrI(float arg1, double arg2, float arg3, double arg4 );

	native int nativeDBrI(double arg1, byte arg2 );

	native int nativeDBDBrI(double arg1, byte arg2, double arg3, byte arg4 );

	native int nativeDSrI(double arg1, short arg2 );

	native int nativeDSDSrI(double arg1, short arg2, double arg3, short arg4 );

	native int nativeDIrI(double arg1, int arg2 );

	native int nativeDIDIrI(double arg1, int arg2, double arg3, int arg4 );

	native int nativeDJrI(double arg1, long arg2 );

	native int nativeDJDJrI(double arg1, long arg2, double arg3, long arg4 );

	native int nativeDFrI(double arg1, float arg2 );

	native int nativeDFDFrI(double arg1, float arg2, double arg3, float arg4 );

	native int nativeDDrI(double arg1, double arg2 );

	native int nativeDDDDrI(double arg1, double arg2, double arg3, double arg4 );

	native long nativeBBrJ(byte arg1, byte arg2 );

	native long nativeBBBBrJ(byte arg1, byte arg2, byte arg3, byte arg4 );

	native long nativeBSrJ(byte arg1, short arg2 );

	native long nativeBSBSrJ(byte arg1, short arg2, byte arg3, short arg4 );

	native long nativeBIrJ(byte arg1, int arg2 );

	native long nativeBIBIrJ(byte arg1, int arg2, byte arg3, int arg4 );

	native long nativeBJrJ(byte arg1, long arg2 );

	native long nativeBJBJrJ(byte arg1, long arg2, byte arg3, long arg4 );

	native long nativeBFrJ(byte arg1, float arg2 );

	native long nativeBFBFrJ(byte arg1, float arg2, byte arg3, float arg4 );

	native long nativeBDrJ(byte arg1, double arg2 );

	native long nativeBDBDrJ(byte arg1, double arg2, byte arg3, double arg4 );

	native long nativeSBrJ(short arg1, byte arg2 );

	native long nativeSBSBrJ(short arg1, byte arg2, short arg3, byte arg4 );

	native long nativeSSrJ(short arg1, short arg2 );

	native long nativeSSSSrJ(short arg1, short arg2, short arg3, short arg4 );

	native long nativeSIrJ(short arg1, int arg2 );

	native long nativeSISIrJ(short arg1, int arg2, short arg3, int arg4 );

	native long nativeSJrJ(short arg1, long arg2 );

	native long nativeSJSJrJ(short arg1, long arg2, short arg3, long arg4 );

	native long nativeSFrJ(short arg1, float arg2 );

	native long nativeSFSFrJ(short arg1, float arg2, short arg3, float arg4 );

	native long nativeSDrJ(short arg1, double arg2 );

	native long nativeSDSDrJ(short arg1, double arg2, short arg3, double arg4 );

	native long nativeIBrJ(int arg1, byte arg2 );

	native long nativeIBIBrJ(int arg1, byte arg2, int arg3, byte arg4 );

	native long nativeISrJ(int arg1, short arg2 );

	native long nativeISISrJ(int arg1, short arg2, int arg3, short arg4 );

	native long nativeIIrJ(int arg1, int arg2 );

	native long nativeIIIIrJ(int arg1, int arg2, int arg3, int arg4 );

	native long nativeIJrJ(int arg1, long arg2 );

	native long nativeIJIJrJ(int arg1, long arg2, int arg3, long arg4 );

	native long nativeIFrJ(int arg1, float arg2 );

	native long nativeIFIFrJ(int arg1, float arg2, int arg3, float arg4 );

	native long nativeIDrJ(int arg1, double arg2 );

	native long nativeIDIDrJ(int arg1, double arg2, int arg3, double arg4 );

	native long nativeJBrJ(long arg1, byte arg2 );

	native long nativeJBJBrJ(long arg1, byte arg2, long arg3, byte arg4 );

	native long nativeJSrJ(long arg1, short arg2 );

	native long nativeJSJSrJ(long arg1, short arg2, long arg3, short arg4 );

	native long nativeJIrJ(long arg1, int arg2 );

	native long nativeJIJIrJ(long arg1, int arg2, long arg3, int arg4 );

	native long nativeJJrJ(long arg1, long arg2 );

	native long nativeJJJJrJ(long arg1, long arg2, long arg3, long arg4 );

	native long nativeJFrJ(long arg1, float arg2 );

	native long nativeJFJFrJ(long arg1, float arg2, long arg3, float arg4 );

	native long nativeJDrJ(long arg1, double arg2 );

	native long nativeJDJDrJ(long arg1, double arg2, long arg3, double arg4 );

	native long nativeFBrJ(float arg1, byte arg2 );

	native long nativeFBFBrJ(float arg1, byte arg2, float arg3, byte arg4 );

	native long nativeFSrJ(float arg1, short arg2 );

	native long nativeFSFSrJ(float arg1, short arg2, float arg3, short arg4 );

	native long nativeFIrJ(float arg1, int arg2 );

	native long nativeFIFIrJ(float arg1, int arg2, float arg3, int arg4 );

	native long nativeFJrJ(float arg1, long arg2 );

	native long nativeFJFJrJ(float arg1, long arg2, float arg3, long arg4 );

	native long nativeFFrJ(float arg1, float arg2 );

	native long nativeFFFFrJ(float arg1, float arg2, float arg3, float arg4 );

	native long nativeFDrJ(float arg1, double arg2 );

	native long nativeFDFDrJ(float arg1, double arg2, float arg3, double arg4 );

	native long nativeDBrJ(double arg1, byte arg2 );

	native long nativeDBDBrJ(double arg1, byte arg2, double arg3, byte arg4 );

	native long nativeDSrJ(double arg1, short arg2 );

	native long nativeDSDSrJ(double arg1, short arg2, double arg3, short arg4 );

	native long nativeDIrJ(double arg1, int arg2 );

	native long nativeDIDIrJ(double arg1, int arg2, double arg3, int arg4 );

	native long nativeDJrJ(double arg1, long arg2 );

	native long nativeDJDJrJ(double arg1, long arg2, double arg3, long arg4 );

	native long nativeDFrJ(double arg1, float arg2 );

	native long nativeDFDFrJ(double arg1, float arg2, double arg3, float arg4 );

	native long nativeDDrJ(double arg1, double arg2 );

	native long nativeDDDDrJ(double arg1, double arg2, double arg3, double arg4 );

	native float nativeBBrF(byte arg1, byte arg2 );

	native float nativeBBBBrF(byte arg1, byte arg2, byte arg3, byte arg4 );

	native float nativeBSrF(byte arg1, short arg2 );

	native float nativeBSBSrF(byte arg1, short arg2, byte arg3, short arg4 );

	native float nativeBIrF(byte arg1, int arg2 );

	native float nativeBIBIrF(byte arg1, int arg2, byte arg3, int arg4 );

	native float nativeBJrF(byte arg1, long arg2 );

	native float nativeBJBJrF(byte arg1, long arg2, byte arg3, long arg4 );

	native float nativeBFrF(byte arg1, float arg2 );

	native float nativeBFBFrF(byte arg1, float arg2, byte arg3, float arg4 );

	native float nativeBDrF(byte arg1, double arg2 );

	native float nativeBDBDrF(byte arg1, double arg2, byte arg3, double arg4 );

	native float nativeSBrF(short arg1, byte arg2 );

	native float nativeSBSBrF(short arg1, byte arg2, short arg3, byte arg4 );

	native float nativeSSrF(short arg1, short arg2 );

	native float nativeSSSSrF(short arg1, short arg2, short arg3, short arg4 );

	native float nativeSIrF(short arg1, int arg2 );

	native float nativeSISIrF(short arg1, int arg2, short arg3, int arg4 );

	native float nativeSJrF(short arg1, long arg2 );

	native float nativeSJSJrF(short arg1, long arg2, short arg3, long arg4 );

	native float nativeSFrF(short arg1, float arg2 );

	native float nativeSFSFrF(short arg1, float arg2, short arg3, float arg4 );

	native float nativeSDrF(short arg1, double arg2 );

	native float nativeSDSDrF(short arg1, double arg2, short arg3, double arg4 );

	native float nativeIBrF(int arg1, byte arg2 );

	native float nativeIBIBrF(int arg1, byte arg2, int arg3, byte arg4 );

	native float nativeISrF(int arg1, short arg2 );

	native float nativeISISrF(int arg1, short arg2, int arg3, short arg4 );

	native float nativeIIrF(int arg1, int arg2 );

	native float nativeIIIIrF(int arg1, int arg2, int arg3, int arg4 );

	native float nativeIJrF(int arg1, long arg2 );

	native float nativeIJIJrF(int arg1, long arg2, int arg3, long arg4 );

	native float nativeIFrF(int arg1, float arg2 );

	native float nativeIFIFrF(int arg1, float arg2, int arg3, float arg4 );

	native float nativeIDrF(int arg1, double arg2 );

	native float nativeIDIDrF(int arg1, double arg2, int arg3, double arg4 );

	native float nativeJBrF(long arg1, byte arg2 );

	native float nativeJBJBrF(long arg1, byte arg2, long arg3, byte arg4 );

	native float nativeJSrF(long arg1, short arg2 );

	native float nativeJSJSrF(long arg1, short arg2, long arg3, short arg4 );

	native float nativeJIrF(long arg1, int arg2 );

	native float nativeJIJIrF(long arg1, int arg2, long arg3, int arg4 );

	native float nativeJJrF(long arg1, long arg2 );

	native float nativeJJJJrF(long arg1, long arg2, long arg3, long arg4 );

	native float nativeJFrF(long arg1, float arg2 );

	native float nativeJFJFrF(long arg1, float arg2, long arg3, float arg4 );

	native float nativeJDrF(long arg1, double arg2 );

	native float nativeJDJDrF(long arg1, double arg2, long arg3, double arg4 );

	native float nativeFBrF(float arg1, byte arg2 );

	native float nativeFBFBrF(float arg1, byte arg2, float arg3, byte arg4 );

	native float nativeFSrF(float arg1, short arg2 );

	native float nativeFSFSrF(float arg1, short arg2, float arg3, short arg4 );

	native float nativeFIrF(float arg1, int arg2 );

	native float nativeFIFIrF(float arg1, int arg2, float arg3, int arg4 );

	native float nativeFJrF(float arg1, long arg2 );

	native float nativeFJFJrF(float arg1, long arg2, float arg3, long arg4 );

	native float nativeFFrF(float arg1, float arg2 );

	native float nativeFFFFrF(float arg1, float arg2, float arg3, float arg4 );

	native float nativeFDrF(float arg1, double arg2 );

	native float nativeFDFDrF(float arg1, double arg2, float arg3, double arg4 );

	native float nativeDBrF(double arg1, byte arg2 );

	native float nativeDBDBrF(double arg1, byte arg2, double arg3, byte arg4 );

	native float nativeDSrF(double arg1, short arg2 );

	native float nativeDSDSrF(double arg1, short arg2, double arg3, short arg4 );

	native float nativeDIrF(double arg1, int arg2 );

	native float nativeDIDIrF(double arg1, int arg2, double arg3, int arg4 );

	native float nativeDJrF(double arg1, long arg2 );

	native float nativeDJDJrF(double arg1, long arg2, double arg3, long arg4 );

	native float nativeDFrF(double arg1, float arg2 );

	native float nativeDFDFrF(double arg1, float arg2, double arg3, float arg4 );

	native float nativeDDrF(double arg1, double arg2 );

	native float nativeDDDDrF(double arg1, double arg2, double arg3, double arg4 );

	native double nativeBBrD(byte arg1, byte arg2 );

	native double nativeBBBBrD(byte arg1, byte arg2, byte arg3, byte arg4 );

	native double nativeBSrD(byte arg1, short arg2 );

	native double nativeBSBSrD(byte arg1, short arg2, byte arg3, short arg4 );

	native double nativeBIrD(byte arg1, int arg2 );

	native double nativeBIBIrD(byte arg1, int arg2, byte arg3, int arg4 );

	native double nativeBJrD(byte arg1, long arg2 );

	native double nativeBJBJrD(byte arg1, long arg2, byte arg3, long arg4 );

	native double nativeBFrD(byte arg1, float arg2 );

	native double nativeBFBFrD(byte arg1, float arg2, byte arg3, float arg4 );

	native double nativeBDrD(byte arg1, double arg2 );

	native double nativeBDBDrD(byte arg1, double arg2, byte arg3, double arg4 );

	native double nativeSBrD(short arg1, byte arg2 );

	native double nativeSBSBrD(short arg1, byte arg2, short arg3, byte arg4 );

	native double nativeSSrD(short arg1, short arg2 );

	native double nativeSSSSrD(short arg1, short arg2, short arg3, short arg4 );

	native double nativeSIrD(short arg1, int arg2 );

	native double nativeSISIrD(short arg1, int arg2, short arg3, int arg4 );

	native double nativeSJrD(short arg1, long arg2 );

	native double nativeSJSJrD(short arg1, long arg2, short arg3, long arg4 );

	native double nativeSFrD(short arg1, float arg2 );

	native double nativeSFSFrD(short arg1, float arg2, short arg3, float arg4 );

	native double nativeSDrD(short arg1, double arg2 );

	native double nativeSDSDrD(short arg1, double arg2, short arg3, double arg4 );

	native double nativeIBrD(int arg1, byte arg2 );

	native double nativeIBIBrD(int arg1, byte arg2, int arg3, byte arg4 );

	native double nativeISrD(int arg1, short arg2 );

	native double nativeISISrD(int arg1, short arg2, int arg3, short arg4 );

	native double nativeIIrD(int arg1, int arg2 );

	native double nativeIIIIrD(int arg1, int arg2, int arg3, int arg4 );

	native double nativeIJrD(int arg1, long arg2 );

	native double nativeIJIJrD(int arg1, long arg2, int arg3, long arg4 );

	native double nativeIFrD(int arg1, float arg2 );

	native double nativeIFIFrD(int arg1, float arg2, int arg3, float arg4 );

	native double nativeIDrD(int arg1, double arg2 );

	native double nativeIDIDrD(int arg1, double arg2, int arg3, double arg4 );

	native double nativeJBrD(long arg1, byte arg2 );

	native double nativeJBJBrD(long arg1, byte arg2, long arg3, byte arg4 );

	native double nativeJSrD(long arg1, short arg2 );

	native double nativeJSJSrD(long arg1, short arg2, long arg3, short arg4 );

	native double nativeJIrD(long arg1, int arg2 );

	native double nativeJIJIrD(long arg1, int arg2, long arg3, int arg4 );

	native double nativeJJrD(long arg1, long arg2 );

	native double nativeJJJJrD(long arg1, long arg2, long arg3, long arg4 );

	native double nativeJFrD(long arg1, float arg2 );

	native double nativeJFJFrD(long arg1, float arg2, long arg3, float arg4 );

	native double nativeJDrD(long arg1, double arg2 );

	native double nativeJDJDrD(long arg1, double arg2, long arg3, double arg4 );

	native double nativeFBrD(float arg1, byte arg2 );

	native double nativeFBFBrD(float arg1, byte arg2, float arg3, byte arg4 );

	native double nativeFSrD(float arg1, short arg2 );

	native double nativeFSFSrD(float arg1, short arg2, float arg3, short arg4 );

	native double nativeFIrD(float arg1, int arg2 );

	native double nativeFIFIrD(float arg1, int arg2, float arg3, int arg4 );

	native double nativeFJrD(float arg1, long arg2 );

	native double nativeFJFJrD(float arg1, long arg2, float arg3, long arg4 );

	native double nativeFFrD(float arg1, float arg2 );

	native double nativeFFFFrD(float arg1, float arg2, float arg3, float arg4 );

	native double nativeFDrD(float arg1, double arg2 );

	native double nativeFDFDrD(float arg1, double arg2, float arg3, double arg4 );

	native double nativeDBrD(double arg1, byte arg2 );

	native double nativeDBDBrD(double arg1, byte arg2, double arg3, byte arg4 );

	native double nativeDSrD(double arg1, short arg2 );

	native double nativeDSDSrD(double arg1, short arg2, double arg3, short arg4 );

	native double nativeDIrD(double arg1, int arg2 );

	native double nativeDIDIrD(double arg1, int arg2, double arg3, int arg4 );

	native double nativeDJrD(double arg1, long arg2 );

	native double nativeDJDJrD(double arg1, long arg2, double arg3, long arg4 );

	native double nativeDFrD(double arg1, float arg2 );

	native double nativeDFDFrD(double arg1, float arg2, double arg3, float arg4 );

	native double nativeDDrD(double arg1, double arg2 );

	native double nativeDDDDrD(double arg1, double arg2, double arg3, double arg4 );

	native long nativeIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIrJ(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6, int arg7, int arg8, int arg9, int arg10, int arg11, int arg12, int arg13, int arg14, int arg15, int arg16, int arg17, int arg18, int arg19, int arg20, int arg21, int arg22, int arg23, int arg24, int arg25, int arg26, int arg27, int arg28, int arg29, int arg30, int arg31, int arg32, int arg33, int arg34, int arg35, int arg36, int arg37, int arg38, int arg39, int arg40, int arg41, int arg42, int arg43, int arg44, int arg45, int arg46, int arg47, int arg48, int arg49, int arg50, int arg51, int arg52, int arg53, int arg54, int arg55, int arg56, int arg57, int arg58, int arg59, int arg60, int arg61, int arg62, int arg63, int arg64, int arg65, int arg66, int arg67, int arg68, int arg69, int arg70, int arg71, int arg72, int arg73, int arg74, int arg75, int arg76, int arg77, int arg78, int arg79, int arg80, int arg81, int arg82, int arg83, int arg84, int arg85, int arg86, int arg87, int arg88, int arg89, int arg90, int arg91, int arg92, int arg93, int arg94, int arg95, int arg96, int arg97, int arg98, int arg99, int arg100, int arg101, int arg102, int arg103, int arg104, int arg105, int arg106, int arg107, int arg108, int arg109, int arg110, int arg111, int arg112, int arg113, int arg114, int arg115, int arg116, int arg117, int arg118, int arg119, int arg120, int arg121, int arg122, int arg123, int arg124, int arg125, int arg126, int arg127, int arg128, int arg129, int arg130, int arg131, int arg132, int arg133, int arg134, int arg135, int arg136, int arg137, int arg138, int arg139, int arg140, int arg141, int arg142, int arg143, int arg144, int arg145, int arg146, int arg147, int arg148, int arg149, int arg150, int arg151, int arg152, int arg153, int arg154, int arg155, int arg156, int arg157, int arg158, int arg159, int arg160, int arg161, int arg162, int arg163, int arg164, int arg165, int arg166, int arg167, int arg168, int arg169, int arg170, int arg171, int arg172, int arg173, int arg174, int arg175, int arg176, int arg177, int arg178, int arg179, int arg180, int arg181, int arg182, int arg183, int arg184, int arg185, int arg186, int arg187, int arg188, int arg189, int arg190, int arg191, int arg192, int arg193, int arg194, int arg195, int arg196, int arg197, int arg198, int arg199, int arg200, int arg201, int arg202, int arg203, int arg204, int arg205, int arg206, int arg207, int arg208, int arg209, int arg210, int arg211, int arg212, int arg213, int arg214, int arg215, int arg216, int arg217, int arg218, int arg219, int arg220, int arg221, int arg222, int arg223, int arg224, int arg225, int arg226, int arg227, int arg228, int arg229, int arg230, int arg231, int arg232, int arg233, int arg234, int arg235, int arg236, int arg237, int arg238, int arg239, int arg240, int arg241, int arg242, int arg243, int arg244, int arg245, int arg246, int arg247, int arg248, int arg249, int arg250, int arg251, int arg252, int arg253, int arg254 );

	native int nativeIJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJJrI(int arg1, long arg2, long arg3, long arg4, long arg5, long arg6, long arg7, long arg8, long arg9, long arg10, long arg11, long arg12, long arg13, long arg14, long arg15, long arg16, long arg17, long arg18, long arg19, long arg20, long arg21, long arg22, long arg23, long arg24, long arg25, long arg26, long arg27, long arg28, long arg29, long arg30, long arg31, long arg32, long arg33, long arg34, long arg35, long arg36, long arg37, long arg38, long arg39, long arg40, long arg41, long arg42, long arg43, long arg44, long arg45, long arg46, long arg47, long arg48, long arg49, long arg50, long arg51, long arg52, long arg53, long arg54, long arg55, long arg56, long arg57, long arg58, long arg59, long arg60, long arg61, long arg62, long arg63, long arg64, long arg65, long arg66, long arg67, long arg68, long arg69, long arg70, long arg71, long arg72, long arg73, long arg74, long arg75, long arg76, long arg77, long arg78, long arg79, long arg80, long arg81, long arg82, long arg83, long arg84, long arg85, long arg86, long arg87, long arg88, long arg89, long arg90, long arg91, long arg92, long arg93, long arg94, long arg95, long arg96, long arg97, long arg98, long arg99, long arg100, long arg101, long arg102, long arg103, long arg104, long arg105, long arg106, long arg107, long arg108, long arg109, long arg110, long arg111, long arg112, long arg113, long arg114, long arg115, long arg116, long arg117, long arg118, long arg119, long arg120, long arg121, long arg122, long arg123, long arg124, long arg125, long arg126, long arg127 );

	native long nativeJJJJJJJJJJJJrJ(long arg1, long arg2, long arg3, long arg4, long arg5, long arg6, long arg7, long arg8, long arg9, long arg10, long arg11, long arg12 );

	native long nativeIJJJJJJJJJJJJrJ(int arg1, long arg2, long arg3, long arg4, long arg5, long arg6, long arg7, long arg8, long arg9, long arg10, long arg11, long arg12, long arg13 );

	native long nativeIJIJIJIJIJIJrJ(int arg1, long arg2, int arg3, long arg4, int arg5, long arg6, int arg7, long arg8, int arg9, long arg10, int arg11, long arg12 );

	native long nativeJIJIJIJIJIJIrJ(long arg1, int arg2, long arg3, int arg4, long arg5, int arg6, long arg7, int arg8, long arg9, int arg10, long arg11, int arg12 );

	native long nativeFFFFFFFFFFFFrJ(float arg1, float arg2, float arg3, float arg4, float arg5, float arg6, float arg7, float arg8, float arg9, float arg10, float arg11, float arg12 );

	native long nativeDDDDDDDDDDDDrJ(double arg1, double arg2, double arg3, double arg4, double arg5, double arg6, double arg7, double arg8, double arg9, double arg10, double arg11, double arg12 );

	native long nativeFDFDFDFDFDFDrJ(float arg1, double arg2, float arg3, double arg4, float arg5, double arg6, float arg7, double arg8, float arg9, double arg10, float arg11, double arg12 );

	native long nativeDFDFDFDFDFDFrJ(double arg1, float arg2, double arg3, float arg4, double arg5, float arg6, double arg7, float arg8, double arg9, float arg10, double arg11, float arg12 );

	native long nativeBBSSIJFDIFDFDFDBBSSIJFDrJ(byte arg1, byte arg2, short arg3, short arg4, int arg5, long arg6, float arg7, double arg8, int arg9, float arg10, double arg11, float arg12, double arg13, float arg14, double arg15, byte arg16, byte arg17, short arg18, short arg19, int arg20, long arg21, float arg22, double arg23 );

}
