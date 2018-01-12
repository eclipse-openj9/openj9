/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.dtfj.image.j9;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;

import java.util.Iterator;
import java.util.List;
import java.util.Properties;
import java.util.Vector;

import javax.imageio.stream.ImageInputStream;

import com.ibm.dtfj.addressspace.IAbstractAddressSpace;
import com.ibm.dtfj.corereaders.ClosingFileReader;
import com.ibm.dtfj.corereaders.ICoreFileReader;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;

/**
 * @author fbogsanyi
 *
 */
public class Builder implements com.ibm.dtfj.corereaders.Builder
{
	private IAbstractAddressSpace _memory;
	private Vector _addressSpaces = new Vector();
	private long _environmentAddress;
	private DataUnavailable _executableException;
	private DataUnavailable _libraryException;
	private String _osType;
	private String _cpuType;
	private String _cpuSubType;
	private long _creationTimeMillis;
	private IFileLocationResolver _resolvingAgent;
	
	private BuilderShutdownHook _fileTracker = new BuilderShutdownHook();
	
	
	/**
	 * @param core The abstraction over the core file
	 * @param openCoreFile The open file which 
	 * @param environmentAddress The address of the environment structure in the core
	 * @param resolvingAgent The agent we can delegate to to locate files for us
	 */
	public Builder(ICoreFileReader core, ClosingFileReader openCoreFile, long environmentAddress, IFileLocationResolver resolvingAgent)
	{
		_memory = core.getAddressSpace();
		_environmentAddress = environmentAddress;
		_resolvingAgent = resolvingAgent;
		
		//we need to ask for a shutdown hook so we can close all the files we opened
		Runtime.getRuntime().addShutdownHook(_fileTracker);
		//no longer track the core as that is being done by the Image
		_fileTracker.addFile(openCoreFile);
	}
	
	
	/**
	 * @param core The abstraction over the core file
	 * @param stream The stream for the core file 
	 * @param environmentAddress The address of the environment structure in the core
	 * @param resolvingAgent The agent we can delegate to to locate files for us
	 */
	public Builder(ICoreFileReader core, ImageInputStream stream, long environmentAddress, IFileLocationResolver resolvingAgent)
	{
		_memory = core.getAddressSpace();
		_environmentAddress = environmentAddress;
		_resolvingAgent = resolvingAgent;		
	}

	/* (non-Javadoc)
	 * @see com.ibm.jvm.j9.dump.systemdump.Builder#buildProcess(java.lang.String, java.lang.String, java.util.Properties, java.lang.Object, java.util.Iterator, java.lang.Object, java.util.Iterator, int)
	 */
	public Object buildProcess(Object addressSpace, String pid,
			String commandLine, Properties environment, Object currentThread,
			Iterator threads, Object executable, Iterator libraries,
			int addressSize)
	{
		ImageProcess process = null;
		if ((null == _executableException) && (null == _libraryException)) {
			process = new ImageProcess(pid, commandLine, environment,
					(ImageThread) currentThread, threads,
					(ImageModule) executable, libraries, addressSize);
		} else {
			process = new PartialProcess(pid, commandLine, environment, (ImageThread) currentThread, threads, (ImageModule) executable, libraries, addressSize, _executableException, _libraryException);
		}
		ImageAddressSpace space = (ImageAddressSpace) addressSpace;
		space.addProcess(process);
		return process;
	}

	/* (non-Javadoc)
	 * @see com.ibm.jvm.j9.dump.systemdump.Builder#buildAddressSpace(java.lang.Object, java.util.Iterator)
	 */
	public Object buildAddressSpace(String name, int id) {
		ImageAddressSpace space = new ImageAddressSpace(_memory, id);
		_addressSpaces.add(space);
		return space;
	}

	/* (non-Javadoc)
	 * @see com.ibm.jvm.j9.dump.systemdump.Builder#buildRegister(java.lang.String, java.lang.Number)
	 */
	public Object buildRegister(String name, Number value) {
		return new ImageRegister(name, value);
	}

	/* (non-Javadoc)
	 * @see com.ibm.jvm.j9.dump.systemdump.Builder#buildStackSection(long, long)
	 */
	public Object buildStackSection(Object addressSpace, long stackStart, long stackEnd) {
		return new StackImageSection(pointer(addressSpace, stackStart), stackEnd - stackStart);
	}

