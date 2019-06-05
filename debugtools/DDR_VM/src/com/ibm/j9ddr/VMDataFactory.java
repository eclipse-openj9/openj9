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

package com.ibm.j9ddr;

import static java.util.logging.Level.FINE;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.io.UnsupportedEncodingException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.ArrayList;
import java.util.List;
import java.util.Properties;
import java.util.logging.Logger;

import javax.imageio.stream.FileImageInputStream;
import javax.imageio.stream.ImageInputStream;
import javax.imageio.stream.ImageInputStreamImpl;
import javax.imageio.stream.MemoryCacheImageInputStream;

import com.ibm.j9ddr.StructureHeader.BlobID;
import com.ibm.j9ddr.StructureReader.PackageNameType;
import com.ibm.j9ddr.corereaders.ICore;
import com.ibm.j9ddr.corereaders.Platform;
import com.ibm.j9ddr.corereaders.memory.IMemoryImageInputStream;
import com.ibm.j9ddr.corereaders.memory.IModule;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.corereaders.memory.ISymbol;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.exceptions.CorruptStructuresException;
import com.ibm.j9ddr.exceptions.JVMNotDDREnabledException;
import com.ibm.j9ddr.exceptions.JVMNotFoundException;
import com.ibm.j9ddr.exceptions.MissingDDRStructuresException;
import com.ibm.j9ddr.exceptions.UnknownArchitectureException;
import com.ibm.j9ddr.logging.LoggerNames;
import com.ibm.j9ddr.util.WeakValueMap;
import com.ibm.j9ddr.blobs.BlobFactory;
import com.ibm.j9ddr.blobs.IBlobFactory;
import com.ibm.j9ddr.blobs.IBlobFactory.Platforms;

/**
 * Create IVMData instances for each VM found in a Process
 */

public abstract class VMDataFactory {
	private final static String STRUCTUREFILE_PROPERTY = "com.ibm.j9ddr.structurefile";
	private final static String SEARCHEYECATCHER_PROPERTY = "com.ibm.j9ddr.searcheyecatcher";
	private final static String NOEXTRASEARCHFORNODE_PROPERTY = "com.ibm.j9ddr.noextrasearchfornode";

	/* Symbol used to find the J9RAS structure on z/OS */
	public static final String J9RAS_SYMBOL = "_j9ras_";
	private static final byte[] eyecatcher;
	private static final long integrityCheck = 0xaa55aa55aa55aa55L;
	private static final int BIT_PATTERNS_OFFSET = 0x8;
	private static final int J9RAS_VERSION_OFFSET = 0x10;
	private static final int DDR_DATA_POINTER_OFFSET = 0x18;
	private static final int MINIMUM_J9RAS_MAJOR_VERSION = 2;
	private static long j9RASAddress;

	//a process can have more than one blob present, each blob is represented as a separate IVMData
	private static WeakValueMap<IProcess, List<IVMData>> vmDataCache = new WeakValueMap<IProcess, List<IVMData>>();
	
	static {
		try {
			eyecatcher = "J9VMRAS".getBytes("ASCII");
		} catch (UnsupportedEncodingException e) {
			// This shouldn't happen
			throw new Error(e);
		}
	}

