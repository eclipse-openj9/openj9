/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.pointer.helper;

import static com.ibm.j9ddr.vm29.structure.J9ROMFieldOffsetWalkState.*;

import java.lang.reflect.Modifier;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

import com.ibm.j9ddr.AddressedCorruptDataException;
import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.InvalidDataTypeException;
import com.ibm.j9ddr.vm29.j9.AlgorithmVersion;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffset;
import com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffsetIterator;
import com.ibm.j9ddr.vm29.pointer.ObjectReferencePointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ArrayClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ITablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VTableHeaderPointer;
import com.ibm.j9ddr.vm29.structure.J9Class;
import com.ibm.j9ddr.vm29.structure.J9Consts;
import com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags;
import com.ibm.j9ddr.vm29.structure.J9Method;
import com.ibm.j9ddr.vm29.structure.J9VTableHeader;
import com.ibm.j9ddr.vm29.types.Scalar;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.UDATA;

public class J9ClassHelper 
{

	private static HashMap<Long, HashMap<String, J9ObjectFieldOffset>> classToFieldOffsetCacheMap = new HashMap<Long, HashMap<String, J9ObjectFieldOffset>>();
	
	private static final Map<String, Character>TYPE_MAP;
	private static final int MAXIMUM_ARRAY_ARITY = 100;
	
	static {
		TYPE_MAP = new HashMap<String, Character>();
		TYPE_MAP.put("void", 'V');
		TYPE_MAP.put("boolean", 'Z');
		TYPE_MAP.put("byte", 'B');
		TYPE_MAP.put("char", 'C');
		TYPE_MAP.put("short", 'S');
		TYPE_MAP.put("int", 'I');
		TYPE_MAP.put("long", 'J');
		TYPE_MAP.put("float", 'F');
		TYPE_MAP.put("double", 'D');
	};
	
	public static boolean isArrayClass(J9ClassPointer clazz) throws CorruptDataException
	{
		return classDepthAndFlags(clazz).anyBitsIn(J9JavaAccessFlags.J9AccClassRAMArray);
	}
	
	public static String getName(J9ClassPointer clazz) throws CorruptDataException
	{
		return J9UTF8Helper.stringValue(clazz.romClass().className()); 
	}
	
	public static String getSignature(J9ClassPointer clazz) throws CorruptDataException 
	{
		String result;
		if (isArrayClass(clazz)) {
			result = getArrayName(clazz);
		} else {			
			String className = getName(clazz);
			Character type = TYPE_MAP.get(className);
			if (type != null) {
				result = type.toString();
			} else {
				result = 'L' + className + ';';
			}			
		}
		return result;
	}

	public static String getArrayName(J9ClassPointer clazz) throws CorruptDataException
	{
		J9ArrayClassPointer arrayClass = J9ArrayClassPointer.cast(clazz);
		
		StringBuilder name = new StringBuilder();
		
		int arity = 0;
		try {
			arity = arrayClass.arity().intValue();
		} catch (InvalidDataTypeException e) {
			throw new AddressedCorruptDataException(arrayClass.getAddress(), "Array arity larger than MAXINT");
		}
		
		if (arity > MAXIMUM_ARRAY_ARITY) {
			//Doubtful
			throw new AddressedCorruptDataException(arrayClass.getAddress(), "Very high arity " + arity + " from array class 0x" + Long.toHexString(arrayClass.getAddress()));
		}
		
		for (int i = 0; i < arity; i++) {
			name.append('[');
		}
		
		String elementClassName = J9ClassHelper.getName(arrayClass.leafComponentType());
		
		Character type = TYPE_MAP.get(elementClassName);
		if (type != null) {
			name.append(type);
		} else {
			name.append('L');
			name.append(elementClassName);
			name.append(";");
		}

		return name.toString();
	}
	
