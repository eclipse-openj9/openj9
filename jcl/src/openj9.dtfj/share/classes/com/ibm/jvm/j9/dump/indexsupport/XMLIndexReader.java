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
package com.ibm.jvm.j9.dump.indexsupport;

import java.io.IOException;
import java.io.InputStream;
import java.util.Iterator;
import java.util.Stack;

import javax.imageio.stream.ImageInputStream;
import javax.xml.parsers.ParserConfigurationException;
import javax.xml.parsers.SAXParser;
import javax.xml.parsers.SAXParserFactory;

import org.xml.sax.Attributes;
import org.xml.sax.SAXException;
import org.xml.sax.helpers.DefaultHandler;

import com.ibm.dtfj.corereaders.ClosingFileReader;
import com.ibm.dtfj.corereaders.ICoreFileReader;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.image.j9.Builder;
import com.ibm.dtfj.image.j9.IFileLocationResolver;
import com.ibm.dtfj.image.j9.Image;
import com.ibm.dtfj.image.j9.ImageAddressSpace;
import com.ibm.dtfj.image.j9.ImageProcess;

/**
 * @author jmdisher
 * Reads the XML Index and builds the DTFJ objects as it goes.
 */
public class XMLIndexReader extends DefaultHandler implements IParserNode
{
	private ICoreFileReader _coreFile;
	private Image _coreImage;
	private Stack _elements;
		
	//used for pulling out raw data between XML tags
	private StringBuffer _scrapingBuffer = new StringBuffer();
	
	/**
	 * Responsible for finding files requested by the DTFJ components.  This is passed to the Builder so it can
	 * intelligently delegate file resolution.
	 */
	private IFileLocationResolver _fileResolvingAgent;
	
	private ClosingFileReader _reader;
	private ImageInputStream _stream;
	
	
	/**
	 * Creates an Image from the given XML index stream and the corresponding corefile
	 * @param input
	 * @param core
	 * @param reader The open file that the core is built upon
	 * @param fileResolver The file location resolving agent which we can use when constructing the builder to find other files for us
	 * @return
	 */
	public Image parseIndexWithDump(InputStream input, ICoreFileReader core, ClosingFileReader reader, IFileLocationResolver fileResolver)
	{
		_fileResolvingAgent = fileResolver;
		_elements = new Stack();
		_coreFile = core;	//some entities need this for instantiation
		_reader = reader;	//required for when the builder starts up
		SAXParserFactory factory = SAXParserFactory.newInstance();
		try {
			SAXParser parser = factory.newSAXParser();
			//push this object onto the stack since it is the first delegate for the parsing
			_elements.push(this);
			parser.parse(input, this);
			_elements.pop();
			_coreImage.SetSource(reader.getURIOfFile());
		} catch (ParserConfigurationException e) {
			_createCoreImageAfterParseError(e);
		} catch (SAXException e) {
			_createCoreImageAfterParseError(e);
		} catch (IOException e) {
			_createCoreImageAfterParseError(e);
		} catch (IllegalStateException e ) {
			_createCoreImageAfterParseError(e);
		}
		//we need to hook the image we created here
		return _coreImage;
	}
	
	/**
	 * Creates an Image from the given XML index stream and the corresponding corefile
	 * @param input
	 * @param core
	 * @param reader The open file that the core is built upon
	 * @param fileResolver The file location resolving agent which we can use when constructing the builder to find other files for us
	 * @return
	 */
	public Image parseIndexWithDump(InputStream input, ICoreFileReader core, ImageInputStream stream, IFileLocationResolver fileResolver)
	{
		_fileResolvingAgent = fileResolver;
		_elements = new Stack();
		_coreFile = core;	//some entities need this for instantiation
		_stream = stream;	//required for when the builder starts up
		SAXParserFactory factory = SAXParserFactory.newInstance();
		try {
			SAXParser parser = factory.newSAXParser();
			//push this object onto the stack since it is the first delegate for the parsing
			_elements.push(this);
			parser.parse(input, this);
			_elements.pop();
		} catch (ParserConfigurationException e) {
			_createCoreImageAfterParseError(e);
		} catch (SAXException e) {
			_createCoreImageAfterParseError(e);
		} catch (IOException e) {
			_createCoreImageAfterParseError(e);
		} catch (IllegalStateException e ) {
			_createCoreImageAfterParseError(e);
		}
		//we need to hook the image we created here
		return _coreImage;
	}
	
