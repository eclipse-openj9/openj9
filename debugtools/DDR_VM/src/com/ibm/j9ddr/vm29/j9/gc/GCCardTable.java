/*******************************************************************************
 * Copyright (c) 2001, 2015 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9.gc;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

import java.util.Iterator;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.types.U8;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_CardTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_GCExtensionsPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.structure.J9ModroncoreConstants;
import com.ibm.j9ddr.vm29.types.UDATA;

public class GCCardTable {

	@SuppressWarnings("unused")
	private MM_CardTablePointer _cardTable;
	private U8Pointer _cardTableVirtualStart;
	private VoidPointer _heapBase;
	private U8Pointer _cardTableStart;
	
	static UDATA CARD_SIZE_SHIFT = new UDATA(J9ModroncoreConstants.CARD_SIZE_SHIFT);
	
	public GCCardTable(MM_CardTablePointer cardTable) throws CorruptDataException
	{
		_cardTable = cardTable;
		_cardTableVirtualStart = cardTable._cardTableVirtualStart();
		_heapBase = cardTable._heapBase();
		_cardTableStart = cardTable._cardTableStart();
	}
	
	static public GCCardTable from() throws CorruptDataException
	{
		J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
		MM_GCExtensionsPointer extensions = GCExtensions.getGCExtensionsPointer();
		MM_CardTablePointer cardTable = extensions.cardTable();
		return new GCCardTable(cardTable);
	}
	
	public U8Pointer heapAddrToCardAddr(VoidPointer heapAddr)
	{
		/* Check passed reference within the heap */
//		Assert_MM_true((UDATA *)heapAddr >= (UDATA *)getHeapBase());
//		Assert_MM_true((UDATA *)heapAddr <= (UDATA *)_heapAlloc);
		
		UDATA index = UDATA.cast(heapAddr).rightShift(CARD_SIZE_SHIFT);
		return _cardTableVirtualStart.add(index);
	}
	
	public VoidPointer cardAddrToHeapAddr(U8Pointer cardAddr)
	{
		/* Check passed card address is within the card table  */
//		Assert_MM_true((void *)cardAddr >= getCardTableStart());
//		Assert_MM_true((void *)cardAddr < _cardTableMemory->getHeapTop());
		
		UDATA index = UDATA.cast(cardAddr).sub(UDATA.cast(_cardTableStart));
		UDATA delta = index.leftShift(CARD_SIZE_SHIFT);
		
		return _heapBase.addOffset(delta);
	}
	
	public void cleanRange(U8Pointer lowCard, U8Pointer highCard, GCCardCleaner cardCleaner) throws CorruptDataException
	{
		U8Pointer thisCard = lowCard;
		U8Pointer endCard = highCard;
		
		while (thisCard.lt(endCard)) {
			try {
				U8 card = thisCard.at(0);
				if (!card.eq(J9ModroncoreConstants.CARD_CLEAN)) {
					VoidPointer lowAddress = cardAddrToHeapAddr(thisCard);
					VoidPointer highAddress = lowAddress.addOffset(J9ModroncoreConstants.CARD_SIZE);
					cardCleaner.clean(lowAddress, highAddress, thisCard);
				}
			} catch (CorruptDataException cde) {
				raiseCorruptDataEvent("Corrupt Card", cde, false);
			}
			thisCard = thisCard.add(1);
		}
	}
	
	public void cleanCardsInRegion(GCHeapRegionDescriptor region, GCCardCleaner cardCleaner) throws CorruptDataException
	{
		U8Pointer lowCard = heapAddrToCardAddr(region.getLowAddress());
		U8Pointer highCard = heapAddrToCardAddr(region.getHighAddress());
		cleanRange(lowCard, highCard, cardCleaner);
	}
	
	public void cleanCardsInRegions(Iterator<GCHeapRegionDescriptor> regionIterator, GCCardCleaner cardCleaner) throws CorruptDataException
	{
		while (regionIterator.hasNext()) {
			GCHeapRegionDescriptor region = regionIterator.next();
			cleanCardsInRegion(region, cardCleaner);
		}
	}
	
}