	/**
	 * Returns a VMData for the first VM located in the process
	 * @param process
	 * @return
	 * @throws IOException
	 */
	public static IVMData getVMData(IProcess process) throws IOException {
		
		List<IVMData> cachedVMData = vmDataCache.get(process);

		if ((cachedVMData != null) && (cachedVMData.size() > 0)) {
			return cachedVMData.get(0);		//return the first VM found
		}

		cachedVMData = getAllVMData(process);
		
		if(cachedVMData.size() > 0) {
			return cachedVMData.get(0);		//return the first VM found
		} else {
			return null;					//nothing was found in scan
		}		

	}
	//TODO - fix this for z/OS which will require noting which RAS symbols have already been found in the core
	
	
	/**
	 * Finds all of the blobs in a given process and wraps them in a IVMData structure.
	 * 
	 * @param process process to scan
	 * @return all located blobs
	 * @throws IOException re-throws IOExceptions
	 */
	public synchronized static List<IVMData> getAllVMData(IProcess process) throws IOException {
		List<IVMData> cachedVMData = vmDataCache.get(process);

		if (cachedVMData != null) {
			return cachedVMData;
		}

		// nothing in the cache for this process, so need to scan
		// Get an ImageInputStream on the Structure Offset Data.  This may or may not be in the core file itself.
		List<IVMData> data = new ArrayList<IVMData>();
		ImageInputStream in = null;
		boolean EOM = false;	//End Of Memory
		long address = 0;
		j9RASAddress= 0;
		while(!EOM) {
			try {
				address = j9RASAddress + 1;
				in = getStructureDataFile(process, address);
				if(in != null) {
					EOM = !(in instanceof IMemoryImageInputStream);
					IVMData vmdata = getVMData(process, in);
					data.add(vmdata);
					if(vmdata.getClassLoader().getHeader().getCoreVersion() == 1) {
						//version 1 does not support multiple blobs
						EOM = true;
						break;
					}
				} else {
					EOM = true;
				}
			} catch (JVMNotFoundException e) {
				//no more JVMs were found
				EOM = true;
			} catch (JVMNotDDREnabledException e) {
				//an older JVM was found, so ignore that and carry on looking
				EOM = EOM | (process.getPlatform() == Platform.ZOS);	//on z/OS a failure with the j9ras symbol resolution aborts the scan
				continue;				
			} catch (MissingDDRStructuresException e) {
				//cannot process as the structures are missing
				EOM = EOM | (process.getPlatform() == Platform.ZOS);	//on z/OS a failure with the j9ras symbol resolution aborts the scan
				continue;
			} catch (CorruptStructuresException e) {
				//cannot process as the structures are corrupt and cannot be read
				EOM = EOM | (process.getPlatform() == Platform.ZOS);	//on z/OS a failure with the j9ras symbol resolution aborts the scan
				continue;
			} catch (IOException e) {
				continue;
			}
		}		
		
		// For Node.JS only, if we have not found a BLOb at this point we do a more expensive scan. This extra
		// scan is switched off for Java-only applications (specifically jdmpview) via a system property.
		if ((System.getProperty(NOEXTRASEARCHFORNODE_PROPERTY) == null) && (data.size() == 0)) {
			StructureHeader header = null;
			try {
				header = findNodeVersion(process);
			} catch (Exception e) {
				if(e instanceof IOException) {
					throw (IOException)e;
				} else {
					IOException ioe = new IOException();
					ioe.initCause(e);		//avoid use of IOException(throwable) to be compatible with J5
					throw ioe;
				}
			}
			if(header != null) {
				in = getBlobFromLibrary(process, header);
				if(in != null) {
					IVMData vmdata = getVMData(process, in);
					data.add(vmdata);
				}
			}
		}
		vmDataCache.put(process, data);
		return data;
		}