	public void startElement(String uri,
            String localName,
            String qName,
            Attributes attributes)
     throws SAXException
     {
		_checkScrapeBuffer();
		IParserNode node = ((IParserNode)(_elements.peek())).nodeToPushAfterStarting(uri, localName, qName, attributes);
		assert (null != node) : "Node should not be null when starting new tag: " + qName;
		_elements.push(node);
    }
	
	public void endElement(String uri,
            String localName,
            String qName)
     throws SAXException
     {
		_checkScrapeBuffer();
		// pop whatever we were parsing and notify them that we are discarding them
		IParserNode formerTop = (IParserNode) _elements.pop();
		formerTop.didFinishParsing();
     }

	private void _checkScrapeBuffer()
	{
		String collapse = _scrapingBuffer.toString();
		_scrapingBuffer = new StringBuffer();
		((IParserNode)(_elements.peek())).stringWasParsed(collapse);
	}
	
	public void characters(char[] arg0, int arg1, int arg2) throws SAXException
	{
		_scrapingBuffer.append(arg0, arg1, arg2);
	}

	public IParserNode nodeToPushAfterStarting(String uri, String localName, String qName, Attributes attributes)
	{
		IParserNode next = null;
		
		if (qName.equals("j9dump")) {
			next = new NodeJ9Dump(this,  attributes);
		} else {
			//this is an error
			next = NodeUnexpectedTag.unexpectedTag(qName, attributes);
		}
		return next;
     }
	
	public void stringWasParsed(String string)
	{
		//ignore
	}
	
	public void didFinishParsing()
	{
		//ignore
	}
	
	public void setJ9DumpData(long environ, String osType, String osSubType, String cpuType, int cpuCount, long bytesMem, int pointerSize, Image[] imageRef, ImageAddressSpace[] addressSpaceRef, ImageProcess[] processRef)
	{
		Builder builder = null;
		if(_stream == null) {
			//extract directly from the file
			builder = new Builder(_coreFile, _reader, environ, _fileResolvingAgent);
		} else {
			//extract using the data stream
			builder = new Builder(_coreFile, _stream, environ, _fileResolvingAgent);
		}
		_coreFile.extract(builder);
		//Jazz 4961 : chamlain : NumberFormatException opening corrupt dump
		if (cpuType == null) cpuType = builder.getCPUType();
		String cpuSubType = builder.getCPUSubType();
		if (osType == null) osType = builder.getOSType();
		long creationTime = builder.getCreationTime();
		_coreImage = new Image(osType, osSubType, cpuType, cpuSubType, cpuCount, bytesMem, creationTime);
		ImageAddressSpace addressSpace = (ImageAddressSpace) builder.getAddressSpaces().next();
		ImageProcess process = (ImageProcess) addressSpace.getCurrentProcess();
		// Add all the address spaces to the core image
		// Try to find the corresponding process and address space for the XML
		// If not sure, use the first address space/process pair found
		for (Iterator it = builder.getAddressSpaces(); it.hasNext();) {
			ImageAddressSpace addressSpace1 = (ImageAddressSpace) it.next();
			final boolean vb = false;
			if (vb) System.out.println("address space "+addressSpace1);
			_coreImage.addAddressSpace(addressSpace1);
			for (Iterator it2 = addressSpace1.getProcesses(); it2.hasNext(); ) {
				ImageProcess process1 = (ImageProcess)it2.next();
				if (vb) try {
					System.out.println("process "+process1.getID());
				} catch (DataUnavailable e) {
				} catch (CorruptDataException e) {	
				}
				if (process == null || isProcessForEnvironment(environ, addressSpace1, process1)) {
					addressSpace = addressSpace1;
					process = process1;
					if (vb) System.out.println("default process for Runtime");
				}
			}
		}
		if (null != process) {
			// Double-check core file and XML pointer sizes
			//CMVC 156226 - DTFJ exception: XML and core file pointer sizes differ (zOS)
			// z/OS can have 64-bit or 31-bit processes, Java only reports 64-bit or 32-bit.
			if (process.getPointerSize() != pointerSize && 
				!(process.getPointerSize() == 31 && pointerSize == 32)) {
				System.out.println("XML and core file pointer sizes differ "+process.getPointerSize()+"!="+pointerSize);
			}
		} else {
			throw new IllegalStateException("No process found in the dump.");
		}
		imageRef[0] = _coreImage;
		addressSpaceRef[0] = addressSpace;
		processRef[0] = process;
	}

