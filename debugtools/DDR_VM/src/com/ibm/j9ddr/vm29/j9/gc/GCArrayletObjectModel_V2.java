/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 *******************************************************************************/
package com.ibm.j9ddr.vm29.j9.gc;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.J9IndexableObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9IndexableObjectHelper;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.types.UDATA;

class GCArrayletObjectModel_V2 extends GCArrayletObjectModelBase
{
	
	public GCArrayletObjectModel_V2() throws CorruptDataException 
	{		
	}

	@Override
	public UDATA getSizeInBytesWithHeader(J9IndexableObjectPointer array) throws CorruptDataException
	{
		return super.getSpineSize(array);
	}

	/**
	 * Return the total number of arraylets for an indexable object with a size of dataInSizeByte, including a (possibly empty) leaf in the spine.
	 * @param dataSizeInBytes size of an array in bytes (not elements)
	 * @return the number of arraylets used for an array of dataSizeInBytes bytes
	 */
	@Override
	protected UDATA numArraylets(UDATA dataSizeInBytes) throws CorruptDataException
	{
		UDATA numberOfArraylets = new UDATA(1);
		if (!UDATA.MAX.eq(arrayletLeafSize)) {
			/* CMVC 135307 : following logic for calculating the leaf count would not overflow dataSizeInBytes.
			 * the assumption is leaf size is order of 2. It's identical to:
			 * if (dataSizeInBytes % leafSize) is 0
			 * 	leaf count = dataSizeInBytes >> leafLogSize
			 * else
			 * 	leaf count = (dataSizeInBytes >> leafLogSize) + 1
			 */
			numberOfArraylets = dataSizeInBytes.rightShift(arrayletLeafLogSize).add(dataSizeInBytes.bitAnd(arrayletLeafSizeMask).add(arrayletLeafSizeMask).rightShift(arrayletLeafLogSize));
		}
		return numberOfArraylets;
	}

	@Override
	public UDATA getTotalFootprintInBytesWithHeader(J9IndexableObjectPointer arrayPtr) throws CorruptDataException {
		UDATA spineSize = getSizeInBytesWithHeader(arrayPtr);
		UDATA externalArrayletSize = externalArrayletsSize(arrayPtr);
		UDATA totalFootprint = spineSize.add(externalArrayletSize);
		return totalFootprint;
	}

	/**
	 * Determine the validity of the data address belonging to arrayPtr.
	 *
	 * @param arrayPtr array object who's data address validity we are checking
	 * @throws CorruptDataException if there's a problem accessing the indexable object dataAddr field
	 * @throws NoSuchFieldException if the indexable object dataAddr field does not exist on the build that generated the core file
	 * @return true if the data address of arrayPtr is valid, false otherwise
	 */
	@Override
	public boolean hasCorrectDataAddrPointer(J9IndexableObjectPointer arrayPtr) throws CorruptDataException, NoSuchFieldException
	{
		boolean hasCorrectDataAddrPointer;
		UDATA dataSizeInBytes = getDataSizeInBytes(arrayPtr);
		VoidPointer dataAddr = J9IndexableObjectHelper.getDataAddrForIndexable(arrayPtr);
		boolean isValidDataAddrForDoubleMappedObject = isIndexableObjectDoubleMapped(dataAddr, dataSizeInBytes);

		if (dataSizeInBytes.isZero()) {
			VoidPointer discontiguousDataAddr = VoidPointer.cast(arrayPtr.addOffset(J9IndexableObjectHelper.discontiguousHeaderSize()));
			hasCorrectDataAddrPointer = (dataAddr.isNull() || dataAddr.equals(discontiguousDataAddr));
		} else if (dataSizeInBytes.lt(arrayletLeafSize)) {
			VoidPointer contiguousDataAddr = VoidPointer.cast(arrayPtr.addOffset(J9IndexableObjectHelper.contiguousHeaderSize()));
			hasCorrectDataAddrPointer = dataAddr.equals(contiguousDataAddr);
		} else {
			if (enableVirtualLargeObjectHeap || enableDoubleMapping) {
				hasCorrectDataAddrPointer = isValidDataAddrForDoubleMappedObject;
			} else {
				hasCorrectDataAddrPointer = dataAddr.isNull();
			}
		}

		return hasCorrectDataAddrPointer;
	}
}
