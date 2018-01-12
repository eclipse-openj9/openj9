/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#define CRC 0x4c11db7
#define CRC_XOR
#define REFLECT

#ifndef __ASSEMBLY__
#ifdef CRC_TABLE
static const unsigned int crc_table[] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
        0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
        0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
        0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
        0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
        0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
        0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
        0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
        0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,
        0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
        0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
        0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
        0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,
        0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
        0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
        0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
        0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,
        0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
        0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
        0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
        0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,
        0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,
        0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
        0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
        0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
        0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
        0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,
        0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
        0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
        0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
        0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,
        0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,
        0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
        0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
        0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,
        0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
        0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
        0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
        0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
        0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
        0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
        0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
        0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
        0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
        0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
        0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,
        0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
        0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,};

#endif
#else
#define MAX_SIZE    32768
#if defined(AIXPPC)

.constants:

        /* Reduce 262144 kbits to 1024 bits */
        /* x^261120 mod p(x)` << 1, x^261184 mod p(x)` << 1 */
        .llong 0x00000001651797d2
        .llong 0x0000000099ea94a8

        /* x^260096 mod p(x)` << 1, x^260160 mod p(x)` << 1 */
        .llong 0x0000000021e0d56c
        .llong 0x00000000945a8420

        /* x^259072 mod p(x)` << 1, x^259136 mod p(x)` << 1 */
        .llong 0x000000000f95ecaa
        .llong 0x0000000030762706

        /* x^258048 mod p(x)` << 1, x^258112 mod p(x)` << 1 */
        .llong 0x00000001ebd224ac
        .llong 0x00000001a52fc582

        /* x^257024 mod p(x)` << 1, x^257088 mod p(x)` << 1 */
        .llong 0x000000000ccb97ca
        .llong 0x00000001a4a7167a

        /* x^256000 mod p(x)` << 1, x^256064 mod p(x)` << 1 */
        .llong 0x00000001006ec8a8
        .llong 0x000000000c18249a

        /* x^254976 mod p(x)` << 1, x^255040 mod p(x)` << 1 */
        .llong 0x000000014f58f196
        .llong 0x00000000a924ae7c

        /* x^253952 mod p(x)` << 1, x^254016 mod p(x)` << 1 */
        .llong 0x00000001a7192ca6
        .llong 0x00000001e12ccc12

        /* x^252928 mod p(x)` << 1, x^252992 mod p(x)` << 1 */
        .llong 0x000000019a64bab2
        .llong 0x00000000a0b9d4ac

        /* x^251904 mod p(x)` << 1, x^251968 mod p(x)` << 1 */
        .llong 0x0000000014f4ed2e
        .llong 0x0000000095e8ddfe

        /* x^250880 mod p(x)` << 1, x^250944 mod p(x)` << 1 */
        .llong 0x000000011092b6a2
        .llong 0x00000000233fddc4

        /* x^249856 mod p(x)` << 1, x^249920 mod p(x)` << 1 */
        .llong 0x00000000c8a1629c
        .llong 0x00000001b4529b62

        /* x^248832 mod p(x)` << 1, x^248896 mod p(x)` << 1 */
        .llong 0x000000017bf32e8e
        .llong 0x00000001a7fa0e64

        /* x^247808 mod p(x)` << 1, x^247872 mod p(x)` << 1 */
        .llong 0x00000001f8cc6582
        .llong 0x00000001b5334592

        /* x^246784 mod p(x)` << 1, x^246848 mod p(x)` << 1 */
        .llong 0x000000008631ddf0
        .llong 0x000000011f8ee1b4

        /* x^245760 mod p(x)` << 1, x^245824 mod p(x)` << 1 */
        .llong 0x000000007e5a76d0
        .llong 0x000000006252e632

        /* x^244736 mod p(x)` << 1, x^244800 mod p(x)` << 1 */
        .llong 0x000000002b09b31c
        .llong 0x00000000ab973e84

        /* x^243712 mod p(x)` << 1, x^243776 mod p(x)` << 1 */
        .llong 0x00000001b2df1f84
        .llong 0x000000007734f5ec

        /* x^242688 mod p(x)` << 1, x^242752 mod p(x)` << 1 */
        .llong 0x00000001d6f56afc
        .llong 0x000000007c547798

        /* x^241664 mod p(x)` << 1, x^241728 mod p(x)` << 1 */
        .llong 0x00000001b9b5e70c
        .llong 0x000000007ec40210

        /* x^240640 mod p(x)` << 1, x^240704 mod p(x)` << 1 */
        .llong 0x0000000034b626d2
        .llong 0x00000001ab1695a8

        /* x^239616 mod p(x)` << 1, x^239680 mod p(x)` << 1 */
        .llong 0x000000014c53479a
        .llong 0x0000000090494bba

        /* x^238592 mod p(x)` << 1, x^238656 mod p(x)` << 1 */
        .llong 0x00000001a6d179a4
        .llong 0x00000001123fb816

        /* x^237568 mod p(x)` << 1, x^237632 mod p(x)` << 1 */
        .llong 0x000000015abd16b4
        .llong 0x00000001e188c74c

        /* x^236544 mod p(x)` << 1, x^236608 mod p(x)` << 1 */
        .llong 0x00000000018f9852
        .llong 0x00000001c2d3451c

        /* x^235520 mod p(x)` << 1, x^235584 mod p(x)` << 1 */
        .llong 0x000000001fb3084a
        .llong 0x00000000f55cf1ca

        /* x^234496 mod p(x)` << 1, x^234560 mod p(x)` << 1 */
        .llong 0x00000000c53dfb04
        .llong 0x00000001a0531540

        /* x^233472 mod p(x)` << 1, x^233536 mod p(x)` << 1 */
        .llong 0x00000000e10c9ad6
        .llong 0x0000000132cd7ebc

        /* x^232448 mod p(x)` << 1, x^232512 mod p(x)` << 1 */
        .llong 0x0000000025aa994a
        .llong 0x0000000073ab7f36

        /* x^231424 mod p(x)` << 1, x^231488 mod p(x)` << 1 */
        .llong 0x00000000fa3a74c4
        .llong 0x0000000041aed1c2

        /* x^230400 mod p(x)` << 1, x^230464 mod p(x)` << 1 */
        .llong 0x0000000033eb3f40
        .llong 0x0000000136c53800

        /* x^229376 mod p(x)` << 1, x^229440 mod p(x)` << 1 */
        .llong 0x000000017193f296
        .llong 0x0000000126835a30

        /* x^228352 mod p(x)` << 1, x^228416 mod p(x)` << 1 */
        .llong 0x0000000043f6c86a
        .llong 0x000000006241b502

        /* x^227328 mod p(x)` << 1, x^227392 mod p(x)` << 1 */
        .llong 0x000000016b513ec6
        .llong 0x00000000d5196ad4

        /* x^226304 mod p(x)` << 1, x^226368 mod p(x)` << 1 */
        .llong 0x00000000c8f25b4e
        .llong 0x000000009cfa769a

        /* x^225280 mod p(x)` << 1, x^225344 mod p(x)` << 1 */
        .llong 0x00000001a45048ec
        .llong 0x00000000920e5df4

        /* x^224256 mod p(x)` << 1, x^224320 mod p(x)` << 1 */
        .llong 0x000000000c441004
        .llong 0x0000000169dc310e

        /* x^223232 mod p(x)` << 1, x^223296 mod p(x)` << 1 */
        .llong 0x000000000e17cad6
        .llong 0x0000000009fc331c

        /* x^222208 mod p(x)` << 1, x^222272 mod p(x)` << 1 */
        .llong 0x00000001253ae964
        .llong 0x000000010d94a81e

        /* x^221184 mod p(x)` << 1, x^221248 mod p(x)` << 1 */
        .llong 0x00000001d7c88ebc
        .llong 0x0000000027a20ab2

        /* x^220160 mod p(x)` << 1, x^220224 mod p(x)` << 1 */
        .llong 0x00000001e7ca913a
        .llong 0x0000000114f87504

        /* x^219136 mod p(x)` << 1, x^219200 mod p(x)` << 1 */
        .llong 0x0000000033ed078a
        .llong 0x000000004b076d96

        /* x^218112 mod p(x)` << 1, x^218176 mod p(x)` << 1 */
        .llong 0x00000000e1839c78
        .llong 0x00000000da4d1e74

        /* x^217088 mod p(x)` << 1, x^217152 mod p(x)` << 1 */
        .llong 0x00000001322b267e
        .llong 0x000000001b81f672

        /* x^216064 mod p(x)` << 1, x^216128 mod p(x)` << 1 */
        .llong 0x00000000638231b6
        .llong 0x000000009367c988

        /* x^215040 mod p(x)` << 1, x^215104 mod p(x)` << 1 */
        .llong 0x00000001ee7f16f4
        .llong 0x00000001717214ca

        /* x^214016 mod p(x)` << 1, x^214080 mod p(x)` << 1 */
        .llong 0x0000000117d9924a
        .llong 0x000000009f47d820

        /* x^212992 mod p(x)` << 1, x^213056 mod p(x)` << 1 */
        .llong 0x00000000e1a9e0c4
        .llong 0x000000010d9a47d2

        /* x^211968 mod p(x)` << 1, x^212032 mod p(x)` << 1 */
        .llong 0x00000001403731dc
        .llong 0x00000000a696c58c

        /* x^210944 mod p(x)` << 1, x^211008 mod p(x)` << 1 */
        .llong 0x00000001a5ea9682
        .llong 0x000000002aa28ec6

        /* x^209920 mod p(x)` << 1, x^209984 mod p(x)` << 1 */
        .llong 0x0000000101c5c578
        .llong 0x00000001fe18fd9a

        /* x^208896 mod p(x)` << 1, x^208960 mod p(x)` << 1 */
        .llong 0x00000000dddf6494
        .llong 0x000000019d4fc1ae

        /* x^207872 mod p(x)` << 1, x^207936 mod p(x)` << 1 */
        .llong 0x00000000f1c3db28
        .llong 0x00000001ba0e3dea

        /* x^206848 mod p(x)` << 1, x^206912 mod p(x)` << 1 */
        .llong 0x000000013112fb9c
        .llong 0x0000000074b59a5e

        /* x^205824 mod p(x)` << 1, x^205888 mod p(x)` << 1 */
        .llong 0x00000000b680b906
        .llong 0x00000000f2b5ea98

        /* x^204800 mod p(x)` << 1, x^204864 mod p(x)` << 1 */
        .llong 0x000000001a282932
        .llong 0x0000000187132676

        /* x^203776 mod p(x)` << 1, x^203840 mod p(x)` << 1 */
        .llong 0x0000000089406e7e
        .llong 0x000000010a8c6ad4

        /* x^202752 mod p(x)` << 1, x^202816 mod p(x)` << 1 */
        .llong 0x00000001def6be8c
        .llong 0x00000001e21dfe70

        /* x^201728 mod p(x)` << 1, x^201792 mod p(x)` << 1 */
        .llong 0x0000000075258728
        .llong 0x00000001da0050e4

        /* x^200704 mod p(x)` << 1, x^200768 mod p(x)` << 1 */
        .llong 0x000000019536090a
        .llong 0x00000000772172ae

        /* x^199680 mod p(x)` << 1, x^199744 mod p(x)` << 1 */
        .llong 0x00000000f2455bfc
        .llong 0x00000000e47724aa

        /* x^198656 mod p(x)` << 1, x^198720 mod p(x)` << 1 */
        .llong 0x000000018c40baf4
        .llong 0x000000003cd63ac4

        /* x^197632 mod p(x)` << 1, x^197696 mod p(x)` << 1 */
        .llong 0x000000004cd390d4
        .llong 0x00000001bf47d352

        /* x^196608 mod p(x)` << 1, x^196672 mod p(x)` << 1 */
        .llong 0x00000001e4ece95a
        .llong 0x000000018dc1d708

        /* x^195584 mod p(x)` << 1, x^195648 mod p(x)` << 1 */
        .llong 0x000000001a3ee918
        .llong 0x000000002d4620a4

        /* x^194560 mod p(x)` << 1, x^194624 mod p(x)` << 1 */
        .llong 0x000000007c652fb8
        .llong 0x0000000058fd1740

        /* x^193536 mod p(x)` << 1, x^193600 mod p(x)` << 1 */
        .llong 0x000000011c67842c
        .llong 0x00000000dadd9bfc

        /* x^192512 mod p(x)` << 1, x^192576 mod p(x)` << 1 */
        .llong 0x00000000254f759c
        .llong 0x00000001ea2140be

        /* x^191488 mod p(x)` << 1, x^191552 mod p(x)` << 1 */
        .llong 0x000000007ece94ca
        .llong 0x000000009de128ba

        /* x^190464 mod p(x)` << 1, x^190528 mod p(x)` << 1 */
        .llong 0x0000000038f258c2
        .llong 0x000000013ac3aa8e

        /* x^189440 mod p(x)` << 1, x^189504 mod p(x)` << 1 */
        .llong 0x00000001cdf17b00
        .llong 0x0000000099980562

        /* x^188416 mod p(x)` << 1, x^188480 mod p(x)` << 1 */
        .llong 0x000000011f882c16
        .llong 0x00000001c1579c86

        /* x^187392 mod p(x)` << 1, x^187456 mod p(x)` << 1 */
        .llong 0x0000000100093fc8
        .llong 0x0000000068dbbf94

        /* x^186368 mod p(x)` << 1, x^186432 mod p(x)` << 1 */
        .llong 0x00000001cd684f16
        .llong 0x000000004509fb04

        /* x^185344 mod p(x)` << 1, x^185408 mod p(x)` << 1 */
        .llong 0x000000004bc6a70a
        .llong 0x00000001202f6398

        /* x^184320 mod p(x)` << 1, x^184384 mod p(x)` << 1 */
        .llong 0x000000004fc7e8e4
        .llong 0x000000013aea243e

        /* x^183296 mod p(x)` << 1, x^183360 mod p(x)` << 1 */
        .llong 0x0000000130103f1c
        .llong 0x00000001b4052ae6

        /* x^182272 mod p(x)` << 1, x^182336 mod p(x)` << 1 */
        .llong 0x0000000111b0024c
        .llong 0x00000001cd2a0ae8

        /* x^181248 mod p(x)` << 1, x^181312 mod p(x)` << 1 */
        .llong 0x000000010b3079da
        .llong 0x00000001fe4aa8b4

        /* x^180224 mod p(x)` << 1, x^180288 mod p(x)` << 1 */
        .llong 0x000000010192bcc2
        .llong 0x00000001d1559a42

        /* x^179200 mod p(x)` << 1, x^179264 mod p(x)` << 1 */
        .llong 0x0000000074838d50
        .llong 0x00000001f3e05ecc

        /* x^178176 mod p(x)` << 1, x^178240 mod p(x)` << 1 */
        .llong 0x000000001b20f520
        .llong 0x0000000104ddd2cc

        /* x^177152 mod p(x)` << 1, x^177216 mod p(x)` << 1 */
        .llong 0x0000000050c3590a
        .llong 0x000000015393153c

        /* x^176128 mod p(x)` << 1, x^176192 mod p(x)` << 1 */
        .llong 0x00000000b41cac8e
        .llong 0x0000000057e942c6

        /* x^175104 mod p(x)` << 1, x^175168 mod p(x)` << 1 */
        .llong 0x000000000c72cc78
        .llong 0x000000012c633850

        /* x^174080 mod p(x)` << 1, x^174144 mod p(x)` << 1 */
        .llong 0x0000000030cdb032
        .llong 0x00000000ebcaae4c

        /* x^173056 mod p(x)` << 1, x^173120 mod p(x)` << 1 */
        .llong 0x000000013e09fc32
        .llong 0x000000013ee532a6

        /* x^172032 mod p(x)` << 1, x^172096 mod p(x)` << 1 */
        .llong 0x000000001ed624d2
        .llong 0x00000001bf0cbc7e

        /* x^171008 mod p(x)` << 1, x^171072 mod p(x)` << 1 */
        .llong 0x00000000781aee1a
        .llong 0x00000000d50b7a5a

        /* x^169984 mod p(x)` << 1, x^170048 mod p(x)` << 1 */
        .llong 0x00000001c4d8348c
        .llong 0x0000000002fca6e8

        /* x^168960 mod p(x)` << 1, x^169024 mod p(x)` << 1 */
        .llong 0x0000000057a40336
        .llong 0x000000007af40044

        /* x^167936 mod p(x)` << 1, x^168000 mod p(x)` << 1 */
        .llong 0x0000000085544940
        .llong 0x0000000016178744

        /* x^166912 mod p(x)` << 1, x^166976 mod p(x)` << 1 */
        .llong 0x000000019cd21e80
        .llong 0x000000014c177458

        /* x^165888 mod p(x)` << 1, x^165952 mod p(x)` << 1 */
        .llong 0x000000013eb95bc0
        .llong 0x000000011b6ddf04

        /* x^164864 mod p(x)` << 1, x^164928 mod p(x)` << 1 */
        .llong 0x00000001dfc9fdfc
        .llong 0x00000001f3e29ccc

        /* x^163840 mod p(x)` << 1, x^163904 mod p(x)` << 1 */
        .llong 0x00000000cd028bc2
        .llong 0x0000000135ae7562

        /* x^162816 mod p(x)` << 1, x^162880 mod p(x)` << 1 */
        .llong 0x0000000090db8c44
        .llong 0x0000000190ef812c

        /* x^161792 mod p(x)` << 1, x^161856 mod p(x)` << 1 */
        .llong 0x000000010010a4ce
        .llong 0x0000000067a2c786

        /* x^160768 mod p(x)` << 1, x^160832 mod p(x)` << 1 */
        .llong 0x00000001c8f4c72c
        .llong 0x0000000048b9496c

        /* x^159744 mod p(x)` << 1, x^159808 mod p(x)` << 1 */
        .llong 0x000000001c26170c
        .llong 0x000000015a422de6

        /* x^158720 mod p(x)` << 1, x^158784 mod p(x)` << 1 */
        .llong 0x00000000e3fccf68
        .llong 0x00000001ef0e3640

        /* x^157696 mod p(x)` << 1, x^157760 mod p(x)` << 1 */
        .llong 0x00000000d513ed24
        .llong 0x00000001006d2d26

        /* x^156672 mod p(x)` << 1, x^156736 mod p(x)` << 1 */
        .llong 0x00000000141beada
        .llong 0x00000001170d56d6

        /* x^155648 mod p(x)` << 1, x^155712 mod p(x)` << 1 */
        .llong 0x000000011071aea0
        .llong 0x00000000a5fb613c

        /* x^154624 mod p(x)` << 1, x^154688 mod p(x)` << 1 */
        .llong 0x000000012e19080a
        .llong 0x0000000040bbf7fc

        /* x^153600 mod p(x)` << 1, x^153664 mod p(x)` << 1 */
        .llong 0x0000000100ecf826
        .llong 0x000000016ac3a5b2

        /* x^152576 mod p(x)` << 1, x^152640 mod p(x)` << 1 */
        .llong 0x0000000069b09412
        .llong 0x00000000abf16230

        /* x^151552 mod p(x)` << 1, x^151616 mod p(x)` << 1 */
        .llong 0x0000000122297bac
        .llong 0x00000001ebe23fac

        /* x^150528 mod p(x)` << 1, x^150592 mod p(x)` << 1 */
        .llong 0x00000000e9e4b068
        .llong 0x000000008b6a0894

        /* x^149504 mod p(x)` << 1, x^149568 mod p(x)` << 1 */
        .llong 0x000000004b38651a
        .llong 0x00000001288ea478

        /* x^148480 mod p(x)` << 1, x^148544 mod p(x)` << 1 */
        .llong 0x00000001468360e2
        .llong 0x000000016619c442

        /* x^147456 mod p(x)` << 1, x^147520 mod p(x)` << 1 */
        .llong 0x00000000121c2408
        .llong 0x0000000086230038

        /* x^146432 mod p(x)` << 1, x^146496 mod p(x)` << 1 */
        .llong 0x00000000da7e7d08
        .llong 0x000000017746a756

        /* x^145408 mod p(x)` << 1, x^145472 mod p(x)` << 1 */
        .llong 0x00000001058d7652
        .llong 0x0000000191b8f8f8

        /* x^144384 mod p(x)` << 1, x^144448 mod p(x)` << 1 */
        .llong 0x000000014a098a90
        .llong 0x000000008e167708

        /* x^143360 mod p(x)` << 1, x^143424 mod p(x)` << 1 */
        .llong 0x0000000020dbe72e
        .llong 0x0000000148b22d54

        /* x^142336 mod p(x)` << 1, x^142400 mod p(x)` << 1 */
        .llong 0x000000011e7323e8
        .llong 0x0000000044ba2c3c

        /* x^141312 mod p(x)` << 1, x^141376 mod p(x)` << 1 */
        .llong 0x00000000d5d4bf94
        .llong 0x00000000b54d2b52

        /* x^140288 mod p(x)` << 1, x^140352 mod p(x)` << 1 */
        .llong 0x0000000199d8746c
        .llong 0x0000000005a4fd8a

        /* x^139264 mod p(x)` << 1, x^139328 mod p(x)` << 1 */
        .llong 0x00000000ce9ca8a0
        .llong 0x0000000139f9fc46

        /* x^138240 mod p(x)` << 1, x^138304 mod p(x)` << 1 */
        .llong 0x00000000136edece
        .llong 0x000000015a1fa824

        /* x^137216 mod p(x)` << 1, x^137280 mod p(x)` << 1 */
        .llong 0x000000019b92a068
        .llong 0x000000000a61ae4c

        /* x^136192 mod p(x)` << 1, x^136256 mod p(x)` << 1 */
        .llong 0x0000000071d62206
        .llong 0x0000000145e9113e

        /* x^135168 mod p(x)` << 1, x^135232 mod p(x)` << 1 */
        .llong 0x00000000dfc50158
        .llong 0x000000006a348448

        /* x^134144 mod p(x)` << 1, x^134208 mod p(x)` << 1 */
        .llong 0x00000001517626bc
        .llong 0x000000004d80a08c

        /* x^133120 mod p(x)` << 1, x^133184 mod p(x)` << 1 */
        .llong 0x0000000148d1e4fa
        .llong 0x000000014b6837a0

        /* x^132096 mod p(x)` << 1, x^132160 mod p(x)` << 1 */
        .llong 0x0000000094d8266e
        .llong 0x000000016896a7fc

        /* x^131072 mod p(x)` << 1, x^131136 mod p(x)` << 1 */
        .llong 0x00000000606c5e34
        .llong 0x000000014f187140

        /* x^130048 mod p(x)` << 1, x^130112 mod p(x)` << 1 */
        .llong 0x000000019766beaa
        .llong 0x000000019581b9da

        /* x^129024 mod p(x)` << 1, x^129088 mod p(x)` << 1 */
        .llong 0x00000001d80c506c
        .llong 0x00000001091bc984

        /* x^128000 mod p(x)` << 1, x^128064 mod p(x)` << 1 */
        .llong 0x000000001e73837c
        .llong 0x000000001067223c

        /* x^126976 mod p(x)` << 1, x^127040 mod p(x)` << 1 */
        .llong 0x0000000064d587de
        .llong 0x00000001ab16ea02

        /* x^125952 mod p(x)` << 1, x^126016 mod p(x)` << 1 */
        .llong 0x00000000f4a507b0
        .llong 0x000000013c4598a8

        /* x^124928 mod p(x)` << 1, x^124992 mod p(x)` << 1 */
        .llong 0x0000000040e342fc
        .llong 0x00000000b3735430

        /* x^123904 mod p(x)` << 1, x^123968 mod p(x)` << 1 */
        .llong 0x00000001d5ad9c3a
        .llong 0x00000001bb3fc0c0

        /* x^122880 mod p(x)` << 1, x^122944 mod p(x)` << 1 */
        .llong 0x0000000094a691a4
        .llong 0x00000001570ae19c

        /* x^121856 mod p(x)` << 1, x^121920 mod p(x)` << 1 */
        .llong 0x00000001271ecdfa
        .llong 0x00000001ea910712

        /* x^120832 mod p(x)` << 1, x^120896 mod p(x)` << 1 */
        .llong 0x000000009e54475a
        .llong 0x0000000167127128

        /* x^119808 mod p(x)` << 1, x^119872 mod p(x)` << 1 */
        .llong 0x00000000c9c099ee
        .llong 0x0000000019e790a2

        /* x^118784 mod p(x)` << 1, x^118848 mod p(x)` << 1 */
        .llong 0x000000009a2f736c
        .llong 0x000000003788f710

        /* x^117760 mod p(x)` << 1, x^117824 mod p(x)` << 1 */
        .llong 0x00000000bb9f4996
        .llong 0x00000001682a160e

        /* x^116736 mod p(x)` << 1, x^116800 mod p(x)` << 1 */
        .llong 0x00000001db688050
        .llong 0x000000007f0ebd2e

        /* x^115712 mod p(x)` << 1, x^115776 mod p(x)` << 1 */
        .llong 0x00000000e9b10af4
        .llong 0x000000002b032080

        /* x^114688 mod p(x)` << 1, x^114752 mod p(x)` << 1 */
        .llong 0x000000012d4545e4
        .llong 0x00000000cfd1664a

        /* x^113664 mod p(x)` << 1, x^113728 mod p(x)` << 1 */
        .llong 0x000000000361139c
        .llong 0x00000000aa1181c2

        /* x^112640 mod p(x)` << 1, x^112704 mod p(x)` << 1 */
        .llong 0x00000001a5a1a3a8
        .llong 0x00000000ddd08002

        /* x^111616 mod p(x)` << 1, x^111680 mod p(x)` << 1 */
        .llong 0x000000006844e0b0
        .llong 0x00000000e8dd0446

        /* x^110592 mod p(x)` << 1, x^110656 mod p(x)` << 1 */
        .llong 0x00000000c3762f28
        .llong 0x00000001bbd94a00

        /* x^109568 mod p(x)` << 1, x^109632 mod p(x)` << 1 */
        .llong 0x00000001d26287a2
        .llong 0x00000000ab6cd180

        /* x^108544 mod p(x)` << 1, x^108608 mod p(x)` << 1 */
        .llong 0x00000001f6f0bba8
        .llong 0x0000000031803ce2

        /* x^107520 mod p(x)` << 1, x^107584 mod p(x)` << 1 */
        .llong 0x000000002ffabd62
        .llong 0x0000000024f40b0c

        /* x^106496 mod p(x)` << 1, x^106560 mod p(x)` << 1 */
        .llong 0x00000000fb4516b8
        .llong 0x00000001ba1d9834

        /* x^105472 mod p(x)` << 1, x^105536 mod p(x)` << 1 */
        .llong 0x000000018cfa961c
        .llong 0x0000000104de61aa

        /* x^104448 mod p(x)` << 1, x^104512 mod p(x)` << 1 */
        .llong 0x000000019e588d52
        .llong 0x0000000113e40d46

        /* x^103424 mod p(x)` << 1, x^103488 mod p(x)` << 1 */
        .llong 0x00000001180f0bbc
        .llong 0x00000001415598a0

        /* x^102400 mod p(x)` << 1, x^102464 mod p(x)` << 1 */
        .llong 0x00000000e1d9177a
        .llong 0x00000000bf6c8c90

        /* x^101376 mod p(x)` << 1, x^101440 mod p(x)` << 1 */
        .llong 0x0000000105abc27c
        .llong 0x00000001788b0504

        /* x^100352 mod p(x)` << 1, x^100416 mod p(x)` << 1 */
        .llong 0x00000000972e4a58
        .llong 0x0000000038385d02

        /* x^99328 mod p(x)` << 1, x^99392 mod p(x)` << 1 */
        .llong 0x0000000183499a5e
        .llong 0x00000001b6c83844

        /* x^98304 mod p(x)` << 1, x^98368 mod p(x)` << 1 */
        .llong 0x00000001c96a8cca
        .llong 0x0000000051061a8a

        /* x^97280 mod p(x)` << 1, x^97344 mod p(x)` << 1 */
        .llong 0x00000001a1a5b60c
        .llong 0x000000017351388a

        /* x^96256 mod p(x)` << 1, x^96320 mod p(x)` << 1 */
        .llong 0x00000000e4b6ac9c
        .llong 0x0000000132928f92

        /* x^95232 mod p(x)` << 1, x^95296 mod p(x)` << 1 */
        .llong 0x00000001807e7f5a
        .llong 0x00000000e6b4f48a

        /* x^94208 mod p(x)` << 1, x^94272 mod p(x)` << 1 */
        .llong 0x000000017a7e3bc8
        .llong 0x0000000039d15e90

        /* x^93184 mod p(x)` << 1, x^93248 mod p(x)` << 1 */
        .llong 0x00000000d73975da
        .llong 0x00000000312d6074

        /* x^92160 mod p(x)` << 1, x^92224 mod p(x)` << 1 */
        .llong 0x000000017375d038
        .llong 0x000000017bbb2cc4

        /* x^91136 mod p(x)` << 1, x^91200 mod p(x)` << 1 */
        .llong 0x00000000193680bc
        .llong 0x000000016ded3e18

        /* x^90112 mod p(x)` << 1, x^90176 mod p(x)` << 1 */
        .llong 0x00000000999b06f6
        .llong 0x00000000f1638b16

        /* x^89088 mod p(x)` << 1, x^89152 mod p(x)` << 1 */
        .llong 0x00000001f685d2b8
        .llong 0x00000001d38b9ecc

        /* x^88064 mod p(x)` << 1, x^88128 mod p(x)` << 1 */
        .llong 0x00000001f4ecbed2
        .llong 0x000000018b8d09dc

        /* x^87040 mod p(x)` << 1, x^87104 mod p(x)` << 1 */
        .llong 0x00000000ba16f1a0
        .llong 0x00000000e7bc27d2

        /* x^86016 mod p(x)` << 1, x^86080 mod p(x)` << 1 */
        .llong 0x0000000115aceac4
        .llong 0x00000000275e1e96

        /* x^84992 mod p(x)` << 1, x^85056 mod p(x)` << 1 */
        .llong 0x00000001aeff6292
        .llong 0x00000000e2e3031e

        /* x^83968 mod p(x)` << 1, x^84032 mod p(x)` << 1 */
        .llong 0x000000009640124c
        .llong 0x00000001041c84d8

        /* x^82944 mod p(x)` << 1, x^83008 mod p(x)` << 1 */
        .llong 0x0000000114f41f02
        .llong 0x00000000706ce672

        /* x^81920 mod p(x)` << 1, x^81984 mod p(x)` << 1 */
        .llong 0x000000009c5f3586
        .llong 0x000000015d5070da

        /* x^80896 mod p(x)` << 1, x^80960 mod p(x)` << 1 */
        .llong 0x00000001878275fa
        .llong 0x0000000038f9493a

        /* x^79872 mod p(x)` << 1, x^79936 mod p(x)` << 1 */
        .llong 0x00000000ddc42ce8
        .llong 0x00000000a3348a76

        /* x^78848 mod p(x)` << 1, x^78912 mod p(x)` << 1 */
        .llong 0x0000000181d2c73a
        .llong 0x00000001ad0aab92

        /* x^77824 mod p(x)` << 1, x^77888 mod p(x)` << 1 */
        .llong 0x0000000141c9320a
        .llong 0x000000019e85f712

        /* x^76800 mod p(x)` << 1, x^76864 mod p(x)` << 1 */
        .llong 0x000000015235719a
        .llong 0x000000005a871e76

        /* x^75776 mod p(x)` << 1, x^75840 mod p(x)` << 1 */
        .llong 0x00000000be27d804
        .llong 0x000000017249c662

        /* x^74752 mod p(x)` << 1, x^74816 mod p(x)` << 1 */
        .llong 0x000000006242d45a
        .llong 0x000000003a084712

        /* x^73728 mod p(x)` << 1, x^73792 mod p(x)` << 1 */
        .llong 0x000000009a53638e
        .llong 0x00000000ed438478

        /* x^72704 mod p(x)` << 1, x^72768 mod p(x)` << 1 */
        .llong 0x00000001001ecfb6
        .llong 0x00000000abac34cc

        /* x^71680 mod p(x)` << 1, x^71744 mod p(x)` << 1 */
        .llong 0x000000016d7c2d64
        .llong 0x000000005f35ef3e

        /* x^70656 mod p(x)` << 1, x^70720 mod p(x)` << 1 */
        .llong 0x00000001d0ce46c0
        .llong 0x0000000047d6608c

        /* x^69632 mod p(x)` << 1, x^69696 mod p(x)` << 1 */
        .llong 0x0000000124c907b4
        .llong 0x000000002d01470e

        /* x^68608 mod p(x)` << 1, x^68672 mod p(x)` << 1 */
        .llong 0x0000000018a555ca
        .llong 0x0000000158bbc7b0

        /* x^67584 mod p(x)` << 1, x^67648 mod p(x)` << 1 */
        .llong 0x000000006b0980bc
        .llong 0x00000000c0a23e8e

        /* x^66560 mod p(x)` << 1, x^66624 mod p(x)` << 1 */
        .llong 0x000000008bbba964
        .llong 0x00000001ebd85c88

        /* x^65536 mod p(x)` << 1, x^65600 mod p(x)` << 1 */
        .llong 0x00000001070a5a1e
        .llong 0x000000019ee20bb2

        /* x^64512 mod p(x)` << 1, x^64576 mod p(x)` << 1 */
        .llong 0x000000002204322a
        .llong 0x00000001acabf2d6

        /* x^63488 mod p(x)` << 1, x^63552 mod p(x)` << 1 */
        .llong 0x00000000a27524d0
        .llong 0x00000001b7963d56

        /* x^62464 mod p(x)` << 1, x^62528 mod p(x)` << 1 */
        .llong 0x0000000020b1e4ba
        .llong 0x000000017bffa1fe

        /* x^61440 mod p(x)` << 1, x^61504 mod p(x)` << 1 */
        .llong 0x0000000032cc27fc
        .llong 0x000000001f15333e

        /* x^60416 mod p(x)` << 1, x^60480 mod p(x)` << 1 */
        .llong 0x0000000044dd22b8
        .llong 0x000000018593129e

        /* x^59392 mod p(x)` << 1, x^59456 mod p(x)` << 1 */
        .llong 0x00000000dffc9e0a
        .llong 0x000000019cb32602

        /* x^58368 mod p(x)` << 1, x^58432 mod p(x)` << 1 */
        .llong 0x00000001b7a0ed14
        .llong 0x0000000142b05cc8

        /* x^57344 mod p(x)` << 1, x^57408 mod p(x)` << 1 */
        .llong 0x00000000c7842488
        .llong 0x00000001be49e7a4

        /* x^56320 mod p(x)` << 1, x^56384 mod p(x)` << 1 */
        .llong 0x00000001c02a4fee
        .llong 0x0000000108f69d6c

        /* x^55296 mod p(x)` << 1, x^55360 mod p(x)` << 1 */
        .llong 0x000000003c273778
        .llong 0x000000006c0971f0

        /* x^54272 mod p(x)` << 1, x^54336 mod p(x)` << 1 */
        .llong 0x00000001d63f8894
        .llong 0x000000005b16467a

        /* x^53248 mod p(x)` << 1, x^53312 mod p(x)` << 1 */
        .llong 0x000000006be557d6
        .llong 0x00000001551a628e

        /* x^52224 mod p(x)` << 1, x^52288 mod p(x)` << 1 */
        .llong 0x000000006a7806ea
        .llong 0x000000019e42ea92

        /* x^51200 mod p(x)` << 1, x^51264 mod p(x)` << 1 */
        .llong 0x000000016155aa0c
        .llong 0x000000012fa83ff2

        /* x^50176 mod p(x)` << 1, x^50240 mod p(x)` << 1 */
        .llong 0x00000000908650ac
        .llong 0x000000011ca9cde0

        /* x^49152 mod p(x)` << 1, x^49216 mod p(x)` << 1 */
        .llong 0x00000000aa5a8084
        .llong 0x00000000c8e5cd74

        /* x^48128 mod p(x)` << 1, x^48192 mod p(x)` << 1 */
        .llong 0x0000000191bb500a
        .llong 0x0000000096c27f0c

        /* x^47104 mod p(x)` << 1, x^47168 mod p(x)` << 1 */
        .llong 0x0000000064e9bed0
        .llong 0x000000002baed926

        /* x^46080 mod p(x)` << 1, x^46144 mod p(x)` << 1 */
        .llong 0x000000009444f302
        .llong 0x000000017c8de8d2

        /* x^45056 mod p(x)` << 1, x^45120 mod p(x)` << 1 */
        .llong 0x000000019db07d3c
        .llong 0x00000000d43d6068

        /* x^44032 mod p(x)` << 1, x^44096 mod p(x)` << 1 */
        .llong 0x00000001359e3e6e
        .llong 0x00000000cb2c4b26

        /* x^43008 mod p(x)` << 1, x^43072 mod p(x)` << 1 */
        .llong 0x00000001e4f10dd2
        .llong 0x0000000145b8da26

        /* x^41984 mod p(x)` << 1, x^42048 mod p(x)` << 1 */
        .llong 0x0000000124f5735e
        .llong 0x000000018fff4b08

        /* x^40960 mod p(x)` << 1, x^41024 mod p(x)` << 1 */
        .llong 0x0000000124760a4c
        .llong 0x0000000150b58ed0

        /* x^39936 mod p(x)` << 1, x^40000 mod p(x)` << 1 */
        .llong 0x000000000f1fc186
        .llong 0x00000001549f39bc

        /* x^38912 mod p(x)` << 1, x^38976 mod p(x)` << 1 */
        .llong 0x00000000150e4cc4
        .llong 0x00000000ef4d2f42

        /* x^37888 mod p(x)` << 1, x^37952 mod p(x)` << 1 */
        .llong 0x000000002a6204e8
        .llong 0x00000001b1468572

        /* x^36864 mod p(x)` << 1, x^36928 mod p(x)` << 1 */
        .llong 0x00000000beb1d432
        .llong 0x000000013d7403b2

        /* x^35840 mod p(x)` << 1, x^35904 mod p(x)` << 1 */
        .llong 0x0000000135f3f1f0
        .llong 0x00000001a4681842

        /* x^34816 mod p(x)` << 1, x^34880 mod p(x)` << 1 */
        .llong 0x0000000074fe2232
        .llong 0x0000000167714492

        /* x^33792 mod p(x)` << 1, x^33856 mod p(x)` << 1 */
        .llong 0x000000001ac6e2ba
        .llong 0x00000001e599099a

        /* x^32768 mod p(x)` << 1, x^32832 mod p(x)` << 1 */
        .llong 0x0000000013fca91e
        .llong 0x00000000fe128194

        /* x^31744 mod p(x)` << 1, x^31808 mod p(x)` << 1 */
        .llong 0x0000000183f4931e
        .llong 0x0000000077e8b990

        /* x^30720 mod p(x)` << 1, x^30784 mod p(x)` << 1 */
        .llong 0x00000000b6d9b4e4
        .llong 0x00000001a267f63a

        /* x^29696 mod p(x)` << 1, x^29760 mod p(x)` << 1 */
        .llong 0x00000000b5188656
        .llong 0x00000001945c245a

        /* x^28672 mod p(x)` << 1, x^28736 mod p(x)` << 1 */
        .llong 0x0000000027a81a84
        .llong 0x0000000149002e76

        /* x^27648 mod p(x)` << 1, x^27712 mod p(x)` << 1 */
        .llong 0x0000000125699258
        .llong 0x00000001bb8310a4

        /* x^26624 mod p(x)` << 1, x^26688 mod p(x)` << 1 */
        .llong 0x00000001b23de796
        .llong 0x000000019ec60bcc

        /* x^25600 mod p(x)` << 1, x^25664 mod p(x)` << 1 */
        .llong 0x00000000fe4365dc
        .llong 0x000000012d8590ae

        /* x^24576 mod p(x)` << 1, x^24640 mod p(x)` << 1 */
        .llong 0x00000000c68f497a
        .llong 0x0000000065b00684

        /* x^23552 mod p(x)` << 1, x^23616 mod p(x)` << 1 */
        .llong 0x00000000fbf521ee
        .llong 0x000000015e5aeadc

        /* x^22528 mod p(x)` << 1, x^22592 mod p(x)` << 1 */
        .llong 0x000000015eac3378
        .llong 0x00000000b77ff2b0

        /* x^21504 mod p(x)` << 1, x^21568 mod p(x)` << 1 */
        .llong 0x0000000134914b90
        .llong 0x0000000188da2ff6

        /* x^20480 mod p(x)` << 1, x^20544 mod p(x)` << 1 */
        .llong 0x0000000016335cfe
        .llong 0x0000000063da929a

        /* x^19456 mod p(x)` << 1, x^19520 mod p(x)` << 1 */
        .llong 0x000000010372d10c
        .llong 0x00000001389caa80

        /* x^18432 mod p(x)` << 1, x^18496 mod p(x)` << 1 */
        .llong 0x000000015097b908
        .llong 0x000000013db599d2

        /* x^17408 mod p(x)` << 1, x^17472 mod p(x)` << 1 */
        .llong 0x00000001227a7572
        .llong 0x0000000122505a86

        /* x^16384 mod p(x)` << 1, x^16448 mod p(x)` << 1 */
        .llong 0x000000009a8f75c0
        .llong 0x000000016bd72746

        /* x^15360 mod p(x)` << 1, x^15424 mod p(x)` << 1 */
        .llong 0x00000000682c77a2
        .llong 0x00000001c3faf1d4

        /* x^14336 mod p(x)` << 1, x^14400 mod p(x)` << 1 */
        .llong 0x00000000231f091c
        .llong 0x00000001111c826c

        /* x^13312 mod p(x)` << 1, x^13376 mod p(x)` << 1 */
        .llong 0x000000007d4439f2
        .llong 0x00000000153e9fb2

        /* x^12288 mod p(x)` << 1, x^12352 mod p(x)` << 1 */
        .llong 0x000000017e221efc
        .llong 0x000000002b1f7b60

        /* x^11264 mod p(x)` << 1, x^11328 mod p(x)` << 1 */
        .llong 0x0000000167457c38
        .llong 0x00000000b1dba570

        /* x^10240 mod p(x)` << 1, x^10304 mod p(x)` << 1 */
        .llong 0x00000000bdf081c4
        .llong 0x00000001f6397b76

        /* x^9216 mod p(x)` << 1, x^9280 mod p(x)` << 1 */
        .llong 0x000000016286d6b0
        .llong 0x0000000156335214

        /* x^8192 mod p(x)` << 1, x^8256 mod p(x)` << 1 */
        .llong 0x00000000c84f001c
        .llong 0x00000001d70e3986

        /* x^7168 mod p(x)` << 1, x^7232 mod p(x)` << 1 */
        .llong 0x0000000064efe7c0
        .llong 0x000000003701a774

        /* x^6144 mod p(x)` << 1, x^6208 mod p(x)` << 1 */
        .llong 0x000000000ac2d904
        .llong 0x00000000ac81ef72

        /* x^5120 mod p(x)` << 1, x^5184 mod p(x)` << 1 */
        .llong 0x00000000fd226d14
        .llong 0x0000000133212464

        /* x^4096 mod p(x)` << 1, x^4160 mod p(x)` << 1 */
        .llong 0x000000011cfd42e0
        .llong 0x00000000e4e45610

        /* x^3072 mod p(x)` << 1, x^3136 mod p(x)` << 1 */
        .llong 0x000000016e5a5678
        .llong 0x000000000c1bd370

        /* x^2048 mod p(x)` << 1, x^2112 mod p(x)` << 1 */
        .llong 0x00000001d888fe22
        .llong 0x00000001a7b9e7a6

        /* x^1024 mod p(x)` << 1, x^1088 mod p(x)` << 1 */
        .llong 0x00000001af77fcd4
        .llong 0x000000007d657a10