	/**
	 * Check whether the process id might correspond to the environment pointer.
	 * Search memory nearby the location pointed to by the PID for the environment pointer.
	 * This is a crude test which works on z/OS when the PID is the Enclave Data Block.
	 * For other operating systems there is only one address space/process so matching it
	 * with the XML is not important.
	 * @param environ The pointer to look for.
	 * @param addressSpace1 The candidate address space.
	 * @param process1 The candidate process.
	 * @return true if this looks like the right process for the the environment.
	 */
	private boolean isProcessForEnvironment(long environ,
			ImageAddressSpace addressSpace1, ImageProcess process1) {

		//CMVC 156226 - DTFJ exception: XML and core file pointer sizes differ (zOS)
		int pointerSize = process1.getPointerSize();
		int bytesPerPointer = (process1.getPointerSize() + 7) / 8;
		long bitmask = (1L << pointerSize) - 1;
		environ &= bitmask;		//if this is a 31 bit z/OS process, then make the environ pointer 31 bit as well

		long ptr;
		long address;
		try {
			String id = process1.getID();
			if(null == id) {		//cannot proceed without the process ID
				return false;
			}
			address = Long.decode(id).longValue();
		} catch (NumberFormatException e) {
			return false;
		} catch (DataUnavailable e) {
			return false;
		} catch (CorruptDataException e) {
			return false;
		}
		for(int i = 0; i < 64; i++) {
			try {
				ptr = addressSpace1.readPointerAtIndex(address).getAddress() & bitmask;
				if (ptr == environ) {
					return true;
				}
			} catch (MemoryAccessException e) {
				// ignore the memory access error and try the next slot
			}
			address += bytesPerPointer;
		}
		return false;
	}
	
	/**
	 * This is like the setJ9DumpData method but is to be used when we fail to parse the file.  It is meant to try to construct the 
	 * Image object with what it was able to get
	 * 
	 * @param e The cause of the error
	 */
	private void _createCoreImageAfterParseError(Exception e)
	{
		Builder builder = null;
		if(_stream == null) {
			//extract directly from the file
			builder = new Builder(_coreFile, _reader, 0, _fileResolvingAgent);
		} else {
			//extract using the data stream
			builder = new Builder(_coreFile, _stream, 0, _fileResolvingAgent);
		}
		_coreFile.extract(builder);
		String osType = builder.getOSType();
		String cpuType = builder.getCPUType();
		String cpuSubType = builder.getCPUSubType();
		long creationTime = builder.getCreationTime();
		_coreImage = new Image(osType, null, cpuType, cpuSubType, 0, 0, creationTime);
		Iterator spaces = builder.getAddressSpaces();
		while (spaces.hasNext())
		{
			ImageAddressSpace addressSpace = (ImageAddressSpace) spaces.next();
			// Set all processes as having invalid runtimes
			for (Iterator processes = addressSpace.getProcesses(); processes.hasNext();) {
				ImageProcess process = (ImageProcess) processes.next();
				process.runtimeExtractionFailed(e);
			}
			_coreImage.addAddressSpace(addressSpace);
		}
	}
}