	private static IVMData getVMData(IProcess process, ImageInputStream in) throws IOException {
		// Create and initialize a StructureClassLoader for this VM instance
		final StructureReader structureReader = new StructureReader(in);

		J9DDRClassLoader ddrClassLoader = AccessController.doPrivileged(new PrivilegedAction<J9DDRClassLoader>() {

			public J9DDRClassLoader run()
			{
				return new J9DDRClassLoader(structureReader, VMDataFactory.class.getClassLoader());
			}});

		try {
			// Load and instantiate a VMData object.  Load the VMData class for each core file
			IVMData vmData = ddrClassLoader.getIVMData(process, j9RASAddress);

			// Load and initialize the statics on DataType for this core file.
			Class<?> dataTypeClazz = ddrClassLoader.loadClassRelativeToStream("j9.DataType", false);
			Method initMethod = dataTypeClazz.getDeclaredMethod("init", IProcess.class, StructureReader.class);
			initMethod.invoke(null, process, structureReader);

			// Load an instantiate a VM specific J9RASPointer via a call to J9RASPointer.cast(long);
			String basePackageName = structureReader.getPackageName(PackageNameType.POINTER_PACKAGE_DOT_NAME);
			Class<?> rasClazz = ddrClassLoader.loadClass(basePackageName + ".J9RASPointer");
			Method getStructureMethod = rasClazz.getDeclaredMethod("cast", new Class[] { Long.TYPE });
			Object pointer = getStructureMethod.invoke(null, new Object[] { j9RASAddress });

			// Set the J9RASPointer on DataType.
			Method setMethod = dataTypeClazz.getDeclaredMethod("setJ9RASPointer", pointer.getClass());
			setMethod.invoke(vmData, new Object[] { pointer });

			//now add any blob fragments for this build
			addFragments(rasClazz, pointer, structureReader, vmData);
			
			if( !Boolean.FALSE.toString().equals(System.getProperty("com.ibm.j9ddr.symbols.from.pointers")) ) {
				DDRSymbolFinder.addSymbols(process, j9RASAddress, structureReader);
			}
			
			return vmData;
		} catch (ClassNotFoundException e) {
			Logger logger = Logger.getLogger(LoggerNames.LOGGER_STRUCTURE_READER);
			logger.log(FINE, null, e);
			throw new IOException(String.format("Invalid or unavailable structure offset data.  %s", e.getMessage()));
		} catch (NoSuchMethodException e) {
			Logger logger = Logger.getLogger(LoggerNames.LOGGER_STRUCTURE_READER);
			logger.log(FINE, null, e);
			throw new IOException(String.format("Invalid or unavailable structure offset data.  %s", e.getMessage()));
		} catch (IllegalAccessException e) {
			Logger logger = Logger.getLogger(LoggerNames.LOGGER_STRUCTURE_READER);
			logger.log(FINE, null, e);
			throw new IOException(String.format("Invalid or unavailable structure offset data.  %s", e.getMessage()));
		} catch (InvocationTargetException e) {
			Logger logger = Logger.getLogger(LoggerNames.LOGGER_STRUCTURE_READER);
			logger.log(FINE, null, e);
			throw new IOException(String.format("Invalid or unavailable structure offset data.  %s", e.getMessage()));
		} catch (InstantiationException e) {
			Logger logger = Logger.getLogger(LoggerNames.LOGGER_STRUCTURE_READER);
			logger.log(FINE, null, e);
			throw new IOException(String.format("Invalid or unavailable structure offset data.  %s", e.getMessage()));
		}
	}

	/**
	 * Add any blob fragments which apply to this build
	 * 
	 * @param rasptr pointer to the RAS structure so that we can get the buildID
	 */
	private static void addFragments(Class<?> rasClazz, Object rasptr, final StructureReader structureReader, IVMData vmdata) {
		try {
			InputStream in = rasClazz.getResourceAsStream("/fragments/fragments.properties");
			if(in == null) {
				Logger logger = Logger.getLogger(LoggerNames.LOGGER_STRUCTURE_READER);
				logger.log(FINE, "Failed to find fragments property file");
				return;		//no property file found
			}
			Properties fragments = new Properties();
			fragments.load(in);
			in.close();
			Method method = rasClazz.getDeclaredMethod("buildID", (Class<?>[]) null);
			Object u64 = method.invoke(rasptr, (Object[]) null);
			method = u64.getClass().getMethod("getHexValue", (Class<?>[]) null);
			Object result = method.invoke(u64, (Object[]) null);
			String buildID = result.toString();
			//the buildID field consists of a 4 byte platform ID followed by a 4 byte Axxon build ID
			if(buildID.length() < 8) {
				return;		//build ID hex string is not long enough so can't match any fragments
			}
			//4 bytes = 8 hex chars
			String shortID = buildID.substring(buildID.length() - 8);
			String key = vmdata.getVersion() + "-" + buildID;		//default is nothing to match
			if(hasFragmentBeenLoaded(rasClazz, fragments, structureReader, key)) {
				return;		//loaded a fragment matched by it's full build ID
			}
			key = vmdata.getVersion() + "-" + shortID;
			if(hasFragmentBeenLoaded(rasClazz, fragments, structureReader, key)) {
				return;		//loaded a fragment matched by it's short ID
			}
			
		} catch (Exception e) {
			Logger logger = Logger.getLogger(LoggerNames.LOGGER_STRUCTURE_READER);
			logger.log(FINE, "Failed to process blob fragments", e);
		}
	}
	
