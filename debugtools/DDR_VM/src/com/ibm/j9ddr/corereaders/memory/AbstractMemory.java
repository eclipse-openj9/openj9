/*******************************************************************************
 * Copyright (c) 2004, 2014 IBM Corp. and others
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

import java.lang.ref.Reference;
import java.lang.ref.WeakReference;
import java.nio.ByteOrder;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.Collection;
import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.Properties;
import java.util.TreeMap;
import java.util.logging.Logger;

import com.ibm.j9ddr.util.WeakValueMap;

import static java.util.logging.Level.*;


/**
 * Abstract class containing the logic for mapping a memory space
 * onto a set of IMemorySource objects through a caching layer.
 * 
 * @author andhall
 *
 */
public abstract class AbstractMemory extends SearchableMemory implements IMemory 
{
	static final Logger logger = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);
	
	/**
	 * Limit of core file byte cache
	 */
	private static final long MAXIMUM_CORE_FILE_CACHE_BYTES;
	
	private static final long DEFAULT_MAX_CORE_FILE_CACHE_BYTES = 10 * 1024 * 1024;
	
	private static final String MAX_CORE_FILE_CACHE_SIZE_SYSTEM_PROPERTY = "ddr.max.core.data.cache.bytes";
	
	private static final String ENABLE_CACHE_STATS_SYSTEM_PROPERTY = "ddr.track.core.cache.stats";
	
	/**
	 * Size of data block used for caching core data.
	 */
	private static final long CACHE_BLOCK_SIZE = 1024l;
	
	/**
	 * Global boolean for enabling/disabling core file caching
	 */
	private static final boolean GLOBAL_CACHE_ENABLED;
	
	/**
	 * Flag for enabling cache stats to be printed at shutdown
	 */
	static final boolean RECORDING_CACHE_STATS;
	
	/* Cache stats counters */
	private static long cacheHits = 0;
	private static long cacheMisses = 0;
	private static long bytesReadFromDisk = 0;
	private static long bytesReadFromBlockCache = 0;
	private static long purgedBlocks = 0;
	private static long purgedBytes = 0;
	private static long cacheByteHighWaterMark = 0;
	
	
	/* A global list to hold strong references to the blocks used by all CachingMemorySource's
	 * either in their blockMap or singleBlockRef fields.
	 * Updates must be synchronized on the list to prevent multiple threads from interfering with
	 * each other.
	 * It is never read from directly, blocks are retrieved from the blockMap or singleBlockRef.
	 * This list ensures that at least MAXIMUM_CORE_FILE_CACHE_BYTES worth of blocks are kept
	 * alive by strong references and GC doesn't clear all the WeakReferences to blocks in one go.
	 */
	private static final List<CacheBlock> keepAliveList = new ArrayList<CacheBlock>();
	
	/* Current core file cache size
	 * Access should be synchronized on keepAliveList */
	private static long cacheSize = 0;
	
	private final ByteOrder byteOrder;
	
	protected final MemorySourceTable memorySources = new MemorySourceTable();
	
	protected final Map<IMemorySource, IMemorySource> decoratorMappingTable = new TreeMap<IMemorySource, IMemorySource>();
	
	static {
		String maxCoreFileCacheSize = AccessController.doPrivileged(new PrivilegedAction<String>() {

			public String run()
			{
				return System.getProperty(MAX_CORE_FILE_CACHE_SIZE_SYSTEM_PROPERTY);
			}
			
		});

		logger.logp(FINE,"AbstractMemory","<clinit>","System property value from {0} was {1}",new Object[]{MAX_CORE_FILE_CACHE_SIZE_SYSTEM_PROPERTY,maxCoreFileCacheSize});
		
		long size;
		if (maxCoreFileCacheSize != null) {
			size = Long.parseLong(maxCoreFileCacheSize);
			logger.logp(FINE,"AbstractMemory","<clinit>","Max core memory cache size parsed as {0}",size);
		} else {
			size = DEFAULT_MAX_CORE_FILE_CACHE_BYTES;
			logger.logp(FINE,"AbstractMemory","<clinit>","Max core memory cache set to default: {0}",size);
		}
		
		MAXIMUM_CORE_FILE_CACHE_BYTES = size;
		
		if (size <= 0) {
			GLOBAL_CACHE_ENABLED = false;
			logger.logp(FINE,"AbstractMemory","<clinit>","Disabled core memory caching");
		} else {
			GLOBAL_CACHE_ENABLED = true;
		}

		String enableCacheStats = AccessController.doPrivileged(new PrivilegedAction<String>() {

			public String run()
			{
				return System.getProperty(ENABLE_CACHE_STATS_SYSTEM_PROPERTY);
			}
			
		});
		
		if (enableCacheStats != null && enableCacheStats.toLowerCase().equals("true")) {
			Runtime.getRuntime().addShutdownHook(new Thread(new CacheStatsReporter()));
			RECORDING_CACHE_STATS = true;
			logger.logp(FINE,"AbstractMemory","<clinit>","Cache stats enabled");
		} else {
			RECORDING_CACHE_STATS = false;
		}
		
	}
	
	protected AbstractMemory(ByteOrder byteOrder)
	{
		this.byteOrder = byteOrder;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.j9ddr.corereaders.memory.IMemory#getByteAt(long)
	 */
	public byte getByteAt(long address) throws MemoryFault {
		//allocate 1 byte
		byte buffer[] = new byte[1];
		getBytesAt(address, buffer);
		return buffer[0];
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.j9ddr.corereaders.memory.IMemory#getBytesAt(long, byte[])
	 */
	public int getBytesAt(long address, byte[] buffer) throws MemoryFault {
		return getBytesAt(address,buffer,0,buffer.length);
	}
	
	public int getBytesAt(final long address, byte[] buffer, final int offset,final int length) throws MemoryFault {
		IMemorySource range = memorySources.getRangeForAddress(address);
		int read = 0;
		
		if (range == null) {
			throw new MemoryFault(address);
		}
		
		long maxAddress = address + length - 1;
		
		/* If the range spans address ranges, we have to be more careful */
		if (range.contains(maxAddress)) {
			read = range.getBytes(address,buffer,offset,length);
		} else {
			int index = offset;
			long addressPointer = address;
			
			while (Addresses.lessThanOrEqual(addressPointer, maxAddress)) {
				if (! range.contains(addressPointer)) {
					range = memorySources.getRangeForAddress(addressPointer);
					
					if (null == range) {
						break;
					}
				}
				
				long topAddress = range.getTopAddress();
				
				long toRead = Addresses.greaterThan(topAddress,maxAddress) ? maxAddress - addressPointer + 1: topAddress - addressPointer + 1;
				
				int readThisTime = range.getBytes(addressPointer, buffer, index, (int)toRead);
				
				read += readThisTime;
				addressPointer += readThisTime;
				index += readThisTime;
			}
		}
		
		return read;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.j9ddr.corereaders.memory.IMemory#getIntAt(long)
	 */
	public int getIntAt(long address) throws MemoryFault {
		//allocate 4 bytes, do the read, then byte-swap if little endian
		byte buffer[] = new byte[4];
		getBytesAt(address, buffer);
		if (getByteOrder() == ByteOrder.LITTLE_ENDIAN) {
			return ((0xFF & buffer[3]) << 24) | ((0xFF & buffer[2]) << 16) | ((0xFF & buffer[1]) << 8) | (0xFF & buffer[0]);
		} else {
			//by this point, the buffer is in big endian
			return ((0xFF & buffer[0]) << 24) | ((0xFF & buffer[1]) << 16) | ((0xFF & buffer[2]) << 8) | (0xFF & buffer[3]);
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.j9ddr.corereaders.memory.IMemory#getLongAt(long)
	 */
	public long getLongAt(long address) throws MemoryFault {
		//allocate 8 bytes, do the read, then byte-swap if little endian
		byte buffer[] = new byte[8];
		getBytesAt(address, buffer);
		if (getByteOrder() == ByteOrder.LITTLE_ENDIAN) {
			return    (0xFF00000000000000L & (((long) buffer[7]) << 56))
			| (0x00FF000000000000L & (((long) buffer[6]) << 48)) 
			| (0x0000FF0000000000L & (((long) buffer[5]) << 40)) 
			| (0x000000FF00000000L & (((long) buffer[4]) << 32)) 
			| (0x00000000FF000000L & (((long) buffer[3]) << 24)) 
			| (0x0000000000FF0000L & (((long) buffer[2]) << 16)) 
			| (0x000000000000FF00L & (((long) buffer[1]) << 8)) 
			| (0x00000000000000FFL & (buffer[0]));
		} else {
			//by this point, the buffer is in big endian
			return    (0xFF00000000000000L & (((long) buffer[0]) << 56))
			| (0x00FF000000000000L & (((long) buffer[1]) << 48)) 
			| (0x0000FF0000000000L & (((long) buffer[2]) << 40)) 
			| (0x000000FF00000000L & (((long) buffer[3]) << 32)) 
			| (0x00000000FF000000L & (((long) buffer[4]) << 24)) 
			| (0x0000000000FF0000L & (((long) buffer[5]) << 16)) 
			| (0x000000000000FF00L & (((long) buffer[6]) << 8)) 
			| (0x00000000000000FFL & (buffer[7]));
		}
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.j9ddr.corereaders.memory.IMemory#getShortAt(long)
	 */
	public short getShortAt(long address) throws MemoryFault {
		//allocate 2 bytes, do the read, then byte-swap if little endian
		byte buffer[] = new byte[2];
		getBytesAt(address, buffer);
		if (getByteOrder() == ByteOrder.LITTLE_ENDIAN) {
			return (short)(((0xFF & buffer[1]) << 8) | (0xFF & buffer[0]));
		} else {
			return (short)(((0xFF & buffer[0]) << 8) | (0xFF & buffer[1]));
		}
	}
	
	public void addMemorySource(IMemorySource source)
	{
		logger.logp(FINEST,"AbstractMemory","addMemorySource","Added memory range {0}-{1}",new Object[]{Long.toHexString(source.getBaseAddress()),
																										Long.toHexString(source.getTopAddress())
		});
		
		if (GLOBAL_CACHE_ENABLED) {
			IMemorySource wrappedSource = new CachingMemorySource(source);
			decoratorMappingTable.put(source, wrappedSource);
			
			memorySources.addMemorySource(wrappedSource);
		} else {
			if (RECORDING_CACHE_STATS) {
				IMemorySource wrappedSource = new CountingMemorySource(source);
				decoratorMappingTable.put(source, wrappedSource);
				
				memorySources.addMemorySource(wrappedSource);
			} else {
				memorySources.addMemorySource(source);
			}
		}
		
		rangeTable = null;
	}
	
	public void removeMemorySource(IMemorySource source)
	{
		IMemorySource wrappedSource = decoratorMappingTable.remove(source);
		
		if (wrappedSource != null) {
			memorySources.removeMemorySource(wrappedSource);
		} else {
			memorySources.removeMemorySource(source);
		}
		
		rangeTable = null;
	}
	
	public void addMemorySources(Collection<? extends IMemorySource> memorySources) {
		for (IMemorySource range : memorySources) {
			addMemorySource(range);
		}
	}
	
	public List<IMemoryRange> getMemoryRanges() {
		return memorySources.getMemorySources();
	}

	public ByteOrder getByteOrder() {
		return byteOrder;
	}	
	
	public boolean isShared(long address) {
		IMemoryRange match = memorySources.getRangeForAddress(address);
		if (match == null) {
			return false;
		}
		return match.isShared();
	}
	
	public boolean isExecutable(long address) {
		IMemoryRange match = memorySources.getRangeForAddress(address);
		if (match == null) {
			return false;
		}
		return match.isExecutable();
	}
	
	public boolean isReadOnly(long address) {
		IMemoryRange match = memorySources.getRangeForAddress(address);
		if (match == null) {
			return false;
		}
		return match.isReadOnly();
	}
	
	public Properties getProperties(long address) {
		IMemoryRange match = memorySources.getRangeForAddress(address);
		if (match != null && match instanceof IDetailedMemoryRange) {
			return ((IDetailedMemoryRange)match).getProperties();
		}
		return new Properties();
	}
	
	/**
	 * Record used for tracking cached memory bytes.
	 * 
	 * @author andhall
	 *
	 */
	static class CacheBlock
	{
		public CacheBlock(byte[] buffer)
		{
			this.buffer = buffer;
		}
		
		public final byte[] buffer;
	}
	
	/**
	 * Dummy memory range that adds byte caching to the delegate memory range.
	 * @author andhall
	 *
	 */
	private final static class CachingMemorySource extends DelegatingMemorySource
	{
		//For ranges smaller than a block, we have an optimised path to the single cache block
		//that represents this range
		private final boolean singleBlockRange;
		
		private final WeakValueMap<Integer, CacheBlock> blockMap;
		
		private Reference<CacheBlock> singleBlockRef;

		public CachingMemorySource(IMemorySource source)
		{
			super(source);
			
			if (delegate.getSize() <= CACHE_BLOCK_SIZE) {
				singleBlockRange = true;
				blockMap = null;
			} else {
				singleBlockRange = false;
				blockMap = new WeakValueMap<Integer, CacheBlock>();
			}
		}

		public int getBytes(long address, byte[] buffer, int offset, int length)
				throws MemoryFault
		{
			if (singleBlockRange) {
				CacheBlock block;
				boolean cacheHit = false;
				if (singleBlockRef == null || (block = singleBlockRef.get()) == null) {
					if (RECORDING_CACHE_STATS) {
						cacheHit = true;
					}
					block = loadBlock(this.getBaseAddress(),address,(int)this.getSize());
					singleBlockRef = new WeakReference<CacheBlock>(block);
				} else {
					if (RECORDING_CACHE_STATS) {
						cacheHit = false;
					}
				}
				
				System.arraycopy(block.buffer, (int)(address - delegate.getBaseAddress()), buffer, offset, length);
				
				if (RECORDING_CACHE_STATS) {
					if (cacheHit) {
						cacheHits++;
						bytesReadFromBlockCache += length;
					} else {
						cacheMisses++;
					}
				}
				
				return length;
			} else {
				int read = 0;
				int destIndex = offset;
				int toRead;
			
				while ((toRead = length - read) > 0) {
					long rangeOffset = address - this.getBaseAddress();
					int blockIndex = (int)(rangeOffset / CACHE_BLOCK_SIZE);
					long blockBase = delegate.getBaseAddress() + (CACHE_BLOCK_SIZE * ((long)blockIndex));
					long sizeToEndOfRange = delegate.getTopAddress() - blockBase + 1;
					int blockSize = (int)(sizeToEndOfRange > CACHE_BLOCK_SIZE ? CACHE_BLOCK_SIZE : sizeToEndOfRange);
					boolean cacheHit = false;
					
					CacheBlock block = blockMap.get(blockIndex);
					
					if (RECORDING_CACHE_STATS) {
						cacheHit = block != null;
					}
					
					if (block == null) {
						block = loadBlock(blockBase,address,blockSize);
						blockMap.put(blockIndex, block);
					}
					
					long offsetInBlock = address - blockBase;
					long remainingInBlock = blockSize - offsetInBlock;
					long amountToReadInBlock = remainingInBlock > toRead ? toRead : remainingInBlock;
					
					System.arraycopy(block.buffer,(int)offsetInBlock,buffer,destIndex,(int)amountToReadInBlock);
					
					if (RECORDING_CACHE_STATS) {
						if (cacheHit) {
							cacheHits++;
							bytesReadFromBlockCache += amountToReadInBlock;
						} else {
							cacheMisses++;
						}
					}
					
					address += amountToReadInBlock;
					read += amountToReadInBlock;
					destIndex += amountToReadInBlock;
				}
			
				return read;
			}
		}

		private CacheBlock loadBlock(long blockBaseAddress, long actualAddress, int blockSize) throws MemoryFault
		{
			byte[] buffer = new byte[blockSize];
			
			/* Avoid throwing 2 memory faults when accessing unbacked Memory.
			 * Elf core dumps often include large unbacked ranges so this is
			 * significant.
			 */
			if( delegate.isBacked() ) {
				try {
					delegate.getBytes(blockBaseAddress, buffer, 0, blockSize);
				} catch (MemoryFault e) {
					throw new MemoryFault(actualAddress, "MemoryFault loading cache block",e);
				}
			} else {
				throw new MemoryFault(actualAddress, "MemoryFault loading cache block, unbacked memory");
			}
			
			if (RECORDING_CACHE_STATS) {
				bytesReadFromDisk += blockSize;
			}
			
			CacheBlock block = new CacheBlock(buffer);
			
			synchronized (keepAliveList) {
				cacheSize += blockSize;
				
				if (cacheSize > MAXIMUM_CORE_FILE_CACHE_BYTES) {
					trimCache();
				}
				
				if (RECORDING_CACHE_STATS) {
					if (cacheSize > cacheByteHighWaterMark) {
						cacheByteHighWaterMark = cacheSize;
					}
				}
				keepAliveList.add(block);
			}
			
			return block;
		}
		
		/*
		 * Clear the keepAliveList so that GC can collect the items removed from
		 * blockMap. 
		 */
		private static void trimCache()
		{
			synchronized (keepAliveList) {
				// Drop entries from the cache until we're under the cache limit
				// At the moment we just randomly drop things - a caching strategy that's proving hard to beat.
				while (cacheSize > MAXIMUM_CORE_FILE_CACHE_BYTES && keepAliveList.size() > 0) {
					CacheBlock block = keepAliveList.remove(0);
					
					cacheSize -= block.buffer.length;
					
					if (RECORDING_CACHE_STATS) {
						purgedBlocks++;
						purgedBytes += block.buffer.length;
					}
				}
			}
		}
		
		

		@Override
		public int hashCode()
		{
			final int prime = 31;
			int result = super.hashCode();
			result = prime * result
					+ ((blockMap == null) ? 0 : blockMap.hashCode());
			result = prime * result + (singleBlockRange ? 1231 : 1237);
			result = prime
					* result
					+ ((singleBlockRef == null) ? 0 : singleBlockRef.hashCode());
			return result;
		}

		@Override
		public boolean equals(Object obj)
		{
			if (this == obj) {
				return true;
			}
			if (!super.equals(obj)) {
				return false;
			}
			if (!(obj instanceof CachingMemorySource)) {
				return false;
			}
			CachingMemorySource other = (CachingMemorySource) obj;
			if (blockMap == null) {
				if (other.blockMap != null) {
					return false;
				}
			} else if (!blockMap.equals(other.blockMap)) {
				return false;
			}
			if (singleBlockRange != other.singleBlockRange) {
				return false;
			}
			if (singleBlockRef == null) {
				if (other.singleBlockRef != null) {
					return false;
				}
			} else if (!singleBlockRef.equals(other.singleBlockRef)) {
				return false;
			}
			return true;
		}

	}
	
	/*
	 * Dummy memory range used when caching is off, but recording cache states is on. Increments the cache counters
	 * without doing any caching
	 */
	private static class CountingMemorySource extends DelegatingMemorySource
	{
		public CountingMemorySource(IMemorySource source)
		{
			super(source);
		}

		@Override
		public int getBytes(long address, byte[] buffer, int offset, int length)
				throws MemoryFault
		{
			int read = super.getBytes(address, buffer, offset, length);
			
			cacheMisses++;
			bytesReadFromDisk += read;
			
			return read;
		}
	}

	
	private static class CacheStatsReporter implements Runnable
	{

		public void run()
		{
			System.err.println("**DDR Core Reader Cache Stats**");
			System.err.println("Global cache enabled: " + GLOBAL_CACHE_ENABLED);
			System.err.println("Cache hits: " + cacheHits);
			System.err.println("Cache misses: " + cacheMisses);
			double cacheHitRate = ((double)cacheHits / (cacheHits + cacheMisses)) * 100;
			System.err.println("Cache hit rate: " + cacheHitRate);
			System.err.println("Bytes read from disk: " + bytesReadFromDisk);
			System.err.println("Bytes read from cache: " + bytesReadFromBlockCache);
			System.err.println("Purged blocks: " + purgedBlocks);
			System.err.println("Purged bytes: " + purgedBytes);
			System.err.println("Cache bytes high water mark: " + cacheByteHighWaterMark);
			System.err.println("TLB Cache hits: " +  MemorySourceTable.tlbCacheHits);
			System.err.println("TLB Cache misses: " +  MemorySourceTable.tlbCacheMisses);
			double tlbHitRate = ((double)MemorySourceTable.tlbCacheHits / (MemorySourceTable.tlbCacheHits + MemorySourceTable.tlbCacheMisses)) * 100;
			System.err.println("TLB Cache hit rate: " + tlbHitRate);
			
			logger.logp(FINE,"AbstractMemory","CacheStatsReporter","DDR Core Reader Cache Stats");
			logger.logp(FINE,"AbstractMemory","CacheStatsReporter","Global cache enabled: {0}",GLOBAL_CACHE_ENABLED);
			logger.logp(FINE,"AbstractMemory","CacheStatsReporter","Cache hits: {0}, cache misses: {1}, cache hit rate: {2}", new Object[]{cacheHits,cacheMisses, cacheHitRate});
			logger.logp(FINE,"AbstractMemory","CacheStatsReporter","Bytes read from disk: {0}, from cache: {1}, purged blocks: {2}, purged bytes: {3}", new Object[]{bytesReadFromDisk,bytesReadFromBlockCache,purgedBlocks,purgedBytes});
			logger.logp(FINE,"AbstractMemory","CacheStatsReporter","Cache bytes high water mark {0}", new Object[]{cacheByteHighWaterMark});
			logger.logp(FINE,"AbstractMemory","CacheStatsReporter","TLB Cache hits: {0}, misses: {1}, hit rate:{2}",new Object[]{MemorySourceTable.tlbCacheHits,MemorySourceTable.tlbCacheMisses,tlbHitRate});
		}
	}
	

}
