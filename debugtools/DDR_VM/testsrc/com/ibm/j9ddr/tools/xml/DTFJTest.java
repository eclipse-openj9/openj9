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
package com.ibm.j9ddr.tools.xml;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.Iterator;
import java.util.logging.Logger;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImageFactory;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaField;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaRuntime;

public abstract class DTFJTest extends XMLComparisonUnitTest {
	private static Logger log = Logger.getLogger("j9ddr.view.dtfj");
	private int indent = 0;
	private FileWriter out = null;
	
	public File parseCoreFilePath(String path) {
		File core = new File(path);
		if(core.exists()) {
			return core;
		}
		throw new IllegalArgumentException("The path specified to the core file " + path + " is not valid");
	}
	
	public abstract ImageFactory getImageFactory();
	
	public JavaRuntime getRuntime(File core) throws Exception {
		ImageFactory factory = getImageFactory();
		
		Image image = factory.getImage(core);
		
		log.finest("Image returned: " + image);
		
		@SuppressWarnings("unchecked")
		Iterator addressSpaceIt = image.getAddressSpaces();
		while (addressSpaceIt.hasNext()) {
			Object asObj = addressSpaceIt.next();
			
			if (asObj instanceof CorruptData) {
				log.warning("Corrupt AddressSpace returned: " + asObj);
			} else if (asObj instanceof ImageAddressSpace) {
				ImageAddressSpace as = (ImageAddressSpace)asObj;
				
				log.finest("Address Space: " + as + " found");
				
				@SuppressWarnings("unchecked")
				Iterator processIterator = as.getProcesses();
				
				while (processIterator.hasNext()) {
					Object processObj = processIterator.next();
					
					if (processObj instanceof CorruptData) {
						log.warning("Corrupt ImageProcess returned: " + asObj);
					} else if (processObj instanceof ImageProcess) {
						ImageProcess process = (ImageProcess)processObj;
						
						log.finest("ImageProcess: " + process + " found");

						@SuppressWarnings("unchecked")
						Iterator runtimeIterator = process.getRuntimes();
						
						while (runtimeIterator.hasNext()) {
							Object runtimeObj = runtimeIterator.next();
							
							if (runtimeObj instanceof CorruptData) {
								log.warning("Corrupt ImageProcess returned: " + asObj);
							} else if (runtimeObj instanceof JavaRuntime) {
								JavaRuntime runtime = (JavaRuntime)runtimeObj;
								
								log.finer("JavaRuntime found: " + runtime + ", was loaded by " + runtime.getClass().getClassLoader());
								return runtime;
							} else {
								throw new ClassCastException("Unexpected type from Runtime iterator: " + runtimeObj.getClass() + ": " + runtimeObj);
							}
						}
					} else {
						throw new ClassCastException("Unexpected type from Process iterator: " + processObj.getClass() + ": " + processObj);
					}
				}
				
			} else {
				throw new ClassCastException("Unexpected type from AddressSpace iterator: " + asObj.getClass() + ": " + asObj);
			}
		}
		throw new RuntimeException("Could not find a Java Runtime");
	}
	
	protected void writeIndent() throws Exception {
		for(int i = 0; i < indent; i++) {
			out.write("\t");
		}
	}
	
	protected void createWriter(File path) throws IOException {
		out = new FileWriter(path);
	}
	
	protected void startTag(String data) throws Exception {
		indent++;
		writeIndent();
		out.write(data);
	}
	
	protected void endTag(String data) throws Exception {
		writeIndent();
		out.write(data);
		indent--;
	}
	
	protected void tag(String data) throws Exception {
		indent++;
		writeIndent();
		out.write(data);
		indent--;
	}
	
	protected void write(String data) throws Exception {
		out.write(data);
	}
	
	protected void closeWriter() throws Exception {
		out.close();
	}
	
	@SuppressWarnings("unchecked")
	protected void writeObject(JavaObject obj, long index) throws Exception {
		startTag("<object");
		write(" index=\"" + index + "\"");
		write(" address=\"0x" + Long.toHexString(obj.getID().getAddress()) + "\"");
		write(" size=\"0x" + Long.toHexString(obj.getSize()) + "\"");
		write(">\n");
		writeClass(obj.getJavaClass());
		tag("<toString>" + obj.toString() + "</toString>\n");
		tag("<persistentHashCode>" + obj.getPersistentHashcode() + "</persistentHashCode>\n");
		if(obj.isArray()) {
			tag("<array elementCount=\"0x" + Long.toHexString(obj.getArraySize()) + "\" />\n");
		}
		for(Iterator sections = obj.getSections(); sections.hasNext(); ) {
			ImageSection section = (ImageSection) sections.next();
			writeSection(section);
		}
		endTag("</object>\n");
	}
	
	@SuppressWarnings("unchecked")
	protected void writeClass(JavaClass clazz) throws Exception {
		startTag("<class name=\"" + clazz.getName() + "\">\n");
		if(clazz.getSuperclass() == null) {
			tag("<super>null</super>\n");
		} else {
			startTag("<super>\n");
			writeClass(clazz.getSuperclass());
			endTag("</super>\n");
		}
		startTag("<fields>\n");
		for(Iterator i = clazz.getDeclaredFields(); i.hasNext(); ) {
			writeField((JavaField) i.next());
		}
		endTag("</fields>\n");
		endTag("</class>\n");
	}
	
	protected void writeField(JavaField field) throws Exception {
		tag("<field name=\"" + getSafeString(field.getName()) + "\" signature=\"" + field.getSignature() + "\" />\n");
	}
	
	private String getSafeString(String data) {
		String result = data.replace('<', '-');
		return result.replace('<', '-');
	}
	
	protected void writeSection(ImageSection section) throws Exception {
		startTag("<section address=\"0x" + section.getBaseAddress().toString() + "\"");
		write(" size=\"0x" + Long.toHexString(section.getSize()) + "\"" );
		write(" name=\"" + section.getName());
		endTag("\" />\n" );
	}
}
