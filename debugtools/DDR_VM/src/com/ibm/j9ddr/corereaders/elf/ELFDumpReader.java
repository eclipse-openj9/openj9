/*******************************************************************************
 * Copyright (c) 2009, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders.elf;

import static com.ibm.j9ddr.corereaders.elf.ELFFileReader.ARCH_AMD64;
import static com.ibm.j9ddr.corereaders.elf.ELFFileReader.ARCH_IA32;
import static com.ibm.j9ddr.corereaders.elf.ELFFileReader.ARCH_PPC32;
import static com.ibm.j9ddr.corereaders.elf.ELFFileReader.ARCH_PPC64;
import static com.ibm.j9ddr.corereaders.elf.ELFFileReader.ARCH_S390;
import static com.ibm.j9ddr.corereaders.elf.ELFFileReader.ARCH_ARM;
import static com.ibm.j9ddr.corereaders.elf.ELFFileReader.AT_ENTRY;
import static com.ibm.j9ddr.corereaders.elf.ELFFileReader.AT_HWCAP;
import static com.ibm.j9ddr.corereaders.elf.ELFFileReader.AT_NULL;
import static com.ibm.j9ddr.corereaders.elf.ELFFileReader.AT_PLATFORM;
import static com.ibm.j9ddr.corereaders.elf.ELFFileReader.DT_DEBUG;
import static com.ibm.j9ddr.corereaders.elf.ELFFileReader.DT_NULL;
import static com.ibm.j9ddr.corereaders.elf.ELFFileReader.ELF_NOTE_HEADER_SIZE;
import static com.ibm.j9ddr.corereaders.elf.ELFFileReader.ELF_PRARGSZ;
import static com.ibm.j9ddr.corereaders.elf.ELFFileReader.NT_AUXV;
import static com.ibm.j9ddr.corereaders.elf.ELFFileReader.NT_HGPRS;
import static com.ibm.j9ddr.corereaders.elf.ELFFileReader.NT_PRPSINFO;
import static com.ibm.j9ddr.corereaders.elf.ELFFileReader.NT_PRSTATUS;
import static java.util.logging.Level.FINER;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Comparator;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Properties;
import java.util.SortedMap;
import java.util.TreeMap;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.imageio.stream.ImageInputStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.ICore;
import com.ibm.j9ddr.corereaders.ILibraryDependentCore;
import com.ibm.j9ddr.corereaders.ILibraryResolver;
import com.ibm.j9ddr.corereaders.InvalidDumpFormatException;
import com.ibm.j9ddr.corereaders.LibraryDataSource;
import com.ibm.j9ddr.corereaders.LibraryResolverFactory;
import com.ibm.j9ddr.corereaders.Platform;
import com.ibm.j9ddr.corereaders.elf.unwind.Unwind;
import com.ibm.j9ddr.corereaders.elf.unwind.UnwindTable;
import com.ibm.j9ddr.corereaders.memory.IAddressSpace;
import com.ibm.j9ddr.corereaders.memory.IMemoryRange;
import com.ibm.j9ddr.corereaders.memory.IMemorySource;
import com.ibm.j9ddr.corereaders.memory.IModule;
import com.ibm.j9ddr.corereaders.memory.ISymbol;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.corereaders.memory.MemoryRange;
import com.ibm.j9ddr.corereaders.memory.MissingFileModule;
import com.ibm.j9ddr.corereaders.memory.Module;
import com.ibm.j9ddr.corereaders.osthread.IOSStackFrame;
import com.ibm.j9ddr.corereaders.osthread.IOSThread;
import com.ibm.j9ddr.corereaders.osthread.IRegister;
import com.ibm.j9ddr.corereaders.osthread.OSStackFrame;
import com.ibm.j9ddr.corereaders.osthread.Register;

/**
 * 
 * 
 * @author andhall
 *
 */
