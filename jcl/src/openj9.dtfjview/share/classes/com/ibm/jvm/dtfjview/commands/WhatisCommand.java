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
package com.ibm.jvm.dtfjview.commands;

import java.io.PrintStream;
import java.util.Iterator;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaHeap;
import com.ibm.dtfj.java.JavaMethod;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;
import com.ibm.jvm.dtfjview.commands.helpers.Utils;

@DTFJPlugin(version="1.*", runtime=false)
public class WhatisCommand extends BaseJdmpviewCommand {

	{
		addCommand("whatis", "[hex address]", "gives information about what is stored at the given memory address");	
	}
	
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		if(args.length == 0) {
			out.println("missing address argument");
			return;
		}
		Long lAddressInDecimal = Utils.longFromString(args[0]);
		if (null == lAddressInDecimal){
			out.println("cannot convert \"" + args[0] + "\" to numeric value"); 
			return;
		}
		long addressInDecimal = lAddressInDecimal.longValue();
		
		findInRuntime(addressInDecimal);
		
		out.print("\n");
	}
	
	private void findInRuntime(long address)
	{
		JavaHeap jh;
		Iterator itHeap = ctx.getRuntime().getHeaps();
		int count = 1;
		
		while (itHeap.hasNext()) {
			jh = (JavaHeap)itHeap.next();
			
			out.print("\theap #" + count + " - name: ");
			out.print(jh.getName() + "\n");
			
			findInHeap(jh, address);
			count++;
		}
	}
	
	private void findInHeap(JavaHeap jh, long address)
	{
		//TODO: checkWithinValidMemRange(sb, ...);
		
		if (isWithinImageSections(jh.getSections(), null, false, address)){
			//if it's start or within the range of an object
			if (!isStartOfObj(jh.getObjects(), address)){
				if (!isWithinObjectRange(jh.getObjects(), address)){
					out.print("\t\t0x" + Long.toHexString(address) + " is orphaned on the heap.\n");
				}
			}
		} else {
			out.print("\t\t0x" + Long.toHexString(address) + " is not within this heap.\n");
			//TODO : function to indicate 16 or 32 bit 
			long bound = 12; //bounds default to 16 for 32 bit system.
			checkClassInRange(jh.getObjects(), bound, address);
			checkMethodInRange(jh.getObjects(), address);
		}
	}
	
	
	private void checkMethodInRange(Iterator objects, long address){
		while(objects.hasNext()){
			JavaObject jObject = (JavaObject)objects.next();
			JavaClass jClass;
			try{
				jClass = jObject.getJavaClass();
			}catch(CorruptDataException cde){
				//go to the next iteration
				continue;
			}
			Iterator methods = jClass.getDeclaredMethods();
			while(methods.hasNext()){
				JavaMethod jMethod = (JavaMethod)methods.next();
				Iterator bytecodeSections = jMethod.getBytecodeSections();
				Iterator compiledSections = jMethod.getCompiledSections();
				if (isWithinImageSections(bytecodeSections, jMethod, false, address)) {
					return; // found it, we are done
				}
				if (isWithinImageSections(compiledSections, jMethod, true, address)) {
					return; // found it, we are done
				}
			}
		}
	}

