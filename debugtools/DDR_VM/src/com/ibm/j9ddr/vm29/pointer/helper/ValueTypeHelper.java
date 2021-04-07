/*******************************************************************************
 * Copyright (c) 2019, 2021 IBM Corp. and others
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

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import java.lang.invoke.WrongMethodTypeException;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.regex.Pattern;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.logging.LoggerNames;
import com.ibm.j9ddr.vm29.j9.J9ConstantHelper;
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.StructurePointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMFieldShapePointer;
import com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags;
import com.ibm.j9ddr.vm29.structure.J9JavaClassFlags;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * Value type helper class
 *
 * areValueTypesSupported() must return true before using any other methods in this class
 */
public class ValueTypeHelper {
	protected static final Logger logger = Logger.getLogger(LoggerNames.LOGGER_INTERACTIVE_CONTEXT);

	private static ValueTypeHelper helper = null;

	private static class ValueTypeSupportEnabledHelper extends ValueTypeHelper {
		private static final UDATA J9ClassFlagsMask = new UDATA(0xFF);
		private static final UDATA J9ClazzInEntryMask = new UDATA(J9ClassFlagsMask.bitNot());
		private MethodHandle getFlattenedClassCachePointer = null;
		private Class<?> flattenedClassCachePointer = null;
		private MethodHandle flattenedClassCache_numberOfEntries = null;
		private MethodHandle flattenedClassCacheEntry_field = null;
		private MethodHandle flattenedClassCacheEntry_clazz = null;
		private MethodHandle flattenedClassCacheEntry_cast = null;
		private Class<?> flattenedClassCacheEntryPointer = null;

		ValueTypeSupportEnabledHelper() {
			super();
			try {
				Lookup lookup = MethodHandles.lookup();
				flattenedClassCachePointer = Class.forName("com.ibm.j9ddr.vm29.pointer.generated.J9FlattenedClassCachePointer");
				getFlattenedClassCachePointer = lookup.findVirtual(J9ClassPointer.class, "flattenedClassCache", MethodType.methodType(flattenedClassCachePointer));
				flattenedClassCache_numberOfEntries = lookup.findVirtual(flattenedClassCachePointer, "numberOfEntries", MethodType.methodType(UDATA.class));
				flattenedClassCacheEntryPointer = Class.forName("com.ibm.j9ddr.vm29.pointer.generated.J9FlattenedClassCacheEntryPointer");
				flattenedClassCacheEntry_field = lookup.findVirtual(flattenedClassCacheEntryPointer, "field", MethodType.methodType(J9ROMFieldShapePointer.class));
				flattenedClassCacheEntry_clazz = lookup.findVirtual(flattenedClassCacheEntryPointer, "clazz", MethodType.methodType(J9ClassPointer.class));
				flattenedClassCacheEntry_cast = lookup.findStatic(flattenedClassCacheEntryPointer, "cast", MethodType.methodType(flattenedClassCacheEntryPointer, AbstractPointer.class));
			} catch (Throwable t) {
				/* should not happen */
				throwUncheckedExceptions(t);
				logger.log(Level.SEVERE, "ValueTypeHelper: failed to get ValueTypeHelper method handles", t);
			}
		}

		private static void throwUncheckedExceptions(Throwable t) {
			if (t instanceof Error) {
				throw (Error)t;
			} else if (t instanceof RuntimeException) {
				throw (RuntimeException)t;
			}
		}

		private static CorruptDataException handleThrowable(Throwable t) throws CorruptDataException {
			throwUncheckedExceptions(t);
			throw new CorruptDataException(t);
		}

		@Override
		public boolean areValueTypesSupported() {
			return true;
		}

		private J9ClassPointer findJ9ClassInFlattenedClassCacheWithFieldNameImpl(StructurePointer flattenedClassCache, String fieldName) throws CorruptDataException {
			J9ClassPointer resultClazz = J9ClassPointer.NULL;
			try {
				UDATA length = (UDATA) flattenedClassCache_numberOfEntries.invoke(flattenedClassCache);

				/* bump the pointer past the header */
				StructurePointer entry = (StructurePointer) flattenedClassCacheEntry_cast.invoke(flattenedClassCache.add(1));

				int cacheLength = length.intValue();
				for (int i = 0; i < cacheLength; i++) {
					String field = J9UTF8Helper.stringValue(((J9ROMFieldShapePointer)flattenedClassCacheEntry_field.invoke(entry)).nameAndSignature().name());
					if (field.equals(fieldName)) {
						resultClazz = (J9ClassPointer) flattenedClassCacheEntry_clazz.invoke(entry);
						UDATA clazzUDATA = UDATA.cast(resultClazz);
						resultClazz = J9ClassPointer.cast(clazzUDATA.bitAnd(J9ClazzInEntryMask));
						break;
					}
					entry = (StructurePointer) entry.add(1);
				}
			} catch (WrongMethodTypeException | ClassCastException e) {
				/* should not happen */
				logger.log(Level.SEVERE, "ValueTypeHelper: failed to find field in flattened class cache with field name.", e);
			} catch (Throwable t) {
				throw handleThrowable(t);
			}
			return resultClazz;
		}

