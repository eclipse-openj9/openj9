/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2008, 2019 IBM Corp. and others
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
package com.ibm.dtfj.phd;

import java.io.EOFException;
import java.io.File;
import java.io.IOException;
import java.lang.ref.SoftReference;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.NoSuchElementException;

import javax.imageio.stream.ImageInputStream;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaHeap;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.phd.parser.HeapdumpReader;
import com.ibm.dtfj.phd.parser.PortableHeapDumpListener;
import com.ibm.dtfj.phd.util.LongEnumeration;

/** 
 * @author ajohnson
 */
class PHDJavaHeap implements JavaHeap {
	private static final boolean LOG = false;
	private final ImageAddressSpace space;
	protected final PHDJavaRuntime runtime;
	private final PHDImage image;
	private final File file;
	private final ImageInputStream stream;
	private final boolean isJ9V4;
	// Approximately ten million refs collected at once, limited if memory is short
	private final int STEP = (int)Math.min(10000000, Runtime.getRuntime().maxMemory()/32);
	
	/** A cache of all the chunks */
	private final LinkedHashMap<Integer,CacheHeapSegment>cache = new LinkedHashMap<Integer,CacheHeapSegment>();
	/** Cache the PHD readers to allow resumption of reads */
	private final CachedReader readerCache;
	/** Flag used to show that all the CacheHeapSegments are set up */
	private boolean doneScan;
	private boolean lastSegment;
	
	PHDJavaHeap(ImageInputStream stream, final PHDImage parentImage, ImageAddressSpace space, PHDJavaRuntime runtime) throws IOException {
		this.image = parentImage;
		this.space = space;
		this.runtime = runtime;
		HeapdumpReader reader = new HeapdumpReader(stream, parentImage);
		this.isJ9V4 = reader.version() == 4 && reader.isJ9();
		reader.close();
		this.readerCache = new CachedReader(stream, parentImage);
		this.stream = stream;
		this.file = null;
	}
	
	PHDJavaHeap(File file, final PHDImage parentImage, ImageAddressSpace space, PHDJavaRuntime runtime) throws IOException {
		this.file = file;
		this.image = parentImage;
		this.space = space;
		this.runtime = runtime;
		HeapdumpReader reader = new HeapdumpReader(file, parentImage);
		this.isJ9V4 = reader.version() == 4 && reader.isJ9();
		reader.close();
		this.readerCache = new CachedReader(file, parentImage);
		this.stream = null;
	}

	public String getName() {
		return "Java heap";
	}

	/**
	 * Return all the objects in the heap
	 * Accumulate the objects in several chunks so that not everything has to be active at once
	 * Iterate over each chunk
	 */
	private Iterator<JavaObject> getObjects0() {
		try {
			return new Iterator<JavaObject>() {
				int count[] = new int[1];
				int prev;
				// Initial chunk
				Iterator<JavaObject> it = getObjectsViaCache(STEP, count, false).values().iterator();
				public boolean hasNext() {
					if (it == null) {
						return false;
					} else if (it.hasNext()) {
						return true;
					} else {
						// When we processed the previous chunk did the end user require references from the JavaObjects?
						// If so, then get the references for this chunk
						boolean withRefs = withRefs(prev);
						prev = count[0];
						try {
							it = getObjectsViaCache(STEP, count, withRefs).values().iterator();
						} catch (IOException e) {
							return false;
						}
						if (it.hasNext()) {
							return true;
						} else {
							it = null;
							return false;
						}
					}
				}

				public JavaObject next() {
					if (!hasNext()) throw new NoSuchElementException(); 
					return it.next();
				}

				public void remove() {
					throw new UnsupportedOperationException();
				}
				
			};
		} catch (IOException e) {
			return new ArrayList<JavaObject>().iterator();
		}
	}
	
	/**
	 * Used to index objects from addresses.
	 * Subclasses of this can be a bit smaller than a Long - perhaps 16 bytes, not 24.
	 *
	 */
	static abstract class AddressKey {
		abstract long value(PHDJavaHeap jh);
		/**
		 * Factory to create an AddressKey
		 * @param jh The PHDJavaHeap
		 * @param addr The address to compress
		 * @return
		 */
		static AddressKey getAddress(PHDJavaHeap jh, long addr)
		{
			// It is not safe to not compress on creating of HashMap entry key,
			// but to compress on retrieval as then the key is different and will not match
			if (jh.runtime.noCompress())
				throw new IllegalStateException("Unable to compress addresses");
			int iv = jh.runtime.compressAddress(addr);
			long addr2 = jh.runtime.expandAddress(iv);
			if (addr == addr2)
				return new IntAddressKey(iv);
			else
				return new LongAddressKey(addr);
		}
	}