	public static String getJavaName(J9ClassPointer clazz) throws CorruptDataException
	{
		String baseName = getName(clazz);
		char ch = baseName.charAt(0);
		if(ch != '[') {
			return baseName;
		}
		J9ArrayClassPointer arrayClass = J9ArrayClassPointer.cast(clazz);
		int arity = arrayClass.arity().intValue();
		StringBuilder buf = new StringBuilder();
		for(int i = 0; i < arity; i++) {
			buf.append("[]");
		}
		String aritySuffix = buf.toString();

		ch = baseName.charAt(1);
		switch(ch) {
		case 'B': return "byte" + aritySuffix;
		case 'C': return "char" + aritySuffix;
		case 'D': return "double" + aritySuffix;
		case 'F': return "float" + aritySuffix;
		case 'I': return "int" + aritySuffix;
		case 'J': return "long" + aritySuffix;
		case 'S': return "void" + aritySuffix;
		case 'V': return "void" + aritySuffix;
		case 'Z': return "boolean" + aritySuffix;
		
		case 'L': 
			return getName(arrayClass.leafComponentType()) + aritySuffix;
		}
		
		// Should never happen
		return baseName;
	}

	public static String formatFullInteractive(J9ClassPointer clazz) 
	{
		String prefix = clazz.formatFullInteractive();
		String suffix;
		try {
			suffix = String.format("\nClass name: %s\nTo view instance shape, use !j9classshape %s", J9ClassHelper.getName(clazz), clazz.getHexAddress());
		} catch (CorruptDataException e) {
			suffix = String.format("\nClass name: <ERROR>\n To view instance shape, use !j9classshape %s", clazz.getHexAddress());		
		}
		return prefix + suffix;
	}

	public static Iterator<J9ObjectFieldOffset> getFieldOffsets(J9ClassPointer clazz) throws CorruptDataException 
	{
		U32 flags = new U32(J9VM_FIELD_OFFSET_WALK_INCLUDE_STATIC | J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE);
		return J9ObjectFieldOffsetIterator.J9ObjectFieldOffsetIteratorFor(clazz, superclass(clazz), flags);
	}
	
	public static J9ClassPointer superclass(J9ClassPointer clazz) throws CorruptDataException 
	{
		long index = classDepth(clazz).longValue() - 1;
		if (index < 0) {
			return J9ClassPointer.NULL;
		}
		VoidPointer j9ClassInstancePointer = clazz.superclasses().at(index);
		return J9ClassPointer.cast(j9ClassInstancePointer);
	}
	
	private static HashMap<String, J9ObjectFieldOffset> getFieldOffsetCache(J9ClassPointer clazz)
	{
		Long classAddr = new Long(clazz.getAddress());
		HashMap<String, J9ObjectFieldOffset> fieldOffsetCache = classToFieldOffsetCacheMap.get(classAddr);
		
		if(null != fieldOffsetCache) { 
			return fieldOffsetCache;
		} else {
			fieldOffsetCache = new HashMap<String, J9ObjectFieldOffset>();
			classToFieldOffsetCacheMap.put(classAddr, fieldOffsetCache);
			return fieldOffsetCache ;
		}
	}
	
	public static J9ObjectFieldOffset checkFieldOffsetCache(J9ClassPointer clazz, String fieldName, String signature) 
	{
		HashMap<String, J9ObjectFieldOffset> fieldOffsetCache = getFieldOffsetCache(clazz);
		
		return fieldOffsetCache.get(fieldName + "." + signature);
	}
	
	public static void setFieldOffsetCache(J9ClassPointer clazz, J9ObjectFieldOffset offset, String fieldName, String signature) 
	{
		HashMap<String, J9ObjectFieldOffset> fieldOffsetCache = getFieldOffsetCache(clazz);
		
		fieldOffsetCache.put(fieldName + "." + signature, offset);
	}
	
	public static boolean isSameOrSuperClassOf(J9ClassPointer superClazz, J9ClassPointer clazz) throws CorruptDataException
	{
		long superClassDepth = classDepth(superClazz).longValue();
		if(superClazz.eq(clazz)) {
			return true;
		}
		if(classDepth(clazz).longValue() > superClassDepth) {
			return superClazz.eq(clazz.superclasses().at(superClassDepth));
		}
		return false;
	}
	