public abstract class ELFDumpReader implements ILibraryDependentCore
{
	private static final Logger logger = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);
	
	/**
	 * Setting this system property (to anything) will cause the dump reader
	 * to only search for libraries that are present in the core file because
	 * the process loaded them and not to use any that are separately on disk
	 * *OR* attached to the end of the core file by the diagnostics collector.
	 * 
	 * The system property name matches the field name, so it can be set with:
	 * -Dcom.ibm.j9ddr.corereaders.elf.ELFDumpReader.USELOADEDLIBRARIES=true
	 * at the command line.
	 */ 
	public static final String USELOADEDLIBRARIES = "com.ibm.j9ddr.corereaders.elf.ELFDumpReader.USELOADEDLIBRARIES";
	
	/**
	 * Setting this system property will cause the dump reader to retry searches
	 * for Linux libraries without fully qualified names by prepending the specified
	 * paths in turn.
	 * The search locations are the same as the original search, the libraries may
	 * be found on the local disk, appended to the core file or included in a
	 * jextracted zip file.
	 */
	public static final String KNOWNLIBPATHS_PROPERTY = "com.ibm.j9ddr.corereaders.elf.ELFDumpReader.KNOWNLIBPATHS";
	public static final String[] KNOWNLIBPATHS_DEFAULT_32 = {"/lib", "/usr/lib", "/usr/local/lib"};
	public static final String[] KNOWNLIBPATHS_DEFAULT_64 = {"/lib64", "/usr/lib64", "/usr/local/lib64"};
	
	private static final String[] KNOWNLIBPATHSGLOBAL;
	private final String[] knownLibPaths;
	
	static {
		String paths = System.getProperty(KNOWNLIBPATHS_PROPERTY);
		if( paths != null ) {
			KNOWNLIBPATHSGLOBAL = paths.split(File.pathSeparator);
		} else {
			KNOWNLIBPATHSGLOBAL = null;
		}
	}
	
	protected final ELFFileReader _reader;
	protected final LinuxProcessAddressSpace _process;
	protected final ILibraryResolver _resolver;
	
	private List<DataEntry> _processEntries = new ArrayList<DataEntry>();
	private List<DataEntry> _threadEntries = new ArrayList<DataEntry>();
	private List<DataEntry> _auxiliaryVectorEntries = new ArrayList<DataEntry>();
	private List<DataEntry> _highwordRegisterEntries = new ArrayList<DataEntry>();
	private int _pid;
	private String _executableFileName;
	private String _commandLine;
	private String _executablePathOverride;
	private int _signalNumber = -1;
	
	private IModule _executable;
	private List<IModule> _modules;
	private CorruptDataException _modulesException;
	
	private long _platformIdAddress = 0;
	
	private boolean useLoadedLibraries = false;
	
	//list to keep track of all the files that have been opened by this dump reader
	private ArrayList<ELFFileReader> openFileTracker = new ArrayList<ELFFileReader>(); 

	private Unwind unwinder = null;
	
	protected ELFDumpReader(ELFFileReader reader) throws IOException, InvalidDumpFormatException
	{
		_reader = reader;
		_process = new LinuxProcessAddressSpace(_reader.addressSizeBits() / 8, _reader.getByteOrder(), this);
		unwinder = new Unwind(_process);
		if(reader.getFile() == null) {
			_resolver = LibraryResolverFactory.getResolverForCoreFile(reader.getStream());
		} else {
			_resolver = LibraryResolverFactory.getResolverForCoreFile(reader.getFile());
		}
		
		useLoadedLibraries = (System.getProperty(USELOADEDLIBRARIES) != null);
		
		if( KNOWNLIBPATHSGLOBAL != null ) {
			knownLibPaths = KNOWNLIBPATHSGLOBAL;
		} else {
			knownLibPaths = _reader.is64Bit()?KNOWNLIBPATHS_DEFAULT_64:KNOWNLIBPATHS_DEFAULT_32;
		}
		processProgramHeader();
		processAuxiliaryHeader();
		readProcessData();
		try {
			_executablePathOverride = System.getProperty(SYSTEM_PROP_EXE_PATH);
			readModules();
		} catch (Exception e) {
			//Store it to throw later
			_modulesException = new CorruptDataException(e);
		}
	}
	
	public void close() throws IOException {
		//close the handle to the dump
		_reader.close();
		//now close any open module handles
		if((_executable != null) && (_executable instanceof ELFFileReader)) {
			((ELFFileReader) _executable).is.close();
		}
		for(IModule module : _modules) {
			if(module instanceof ELFFileReader) {
				((ELFFileReader) module).is.close();
			}
		}
		//close any tracked open files
		for(ELFFileReader reader : openFileTracker) {
			if(reader != null) {
				reader.close();
			}
		}
		//release any resources acquired for library resolution
		_resolver.dispose();
	}
	
	public static ELFDumpReader getELFDumpReader(ELFFileReader reader) throws IOException, InvalidDumpFormatException
	{		
		switch (reader.getMachineType()) {
		case (ARCH_IA32) :
			return new ELFIA32DumpReader(reader);
		case (ARCH_AMD64) :
			return new ELFAMD64DumpReader(reader);
		case (ARCH_PPC32) :
			return new ELFPPC32DumpReader(reader);
		case (ARCH_PPC64) :
			return new ELFPPC64DumpReader(reader);
		case (ARCH_S390)  :
			if(reader.is64Bit()) {
				return new ELFS39064DumpReader(reader);
			} else {
				return new ELFS39031DumpReader(reader);
			}
		case (ARCH_ARM) :
			if(reader.is64Bit()) {
				throw new IOException("Unsupported architecture - ARM 64");
			} else {
				return new ELFARM32DumpReader(reader);
			}
		default:
			throw new IOException("Unrecognised machine type: " + reader.getMachineType());
		}
	}
	
	public static ELFDumpReader getELFDumpReader(File file) throws IOException, InvalidDumpFormatException
	{
		ELFFileReader reader = ELFFileReader.getELFFileReader(file);
		return getELFDumpReader(reader);
	}

	public static ELFDumpReader getELFDumpReader(ImageInputStream in) throws IOException, InvalidDumpFormatException
	{
		ELFFileReader reader = ELFFileReader.getELFFileReader(in);
		return getELFDumpReader(reader);
	}

	
	public List<IAddressSpace> getAddressSpaces()
	{
		return Collections.singletonList((IAddressSpace)_process);
	}

	public String getDumpFormat()
	{
		return "ELF";
	}

	public Platform getPlatform()
	{
		return Platform.LINUX;
	}

	public Properties getProperties()
	{
		Properties props = new Properties();
		
		props.setProperty(ICore.SYSTEM_TYPE_PROPERTY, "Linux");
		props.setProperty(ICore.PROCESSOR_TYPE_PROPERTY,getProcessorType());
		props.setProperty(ICore.PROCESSOR_SUBTYPE_PROPERTY,getProcessorSubType());
		
		return props;
	}

	public String getCommandLine() throws CorruptDataException
	{
		return _commandLine;
	}

	private void readProcessData() throws IOException
	{
		if (_processEntries.size() == 0) {
			throw new IOException("No process entries found in Elf file.");
		} else if (_processEntries.size() != 1) {
			throw new IOException("Unexpected number of process entries found in ELF file. Expected 1, found " + _processEntries.size());
		}
		
		DataEntry entry = _processEntries.get(0);
		
		
		_reader.seek(entry.offset);
		_reader.readByte(); // Ignore state
		_reader.readByte(); // Ignore sname
		_reader.readByte(); // Ignore zombie
		_reader.readByte(); // Ignore nice
		_reader.seek(entry.offset + _reader.padToWordBoundary(4));
		_reader.readElfWord(); // Ignore flags
		readUID(); // Ignore uid
		readUID(); // Ignore gid
		_pid = _reader.readInt();
				
		_reader.readInt(); // Ignore ppid
		_reader.readInt(); // Ignore pgrp
		_reader.readInt(); // Ignore sid
		
		// Ignore filler. Command-line is last 80 bytes (defined by ELF_PRARGSZ
		// in elfcore.h).
		// Command-name is 16 bytes before command-line.
		_reader.seek(entry.offset + entry.size - 96);
		_executableFileName = new String(_reader.readBytes(16), "ASCII").trim(); // Ignore command
		try {	
			_commandLine = new String(_reader.readBytes(ELF_PRARGSZ),"ASCII").trim();
		} catch (UnsupportedEncodingException ex) {
			throw new RuntimeException(ex);
		}
	}

	/**
	 * Read all modules in the core file, where "modules" = the executable and all
	 * shared libraries. Put the executable into _executable and the libraries without
	 * the executable into _modules. 
	 * <p>
	 * Find libraries by two methods: iterating through all the segments in the core
	 * file looking for which are libraries and iterating through the debug information 
	 * within the executable. We may find the executable either on disk, within the 
	 * core file as one of the loaded segments, or appended to the core file by 
	 * library collections. Consolidate the list of libraries that we find from the 
	 * debug information with the list from the core file to build the best list possible. 
	 * <p>
	 * When constructing the module objects, use the best available data. This means using 
	 * the section header information from the collected libraries if present since this 
	 * is always more reliable than that in the core file.  
	 *
	 * @throws IOException
	 */

	/* In a second comment to avoid it appearing in hovertext
	 * 
	 * This method is called under at least two completely different circumstances:
	 * 
	 * Circumstance 1 - called during library collection when there are not yet any collected libraries
	 * appended to the core file. In this case all that is really wanted is a list of the 
	 * names of the libraries to guide the collection but this method does not discriminate and 
	 * constructs everything possible, reading section header tables and symbol tables and so on. 
	 * In this case the aim is to return the most complete list of modules, examining both the 
	 * modules within the core file and the list that can be found from the debug data in the 
	 * executable. 
	 *  
	 * Circumstance 2 - called when it is important to have the best possible information about
	 * each module - as for example when called from jdmpview. In this case the collected libraries 
	 * may or may not be appended to the core file but if they are then the section header information 
	 * is always better when taken from the collected library so the modules should be constructed 
	 * from the them. 
	 * 
	 * The reason it is important to keep an eye on both circumstances is that they affect one another.
	 * In circumstance 1, some of the constructed modules are of poor quality because the original library is not
	 * available. In circumstance 2, whether or not the library will be found appended to the core file
	 * depends on whether it was found and returned in circumstance 1. 
	 * 
	 * TODO refactor so that the code paths for the two circumstances are not so entwined. Separate out the 
	 * two functions of gathering the list of names of which libraries exist, needed for both circumstances, and constructing
	 * the best possible image of each library, using core and collected library, only needed for the second circumstance. 
	 * 
	 * There are three sorts of module, too. 
	 * 1. Those found only via the program header table of the core file. This includes most of the system
	 * libraries e.g. ld-linux.so.2. These only have the library name, no path, because they are found from 
	 * the SOname within the module.
	 * 2. Those found only via the debug data in the executable. This can include several of the j9 libraries.
	 * They are present within the core file but the route to the SOname is broken somehow
	 * 3. Those found both ways - most of the j9 libraries are in this case. However note that the 
	 * names will be different because the names found via the debug data are full pathnames and the 
	 * names found via the SOname is just the library name. However they can be matched up via the load
	 * address which is available on both routes. 
	 * 
	 * There are a some oddities too:
	 * 1. the executable always appears in the core file with a SOname of "lib.so"
	 * 2. The core file always contains a library called linux-gate.so which does not correspond to 
	 * a file on disk
	 * 3. Sometimes the same file - e.g. libvmi.so, and the executable itself - will be mapped into memory 
	 * twice or more at different addresses so the same name will appear more than once in the 
	 * list from the core file.
	 */
	
	private void readModules() throws IOException
	{
		if (_executable == null || _executable instanceof MissingFileModule) {
			_modulesException = null;
			_modules = new LinkedList<IModule>();
			readProcessData();
			// Try to find the executable image either on disk or appended to the core file by library collection. 
			// If we can we will probably also find the libraries on disk or appended to the core file and will 
			// therefore get good section header tables. 
			// Otherwise use the copy of the executable within the core file. It may contain some of the library info
			// so it makes a good fallback.
			LibraryDataSource executableFile = findExecutableOnDiskOrAppended();
			ELFFileReader executableELF = null;

			if (executableFile != null && executableFile.getType() != LibraryDataSource.Source.NOT_FOUND && !useLoadedLibraries) {
				try {
					executableELF = getELFReaderFromDataSource(executableFile);
				} catch (InvalidDumpFormatException e) {
					//Not an ELF file.
				}
				if (null != executableELF) {
					createAllModules(executableELF, executableFile.getName());
				}
			} else if( _executable instanceof MissingFileModule) {
				logger.log(Level.FINE, "Libraries unavailable, falling back to loaded modules within the core file.");
				executableELF = getELFReaderForExecutableWithinCoreFile();
				if (null != executableELF) {
					createAllModules(executableELF, _executablePathOverride);
				} else {
					_executable = new MissingFileModule(_process, _executableFileName, Collections.<IMemoryRange>emptyList());
				}
			} else {
				_executable = new MissingFileModule(_process, _executableFileName, Collections.<IMemoryRange>emptyList());
			}
		}
	}

	/**
	 * Gather all of the information about all of the modules in the core file and fill in the _executable and _modules 
	 * instance variables. 
	 * <p>
	 *  The name and a reader for the executable need to be passed in as there is special handling for the executable: not only must it
	 *  be removed from the list of libraries since it is special, but also the name is needed so that when we come across a modules
	 *  with a name of "lib.so" the name of the executable can be put in instead. 
	 * <p>
	 * Gather the full set of libraries using both the debug data in the executable and the list of segments from 
	 * the program header table in the core file. 
	 * <p>
	 * When constructing the module objects, use the copies of the module from disk or appended to the core file for their
	 * section info, if it is available. It is invariably better than that in the core file which may have been overwritten.
	 * <p>
	 * When constructing the module objects, use the name of the module from the debug data if there is a choice, since
	 * the debug data always has full path names. Full path names are needed for accurate library collection. 
	 * <p>
	 * During library collection or when using jdmpview with a core file which does not have libraries appended, the best module 
	 * object will be constructed using data from both sources: the full path name from the 
	 * debug data but the section and symbol information from what is available in the core file. 
	 * <p>
	 * The overall logic is to gather all of the names from the debug data and to gather readers for all of the library files
	 * within the core file, all indexed and sorted by virtual address, then to do a two-way merge on address and 
	 * create the module objects with the best data available. 
	 *  
	 * @param ELFFileReader for the executable
	 * @param executable name
	 * @throws IOException
	 */
	private void createAllModules(ELFFileReader executableELF, String executableName) throws IOException {
		/* Load modules from the core file. Modules in the core file may not have a section header table as
		 * the program header table describes what is loaded into memory. The section header table is used
		 * while loading but not once loaded. If we have a disk version of the library we can pull in the
		 * section header table from that (as well as the symbols as those sections may not be loaded either).
		 */
		List<IModule> allModules = new LinkedList<IModule>();
		long executableBaseAddress = executableELF.getBaseAddress();
		
		Map<Long, ELFFileReader> inCoreReaders = getElfReadersForModulesWithinCoreFile(executableELF, executableName);
		Map<Long, String> libraryNamesFromDebugData = readLibraryNamesFromDebugData(executableELF,
				inCoreReaders, executableELF);
		
		for( Map.Entry<Long, ELFFileReader> entry: inCoreReaders.entrySet() ) {
			long coreFileAddress = entry.getKey();
			ELFFileReader inCoreReader = entry.getValue();
			String moduleName = inCoreReader.readSONAME(_reader);
			/* A module name of lib.so apparently really means the executable. */
			if( "lib.so".equals(moduleName) ) {
				moduleName = executableName;
			}
			String debugName = libraryNamesFromDebugData.get(coreFileAddress);
			ELFFileReader readerForModuleOnDiskOrAppended = null;
			/* Is there an entry in the debug data for this module? */
			if( debugName != null ) {
				moduleName = debugName;
				readerForModuleOnDiskOrAppended = getReaderForModuleOnDiskOrAppended(debugName);
			} else {
				readerForModuleOnDiskOrAppended = getReaderForModuleOnDiskOrAppended(moduleName);
			}
			/* Patch up this modules section header table with the one from the disk version before we
			 * create the module.
			 */
			if ( !inCoreReader.isCompatibleWith( readerForModuleOnDiskOrAppended ) ) {
				// Can we use the on disk reader so we can use it to get missing data.
				readerForModuleOnDiskOrAppended = null;
			}
			IModule module = createModuleFromElfReader(coreFileAddress , moduleName, inCoreReader, readerForModuleOnDiskOrAppended);
			if( module != null ) {
				allModules.add(module);				
			}
		}
		
		// TODO - Sort modules. For the sake of tidiness.
		_modules.addAll(allModules);
		ELFFileReader readerForExectuableOnDiskOrAppended = getReaderForModuleOnDiskOrAppended(executableName);
		_executable = createModuleFromElfReader(executableBaseAddress, executableName, executableELF, readerForExectuableOnDiskOrAppended);
	}
	
	/**
	 * Make a best effort to find the program header table entry in the core file for the executable.
	 * Do this by working through all the loadable entries, looking for the first  
	 * one that says in the elf header that it is for an executable. 
	 *  
	 * @return a reader for the executable
	 */
	private ELFFileReader getELFReaderForExecutableWithinCoreFile() {
		for (ProgramHeaderEntry entry : _reader.getProgramHeaderEntries()) {
			if( !entry.isLoadable() ) {
				continue;
			}
			ELFFileReader elfReader;
			try {
				elfReader = ELFFileReader.getELFFileReaderWithOffset(_reader.getStream(), entry.fileOffset);
				if( elfReader.isExecutable() ) {
					return elfReader;
				}
			} catch (IOException e) {
				// keep iterating through the program header table
			} catch (InvalidDumpFormatException e) {
				// keep iterating through the program header table
			}
		}
		return null;
	}
	
	/**
	 * Iterate through the program header table of the core file constructing an ELFFileReader for
	 * everything that is loadable and is in ELF format. Insert each into a map where they are indexed by base address.
	 * 
	 * @return map BaseAddress -> ELFFileReader
	 */
	private TreeMap<Long, ELFFileReader> getElfReadersForModulesWithinCoreFile(ELFFileReader executableELF, String executableName) {
		TreeMap<Long,ELFFileReader> moduleReadersByBaseAddress = new TreeMap<Long,ELFFileReader>();
//		System.out.println("dumping program header table");
//		for (ProgramHeaderEntry entry : _reader.getProgramHeaderEntries()) {
//			System.out.printf("offset %x address %x, type %d\n",entry.fileOffset,entry.virtualAddress, entry._type);
//		}
		
		for (ProgramHeaderEntry entry : _reader.getProgramHeaderEntries()) {
			if( !entry.isLoadable() || entry.fileSize == 0 ) {
				continue;
			}
			try {
//				System.err.printf("Getting internal reader for %s, virtual address 0x%x, file offset 0x%x\n", executableName, entry.virtualAddress, entry.fileOffset );
				ELFFileReader loadedElf = getReaderFromOffsetWithinCoreFile(_reader.getStream(), entry.fileOffset);
				if (loadedElf != null) {
					moduleReadersByBaseAddress.put(entry.virtualAddress,loadedElf);						
				}
			} catch (IOException e) {
				logger.log(Level.FINER,"IOException reading module from: " + _reader.getSourceName()
						+ " @ " + Long.toHexString(entry.fileOffset));
			}				
		}
		return moduleReadersByBaseAddress;
	}
	
	private ELFFileReader getELFReaderFromDataSource(LibraryDataSource source) throws IOException, InvalidDumpFormatException {
		ELFFileReader executableELF = null;
		switch(source.getType()) {
			case FILE :
				executableELF = ELFFileReader.getELFFileReader(source.getFile());
				break;
			case STREAM :
				executableELF = ELFFileReader.getELFFileReader(source.getStream());
				break;
		}
		openFileTracker.add(executableELF);
		return executableELF;
	}
	
	/**
	 * Use the debug data pointed at by the executable's dynamic table to get a list of the libraries. 
	 * The debug data is supposed to point to all the libraries that the executable uses. 
	 * <p>
	 * @param executableELF a reader to the executable - could be on disk, could be appended to the core file, could be within the core file
	 * @param map of base Address to readers for the modules within the core file
	 * @param reader for the core file as a whole
	 * @return TreeMap of Long -> String - map from base address to the library name 
	 * @throws IOException
	 */
	private Map<Long, String> readLibraryNamesFromDebugData(ELFFileReader executableELF, 
			Map<Long, ELFFileReader> coreModulesMap,
			ELFFileReader coreFileReader) throws IOException {
		
		// Create the method return value
		TreeMap<Long,String> libraryNamesByBaseAddress = new TreeMap<Long, String>();		

		ProgramHeaderEntry dynamic = executableELF.getDynamicTableEntry();
		if (dynamic == null) {
			return libraryNamesByBaseAddress;
		}

		long dynamicTableAddress = dynamic.virtualAddress;
		if (!isReadableAddress(dynamicTableAddress)) {
			return libraryNamesByBaseAddress;
		}
		
		// Seek to the start of the dynamic section
		// WARNING: there is a surprising gotcha here. To read the dynamic table and debug data
		// it is essential to use the reader for the core file as a whole, _reader, 
		// and not the elfReader that was passed in. 
		// The reason is that the executable is loaded into memory twice, and the 
		// entry for the dynamic table in the program header table of the first points at the 
		// dynamic table in the second. For example, the executable is loaded at both 0x8048000 and 
		// 8049000. Both have a program header table where the dynamic entry points at the 
		// dynamic table in the one loaded at 8049000. If you slip up, by the way, and look at 
		// the dynamic table in one loaded at 8048000 you find the debug entry is 0.  
		// It is essential to use _reader because elfReader that is passed in will be 
		// for the 8048000 executable and will not be able to resolve addresses in the 8049000+ range - 
		// it does not have the program header table entries to do so. 
		// The reader for the core file as a whole though can do so. 
		_reader.seekToAddress(dynamicTableAddress);

		// Loop reading the dynamic section tag-value/pointer pairs until a 'debug'
		// entry is found
		long tag;
		long address;

		do {
			// Read the tag and the value/pointer (only used after the loop has terminated)
			tag     = _reader.readElfWord();
			address = _reader.readElfWord();
			
			// Check that the tag is valid. As there may be some debate about the
			// set of valid values, a message will be issued but reading will continue
			/*
			 * CMVC 161449 - SVT:70:jextract invalid tag error
			 * The range of valid values in the header has been expanded, so increasing the valid range
			 * http://refspecs.freestandards.org/elf/gabi4+/ch5.dynamic.html
			 * DT_RUNPATH 			29 	d_val 	optional 	optional
			 * DT_FLAGS 			30 	d_val 	optional 	optional
			 * DT_ENCODING 			32 	unspecified 	unspecified 	unspecified
			 * DT_PREINIT_ARRAY 	32 	d_ptr 	optional 	ignored
			 * DT_PREINIT_ARRAYSZ 	33 	d_val 	optional 	ignored
			 */
			if ( !((tag >= 0         ) && (tag <= 33        )) &&
			     !((tag >= 0x60000000) && (tag <= 0x6FFFFFFF)) &&
			     !((tag >= 0x70000000) && (tag <= 0x7FFFFFFF))    ) {
				logger.log(Level.WARNING,
					"Error reading dynamic section. Invalid tag value '0x" + 
					Long.toHexString(tag) +
					"'. The core file is invalid and the results may unpredictable"
				);
			}
			
			// System.err.println("Found dynamic section tag 0x" + Long.toHexString(tag));
		} while ((tag != DT_NULL) && (tag != DT_DEBUG));

		// If there is no debug section, there is nothing to do
		if ( tag != DT_DEBUG) {
			return libraryNamesByBaseAddress;
		}

		// Seek to the start of the debug data		
		_reader.seekToAddress(address);
			
		// NOTE the rendezvous structure is described in /usr/include/link.h
		//	struct r_debug {
		//		int r_version;
		//		struct link_map *r_map;
		//		ElfW(Addr) r_brk;		/* Really a function pointer */
		//		enum { ... } r_state;
		//		ElfW(Addr) r_ldbase;
		//	};
		//	struct link_map {
		   //		ElfW(Addr) l_addr;		/* Base address shared object is loaded at.  */
		   //		char *l_name;			/* Absolute file name object was found in.  */
		   //		ElfW(Dyn) *l_ld;		/* Dynamic section of the shared object.  */
		   //		struct link_map *l_next, *l_prev;	/* Chain of loaded objects.  */
		//	};
			
		_reader.readElfWord(); // Ignore version (and alignment padding)
		long next = _reader.readElfWord();
		while (0 != next) {
			_reader.seekToAddress(next);
			long baseAddress = _reader.readElfWord();
			long nameAddress = _reader.readElfWord();
			_reader.readElfWord(); // Ignore dynamicSectionAddress
			next = _reader.readElfWord();
			if (0 != baseAddress) {
				// NOTE There is an apparent bug in the link editor on Linux x86-64.
				// The loadedBaseAddress for libnuma.so is invalid, although it is
				// correctly loaded and its symbols are resolved.  The library is loaded
				// around 0x2a95c4f000 but its base is recorded in the link map as
				// 0xfffffff09f429000.

				String name = null;
				try {
					name = _process.readStringAt(nameAddress);
				} catch (MemoryFault e1) {
					//Do nothing
				}
				// If we have a valid link_map entry, open up the actual library file to get the symbols and 
				// library sections.
				// Note: on SLES 10 we have seen a link_map entry with a null name, in non-corrupt dumps, so we now 
				// ignore link_map entries with null names rather than report them as corrupt. See defect 132140
				// Check for zero-length name - sometimes we get empty name even for valid modules. See defect 182917
				// Note : on SLES 11 we have seen a link_map entry with a module name of "7". Using the base address and correlating
				// with the libraries we find by iterating through the program header table of the core file, we know that 
				// this is in fact linux-vdso64.so.1 
				// Detect this by spotting that the name does not begin with "/" - the names should
				// always be full pathnames. In this case just ignore this one.   
				// See defect CMVC 184115 
				if (null != name && name.length() != 0) {
					if (name.startsWith("/")) { // normal case - full pathname
						libraryNamesByBaseAddress.put(baseAddress,name);											
					} else {
						ELFFileReader moduleWithinCoreFile = coreModulesMap.get(baseAddress);
						if (moduleWithinCoreFile != null) {
							String SONameForModuleAtThisAddress = moduleWithinCoreFile.readSONAME(coreFileReader);				
							libraryNamesByBaseAddress.put(baseAddress,SONameForModuleAtThisAddress);
						}
					}
				}
			}
		}

		return libraryNamesByBaseAddress;
	}

	/**
	 * Given an ELF reader, read the symbols, memory ranges and properties from the module 
	 * and construct a Module object. The ELF reader may point to a segment within the core file
	 * or may point to a copy of the module on disk or appended to the core file. 
	 * 
	 * @param loadedBaseAddress of the module
	 * @param name of the module
	 * @param elfReader to the module
	 * 
	 * @return IModule
	 * @throws IOException
	 */
	private IModule createModuleFromElfReader(final long loadedBaseAddress, String name, ELFFileReader inCoreReader, ELFFileReader diskReader) {
		if (name == null) {
			return null;
		}
		if (inCoreReader == null) {
			return new MissingFileModule(_process, name, Collections.<IMemoryRange>emptyList());
		}
		List<? extends ISymbol> symbols = null;
		Map<Long, String> sectionHeaderStringTable = null; 
		List<SectionHeaderEntry> sectionHeaderEntries = null;
		Properties properties;
		Collection<? extends IMemorySource> declaredRanges;
		
		ProgramHeaderEntry ehFrameEntry = null;
		
		// Create the unwind data from the program header with the type GNU_UNWIND.
		// We could look for the .eh_frame section header but section headers aren't
		// guaranteed to be loaded. (More accurately they shouldn't be but sometimes
		// seem to be.)
		// Without this data we can't walk stacks on the platforms that use it.
		// If we don't find the entry, we are probably on a platform which
		// doesn't require it or reading a library that was compiled not to use it
		// so don't raise an error if it isn't present.
		for( ProgramHeaderEntry ph: inCoreReader.getProgramHeaderEntries() ) {
			if( ph.isEhFrame() ) {
				ehFrameEntry = ph;
			}
		}
		try {
			if( ehFrameEntry != null ) {
				unwinder.addCallFrameInformation(loadedBaseAddress, ehFrameEntry, name);
			}
		} catch (MemoryFault mf) {
			// We get known memory faults for ld-linux-x86-64.so.2 in AMD64 dumps (at the first address it's loaded at)
			// and linux-vdso.so.1. The first of these turns up again at a location that works, the second is
			// "magic" so we don't worry about them. We want this code to be as resilient as possible.
			logger.log(Level.FINER,"MemoryFault reading GNU_EH_FRAME data for module with name " + name + " and base address "
					+ Long.toHexString(loadedBaseAddress));
		} catch (CorruptDataException cde) {
			logger.log(Level.FINER,"CorruptDataException reading GNU_EH_FRAME data for module with name " + name + " and base address "
					+ Long.toHexString(loadedBaseAddress));
		} catch (IOException e) {
			logger.log(Level.FINER,"IOException reading GNU_EH_FRAME data for module with name " + name + " and base address "
					+ Long.toHexString(loadedBaseAddress));
		}
		
		try {
			if( diskReader != null ) {
				symbols = diskReader.getSymbols(loadedBaseAddress, true);
				sectionHeaderStringTable = diskReader.getSectionHeaderStringTable();
				sectionHeaderEntries = diskReader.getSectionHeaderEntries();
			} else {
				symbols = inCoreReader.getSymbols(loadedBaseAddress, false);
				sectionHeaderStringTable = inCoreReader.getSectionHeaderStringTable();
				sectionHeaderEntries = inCoreReader.getSectionHeaderEntries();
			}
			properties = inCoreReader.getProperties();
			// Only ever get memory ranges from the data loaded into the core file!
			// But we can use the section headers and string table from the disk or zipped library
			// to navigate. (Section headers are redundant once the library is loaded so may not
			// be in memory.)
			declaredRanges = inCoreReader.getMemoryRanges(loadedBaseAddress, sectionHeaderEntries, sectionHeaderStringTable);
		} catch (IOException e) {
			logger.log(Level.FINER,"Error generating module with name " + name + " and base address "
					+ Long.toHexString(loadedBaseAddress));
			return null;
		}
		
		//Of the declared memory ranges, some will already be in core (i.e. .data) others will have been declared
		//in the core, but not backed.
		List<IMemoryRange> ranges = new ArrayList<IMemoryRange>(declaredRanges.size());
		
		for (IMemorySource source : declaredRanges) {
			IMemorySource coreSource = _process.getRangeForAddress(source.getBaseAddress());
			
			/**
			 * The following test skips sections that originally had an address of 0 
			 * the section header table. 
			 * 
			 * Explanation follows - see also the example section header table at the top 
			 * of SectionHeaderEntry.java  
			 * 
			 * Some of the later sections in the section header table have an address field 
			 * 0. That is a relative address, relative to the base address of the module.
			 * They are usually the sections from .comment onwards.
			 *  
			 * When the entry is constructed the code that creates the entry creates it with
			 * address (base address of the module) + (address field in the SHT) hence those
			 * that had an address of 0 will have an address == base address of the module.
			 * These entries are precisely those that are often not in the core file so it is
			 * not safe to create them as sections - they were there in the on-disk version of
			 * the library but often not in memory. The code above that called elfReader.getMemoryRanges
			 * may have been reading the version of the module from disk (which is good, it means
			 * you get a good section header string table so you get good names for all the 
			 * sections) but not all the sections exist in memory. The danger of adding them
			 * as memory ranges is that they start to overlay the segments in the core file
			 * that come immediately afterwards; that is, they cause jdmpview, for example, to 
			 * believe that the contents of these later areas of memory are backed by the  
			 * contents of the library on disk when they are not.  
			 * See CMVC 185753
			 * 
			 * So, don't add sections that originally had address 0. 
			 * 
			 */
			if (source.getBaseAddress() == loadedBaseAddress) { // must have originally had address 0 in the section header table
				continue;
			}
			
			if (null != coreSource) {
				if (coreSource.isBacked()) {
					// Range already exists
				} else {
					//The range exists but isn't backed. We need to replace the in-core memory range with the one
					//from the library
					if (source.getSize() > 0) {
						_process.removeMemorySource(coreSource);
						_process.addMemorySource(source);
					}
				}
			} else {
				if (source.getSize() > 0) {
					_process.addMemorySource(source);
				}
			}
			ranges.add(source);
		}
		return new Module(_process,name,symbols, ranges, loadedBaseAddress, properties);

	}

	private boolean isValidAddress(long address) {
		IMemoryRange range = _process.getRangeForAddress(address);
		if (range != null) {
			return true;
		}
		return false;
	}

	private boolean isReadableAddress(long address) {
		try {
			_process.getByteAt(address);
			return true;
		} catch (MemoryFault f) {
			return false;
		}
	}
	
	private LibraryDataSource findExecutableOnDiskOrAppended()
	{
		//Three sources for executable data. The command line (limit 80 chars), the executable file name (no path info, limit 16 chars),
		//and the executablePathOverride (passed down from higher layers, not necessarily set).
		
		if (_executablePathOverride != null) {
			try {
				LibraryDataSource libSource = _resolver.getLibrary(_executablePathOverride, false);
				return libSource;
			} catch (FileNotFoundException e) {
				logger.fine("Could not resolve executable, have the native libraries been collected ?");
				logger.logp(FINER,"com.ibm.j9ddr.corereaders.elf.ELFDumpReader","findExecutableOnDiskOrAppended","IOException resolving library", e);	
			}
		}
		
		//Try the command line
		int spaceIndex = _commandLine.indexOf(" ");
		
		if (spaceIndex != -1) {
			String executablePath = _commandLine.substring(0, spaceIndex);
			
			try {
				LibraryDataSource libSource = _resolver.getLibrary(executablePath, true);
				return libSource;
			} catch (IOException e) {
				//Do nothing
			}
		}
		
		//Try the executable
		try {
			LibraryDataSource libSource = _resolver.getLibrary(_executableFileName, true);
			return libSource;
		} catch (IOException e) {
			//Do nothing
		}
		
		return new LibraryDataSource(_executableFileName);
	}

	//Architecture-specific template methods:
	protected abstract long readUID() throws IOException;
	protected abstract String getProcessorType();
	protected abstract SortedMap<String, Number> readRegisters() throws IOException;
	protected abstract String getStackPointerRegisterName();
	protected abstract long getBasePointerFrom(Map<String, Number> registers);
	protected abstract long getInstructionPointerFrom(Map<String, Number> registers);
	protected abstract long getLinkRegisterFrom(Map<String, Number> registers);
	protected abstract void readHighwordRegisters(DataEntry entry, Map<String, Number> registers) throws IOException, InvalidDumpFormatException;

	protected long getStackPointerFrom(Map<String, Number> registers)
	{
		return registers.get(getStackPointerRegisterName()).longValue();
	}
	
	/*
	 * The offset of the instruction pointer from the base pointer within a stack frame.
	 * (Generally 1 word, except on PPC 64 where it's 2 words because of the status register.)
	 */
	protected long getOffsetToIPFromBP() {
		return 1;
	}

	protected String getProcessorSubType() {
		try {
			String proc = readStringAt(_platformIdAddress);
			if(proc == null) {
				return "unknown";
			} else {
				return proc;
			}
		} catch (Exception e) {
			return "unknown";
		}
	}
	
	public String readStringAt(long address) throws IOException
	{
		String toReturn = null;
		
		if (isReadableAddress(address)) {
			StringBuffer buf = new StringBuffer();
			_reader.seekToAddress(address);
			byte b = _reader.readByte();
			while (0 != b) {
				buf.append(new String(new byte[] { b }, "ASCII"));
				b = _reader.readByte();
			}
			toReturn = buf.toString();
		} 
		return toReturn;
	}
	
	private void processProgramHeader() throws IOException
	{
		for (ProgramHeaderEntry entry : _reader.getProgramHeaderEntries()) {
			if (entry.isNote()) {
				readNotes(entry);
			}
			IMemorySource range = entry.asMemorySource();
			//Range gets declared covering entire 64 bit address space. This messes up the overlapping ranges logic 
			//in AbstractMemory so is explicitly filtered here.
			if (! (range.getTopAddress() == 0xffffffffffffffffL && range.getBaseAddress() == 0)) {
				_process.addMemorySource(range);
			}
		}
	}

	private void processAuxiliaryHeader() throws IOException
	{
		for (DataEntry entry : _auxiliaryVectorEntries) {
			readAuxiliaryVector(entry);
		}
	}
	
	private void readNotes(ProgramHeaderEntry entry) throws IOException
	{
		long offset = entry.fileOffset;
		long limit = offset + entry.fileSize;
		while (offset >= entry.fileOffset && offset < limit) {
			offset = readNote(offset);
		}
	}

	private long readNote(long offset) throws IOException
	{
		_reader.seek(offset);
		long nameLength = padToIntBoundary(_reader.readInt());
		long dataSize = _reader.readInt();
		long type = _reader.readInt();
		_reader.readBytes((int) nameLength); // Ignore name

		long dataOffset = offset + ELF_NOTE_HEADER_SIZE + nameLength;

		if (NT_PRSTATUS == type)
			_threadEntries.add(new DataEntry(dataOffset, dataSize));
		else if (NT_PRPSINFO == type)
			_processEntries.add(new DataEntry(dataOffset, dataSize));
		else if (NT_AUXV == type)
			_auxiliaryVectorEntries.add(new DataEntry(dataOffset, dataSize));
		else if (NT_HGPRS == type )
			_highwordRegisterEntries.add(new DataEntry(dataOffset, dataSize));
		
		return dataOffset + padToIntBoundary(dataSize);
	}
	
	/* The ELF NOTES section is padded to 4 byte boundaries.
	 * (Not word size!)
	 */
	private static long padToIntBoundary(long l) {
		return ((l + 3) / 4) * 4;
	}
	
	private void readAuxiliaryVector(DataEntry entry) throws IOException
	{
		_reader.seek(entry.offset);
		if (0 != entry.size) {
			long type = _reader.readElfWord();
			while (AT_NULL != type) {
				if (AT_PLATFORM == type)
					_platformIdAddress = _reader.readElfWord();
				else if (AT_ENTRY == type)
					_reader.readElfWord();
				else if (AT_HWCAP == type)
					_reader.readElfWord(); // TODO extract features in some useful fashion
				else
					_reader.readElfWord(); // Ignore
				type = _reader.readElfWord();
			}
		}
	}

	public IModule getExecutable() throws CorruptDataException
	{
		if (_modulesException != null) {
			throw _modulesException;
		}
		return _executable;
	}

	public List<? extends IModule> getModules() throws CorruptDataException
	{
		if (_modulesException != null) {
			throw _modulesException;
		}
		return _modules;
	}

	public long getProcessId()
	{
		return _pid;
	}

	public List<? extends IOSThread> getThreads()
	{
		List<IOSThread> threads = new ArrayList<IOSThread>(_threadEntries.size());
		
		for (DataEntry entry : _threadEntries) {
			try {
				threads.add(readThread(entry));
			} catch (IOException ex) {
				//TODO handle
			}
		}
		
		return threads;
	}
	
	// Reads the prstatus structure.
	private IOSThread readThread(DataEntry entry) throws IOException {
		_reader.seek(entry.offset);
		_signalNumber = _reader.readInt(); //  signalNumber
		_reader.readInt(); // Ignore code
		_reader.readInt(); // Ignore errno
		_reader.readShort(); // Ignore cursig
		_reader.readShort(); // Ignore dummy
		_reader.readElfWord(); // Ignore pending
		_reader.readElfWord(); // Ignore blocked
		long pid = _reader.readInt() & 0xffffffffL;
		_reader.readInt(); // Ignore ppid
		_reader.readInt(); // Ignore pgroup
		_reader.readInt(); // Ignore session
		
		long utimeSec   = _reader.readElfWord(); // utime_sec
		long utimeUSec  = _reader.readElfWord(); // utime_usec
		long stimeSec   = _reader.readElfWord(); // stime_sec
		long stimeUSec  = _reader.readElfWord(); // stime_usec
		_reader.readElfWord(); // Ignore cutime_sec
		_reader.readElfWord(); // Ignore cutime_usec
		_reader.readElfWord(); // Ignore cstime_sec
		_reader.readElfWord(); // Ignore cstime_usec
		
		// Registers are in a elf_gregset_t (which is defined per platform).
		SortedMap<String, Number> registers = readRegisters();
		Properties properties = new Properties();
		properties.setProperty("Thread user time secs", Long.toString(utimeSec));
		properties.setProperty("Thread user time usecs", Long.toString(utimeUSec));
		properties.setProperty("Thread sys time secs", Long.toString(stimeSec));
		properties.setProperty("Thread sys time usecs", Long.toString(stimeUSec));
		
		if (pid == _pid ) { // main thread (in Linux JVM fork&abort case this is the only thread) 
			for (DataEntry registerEntry : _highwordRegisterEntries) {
				try {
					readHighwordRegisters(registerEntry, registers);
				} catch (InvalidDumpFormatException e) {
					logger.warning(e.toString());
				}
			}
		}
	
		return new LinuxThread(pid, registers, properties);
	}
	
	protected class LinuxThread implements IOSThread
	{
		private final Map<String, Number> registers;
		
		private final Properties properties;
		
		private final long tid;
		
		private final List<IMemoryRange> memoryRanges = new LinkedList<IMemoryRange>();
		
		private final List<IOSStackFrame> stackFrames = new LinkedList<IOSStackFrame>();
		
		private boolean stackWalked = false;
		
		private LinuxThread(long tid, Map<String, Number> registers, Properties properties)
		{
			this.registers = registers;
			this.properties = properties;
			this.tid = tid;
		}
		
		public Collection<? extends IMemoryRange> getMemoryRanges()
		{
			// Get stack frames will ensure the memory ranges for the stack
			// are populated. (And checks if the stack has already been walked.)
			getStackFrames();
			
			return memoryRanges;
		}

		public Properties getProperties()
		{
			return properties;
		}

		public List<? extends IRegister> getRegisters()
		{
			List<IRegister> regList = new ArrayList<IRegister>(registers.size());
			
			for (String regName : registers.keySet()) {
				Number value  = registers.get(regName);
				
				regList.add(new Register(regName,value));
			}
			
			return regList;
		}
		
		public long getInstructionPointer() {
			if (registers != null) {
				return getInstructionPointerFrom(registers);
			} else {
				return 0;
			}
		}

		public List<? extends IOSStackFrame> getStackFrames()
		{
			if (!stackWalked) {
				walkStack();
				stackWalked = true;
			}
			return stackFrames;
		}

		public void walkStack() {

			long stackPointer = getStackPointerFrom(registers);
			long basePointer = getBasePointerFrom(registers);
			long instructionPointer = maskInstructionPointer(getInstructionPointerFrom(registers));

			if (0 == instructionPointer || !isValidAddress(instructionPointer)) {
				instructionPointer = maskInstructionPointer(getLinkRegisterFrom(registers));
			}

			if ((0 != instructionPointer)
				&& isValidAddress(instructionPointer)
				&& isValidAddress(stackPointer)
			) {
				IMemoryRange range = _process.getRangeForAddress(stackPointer);
				memoryRanges.add(new MemoryRange(_process.getAddressSpace(), range, "stack"));
				UnwindTable unwindTable = null;

				try {
					
					/* Attempt to obtain the unwind information for this ip. */
					if (unwinder == null) {
						unwinder = new Unwind(_process);
					}
					// dumpRegisters(registers);
					unwindTable = unwinder
							.getUnwindTableForInstructionAddress(instructionPointer);
					// If we couldn't find any unwind data, the the ip and base pointer are valid.
					Map<String, Number> nextRegisters = registers;
					if (unwindTable != null) {
						// Apply the unwind data based on this ip to generate the first frame.
//						System.err.printf("Using dwarf unwind for top frame 0\n");
						
						nextRegisters = unwindTable.apply(nextRegisters, getDwarfRegisterKeys());
						
						// dumpRegisters(nextRegisters);
						
						long newStackPointer = unwindTable.getFrameAddress();
						// System.err.printf("Replacing %s:0x%x with %s:0x%x as stack pointer\n", stackPointerName, nextRegisters.get(stackPointerName), stackPointerName, newStackPointer);
						nextRegisters.put(getStackPointerRegisterName(), newStackPointer);
						OSStackFrame stackFrame = new OSStackFrame(newStackPointer, instructionPointer);
						stackFrames.add(stackFrame);
						instructionPointer = maskInstructionPointer(unwindTable.getReturnAddress());
						unwindTable = unwinder.getUnwindTableForInstructionAddress(instructionPointer);
					}

					// Loop through stack frames, starting from current frame base pointer
					// Maximum 256 frames, for protection against excessive or infinite loops in corrupt dumps
					int loops = 0;
//					System.err.printf("Instruction pointer is 0x%x, unwind table is: %s\n", instructionPointer, unwindTable);
					while (instructionPointer != 0x0 && isValidAddress(instructionPointer) && loops++ < 256) {

						if (unwindTable != null) {
//							System.err.printf("Using dwarf unwind for frame %d\n", loops);
							
							nextRegisters = unwindTable.apply(nextRegisters, getDwarfRegisterKeys());

							// dumpRegisters(nextRegisters);

							long newStackPointer = unwindTable.getFrameAddress();
							// System.err.printf("Replacing %s:0x%x with %s:0x%x as stack pointer\n", stackPointerName, nextRegisters.get(stackPointerName), stackPointerName, newStackPointer);
							nextRegisters.put(getStackPointerRegisterName(), newStackPointer);
							OSStackFrame stackFrame = new OSStackFrame(newStackPointer, instructionPointer);
							stackFrames.add(stackFrame);
							// System.err.printf("#%d bp:0x%016X ip:0x%016X \n", loops-1, stackFrame.getBasePointer(), stackFrame.getInstructionPointer() );
							instructionPointer = maskInstructionPointer(unwindTable.getReturnAddress());
							unwindTable = unwinder.getUnwindTableForInstructionAddress(instructionPointer);
							// basePointer = newStackPointer;

						} else if (basePointer != 0) {
//							 System.err.printf("Using basic unwind for frame %d, ip: 0x%x\n", loops, instructionPointer);
							// previousBasePointer = basePointer;
							if( isValidAddress(instructionPointer) ) {
								stackFrames.add(new OSStackFrame(basePointer, instructionPointer));
							}
							long ptr = basePointer;
							basePointer = _process.getPointerAt(ptr);
							ptr += _process.bytesPerPointer() * getOffsetToIPFromBP();
							instructionPointer = maskInstructionPointer(_process.getPointerAt(ptr));
							unwindTable = unwinder.getUnwindTableForInstructionAddress(instructionPointer);
							// TODO - if we do transition back to unwinding with DWARF we need to get the registers right.
						} else {
							logger.log(Level.FINER, "Base pointer is zero");
						}

//						System.err.printf("Instruction pointer is 0x%x, unwind table is: %s\n", instructionPointer, unwindTable);

					}
				} catch (MemoryFault ex) {
					logger.log(Level.FINER,"MemoryFault reading stack @ " + String.format("0x%x", ex.getAddress())
							+ " for thread " + tid);
				} catch (CorruptDataException e) {
					logger.log(Level.FINER,"CorruptDataException reading stack from: " + _reader.getSourceName()
							+ " for thread " + tid);
				} catch (IOException e) {
					logger.log(Level.FINER,"IOException reading stack from: " + _reader.getSourceName()
							+ " for thread " + tid);
				}
			}
		}
		
		private void dumpRegisters(Map<String, Number> registers) {
			System.err.printf("-------------------------\n");
			int rcount = 0;
			List<String> keys = new LinkedList<String>();
			for( String key : registers.keySet() ) {
				if(key != null ) {
					keys.add(key);
				}
			}
			Collections.sort(keys);
			for( String key: keys  ) {
				System.err.printf("%s = 0x%x (%d) ", key, registers.get(key), registers.get(key));
				rcount++;
				if( rcount % 4 == 0) {
					System.err.println();
				}
			}
			System.err.println();
		}

		public long getThreadId() throws CorruptDataException
		{
			return tid;
		}

		public long getBasePointer() {
			return getBasePointerFrom(registers);
		}

		public long getStackPointer() {
			return getStackPointerFrom(registers);
		}
		
	}

	public int getSignalNumber()
	{
		if (_signalNumber == -1) {
			getThreads();
		}
		return _signalNumber;
	}

	/*
	 * Returns an array of strings containing the register names for each
	 * register in that element index. For example element 1 might be "r1" and
	 * element 16 might be "rip" Allows the hash table of register values to be
	 * mapped to/from the register numbers that dwarf uses. This methods needs
	 * to be overridden by each platform that returns true from useDwarfUnwind()
	 * You need to consult the platform ABI to map register names to dwarf
	 * numbers.
	 */
	protected abstract String[] getDwarfRegisterKeys();
	
	/* Overridden for z/Linux 31 as the top bit needs to be masked out. */
	protected long maskInstructionPointer(long pointer) {
		return pointer;
	}
	
	public void executablePathHint(String path)
	{
		_executablePathOverride = path;
		try {
			readModules();
		} catch (IOException e) {
			//TODO handle
		}
	}

	/**
	 * Create an ELFReader to a module at a given offset within the core file. Return null if this is not
	 * possible - usually this is because the offset does not point to something in ELF format.  
	 * 
	 * @param in stream for the core file
	 * @param offset
	 * @return reader to the module or null if not possible
	 * @throws IOException
	 */
	private ELFFileReader getReaderFromOffsetWithinCoreFile(ImageInputStream in, long offset) throws IOException {
		ELFFileReader loadedElf = null;
		try {
			loadedElf = ELFFileReader
					.getELFFileReaderWithOffset(in, offset);
		} catch (InvalidDumpFormatException e) {
			// This is usually thrown because the given program header entry
			// points to a segment within the core file that does not start with an ELF header.
			// This is not an error and is quite common: many of the segments within the core file are 
			// not ELF-format modules. For example the segment that contains information about 
			// the process, or segments to represent thread are not 
			// ELF-format modules and do not start with an ELF header.
			return null;
		}
		return loadedElf;
	}

	/**
	 * See if a reader can be found for the module from disk or at the bottom of the core.
	 * If one does exist, the section header table is invariably better than the one within the core file.
	 * <p>
	 * Typically this will only be called with a full path library name e.g. /usr/lib/libnuma.so
	 * however it attempts to search well known locations if the library isn't found.
	 * 
	 * @param name
	 * @return ElfFileReader to the module or null if could not be created
	 * 
	 * @throws IOException
	 * @throws InvalidDumpFormatException
	 */
	private ELFFileReader getReaderForModuleOnDiskOrAppended(String name)	throws IOException {
		if (name != null) { 
			try {
				LibraryDataSource libsource = _resolver.getLibrary(name);
				if (libsource != null && LibraryDataSource.Source.NOT_FOUND != libsource.getType()) {
					ELFFileReader loadedElf = getELFReaderFromDataSource(libsource);
					return loadedElf;
				}
			} catch (FileNotFoundException e) {
				// in this circumstance it simply means that there is no collected version to be found
				// not an error, just continue
			} catch (InvalidDumpFormatException e) {
				// in this circumstance it simply means that there is no collected version to be found
				// not an error, just continue
			}

			if( !name.startsWith("/") ) {
				// Failed to find the library but it's a non-absolute unix path.
				// Retry with some well known locations.
				for( String path: knownLibPaths ) {
					String fullPath = path.endsWith("/")?path+name:path+"/"+name;
					try {
						//System.err.println("Looking for library " + name + " at " + fullPath);
						LibraryDataSource libsource = _resolver.getLibrary(fullPath);
						if (libsource != null && LibraryDataSource.Source.NOT_FOUND != libsource.getType()) {
							ELFFileReader loadedElf = getELFReaderFromDataSource(libsource);
							// Useful for identifying which library was with which full path using which type of stream (file or zip).
							//System.err.println("Found library " + name + " at " + fullPath + " using " + loadedElf.getStream() );
							return loadedElf;
						}
					} catch (FileNotFoundException e) {
						// in this circumstance it simply means that there is no collected version to be found
						// not an error, just continue
					} catch (InvalidDumpFormatException e) {
						// in this circumstance it simply means that there is no collected version to be found
						// not an error, just continue
					}
				}
			}
		}
		return null;
	}
	
	public class RegisterComparator implements Comparator<String> {
		public int compare(String s1, String s2) {
			// Pad trailing single digit register names, eg gpr1 to gpr01 to sort nicely
			int last = s1.length()-1;
			if (last >= 1) {
				if (Character.isDigit(s1.charAt(last)) && Character.isLetter(s1.charAt(last-1))) {
					s1 = s1.substring(0,last) + "0" + s1.substring(last);
				}
			}
			last = s2.length()-1;
			if (last >= 1) {
				if (Character.isDigit(s2.charAt(last)) && Character.isLetter(s2.charAt(last-1))) {
					s2 = s2.substring(0,last) + "0" + s2.substring(last);
				}
			}
			return s1.compareTo(s2);
		}
	}
}