/*	TODO: need to implement some output for this method and use it
	private void checkMonitorInRange(Iterator monitors){
		while(monitors.hasNext()){
			JavaMonitor jMonitor = (JavaMonitor)monitors.next();
			//TODO: need API for retrieving monitor ID (address)
			//sb.append("\n" + "monitor object id: " + jMonitor.getObject().getID().getAddress());
		}
	}
*/
	
	private void checkClassInRange(Iterator objects, long bound, long address){
		long startAddress, endAddress ;
		while(objects.hasNext()){
			JavaObject jObject = (JavaObject)objects.next();
			JavaClass jClass;
			String className;
			try{
				jClass = jObject.getJavaClass();
				className = jClass.getName();
			}catch(CorruptDataException cde){
				//TODO exception handling
				continue;
			}
			startAddress = jClass.getID().getAddress();
			endAddress = startAddress + bound;
			if(address == startAddress){
				out.print("\t0x" + Long.toHexString(address) +
						" is the address of the java/lang/Class object for " + className);
				return;
			}
			if (isWithinRange(startAddress, endAddress, address)){
				out.print("0x" + Long.toHexString(address) 
						+ " is within the java/lang/Class object for " + className);
				return;
			}
		}
	}
	
	private boolean isWithinObjectRange(Iterator objects, long address){
		long startAddress, endAddress ;
		String className;
		while(objects.hasNext()){
			JavaObject jObject = (JavaObject)objects.next();
			try{
				className = jObject.getJavaClass().getName();
				startAddress = jObject.getID().getAddress();
				endAddress = startAddress + jObject.getSize();
			}catch(CorruptDataException cde){
				//TODO exception handling
				continue;
			}
			if (isWithinRange(startAddress, endAddress, address)){
				out.print("\t\t0x" + Long.toHexString(address) + " is within an object on the heap:\n" + 
						"\t\t\toffset " + (address - startAddress) + " within "+ className +
						" instance @ 0x" + Long.toHexString(startAddress) + "\n");
				return true;
			}
		}
		return false;
	}
	
	private boolean isWithinRange(long startAddress, long endAddress, long address){
		return (address <= endAddress && address > startAddress);
	}
	
	private boolean isStartOfObj(Iterator objects, long address){
		String className;
		long corruptObjectCount = 0;
		
		while(objects.hasNext()){
			Object obj = objects.next();
			if (obj instanceof CorruptData) {
				corruptObjectCount++;
				continue;
			}
			JavaObject jObject = (JavaObject)obj;
			if (address == jObject.getID().getAddress()){
				try{
					className = jObject.getJavaClass().getName();
				} catch(CorruptDataException cde){
					className = "<corrupt class name>";
				}
				out.print("\t\t0x" + Long.toHexString(address) + " is the start of an object of type " + className);
				return true;
			}
		}
		if (corruptObjectCount > 0) {
			out.println("\t\t[skipped " + corruptObjectCount + " corrupt object(s) in heap]");
		}
		return false;
	}
	
	private boolean isWithinImageSections(Iterator heapImageSections, Object memType,
	  boolean isMethodCompiled, long address)
	{
		while (heapImageSections.hasNext()){
			ImageSection imageSection = (ImageSection)heapImageSections.next();
			long baseAddress = imageSection.getBaseAddress().getAddress();
			long size = imageSection.getSize();
			long endAddress = baseAddress + size;
			
			if (address <= endAddress  && address >= baseAddress) {
				if (null == memType) {
					out.print("\t\t0x" + Long.toHexString(address) + " is within heap segment: " +  
							Long.toHexString(baseAddress) + " -- " + Long.toHexString(endAddress) + "\n");
					return true;
				}
				if (memType instanceof JavaMethod) {
					String methodName = "N/A", methodSig = "N/A", className = "N/A";
					try{
						methodName = ((JavaMethod)memType).getName();
						methodSig = ((JavaMethod)memType).getSignature();
						className = ((JavaMethod)memType).getDeclaringClass().getName();
					}catch(CorruptDataException cde){				
					}catch(DataUnavailable du){
					}
					String codeType = isMethodCompiled ? "compiled code" : "byte code";
					out.print("\t0x" + Long.toHexString(address) + " is within the " + codeType + " range: " +  
							Long.toHexString(baseAddress) + " -- " + Long.toHexString(endAddress) + "\n\t" +  "...of method " +
							methodName + " with signature " + methodSig + "\n\t" + 
							"...in class " + className + "\n");
					return true;
				}
			}
		}
		return false;
	}

	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println("gives information about what is stored at the given memory address\n\n" +
				"parameters: <hex_address>\n\n" +
				"the whatis command examines the memory location at <hex_address> and tries to find out more information about this address " +
				"- for example whether it is within an object in a heap or within the byte codes associated with a class method."
				);
	}
}