	private ImagePointer pointer(Object addressSpace, long address) {
		return new ImagePointer((ImageAddressSpace) addressSpace, address);
	}

	/* (non-Javadoc)
	 * @see com.ibm.jvm.j9.dump.systemdump.Builder#buildThread(java.lang.String, java.util.Iterator, java.lang.Object, java.util.Iterator, java.util.Properties)
	 */
	public Object buildThread(String name, Iterator registers, Iterator stackSections, Iterator stackFrames, Properties properties, int signalNumber)
	{
		return new ImageThread(name, registers, stackSections, stackFrames, properties, signalNumber);
	}

	/* (non-Javadoc)
	 * @see com.ibm.jvm.j9.dump.systemdump.Builder#buildModuleSection(java.lang.String, long, long)
	 */
	public Object buildModuleSection(Object addressSpace, String name, long imageStart, long imageEnd) {
		return new ModuleImageSection(name, pointer(addressSpace, imageStart), imageEnd - imageStart);
	}

	/* (non-Javadoc)
	 * @see com.ibm.jvm.j9.dump.systemdump.Builder#buildModule(java.lang.String, java.util.Properties, java.util.Iterator, java.util.Iterator)
	 */
	public Object buildModule(String name, Properties properties, Iterator sections, Iterator symbols, long loadAddress) {
		return new ImageModule(name, properties, sections, symbols, loadAddress);
	}

	public Iterator getAddressSpaces() {
		return _addressSpaces.iterator();
	}
	
	public long getEnvironmentAddress() {
		return _environmentAddress;
	}

	public long getValueOfNamedRegister(List registers, String string)
	{
		Iterator regs = registers.iterator();
		
		while (regs.hasNext()) {
			ImageRegister register = (ImageRegister) regs.next();
			if (register.getName().equals(string)) {
				try {
					return register.getValue().longValue();
				} catch (CorruptDataException e) {
				}
			}
		}
		return -1;	//maybe this should throw
	}

	public Object buildStackFrame(Object addressSpace, long stackBasePointer, long pc)
	{
		ImageAddressSpace space = (ImageAddressSpace) addressSpace;
		return new ImageStackFrame(space, space.getPointer(pc), space.getPointer(stackBasePointer));
	}

	public ClosingFileReader openFile(String filename) throws IOException
	{
		//this is where the resolving agent comes in handy.  Not only will it look on the filesystem but it is also free to look into jars and other interesting containers where we can expect to find things
		File foundFile = _resolvingAgent.findFileWithFullPath(filename);
		ClosingFileReader reader = new ClosingFileReader(foundFile);
		_fileTracker.addFile(reader);
		return reader;
	}
	
	public Object buildSymbol(Object addressSpace, String functionName, long relocatedFunctionAddress)
	{
		return new ImageSymbol(functionName, ((ImageAddressSpace)addressSpace).getPointer(relocatedFunctionAddress));
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.corereaders.Builder#setExecutableUnavailable(java.lang.String)
	 */
	public void setExecutableUnavailable(String description)
	{
		_executableException = new DataUnavailable(description);
		//note that we usually can't look for libraries if we don't have an executable so set this here, as well
		_libraryException = _executableException;
		///XXX: this might not be safe!  It depends on the actual use of this component in the core readers
	}

	public String getOSType()
	{
		return _osType;
	}
	
	public String getCPUType()
	{
		return _cpuType;
	}
	
	public String getCPUSubType()
	{
		return _cpuSubType;
	}

	public long getCreationTime()
	{
		return _creationTimeMillis;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.corereaders.Builder#setOSType(java.lang.String)
	 */
	public void setOSType(String osType)
	{
		_osType = osType;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.corereaders.Builder#setCPUType(java.lang.String)
	 */
	public void setCPUType(String cpuType)
	{
		_cpuType = cpuType;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.corereaders.Builder#setCPUSubType(java.lang.String)
	 */
	public void setCPUSubType(String subType)
	{
		_cpuSubType = subType;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.corereaders.Builder#setCreationTime(long)
	 */
	public void setCreationTime(long millis)
	{
		_creationTimeMillis = millis;
	}

	public Object buildCorruptData(Object addressSpace, String message, long address)
	{
		return new CorruptData(message, pointer(addressSpace, address));
	}
}
