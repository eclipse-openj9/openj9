/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2008, 2018 IBM Corp. and others
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

import static com.ibm.jvm.dtfjview.SessionProperties.VERBOSE_MODE_PROPERTY;

import java.io.BufferedOutputStream;
import java.io.DataOutputStream;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintStream;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.reflect.Modifier;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DTFJException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaField;
import com.ibm.dtfj.java.JavaHeap;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaReference;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;
import com.ibm.jvm.dtfjview.heapdump.HeapDumpFormatter;
import com.ibm.jvm.dtfjview.heapdump.HeapDumpSettings;
import com.ibm.jvm.dtfjview.heapdump.LongListReferenceIterator;
import com.ibm.jvm.dtfjview.heapdump.ReferenceIterator;
import com.ibm.jvm.dtfjview.heapdump.classic.ClassicHeapDumpFormatter;
import com.ibm.jvm.dtfjview.heapdump.portable.PortableHeapDumpFormatter;

/**
 * Command for dumping heapdumps from DTFJ.
 * 
 * Contains the heap-walking logic for building the reference tree. The code for writing the heapdumps
 * (both PHD and classic) is in the com.ibm.jvm.heapdump package.
 * @author andhall
 *
 */
@DTFJPlugin(version="1.*",runtime=false)
public class HeapdumpCommand extends BaseJdmpviewCommand
{
	public static final String COMMAND_NAME = "heapdump";
	public static final String DESCRIPTION = "generates a PHD or classic format heapdump";
	public static final String LONG_DESCRIPTION = "Parameters: [heapname+]\n\n"
			+ "\t[heapname+] - space-separated name of heap or heaps to dump. Use \"info heap\" to get the list of heap names. Default: all heaps are dumped.\n\n"
			+ "Writes a heapdump from the memory image.\n"
			+ "The file name and format are controlled using the \"set heapdump\" command; the current settings "
			+ "can be displayed using \"show heapdump\".\n";
		
	private static final String PROTECTION_DOMAIN_FIELD_NAME = "protectionDomain";
	/**
	 * Regexp pattern used to extract a subset of the versions string
	 */
	private static final Pattern J9_VERSION_PATTERN = Pattern.compile("(IBM J9 VM.*?\\))");
	//Do not change the order this array - the indexes are used to extract type codes in the getPrimitiveTypeCode method
	private static final String[] PRIMITIVE_TYPES = { "boolean", "char",
			"float", "double", "byte", "short", "int", "long", "void" };
	private int _numberOfObjects = 0;
	private int _numberOfClasses = 0;
	private int _numberOfErrors = 0;
	private boolean _verbose = false;
	private boolean _is32BitHash;

