/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
package com.ibm.jvm.trace.format.api;

import java.io.UnsupportedEncodingException;
import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;

public class DataHeader {

	protected byte eyecatcher[];
	protected int length;
	protected int version;
	protected int modification;
	protected String signature;
	boolean ascii = true;

	protected static final int DATAHEADER_SIZE = 16;

	int AscToEbcDef[] = {
			0x00,0x01,0x02,0x03,0x37,0x2D,0x2E,0x2F,0x16,0x05,0x25,0x0B,0x0C,0x0D,0x0E,0x0F,
			0x10,0x11,0x12,0x13,0x3C,0x3D,0x32,0x26,0x18,0x19,0x3F,0x27,0x22,0x1D,0x35,0x1F,
			0x40,0x5A,0x7F,0x7B,0x5B,0x6C,0x50,0x7D,0x4D,0x5D,0x5C,0x4E,0x6B,0x60,0x4B,0x61,
			0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0x7A,0x5E,0x4C,0x7E,0x6E,0x6F,
			0x7C,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,
			0xD7,0xD8,0xD9,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xAD,0xE0,0xBD,0x5F,0x6D,
			0x79,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x91,0x92,0x93,0x94,0x95,0x96,
			0x97,0x98,0x99,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xC0,0x4F,0xD0,0xA1,0x07,
			0x43,0x20,0x21,0x1C,0x23,0xEB,0x24,0x9B,0x71,0x28,0x38,0x49,0x90,0xBA,0xEC,0xDF,
			0x45,0x29,0x2A,0x9D,0x72,0x2B,0x8A,0x9A,0x67,0x56,0x64,0x4A,0x53,0x68,0x59,0x46,
			0xEA,0xDA,0x2C,0xDE,0x8B,0x55,0x41,0xFE,0x58,0x51,0x52,0x48,0x69,0xDB,0x8E,0x8D,
			0x73,0x74,0x75,0xFA,0x15,0xB0,0xB1,0xB3,0xB4,0xB5,0x6A,0xB7,0xB8,0xB9,0xCC,0xBC,
			0xAB,0x3E,0x3B,0x0A,0xBF,0x8F,0x3A,0x14,0xA0,0x17,0xCB,0xCA,0x1A,0x1B,0x9C,0x04,
			0x34,0xEF,0x1E,0x06,0x08,0x09,0x77,0x70,0xBE,0xBB,0xAC,0x54,0x63,0x65,0x66,0x62,
			0x30,0x42,0x47,0x57,0xEE,0x33,0xB6,0xE1,0xCD,0xED,0x36,0x44,0xCE,0xCF,0x31,0xAA,
			0xFC,0x9E,0xAE,0x8C,0xDD,0xDC,0x39,0xFB,0x80,0xAF,0xFD,0x78,0x76,0xB2,0x9F,0xFF
			};

	public DataHeader(TraceContext context, ByteBuffer data, String signature) throws IllegalArgumentException {
		this(context, data, signature, true);
	}
	
	protected DataHeader(TraceContext context, ByteBuffer data, String signature, boolean confirmBody) throws IllegalArgumentException {
		/* check there's enough data to reconstruct the UtDataHeader */
		if (data.remaining() < DATAHEADER_SIZE) {
			context.error(this, "Truncated data header");

			throw new BufferUnderflowException();
		}

		/*
		 * TODO: check the section eyecatcher against the three
		 * permutations of the provided signature: little endian/ascii
		 * big endian/ascii big endian/ebcdic
		 */
		byte sigBytes[];
		try {
			sigBytes = signature.getBytes("ASCII");
		} catch (UnsupportedEncodingException e) {
			/* asci encoding should always be present */
			throw new RuntimeException("No ascii charset available");
		}

		eyecatcher = new byte[sigBytes.length];
		data.get(eyecatcher);

		for (int i = 0; i < sigBytes.length; i++) {
			/* ascii */
			if (eyecatcher[i] == sigBytes[i]) {
				if (ascii) {
					continue;
				}
			}

			/* ebcdic */
			if (eyecatcher[i] == AscToEbcDef[sigBytes[i] & 0xFF]) {
				ascii = false;
				continue;
			}

			throw new IllegalArgumentException("Eyecatcher \""+signature+"\" not found");
		}

		length = data.getInt();
		version = data.getInt();
		modification = data.getInt();
		this.signature = signature;
		
		if (confirmBody) {
			/* check there's sufficient data available to read the structure */
			if (data.remaining() < length - DATAHEADER_SIZE) {
				context.error(this, "Truncated " + signature + " section, bytes required: " + (length - DATAHEADER_SIZE) + ", bytes available: " + data.remaining());

				throw new BufferUnderflowException();
			}
		}
	}

	public String toString() {
		return
		this.signature+": eyecatcher:         " + eyecatcher + System.getProperty("line.separator") +
		this.signature+": length:             " + length + System.getProperty("line.separator") +
		this.signature+": version:            " + version + System.getProperty("line.separator") +
		this.signature+": modification:       " + modification + System.getProperty("line.separator");
	}
}
