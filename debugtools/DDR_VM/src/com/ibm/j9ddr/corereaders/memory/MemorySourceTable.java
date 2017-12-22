/*******************************************************************************
 * Copyright (c) 2009, 2016 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders.memory;

import static java.util.logging.Level.FINE;
import static java.util.logging.Level.FINER;

import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.ListIterator;
import java.util.logging.Logger;

/**
 * Class encapsulating the logic for taking a list of memory 
 * sources and efficiently finding ranges for addresses
 * 
 * @author andhall
 *
 */
public class MemorySourceTable
{
	private static final Logger logger = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);
	
	private static final String FORCE_BINARY_CHOP_RESOLVER_SYSTEM_PROPERTY = "ddr.force.binary.address.resolver";
	
	private static final boolean FORCE_BINARY_CHOP_RESOLVER;
	
	private static final String ALLOW_THREE_TIER_TABLE_RESOLVER_PROPERTY = "ddr.allow.three.tier.address.resolver";
	
	private static final boolean ALLOW_THREE_TIER_TABLE_RESOLVER;
	
	static long tlbCacheHits = 0;
	static long tlbCacheMisses = 0;
	
	static {
		String forceBinaryResolverString = AccessController.doPrivileged(new PrivilegedAction<String>() {

			public String run()
			{
				return System.getProperty(FORCE_BINARY_CHOP_RESOLVER_SYSTEM_PROPERTY);
			}
			
		});
		
		logger.logp(FINE,"MemoryRangeTable","<clinit>","System property {0} set to {1}",new Object[]{FORCE_BINARY_CHOP_RESOLVER_SYSTEM_PROPERTY,forceBinaryResolverString});
		
		if ( forceBinaryResolverString != null && forceBinaryResolverString.toLowerCase().equals("true") ) {
			FORCE_BINARY_CHOP_RESOLVER = true;
			logger.logp(FINE,"MemoryRangeTable","<clinit>","BinaryChopResolver forced on.");
		} else {
			logger.logp(FINE,"MemoryRangeTable","<clinit>","BinaryChopResolver NOT forced on.");
			FORCE_BINARY_CHOP_RESOLVER = false;
		}
		
		String allowThreeTierResolverString = AccessController.doPrivileged(new PrivilegedAction<String>() {

			public String run()
			{
				return System.getProperty(ALLOW_THREE_TIER_TABLE_RESOLVER_PROPERTY);
			}
			
		});
		
		logger.logp(FINE,"MemoryRangeTable","<clinit>","System property {0} set to {1}",new Object[]{ALLOW_THREE_TIER_TABLE_RESOLVER_PROPERTY,allowThreeTierResolverString});
		
		if ( allowThreeTierResolverString != null && allowThreeTierResolverString.toLowerCase().equals("true") ) {
			ALLOW_THREE_TIER_TABLE_RESOLVER = true;
			logger.logp(FINE,"MemoryRangeTable","<clinit>","ThreeTierResolver selection allowed.");
		} else {
			logger.logp(FINE,"MemoryRangeTable","<clinit>","ThreeTierResolver selection NOT allowed.");
			ALLOW_THREE_TIER_TABLE_RESOLVER = false;
		}
	}
	
	private IAddressResolverStrategy addressResolver = null;
	
	private final List<IMemorySource> rawMemorySources = new ArrayList<IMemorySource>();
	private List<IMemorySource> memorySources;

	public final void addMemorySource(IMemorySource source)
	{
		rawMemorySources.add(source);
		addressResolver = null;
	}

	public void removeMemorySource(IMemorySource source)
	{
		rawMemorySources.remove(source);
		addressResolver = null;
	}
	
	public final List<IMemoryRange> getMemorySources() 
	{
		mergeOverlappingRanges();
		return new ArrayList<IMemoryRange>(memorySources);
	}
	
	public final IMemorySource getRangeForAddress(long address)
	{
		if (addressResolver == null) {
			pickAddressResolver();
		}
		
		return addressResolver.getRangeForAddress(address);
	}
	
	private void pickAddressResolver()
	{
		mergeOverlappingRanges();
		
		//Need to figure out highest address and worst alignment
		long highestAddress = 0;
		int worstAlignment = Integer.MAX_VALUE;
		long smallestRange = Integer.MAX_VALUE;
		
		for (IMemoryRange range : memorySources) {
			if ( Addresses.greaterThan(range.getTopAddress(), highestAddress) ) {
				highestAddress = range.getTopAddress();
			}
			
			int alignment = getAlignment(range.getBaseAddress());
			
			if (alignment < worstAlignment) {
				worstAlignment = alignment;
			}
			
			if (range.getSize() < smallestRange) {
				smallestRange = range.getSize();
			}
		}
		
		logger.logp(FINE,"MemoryRangeTable", "pickAddressResolver", "Picking address resolver. Highest Address = 0x{0}, worst alignment = {1} bit.",new Object[]{Long.toHexString(highestAddress),worstAlignment});
		
		if (FORCE_BINARY_CHOP_RESOLVER) {
			logger.logp(FINE,"MemoryRangeTable", "pickAddressResolver", "Selection overridden with {0}",FORCE_BINARY_CHOP_RESOLVER_SYSTEM_PROPERTY);
			addressResolver = new BinaryChopAddressResolver(memorySources);
		} else {
			if (worstAlignment >= 12 && smallestRange >= 4096) {
				if (Addresses.lessThan(highestAddress,0x100000000L)) {
					addressResolver = new FlatPageTableAddressResolver(memorySources,highestAddress,worstAlignment);
				} else if( ALLOW_THREE_TIER_TABLE_RESOLVER ){
					logger.logp(FINE,"MemoryRangeTable", "pickAddressResolver", "Three tier table resolver selected, allowed by {0} setting",ALLOW_THREE_TIER_TABLE_RESOLVER_PROPERTY);
					addressResolver = new ThreeTierPageTableAddressResolver(memorySources, highestAddress, worstAlignment);
				}
			}

		}
		/* Can't use one of the clever resolvers. We will fall back to binary-chop. */
		if( addressResolver == null ) {
			addressResolver = new BinaryChopAddressResolver(memorySources);
		}
		
		logger.logp(FINE,"MemoryRangeTable", "pickAddressResolver", "Picked {0} as address resolver.",addressResolver.getClass().getSimpleName());
	}
	
	private void mergeOverlappingRanges()
	{
		memorySources = new ArrayList<IMemorySource>(rawMemorySources);
		Collections.sort(memorySources);
		//Resolve any overlapping ranges (seems to happen on AIX occasionally)
		if (memorySources.size() > 0) {
			
			ListIterator<IMemorySource> rangeIt = memorySources.listIterator();
			
			IMemorySource previous = rangeIt.next();
			
			while (rangeIt.hasNext()) {
				IMemorySource thisRange = rangeIt.next();
				
				if (thisRange.overlaps(previous)) {
					logger.logp(FINE,"MemoryRangeTable","mergeOverlappingRanges","Address range {0} overlaps with {1}",new Object[]{thisRange,previous});
					//Either thisRange is a subrange of previous or we overlap partially
					if (previous.isSubRange(thisRange)) {
						//Remove thisRange
						logger.logp(FINER,"MemoryRangeTable","mergeOverlappingRanges","Removing {0}",thisRange);
						rangeIt.remove();
						//Leave previous as-is
					} else if (previous.isBacked() == thisRange.isBacked()) {
						logger.logp(FINER,"MemoryRangeTable","mergeOverlappingRanges","Merging {0} and {1}", new Object[]{thisRange,previous});
						//Merge
						rangeIt.remove();
						rangeIt.previous();
						rangeIt.remove();
						IMemorySource toAdd = new MergedMemoryRange(previous,thisRange);
						rangeIt.add(toAdd);
						previous = toAdd;
					} else {
						previous = thisRange;
					}
				} else {
					previous = thisRange;
				}
			}
		}
	}
	
	static int getAlignment(long address)
	{
		int alignment = 0;
		for (int i=0; i < 64; i++) {
			
			if ((address & 0x1) == 0) {
				alignment++;
			} else {
				break;
			}
			
			address >>= 1;
		}
		
		return alignment;
	}

	/**
	 * Memory range used for merging overlapping memory ranges.
	 * 
	 */
	private static class MergedMemoryRange extends BaseMemoryRange implements IMemorySource
	{

		private final IMemorySource lower;
		
		private final IMemorySource upper;
		
		public MergedMemoryRange(IMemorySource lower, IMemorySource upper)
		{
			super(lower.getBaseAddress(),upper.getTopAddress() - lower.getBaseAddress() + 1);
			this.lower = lower;
			this.upper = upper;
			if (lower.isBacked() != upper.isBacked()) {
				throw new RuntimeException("Error: creating an instance of MergedMemoryRange where backed and unbacked memory sources are merged");
			}
		}
		
		public int getAddressSpaceId()
		{
			return lower.getAddressSpaceId();
		}
		
		@Override
		public boolean isBacked() {
			return upper.isBacked();
		}

		public int getBytes(long address, byte[] buffer, int offset, int length)
				throws MemoryFault
		{
			if (lower.contains(address) && lower.contains(address + length)) {
				return lower.getBytes(address, buffer, offset, length);
			} else if (Addresses.greaterThan(address, lower.getTopAddress())) {
				return upper.getBytes(address, buffer, offset, length);
			} else {
				//Span
				int sizeInLower = (int)(lower.getTopAddress() - address + 1);
				
				int read = lower.getBytes(address, buffer, offset, sizeInLower);
				
				if (read == sizeInLower) {
					read += upper.getBytes(address + sizeInLower, buffer, offset + sizeInLower, length - sizeInLower);
				}
				
				return read;
			}
		}

		public boolean isExecutable()
		{
			return lower.isExecutable() && upper.isExecutable();
		}

		public boolean isReadOnly()
		{
			return lower.isExecutable() && upper.isExecutable();
		}

		public boolean isShared()
		{
			return lower.isShared() && upper.isShared();
		}

		public String getName()
		{
			String lowerResult = lower.getName();
			String upperResult = upper.getName();
			
			if (lowerResult != null) {
				if (upperResult != null) {
					return lowerResult + " merged with " + upperResult;
				} else {
					return lowerResult;
				}
			} else {
				if (upperResult != null) {
					return upperResult;
				} else {
					return null;
				}
			}
		}
	}

	
	/* Abstract strategy for matching addresses to address ranges */
	private static interface IAddressResolverStrategy
	{
		public IMemorySource getRangeForAddress(long address);
	}
	
	/* Class used for debugging misbehaving addres resolver strategies */
	@SuppressWarnings("unused")
	private static class DebugAddressResolverStrategy implements IAddressResolverStrategy
	{
		private final IAddressResolverStrategy trusted;
		private final IAddressResolverStrategy untrusted;
		
		public DebugAddressResolverStrategy(IAddressResolverStrategy trusted, IAddressResolverStrategy untrusted)
		{
			this.trusted = trusted;
			this.untrusted = untrusted;
		}
		
		public IMemorySource getRangeForAddress(long address)
		{
			IMemorySource trustedResult = trusted.getRangeForAddress(address);
			IMemorySource untrustedResult = untrusted.getRangeForAddress(address);
			
			if (trustedResult.equals(untrustedResult)) {
				return trustedResult;
			} else {
				throw new RuntimeException("Mismatch for address 0x" + Long.toHexString(address) + ". Trusted gave " + trustedResult + ", untrusted gave " + untrustedResult);
			}
		}
	}
	
	private static class BinaryChopAddressResolver implements IAddressResolverStrategy
	{
		private List<IMemorySource> memoryRanges;
		
		private IMemorySource tlbEntry1 = null;
		private long entry1HitCount = 0;
		
		private IMemorySource tlbEntry2 = null;
		private long entry2HitCount = 0;
		
		public BinaryChopAddressResolver(List<IMemorySource> memoryRanges)
		{
			this.memoryRanges = memoryRanges;
			Collections.sort(memoryRanges);
		}
		
		public IMemorySource getRangeForAddress(long address)
		{
			int bottom = 0;
			int top = memoryRanges.size() - 1;
			
			IMemorySource tlbEntry = tlbCheck(address);
			
			if (tlbEntry != null) {
				if (AbstractMemory.RECORDING_CACHE_STATS) {
					tlbCacheHits++;
				}
				
				return tlbEntry;
			}
			
			if (AbstractMemory.RECORDING_CACHE_STATS) {
				tlbCacheMisses++;
			}
			
			while (true) {
				int middle = (bottom + top) / 2;
				
				IMemorySource midPoint = memoryRanges.get(middle);
				
				if (Addresses.greaterThan(midPoint.getBaseAddress(),address)) {
					if (bottom == top) {
						//Memory fault
						return null;
					} else {
						top = middle;
					}
				} else if (Addresses.lessThan(midPoint.getTopAddress(),address)) {
					if (top == bottom) {
						//Memory fault
						return null;
					} else if (middle == bottom) {
						//If middle == bottom then top must be bottom + 1
						bottom = top;
					} else {
						bottom = middle;
					}
				} else {
					/* Match */
					tlbInsert(midPoint);
					return midPoint;
				}
			}
		}
		
		private IMemorySource tlbCheck(long address)
		{
			if (tlbEntry1 != null && tlbEntry1.contains(address)) {
				entry1HitCount++;
				return tlbEntry1;
			}
			
			if (tlbEntry2 != null && tlbEntry2.contains(address)) {
				entry2HitCount++;
				return tlbEntry2;
			}
			
			return null;
		}

		private void tlbInsert(IMemorySource newEntry)
		{
			if (tlbEntry1 == null) {
				tlbEntry1 = newEntry;
			} else  if (tlbEntry2 == null) {
				tlbEntry2 = newEntry;
			} else if (entry1HitCount >= entry2HitCount) {
				tlbEntry2 = newEntry;
			} else {
				tlbEntry1 = newEntry;
			}
			
			entry1HitCount = 0;
			entry2HitCount = 0;
		}
		
	}
	
	private static class FlatPageTableAddressResolver implements IAddressResolverStrategy
	{
		private final IMemorySource[] pageTable;
		
		private final long alignment;

		public FlatPageTableAddressResolver(List<IMemorySource> memorySource,
				long highestAddress, int alignment)
		{
			this.alignment = alignment;
			long slots = (highestAddress >>> alignment) + 1;
			
			pageTable = new IMemorySource[(int)slots];
			
			for (IMemorySource range : memorySource) {
				int baseSlot = (int)(range.getBaseAddress() >>> alignment);
				int topSlot = (int)(range.getTopAddress() >>> alignment);
				
				for (int i = baseSlot; i <= topSlot; i++) {
					pageTable[i] = range;
				}
			}
		}

		public IMemorySource getRangeForAddress(long address)
		{
			int slot = (int)(address >>> alignment);
			
			if (slot >= pageTable.length) {
				return null;
			}
			
			return pageTable[slot];
		}
		
	}
	
	/**
	 * Uses a three-tier page table to break up the look-up table into smaller chunks. Uses much less space
	 * when the address space is sparse.
	 * @author andhall
	 *
	 */
	private static class ThreeTierPageTableAddressResolver implements IAddressResolverStrategy
	{

		private final Object[] pageTable;
		private final int pageTableSize;
		
		private final long alignment;
		
		//Sizes of each tier
		private final int topLevelBits;
		private final int midTierBits;
		private final int bottomTierBits;
		
		public ThreeTierPageTableAddressResolver(List<IMemorySource> memorySources, long highestAddress, int alignment)
		{
			this.alignment = alignment;
			long slots = (highestAddress >>> alignment);
			
			//Work out bits
			int bits = (int)Math.ceil((Math.log(slots) / Math.log(2)));
			
			if (bits > 40) {
				midTierBits = 20;
				bottomTierBits = 20;
				topLevelBits = bits - midTierBits - bottomTierBits;
			} else if (bits > 20) {
				topLevelBits = 0;
				bottomTierBits = 20;
				midTierBits = bits - bottomTierBits;
			} else {
				topLevelBits = 0;
				midTierBits = 0;
				bottomTierBits = bits;
			}
			
			pageTableSize = 1 << topLevelBits;
			pageTable = new Object[pageTableSize];
			
			for (IMemorySource range : memorySources) {
				
				long baseSlot = range.getBaseAddress() >>> alignment;
				long topSlot = range.getTopAddress() >>> alignment;
				
				for (long i = baseSlot; i <= topSlot; i++) {
					insert(i,range);
				}
			}
		}
		
		private void insert(long slotIndex, IMemorySource range)
		{
			int bottomTierIndex = (int)(slotIndex & ((1 << bottomTierBits) - 1));
			long workingSlotIndex = slotIndex >>> bottomTierBits;
			int midTierIndex = (int)(workingSlotIndex & ((1 << midTierBits) - 1));
			workingSlotIndex >>>= midTierBits;
			int topTierIndex = (int)workingSlotIndex;
			
			Object[] midTier = (Object[]) pageTable[topTierIndex];
			
			if (midTier == null) {
				midTier = new Object[1 << midTierBits];
				pageTable[topTierIndex] = midTier;
			}
			
			IMemorySource[] bottomTier = (IMemorySource[]) midTier[midTierIndex];
			
			if (null == bottomTier) {
				bottomTier = new IMemorySource[1 << bottomTierBits];
				midTier[midTierIndex] = bottomTier;
			}
			
			bottomTier[bottomTierIndex] = range;
		}

		public IMemorySource getRangeForAddress(long address)
		{
			long slotIndex = address >>> alignment;

			int bottomTierIndex = (int)(slotIndex & ((1 << bottomTierBits) - 1));
			long workingSlotIndex = slotIndex >>> bottomTierBits;
			int midTierIndex = (int)(workingSlotIndex & ((1 << midTierBits) - 1));
			workingSlotIndex >>>= midTierBits;
			int topTierIndex = (int)workingSlotIndex;
			
			
			if((topTierIndex < 0) || (topTierIndex >= pageTableSize)) {
				return null;			//if the requested address is out of range then return null
			}
			Object[] midTier = (Object[]) pageTable[topTierIndex];
			if (midTier == null) {
				return null;
			}
			
			IMemorySource[] bottomTier = (IMemorySource[]) midTier[midTierIndex];
			
			if (null == bottomTier) {
				return null;
			}
			
			return bottomTier[bottomTierIndex];
		}
		
	}

}