	private final static class IntAddressKey extends AddressKey {
		final int val;

		private IntAddressKey(int v) {
			val = v;
		}

		long value(PHDJavaHeap jh) {
			return jh.runtime.expandAddress(val);
		}

		public int hashCode() {
			return val;
		}
		
		public boolean equals(Object o) {
			if (this == o) return true;
			return o instanceof IntAddressKey && val == ((IntAddressKey)o).val ;
		}
	}	
		
	
	private final static class LongAddressKey extends AddressKey {
		final long val;

		private LongAddressKey(long v) {
			val = v;
		}

		long value(PHDJavaHeap jh) {
			return val;
		}

		public int hashCode() {
			return (int)val^(int)(val>>>32);
		}
		
		public boolean equals(Object o) {
			if (this == o) return true;
			return o instanceof LongAddressKey && val == ((LongAddressKey)o).val;
		}
	}	
	
	/**
	 * Used to store all objects from part of the heap.
	 * The actual objects can be discarded if we run out of memory, but the range and details remain
	 * so that if we want an object from this segment we can decide to refetch just this segment details,
	 * rather than searching the entire heap.
	 * We can also selectively build objects with or without all outbound references.
	 */
	static class CacheHeapSegment {
		/** The number in the heap of the first object in the chunk */
		final int index;
		/** How many references were accumulated in this chunk. Use the same number when rereading the chunk. */
		final int maxSize;
		/** The number in the heap of the next object after the objects in the chunk. Used to find the next chunk */
		final int nextIndex;
		/** The actual JavaObjects, held via a SoftReference to avoid OutOfMemoryErrors */
		SoftReference<Map<AddressKey,JavaObject>> objects;
		/** Whether the JavaObjects have references available */
		boolean withRefs;
		/** Smallest address - used to find if a JavaObject at a particular address might be in this chunk. */ 
		final long minAddress;
		/** Largest address - used to find if a JavaObject at a particular address might be in this chunk. */
		final long maxAddress;
		/**
		 * Construct the metadata for a chunk.
		 * @param index
		 * @param size
		 * @param nextIndex
		 * @param objs
		 * @param withRefs
		 */
		CacheHeapSegment(int index, int size, int nextIndex, Map<AddressKey,JavaObject> objs, boolean withRefs) {
			objects = new SoftReference<Map<AddressKey,JavaObject>>(objs);
			// Find the maximum and minimum addresses
			long max = Long.MIN_VALUE;
			long min = Long.MAX_VALUE;
			for (JavaObject jo : objs.values()) {
				if (jo instanceof CorruptData) continue;
				long addr = jo.getID().getAddress();
				max = Math.max(max, addr);
				min = Math.min(min, addr);
			}
			this.index = index;
			this.maxSize = size;
			this.nextIndex = nextIndex;
			this.maxAddress = max;
			this.minAddress = min;
			this.withRefs = withRefs;
		}
	}
	
	/**
	 * Used to save PHD readers to continue reading from later on in the file
	 *
	 */
	static class CachedReader {
		File file;
		ImageInputStream stream;
		final PHDImage parentImage;
		class ReaderPos {
			int where;
			HeapdumpReader reader;
			ReaderPos(PHDImage parentImage) throws IOException {
				if(stream == null) {
					reader = new HeapdumpReader(file, parentImage);
				} else {
					reader = new HeapdumpReader(stream, parentImage);
				}
				where = 0;
			}
		}
		List<ReaderPos> readers = new ArrayList<ReaderPos>();
		CachedReader(File f, PHDImage parentImage) {
			file = f;
			this.parentImage = parentImage;
		}
		CachedReader(ImageInputStream stream, PHDImage parentImage) {
			this.stream = stream;
			this.parentImage = parentImage;
		}
		ReaderPos getReader(int n) throws IOException {
			ReaderPos best = null;
			for (ReaderPos rp : readers) {
				if (rp.where <= n && (best == null || best.where < rp.where)) {
					best =rp;
				}
			}
			if (best == null) {	
				best = new ReaderPos(parentImage);
			} else {
				readers.remove(best);
			}
			return best;
		}
		void returnReader(ReaderPos rdr) {
			readers.add(rdr);
		}
	}
	