		@Override
		public J9ClassPointer findJ9ClassInFlattenedClassCacheWithFieldName(J9ClassPointer clazz, String fieldName) throws CorruptDataException {
			J9ClassPointer resultClazz = J9ClassPointer.NULL;

			try {
				StructurePointer flattenedClassCache = (StructurePointer) getFlattenedClassCachePointer.invoke(clazz);
				if (!flattenedClassCache.isNull()) {
					resultClazz = findJ9ClassInFlattenedClassCacheWithFieldNameImpl(flattenedClassCache, fieldName);
				}
			} catch (WrongMethodTypeException | ClassCastException e) {
				/* should not happen */
				logger.log(Level.SEVERE, "ValueTypeHelper: failed to find field in flattened class cache with field name", e);
			} catch (Throwable t) {
				throw handleThrowable(t);
			}
			return resultClazz;
		}

		private J9ClassPointer findJ9ClassInFlattenedClassCacheWithFieldSigImpl(StructurePointer flattenedClassCache, String fieldSig) throws CorruptDataException {
			J9ClassPointer resultClazz = J9ClassPointer.NULL;
			try {
				UDATA length = (UDATA) flattenedClassCache_numberOfEntries.invoke(flattenedClassCache);

				/* bump the pointer past the header */
				StructurePointer entry = (StructurePointer) flattenedClassCacheEntry_cast.invoke(flattenedClassCache.add(1));

				int cacheLength = length.intValue();
				for (int i = 0; i < cacheLength; i++) {
					String field = J9UTF8Helper.stringValue(((J9ROMFieldShapePointer)flattenedClassCacheEntry_field.invoke(entry)).nameAndSignature().signature());
					field = field.substring(1,  field.length() - 1);
					if (field.equals(fieldSig)) {
						resultClazz = (J9ClassPointer) flattenedClassCacheEntry_clazz.invoke(entry);
						UDATA clazzUDATA = UDATA.cast(resultClazz);
						resultClazz = J9ClassPointer.cast(clazzUDATA.bitAnd(J9ClazzInEntryMask));
						break;
					}
					entry = (StructurePointer) entry.add(1);
				}
			} catch (WrongMethodTypeException | ClassCastException e) {
				/* should not happen */
				logger.log(Level.SEVERE, "ValueTypeHelper: failed to find field in flattened class cache with field signature", e);
			} catch (Throwable t) {
				throw handleThrowable(t);
			}
			return resultClazz;
		}

		@Override
		public J9ClassPointer[] findNestedClassHierarchy(J9ClassPointer containerClazz, String[] nestingHierarchy) throws CorruptDataException {
			J9ClassPointer[] resultClasses = new J9ClassPointer[nestingHierarchy.length + 1];
			J9ClassPointer clazz = containerClazz;
			int index = 0;

			if (Pattern.matches("\\[\\d+\\]", nestingHierarchy[0])) {
				resultClasses[0] = containerClazz.arrayClass();
				index = 1;
			}
			resultClasses[index] = containerClazz;

			for (; index < nestingHierarchy.length; index++) {
				clazz = findJ9ClassInFlattenedClassCacheWithFieldName(clazz, nestingHierarchy[index]);
				resultClasses[index + 1] = clazz;
			}

			return resultClasses;
		}

		@Override
		public J9ClassPointer findJ9ClassInFlattenedClassCacheWithSigName(J9ClassPointer clazz, String fieldSig) throws CorruptDataException {
			J9ClassPointer resultClazz = J9ClassPointer.NULL;
			try {
				StructurePointer flattenedClassCache = (StructurePointer) getFlattenedClassCachePointer.invoke(clazz);
				if (!flattenedClassCache.isNull()) {
					resultClazz = findJ9ClassInFlattenedClassCacheWithFieldSigImpl(flattenedClassCache, fieldSig);
				}
			} catch (WrongMethodTypeException | ClassCastException e) {
				/* should not happen */
				logger.log(Level.SEVERE, "ValueTypeHelper: failed to find field in flattened class cache with field signature", e);
			} catch (Throwable t) {
				throw handleThrowable(t);
			}
			return resultClazz;
		}