	private static boolean hasFragmentBeenLoaded(Class<?> rasClazz, Properties fragments, StructureReader reader, String key) throws IOException {
		if(fragments.containsKey(key)) {
			InputStream in = rasClazz.getResourceAsStream("/fragments/" + fragments.getProperty(key));
			ImageInputStream iis = new MemoryCacheImageInputStream(in);
			reader.addStructures(iis);
			iis.close();
			return true;
		} else {
			return false;
		}
	}
	
	private static ImageInputStream getStructureDataFile(IProcess process, long start) throws IOException {
		try {
			if (process.getPlatform() == Platform.ZOS) {
				return getStructureDataFileFromSymbol(process);
			} else {
				return getStructureDataFileFromRASEyecatcher(process, start);
			}
		} catch (JVMNotFoundException e) {
			String structureFileName = System.getProperty(STRUCTUREFILE_PROPERTY);
			if (structureFileName != null) {
				// first try to find value specified as a file, if that doesn't work look
				// for it in the blob archive
				try {
					return getStructureDataFromFile(structureFileName, process);
				} catch(Exception e1) {
					IOException ioe = new IOException();
					ioe.initCause(e);
					throw ioe;
				}
			} else if (process.getPlatform() == Platform.ZOS) {
				try {
					return getStructureDataFileFromSymbol(process);
				} catch (JVMNotFoundException e1) {
					// Check for system property to force searching for JVMs via the eyecatcher
					if (System.getProperty(SEARCHEYECATCHER_PROPERTY) != null) {
						return getStructureDataFileFromRASEyecatcher(process, start);
					} else {
						throw e1;		//no system property set so override
					}
				}
			} else {
				throw e;
			}
		}
	}

	private static ImageInputStream getStructureDataFileFromRASEyecatcher(IProcess process, long start) throws IOException {
		try {
			long address = process.findPattern(eyecatcher, 1, start);
			while (address != -1) {

				long bitPattern = process.getLongAt(address + BIT_PATTERNS_OFFSET);
				if (bitPattern == integrityCheck) {
					return foundRAS(process, address);
				}
				address = process.findPattern(eyecatcher, 1, address + eyecatcher.length);
			}

			// Can't find RAS structure, bail out
			throw new JVMNotFoundException(process, "Could not find J9RAS structure. No Java in process?");
		} catch (MemoryFault e) {
			// put the stack trace to the log
			Logger logger = Logger.getLogger(LoggerNames.LOGGER_STRUCTURE_READER);
			StringWriter sw = new StringWriter();
			PrintWriter pw = new PrintWriter(sw);
			e.printStackTrace(pw);
			logger.logp(FINE, null, null, sw.toString());
			throw new IOException(e.getMessage());
		}
	}

	//On z/OS multiple VMs could inhabit the same address space - so we can't just search the A/S for a pattern (we can't
	// tell which enclave (process) it belongs to.
	// On z/OS we find the RAS structure by looking for the _j9ras_ symbol
	private static ImageInputStream getStructureDataFileFromSymbol(IProcess process) throws IOException {
		try {
			for (IModule thisModule : process.getModules()) {
				for (ISymbol thisSymbol : thisModule.getSymbols()) {
					if (thisSymbol.getName().equals(J9RAS_SYMBOL)) {
						return foundRAS(process, thisSymbol.getAddress());
					}
				}
			}
		} catch (CorruptDataException e) {
			throw new IOException(e.getMessage());
		} catch (DataUnavailableException e) {
			throw new IOException(e.getMessage());
		}
		throw new JVMNotFoundException(process, "Could not find _j9ras_ symbol in process");
	}

	private static ImageInputStream getStructureDataFromFile(String fileName, IProcess addressSpace) throws IOException {
		File blobFile = new File(fileName);
		ImageInputStream iis = new FileImageInputStream(blobFile);

		iis.setByteOrder(addressSpace.getByteOrder());
		return iis;
	}