	public static J9VTableHeaderPointer vTableHeader(J9ClassPointer clazz)
	{
		J9VTableHeaderPointer pointer = J9VTableHeaderPointer.cast(clazz.add(1));
		return pointer;
	}

	public static UDATAPointer vTable(J9VTableHeaderPointer vTableHeader)
	{
		UDATAPointer pointer = UDATAPointer.cast(vTableHeader.add(1));
		return pointer;
	}

	public static UDATAPointer oldVTable(J9ClassPointer clazz)
	{
		UDATAPointer pointer = UDATAPointer.cast(clazz.add(1));
		return pointer;
	}

	public static UDATA size(J9ClassPointer clazz, J9JavaVMPointer vm) throws CorruptDataException 
	{
		/*
		 * Size includes up to 7 fragments:
		 * 		0. RAM class header = J9Class struct + vTable + JIT vTable
		 * 		1. RAM methods + extended method block
		 * 		2. Superclasses
		 * 		3. Instance description
		 * 		4. iTable
		 * 		5. Static slots
		 * 		6. Constant pool
		 * 
		 * Array classes omit 1, 3, 5 and 6.
		 */
		
		// Fragment 0. RAM class header = J9Class struct + vTable + JIT vTable
		UDATA size = new UDATA(J9Class.SIZEOF);
		UDATA vTableSlotCount;
		/* Check vTable algorithm version */
		if (AlgorithmVersion.getVersionOf(AlgorithmVersion.VTABLE_VERSION).getAlgorithmVersion() >= 1) {
			vTableSlotCount = vTableHeader(clazz).size().add(J9VTableHeader.SIZEOF / UDATA.SIZEOF);
		} else {
			vTableSlotCount = oldVTable(clazz).at(0);
		}
		size = size.add(Scalar.convertSlotsToBytes(vTableSlotCount));
		if (vm.jitConfig().notNull()) {
			UDATA jitVTableSlotCount = vTableSlotCount.sub(1);
			size = size.add(Scalar.convertSlotsToBytes(jitVTableSlotCount));
		}

		if (!J9ROMClassHelper.isArray(clazz.romClass())) {
			// Fragment 1. RAM methods + extended method block
			UDATA ramMethodsSize = clazz.romClass().romMethodCount().mult((int)J9Method.SIZEOF);
			size = size.add(ramMethodsSize);
			if (vm.runtimeFlags().allBitsIn(J9Consts.J9_RUNTIME_EXTENDED_METHOD_BLOCK)) {
				UDATA extendedMethodBlockSize = Scalar.roundToSizeofUDATA(new UDATA(clazz.romClass().romMethodCount()));
				size = size.add(extendedMethodBlockSize);
			}
			
			// Fragment 3. Instance description
			if (!clazz.instanceDescription().anyBitsIn(1)) {
				UDATA highestBitInSlot = new UDATA(UDATA.SIZEOF * 8 - 1);
				UDATA instanceDescriptionSize = clazz.totalInstanceSize().rightShift((int) (ObjectReferencePointer.SIZEOF >> 2) + 1);
				instanceDescriptionSize = instanceDescriptionSize.add(highestBitInSlot).bitAnd(highestBitInSlot.bitNot());
				if (J9BuildFlags.gc_leafBits) {
					instanceDescriptionSize = instanceDescriptionSize.mult(2);
				}
				size = size.add(instanceDescriptionSize);
			}

			// Fragment 5. Static slots
			UDATA staticSlotCount = clazz.romClass().objectStaticCount().add(clazz.romClass().singleScalarStaticCount());
			if (J9BuildFlags.env_data64) {
				staticSlotCount = staticSlotCount.add(clazz.romClass().doubleScalarStaticCount());
			} else {
				staticSlotCount = staticSlotCount.add(1).bitAnd(~1L).add(clazz.romClass().doubleScalarStaticCount().mult(2));
			}
			size = size.add(Scalar.convertSlotsToBytes(new UDATA(staticSlotCount)));
			
			// Fragment 6. Constant pool
			UDATA constantPoolSlotCount = clazz.romClass().ramConstantPoolCount().mult(2);
			size = size.add(Scalar.convertSlotsToBytes(new UDATA(constantPoolSlotCount)));
		}
		
		// Fragment 2. Superclasses
		UDATA classDepth = classDepthAndFlags(clazz).bitAnd(J9JavaAccessFlags.J9AccClassDepthMask);
		if (classDepth.eq(0)) {
			// java/lang/Object has a single slot superclasses array
			size = size.add(UDATA.SIZEOF);
		} else {
			size = size.add(Scalar.convertSlotsToBytes(classDepth));
		}
		
		// Fragment 4. iTable
		if (clazz.iTable().notNull()) {
			J9ClassPointer superclass = J9ClassPointer.cast(clazz.superclasses().at(classDepth.sub(1)));
			if (superclass.isNull() || !superclass.iTable().eq(clazz.iTable())) {
				J9ITablePointer iTable = J9ITablePointer.cast(clazz.iTable());
	
				// Scan to the last iTable belonging to classPointer
				if (superclass.isNull()) {
					while (iTable.next().notNull()) {
						iTable = iTable.next();
					}
				} else {
					while (iTable.next().notNull() && !iTable.next().eq(superclass.iTable())) {
						iTable = iTable.next();
					}
				}
				
				// Find the end of the last iTable
				if (clazz.romClass().modifiers().allBitsIn(J9JavaAccessFlags.J9AccInterface)) {
					iTable = iTable.add(1);
				} else {
					iTable = iTable.add(1).addOffset(iTable.interfaceClass().romClass().romMethodCount().mult(UDATA.SIZEOF));
				}
				
				size = size.add(iTable.getAddress() - clazz.iTable().getAddress());
			}
		}
	
		return size;
	}
	