		@Override
		public boolean isRomClassAValueType(J9ROMClassPointer romClass) throws CorruptDataException {
			return romClass.modifiers().allBitsIn(J9JavaAccessFlags.J9AccValueType);
		}

		@Override
		public boolean isJ9ClassAValueType(J9ClassPointer clazz) throws CorruptDataException {
			return clazz.classFlags().allBitsIn(J9JavaClassFlags.J9ClassIsValueType);
		}

		@Override
		public boolean isFieldInClassFlattened(J9ClassPointer clazz, String fieldName) throws CorruptDataException {
			boolean result = false;

			try {
				StructurePointer flattenedClassCache = (StructurePointer) getFlattenedClassCachePointer.invoke(clazz);
				if (!flattenedClassCache.isNull()) {
					J9ClassPointer flattenableClazz = findJ9ClassInFlattenedClassCacheWithFieldNameImpl(flattenedClassCache, fieldName);
					if (!flattenableClazz.isNull()) {
						result = isJ9ClassIsFlattened(flattenableClazz);
					}
				}
			} catch (WrongMethodTypeException | ClassCastException e) {
				/* should not happen */
				logger.log(Level.SEVERE, "ValueTypeHelper: failed to determine if field is flattened", e);
				throw new RuntimeException(e);
			} catch (Throwable t) {
				throw handleThrowable(t);
			}

			return result;
		}

		@Override
		public boolean isJ9ClassLargestAlignmentConstraintDouble(J9ClassPointer clazz) throws CorruptDataException {
			return J9ClassHelper.extendedClassFlags(clazz).allBitsIn(J9JavaClassFlags.J9ClassLargestAlignmentConstraintDouble);
		}

		@Override
		public boolean isJ9ClassLargestAlignmentConstraintReference(J9ClassPointer clazz) throws CorruptDataException {
			return J9ClassHelper.extendedClassFlags(clazz).allBitsIn(J9JavaClassFlags.J9ClassLargestAlignmentConstraintReference);
		}

		@Override
		public boolean isJ9ClassIsFlattened(J9ClassPointer clazz) throws CorruptDataException {
			return J9ClassHelper.extendedClassFlags(clazz).allBitsIn(J9JavaClassFlags.J9ClassIsFlattened);
		}

		@Override
		public boolean classRequires4BytePrePadding(J9ClassPointer clazz) throws CorruptDataException {
			return J9ClassHelper.extendedClassFlags(clazz).allBitsIn(J9JavaClassFlags.J9ClassRequiresPrePadding);
		}

		@Override
		public boolean isFlattenableFieldSignature(String signature) {
			return signature.startsWith("Q");
		}
	}

	private ValueTypeHelper() {
		/* empty constructor */
	}

	private static boolean checkIfValueTypesAreSupported() {
		/* Older builds have builds flags in camel case, newer builds have flags capitalized */
		return J9ConstantHelper.getBoolean(J9BuildFlags.class, "J9VM_OPT_VALHALLA_VALUE_TYPES", false)
			|| J9ConstantHelper.getBoolean(J9BuildFlags.class, "opt_valhallaValueTypes", false);
	}

	/**
	 * Factory method that returns a valueTypeHelper instance.
	 *
	 * @return valueTypeHelper
	 */
	public static ValueTypeHelper getValueTypeHelper() {
		if (helper == null) {
			if (checkIfValueTypesAreSupported()) {
				helper = new ValueTypeSupportEnabledHelper();
			} else {
				helper = new ValueTypeHelper();
			}
		}
		return helper;
	}

	/**
	 * Queries if the JVM that produced the core file supports value types. This
	 * must be called before any other ValueTypeHelper functions are called.
	 *
	 * @return true if value types are supported, false otherwise
	 */
	public boolean areValueTypesSupported() {
		return false;
	}

	/**
	 * Searches the flattenedClassCache (FCC) for a J9Class given a field name.
	 * areValueTypesSupported() must return true before using this method.
	 *
	 * @param clazz J9Class pointer that owns the FCC to be searched
	 * @param fieldName name of the field to lookup in the FCC
	 *
	 * @return J9Class of the fieldName if found, J9ClassPointer.NULL otherwise
	 * @throws CorruptDataException
	 */
	public J9ClassPointer findJ9ClassInFlattenedClassCacheWithFieldName(J9ClassPointer clazz, String fieldName) throws CorruptDataException {
		logger.log(Level.SEVERE, "Value types were not enabled in the JVM that produced this core file.");
		throw new RuntimeException("Invalid operation");
	}

