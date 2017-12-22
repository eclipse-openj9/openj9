/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.view.dtfj;

import static com.ibm.j9ddr.logging.LoggerNames.LOGGER_VIEW_DTFJ;
import static com.ibm.j9ddr.vm29.j9.J9VMThreadPointerUtil.getJ9State;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImagePointer;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageProcess;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageSection;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.J9VMThreadPointerUtil.ThreadInfo;
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JITDataCacheHeaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JITExceptionTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MemorySegmentListPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MemorySegmentPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.structure.J9JITDataCacheHeader;
import com.ibm.j9ddr.vm29.types.UDATA;
import com.ibm.j9ddr.vm29.view.dtfj.java.DTFJJavaRuntime;

/**
 * Represents the current context under which DTFJ is being used. This will identify
 * the AddressSpace, Process and RunTime under which operations are being carried out
 * 
 * @author Adam Pilkington
 *
 */
public class DTFJContext {
	
	// TODO: Move these constants to real home
	/* Constants from J9JITDataCacheConstants */
	public static final long J9DataTypeExceptionInfo = 0x1;
	public static final long  J9DataTypeHashTable = 0x20;
	public static final long  J9DataTypeRelocationData = 0x4;
	public static final long  J9DataTypeStackAtlas = 0x2;
	public static final long  J9DataTypeThunkMappingData = 0x10;
	public static final long  J9DataTypeThunkMappingList = 0x8;
	
	private final static IProcess process;
	private final static J9JavaVMPointer vm;
	private static List<ThreadInfo> threadInfoCache = null;
	private static DTFJJavaRuntime runtime;
	private static J9DDRImageProcess imageProcess;
	private static Map<J9MethodPointer, List<J9JITExceptionTablePointer>> jitMethodCache = null;
	
	static {
		process = AbstractPointer.getProcess();
		J9JavaVMPointer temp = null;
		try {
			temp = J9RASHelper.getVM(DataType.getJ9RASPointer());
		} catch (CorruptDataException e) {
			getLogger().log(Level.FINE, "", e);
		}
		vm = temp;
	}
	
	public static IProcess getProcess() {
		return process;
	}
	
	public static J9JavaVMPointer getVm() {
		return vm;
	}
	
	public static J9DDRImagePointer getImagePointer(long address) {
		return new J9DDRImagePointer(process, address);
	}
	
	public static J9DDRImageSection getImageSection(long address, String name) {
		return new J9DDRImageSection(process, address, name);
	}
	
	/**
	 * Convenience method to ensure that all DTFJ components log in the same way
	 * @return
	 */
	public static Logger getLogger() {
		return Logger.getLogger(LOGGER_VIEW_DTFJ);
	}
	
	public static List<ThreadInfo> getThreadInfoCache() throws CorruptDataException {
		if(threadInfoCache == null) {
			List<ThreadInfo> localThreadInfoCache = new ArrayList<ThreadInfo>();
			J9VMThreadPointer vmThread = DTFJContext.getVm().mainThread();
			J9VMThreadPointer firstThread = vmThread;
			do {
				ThreadInfo info = getJ9State(vmThread);
				localThreadInfoCache.add(info);
				vmThread = vmThread.linkNext();
			} while(!vmThread.eq(firstThread));
			
			threadInfoCache = localThreadInfoCache;
		}
		return threadInfoCache;
	}

	public static void setRuntime(DTFJJavaRuntime r)
	{
		runtime = r;
	}
	
	public static DTFJJavaRuntime getRuntime()
	{
		return runtime;
	}
	
	public static List<J9JITExceptionTablePointer> getJITMetaData(J9MethodPointer j9ramMethod) 
	{
		if (jitMethodCache == null) {
			cacheJITMethodAddresses();
		}
		
		return jitMethodCache.get(j9ramMethod);
	}
	
	private static void cacheJITMethodAddresses() {
		jitMethodCache = new HashMap<J9MethodPointer, List<J9JITExceptionTablePointer>>();
		
		try {
			J9MemorySegmentListPointer dataCacheList = getVm().jitConfig().dataCacheList();
			J9MemorySegmentPointer dataCache = dataCacheList.nextSegment();
			
			
			while (dataCache.notNull()) {
				UDATA current = UDATA.cast(dataCache.heapBase());
				UDATA end = UDATA.cast(dataCache.heapAlloc());
				
				while(current.lt(end)) {
					J9JITDataCacheHeaderPointer hdr = J9JITDataCacheHeaderPointer.cast(current);
					
					if (hdr.type().longValue() == J9DataTypeExceptionInfo) {
						J9JITExceptionTablePointer metaData = J9JITExceptionTablePointer.cast(current.add(J9JITDataCacheHeader.SIZEOF));
						addMetaData(metaData);
					}
					
					current = current.add(hdr.size());
				}
				dataCache = dataCache.nextSegment();
			}
		} catch (CorruptDataException e) {
			return;
		}
	}

	private static void addMetaData(J9JITExceptionTablePointer metaData) throws CorruptDataException {
		if (metaData.constantPool().isNull()) {
			return;
		}
		
		List<J9JITExceptionTablePointer> bucket = jitMethodCache.get(metaData.ramMethod());
		if (bucket == null) {
			bucket = new ArrayList<J9JITExceptionTablePointer>();
			jitMethodCache.put(metaData.ramMethod(), bucket);
		}
		bucket.add(metaData);
	}

	public static void setImageProcess(J9DDRImageProcess process)
	{
		imageProcess = process;
	}
	
	public static J9DDRImageProcess getImageProcess()
	{
		return imageProcess;
	}
}