	/** 
	 * Does the chunk starting at object pos has references for all the JavaObjects?
	 * @param pos
	 * @return
	 */
	private boolean withRefs(int pos) {
		CacheHeapSegment seg = cache.get(pos);
		return seg != null && seg.withRefs;
		
	}
	
	/**
	 * Get a chunk, via a cache, and populate the cache if a new chunk is read.
	 * @param size
	 * @param next
	 * @param withRefs
	 * @return
	 * @throws IOException 
	 */
	Map<AddressKey,JavaObject> getObjectsViaCache(final int size, final int next[], boolean withRefs) throws IOException {
		int index = next[0];
		CacheHeapSegment seg = cache.get(next[0]);
		SoftReference<Map<AddressKey,JavaObject>> sr;
		Map<AddressKey,JavaObject> objects;
		// If no chunk, or the chunk data has been cleared, or if the chunk doesn't have references for the JavaObjects
		// and we need the refs, create the chunk data
		if (seg == null || (sr = seg.objects) == null || (objects = sr.get()) == null || withRefs && !seg.withRefs) {
			objects = getObjects(size, next, withRefs);
			if (seg == null || size != seg.maxSize || next[0] != seg.nextIndex || withRefs != seg.withRefs) {
				// Something changed in the chunk, so redo it
				seg = new CacheHeapSegment(index,size,next[0],objects,withRefs);
				cache.put(index, seg);
			} else {
				// Just replace the soft reference
				sr = new SoftReference<Map<AddressKey,JavaObject>>(objects);
				seg.objects = sr;
			}
		} else {
			// Found in cache
			next[0] = seg.nextIndex;
		}
		if (objects.size() == 0) {
			// No objects were found, so guess the segments are set up
			//lastSegment = true;
			lastSegment = true;
		}
		if (lastSegment && !doneScan) {
			// We have had an end of search
			// Try scanning entire cache to see if it is complete
			CacheHeapSegment seg1 = cache.get(0);
			while (seg1 != null) {
				if (seg1.nextIndex == seg1.index) {
					doneScan = true;
					break;
				}
				seg1 = cache.get(seg1.nextIndex);
			}
		}
		return objects;
	}
	