	{
		addCommand(COMMAND_NAME, "", DESCRIPTION);	
	}
	
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		doCommand(args);
	}

	public void doCommand(String[] args)
	{
		Set heapsToDump = new HashSet();
		
		_numberOfObjects = 0;
		_numberOfErrors = 0;
		_numberOfClasses = 0;
		
		if (ctx.hasPropertyBeenSet(VERBOSE_MODE_PROPERTY)) {
			_verbose = true;
		}
		
		JavaRuntime runtime = ctx.getRuntime();
			
		while( runtime != null ) {
			ImageAddressSpace addressSpace = null;
			try {
				addressSpace = runtime.getJavaVM().getAddressSpace();
			} catch (CorruptDataException e) {
			}

			if(addressSpace == null) {
				out.println("Couldn't get handle on address space");
				break;
			}

			if(! heapArgumentsAreValid(runtime,heapsToDump)) {
				break;
			}

			String version = getVersionString(runtime);
			if (version.contains("IBM J9 2.3") || version.contains("IBM J9 2.4") || version.contains("IBM J9 2.5")) {
				// J9 JVMs versions prior to 2.6 have 16-bit hashcodes, later JVMs have 32-bit hashcodes
				_is32BitHash = false; 
			} else {
				_is32BitHash = true;
			}
			
			boolean is64Bit = addressSpace.getCurrentProcess().getPointerSize() == 64;

			String filename = HeapDumpSettings.getFileName(ctx.getProperties());
			boolean phdFormat = HeapDumpSettings.areHeapDumpsPHD(ctx.getProperties());

			try {
				if(HeapDumpSettings.multipleHeapsInMultipleFiles(ctx.getProperties())) {
					dumpMultipleHeapsInSeparateFiles(runtime,version,is64Bit,phdFormat,filename,heapsToDump);
				} else {
					dumpMultipleHeapsInOneFile(runtime,version,is64Bit,phdFormat,filename,heapsToDump);
				}

				if(_numberOfErrors == 0) {
					out.print("\nSuccessfully wrote " + _numberOfObjects + " objects and " + _numberOfClasses + " classes\n");
				} else {
					out.print("\nWrote " 
							+ _numberOfObjects 
							+ " objects and " 
							+ _numberOfClasses 
							+ " classes and encountered " 
							+ _numberOfErrors 
							+ " errors."
							+ "\n");
				}
			}
			catch (IOException ex) {
				out.println("I/O error writing dump:\n");
				StringWriter writer = new StringWriter();
				ex.printStackTrace(new PrintWriter(writer));
				out.println(writer.toString());
			}
			
			runtime = null;
		}
	}
	
	/**
	 * Checks the list of heaps to dump as specified by the user.
	 * @param runtime Current java runtime
	 * @param heapsToDump Set of strings the user passed as heaps to dump
	 * @return True if all the names are valid heaps, false otherwise
	 */
	private boolean heapArgumentsAreValid(JavaRuntime runtime, Set heapsToDump)
	{
		if(heapsToDump.size() == 0) {
			return true;
		}
		
		Set workingSet = new HashSet();
		workingSet.addAll(heapsToDump);
		
		Iterator heapIt = runtime.getHeaps();
		
		while(heapIt.hasNext()) {
			Object potential = heapIt.next();
			
			if(potential instanceof JavaHeap) {
				JavaHeap thisHeap = (JavaHeap)potential;
				
				workingSet.remove(thisHeap.getName());
			} else if (potential instanceof CorruptData) {
				reportError("Corrupt heap found. Address = " + ((CorruptData)potential).getAddress(),null);
				_numberOfErrors++;
			} else {
				_numberOfErrors++;
				reportError("Unexpected type " + potential.getClass().getName() + " found in heap iterator",null);
			}
		}
		
		if(workingSet.isEmpty()) {
			return true;
		} else {
			StringBuffer buffer = new StringBuffer();
			buffer.append("These specified heaps do not exist:\n");
			
			Iterator nameIterator = workingSet.iterator();
			
			while(nameIterator.hasNext()) {
				buffer.append("\t\t" + nameIterator.next() + "\n");
			}
			
			buffer.append("\tUse \"info heap\" to see list of heap names");
			
			out.println(buffer.toString());
			
			return false;
		}
	}

	/**
	 * DTFJ for dumps prior to JVM 2.6 provides multi-line version information. This method extracts the second line, eg:
	 *		"IBM J9 VM(JRE 1.6.0 IBM J9 2.4 Windows 7 amd64-64 jvmwa6460sr17-20140620_203740 (JIT enabled, AOT enabled)"
	 * DTFJ for dumps from JVM 2.6 and later provide just a single-line version, for example:
	 *		"JRE 1.6.0 Windows 7 amd64-64 build 20111101_93964 (pwa6460_26sr1-20111103_01(SR1))"
	 */
	private String getVersionString(JavaRuntime runtime)
	{
		try {
			String rawVersion = runtime.getVersion();
			
			Matcher matcher = J9_VERSION_PATTERN.matcher(rawVersion);
			
			if(matcher.find()) { // dump prior to JVM 2.6, extract single line version string 
				String minimalVersion = matcher.group(1);
				return minimalVersion;
			} else { // dump from JVM 2.6 or later, return unchanged version string
				return rawVersion;
			}
		}
		catch (CorruptDataException e) {
			_numberOfErrors++;
			out.println("Could not read version string from dump: data corrupted at "
							+ e.getCorruptData().getAddress());
			return "*Corrupt*";
		}
	}
	
	private void dumpMultipleHeapsInOneFile(JavaRuntime runtime,
			String version, boolean is64Bit, boolean phdFormat, String filename, Set heapsToDump) throws IOException
	{
		Iterator heapIterator = runtime.getHeaps();
		
		HeapDumpFormatter formatter = getFormatter(filename, version, is64Bit, phdFormat);
		
		out.println("Writing " + ( phdFormat ? "PHD" : "Classic") + " format heapdump into " + filename);
		

		
		while (heapIterator.hasNext()) {
			Object thisHeapObj = heapIterator.next();

			if (thisHeapObj instanceof CorruptData) {
				out.println("Corrupt heap data found at: " 
						+ ((CorruptData) thisHeapObj).getAddress());
				_numberOfErrors++;
				continue;
			}

			JavaHeap thisHeap = (JavaHeap) thisHeapObj;
			
			if(heapsToDump.size() > 0 && ! heapsToDump.contains(thisHeap.getName())) {
				continue;
			}
			
			dumpHeap(formatter, thisHeap);
		}
		
		dumpClasses(formatter,runtime);
		
		formatter.close();
	}

	private void dumpMultipleHeapsInSeparateFiles(JavaRuntime runtime,String version, boolean is64Bit, boolean phdFormat,String baseFileName, Set heapsToDump) throws IOException
	{
		Iterator heapIterator = runtime.getHeaps();
		
		HeapDumpFormatter formatter = null;
		
		while (heapIterator.hasNext()) {
			Object thisHeapObj = heapIterator.next();

			if (thisHeapObj instanceof CorruptData) {
				out.println("Heap corrupted at: "
						+ ((CorruptData) thisHeapObj).getAddress());
				_numberOfErrors++;
				continue;
			}

			JavaHeap thisHeap = (JavaHeap) thisHeapObj;

			// Create a new heapdump formatter for every heap we find
			if (formatter != null) {
				formatter.close();
			}

			if(heapsToDump.size() > 0 && ! heapsToDump.contains(thisHeap.getName())) {
				continue;
			}
			
			String fileName = getFileNameForHeap(thisHeap,baseFileName);

			out.print("Writing " 
					+ ( phdFormat ? "PHD" : "Classic") 
					+ " format heapdump for heap " 
					+ thisHeap.getName() 
					+ " into " 
					+ fileName + "\n");
			
			formatter = getFormatter(fileName, version, is64Bit, phdFormat);
			
			//We have to dump classes in every heapdump
			dumpClasses(formatter,runtime);
			
			dumpHeap(formatter, thisHeap);
		}
		
		if(formatter != null) {
			formatter.close();
		}
	}

	/**
	 * Walks the runtime classes and passes them through the formatter interface
	 */
	private void dumpClasses(HeapDumpFormatter formatter, JavaRuntime runtime) throws IOException
	{
		Iterator classLoaderIt = runtime.getJavaClassLoaders();
		
		int numberOfClasses = 0;
		
ITERATING_LOADERS:while(classLoaderIt.hasNext()) {
			Object potential = classLoaderIt.next();
			
			if(potential instanceof CorruptData) {
				_numberOfErrors++;
				reportError("CorruptData found in classloader list at address: " + ((CorruptData)potential).getAddress(), null);
				continue ITERATING_LOADERS;
			}
			
			JavaClassLoader thisClassLoader = (JavaClassLoader)potential;
			
			Iterator classesIt = thisClassLoader.getDefinedClasses();
			
ITERATING_CLASSES:while(classesIt.hasNext()) {
				potential = classesIt.next();
				
				numberOfClasses++;
				
				try {
					
					if(potential instanceof CorruptData) {
						_numberOfErrors++;
						reportError("CorruptData found in class list for classloader "
								+ Long.toHexString(thisClassLoader.getObject().getID().getAddress()) 
								+ " at address: " + ((CorruptData)potential).getAddress(), null);
						continue ITERATING_CLASSES;
					}

					JavaClass thisJavaClass = (JavaClass)potential;
					
					JavaClass superClass = thisJavaClass.getSuperclass();
					
					JavaObject classObject = thisJavaClass.getObject();
					
					long instanceSize;
					if(thisJavaClass.isArray()) {
						instanceSize = 0;
					} else {
						instanceSize = thisJavaClass.getInstanceSize();
					}
					
					int hashcode = 0;
					if (_is32BitHash) { // JVMs from 2.6 on, optional 32-bit hashcodes, if object was hashed 
						try {
							hashcode = classObject != null ? (int)classObject.getPersistentHashcode() : 0;
						} catch (DataUnavailable ex) {
							// no persistent hashcode for this object, pass hashcode=0 to the heapdump formatter
						}
					} else { // JVMs prior to 2.6, all objects should have a 16-bit hashcode
						hashcode = classObject != null ? (int)classObject.getHashcode() : 0;
					}
					
					formatter.addClass(classObject.getID().getAddress(),
								thisJavaClass.getName(), 
								superClass != null ? superClass.getID().getAddress() : 0,
								classObject != null ? (int)classObject.getSize() : 0,
								instanceSize,
								hashcode,
								getClassReferences(thisJavaClass) );
				} catch(DTFJException ex) {
					//Handle CorruptDataException and DataUnavailableException the same way
					_numberOfErrors++;
					reportError(null,ex);
					continue ITERATING_CLASSES;
				}
			}
		}
		
	   _numberOfClasses = numberOfClasses;
	   if((pdSkipCount > 0) && _verbose) {
		   out.println("Warning : The protection domain information was not available for " + pdSkipCount + " classes");
	   }
	}

	/**
	 * Walks the supplied heap and passes the artifacts through the formatter
	 */
	private void dumpHeap(HeapDumpFormatter formatter, JavaHeap thisHeap)
			throws IOException
	{
		Iterator objectIterator = thisHeap.getObjects();

		while (objectIterator.hasNext()) {
			Object next = objectIterator.next();
			_numberOfObjects++;

			if (next instanceof CorruptData) {
				_numberOfErrors++;
				reportError("Corrupt object data found at " + ((CorruptData)next).getAddress() + " while walking heap " + thisHeap.getName(),null);
				continue;
			}

			try {
				JavaObject thisObject = (JavaObject) next;
				if (thisObject.getJavaClass().getName().equals("java/lang/Class")) {
					// heap classes are handled separately, in dumpClasses()
					continue;
				}				
				JavaClass thisClass = thisObject.getJavaClass();
				JavaObject thisClassObject = thisClass.getObject();

				int hashcode = 0;
				if (_is32BitHash) { // JVMs from 2.6 on, optional 32-bit hashcodes, if object was hashed 
					try {
						hashcode = (int) thisObject.getPersistentHashcode();
					} catch (DataUnavailable ex) {
						// no persistent hashcode for this object, pass hashcode=0 to the heapdump formatter
					}
				} else { // JVMs prior to 2.6, all objects should have a 16-bit hashcode
					try {
						hashcode = (int) thisObject.getHashcode();
					} catch (DataUnavailable ex) {
						_numberOfErrors++;
						reportError("Failed to get hashcode for object: " + thisObject.getID(),ex);
					}
				}

				if (thisObject.isArray()) {
					if (isPrimitive(thisClass.getComponentType())) {
						formatter.addPrimitiveArray(thisObject.getID().getAddress(), 
													thisClassObject.getID().getAddress(),
													getPrimitiveTypeCode(thisClass.getComponentType()),
													thisObject.getSize(), 
													hashcode,
													thisObject.getArraySize());
					} else {
						formatter.addObjectArray(thisObject.getID().getAddress(), 
								thisClassObject.getID().getAddress(), 
								thisClass.getName(), 
								thisClass.getComponentType().getObject().getID().getAddress(),
								thisClass.getComponentType().getName(), 
								thisObject.getSize(),
								thisObject.getArraySize(),
								hashcode, 
								getObjectReferences(thisObject));
					}
				}
				else {
					formatter.addObject(thisObject.getID().getAddress(), 
										thisClassObject.getID().getAddress(), 
										thisClass.getName(), 
										(int)thisObject.getSize(),
										hashcode, 
										getObjectReferences(thisObject));
				}
			}
			catch (CorruptDataException ex) {
				_numberOfErrors++;
				reportError(null,ex);
				continue;
			}
		}
	}

	/* Reference code reimplemented (rather than using the DTFJ getReferences() API)
	 * because we are trying to match the behaviour of the runtime heapdump rather than
	 * the GC spec. The set of references we're trying to create is different.
	 */

	/**
	 * Gets the references for the supplied class
	 * 
	 * @param thisJavaClass Class being examined 
	 */
	private ReferenceIterator getClassReferences(JavaClass thisJavaClass)
	{
		List references = new LinkedList();
		
		try {
			// Class object instance references
			addReferences(thisJavaClass.getObject(), references);
			//Statics        
			addStaticReferences(thisJavaClass, references);
			
			addProtectionDomainReference(thisJavaClass,references);
			
			// Constant pool class references
			Iterator constantPoolIt = thisJavaClass.getConstantPoolReferences();
			while (constantPoolIt.hasNext()) {
				Object cpObject = constantPoolIt.next();
				if (cpObject instanceof JavaClass) {
					// Found a class reference, add it to the list
					JavaClass cpJavaClass = (JavaClass)cpObject;
					references.add(Long.valueOf(cpJavaClass.getObject().getID().getAddress()));
				}
			}
					
			// Superclass references
			JavaClass superClass = thisJavaClass.getSuperclass();
			while (null != superClass){
				references.add(Long.valueOf(superClass.getObject().getID().getAddress()));
				superClass = superClass.getSuperclass();
			}
			
			//Classloader
			JavaClassLoader loader = thisJavaClass.getClassLoader();
			
			if(loader != null) {
				JavaObject loaderObject = loader.getObject();
				if(loaderObject != null) {
					references.add(Long.valueOf(loaderObject.getID().getAddress()));
				} else {
					reportError("Null loader object returned for class: " + thisJavaClass.getName() + "(" + thisJavaClass.getID() + ")",null);
					_numberOfErrors++;
				}
			} else {
				reportError("Null classloader returned for class: " + thisJavaClass.getName() + "(" + thisJavaClass.getID() + ")",null);
				_numberOfErrors++;
			}
		
		} catch(DTFJException ex) {
			reportError(null,ex);
			_numberOfErrors++;
		}
		
		return new LongListReferenceIterator(references);
	}
	
	private long pdSkipCount = 0;
	
	private void addProtectionDomainReference(JavaClass thisJavaClass,
			List references) throws CorruptDataException, MemoryAccessException
			
	{
		try {
			JavaObject protectionDomain = thisJavaClass.getProtectionDomain();
			if(protectionDomain != null) {
				references.add(Long.valueOf(protectionDomain.getID().getAddress()));
			}
		} catch (DataUnavailable e) {
			//record that access to the protection domain was not possible
			pdSkipCount++;
		}
	}

	/**
	 * Extracts static references from class
	 * @param thisClass Class being examined
	 * @param references List to add references to
	 */
	private void addStaticReferences(JavaClass thisClass, List references)
			throws CorruptDataException, MemoryAccessException
	{
		Iterator fieldsIt = thisClass.getDeclaredFields();
			
		while(fieldsIt.hasNext()) {
			Object potential = fieldsIt.next();
				
			if(potential instanceof CorruptData) {
				reportError("Corrupt field found in class "
						+ thisClass.getName()
						+ "(" + thisClass.getID() + ") at "
						+ ((CorruptData)potential).getAddress()
						,null);
				_numberOfErrors++;
				continue;
			}
				
			JavaField field = (JavaField) potential;
				
			if(! Modifier.isStatic(field.getModifiers())) {
				continue;
			}
				
			Object referent = field.get(thisClass.getObject());
					
			if(referent instanceof CorruptData) {
				_numberOfErrors++;
				reportError("Corrupt referent found in class "
						+ thisClass.getName()
						+ "(" + thisClass.getID() + ") from field "
						+ field.getName()
						+ " at address "
						+ ((CorruptData)potential).getAddress()
						,null);
			} else if (referent instanceof JavaObject) {
				JavaObject referredObject = (JavaObject) referent;
					
				references.add(Long.valueOf(referredObject.getID().getAddress()));
			} else if (referent == null) {
				references.add(Long.valueOf(0));
			} else if (referent instanceof Number || referent instanceof Boolean || referent instanceof Character) {
				//Ignore
			} else {
				reportError("Unexpected type: " 
						+ referent.getClass().getName() 
						+ " returned from field " 
						+ field.getName() 
						+ " from class "
						+ thisClass.getName()
						+ "(" + thisClass.getID()+ ")"
						,null);
				_numberOfErrors++;
			}
		}
	}
	
	/**
	 * Gets instance references for objects
	 * @param thisObject Object being examined
	 * @return Iterator of references
	 */
	private ReferenceIterator getObjectReferences(JavaObject thisObject)
	{
		List<Long> references = new LinkedList<Long>();

		try {
			addReferences(thisObject, references);
			if(thisObject.getJavaClass().isArray()) {
				/**
				 * Reverse the order of the elements for an object array.
				 *
				 * In a classic heapdump or portable heapdump from the vm, the array elements are always 
				 * in reverse order (i.e. the opposite of index order) because for historical reasons
				 * that is the order in which the gc iterator returns them. 
				 * Therefore reverse the order before writing them out so that the heapdump
				 * that we generate looks as close as possible to one from the vm.
				 * <p>  
				 * See CMVC 193691
				 */
				Long[] refsAsArray = references.toArray(new Long[0]); // Java idiom to dump a list to an array
				references.clear();

				for (int i = refsAsArray.length - 1; i >= 0; i--) {
					references.add(refsAsArray[i]);
				}			    	
			}
		} catch(DTFJException ex) {
			_numberOfErrors++;
			reportError(null,ex);
		}

		return new LongListReferenceIterator(references);
	}

	/**
	 * Extracts the instance references from an object
	 * @param object Object being walked
	 * @param references List<Long> to add references to
	 * @param thisClass Class of object
	 */
	private void addReferences(JavaObject object,
			List<Long> references) throws CorruptDataException,
			MemoryAccessException
	{
		Iterator it = object.getReferences();
		Object ref = null;
		
		// discard the first reference which is always to the class
		if (it.hasNext()) { // test hasNext() to be on the safe side.
			ref = it.next();
		}
		
		while (it.hasNext()) {			
			ref = it.next();
			if(ref instanceof CorruptData) {
				// can sometimes get a nasty surprise in the list - e.g. a J9DDRCorruptData
				_numberOfErrors++;
				reportError("Corrupt data found at address " 
						+ ((CorruptData)ref).getAddress() 
						+ " getting references from object at address: "
						+ Long.toHexString(object.getID().getAddress())
						+ " of class "
						+ object.getJavaClass().getName() 
						+ "(" + object.getJavaClass().getID() + ")"
						,null);
				continue;
			}
			if ( ! (ref instanceof JavaReference)) {
				_numberOfErrors++;
				reportError("Object of unexpected type "
						+ ref.getClass() 
						+ " found within references from object at address: "
						+ object.getID().getAddress()
						+ " of class "
						+ object.getJavaClass().getName() 
						+ "(" + object.getJavaClass().getID() + ")"
						,null);
				continue;
			} else {
				Object target;
				try {
					target = ((JavaReference)ref).getTarget();
				} catch (DataUnavailable e) {
					_numberOfErrors++;
					reportError("DataUnavailable thrown from call to getTarget() on reference: "
							+ ref
							,null);
					continue;
				}
				// the following ugliness is necessary as JavaObject and JavaClass both support getID() but do not inherit from a common parent
				if (target instanceof JavaObject) {
					references.add(Long.valueOf(((JavaObject) target).getID().getAddress()));				
				} else if (target instanceof JavaClass) {
					references.add(Long.valueOf(((JavaClass) target).getID().getAddress()));
				} else {
					_numberOfErrors++;
					reportError("Object of unexpected type "
							+ target.getClass() 
							+ " returned from call to getTarget() on reference "
							+ ref
							,null);
				}
			}
		}
	}

	/**
	 * Checks if class is primitive
	 * 
	 * @param clazz
	 *            Class under test
	 * @return True if clazz represents a primitive type, false otherwise
	 */
	private static boolean isPrimitive(JavaClass clazz)
			throws CorruptDataException
	{
		String name = clazz.getName();

		/* Fastpath, anything containing a / cannot be primitive */
		if (name.indexOf("/") != -1) {
			return false;
		}

		for (int i = 0; i != PRIMITIVE_TYPES.length; i++) {
			if (PRIMITIVE_TYPES[i].equals(name)) {
				return true;
			}
		}

		return false;
	}

	/**
	 * Converts a class into a primitive type code (as used by the HeapdumpFormatter interface). 
	 * @param clazz
	 * @return
	 */
	private int getPrimitiveTypeCode(JavaClass clazz)
			throws CorruptDataException
	{
		String name = clazz.getName();

		for (int i = 0; i != PRIMITIVE_TYPES.length; i++) {
			if (PRIMITIVE_TYPES[i].equals(name)) {
				return i;
			}
		}

		throw new IllegalArgumentException("Class: " + name
				+ " is not primitive");
	}

	/**
	 * Factory method for HeapDumpFormatters
	 * @param fileName File name for output file
	 * @param version Version string
	 * @param is64Bit True if 64 bit
	 * @param phdFormat True if using PhD format
	 */
	private HeapDumpFormatter getFormatter(String fileName, String version,
			boolean is64Bit, boolean phdFormat) throws IOException
	{
		if(phdFormat) {
			DataOutputStream dos = new DataOutputStream(new BufferedOutputStream(new FileOutputStream(fileName)));
			return new PortableHeapDumpFormatter(dos,version,is64Bit,_is32BitHash);
		} else {
			return new ClassicHeapDumpFormatter(new FileWriter(fileName),version,is64Bit);
		}
	}

	/**
	 * Modifies the base file name (e.g. foo.phd) to include the heap name, whilst maintaining
	 * the suffix (so foo.phd becomes foo.immortal.phd).
	 */
	private String getFileNameForHeap(JavaHeap thisHeap, String baseFileName)
	{
		int pointIndex = baseFileName.lastIndexOf(".");
		
		if(pointIndex != -1) {
			return baseFileName.substring(0,pointIndex) + "." + thisHeap.getName() + baseFileName.substring(pointIndex);
		} else {
			return baseFileName + "." + thisHeap.getName();
		}
	}

	/**
	 * Internal error handling routine that only reports the supplied message if verbose was supplied on the command line.
	 */
	private void reportError(String msg,Throwable t) 
	{
		if(!_verbose) {
			return;
		}
		
		if(msg != null) {
			out.println(msg);
		}
		
		if(t != null) {
			StringWriter writer = new StringWriter();
			
			t.printStackTrace(new PrintWriter(writer));
			
			out.println(writer.toString());
		}
	}
	
	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println(LONG_DESCRIPTION);		
	}
}
