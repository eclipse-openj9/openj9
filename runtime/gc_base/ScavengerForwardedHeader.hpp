/*******************************************************************************
 * Copyright (c) 1991, 2021 IBM Corp. and others
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

#if !defined(SCAVENGERFORWARDEDHEADER_HPP_)
#define SCAVENGERFORWARDEDHEADER_HPP_

#include "j9.h"
#include "j9cfg.h"

/* @ddr_namespace: map_to_type=MM_ScavengerForwardedHeader */

/* used to distinguish a forwarded object from a class pointer */
#define FORWARDED_TAG ((uintptr_t)0x4)
/* the grow tag is used by VLHGC and can only be set if the FORWARDED_TAG is already set.  It signifies that the object grew a hash field when moving */
#define GROW_TAG ((uintptr_t)0x2)
/* combine these flags into one mask which should be stripped from the pointer in order to remove all tags */
#define ALL_TAGS (FORWARDED_TAG | GROW_TAG)

/**
 * Used to construct a linked list of objects on the heap.
 * This structure provides the ability to restore the object, so
 * only data which can be restored from the class is destroyed. 
 * Because its fields must line up with J9Object no virtual methods
 * are permitted.
 * 
 * @ingroup GC_Base
 */
class MM_ScavengerForwardedHeader
{
public:
protected:
	omrobjectptr_t _objectPtr; /**< the object on which to act */
	uintptr_t _preserved; /**< a backup copy of the header fields which may be modified by this class */
#if defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS)
	bool _compressObjectReferences;
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS) */
private:
};
#endif /* SCAVENGERFORWARDEDHEADER_HPP_ */