	/**
	 * Read a chunk of objects (but not the whole heap at once!)
	 * @param maxsize number of refs to collect
	 * @param next Start at object count[0], update count[0] to position after last object in iterator
	 * @return
	 */
	private static final long NOREFS[]={};
	Map<AddressKey,JavaObject> getObjects(final int maxsize, final int next[], final boolean withRefs) throws IOException {
		if (LOG) System.err.println("GetObjects "+next[0]+" "+withRefs);
		final Map<AddressKey,JavaObject> objects = new LinkedHashMap<AddressKey,JavaObject>();
		final PHDJavaHeap heap = this;
		// Size of a reference
		final int REFSCALE = 1;
		// Size of a PHDJavaObject
		final int OBJSCALE = 10;
		//HeapdumpReader reader = new HeapdumpReader(file.getAbsolutePath());
		final CachedReader.ReaderPos rdr = readerCache.getReader(next[0]);
		final int adjustLen = rdr.reader.version() == 4 && rdr.reader.isJ9() ? 1 : 0;
		final long current[] = new long[1];
		boolean more;
		try {
			more = rdr.reader.parse(new PortableHeapDumpListener() {
				int total;
				
				public void classDump(long address, long superAddress, String name, int size,
						int flags, int hashCode, LongEnumeration refs) throws Exception {
				}

				public void objectArrayDump(long address, long classAddress, int flags,
						int hashCode, LongEnumeration refs, int length, long instanceSize) throws Exception {
					current[0] = address;
					if (rdr.where++ >= next[0]) {
						int size = OBJSCALE+refs.numberOfElements() / REFSCALE;
						total += size;
						if (total == size || total < maxsize) {
							int refsLen = refs.numberOfElements();
							int adjustLen2 = Math.min(adjustLen, refsLen);
							// Use adjustLen for array class so for corrupt Java 5 with 0 refs we have no array class
							PHDJavaObject.Builder b = new PHDJavaObject.Builder(heap,address,runtime.arrayOf(classAddress, refs, adjustLen),flags,hashCode);
							PHDJavaObject jo = withRefs 
								? b.refs(refs,adjustLen2).length(length-adjustLen2).instanceSize(instanceSize).build() 
								: b.length(length-adjustLen2).instanceSize(instanceSize).build();
							objects.put(AddressKey.getAddress(PHDJavaHeap.this, address),jo);
							next[0] = rdr.where;
						}
						if (total >= maxsize) rdr.reader.exitParse();
					}
					current[0] = 0;
				}

				public void objectDump(long address, long classAddress, int flags, int hashCode,
						LongEnumeration refs, long instanceSize) throws Exception {
					current[0] = address;
					if (rdr.where++ >= next[0]) {
						int size = OBJSCALE+refs.numberOfElements() / REFSCALE;
						total += size;
						if (total == size || total < maxsize) {
							PHDJavaObject.Builder b = new PHDJavaObject.Builder(heap,address,runtime.findClass(classAddress),flags,hashCode)
								.length(PHDJavaObject.SIMPLE_OBJECT)
								.instanceSize(instanceSize);
							PHDJavaObject jo = withRefs 
								? b.refs(refs, 0).build()
								: b.build();
							objects.put(AddressKey.getAddress(PHDJavaHeap.this, address),jo);
							next[0] = rdr.where;
						}
						if (total >= maxsize) rdr.reader.exitParse();
					}
					current[0] = 0;
				}

				public void primitiveArrayDump(long address, int type, int length, int flags,
						int hashCode, long instanceSize) throws Exception {
					current[0] = address;
					if (rdr.where++ >= next[0]) {
						int size = OBJSCALE;
						total += size;
						if (total == size || total < maxsize) {
							objects.put(AddressKey.getAddress(PHDJavaHeap.this, address),
							new PHDJavaObject.Builder(heap,address,runtime.findArrayOfType(type),flags,hashCode)
							.refsAsArray(NOREFS,0).length(length).instanceSize(instanceSize).build());
							next[0] = rdr.where;
						}
						if (total >= maxsize) rdr.reader.exitParse();
					}
					current[0] = 0;
				}
			});
		} catch (Exception e) {
			// Is this right?
			if (next[0] == 0 || current[0] != 0 || objects.size() > 0) {
				// Only add an exception object the first time it happens
				// Give up if exception occurs between objects
				objects.put(AddressKey.getAddress(PHDJavaHeap.this, current[0]),new PHDCorruptJavaObject("building object", space.getPointer(current[0]), e));
				next[0]++;
				more = true;
			} else {
				more = false;
			}
		}
		if (more) {
			readerCache.returnReader(rdr);
		}
		if (LOG) System.err.println("GotObjects "+next[0]+" "+withRefs);
		return objects;
	}

	public Iterator<ImageSection> getSections() {
		List<ImageSection> c = new ArrayList<ImageSection>();
		// This is the start of the last object
		long last = runtime.maxAddress;
		if (runtime.minAddress <= runtime.maxAddress) {
			ImagePointer objPointer = space.getPointer(last);
			JavaObject lastObj = null;
			try {
				lastObj = getLastObject(objPointer,runtime.maxObjClass,runtime.maxObjLen);
			} catch (IOException e1) {
				// allow to fall through so that jo is null
			}
			try {
				// Find the end of the last object
				if (lastObj != null) last += lastObj.getSize();
				ImageSection s = new PHDImageSection(getName(),space.getPointer(runtime.minAddress),last - runtime.minAddress);
				c.add(s);
			} catch (CorruptDataException e) {
				ImageSection s = new PHDImageSection(getName(),space.getPointer(runtime.minAddress),last - runtime.minAddress);
				c.add(s);
				// The object will take some space
				s = new PHDCorruptImageSection("Corrupt "+getName(),objPointer,8);
				c.add(s);
			}
		}
				
		if (runtime.minClassAddress <= runtime.maxClassAddress) {
			// Space in heap for class objects
			long last2 = runtime.maxClassAddress;
			ImagePointer objPointer = space.getPointer(last2);
			int insert = c.size();
			ImageSection s;
			try {
				JavaClass jc = runtime.findClass(last2);
				// Make sure the class takes up some room
				long size = 8;
				if (jc != null) {
					JavaObject lastObj = jc.getObject();
					if (lastObj != null && lastObj.getID().equals(jc.getID())) {
						size = lastObj.getSize();
					}
				}			
				last2 += size;
			} catch (CorruptDataException e) {
				s = new PHDCorruptImageSection("Corrupt "+getName(),objPointer,8);
				c.add(s);
			}
			// Do the object and class object sections overlap, if so combine them
			if (last2 < runtime.minAddress || last < runtime.minClassAddress) {
				s = new PHDImageSection(getName(),space.getPointer(runtime.minClassAddress),last2 - runtime.minClassAddress);
				c.add(insert, s);
			} else {
				long minAddr = Math.min(runtime.minAddress, runtime.minClassAddress);
				long maxAddr = Math.max(last, last2);
				s = new PHDImageSection(getName(),space.getPointer(minAddr),maxAddr - minAddr);
				// Replace with a combined section
				c.set(0, s);
			}
		}
		return c.iterator();
	}