.short_constants:

        /* Reduce final 1024-2048 bits to 64 bits, shifting 32 bits to include the trailing 32 bits of zeros */
        /* x^1952 mod p(x)`, x^1984 mod p(x)`, x^2016 mod p(x)`, x^2048 mod p(x)` */
        .llong 0xed837b2613e8221e
        .llong 0x99168a18ec447f11

        /* x^1824 mod p(x)`, x^1856 mod p(x)`, x^1888 mod p(x)`, x^1920 mod p(x)` */
        .llong 0xc8acdd8147b9ce5a
        .llong 0xe23e954e8fd2cd3c

        /* x^1696 mod p(x)`, x^1728 mod p(x)`, x^1760 mod p(x)`, x^1792 mod p(x)` */
        .llong 0xd9ad6d87d4277e25
        .llong 0x92f8befe6b1d2b53

        /* x^1568 mod p(x)`, x^1600 mod p(x)`, x^1632 mod p(x)`, x^1664 mod p(x)` */
        .llong 0xc10ec5e033fbca3b
        .llong 0xf38a3556291ea462

        /* x^1440 mod p(x)`, x^1472 mod p(x)`, x^1504 mod p(x)`, x^1536 mod p(x)` */
        .llong 0xc0b55b0e82e02e2f
        .llong 0x974ac56262b6ca4b

        /* x^1312 mod p(x)`, x^1344 mod p(x)`, x^1376 mod p(x)`, x^1408 mod p(x)` */
        .llong 0x71aa1df0e172334d
        .llong 0x855712b3784d2a56

        /* x^1184 mod p(x)`, x^1216 mod p(x)`, x^1248 mod p(x)`, x^1280 mod p(x)` */
        .llong 0xfee3053e3969324d
        .llong 0xa5abe9f80eaee722

        /* x^1056 mod p(x)`, x^1088 mod p(x)`, x^1120 mod p(x)`, x^1152 mod p(x)` */
        .llong 0xf44779b93eb2bd08
        .llong 0x1fa0943ddb54814c

        /* x^928 mod p(x)`, x^960 mod p(x)`, x^992 mod p(x)`, x^1024 mod p(x)` */
        .llong 0xf5449b3f00cc3374
        .llong 0xa53ff440d7bbfe6a

        /* x^800 mod p(x)`, x^832 mod p(x)`, x^864 mod p(x)`, x^896 mod p(x)` */
        .llong 0x6f8346e1d777606e
        .llong 0xebe7e3566325605c

        /* x^672 mod p(x)`, x^704 mod p(x)`, x^736 mod p(x)`, x^768 mod p(x)` */
        .llong 0xe3ab4f2ac0b95347
        .llong 0xc65a272ce5b592b8

        /* x^544 mod p(x)`, x^576 mod p(x)`, x^608 mod p(x)`, x^640 mod p(x)` */
        .llong 0xaa2215ea329ecc11
        .llong 0x5705a9ca4721589f

        /* x^416 mod p(x)`, x^448 mod p(x)`, x^480 mod p(x)`, x^512 mod p(x)` */
        .llong 0x1ed8f66ed95efd26
        .llong 0xe3720acb88d14467

        /* x^288 mod p(x)`, x^320 mod p(x)`, x^352 mod p(x)`, x^384 mod p(x)` */
        .llong 0x78ed02d5a700e96a
        .llong 0xba1aca0315141c31

        /* x^160 mod p(x)`, x^192 mod p(x)`, x^224 mod p(x)`, x^256 mod p(x)` */
        .llong 0xba8ccbe832b39da3
        .llong 0xad2a31b3ed627dae

        /* x^32 mod p(x)`, x^64 mod p(x)`, x^96 mod p(x)`, x^128 mod p(x)` */
        .llong 0xedb88320b1e6b092
        .llong 0x6655004fa06a2517


