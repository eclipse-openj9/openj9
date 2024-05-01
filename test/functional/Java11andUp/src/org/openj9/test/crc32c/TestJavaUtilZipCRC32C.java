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

package org.openj9.test.crc32c;
import org.testng.AssertJUnit;
import org.testng.annotations.Test;

import java.util.zip.CRC32C;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class TestJavaUtilZipCRC32C {
    static class TestSet {
        long expected;
        byte[] data;

        TestSet(long i, byte[] b) {
            expected = i;
            data = b;
        }
    }

    private static final TestSet testSets[] = {
        // 15B
        new TestSet(0xf62e6c8aL, new byte[] {
            (byte) 0xc6, (byte) 0xb1, (byte) 0x93, (byte) 0xd2, (byte) 0x2b, (byte) 0xed, (byte) 0x3f, (byte) 0xc8,
            (byte) 0x0c, (byte) 0x32, (byte) 0xb0, (byte) 0x9c, (byte) 0xee, (byte) 0x82, (byte) 0x69
        }),
        // 16B
        new TestSet(0x1b4782d1L, new byte[] {
            (byte) 0x9d, (byte) 0x5f, (byte) 0x49, (byte) 0x94, (byte) 0x7b, (byte) 0xf3, (byte) 0x4c, (byte) 0xee,
            (byte) 0x5c, (byte) 0xe1, (byte) 0x37, (byte) 0x36, (byte) 0x7f, (byte) 0x8f, (byte) 0xd0, (byte) 0x1d
        }),
        // 16B + 15B
        new TestSet(0x4544d8a8L, new byte[] {
            (byte) 0xde, (byte) 0x36, (byte) 0x14, (byte) 0x4d, (byte) 0x97, (byte) 0x9d, (byte) 0xa8, (byte) 0x99,
            (byte) 0xe8, (byte) 0xa1, (byte) 0xa9, (byte) 0xb0, (byte) 0xc4, (byte) 0xdc, (byte) 0xb3, (byte) 0xf7,

            (byte) 0x7d, (byte) 0xa1, (byte) 0x6e, (byte) 0x59, (byte) 0xb7, (byte) 0x44, (byte) 0x7b, (byte) 0x46,
            (byte) 0x5e, (byte) 0xc1, (byte) 0x62, (byte) 0x2a, (byte) 0xb4, (byte) 0xec, (byte) 0x49
        }),
        // 3*16B
        new TestSet(0xdbd72e95L, new byte[] {
            (byte) 0xdd, (byte) 0x37, (byte) 0xae, (byte) 0x38, (byte) 0x70, (byte) 0xde, (byte) 0x60, (byte) 0x05,
            (byte) 0x28, (byte) 0x39, (byte) 0x6d, (byte) 0xde, (byte) 0x5a, (byte) 0x3a, (byte) 0xab, (byte) 0xbd,

            (byte) 0x13, (byte) 0x18, (byte) 0x91, (byte) 0xea, (byte) 0x46, (byte) 0xd0, (byte) 0xb9, (byte) 0xd6,
            (byte) 0x4c, (byte) 0xb3, (byte) 0x6e, (byte) 0xe3, (byte) 0xff, (byte) 0x67, (byte) 0xf7, (byte) 0x33,

            (byte) 0x16, (byte) 0xb0, (byte) 0xc3, (byte) 0xaa, (byte) 0x6a, (byte) 0xc7, (byte) 0x5e, (byte) 0x8b,
            (byte) 0x41, (byte) 0xed, (byte) 0xe6, (byte) 0x8c, (byte) 0x32, (byte) 0xd5, (byte) 0xd6, (byte) 0x95
        }),
        // 3*16B + 15B
        new TestSet(0xe007127eL, new byte[] {
            (byte) 0x1f, (byte) 0x6e, (byte) 0xbb, (byte) 0x71, (byte) 0xc3, (byte) 0x02, (byte) 0xb8, (byte) 0x05,
            (byte) 0x5b, (byte) 0xba, (byte) 0xcd, (byte) 0xec, (byte) 0xf4, (byte) 0xf4, (byte) 0x78, (byte) 0x64,

            (byte) 0x22, (byte) 0xfa, (byte) 0x3d, (byte) 0x3c, (byte) 0x26, (byte) 0x81, (byte) 0x0a, (byte) 0x67,
            (byte) 0x6b, (byte) 0x82, (byte) 0x99, (byte) 0x48, (byte) 0x90, (byte) 0x07, (byte) 0x37, (byte) 0xd3,

            (byte) 0xfd, (byte) 0xd1, (byte) 0x7e, (byte) 0x4a, (byte) 0x6b, (byte) 0xe6, (byte) 0x68, (byte) 0x1b,
            (byte) 0x8a, (byte) 0xf6, (byte) 0x5c, (byte) 0xa9, (byte) 0xe5, (byte) 0x4b, (byte) 0x3b, (byte) 0x89,

            (byte) 0x31, (byte) 0x09, (byte) 0xa8, (byte) 0x64, (byte) 0x95, (byte) 0x65, (byte) 0x9d, (byte) 0x7f,
            (byte) 0xce, (byte) 0xd4, (byte) 0xc8, (byte) 0x50, (byte) 0xb9, (byte) 0x01, (byte) 0xd3
        }),
        // 64B
        new TestSet(0xb482fc35L, new byte[] {
            (byte) 0x2c, (byte) 0x24, (byte) 0xda, (byte) 0xea, (byte) 0x7a, (byte) 0x0e, (byte) 0x69, (byte) 0x55,
            (byte) 0xfe, (byte) 0xde, (byte) 0xb1, (byte) 0x0a, (byte) 0x4e, (byte) 0x9f, (byte) 0x3f, (byte) 0xa7,
            (byte) 0x22, (byte) 0x13, (byte) 0x89, (byte) 0x90, (byte) 0xac, (byte) 0xbd, (byte) 0x3a, (byte) 0x0b,
            (byte) 0xdf, (byte) 0x9a, (byte) 0x22, (byte) 0x9f, (byte) 0x81, (byte) 0x18, (byte) 0xc8, (byte) 0xab,
            (byte) 0xfe, (byte) 0xc0, (byte) 0xbe, (byte) 0xa9, (byte) 0xb4, (byte) 0x94, (byte) 0x6f, (byte) 0x64,
            (byte) 0x19, (byte) 0xba, (byte) 0x37, (byte) 0x7e, (byte) 0x68, (byte) 0xcb, (byte) 0xb3, (byte) 0x67,
            (byte) 0x05, (byte) 0x4e, (byte) 0x20, (byte) 0xd7, (byte) 0x2e, (byte) 0x3b, (byte) 0x3e, (byte) 0x02,
            (byte) 0xbb, (byte) 0x81, (byte) 0xb7, (byte) 0x76, (byte) 0x32, (byte) 0x55, (byte) 0xc5, (byte) 0xa4
        }),
        // 64B + 15B
        new TestSet(0x858fa6f9L, new byte[] {
            (byte) 0x81, (byte) 0xbe, (byte) 0x69, (byte) 0xb5, (byte) 0x72, (byte) 0x75, (byte) 0x79, (byte) 0x2b,
            (byte) 0xfe, (byte) 0x7d, (byte) 0x7f, (byte) 0xf8, (byte) 0x4a, (byte) 0x43, (byte) 0x62, (byte) 0xae,
            (byte) 0x0a, (byte) 0x9f, (byte) 0x80, (byte) 0x47, (byte) 0xb8, (byte) 0x37, (byte) 0xe3, (byte) 0x5a,
            (byte) 0x6a, (byte) 0xb8, (byte) 0x91, (byte) 0xf5, (byte) 0x7f, (byte) 0xbc, (byte) 0x2c, (byte) 0x4f,
            (byte) 0x6b, (byte) 0x1e, (byte) 0x8e, (byte) 0xcf, (byte) 0xd4, (byte) 0x30, (byte) 0x75, (byte) 0xea,
            (byte) 0xeb, (byte) 0x17, (byte) 0x22, (byte) 0xd0, (byte) 0x70, (byte) 0x92, (byte) 0x10, (byte) 0x3f,
            (byte) 0x53, (byte) 0x90, (byte) 0x62, (byte) 0x50, (byte) 0x36, (byte) 0x82, (byte) 0x27, (byte) 0xe2,
            (byte) 0x35, (byte) 0xb1, (byte) 0x61, (byte) 0x91, (byte) 0x07, (byte) 0x68, (byte) 0x49, (byte) 0x5f,

            (byte) 0x15, (byte) 0x4b, (byte) 0xe1, (byte) 0xa1, (byte) 0x2a, (byte) 0xe2, (byte) 0xb1, (byte) 0xf8,
            (byte) 0x40, (byte) 0x6b, (byte) 0x32, (byte) 0x43, (byte) 0x39, (byte) 0x01, (byte) 0x9c
        }),
        // 64B + 3*16B
        new TestSet(0x797e87a9L, new byte[] {
            (byte) 0x60, (byte) 0x20, (byte) 0x2d, (byte) 0xd7, (byte) 0x5e, (byte) 0x8c, (byte) 0x06, (byte) 0x45,
            (byte) 0x8e, (byte) 0x04, (byte) 0xfe, (byte) 0x71, (byte) 0x0e, (byte) 0x01, (byte) 0x03, (byte) 0xcd,
            (byte) 0x33, (byte) 0x65, (byte) 0x9d, (byte) 0x12, (byte) 0x2d, (byte) 0x0d, (byte) 0x96, (byte) 0xfc,
            (byte) 0x36, (byte) 0xf7, (byte) 0x84, (byte) 0xc0, (byte) 0x94, (byte) 0xe6, (byte) 0x85, (byte) 0xd8,
            (byte) 0xe4, (byte) 0x48, (byte) 0xdf, (byte) 0xc3, (byte) 0x84, (byte) 0x40, (byte) 0xc0, (byte) 0x36,
            (byte) 0x36, (byte) 0x3c, (byte) 0xc6, (byte) 0xdb, (byte) 0xa5, (byte) 0x1e, (byte) 0x4d, (byte) 0xaf,
            (byte) 0x43, (byte) 0x4a, (byte) 0x87, (byte) 0x33, (byte) 0xf5, (byte) 0x5c, (byte) 0x93, (byte) 0xb9,
            (byte) 0x6f, (byte) 0x09, (byte) 0xf9, (byte) 0x56, (byte) 0x96, (byte) 0x85, (byte) 0xd8, (byte) 0xb1,

            (byte) 0xd6, (byte) 0x48, (byte) 0x58, (byte) 0x3b, (byte) 0x47, (byte) 0xda, (byte) 0xe9, (byte) 0xb5,
            (byte) 0x53, (byte) 0x03, (byte) 0x82, (byte) 0x8c, (byte) 0x4e, (byte) 0xc4, (byte) 0xbe, (byte) 0x0f,

            (byte) 0xcf, (byte) 0x82, (byte) 0xdb, (byte) 0xaa, (byte) 0x05, (byte) 0xf6, (byte) 0x72, (byte) 0xd2,
            (byte) 0x35, (byte) 0xed, (byte) 0x34, (byte) 0x3b, (byte) 0xce, (byte) 0xec, (byte) 0xa2, (byte) 0x09,

            (byte) 0x44, (byte) 0xfc, (byte) 0x52, (byte) 0x9d, (byte) 0x8c, (byte) 0xc9, (byte) 0x53, (byte) 0x8b,
            (byte) 0xbf, (byte) 0xbb, (byte) 0x77, (byte) 0x89, (byte) 0x30, (byte) 0x10, (byte) 0xe7, (byte) 0xa8
        }),
        // 64B + 3*16B + 15B
        new TestSet(0x9fe8bb4cL, new byte[] {
            (byte) 0x79, (byte) 0x81, (byte) 0x52, (byte) 0x5c, (byte) 0x08, (byte) 0xdf, (byte) 0x05, (byte) 0xab,
            (byte) 0xe0, (byte) 0x15, (byte) 0x02, (byte) 0x7d, (byte) 0x06, (byte) 0xa3, (byte) 0xc1, (byte) 0x02,
            (byte) 0xcc, (byte) 0x9d, (byte) 0x9d, (byte) 0x09, (byte) 0x81, (byte) 0x1d, (byte) 0x7a, (byte) 0x10,
            (byte) 0x69, (byte) 0xdf, (byte) 0x7a, (byte) 0xb1, (byte) 0x62, (byte) 0x16, (byte) 0x91, (byte) 0xad,
            (byte) 0xa7, (byte) 0x25, (byte) 0x6e, (byte) 0xac, (byte) 0xcf, (byte) 0x98, (byte) 0x72, (byte) 0x0c,
            (byte) 0x64, (byte) 0x05, (byte) 0xcd, (byte) 0xde, (byte) 0xd4, (byte) 0x1d, (byte) 0x35, (byte) 0x99,
            (byte) 0xdd, (byte) 0xa9, (byte) 0x83, (byte) 0x50, (byte) 0x25, (byte) 0x38, (byte) 0x94, (byte) 0x37,
            (byte) 0xe0, (byte) 0xe6, (byte) 0x8c, (byte) 0xd9, (byte) 0xee, (byte) 0xf0, (byte) 0x36, (byte) 0xe6,

            (byte) 0xf2, (byte) 0x8b, (byte) 0xfc, (byte) 0x35, (byte) 0x2b, (byte) 0x3f, (byte) 0xd1, (byte) 0xae,
            (byte) 0xe5, (byte) 0x74, (byte) 0x12, (byte) 0x4f, (byte) 0xf9, (byte) 0xd0, (byte) 0xff, (byte) 0x5a,

            (byte) 0x60, (byte) 0x0c, (byte) 0x47, (byte) 0x23, (byte) 0x38, (byte) 0x08, (byte) 0xa2, (byte) 0x13,
            (byte) 0x2f, (byte) 0x6b, (byte) 0xef, (byte) 0x81, (byte) 0x03, (byte) 0x68, (byte) 0xf3, (byte) 0xec,

            (byte) 0xca, (byte) 0xa4, (byte) 0x14, (byte) 0x4e, (byte) 0x82, (byte) 0xf7, (byte) 0x9e, (byte) 0xa1,
            (byte) 0x13, (byte) 0xa1, (byte) 0x4c, (byte) 0xfd, (byte) 0x9c, (byte) 0xd8, (byte) 0x31, (byte) 0x9f,

            (byte) 0xd1, (byte) 0x1d, (byte) 0x32, (byte) 0xbd, (byte) 0x13, (byte) 0xfc, (byte) 0x22, (byte) 0xa6,
            (byte) 0xd1, (byte) 0xdf, (byte) 0x8e, (byte) 0x54, (byte) 0x9f, (byte) 0xd9, (byte) 0x30
        }),
        // 3*64B
        new TestSet(0x3f9313c2L, new byte[] {
            (byte) 0xd0, (byte) 0x9c, (byte) 0xac, (byte) 0x08, (byte) 0xb9, (byte) 0x56, (byte) 0xcf, (byte) 0x35,
            (byte) 0x15, (byte) 0x70, (byte) 0x1c, (byte) 0x6e, (byte) 0xe7, (byte) 0x17, (byte) 0xc6, (byte) 0x82,
            (byte) 0xd8, (byte) 0x54, (byte) 0xba, (byte) 0xca, (byte) 0x04, (byte) 0xd7, (byte) 0x1e, (byte) 0xae,
            (byte) 0xc7, (byte) 0xa8, (byte) 0xa9, (byte) 0xe9, (byte) 0xd4, (byte) 0x2c, (byte) 0x5f, (byte) 0x13,
            (byte) 0xca, (byte) 0x29, (byte) 0x5b, (byte) 0xb5, (byte) 0x79, (byte) 0x4b, (byte) 0x18, (byte) 0x9a,
            (byte) 0x1a, (byte) 0x22, (byte) 0xc1, (byte) 0xd1, (byte) 0x91, (byte) 0x33, (byte) 0x23, (byte) 0x9e,
            (byte) 0x80, (byte) 0x85, (byte) 0x45, (byte) 0x51, (byte) 0xed, (byte) 0xcb, (byte) 0x9a, (byte) 0x70,
            (byte) 0xd4, (byte) 0x15, (byte) 0xfe, (byte) 0x7b, (byte) 0x5b, (byte) 0x43, (byte) 0xe3, (byte) 0xcb,

            (byte) 0x17, (byte) 0xa7, (byte) 0x98, (byte) 0x8d, (byte) 0xd4, (byte) 0x01, (byte) 0xde, (byte) 0x22,
            (byte) 0xbf, (byte) 0xd3, (byte) 0x9e, (byte) 0xd4, (byte) 0xb1, (byte) 0x38, (byte) 0x13, (byte) 0xdc,
            (byte) 0xa0, (byte) 0xba, (byte) 0xe2, (byte) 0xdb, (byte) 0x1f, (byte) 0x6a, (byte) 0x69, (byte) 0x22,
            (byte) 0x88, (byte) 0x89, (byte) 0x4a, (byte) 0x8d, (byte) 0xd3, (byte) 0x0b, (byte) 0x54, (byte) 0x21,
            (byte) 0xb4, (byte) 0xba, (byte) 0x2e, (byte) 0xf5, (byte) 0xd1, (byte) 0x51, (byte) 0xdb, (byte) 0xe8,
            (byte) 0xb5, (byte) 0x55, (byte) 0x7a, (byte) 0xfa, (byte) 0xc7, (byte) 0x1e, (byte) 0xfc, (byte) 0x95,
            (byte) 0xe4, (byte) 0x54, (byte) 0xa6, (byte) 0xca, (byte) 0xbf, (byte) 0x72, (byte) 0x98, (byte) 0x03,
            (byte) 0xfd, (byte) 0x9c, (byte) 0x0a, (byte) 0xf7, (byte) 0xe2, (byte) 0xe0, (byte) 0x21, (byte) 0xcd,

            (byte) 0xbc, (byte) 0xf5, (byte) 0x9e, (byte) 0xb3, (byte) 0x6c, (byte) 0x16, (byte) 0xbe, (byte) 0x01,
            (byte) 0x60, (byte) 0x4e, (byte) 0x08, (byte) 0xd4, (byte) 0x9c, (byte) 0xde, (byte) 0x8f, (byte) 0x8c,
            (byte) 0xa6, (byte) 0xe0, (byte) 0x05, (byte) 0x06, (byte) 0xc7, (byte) 0x76, (byte) 0xe3, (byte) 0xfd,
            (byte) 0x19, (byte) 0x52, (byte) 0x47, (byte) 0xc1, (byte) 0x29, (byte) 0x86, (byte) 0x49, (byte) 0x9f,
            (byte) 0xb7, (byte) 0xb1, (byte) 0x67, (byte) 0x00, (byte) 0x83, (byte) 0xcb, (byte) 0xef, (byte) 0x7f,
            (byte) 0xc7, (byte) 0x2c, (byte) 0xb2, (byte) 0x8f, (byte) 0xeb, (byte) 0x34, (byte) 0x4d, (byte) 0x1e,
            (byte) 0x49, (byte) 0x61, (byte) 0x0c, (byte) 0xbb, (byte) 0x3d, (byte) 0xaf, (byte) 0x02, (byte) 0xbb,
            (byte) 0x03, (byte) 0x64, (byte) 0xd2, (byte) 0xa9, (byte) 0xfc, (byte) 0x3f, (byte) 0xd9, (byte) 0x5e
        }),
        // 3*64B + 15B
        new TestSet(0x126cbe68L, new byte[] {
            (byte) 0xbe, (byte) 0x01, (byte) 0xb9, (byte) 0x57, (byte) 0x42, (byte) 0x34, (byte) 0xc4, (byte) 0x97,
            (byte) 0x49, (byte) 0xeb, (byte) 0x1a, (byte) 0x3a, (byte) 0x98, (byte) 0x97, (byte) 0x6d, (byte) 0xc0,
            (byte) 0x22, (byte) 0xbb, (byte) 0xb8, (byte) 0x1b, (byte) 0x29, (byte) 0x69, (byte) 0x57, (byte) 0x68,
            (byte) 0xec, (byte) 0x18, (byte) 0xde, (byte) 0x0c, (byte) 0x58, (byte) 0xf4, (byte) 0xcb, (byte) 0xdf,
            (byte) 0x26, (byte) 0x90, (byte) 0xb6, (byte) 0xc3, (byte) 0x09, (byte) 0xa6, (byte) 0xc1, (byte) 0x58,
            (byte) 0x99, (byte) 0xf7, (byte) 0x5b, (byte) 0xb5, (byte) 0xa3, (byte) 0x00, (byte) 0x80, (byte) 0x03,
            (byte) 0xcf, (byte) 0xbc, (byte) 0x6f, (byte) 0x63, (byte) 0x29, (byte) 0xa3, (byte) 0xb5, (byte) 0xb8,
            (byte) 0xe3, (byte) 0xbc, (byte) 0x54, (byte) 0x0d, (byte) 0xf8, (byte) 0x92, (byte) 0x0e, (byte) 0xd7,

            (byte) 0x4a, (byte) 0xa4, (byte) 0x86, (byte) 0xcf, (byte) 0x40, (byte) 0x86, (byte) 0xf9, (byte) 0x4f,
            (byte) 0x16, (byte) 0x17, (byte) 0x70, (byte) 0x29, (byte) 0x77, (byte) 0xb6, (byte) 0xfc, (byte) 0x41,
            (byte) 0xc1, (byte) 0x4c, (byte) 0xc3, (byte) 0xfb, (byte) 0x07, (byte) 0xca, (byte) 0x1a, (byte) 0x01,
            (byte) 0xee, (byte) 0x76, (byte) 0xaf, (byte) 0x86, (byte) 0x0e, (byte) 0x23, (byte) 0x00, (byte) 0x0d,
            (byte) 0xb8, (byte) 0x71, (byte) 0xb2, (byte) 0x13, (byte) 0x1e, (byte) 0xe2, (byte) 0x74, (byte) 0x23,
            (byte) 0xc1, (byte) 0xd6, (byte) 0x6b, (byte) 0x94, (byte) 0xfc, (byte) 0x1b, (byte) 0x29, (byte) 0xf8,
            (byte) 0x65, (byte) 0xa8, (byte) 0x30, (byte) 0xde, (byte) 0x9b, (byte) 0x38, (byte) 0x91, (byte) 0xd1,
            (byte) 0xd0, (byte) 0x3e, (byte) 0x5d, (byte) 0x3c, (byte) 0xf6, (byte) 0xa5, (byte) 0xe8, (byte) 0x9d,

            (byte) 0xd2, (byte) 0x78, (byte) 0x6b, (byte) 0x09, (byte) 0x3b, (byte) 0x06, (byte) 0x2e, (byte) 0xb6,
            (byte) 0xf4, (byte) 0xd6, (byte) 0xe0, (byte) 0xbd, (byte) 0xa3, (byte) 0xb4, (byte) 0xdb, (byte) 0x1f,
            (byte) 0xe5, (byte) 0xbe, (byte) 0x9c, (byte) 0x36, (byte) 0x95, (byte) 0x84, (byte) 0xe5, (byte) 0xdc,
            (byte) 0x60, (byte) 0xd7, (byte) 0xef, (byte) 0x07, (byte) 0x4f, (byte) 0x16, (byte) 0x97, (byte) 0x34,
            (byte) 0x4b, (byte) 0x0a, (byte) 0x21, (byte) 0xe6, (byte) 0x39, (byte) 0xc7, (byte) 0x14, (byte) 0xb1,
            (byte) 0xde, (byte) 0x40, (byte) 0xa9, (byte) 0xe5, (byte) 0x78, (byte) 0x29, (byte) 0xa3, (byte) 0x60,
            (byte) 0x2d, (byte) 0xdb, (byte) 0xa9, (byte) 0x95, (byte) 0x65, (byte) 0xe5, (byte) 0xae, (byte) 0xe9,
            (byte) 0x17, (byte) 0x9f, (byte) 0x70, (byte) 0xa9, (byte) 0x88, (byte) 0x3d, (byte) 0x5b, (byte) 0x36,

            (byte) 0xc8, (byte) 0x73, (byte) 0xe4, (byte) 0x01, (byte) 0x47, (byte) 0x29, (byte) 0x8b, (byte) 0x8d,
            (byte) 0xae, (byte) 0xf7, (byte) 0x05, (byte) 0x28, (byte) 0x8a, (byte) 0x33, (byte) 0x97
        }),
        // 3*64B + 3*16B
        new TestSet(0xc1d6e9b3L, new byte[] {
            (byte) 0xf9, (byte) 0x7c, (byte) 0xdf, (byte) 0xc8, (byte) 0xb6, (byte) 0x5f, (byte) 0x1f, (byte) 0x6e,
            (byte) 0xb6, (byte) 0x97, (byte) 0xab, (byte) 0xb1, (byte) 0x51, (byte) 0x44, (byte) 0x26, (byte) 0x12,
            (byte) 0x49, (byte) 0xdc, (byte) 0x15, (byte) 0xda, (byte) 0xbe, (byte) 0x3c, (byte) 0xac, (byte) 0x5a,
            (byte) 0x0b, (byte) 0xdd, (byte) 0x40, (byte) 0x80, (byte) 0xf2, (byte) 0x33, (byte) 0xf9, (byte) 0x65,
            (byte) 0x58, (byte) 0xae, (byte) 0x8b, (byte) 0xac, (byte) 0x7f, (byte) 0x38, (byte) 0x2f, (byte) 0x71,
            (byte) 0x89, (byte) 0x7b, (byte) 0x85, (byte) 0x1c, (byte) 0xe8, (byte) 0xf9, (byte) 0x47, (byte) 0x1c,
            (byte) 0xc7, (byte) 0x12, (byte) 0xae, (byte) 0x03, (byte) 0x72, (byte) 0x86, (byte) 0x99, (byte) 0x63,
            (byte) 0x1e, (byte) 0xe1, (byte) 0x4c, (byte) 0x3a, (byte) 0xcf, (byte) 0xc1, (byte) 0xb4, (byte) 0x51,

            (byte) 0x5b, (byte) 0x10, (byte) 0x4d, (byte) 0xe7, (byte) 0xfa, (byte) 0x27, (byte) 0xc5, (byte) 0x8f,
            (byte) 0x2f, (byte) 0x8b, (byte) 0xf0, (byte) 0x94, (byte) 0x3e, (byte) 0xf4, (byte) 0x39, (byte) 0x5c,
            (byte) 0x5c, (byte) 0xd3, (byte) 0xab, (byte) 0x8e, (byte) 0x74, (byte) 0x1f, (byte) 0xb6, (byte) 0x40,
            (byte) 0xd8, (byte) 0x57, (byte) 0xe5, (byte) 0x8c, (byte) 0x29, (byte) 0x78, (byte) 0xc5, (byte) 0x4a,
            (byte) 0x4f, (byte) 0xcf, (byte) 0xba, (byte) 0xcc, (byte) 0x31, (byte) 0xd7, (byte) 0xfd, (byte) 0xb2,
            (byte) 0x05, (byte) 0x38, (byte) 0x1d, (byte) 0xa5, (byte) 0x37, (byte) 0xeb, (byte) 0x6d, (byte) 0xcd,
            (byte) 0xbc, (byte) 0x29, (byte) 0xe1, (byte) 0xce, (byte) 0x5d, (byte) 0xff, (byte) 0xe5, (byte) 0xb6,
            (byte) 0x2b, (byte) 0xd2, (byte) 0x82, (byte) 0x7b, (byte) 0x19, (byte) 0x92, (byte) 0x35, (byte) 0x48,

            (byte) 0x65, (byte) 0x00, (byte) 0x4e, (byte) 0xc1, (byte) 0xd6, (byte) 0xce, (byte) 0xd2, (byte) 0x4e,
            (byte) 0x8f, (byte) 0x81, (byte) 0x18, (byte) 0xc7, (byte) 0xec, (byte) 0xc5, (byte) 0xf5, (byte) 0xd0,
            (byte) 0xba, (byte) 0x08, (byte) 0x5d, (byte) 0x54, (byte) 0x07, (byte) 0x8d, (byte) 0xcb, (byte) 0xf6,
            (byte) 0xc3, (byte) 0x53, (byte) 0x67, (byte) 0xd1, (byte) 0x9b, (byte) 0xd5, (byte) 0x41, (byte) 0xcb,
            (byte) 0x47, (byte) 0x0b, (byte) 0xf2, (byte) 0x0a, (byte) 0x52, (byte) 0x6a, (byte) 0xb3, (byte) 0x0c,
            (byte) 0x77, (byte) 0x4c, (byte) 0x31, (byte) 0xc7, (byte) 0x0b, (byte) 0xf6, (byte) 0xe7, (byte) 0x37,
            (byte) 0x51, (byte) 0x8c, (byte) 0xf6, (byte) 0xd7, (byte) 0x8e, (byte) 0xd3, (byte) 0x1c, (byte) 0x22,
            (byte) 0xa5, (byte) 0x82, (byte) 0x4a, (byte) 0xb4, (byte) 0x47, (byte) 0x87, (byte) 0x7e, (byte) 0x1e,

            (byte) 0xa9, (byte) 0x38, (byte) 0x25, (byte) 0x6f, (byte) 0xf9, (byte) 0x25, (byte) 0x1c, (byte) 0xb6,
            (byte) 0x59, (byte) 0xdc, (byte) 0x3c, (byte) 0x48, (byte) 0x67, (byte) 0x43, (byte) 0x4b, (byte) 0xfd,

            (byte) 0xd4, (byte) 0x0a, (byte) 0x2a, (byte) 0xa1, (byte) 0x8e, (byte) 0xf0, (byte) 0x6b, (byte) 0xe9,
            (byte) 0x05, (byte) 0x6b, (byte) 0x9d, (byte) 0x80, (byte) 0xb2, (byte) 0x79, (byte) 0x71, (byte) 0xa1,

            (byte) 0x57, (byte) 0x38, (byte) 0xe0, (byte) 0x50, (byte) 0xe4, (byte) 0x06, (byte) 0x67, (byte) 0xef,
            (byte) 0xfe, (byte) 0x0b, (byte) 0xde, (byte) 0x93, (byte) 0x3a, (byte) 0x37, (byte) 0xdf, (byte) 0x29
        }),
        // 3*64B + 3*16B + 15B
        new TestSet(0xc34ffbdL, new byte[] {
            (byte) 0x72, (byte) 0x1d, (byte) 0x17, (byte) 0x4c, (byte) 0x10, (byte) 0xfd, (byte) 0x9a, (byte) 0x25,
            (byte) 0xa8, (byte) 0x32, (byte) 0x02, (byte) 0x5d, (byte) 0x25, (byte) 0x1b, (byte) 0x8c, (byte) 0xd0,
            (byte) 0x51, (byte) 0x87, (byte) 0x4b, (byte) 0x40, (byte) 0xf6, (byte) 0xfc, (byte) 0xc1, (byte) 0xf0,
            (byte) 0x61, (byte) 0xdb, (byte) 0x89, (byte) 0x7c, (byte) 0x2c, (byte) 0x6f, (byte) 0x45, (byte) 0xc4,
            (byte) 0x4c, (byte) 0x29, (byte) 0x79, (byte) 0xe3, (byte) 0x65, (byte) 0xa2, (byte) 0xbe, (byte) 0x08,
            (byte) 0x5c, (byte) 0xf8, (byte) 0xcb, (byte) 0xd1, (byte) 0xaa, (byte) 0x2e, (byte) 0xb0, (byte) 0xdd,
            (byte) 0x71, (byte) 0x44, (byte) 0x92, (byte) 0x12, (byte) 0xd7, (byte) 0xea, (byte) 0x9b, (byte) 0xe5,
            (byte) 0xf8, (byte) 0x4e, (byte) 0xaa, (byte) 0xa3, (byte) 0x14, (byte) 0xe2, (byte) 0x90, (byte) 0x73,

            (byte) 0x5b, (byte) 0xaa, (byte) 0x83, (byte) 0x47, (byte) 0x42, (byte) 0xc8, (byte) 0x48, (byte) 0xde,
            (byte) 0xc5, (byte) 0x67, (byte) 0x9c, (byte) 0xeb, (byte) 0x55, (byte) 0x35, (byte) 0x9b, (byte) 0xe6,
            (byte) 0x6b, (byte) 0x00, (byte) 0xc4, (byte) 0xfe, (byte) 0x86, (byte) 0x12, (byte) 0xa8, (byte) 0xee,
            (byte) 0x54, (byte) 0xf4, (byte) 0xcb, (byte) 0x13, (byte) 0xcd, (byte) 0xd9, (byte) 0xd7, (byte) 0x3d,
            (byte) 0xe5, (byte) 0x8a, (byte) 0x89, (byte) 0x17, (byte) 0xa5, (byte) 0x2f, (byte) 0x73, (byte) 0x81,
            (byte) 0xf1, (byte) 0x93, (byte) 0x6c, (byte) 0xa4, (byte) 0x2d, (byte) 0x5a, (byte) 0xeb, (byte) 0x29,
            (byte) 0xc4, (byte) 0x6b, (byte) 0x1a, (byte) 0xd7, (byte) 0xec, (byte) 0xca, (byte) 0xb7, (byte) 0xd8,
            (byte) 0x10, (byte) 0xfe, (byte) 0xbc, (byte) 0x8d, (byte) 0xb5, (byte) 0xd1, (byte) 0xb8, (byte) 0x10,

            (byte) 0x1c, (byte) 0x19, (byte) 0xc4, (byte) 0xc7, (byte) 0x67, (byte) 0x9a, (byte) 0x9a, (byte) 0x44,
            (byte) 0x8e, (byte) 0x8d, (byte) 0xe9, (byte) 0xff, (byte) 0x6a, (byte) 0x70, (byte) 0xbc, (byte) 0xbf,
            (byte) 0xf2, (byte) 0x8c, (byte) 0x1a, (byte) 0x5d, (byte) 0xb7, (byte) 0x94, (byte) 0xbe, (byte) 0x13,
            (byte) 0xb4, (byte) 0x55, (byte) 0x0a, (byte) 0x68, (byte) 0x60, (byte) 0x9f, (byte) 0xdd, (byte) 0x12,
            (byte) 0xa7, (byte) 0x66, (byte) 0x65, (byte) 0x84, (byte) 0x85, (byte) 0x80, (byte) 0x1b, (byte) 0x0f,
            (byte) 0xfb, (byte) 0xb2, (byte) 0x4f, (byte) 0xe4, (byte) 0x1a, (byte) 0x4f, (byte) 0x96, (byte) 0xf3,
            (byte) 0xa2, (byte) 0x94, (byte) 0x1a, (byte) 0x1a, (byte) 0x39, (byte) 0x48, (byte) 0xb9, (byte) 0x4e,
            (byte) 0x05, (byte) 0x75, (byte) 0x2e, (byte) 0x84, (byte) 0x65, (byte) 0xfe, (byte) 0x59, (byte) 0x6e,

            (byte) 0x40, (byte) 0x6f, (byte) 0x45, (byte) 0x1d, (byte) 0x55, (byte) 0xbe, (byte) 0x13, (byte) 0x1d,
            (byte) 0x00, (byte) 0x3e, (byte) 0x3a, (byte) 0xe4, (byte) 0x0f, (byte) 0x06, (byte) 0xdd, (byte) 0xb7,

            (byte) 0xfd, (byte) 0xb2, (byte) 0x76, (byte) 0x26, (byte) 0x2c, (byte) 0x92, (byte) 0xf0, (byte) 0x4b,
            (byte) 0x05, (byte) 0xa2, (byte) 0x7a, (byte) 0x85, (byte) 0xb9, (byte) 0x53, (byte) 0xe6, (byte) 0x5d,

            (byte) 0x9c, (byte) 0xa3, (byte) 0x4a, (byte) 0x53, (byte) 0xf5, (byte) 0xbd, (byte) 0xad, (byte) 0x4d,
            (byte) 0xf4, (byte) 0xce, (byte) 0x1c, (byte) 0x8d, (byte) 0x2a, (byte) 0xbd, (byte) 0xb7, (byte) 0xc9,

            (byte) 0x71, (byte) 0xcf, (byte) 0x0d, (byte) 0xf1, (byte) 0x34, (byte) 0x8d, (byte) 0x44, (byte) 0xff,
            (byte) 0x3c, (byte) 0xf1, (byte) 0x4d, (byte) 0x03, (byte) 0xd3, (byte) 0x2a, (byte) 0x3d
        }),
    };

    @Test(groups = {"level.sanity"}, invocationCount=2)
    public static void testCRC32CUpdateByteArray() {
        CRC32C checksum = new CRC32C();
        for (TestSet ts : testSets) {
            checksum.update(ts.data);
            AssertJUnit.assertEquals(String.format("Incorrect checksum for length %d byte array", ts.data.length), ts.expected, checksum.getValue());
            checksum.reset();
        }
    }

    @Test(groups = {"level.sanity"}, invocationCount=2)
    public static void testCRC32CUpdateWrappedByteBuffer() {
        CRC32C checksum = new CRC32C();
        for (TestSet ts : testSets) {
            ByteBuffer bb = ByteBuffer.wrap(ts.data);
            checksum.update(bb);
            AssertJUnit.assertEquals(String.format("Incorrect result for length %d wrapped ByteBuffer", ts.data.length), ts.expected, checksum.getValue());
            checksum.reset();
        }
    }

    @Test(groups = {"level.sanity"}, invocationCount=2)
    public static void testCRC32CUpdateReadonlyByteBuffer() {
        CRC32C checksum = new CRC32C();
        for (TestSet ts : testSets) {
            ByteBuffer bb = ByteBuffer.wrap(ts.data).asReadOnlyBuffer();
            checksum.update(bb);
            AssertJUnit.assertEquals(String.format("Incorrect result for length %d read-only ByteBuffer", ts.data.length), ts.expected, checksum.getValue());
            checksum.reset();
        }
    }

    @Test(groups = {"level.sanity"}, invocationCount=2)
    public static void testCRC32CUpdateDirectByteBuffer() {
        CRC32C checksum = new CRC32C();
        for (TestSet ts : testSets){
            ByteBuffer bb = ByteBuffer.allocateDirect(ts.data.length);
            bb.put(ts.data);
            bb.rewind();
            checksum.update(bb);
            AssertJUnit.assertEquals(String.format("Incorrect result for length %d direct ByteBuffer", ts.data.length), ts.expected, checksum.getValue());
            checksum.reset();
        }
    }

    @Test(groups = {"level.sanity"}, invocationCount=2)
    public static void testCRC32CUpdateByteArrayOffset() {
        CRC32C checksum = new CRC32C();
        for (TestSet ts : testSets) {
            byte[] unaligned_bytes = new byte[ts.data.length + 64];
            for (int i = 0; i < unaligned_bytes.length - ts.data.length; i++) {
                System.arraycopy(ts.data, 0, unaligned_bytes, i, ts.data.length);
                checksum.update(unaligned_bytes, i, ts.data.length);
                AssertJUnit.assertEquals(String.format("Incorrect result for length %d byte array with offset %d", ts.data.length, i), ts.expected, checksum.getValue());
                checksum.reset();
            }
        }
    }

    @Test(groups = {"level.sanity"}, invocationCount=2)
    public static void testCRC32CUpdateDirectByteBufferOffset() {
        CRC32C checksum = new CRC32C();
        for (TestSet ts : testSets) {
            byte[] unaligned_bytes = new byte[ts.data.length + 64];
            for (int i = 0; i < unaligned_bytes.length - ts.data.length; i++) {
                ByteBuffer bb = ByteBuffer.allocateDirect(unaligned_bytes.length);
                System.arraycopy(ts.data, 0, unaligned_bytes, i, ts.data.length);
                bb.put(unaligned_bytes);
                bb.position(i);
                bb.limit(i + ts.data.length);
                checksum.update(bb);
                AssertJUnit.assertEquals(String.format("Incorrect result for length %d direct ByteBuffer with offset %d", ts.data.length, i), ts.expected, checksum.getValue());
                checksum.reset();
            }
        }
    }

    @Test(groups = {"level.sanity"}, invocationCount=2)
    public static void testCRC32CUpdateLittleEndianDirectByteBufferOffset() {
        CRC32C checksum = new CRC32C();
        for (TestSet ts : testSets) {
            byte[] unaligned_bytes = new byte[ts.data.length + 64];
            for (int i = 0; i < unaligned_bytes.length - ts.data.length; i++) {
                ByteBuffer bb = ByteBuffer.allocateDirect(unaligned_bytes.length);
                bb.order(ByteOrder.LITTLE_ENDIAN);
                System.arraycopy(ts.data, 0, unaligned_bytes, i, ts.data.length);
                bb.put(unaligned_bytes);
                bb.position(i);
                bb.limit(i + ts.data.length);
                checksum.update(bb);
                AssertJUnit.assertEquals(String.format("Incorrect result for length %d little-endian direct ByteBuffer with offset %d", ts.data.length, i), ts.expected, checksum.getValue());
                checksum.reset();
            }
        }
    }

    @Test(groups = {"level.sanity"}, invocationCount=2)
    public static void testCRC32CUpdateWrappedByteBufferOffset() {
        CRC32C checksum = new CRC32C();
        for (TestSet ts : testSets) {
            byte[] unaligned_bytes = new byte[ts.data.length + 64];
            for (int i = 0; i < unaligned_bytes.length - ts.data.length; i++) {
                System.arraycopy(ts.data, 0, unaligned_bytes, i, ts.data.length);
                ByteBuffer bb = ByteBuffer.wrap(unaligned_bytes);
                bb.position(i);
                bb.limit(i + ts.data.length);
                checksum.update(bb);
                AssertJUnit.assertEquals(String.format("Incorrect result for length %d wrapped ByteBuffer with offset %d", ts.data.length, i), ts.expected, checksum.getValue());
                checksum.reset();
            }
        }
    }

    @Test(groups = {"level.sanity"}, invocationCount=2)
    public static void testCRC32CUpdateLittleEndianWrappedByteBufferOffset() {
        CRC32C checksum = new CRC32C();
        for (TestSet ts : testSets) {
            byte[] unaligned_bytes = new byte[ts.data.length + 64];
            for (int i = 0; i < unaligned_bytes.length - ts.data.length; i++) {
                System.arraycopy(ts.data, 0, unaligned_bytes, i, ts.data.length);
                ByteBuffer bb = ByteBuffer.wrap(unaligned_bytes);
                bb.order(ByteOrder.LITTLE_ENDIAN);
                bb.position(i);
                bb.limit(i + ts.data.length);
                checksum.update(bb);
                AssertJUnit.assertEquals(String.format("Incorrect result for length %d little-endian wrapped ByteBuffer with offset %d", ts.data.length, i), ts.expected, checksum.getValue());
                checksum.reset();
            }
        }
    }

    @Test(groups = {"level.sanity"}, invocationCount=2)
    public static void testCRC32CUpdateReadonlyByteBufferOffset() {
        CRC32C checksum = new CRC32C();
        for (TestSet ts : testSets) {
            byte[] unaligned_bytes = new byte[ts.data.length + 64];
            for (int i = 0; i < unaligned_bytes.length - ts.data.length; i++) {
                System.arraycopy(ts.data, 0, unaligned_bytes, i, ts.data.length);
                ByteBuffer bb = ByteBuffer.wrap(unaligned_bytes).asReadOnlyBuffer();
                bb.position(i);
                bb.limit(i + ts.data.length);
                checksum.update(bb);
                AssertJUnit.assertEquals(String.format("Incorrect result for length %d read-only ByteBuffer with offset %d", ts.data.length, i), ts.expected, checksum.getValue());
                checksum.reset();
            }
        }
    }

    @Test(groups = {"level.sanity"}, invocationCount=2)
    public static void testCRC32CUpdateLittleEndianReadonlyByteBufferOffset() {
        CRC32C checksum = new CRC32C();
        for (TestSet ts : testSets) {
            byte[] unaligned_bytes = new byte[ts.data.length + 64];
            for (int i = 0; i < unaligned_bytes.length - ts.data.length; i++) {
                System.arraycopy(ts.data, 0, unaligned_bytes, i, ts.data.length);
                ByteBuffer bb = ByteBuffer.wrap(unaligned_bytes).asReadOnlyBuffer();
                bb.order(ByteOrder.LITTLE_ENDIAN);
                bb.position(i);
                bb.limit(i + ts.data.length);
                checksum.update(bb);
                AssertJUnit.assertEquals(String.format("Incorrect result for length %d little-endian read-only ByteBuffer with offset %d", ts.data.length, i), ts.expected, checksum.getValue());
                checksum.reset();
            }
        }
    }
}