	ImageAddressSpace getImageAddressSpace() {
		return space;
	}
	
	PHDJavaRuntime getJavaRuntime() {
		return runtime;
	}
	
	JavaObject getCachedObjectAtAddress(ImagePointer address, boolean withRefs) throws IOException {
		for (CacheHeapSegment seg : cache.values()) {
			SoftReference<Map<AddressKey,JavaObject>>sr = seg.objects;
			Map<AddressKey,JavaObject>map = sr.get();
			if (map == null || withRefs && !seg.withRefs && map.get(AddressKey.getAddress(PHDJavaHeap.this, address.getAddress())) != null) {
				long addr = address.getAddress();
				if (seg.minAddress <= addr && addr <= seg.maxAddress) {
					// Possibly here, so refresh the data
					int next[] = new int[]{seg.index};
					map = getObjects(seg.maxSize, next, withRefs);
					seg.withRefs = withRefs;
					seg.objects = sr = new SoftReference<Map<AddressKey,JavaObject>>(map);
				}
			}
			if (map != null) {
				JavaObject jo = map.get(AddressKey.getAddress(PHDJavaHeap.this, address.getAddress()));
				if (jo != null) {
					// Found object
					return jo;
				}
			}
		}
		return null;
	}
	
	JavaObject getObjectAtAddress(ImagePointer address, boolean withRefs) {
		JavaObject jo = null;
		try {
			jo = getCachedObjectAtAddress(address, withRefs);
		} catch (IOException e) {
			// allow to fall through
		}
		if (jo != null) return jo;
		if (!doneScan) {
			try {
				jo = getObjectAtAddress3(address, withRefs);
			} catch (IOException e) {
				// allow to fall through and return null
			}
		}
		return jo;
	}

	/**
	 * Find an object in the heap
	 * Populate each chunk, then search each chunk directly
	 * @throws IOException 
	 */
	private JavaObject getObjectAtAddress3(ImagePointer address, boolean withRefs) throws IOException {
			int count[] = new int[1];
			do {
				Map<AddressKey, JavaObject> map = getObjectsViaCache(STEP, count, withRefs);
				if (map.isEmpty()) break;
				JavaObject jo = map.get(AddressKey.getAddress(this, address.getAddress()));
				if (jo != null) {
					// Found object
					return jo;
				}
			} while (true);
			return null;
	}

	/**
	 * Attempt to get the last object without rereading the entire heap.
	 * @param address
	 * @param classAddress
	 * @param length
	 * @return a JavaObject with sufficient information to determine its length
	 * @throws IOException 
	 */
	JavaObject getLastObject(ImagePointer address, long classAddress, int length) throws IOException {
		JavaObject jo = getCachedObjectAtAddress(address, false);
		if (jo != null) return jo;
		long addr = address.getAddress();
		if (classAddress < 8) {
			JavaClass jc = runtime.findArrayOfType((int)classAddress);
			jo = new PHDJavaObject.Builder(this,addr,jc,PHDJavaObject.NO_HASHCODE,-1).length(length).refsAsArray(NOREFS,0).build();
		} else if (length >= 0) {
			if (isJ9V4) length--;
			JavaClass jc = runtime.findArrayOfClass(classAddress);
			jo = new PHDJavaObject.Builder(this,addr,jc,PHDJavaObject.NO_HASHCODE,-1).length(length).build();
		} else {
			JavaClass jc = runtime.findClass(classAddress);
			jo = new PHDJavaObject.Builder(this,addr,jc,PHDJavaObject.NO_HASHCODE,-1).length(PHDJavaObject.SIMPLE_OBJECT).build();			
		}
		return jo;
	}
	