.barrett_constants:
        /* 33 bit reflected Barrett constant m - (4^32)/n,  x^64 div p(x)` */
        .llong 0x0000000000000000
        .llong 0x00000001f7011641
        /* 33 bit reflected Barrett constant n */
        .llong 0x0000000000000000
        .llong 0x00000001db710641

#elif defined(LINUXPPC64) || defined(LINUXPPC)

.constants:

        /* Reduce 262144 kbits to 1024 bits */
        /* x^261120 mod p(x)` << 1, x^261184 mod p(x)` << 1 */
        .octa 0x00000001651797d20000000099ea94a8

        /* x^260096 mod p(x)` << 1, x^260160 mod p(x)` << 1 */
        .octa 0x0000000021e0d56c00000000945a8420

        /* x^259072 mod p(x)` << 1, x^259136 mod p(x)` << 1 */
        .octa 0x000000000f95ecaa0000000030762706

        /* x^258048 mod p(x)` << 1, x^258112 mod p(x)` << 1 */
        .octa 0x00000001ebd224ac00000001a52fc582

        /* x^257024 mod p(x)` << 1, x^257088 mod p(x)` << 1 */
        .octa 0x000000000ccb97ca00000001a4a7167a

        /* x^256000 mod p(x)` << 1, x^256064 mod p(x)` << 1 */
        .octa 0x00000001006ec8a8000000000c18249a

        /* x^254976 mod p(x)` << 1, x^255040 mod p(x)` << 1 */
        .octa 0x000000014f58f19600000000a924ae7c

        /* x^253952 mod p(x)` << 1, x^254016 mod p(x)` << 1 */
        .octa 0x00000001a7192ca600000001e12ccc12

        /* x^252928 mod p(x)` << 1, x^252992 mod p(x)` << 1 */
        .octa 0x000000019a64bab200000000a0b9d4ac

        /* x^251904 mod p(x)` << 1, x^251968 mod p(x)` << 1 */
        .octa 0x0000000014f4ed2e0000000095e8ddfe

        /* x^250880 mod p(x)` << 1, x^250944 mod p(x)` << 1 */
        .octa 0x000000011092b6a200000000233fddc4

        /* x^249856 mod p(x)` << 1, x^249920 mod p(x)` << 1 */
        .octa 0x00000000c8a1629c00000001b4529b62

        /* x^248832 mod p(x)` << 1, x^248896 mod p(x)` << 1 */
        .octa 0x000000017bf32e8e00000001a7fa0e64

        /* x^247808 mod p(x)` << 1, x^247872 mod p(x)` << 1 */
        .octa 0x00000001f8cc658200000001b5334592

        /* x^246784 mod p(x)` << 1, x^246848 mod p(x)` << 1 */
        .octa 0x000000008631ddf0000000011f8ee1b4

        /* x^245760 mod p(x)` << 1, x^245824 mod p(x)` << 1 */
        .octa 0x000000007e5a76d0000000006252e632

        /* x^244736 mod p(x)` << 1, x^244800 mod p(x)` << 1 */
        .octa 0x000000002b09b31c00000000ab973e84

        /* x^243712 mod p(x)` << 1, x^243776 mod p(x)` << 1 */
        .octa 0x00000001b2df1f84000000007734f5ec

        /* x^242688 mod p(x)` << 1, x^242752 mod p(x)` << 1 */
        .octa 0x00000001d6f56afc000000007c547798

        /* x^241664 mod p(x)` << 1, x^241728 mod p(x)` << 1 */
        .octa 0x00000001b9b5e70c000000007ec40210

        /* x^240640 mod p(x)` << 1, x^240704 mod p(x)` << 1 */
        .octa 0x0000000034b626d200000001ab1695a8

        /* x^239616 mod p(x)` << 1, x^239680 mod p(x)` << 1 */
        .octa 0x000000014c53479a0000000090494bba

        /* x^238592 mod p(x)` << 1, x^238656 mod p(x)` << 1 */
        .octa 0x00000001a6d179a400000001123fb816

        /* x^237568 mod p(x)` << 1, x^237632 mod p(x)` << 1 */
        .octa 0x000000015abd16b400000001e188c74c

        /* x^236544 mod p(x)` << 1, x^236608 mod p(x)` << 1 */
        .octa 0x00000000018f985200000001c2d3451c

        /* x^235520 mod p(x)` << 1, x^235584 mod p(x)` << 1 */
        .octa 0x000000001fb3084a00000000f55cf1ca

        /* x^234496 mod p(x)` << 1, x^234560 mod p(x)` << 1 */
        .octa 0x00000000c53dfb0400000001a0531540

        /* x^233472 mod p(x)` << 1, x^233536 mod p(x)` << 1 */
        .octa 0x00000000e10c9ad60000000132cd7ebc

        /* x^232448 mod p(x)` << 1, x^232512 mod p(x)` << 1 */
        .octa 0x0000000025aa994a0000000073ab7f36

        /* x^231424 mod p(x)` << 1, x^231488 mod p(x)` << 1 */
        .octa 0x00000000fa3a74c40000000041aed1c2

        /* x^230400 mod p(x)` << 1, x^230464 mod p(x)` << 1 */
        .octa 0x0000000033eb3f400000000136c53800

        /* x^229376 mod p(x)` << 1, x^229440 mod p(x)` << 1 */
        .octa 0x000000017193f2960000000126835a30

        /* x^228352 mod p(x)` << 1, x^228416 mod p(x)` << 1 */
        .octa 0x0000000043f6c86a000000006241b502

        /* x^227328 mod p(x)` << 1, x^227392 mod p(x)` << 1 */
        .octa 0x000000016b513ec600000000d5196ad4

        /* x^226304 mod p(x)` << 1, x^226368 mod p(x)` << 1 */
        .octa 0x00000000c8f25b4e000000009cfa769a

        /* x^225280 mod p(x)` << 1, x^225344 mod p(x)` << 1 */
        .octa 0x00000001a45048ec00000000920e5df4

        /* x^224256 mod p(x)` << 1, x^224320 mod p(x)` << 1 */
        .octa 0x000000000c4410040000000169dc310e

        /* x^223232 mod p(x)` << 1, x^223296 mod p(x)` << 1 */
        .octa 0x000000000e17cad60000000009fc331c

        /* x^222208 mod p(x)` << 1, x^222272 mod p(x)` << 1 */
        .octa 0x00000001253ae964000000010d94a81e

        /* x^221184 mod p(x)` << 1, x^221248 mod p(x)` << 1 */
        .octa 0x00000001d7c88ebc0000000027a20ab2

        /* x^220160 mod p(x)` << 1, x^220224 mod p(x)` << 1 */
        .octa 0x00000001e7ca913a0000000114f87504

        /* x^219136 mod p(x)` << 1, x^219200 mod p(x)` << 1 */
        .octa 0x0000000033ed078a000000004b076d96

        /* x^218112 mod p(x)` << 1, x^218176 mod p(x)` << 1 */
        .octa 0x00000000e1839c7800000000da4d1e74

        /* x^217088 mod p(x)` << 1, x^217152 mod p(x)` << 1 */
        .octa 0x00000001322b267e000000001b81f672

        /* x^216064 mod p(x)` << 1, x^216128 mod p(x)` << 1 */
        .octa 0x00000000638231b6000000009367c988

        /* x^215040 mod p(x)` << 1, x^215104 mod p(x)` << 1 */
        .octa 0x00000001ee7f16f400000001717214ca

        /* x^214016 mod p(x)` << 1, x^214080 mod p(x)` << 1 */
        .octa 0x0000000117d9924a000000009f47d820

        /* x^212992 mod p(x)` << 1, x^213056 mod p(x)` << 1 */
        .octa 0x00000000e1a9e0c4000000010d9a47d2

        /* x^211968 mod p(x)` << 1, x^212032 mod p(x)` << 1 */
        .octa 0x00000001403731dc00000000a696c58c

        /* x^210944 mod p(x)` << 1, x^211008 mod p(x)` << 1 */
        .octa 0x00000001a5ea9682000000002aa28ec6

        /* x^209920 mod p(x)` << 1, x^209984 mod p(x)` << 1 */
        .octa 0x0000000101c5c57800000001fe18fd9a

        /* x^208896 mod p(x)` << 1, x^208960 mod p(x)` << 1 */
        .octa 0x00000000dddf6494000000019d4fc1ae

        /* x^207872 mod p(x)` << 1, x^207936 mod p(x)` << 1 */
        .octa 0x00000000f1c3db2800000001ba0e3dea

        /* x^206848 mod p(x)` << 1, x^206912 mod p(x)` << 1 */
        .octa 0x000000013112fb9c0000000074b59a5e

        /* x^205824 mod p(x)` << 1, x^205888 mod p(x)` << 1 */
        .octa 0x00000000b680b90600000000f2b5ea98

        /* x^204800 mod p(x)` << 1, x^204864 mod p(x)` << 1 */
        .octa 0x000000001a2829320000000187132676

        /* x^203776 mod p(x)` << 1, x^203840 mod p(x)` << 1 */
        .octa 0x0000000089406e7e000000010a8c6ad4

        /* x^202752 mod p(x)` << 1, x^202816 mod p(x)` << 1 */
        .octa 0x00000001def6be8c00000001e21dfe70

        /* x^201728 mod p(x)` << 1, x^201792 mod p(x)` << 1 */
        .octa 0x000000007525872800000001da0050e4

        /* x^200704 mod p(x)` << 1, x^200768 mod p(x)` << 1 */
        .octa 0x000000019536090a00000000772172ae

        /* x^199680 mod p(x)` << 1, x^199744 mod p(x)` << 1 */
        .octa 0x00000000f2455bfc00000000e47724aa

        /* x^198656 mod p(x)` << 1, x^198720 mod p(x)` << 1 */
        .octa 0x000000018c40baf4000000003cd63ac4

        /* x^197632 mod p(x)` << 1, x^197696 mod p(x)` << 1 */
        .octa 0x000000004cd390d400000001bf47d352

        /* x^196608 mod p(x)` << 1, x^196672 mod p(x)` << 1 */
        .octa 0x00000001e4ece95a000000018dc1d708

        /* x^195584 mod p(x)` << 1, x^195648 mod p(x)` << 1 */
        .octa 0x000000001a3ee918000000002d4620a4

        /* x^194560 mod p(x)` << 1, x^194624 mod p(x)` << 1 */
        .octa 0x000000007c652fb80000000058fd1740

        /* x^193536 mod p(x)` << 1, x^193600 mod p(x)` << 1 */
        .octa 0x000000011c67842c00000000dadd9bfc

        /* x^192512 mod p(x)` << 1, x^192576 mod p(x)` << 1 */
        .octa 0x00000000254f759c00000001ea2140be

        /* x^191488 mod p(x)` << 1, x^191552 mod p(x)` << 1 */
        .octa 0x000000007ece94ca000000009de128ba

        /* x^190464 mod p(x)` << 1, x^190528 mod p(x)` << 1 */
        .octa 0x0000000038f258c2000000013ac3aa8e

        /* x^189440 mod p(x)` << 1, x^189504 mod p(x)` << 1 */
        .octa 0x00000001cdf17b000000000099980562

        /* x^188416 mod p(x)` << 1, x^188480 mod p(x)` << 1 */
        .octa 0x000000011f882c1600000001c1579c86

        /* x^187392 mod p(x)` << 1, x^187456 mod p(x)` << 1 */
        .octa 0x0000000100093fc80000000068dbbf94

        /* x^186368 mod p(x)` << 1, x^186432 mod p(x)` << 1 */
        .octa 0x00000001cd684f16000000004509fb04

        /* x^185344 mod p(x)` << 1, x^185408 mod p(x)` << 1 */
        .octa 0x000000004bc6a70a00000001202f6398

        /* x^184320 mod p(x)` << 1, x^184384 mod p(x)` << 1 */
        .octa 0x000000004fc7e8e4000000013aea243e

        /* x^183296 mod p(x)` << 1, x^183360 mod p(x)` << 1 */
        .octa 0x0000000130103f1c00000001b4052ae6

        /* x^182272 mod p(x)` << 1, x^182336 mod p(x)` << 1 */
        .octa 0x0000000111b0024c00000001cd2a0ae8

        /* x^181248 mod p(x)` << 1, x^181312 mod p(x)` << 1 */
        .octa 0x000000010b3079da00000001fe4aa8b4

        /* x^180224 mod p(x)` << 1, x^180288 mod p(x)` << 1 */
        .octa 0x000000010192bcc200000001d1559a42

        /* x^179200 mod p(x)` << 1, x^179264 mod p(x)` << 1 */
        .octa 0x0000000074838d5000000001f3e05ecc

        /* x^178176 mod p(x)` << 1, x^178240 mod p(x)` << 1 */
        .octa 0x000000001b20f5200000000104ddd2cc

        /* x^177152 mod p(x)` << 1, x^177216 mod p(x)` << 1 */
        .octa 0x0000000050c3590a000000015393153c

        /* x^176128 mod p(x)` << 1, x^176192 mod p(x)` << 1 */
        .octa 0x00000000b41cac8e0000000057e942c6

        /* x^175104 mod p(x)` << 1, x^175168 mod p(x)` << 1 */
        .octa 0x000000000c72cc78000000012c633850

        /* x^174080 mod p(x)` << 1, x^174144 mod p(x)` << 1 */
        .octa 0x0000000030cdb03200000000ebcaae4c

        /* x^173056 mod p(x)` << 1, x^173120 mod p(x)` << 1 */
        .octa 0x000000013e09fc32000000013ee532a6

        /* x^172032 mod p(x)` << 1, x^172096 mod p(x)` << 1 */
        .octa 0x000000001ed624d200000001bf0cbc7e

        /* x^171008 mod p(x)` << 1, x^171072 mod p(x)` << 1 */
        .octa 0x00000000781aee1a00000000d50b7a5a

        /* x^169984 mod p(x)` << 1, x^170048 mod p(x)` << 1 */
        .octa 0x00000001c4d8348c0000000002fca6e8

        /* x^168960 mod p(x)` << 1, x^169024 mod p(x)` << 1 */
        .octa 0x0000000057a40336000000007af40044

        /* x^167936 mod p(x)` << 1, x^168000 mod p(x)` << 1 */
        .octa 0x00000000855449400000000016178744

        /* x^166912 mod p(x)` << 1, x^166976 mod p(x)` << 1 */
        .octa 0x000000019cd21e80000000014c177458

        /* x^165888 mod p(x)` << 1, x^165952 mod p(x)` << 1 */
        .octa 0x000000013eb95bc0000000011b6ddf04

        /* x^164864 mod p(x)` << 1, x^164928 mod p(x)` << 1 */
        .octa 0x00000001dfc9fdfc00000001f3e29ccc

        /* x^163840 mod p(x)` << 1, x^163904 mod p(x)` << 1 */
        .octa 0x00000000cd028bc20000000135ae7562

        /* x^162816 mod p(x)` << 1, x^162880 mod p(x)` << 1 */
        .octa 0x0000000090db8c440000000190ef812c

        /* x^161792 mod p(x)` << 1, x^161856 mod p(x)` << 1 */
        .octa 0x000000010010a4ce0000000067a2c786

        /* x^160768 mod p(x)` << 1, x^160832 mod p(x)` << 1 */
        .octa 0x00000001c8f4c72c0000000048b9496c

        /* x^159744 mod p(x)` << 1, x^159808 mod p(x)` << 1 */
        .octa 0x000000001c26170c000000015a422de6

        /* x^158720 mod p(x)` << 1, x^158784 mod p(x)` << 1 */
        .octa 0x00000000e3fccf6800000001ef0e3640

        /* x^157696 mod p(x)` << 1, x^157760 mod p(x)` << 1 */
        .octa 0x00000000d513ed2400000001006d2d26

        /* x^156672 mod p(x)` << 1, x^156736 mod p(x)` << 1 */
        .octa 0x00000000141beada00000001170d56d6

        /* x^155648 mod p(x)` << 1, x^155712 mod p(x)` << 1 */
        .octa 0x000000011071aea000000000a5fb613c

        /* x^154624 mod p(x)` << 1, x^154688 mod p(x)` << 1 */
        .octa 0x000000012e19080a0000000040bbf7fc

        /* x^153600 mod p(x)` << 1, x^153664 mod p(x)` << 1 */
        .octa 0x0000000100ecf826000000016ac3a5b2

        /* x^152576 mod p(x)` << 1, x^152640 mod p(x)` << 1 */
        .octa 0x0000000069b0941200000000abf16230

        /* x^151552 mod p(x)` << 1, x^151616 mod p(x)` << 1 */
        .octa 0x0000000122297bac00000001ebe23fac

        /* x^150528 mod p(x)` << 1, x^150592 mod p(x)` << 1 */
        .octa 0x00000000e9e4b068000000008b6a0894

        /* x^149504 mod p(x)` << 1, x^149568 mod p(x)` << 1 */
        .octa 0x000000004b38651a00000001288ea478

        /* x^148480 mod p(x)` << 1, x^148544 mod p(x)` << 1 */
        .octa 0x00000001468360e2000000016619c442

        /* x^147456 mod p(x)` << 1, x^147520 mod p(x)` << 1 */
        .octa 0x00000000121c24080000000086230038

        /* x^146432 mod p(x)` << 1, x^146496 mod p(x)` << 1 */
        .octa 0x00000000da7e7d08000000017746a756

        /* x^145408 mod p(x)` << 1, x^145472 mod p(x)` << 1 */
        .octa 0x00000001058d76520000000191b8f8f8

        /* x^144384 mod p(x)` << 1, x^144448 mod p(x)` << 1 */
        .octa 0x000000014a098a90000000008e167708

        /* x^143360 mod p(x)` << 1, x^143424 mod p(x)` << 1 */
        .octa 0x0000000020dbe72e0000000148b22d54

        /* x^142336 mod p(x)` << 1, x^142400 mod p(x)` << 1 */
        .octa 0x000000011e7323e80000000044ba2c3c

        /* x^141312 mod p(x)` << 1, x^141376 mod p(x)` << 1 */
        .octa 0x00000000d5d4bf9400000000b54d2b52

        /* x^140288 mod p(x)` << 1, x^140352 mod p(x)` << 1 */
        .octa 0x0000000199d8746c0000000005a4fd8a

        /* x^139264 mod p(x)` << 1, x^139328 mod p(x)` << 1 */
        .octa 0x00000000ce9ca8a00000000139f9fc46

        /* x^138240 mod p(x)` << 1, x^138304 mod p(x)` << 1 */
        .octa 0x00000000136edece000000015a1fa824

        /* x^137216 mod p(x)` << 1, x^137280 mod p(x)` << 1 */
        .octa 0x000000019b92a068000000000a61ae4c

        /* x^136192 mod p(x)` << 1, x^136256 mod p(x)` << 1 */
        .octa 0x0000000071d622060000000145e9113e

        /* x^135168 mod p(x)` << 1, x^135232 mod p(x)` << 1 */
        .octa 0x00000000dfc50158000000006a348448

        /* x^134144 mod p(x)` << 1, x^134208 mod p(x)` << 1 */
        .octa 0x00000001517626bc000000004d80a08c

        /* x^133120 mod p(x)` << 1, x^133184 mod p(x)` << 1 */
        .octa 0x0000000148d1e4fa000000014b6837a0

        /* x^132096 mod p(x)` << 1, x^132160 mod p(x)` << 1 */
        .octa 0x0000000094d8266e000000016896a7fc

        /* x^131072 mod p(x)` << 1, x^131136 mod p(x)` << 1 */
        .octa 0x00000000606c5e34000000014f187140

        /* x^130048 mod p(x)` << 1, x^130112 mod p(x)` << 1 */
        .octa 0x000000019766beaa000000019581b9da

        /* x^129024 mod p(x)` << 1, x^129088 mod p(x)` << 1 */
        .octa 0x00000001d80c506c00000001091bc984

        /* x^128000 mod p(x)` << 1, x^128064 mod p(x)` << 1 */
        .octa 0x000000001e73837c000000001067223c

        /* x^126976 mod p(x)` << 1, x^127040 mod p(x)` << 1 */
        .octa 0x0000000064d587de00000001ab16ea02

        /* x^125952 mod p(x)` << 1, x^126016 mod p(x)` << 1 */
        .octa 0x00000000f4a507b0000000013c4598a8

        /* x^124928 mod p(x)` << 1, x^124992 mod p(x)` << 1 */
        .octa 0x0000000040e342fc00000000b3735430

        /* x^123904 mod p(x)` << 1, x^123968 mod p(x)` << 1 */
        .octa 0x00000001d5ad9c3a00000001bb3fc0c0

        /* x^122880 mod p(x)` << 1, x^122944 mod p(x)` << 1 */
        .octa 0x0000000094a691a400000001570ae19c

        /* x^121856 mod p(x)` << 1, x^121920 mod p(x)` << 1 */
        .octa 0x00000001271ecdfa00000001ea910712

        /* x^120832 mod p(x)` << 1, x^120896 mod p(x)` << 1 */
        .octa 0x000000009e54475a0000000167127128

        /* x^119808 mod p(x)` << 1, x^119872 mod p(x)` << 1 */
        .octa 0x00000000c9c099ee0000000019e790a2

        /* x^118784 mod p(x)` << 1, x^118848 mod p(x)` << 1 */
        .octa 0x000000009a2f736c000000003788f710

        /* x^117760 mod p(x)` << 1, x^117824 mod p(x)` << 1 */
        .octa 0x00000000bb9f499600000001682a160e

        /* x^116736 mod p(x)` << 1, x^116800 mod p(x)` << 1 */
        .octa 0x00000001db688050000000007f0ebd2e

        /* x^115712 mod p(x)` << 1, x^115776 mod p(x)` << 1 */
        .octa 0x00000000e9b10af4000000002b032080

        /* x^114688 mod p(x)` << 1, x^114752 mod p(x)` << 1 */
        .octa 0x000000012d4545e400000000cfd1664a

        /* x^113664 mod p(x)` << 1, x^113728 mod p(x)` << 1 */
        .octa 0x000000000361139c00000000aa1181c2

        /* x^112640 mod p(x)` << 1, x^112704 mod p(x)` << 1 */
        .octa 0x00000001a5a1a3a800000000ddd08002

        /* x^111616 mod p(x)` << 1, x^111680 mod p(x)` << 1 */
        .octa 0x000000006844e0b000000000e8dd0446

        /* x^110592 mod p(x)` << 1, x^110656 mod p(x)` << 1 */
        .octa 0x00000000c3762f2800000001bbd94a00

        /* x^109568 mod p(x)` << 1, x^109632 mod p(x)` << 1 */
        .octa 0x00000001d26287a200000000ab6cd180

        /* x^108544 mod p(x)` << 1, x^108608 mod p(x)` << 1 */
        .octa 0x00000001f6f0bba80000000031803ce2

        /* x^107520 mod p(x)` << 1, x^107584 mod p(x)` << 1 */
        .octa 0x000000002ffabd620000000024f40b0c

        /* x^106496 mod p(x)` << 1, x^106560 mod p(x)` << 1 */
        .octa 0x00000000fb4516b800000001ba1d9834

        /* x^105472 mod p(x)` << 1, x^105536 mod p(x)` << 1 */
        .octa 0x000000018cfa961c0000000104de61aa

        /* x^104448 mod p(x)` << 1, x^104512 mod p(x)` << 1 */
        .octa 0x000000019e588d520000000113e40d46

        /* x^103424 mod p(x)` << 1, x^103488 mod p(x)` << 1 */
        .octa 0x00000001180f0bbc00000001415598a0

        /* x^102400 mod p(x)` << 1, x^102464 mod p(x)` << 1 */
        .octa 0x00000000e1d9177a00000000bf6c8c90

        /* x^101376 mod p(x)` << 1, x^101440 mod p(x)` << 1 */
        .octa 0x0000000105abc27c00000001788b0504

        /* x^100352 mod p(x)` << 1, x^100416 mod p(x)` << 1 */
        .octa 0x00000000972e4a580000000038385d02

        /* x^99328 mod p(x)` << 1, x^99392 mod p(x)` << 1 */
        .octa 0x0000000183499a5e00000001b6c83844

        /* x^98304 mod p(x)` << 1, x^98368 mod p(x)` << 1 */
        .octa 0x00000001c96a8cca0000000051061a8a

        /* x^97280 mod p(x)` << 1, x^97344 mod p(x)` << 1 */
        .octa 0x00000001a1a5b60c000000017351388a

        /* x^96256 mod p(x)` << 1, x^96320 mod p(x)` << 1 */
        .octa 0x00000000e4b6ac9c0000000132928f92

        /* x^95232 mod p(x)` << 1, x^95296 mod p(x)` << 1 */
        .octa 0x00000001807e7f5a00000000e6b4f48a

        /* x^94208 mod p(x)` << 1, x^94272 mod p(x)` << 1 */
        .octa 0x000000017a7e3bc80000000039d15e90

        /* x^93184 mod p(x)` << 1, x^93248 mod p(x)` << 1 */
        .octa 0x00000000d73975da00000000312d6074

        /* x^92160 mod p(x)` << 1, x^92224 mod p(x)` << 1 */
        .octa 0x000000017375d038000000017bbb2cc4

        /* x^91136 mod p(x)` << 1, x^91200 mod p(x)` << 1 */
        .octa 0x00000000193680bc000000016ded3e18

        /* x^90112 mod p(x)` << 1, x^90176 mod p(x)` << 1 */
        .octa 0x00000000999b06f600000000f1638b16

        /* x^89088 mod p(x)` << 1, x^89152 mod p(x)` << 1 */
        .octa 0x00000001f685d2b800000001d38b9ecc

        /* x^88064 mod p(x)` << 1, x^88128 mod p(x)` << 1 */
        .octa 0x00000001f4ecbed2000000018b8d09dc

        /* x^87040 mod p(x)` << 1, x^87104 mod p(x)` << 1 */
        .octa 0x00000000ba16f1a000000000e7bc27d2

        /* x^86016 mod p(x)` << 1, x^86080 mod p(x)` << 1 */
        .octa 0x0000000115aceac400000000275e1e96

        /* x^84992 mod p(x)` << 1, x^85056 mod p(x)` << 1 */
        .octa 0x00000001aeff629200000000e2e3031e

        /* x^83968 mod p(x)` << 1, x^84032 mod p(x)` << 1 */
        .octa 0x000000009640124c00000001041c84d8

        /* x^82944 mod p(x)` << 1, x^83008 mod p(x)` << 1 */
        .octa 0x0000000114f41f0200000000706ce672

        /* x^81920 mod p(x)` << 1, x^81984 mod p(x)` << 1 */
        .octa 0x000000009c5f3586000000015d5070da

        /* x^80896 mod p(x)` << 1, x^80960 mod p(x)` << 1 */
        .octa 0x00000001878275fa0000000038f9493a

        /* x^79872 mod p(x)` << 1, x^79936 mod p(x)` << 1 */
        .octa 0x00000000ddc42ce800000000a3348a76

        /* x^78848 mod p(x)` << 1, x^78912 mod p(x)` << 1 */
        .octa 0x0000000181d2c73a00000001ad0aab92

        /* x^77824 mod p(x)` << 1, x^77888 mod p(x)` << 1 */
        .octa 0x0000000141c9320a000000019e85f712

        /* x^76800 mod p(x)` << 1, x^76864 mod p(x)` << 1 */
        .octa 0x000000015235719a000000005a871e76

        /* x^75776 mod p(x)` << 1, x^75840 mod p(x)` << 1 */
        .octa 0x00000000be27d804000000017249c662

        /* x^74752 mod p(x)` << 1, x^74816 mod p(x)` << 1 */
        .octa 0x000000006242d45a000000003a084712

        /* x^73728 mod p(x)` << 1, x^73792 mod p(x)` << 1 */
        .octa 0x000000009a53638e00000000ed438478

        /* x^72704 mod p(x)` << 1, x^72768 mod p(x)` << 1 */
        .octa 0x00000001001ecfb600000000abac34cc

        /* x^71680 mod p(x)` << 1, x^71744 mod p(x)` << 1 */
        .octa 0x000000016d7c2d64000000005f35ef3e

        /* x^70656 mod p(x)` << 1, x^70720 mod p(x)` << 1 */
        .octa 0x00000001d0ce46c00000000047d6608c

        /* x^69632 mod p(x)` << 1, x^69696 mod p(x)` << 1 */
        .octa 0x0000000124c907b4000000002d01470e

        /* x^68608 mod p(x)` << 1, x^68672 mod p(x)` << 1 */
        .octa 0x0000000018a555ca0000000158bbc7b0

        /* x^67584 mod p(x)` << 1, x^67648 mod p(x)` << 1 */
        .octa 0x000000006b0980bc00000000c0a23e8e

        /* x^66560 mod p(x)` << 1, x^66624 mod p(x)` << 1 */
        .octa 0x000000008bbba96400000001ebd85c88

        /* x^65536 mod p(x)` << 1, x^65600 mod p(x)` << 1 */
        .octa 0x00000001070a5a1e000000019ee20bb2

        /* x^64512 mod p(x)` << 1, x^64576 mod p(x)` << 1 */
        .octa 0x000000002204322a00000001acabf2d6

        /* x^63488 mod p(x)` << 1, x^63552 mod p(x)` << 1 */
        .octa 0x00000000a27524d000000001b7963d56

        /* x^62464 mod p(x)` << 1, x^62528 mod p(x)` << 1 */
        .octa 0x0000000020b1e4ba000000017bffa1fe

        /* x^61440 mod p(x)` << 1, x^61504 mod p(x)` << 1 */
        .octa 0x0000000032cc27fc000000001f15333e

        /* x^60416 mod p(x)` << 1, x^60480 mod p(x)` << 1 */
        .octa 0x0000000044dd22b8000000018593129e

        /* x^59392 mod p(x)` << 1, x^59456 mod p(x)` << 1 */
        .octa 0x00000000dffc9e0a000000019cb32602

        /* x^58368 mod p(x)` << 1, x^58432 mod p(x)` << 1 */
        .octa 0x00000001b7a0ed140000000142b05cc8

        /* x^57344 mod p(x)` << 1, x^57408 mod p(x)` << 1 */
        .octa 0x00000000c784248800000001be49e7a4

        /* x^56320 mod p(x)` << 1, x^56384 mod p(x)` << 1 */
        .octa 0x00000001c02a4fee0000000108f69d6c

        /* x^55296 mod p(x)` << 1, x^55360 mod p(x)` << 1 */
        .octa 0x000000003c273778000000006c0971f0

        /* x^54272 mod p(x)` << 1, x^54336 mod p(x)` << 1 */
        .octa 0x00000001d63f8894000000005b16467a

        /* x^53248 mod p(x)` << 1, x^53312 mod p(x)` << 1 */
        .octa 0x000000006be557d600000001551a628e

        /* x^52224 mod p(x)` << 1, x^52288 mod p(x)` << 1 */
        .octa 0x000000006a7806ea000000019e42ea92

        /* x^51200 mod p(x)` << 1, x^51264 mod p(x)` << 1 */
        .octa 0x000000016155aa0c000000012fa83ff2

        /* x^50176 mod p(x)` << 1, x^50240 mod p(x)` << 1 */
        .octa 0x00000000908650ac000000011ca9cde0

        /* x^49152 mod p(x)` << 1, x^49216 mod p(x)` << 1 */
        .octa 0x00000000aa5a808400000000c8e5cd74

        /* x^48128 mod p(x)` << 1, x^48192 mod p(x)` << 1 */
        .octa 0x0000000191bb500a0000000096c27f0c

        /* x^47104 mod p(x)` << 1, x^47168 mod p(x)` << 1 */
        .octa 0x0000000064e9bed0000000002baed926

        /* x^46080 mod p(x)` << 1, x^46144 mod p(x)` << 1 */
        .octa 0x000000009444f302000000017c8de8d2

        /* x^45056 mod p(x)` << 1, x^45120 mod p(x)` << 1 */
        .octa 0x000000019db07d3c00000000d43d6068

        /* x^44032 mod p(x)` << 1, x^44096 mod p(x)` << 1 */
        .octa 0x00000001359e3e6e00000000cb2c4b26

        /* x^43008 mod p(x)` << 1, x^43072 mod p(x)` << 1 */
        .octa 0x00000001e4f10dd20000000145b8da26

        /* x^41984 mod p(x)` << 1, x^42048 mod p(x)` << 1 */
        .octa 0x0000000124f5735e000000018fff4b08

        /* x^40960 mod p(x)` << 1, x^41024 mod p(x)` << 1 */
        .octa 0x0000000124760a4c0000000150b58ed0

        /* x^39936 mod p(x)` << 1, x^40000 mod p(x)` << 1 */
        .octa 0x000000000f1fc18600000001549f39bc

        /* x^38912 mod p(x)` << 1, x^38976 mod p(x)` << 1 */
        .octa 0x00000000150e4cc400000000ef4d2f42

        /* x^37888 mod p(x)` << 1, x^37952 mod p(x)` << 1 */
        .octa 0x000000002a6204e800000001b1468572

        /* x^36864 mod p(x)` << 1, x^36928 mod p(x)` << 1 */
        .octa 0x00000000beb1d432000000013d7403b2

        /* x^35840 mod p(x)` << 1, x^35904 mod p(x)` << 1 */
        .octa 0x0000000135f3f1f000000001a4681842

        /* x^34816 mod p(x)` << 1, x^34880 mod p(x)` << 1 */
        .octa 0x0000000074fe22320000000167714492

        /* x^33792 mod p(x)` << 1, x^33856 mod p(x)` << 1 */
        .octa 0x000000001ac6e2ba00000001e599099a

        /* x^32768 mod p(x)` << 1, x^32832 mod p(x)` << 1 */
        .octa 0x0000000013fca91e00000000fe128194

        /* x^31744 mod p(x)` << 1, x^31808 mod p(x)` << 1 */
        .octa 0x0000000183f4931e0000000077e8b990

        /* x^30720 mod p(x)` << 1, x^30784 mod p(x)` << 1 */
        .octa 0x00000000b6d9b4e400000001a267f63a

        /* x^29696 mod p(x)` << 1, x^29760 mod p(x)` << 1 */
        .octa 0x00000000b518865600000001945c245a

        /* x^28672 mod p(x)` << 1, x^28736 mod p(x)` << 1 */
        .octa 0x0000000027a81a840000000149002e76

        /* x^27648 mod p(x)` << 1, x^27712 mod p(x)` << 1 */
        .octa 0x000000012569925800000001bb8310a4

        /* x^26624 mod p(x)` << 1, x^26688 mod p(x)` << 1 */
        .octa 0x00000001b23de796000000019ec60bcc

        /* x^25600 mod p(x)` << 1, x^25664 mod p(x)` << 1 */
        .octa 0x00000000fe4365dc000000012d8590ae

        /* x^24576 mod p(x)` << 1, x^24640 mod p(x)` << 1 */
        .octa 0x00000000c68f497a0000000065b00684

        /* x^23552 mod p(x)` << 1, x^23616 mod p(x)` << 1 */
        .octa 0x00000000fbf521ee000000015e5aeadc

        /* x^22528 mod p(x)` << 1, x^22592 mod p(x)` << 1 */
        .octa 0x000000015eac337800000000b77ff2b0

        /* x^21504 mod p(x)` << 1, x^21568 mod p(x)` << 1 */
        .octa 0x0000000134914b900000000188da2ff6

        /* x^20480 mod p(x)` << 1, x^20544 mod p(x)` << 1 */
        .octa 0x0000000016335cfe0000000063da929a

        /* x^19456 mod p(x)` << 1, x^19520 mod p(x)` << 1 */
        .octa 0x000000010372d10c00000001389caa80

        /* x^18432 mod p(x)` << 1, x^18496 mod p(x)` << 1 */
        .octa 0x000000015097b908000000013db599d2

        /* x^17408 mod p(x)` << 1, x^17472 mod p(x)` << 1 */
        .octa 0x00000001227a75720000000122505a86

        /* x^16384 mod p(x)` << 1, x^16448 mod p(x)` << 1 */
        .octa 0x000000009a8f75c0000000016bd72746

        /* x^15360 mod p(x)` << 1, x^15424 mod p(x)` << 1 */
        .octa 0x00000000682c77a200000001c3faf1d4

        /* x^14336 mod p(x)` << 1, x^14400 mod p(x)` << 1 */
        .octa 0x00000000231f091c00000001111c826c

        /* x^13312 mod p(x)` << 1, x^13376 mod p(x)` << 1 */
        .octa 0x000000007d4439f200000000153e9fb2

        /* x^12288 mod p(x)` << 1, x^12352 mod p(x)` << 1 */
        .octa 0x000000017e221efc000000002b1f7b60

        /* x^11264 mod p(x)` << 1, x^11328 mod p(x)` << 1 */
        .octa 0x0000000167457c3800000000b1dba570

        /* x^10240 mod p(x)` << 1, x^10304 mod p(x)` << 1 */
        .octa 0x00000000bdf081c400000001f6397b76

        /* x^9216 mod p(x)` << 1, x^9280 mod p(x)` << 1 */
        .octa 0x000000016286d6b00000000156335214

        /* x^8192 mod p(x)` << 1, x^8256 mod p(x)` << 1 */
        .octa 0x00000000c84f001c00000001d70e3986

        /* x^7168 mod p(x)` << 1, x^7232 mod p(x)` << 1 */
        .octa 0x0000000064efe7c0000000003701a774

        /* x^6144 mod p(x)` << 1, x^6208 mod p(x)` << 1 */
        .octa 0x000000000ac2d90400000000ac81ef72

        /* x^5120 mod p(x)` << 1, x^5184 mod p(x)` << 1 */
        .octa 0x00000000fd226d140000000133212464

        /* x^4096 mod p(x)` << 1, x^4160 mod p(x)` << 1 */
        .octa 0x000000011cfd42e000000000e4e45610

        /* x^3072 mod p(x)` << 1, x^3136 mod p(x)` << 1 */
        .octa 0x000000016e5a5678000000000c1bd370

        /* x^2048 mod p(x)` << 1, x^2112 mod p(x)` << 1 */
        .octa 0x00000001d888fe2200000001a7b9e7a6

        /* x^1024 mod p(x)` << 1, x^1088 mod p(x)` << 1 */
        .octa 0x00000001af77fcd4000000007d657a10