	private static ImageInputStream foundRAS(IProcess addressSpace, long candidateAddress) throws IOException {
		try {
			j9RASAddress = candidateAddress;

			String structureFileName = System.getProperty(STRUCTUREFILE_PROPERTY);
			if (structureFileName != null) {
				// first try to find value specified as a file, if that doesn't work look
				// for it in the blob archive
				try {
					return getStructureDataFromFile(structureFileName, addressSpace);
				} catch (FileNotFoundException e) {
					return getBlobFromArchive(structureFileName, addressSpace);
				}
			}

			int j9RASVersion = addressSpace.getIntAt(candidateAddress + J9RAS_VERSION_OFFSET);
			short j9RASMajorVersion = (short) (j9RASVersion >> 16);

			if (j9RASMajorVersion < MINIMUM_J9RAS_MAJOR_VERSION) {
				return locateInServiceVMStructure(addressSpace);
			}
			
			long ddrDataStart = addressSpace.getPointerAt(candidateAddress + DDR_DATA_POINTER_OFFSET);
			if (0 == ddrDataStart) {
				// CMVC 172446 : no valid address to DDR blob, so see if we can locate it via the blob archive
				try {
					return locateInServiceVMStructure(addressSpace);
				} catch (IOException e) {
					//failed to locate a blob
					MissingDDRStructuresException ioe = new MissingDDRStructuresException(addressSpace,
							"System dump was generated by a DDR-enabled JVM, but did not contain embedded DDR structures. This dump cannot be analyzed by DDR. " +
							"You can specify the location of a DDR structure file to use with the " + STRUCTUREFILE_PROPERTY + " system property");
					ioe.initCause(e);
					throw ioe;
				}
				
			}
			//the address may be a marker to treat it in a special way, rather than as an actual address
			//-1 = the blob is located directly after this structure
			//-2 = there is only a blob descriptor loaded
			long marker = (addressSpace.bytesPerPointer() == 4) ? 0xFFFFFFFF00000000L | ddrDataStart : ddrDataStart;
			if(marker == -2){
				StructureHeader header = new StructureHeader((byte)1);
				ddrDataStart = candidateAddress + DDR_DATA_POINTER_OFFSET + (addressSpace.bytesPerPointer() * 2);
				ImageInputStream stream = new IMemoryImageInputStream(addressSpace, ddrDataStart);
				header.readBlobVersion(stream);
				return getBlobFromLibrary(addressSpace, header);
			}
			if(marker == -1){
				if(j9RASVersion == 0x100000) {
					ddrDataStart = candidateAddress + DDR_DATA_POINTER_OFFSET + (addressSpace.bytesPerPointer() * 2);		//there is one pointer in the way that needs to be skipped over
				} else {
					MissingDDRStructuresException ioe = new MissingDDRStructuresException(addressSpace,
							"System dump was generated by a DDR-enabled JVM, but did not contain embedded DDR structures. This dump cannot be analyzed by DDR. " +
							"You can specify the location of a DDR structure file to use with the " + STRUCTUREFILE_PROPERTY + " system property");
						throw ioe; 
				}
			}

			return new IMemoryImageInputStream(addressSpace, ddrDataStart);
		} catch (MemoryFault e) {
			// put the stack trace to the log
			Logger logger = Logger.getLogger(LoggerNames.LOGGER_STRUCTURE_READER);
			StringWriter sw = new StringWriter();
			PrintWriter pw = new PrintWriter(sw);
			e.printStackTrace(pw);
			logger.logp(FINE, null, null, sw.toString());
			throw new IOException(e.getMessage());
		}
	}