	/**
	 * Return all the objects in the heap
	 * This uses a modified version of the HeapdumpReader which allows abort and resume.
	 */
	public Iterator<JavaObject> getObjects() {
		final PHDJavaHeap heap = this;
		try {
			return new Iterator<JavaObject>() {
				HeapdumpReader reader = null;
				
				{
					if(stream == null) {
						reader = new HeapdumpReader(file, image);
					} else {
						reader = new HeapdumpReader(stream, image);
					}
				}
				
				final int adjustLen = reader.version() == 4 && reader.isJ9() ? 1 : 0;
				final long current[] = new long[1];
				static final boolean withRefs = true;
				JavaObject jo;
				int count = 0;
				PortableHeapDumpListener listen = new PortableHeapDumpListener() {

					public void classDump(long address, long superAddress, String name, int size,
							int flags, int hashCode, LongEnumeration refs) throws Exception {
					}

					public void objectArrayDump(long address, long classAddress, int flags,
							int hashCode, LongEnumeration refs, int length, long instanceSize) throws Exception {
						current[0] = address;
						int refsLen = refs.numberOfElements();
						int adjustLen2 = Math.min(adjustLen, refsLen);
						// Use adjustLen for array class so for corrupt Java 5 with 0 refs we have no array class
						PHDJavaObject.Builder b = new PHDJavaObject.Builder(heap,address,runtime.arrayOf(classAddress, refs, adjustLen),flags,hashCode)
						.instanceSize(instanceSize);
						jo = withRefs 
							? b.refs(refs,adjustLen2).length(length-adjustLen2).build()
							: b.length(length-adjustLen2).build();
						current[0] = 0;
						reader.exitParse();
					}

					public void objectDump(long address, long classAddress, int flags, int hashCode,
							LongEnumeration refs, long instanceSize) throws Exception {
						current[0] = address;
						PHDJavaObject.Builder b = new PHDJavaObject.Builder(heap,address,runtime.findClass(classAddress),flags,hashCode)
						.length(PHDJavaObject.SIMPLE_OBJECT).instanceSize(instanceSize);
						jo = withRefs 
							? b.refs(refs, 0).build()
							: b.build();
						current[0] = 0;
						reader.exitParse();
					}

					public void primitiveArrayDump(long address, int type, int length, int flags,
							int hashCode, long instanceSize) throws Exception {
						current[0] = address;
						jo = new PHDJavaObject.Builder(heap,address,runtime.findArrayOfType(type),flags,hashCode)
						.refsAsArray(NOREFS,0).length(length).instanceSize(instanceSize).build();
						current[0] = 0;
						reader.exitParse();
					}

				};
				
				public boolean hasNext() {
					if (jo == null) getNext();
					return jo != null;
				}

				public JavaObject next() {
					if (!hasNext()) throw new NoSuchElementException();
					JavaObject ret = jo;
					jo = null;
					++count;
					return ret;
				}
				
				private void getNext() {
					try {
						// Note that the listener forces the parser to end after one object
						// but presumes the reader can restart parsing where it left off.
						if (reader != null && !reader.parse(listen)) {
							reader.close();
							reader = null;
						}
					} catch (EOFException e) {
						jo = new PHDCorruptJavaObject("Truncated dump found while building object "+count+"/"+reader.totalObjects(), space.getPointer(0), e);
						reader.close();
						reader = null;
					} catch (IOException e) {
						jo = new PHDCorruptJavaObject("Corrupted dump found while building object "+count+"/"+reader.totalObjects(), space.getPointer(0), e);
						reader.close();
						reader = null;
					} catch (Exception e) {
						// Is this right?
						if (current[0] != 0 ) {
							// Only add an exception object the first time it happens
							// Give up if exception occurs between objects
							jo = new PHDCorruptJavaObject("Building object "+count+"/"+reader.totalObjects(), space.getPointer(current[0]), e);
						} else {
							jo = new PHDCorruptJavaObject("Building object "+count+"/"+reader.totalObjects(), space.getPointer(0), e);
							reader.close();
							reader = null;
						}
					}
				}

				public void remove() {
					throw new UnsupportedOperationException();
				}
				
				protected void finalize() throws Throwable {
					// shouldn't normally be necessary as the reader is closed in getNext() in most cases, 
					// but the client doesn't have to run the iterator to the end, so we're accounting for that here
					if (reader != null) {
						reader.close();
					}
				}
			};
		} catch (IOException e) {
			return new ArrayList<JavaObject>().iterator();
		}
	}
}
