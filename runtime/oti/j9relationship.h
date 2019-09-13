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
#ifndef j9relationship_h
#define j9relationship_h
typedef struct J9ClassRelationship {
    U_8 *className;
    UDATA classNameLength;
    struct J9ClassRelationshipNode *root;
    U_32 flags;
} J9ClassRelationship;

typedef struct J9ClassRelationshipNode {
    U_8 *className;
    UDATA classNameLength;
    struct J9ClassRelationshipNode *linkNext;
    struct J9ClassRelationshipNode *linkPrevious;
} J9ClassRelationshipNode;

#define J9RELATIONSHIP_JAVA_LANG_THROWABLE_STRING "java/lang/Throwable"
#define J9RELATIONSHIP_JAVA_LANG_THROWABLE_STRING_LENGTH (sizeof(J9RELATIONSHIP_JAVA_LANG_THROWABLE_STRING) - 1)

/* Bits for J9ClassRelationship flags field */
#define J9RELATIONSHIP_MUST_BE_INTERFACE 0x1
#define J9RELATIONSHIP_PARENT_IS_THROWABLE 0x2

#endif /* j9relationship_h */