	// Locate InServiceStructure data for core with old J9RAS
	private static ImageInputStream locateInServiceVMStructure(IProcess process) throws IOException {
		// we find the platform/architecture from the process, and look for J9VM build id
		// in process. Appropriate IOException thrown if any problem here.
		String path = getBlobBasedirInArchive(process);
		String j9vmid = getJ9VMBuildInCore(process);

		try {
			return getBlobFromArchive(path + j9vmid, process);
		} catch (IOException e) {
			// we can't find blob file for this build, try to give user enough info to make manual override
			// using (colon separated) blob index.
			InputStream indexIS = VMDataFactory.class.getResourceAsStream("/ddr.structurefiles.index");
			if (indexIS == null) {
				throw new JVMNotDDREnabledException(process,"DDR could not find VM structure data archive index. J9VM build ID: " + j9vmid
						+ ". Platform: " + path + ". You can specify a structure file to use manually with the "
						+ STRUCTUREFILE_PROPERTY + " system property.");
			}

			BufferedReader reader = new BufferedReader(new InputStreamReader(indexIS));
			String allBlobs = reader.readLine();
			String[] blobs = allBlobs.split(":");
			StringBuffer candidates = new StringBuffer();
			for (int i = 0; i < blobs.length; i++) {
				if (blobs[i].startsWith(path)) {
					if (candidates.length() != 0) {
						candidates.append(", ");
					}
					candidates.append(blobs[i]);
				}
			}

			throw new JVMNotDDREnabledException(process,"DDR could not find VM structure data file. J9VM build ID: "	+ j9vmid
					+ ". Platform: " + path
					+ ". You can specify a structure file to use manually with the " + STRUCTUREFILE_PROPERTY
					+ " system property." + (candidates.length() > 0 ? " Possible structure file matches: "
							+ candidates.toString() + "." : ""));
		}
	}

	private static ImageInputStream getBlobFromArchive(String path, IProcess process) throws IOException {
		InputStream blobFile = VMDataFactory.class.getResourceAsStream('/' + path);
		if (blobFile != null) {
			ImageInputStream iis = new InputStreamImageWrapper(blobFile);
			iis.setByteOrder(process.getByteOrder());
			return iis;
		} else {
			throw new JVMNotDDREnabledException(process, "DDR could not find VM structure data file " + path);
		}
	}

	/**
	 * Wraps InputStream as data source for ImageInputStream
	 * Used to get blob as resource on class path
	 */
	private static class InputStreamImageWrapper extends ImageInputStreamImpl {
		private ByteArrayInputStream is;

		public InputStreamImageWrapper(InputStream inputStream) throws IOException {
			byte[] buffer = new byte[1024];
			ByteArrayOutputStream out = new ByteArrayOutputStream();
			int r = inputStream.read(buffer, 0, buffer.length);
			while (r != -1) {
				out.write(buffer, 0, r);
				r = inputStream.read(buffer, 0, buffer.length);
			}
			is = new ByteArrayInputStream(out.toByteArray());
		}

		@Override
		public int read() throws IOException {
			int read = is.read();
			streamPos++;
			return read;
		}

		@Override
		public int read(byte[] b, int off, int len) throws IOException {
			int read = is.read(b, off, len);
			if (read != -1) {
				streamPos += read;
			}
			return read;
		}

		@Override
		public long getStreamPosition() throws IOException {
			return streamPos;
		}

		@Override
		public void seek(long pos) throws IOException {
			super.seek(pos);
			is.reset();
			is.skip(streamPos);
		}
	}

	private static String getJ9VMBuildInCore(IProcess process) throws IOException {
		try {
			// Find J9VM build ID
			byte[] pattern = null;
			try {
				pattern = "J9VM - ".getBytes("ASCII"); // "J9VM - YYYYMMDD_BUILD_FLAGS"
			} catch (UnsupportedEncodingException e) {
				// This shouldn't happen
				throw new Error(e);
			}

			long addr = process.findPattern(pattern, 1, 0);
			if (addr != -1) {
				// get string between first and second underscores
				// we can't hard-code offset as z/OS processes may contain extra bit
				long startBuildIDAddr = -1;
				for (long i = 0; i < 30; i++) { // search max of 30 bytes
					if (process.getByteAt(addr + i) == (byte) '_') {
						if (startBuildIDAddr == -1) {
							startBuildIDAddr = addr + i + 1;
						} else { // we've found second underscore
							byte[] buildID = new byte[(int) (addr + i - startBuildIDAddr)];
							for (int j = 0; j < addr + i - startBuildIDAddr; j++) {
								buildID[j] = process.getByteAt(startBuildIDAddr + j);
							}
							return new String(buildID, "UTF-8");
						}
					}
				}
			}
		} catch (MemoryFault e) {
			throw new IOException(e.getMessage());
		}
		throw new JVMNotDDREnabledException(process, "No J9VM build ID found in process");
	}