	private static final int SYNTHETIC = 0x1000;
	private static final int ANNOTATION = 0x2000;
	private static final int ENUM = 0x4000;
	
	/**
	 * Returns class modifiers as returned from java.lang.Class.getModifiers()
	 * @param j9class Class to get modifiers for
	 * @return Modifier flags, as returned from java.lang.Class.getModifiers()
	 * @throws CorruptDataException
	 */
	public static int getJavaLangClassModifiers(J9ClassPointer j9class) throws CorruptDataException
	{
		int rawModifiers = getRawModifiers(j9class);
		
		if (J9ClassHelper.isArrayClass(j9class)) {
			rawModifiers &= (Modifier.PUBLIC | Modifier.PRIVATE | Modifier.PROTECTED |
				Modifier.ABSTRACT | Modifier.FINAL);
		} else {
			rawModifiers &= (Modifier.PUBLIC | Modifier.PRIVATE | Modifier.PROTECTED |
							Modifier.STATIC | Modifier.FINAL | Modifier.INTERFACE |
							Modifier.ABSTRACT | SYNTHETIC | ENUM | ANNOTATION);
		}
		
		return rawModifiers;
	}
	
	/**
	 * Returns "raw" modifiers
	 * 
	 * Derived from logic in J9VMJavaLangClassNativesCDLC.getModifiersImpl
	 * @param j9class
	 * @return
	 * @throws CorruptDataException
	 */
	public static int getRawModifiers(J9ClassPointer j9class) throws CorruptDataException
	{
		if (J9ClassHelper.isArrayClass(j9class)) {
			J9ArrayClassPointer arrayClass = J9ArrayClassPointer.cast(j9class);
			
			UDATA modifiers = arrayClass.leafComponentType().romClass().modifiers();

			//OR in the bogus Sun bits
			modifiers = modifiers.bitOr(J9JavaAccessFlags.J9AccAbstract);
			modifiers = modifiers.bitOr(J9JavaAccessFlags.J9AccFinal);
			
			return modifiers.intValue();
		} else {
			UDATA modifiers = j9class.romClass().modifiers();
			
			if (j9class.romClass().outerClassName().notNull()) {
				modifiers = j9class.romClass().memberAccessFlags();
			}
			
			return modifiers.intValue();
		}
	}
	
