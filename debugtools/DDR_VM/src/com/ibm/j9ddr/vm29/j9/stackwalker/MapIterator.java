/*******************************************************************************
 * Copyright (c) 2009, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9.stackwalker;

import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JITExceptionTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JITStackAtlasPointer;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * Mutable version of TR_MapIterator
 * 
 * @author andhall
 *
 */
class MapIterator
{
	UDATA _rangeStartOffset;
	UDATA _rangeEndOffset;

	J9JITExceptionTablePointer _methodMetaData;
	J9JITStackAtlasPointer _stackAtlas;
	U8Pointer _currentMap;
	U8Pointer _currentStackMap;
	U8Pointer _currentInlineMap;
	U8Pointer _nextMap;
	U32 _mapIndex;
	boolean _isGap;

}