	/**
	 * Returns an array of J9ClassPointers that contains the containerClazz passed in addition to the J9ClassPointers
	 * that correspond to the types of the field names passed in the nestingHierarchy array. Each subsequent
	 * field name in the nestingHierarchy array is a nested field of the previous element. For example
	 * nestingHierarchy[1] is a member of nestingHierarchy[0], and nestingHierarchy[2] is a member of
	 * nestingHierarchy[1]. nestingHierarchy[0] is the first nested member in the containerClazz. The resulting array contains
	 * at least one element (containerClazz).
	 *
	 * areValueTypesSupported() must return true before using this method.
	 *
	 * @param containerClazz type that contains all the nested members
	 * @param nestingHierarchy array that contains field names for nested members. Must be non-NULL
	 * @return an array containing at least the containerClazz, and J9ClassPointer's corresponding to the names passed in the nestingHierarchy
	 * @throws CorruptDataException
	 */
	public J9ClassPointer[] findNestedClassHierarchy(J9ClassPointer containerClazz, String[] nestingHierarchy) throws CorruptDataException {
		logger.log(Level.SEVERE, "Value types were not enabled in the JVM that produced this core file.");
		throw new RuntimeException("Invalid operation");
	}

	/**
	 * Searches the flattenedClassCache (FCC) for a J9Class given a field signature.
	 * areValueTypesSupported() must return true before using this method.
	 *
	 * @param clazz J9Class pointer that owns the FCC to be searched
	 * @param fieldSig signature to lookup in the FCC
	 *
	 * @return J9ClassPointer corresponding to the signature if found, J9ClassPointer.NULL otherwise
	 * @throws CorruptDataException
	 */
	public J9ClassPointer findJ9ClassInFlattenedClassCacheWithSigName(J9ClassPointer clazz, String fieldSig) throws CorruptDataException {
		logger.log(Level.SEVERE, "Value types were not enabled in the JVM that produced this core file.");
		throw new RuntimeException("Invalid operation");
	}

	/**
	 * Queries if J9ROMClass is a value type
	 *
	 * @param romClass clazz to query
	 * @return true if romClass is a value type, false otherwise
	 * @throws CorruptDataException
	 */
	public boolean isRomClassAValueType(J9ROMClassPointer romClass) throws CorruptDataException {
		return false;
	}

	/**
	 * Queries if J9Class is a value type
	 *
	 * @param clazz clazz to query
	 * @return true if class is a value type, false otherwise
	 * @throws CorruptDataException
	 */
	public boolean isJ9ClassAValueType(J9ClassPointer clazz) throws CorruptDataException {
		return false;
	}

	/**
	 * Queries whether field is flattened of not.
	 *
	 * @param clazz clazz containing the field
	 * @param fieldName name of the field
	 * @return true if field is flattened, false otherwise
	 * @throws CorruptDataException
	 */
	public boolean isFieldInClassFlattened(J9ClassPointer clazz, String fieldName) throws CorruptDataException {
		return false;
	}

	/**
	 * Queries if field signature is flattenable
	 * @param signature field signature
	 * @return true if field signature is flattenable, false otherwise
	 */
	public boolean isFlattenableFieldSignature(String signature) {
		return false;
	}

	/**
	 * Queries if class contains a field with a double (64 bit) alignment constraint
	 * @param clazz J9Class
	 * @return true if clazz has double alignment constraint, false otherwise
	 */
	public boolean isJ9ClassLargestAlignmentConstraintDouble(J9ClassPointer clazz) throws CorruptDataException {
		return false;
	}

	/**
	 * Queries if class contains a field with a reference alignment constraint
	 * @param clazz J9Class
	 * @return true if clazz has reference alignment constraint, false otherwise
	 */
	public boolean isJ9ClassLargestAlignmentConstraintReference(J9ClassPointer clazz) throws CorruptDataException {
		return false;
	}

	/**
	 * Queries if class is flattened
	 * @param clazz J9Class
	 * @return true if clazz is flattened, false otherwise
	 */
	public boolean isJ9ClassIsFlattened(J9ClassPointer clazz) throws CorruptDataException {
		return false;
	}
	
	/**
	 * Queries if class is has 4byte pre-padding in the stand-alone case
	 * @param clazz J9Class
	 * @return true if clazz is requires pre-padding, false otherwise
	 */
	public boolean classRequires4BytePrePadding(J9ClassPointer clazz) throws CorruptDataException {
		return false;
	}
}
