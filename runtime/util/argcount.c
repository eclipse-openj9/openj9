/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#include "j9.h"

/* These represent argcounts for java field descriptors, characters without descriptions are currently unused */
const U_8 argCountCharConversion[] = {
    0, /* A */
    1, /* B -> byte */
    1, /* C -> char */
    2, /* D -> double */
    0, /* E */
    1, /* F -> float */
    0, /* G */
    0, /* H */
    1, /* I -> int */
    2, /* J -> long */
    0, /* K */
    1, /* L -> nullable named class or interface type */
    0, /* M */
    0, /* N */
    0, /* O */
    0, /* P */
    1, /* Q -> null-free named inline class type (also known as value type) */
    0, /* R */
    1, /* S -> short */
    0, /* T */
    0, /* U */
    0, /* V -> void */
    0, /* W */
    0, /* X */
    0, /* Y */
    1, /* Z -> boolean */
    1  /* [ -> 1-D array reference */
}; 



                 