	private static String getBlobBasedirInArchive(IProcess process) throws IOException {
		ICore core = process.getAddressSpace().getCore();

		Platform platform = core.getPlatform();
		int bytesPerPointer = process.bytesPerPointer();

		switch (platform) {
		case AIX:
			if (bytesPerPointer == 4) {
				return "aix/ppc-32/";
			} else {
				return "aix/ppc-64/";
			}
			// break;
		case LINUX:
			String processorType = core.getProperties().getProperty(ICore.PROCESSOR_TYPE_PROPERTY);
			// determine platform by processorType
			if (bytesPerPointer == 4) {
				if (processorType.equals("x86")) {
					return "linux/ia32/";
				} else if (processorType.equals("ppc")) {
					return "linux/ppc-32/";
				} else if (processorType.equals("s390")) {
					return "linux/s390-31/";
				}
			} else {
				if (processorType.equals("amd64")) {
					return "linux/amd64/";
				} else if (processorType.equals("ppc")) {
					return "linux/ppc-64/";
				} else if (processorType.equals("s390")) {
					return "linux/s390-64/";
				}
			}
			throw new UnknownArchitectureException(process, "Could not determine architecture for Linux core file.");
		case WINDOWS:
			if (bytesPerPointer == 4) {
				return "win/ia32/";
			} else {
				return "win/amd64/";
			}
			// break;
		case ZOS:
			if (bytesPerPointer == 4) {
				return "zos/s390-31/";
			} else {
				return "zos/s390-64/";
			}
			// break;
		}
		// shouldn't be here
		throw new UnknownArchitectureException(process, "Could not determine platform of core file.");
	}
	
	/**
	 * Clear the IVMData cache
	 */
	public static void clearCache() {
		vmDataCache.clear();
	}
	
	/**
	 * Try and determine a blob by scanning the process.
	 * This can be very expensive and should be the last option tried.
	 * 
	 * @param process process to scan
	 * @return a blob if one can be identified, null if not
	 */
	private static ImageInputStream getBlobFromLibrary(IProcess process, StructureHeader header) throws JVMNotFoundException {
		try {
			if(header != null) {
				IBlobFactory factory = BlobFactory.getInstance();
				Platforms platform = null;
				switch(process.getPlatform()) {
					case LINUX :
						platform = (process.bytesPerPointer() == 4) ? Platforms.xi32 : Platforms.xa64;
						break;
				default:
					break;
						
				}
				if(platform != null) {
					int[] data = header.getBlobVersionArray();
					return factory.getBlob(platform, header.getPackageID(), data[2], data[1], data[0]);
				}
			}
			return null;
		} catch (Exception e) {
			throw new JVMNotFoundException(process, e);
		}
	}


	private static StructureHeader findNodeVersion(IProcess proc) throws Exception {
		byte[] pattern = "v0.".getBytes();
		long pos = 0;
		do {
			int[] data = new int[3];
			data[0] = 0;
			int count = 1;
			pos = proc.findPattern(pattern, 0, pos);
			if(pos != -1) {
				pos += (pattern.length - 1);		//jump over the start of the pattern
				StringBuilder version = new StringBuilder();
				while(count < 3) {
					try {
						char b = (char) proc.getByteAt(++pos);
						if((b >= '0') && (b <= '9')) {
							version.append(b);	
						} else {
							try {
								Integer i = Integer.parseInt(version.toString());
								data[count++] = i;
								version = new StringBuilder();
							} catch (NumberFormatException e) {
								break;		//invalid number so can abort
							}
						}
					} catch (MemoryFault e) {
						//ignore, but treat as end of input
						break;
					}
				}
				if(count == 3) {
					int blobVersion = data[0] << 16 | data[1] << 8 | data[2];
					StructureHeader header = new StructureHeader(BlobID.node, blobVersion, "node");
					return header;
				}
			}
		} while (pos != -1);
		return null;
	}
		
}