.short_constants:

        /* Reduce final 1024-2048 bits to 64 bits, shifting 32 bits to include the trailing 32 bits of zeros */
        /* x^1952 mod p(x)`, x^1984 mod p(x)`, x^2016 mod p(x)`, x^2048 mod p(x)` */
        .octa 0xed837b2613e8221e99168a18ec447f11

        /* x^1824 mod p(x)`, x^1856 mod p(x)`, x^1888 mod p(x)`, x^1920 mod p(x)` */
        .octa 0xc8acdd8147b9ce5ae23e954e8fd2cd3c

        /* x^1696 mod p(x)`, x^1728 mod p(x)`, x^1760 mod p(x)`, x^1792 mod p(x)` */
        .octa 0xd9ad6d87d4277e2592f8befe6b1d2b53

        /* x^1568 mod p(x)`, x^1600 mod p(x)`, x^1632 mod p(x)`, x^1664 mod p(x)` */
        .octa 0xc10ec5e033fbca3bf38a3556291ea462

        /* x^1440 mod p(x)`, x^1472 mod p(x)`, x^1504 mod p(x)`, x^1536 mod p(x)` */
        .octa 0xc0b55b0e82e02e2f974ac56262b6ca4b

        /* x^1312 mod p(x)`, x^1344 mod p(x)`, x^1376 mod p(x)`, x^1408 mod p(x)` */
        .octa 0x71aa1df0e172334d855712b3784d2a56

        /* x^1184 mod p(x)`, x^1216 mod p(x)`, x^1248 mod p(x)`, x^1280 mod p(x)` */
        .octa 0xfee3053e3969324da5abe9f80eaee722

        /* x^1056 mod p(x)`, x^1088 mod p(x)`, x^1120 mod p(x)`, x^1152 mod p(x)` */
        .octa 0xf44779b93eb2bd081fa0943ddb54814c

        /* x^928 mod p(x)`, x^960 mod p(x)`, x^992 mod p(x)`, x^1024 mod p(x)` */
        .octa 0xf5449b3f00cc3374a53ff440d7bbfe6a

        /* x^800 mod p(x)`, x^832 mod p(x)`, x^864 mod p(x)`, x^896 mod p(x)` */
        .octa 0x6f8346e1d777606eebe7e3566325605c

        /* x^672 mod p(x)`, x^704 mod p(x)`, x^736 mod p(x)`, x^768 mod p(x)` */
        .octa 0xe3ab4f2ac0b95347c65a272ce5b592b8

        /* x^544 mod p(x)`, x^576 mod p(x)`, x^608 mod p(x)`, x^640 mod p(x)` */
        .octa 0xaa2215ea329ecc115705a9ca4721589f

        /* x^416 mod p(x)`, x^448 mod p(x)`, x^480 mod p(x)`, x^512 mod p(x)` */
        .octa 0x1ed8f66ed95efd26e3720acb88d14467

        /* x^288 mod p(x)`, x^320 mod p(x)`, x^352 mod p(x)`, x^384 mod p(x)` */
        .octa 0x78ed02d5a700e96aba1aca0315141c31

        /* x^160 mod p(x)`, x^192 mod p(x)`, x^224 mod p(x)`, x^256 mod p(x)` */
        .octa 0xba8ccbe832b39da3ad2a31b3ed627dae

        /* x^32 mod p(x)`, x^64 mod p(x)`, x^96 mod p(x)`, x^128 mod p(x)` */
        .octa 0xedb88320b1e6b0926655004fa06a2517


.barrett_constants:
        /* 33 bit reflected Barrett constant m - (4^32)/n */
        .octa 0x000000000000000000000001f7011641        /* x^64 div p(x)` */
        /* 33 bit reflected Barrett constant n */
        .octa 0x000000000000000000000001db710641

#endif
#endif