	public static UDATA classDepthAndFlags(J9ClassPointer j9class) throws CorruptDataException {
		return j9class.classDepthAndFlags();
	}
	
	public static UDATA classDepth(J9ClassPointer j9class) throws CorruptDataException {
		return classDepthAndFlags(j9class).bitAnd(J9JavaAccessFlags.J9AccClassDepthMask);
	}
	
	public static U32 extendedClassFlags(J9ClassPointer j9class) throws CorruptDataException {
		return new U32(j9class.classFlags());
	}

	public static UDATA classFlags(J9ClassPointer j9class) throws CorruptDataException {
		return classDepthAndFlags(j9class);
	}

	public static boolean isObsolete(J9ClassPointer j9class) throws CorruptDataException {
		return classDepthAndFlags(j9class).allBitsIn(J9JavaAccessFlags.J9AccClassHotSwappedOut);
	}

	public static J9ClassPointer currentClass(J9ClassPointer j9class) throws CorruptDataException {
		return isObsolete(j9class) ? j9class.arrayClass() : j9class;
	}
	
	/*
	 * Returns a program space pointer to the matching J9Method for the
	 * specified class and PC.
	 */
	public static J9MethodPointer getMethodFromPCAndClass(J9ClassPointer localClass, U8Pointer pc) throws CorruptDataException 
	{
		J9ROMClassPointer localROMClass = localClass.romClass();
		for (int i = 0; i < localROMClass.romMethodCount().longValue(); i++) {
			J9MethodPointer localMethod = localClass.ramMethods().add(i);
			J9ROMMethodPointer romMethod = J9MethodHelper.romMethod(localMethod);

			boolean a = pc.gte(U8Pointer.cast(romMethod));
			boolean b = pc.lte(J9ROMMethodHelper.bytecodeEnd(romMethod).subOffset(1));
			if (a && b) {
				return localMethod;
			}
		}
		return J9MethodPointer.NULL;
	}
	
	public static boolean isSwappedOut(J9ClassPointer clazz) throws CorruptDataException
	{
		return classDepthAndFlags(clazz).allBitsIn(J9JavaAccessFlags.J9AccClassHotSwappedOut);
	}
	
	public static boolean hasValidEyeCatcher(J9ClassPointer clazz) throws CorruptDataException 
	{
		return clazz.eyecatcher().eq(0x99669966L);
	}
	
	// the method is in hshelp.c for native
	public static boolean areExtensionsEnabled() throws CorruptDataException
	{
		J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
		
		/* If -Xfuture is specified (i.e. JCK testing mode), adhere strictly to the specification */
		if(vm.runtimeFlags().allBitsIn(J9Consts.J9_RUNTIME_XFUTURE)) {
			return false;
		}

		if(J9BuildFlags.interp_nativeSupport) {
			if(J9BuildFlags.jit_fullSpeedDebug) {
				/* Enable RedefineClass and RetransformClass extensions only if we are in full speed debug.
				 * Currently this is always true since acquiring the redefine capability forces us into FSD.
				 */
				if(vm.jitConfig().notNull()) {
					/* JIT is available and initialized for this platform */
					if(!vm.jitConfig().fsdEnabled().eq(0)) {
						return true;
					} else {
						/* but FSD is not enabled */
						return false;
					}
				} else {
					/* JIT is not available, since we run in interpreted mode, no JIT fixups are necessary
					 * and we can allow extensions */
					return true;
				}
			} else {
				/* We have JIT but no FSD, do not allow extensions */
				if(vm.jitConfig().notNull()) {
					return false;
				}
			}
		} else {
			/* No JIT, extensions are always allowed */
		}
	    return true;		
	}
	
	public static boolean isAnonymousClass(J9ClassPointer clazz) throws CorruptDataException
	{
		return J9ROMClassHelper.isAnonymousClass(clazz.romClass());
	}

}
