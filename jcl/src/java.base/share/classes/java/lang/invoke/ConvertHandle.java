/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2009, 2017 IBM Corp. and others
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
package java.lang.invoke;

import java.lang.invoke.MethodHandles.Lookup;
import java.util.concurrent.ConcurrentHashMap;

import com.ibm.oti.util.Msg;

abstract class ConvertHandle extends MethodHandle {
	@VMCONSTANTPOOL_FIELD
	final MethodHandle next;

	ConvertHandle(MethodHandle handle, MethodType type, byte kind, Object thunkArg) {
        	super(type, kind, thunkArg);
        	if ((handle == null) || (type == null)) {
                	throw new IllegalArgumentException();
        	}
        	this.next = handle;
	}

	ConvertHandle(ConvertHandle originalHandle, MethodType newType) {
		super(originalHandle, newType);
		this.next = originalHandle.next;
		this.requiresBoxing = originalHandle.requiresBoxing;
	}

	@VMCONSTANTPOOL_FIELD
	boolean requiresBoxing = false;
	
	/*
	 * Determine if the arguments can be converted from the MethodType fromType to that of toType.
	 * Return conversions are handled by FilterReturnHandle.
	 */
	final void checkConversion(MethodType toType, MethodType fromType) {
		Class<?>[] toArgs = toType.arguments;
		Class<?>[] fromArgs = fromType.arguments;

		if (toArgs.length != fromArgs.length) {
			throwWrongMethodTypeException(fromType, toType, toArgs.length);
		}
		
		boolean isExplicitCast = (kind == KIND_EXPLICITCAST);
			
		// Ensure argsToCollect can be converted to type of array
		for (int i = 0; i < toArgs.length; i++ ) {
			Class<?> toClass = toArgs[i];
			Class<?> fromClass = fromArgs[i];
			
			// identity conversion
			if (fromClass == toClass) {
				continue;
			}
			
			boolean toIsPrimitive = toClass.isPrimitive();
			boolean fromIsPrimitive = fromClass.isPrimitive();
			
			// both are reference types, then apply a cast at runtime
			if (!toIsPrimitive && !fromIsPrimitive) {
				continue;
			}

			// primitive to primitive conversion
			if (toIsPrimitive && fromIsPrimitive) {
				// if not explicitCast, only widening primitive conversions allowed
				if (isExplicitCast || FilterHelpers.checkIfWideningPrimitiveConversion(fromClass, toClass)) {
					continue;
				}
				throwWrongMethodTypeException(fromType, toType, i);
			}
			
			// unbox + widening primitive conversion
			if (toIsPrimitive) {
				if (isExplicitCast) {
					continue;
				} else {
					// fromClass is reference type { Object, Number, Boolean, Byte, Integer, etc }
					Class<?> unwrappedFromClass = MethodType.unwrapPrimitive(fromClass);
					if ((toClass == unwrappedFromClass) ||
						fromClass.isAssignableFrom(MethodType.wrapPrimitive(toClass)) ||
						FilterHelpers.checkIfWideningPrimitiveConversion(unwrappedFromClass, toClass)) {
						continue;
					}
				}
				throwWrongMethodTypeException(fromType, toType, i);
			}
		
			// toClass is reference type
			// primitive wrapper is subclass of fromClass
			if (toClass.isAssignableFrom(MethodType.wrapPrimitive(fromClass))) {
				requiresBoxing = true;
				continue;
			}
			throwWrongMethodTypeException(fromType, toType, i);
		}
	}
	
	static final void throwWrongMethodTypeException(MethodType fromType, MethodType toType, int index) throws WrongMethodTypeException {
		/*[MSG "K05cb", "No conversion from '{0}' to '{1} at index ({2})"]*/
		throw new WrongMethodTypeException(Msg.getString("K05cb", fromType.toString(), toType.toString(), Integer.toString(index))); //$NON-NLS-1$
	}

	/*[IF ]*/
	protected void dumpDetailsTo(java.io.PrintStream out, String prefix, java.util.Set<MethodHandle> alreadyDumped) {
		super.dumpDetailsTo(out, prefix, alreadyDumped);
		next.dumpTo(out, prefix, "next: ", alreadyDumped); //$NON-NLS-1$
	}
	/*[ENDIF]*/

	static final class FilterHelpers {
		/* shared privileged lookup object */
		static Lookup privilegedLookup = MethodHandles.Lookup.internalPrivilegedLookup;

		/* local cache of previously looked up filters */
		static final ConcurrentHashMap<MethodType, MethodHandle> cachedReturnFilters = new ConcurrentHashMap<MethodType, MethodHandle>();
		
		/*
		 * Return conversions: explicit vs asType handled by the boolean isExplicitCast
		 */
		static MethodHandle getReturnFilter(Class<?> fromReturn, Class<?> toReturn, boolean isExplicitCast) {
			assert(fromReturn != toReturn);
			try {
				if (fromReturn == void.class) {
					Object constantValue = null;
					if (toReturn.isPrimitive()) {
						if (toReturn == boolean.class) {
							constantValue = Boolean.valueOf(false);
						} else if (toReturn == char.class) {
							constantValue = Character.valueOf((char)0);
						} else {
							/* covers all other cases as byte can be widened to any other primitive */
							constantValue = Byte.valueOf((byte)0);
						}
					}
					return MethodHandles.constant(toReturn, constantValue);
				}
				MethodType filterType = MethodType.methodType(toReturn, fromReturn);
				if (fromReturn.isPrimitive()) {
					if (toReturn.isPrimitive()) {
						return getPrimitiveReturnFilter(filterType, isExplicitCast);
					} else {
						return getBoxingReturnFilter(filterType);
					}
				} else { // fromReturn is an object
					if (!toReturn.isPrimitive()) {
						return getCastFilter(filterType, isExplicitCast);
					} else if (toReturn == void.class) {
						MethodHandle filter = privilegedLookup.findStatic(FilterHelpers.class, "popObject", MethodType.methodType(void.class, Object.class)); //$NON-NLS-1$
						return filter.cloneWithNewType(filterType);
					} else {
						return getUnboxingReturnFilter(filterType, isExplicitCast);
					}
				}
			} catch(IllegalAccessException e) {
				throw new Error(e);
			} catch (NoSuchMethodException e) {
				throw new Error(e);
			}
		}
		
		
		/* JLSv3: 5.1.2: allowed widening primitive conversions
		 * byte to short, int, long, float, or double
		 * short to int, long, float, or double
		 * char to int, long, float, or double
		 * int to long, float, or double
		 * long to float or double
		 * float to double
		 */
		static boolean checkIfWideningPrimitiveConversion(Class<?> from, Class<?> to) {
			if (!from.isPrimitive() || !to.isPrimitive()) {
				return false;
			}
			if ((from == boolean.class) || (to == boolean.class) || (from == double.class) || (to == char.class)) {
				return false;
			}

			return primitiveIndex1(to) > primitiveIndex1(from);
		}
		
		/* 
		 * doesn't handle boolean or non-primitive.  Also, can't widen to char.
		 * so it's a widening conversion if primitiveIndex1(to) > primitiveIndex1(from)
		 * 
		 */
		private static int primitiveIndex1(Class<?> c) {
			int ch = c.getName().charAt(0);
			int shift = (ch ^ (ch >> 3)) & 0x7;
			/* values for shift:
			0 - double
			2 - float
			1 - long
			4 - int
			7 - char
			5 - short
			6 - byte
			*/
			return (021230546 >> (3*shift)) & 7;	// octal value is 0x21230546 in hex
		} 

		static MethodHandle getPrimitiveReturnFilter(MethodType type, boolean isExplicitCast) throws IllegalAccessException, NoSuchMethodException {
			Class<?> fromClass = type.arguments[0];
			Class<?> toClass = type.returnType;
			
			if (!isExplicitCast) {
				if ((type.returnType != void.class) && !checkIfWideningPrimitiveConversion(fromClass, toClass)) {
					throw new WrongMethodTypeException();
				}
			}
			MethodHandle filter = cachedReturnFilters.get(type);
			if (filter == null) {
				MethodHandle previous;
				String to = MethodType.getBytecodeStringName(toClass);
				String from = MethodType.getBytecodeStringName(type.arguments[0]);
				filter = privilegedLookup.findStatic(FilterHelpers.class, from + "2" + to, type); //$NON-NLS-1$
				if ((previous = cachedReturnFilters.putIfAbsent(type, filter)) != null) {
					filter = previous;
				}
			}
			return filter;
		}
		
		/*[IF ]*/
		/* From 292 javadoc:
		 * If T0 is a primitive and T1 a reference, a boxing conversion is applied if one exists, 
		 * possibly followed by a reference conversion to a superclass. T1 must be a wrapper 
		 * class or a supertype of one. 
		 */
		/*[ENDIF]*/
		/*
		 * Constructors of the Wrapper type provide the filter.
		 */
		static MethodHandle getBoxingReturnFilter(MethodType type) throws IllegalAccessException, NoSuchMethodException {
			Class<?> wrapper = MethodType.wrapPrimitive(type.arguments[0]);
			if (!type.returnType.isAssignableFrom(wrapper)) {
				throw new WrongMethodTypeException();
			}
			MethodHandle filter = cachedReturnFilters.get(type);
			if (filter == null) {
				MethodHandle previous;
				filter = privilegedLookup.findStatic(wrapper, "valueOf", MethodType.methodType(wrapper, type.arguments[0])); //$NON-NLS-1$
				if ((previous = cachedReturnFilters.putIfAbsent(type, filter)) != null) {
					filter = previous;
				}
			}
			if (type.returnType != wrapper) {
				filter = filter.cloneWithNewType(type);
			}
			return filter;
		}
		
		static MethodHandle getUnboxingReturnFilter(MethodType type, boolean isExplicitCast) throws IllegalAccessException, NoSuchMethodException {
			Class<?> toUnbox = type.arguments[0];
			Class<?> returnType = type.returnType;
			
			if (toUnbox.equals(Object.class)) {
				String methodName;
				if (isExplicitCast) {
					methodName = "explicitObject2"; //$NON-NLS-1$
				} else {
					methodName = "object2"; //$NON-NLS-1$
				}
				methodName += MethodType.getBytecodeStringName(returnType);
				return privilegedLookup.findStatic(FilterHelpers.class, methodName, MethodType.methodType(returnType, Object.class));
			
			} else if (toUnbox.equals(Number.class)) {
				/* Widening conversions need to validate the conversion at runtime */
				if (!isExplicitCast) {
					if ((returnType != boolean.class) || (returnType != char.class)) {
						return privilegedLookup.findStatic(FilterHelpers.class, "number2" + MethodType.getBytecodeStringName(returnType), MethodType.methodType(returnType, Number.class)); //$NON-NLS-1$
					}
				} else {
					/* special case these - can't be handled by the <type>Value() methods on Number */
					if ((returnType == boolean.class) || (returnType == char.class)) {
						String methodName = "explicitNumber2"  + MethodType.getBytecodeStringName(returnType); //$NON-NLS-1$
						return privilegedLookup.findStatic(FilterHelpers.class, methodName, MethodType.methodType(returnType, Number.class));
					}
					String methodName = MethodType.getBytecodeStringName(returnType) + "Value"; //$NON-NLS-1$
					return privilegedLookup.findVirtual(Number.class, methodName, MethodType.methodType(returnType));
				}
			
			} else if (toUnbox.equals(Boolean.class)) {
				/* Boolean can be unboxed but there are no widening conversions */
				MethodHandle filter = privilegedLookup.findVirtual(Boolean.class, "booleanValue", MethodType.methodType(boolean.class)); //$NON-NLS-1$
				if (returnType.equals(boolean.class)) {
					return filter;
				}
				if (isExplicitCast) {
					return MethodHandles.filterReturnValue(filter, getPrimitiveReturnFilter(MethodType.methodType(returnType, boolean.class), isExplicitCast));
				}

			} else if (toUnbox.equals(Character.class)) {
				/* Character unboxes to char */
				MethodHandle filter = privilegedLookup.findVirtual(Character.class, "charValue", MethodType.methodType(char.class)); //$NON-NLS-1$
				if (returnType.equals(char.class)) {
					return filter;
				}
				/* widen the return if possible */
				return MethodHandles.filterReturnValue(filter, getPrimitiveReturnFilter(MethodType.methodType(returnType, char.class), isExplicitCast));
			
			} else if (MethodType.WRAPPER_SET.contains(toUnbox)) {
				/* remaining wrappers may have widening conversions - can be handled by toUnbox#'type'Value() methods (ie: Number subclasses)*/
				Class<?> unwrapped = MethodType.unwrapPrimitive(toUnbox);
				boolean justUnwrap = returnType.equals(unwrapped);
				if (justUnwrap || isExplicitCast || checkIfWideningPrimitiveConversion(unwrapped, returnType)) {
					
					if (isExplicitCast && !justUnwrap) {
						/* Special case Boolean/Character --> (!boolean/!char) as it requires a two-step filter */
						if ((returnType == char.class) || (returnType == boolean.class)) {
							// get the unboxed version and filter with necessary conversion
							MethodHandle unbox = privilegedLookup.findVirtual(toUnbox, unwrapped.getName()+"Value", MethodType.methodType(unwrapped)); //$NON-NLS-1$
							MethodHandle filter = getPrimitiveReturnFilter(MethodType.methodType(returnType, unwrapped), isExplicitCast);
							return MethodHandles.filterReturnValue(unbox, filter);
						}
					}
					
					return privilegedLookup.findVirtual(toUnbox, returnType.getName()+"Value", MethodType.methodType(returnType)); //$NON-NLS-1$;
				}
			}

			throw new WrongMethodTypeException();
		}
		
		/*
		 * object to object conversion: use Class#cast(Object)
		 */
		static MethodHandle getCastFilter(MethodType type, boolean isExplicitCast) throws IllegalAccessException, NoSuchMethodException{
			Class<?> returnClass = type.returnType;
			if (isExplicitCast && returnClass.isInterface()) {
				// Interfaces are passed unchecked
				MethodType filterType = MethodType.methodType(Object.class, Object.class);
				return privilegedLookup.findStatic(FilterHelpers.class, "explicitCastInterfaceUnchecked", filterType); //$NON-NLS-1$
			}
			MethodHandleCache methodHandleCache = MethodHandleCache.getCache(returnClass);
			return methodHandleCache.getClassCastHandle();
		}

		public static Object explicitCastInterfaceUnchecked(Object o) {
			return o;
		}
		
		public static boolean explicitObject2Z(Object o) {
			if (o == null) {
				return false;
			} else if (o instanceof Boolean) {
				return ((Boolean)o).booleanValue();
			} else if (o instanceof Character) {
				return ((byte)(((Character)o).charValue()) & 1) == 1;
			} else {
				// assume number - will throw ClassCast if required
				return (((Number)o).byteValue() & 1) == 1;
			}
		}
		
		public static byte explicitObject2B(Object o) {
			if (o == null) {
				return 0;
			} else if (o instanceof Boolean) {
				return (byte)(((Boolean)o).booleanValue() ? 1 : 0);
			} else if (o instanceof Character) {
				return (byte)((Character)o).charValue();
			} else {
				// assume number - will throw ClassCast if required
				return ((Number)o).byteValue();
			}
		}
		public static short explicitObject2S(Object o) {
			if (o == null) {
				return 0;
			} else if (o instanceof Boolean) {
				return (short)(((Boolean)o).booleanValue() ? 1 : 0);
			} else if (o instanceof Character) {
				return (short)((Character)o).charValue();
			} else {
				// assume number - will throw ClassCast if required
				return ((Number)o).shortValue();
			}
		}
		public static char explicitObject2C(Object o) {
			if (o == null) {
				return 0;
			} else if (o instanceof Boolean) {
				return (char)(((Boolean)o).booleanValue() ? 1 : 0);
			} else if (o instanceof Character) {
				return ((Character)o).charValue();
			} else {
				// assume number - will throw ClassCast if required
				return (char)((Number)o).intValue();
			}
		}
		public static int explicitObject2I(Object o) {
			if (o == null) {
				return 0;
			} else if (o instanceof Boolean) {
				return ((Boolean)o).booleanValue() ? 1 : 0;
			} else if (o instanceof Character) {
				return ((Character)o).charValue();
			} else {
				// assume number - will throw ClassCast if required
				return ((Number)o).intValue();
			}
		}
		public static long explicitObject2J(Object o) {
			if (o == null) {
				return 0;
			} else if (o instanceof Boolean) {
				return ((Boolean)o).booleanValue() ? 1 : 0;
			} else if (o instanceof Character) {
				return ((Character)o).charValue();
			} else {
				// assume number - will throw ClassCast if required
				return ((Number)o).longValue();
			}
		}
		public static float explicitObject2F(Object o) {
			if (o == null) {
				return 0;
			} else if (o instanceof Boolean) {
				return ((Boolean)o).booleanValue() ? 1 : 0;
			} else if (o instanceof Character) {
				return ((Character)o).charValue();
			} else {
				// assume number - will throw ClassCast if required
				return ((Number)o).floatValue();
			}
		}
		public static double explicitObject2D(Object o) {
			if (o == null) {
				return 0;
			} else if (o instanceof Boolean) {
				return ((Boolean)o).booleanValue() ? 1 : 0;
			} else if (o instanceof Character) {
				return ((Character)o).charValue();
			} else {
				// assume number - will throw ClassCast if required
				return ((Number)o).doubleValue();
			}
		}
		
		public static boolean explicitNumber2Z(Number n) {
			if (n == null) {
				return false;
			}
			return (n.byteValue() & 1) == 1;
		}

		public static char explicitNumber2C(Number n) {
			if (n == null) {
				return 0;
			}
			return (char)n.intValue();
		}

		/* Move exception allocation out of line */
		private static final ClassCastException newClassCastException() {
			return new ClassCastException();
		}
		
		public static boolean object2Z(Object o) {
			return ((Boolean)o).booleanValue();
		}
		public static char object2C(Object o) {
			return ((Character)o).charValue();
		}
		public static byte object2B(Object o) {
			return number2B((Number)o);
		}
		public static short object2S(Object o) {
			return number2S((Number)o);
		}
		public static int object2I(Object o) {
			o.getClass();	// implicit nullcheck
			if (o instanceof Number) {
				return number2I((Number) o);
			} else if (o instanceof Character) {
				return ((Character)o).charValue();
			}
			throw newClassCastException();
		}
		public static float object2F(Object o) {
			o.getClass();	// implicit nullcheck
			if (o instanceof Number) {
				return number2F((Number) o);
			} else if (o instanceof Character) {
				return ((Character)o).charValue();
			}
			throw newClassCastException();
		}
		public static double object2D(Object o) {
			o.getClass();	// implicit nullcheck
			if (o instanceof Number) {
				return number2D((Number) o);
			} else if (o instanceof Character) {
				return ((Character)o).charValue();
			}
			throw newClassCastException();
		}
		public static long object2J(Object o) {
			o.getClass();	// implicit nullcheck
			if (o instanceof Number) {
				return number2J((Number)o);
			} else if (o instanceof Character) {
				return ((Character)o).charValue();
			}
			throw newClassCastException();
		}
		
		public static byte number2B(Number n) {
			/* Number can be { Byte, Short, Integer, Long, Float,  Double}
			 * Number -> byte:  
			 *		Byte -> byte
			 */
			return ((Byte)n).byteValue();	//Implicit nullcheck;
		}
		public static short number2S(Number n) {
			/* Number can be { Byte, Short, Integer, Long, Float,  Double}
			 * Number -> short:  
			 *		Byte -> byte -> short
			 *		Short -> short
			 */
			if ((n instanceof Short) || (n instanceof Byte)) {
				return n.shortValue();
			}
			n.getClass(); // implicit nullcheck.
			throw newClassCastException();
		}
		public static int number2I(Number n) {
			/* Number can be { Byte, Short, Integer, Long, Float,  Double}
			 * Number -> int:  
			 *		Byte -> byte -> int
			 *		Short -> short -> int
			 *		Integer-> int
			 */
			if ((n instanceof Integer) || (n instanceof Byte) || (n instanceof Short)) {
				return n.intValue();
			}
			n.getClass(); // implicit nullcheck.
			throw newClassCastException();
		}
		public static long number2J(Number n) {
			/* Number can be { Byte, Short, Integer, Long, Float,  Double}
			 * Number -> long:  
			 *		Byte -> byte -> long
			 *		Short -> short -> long
			 *		Integer-> int -> long
			 *		Long -> long
			 */
			if ((n instanceof Long) || (n instanceof Integer) || (n instanceof Byte) || (n instanceof Short)) {
				return n.longValue();
			}
			n.getClass(); // implicit nullcheck.
			throw newClassCastException();
		}

		public static float number2F(Number n) {
			/* Number can be { Byte, Short, Integer, Long, Float,  Double}
			 * Number -> float:  
			 *		Byte -> byte -> float
			 *		Short -> short -> float
			 *		Integer-> int -> float
			 *		Long -> long -> float
			 *		Float -> float
			 */
			if ((n instanceof Float) || (n instanceof Integer) || (n instanceof Long) || (n instanceof Byte) || (n instanceof Short)) {
				return n.floatValue();
			}
			n.getClass(); // implicit nullcheck.
			throw newClassCastException();
		}
		public static double number2D(Number n) {
			/* Number can be { Byte, Short, Integer, Long, Float,  Double}
			 * Number -> double:  
			 *		Byte -> byte -> double
			 *		Short -> short -> double
			 *		Integer-> int -> double
			 *		Long -> long -> double
			 *		Float -> float -> double
			 *		Double -> double
			 */
			if ((n instanceof Double) || (n instanceof Float) || (n instanceof Integer) || (n instanceof Long) || (n instanceof Byte) || (n instanceof Short)) {
				return n.doubleValue();
			}
			n.getClass(); // implicit nullcheck.
			throw newClassCastException();
		}
			
		public static void popObject(Object o) { }
		
		public static double V2D() { return 0; }
		public static long V2J() { return 0; }
		public static float V2F() { return 0; }
		public static int V2I() { return 0; }
		public static char V2C() { return 0; }
		public static short V2S() { return 0; }
		public static byte V2B() { return 0; }
		public static boolean V2Z() { return false; }
		public static Object V2object() { return null; }
		
		public static double Z2D(boolean j) { return j ? 1 : 0; }
		public static long Z2J(boolean j) { return j ? 1 : 0; }
		public static float Z2F(boolean j) { return j ? 1 : 0; }
		public static int Z2I(boolean j) { return j ? 1 : 0; }
		public static char Z2C(boolean j) { return (char)(j ? 1 : 0); }
		public static short Z2S(boolean j) { return (short)(j ? 1 : 0); }
		public static byte Z2B(boolean j) { return (byte)(j ? 1 : 0); }
		public static void Z2V(boolean j) { }
		public static Object Z2object(boolean j) { return Boolean.valueOf(j); }
		
		public static double B2D(byte j) { return j; }
		public static long B2J(byte j) { return j; }
		public static float B2F(byte j) { return j; }
		public static int B2I(byte j) { return j; }
		public static char B2C(byte j) { return (char)j; }
		public static short B2S(byte j) { return j; }
		public static boolean B2Z(byte j) { return (byte)(j & 1) == 1; }
		public static void B2V(byte j) { }
		public static Object B2object(byte j) { return Byte.valueOf(j); }
		
		public static double S2D(short j) { return j; }
		public static long S2J(short j) { return j; }
		public static float S2F(short j) { return j; }
		public static int S2I(short j) { return j; }
		public static char S2C(short j) { return (char)j; }
		public static byte S2B(short j) { return (byte)j; }
		public static boolean S2Z(short j) { return (((byte)j) & 1) == 1; }
		public static void S2V(short j) { }
		public static Object S2object(short j) { return Short.valueOf(j); }
		
		public static double C2D(char j) { return j; }
		public static long C2J(char j) { return j; }
		public static float C2F(char j) { return j; }
		public static int C2I(char j) { return j; }
		public static short C2S(char j) { return (short)j; }
		public static byte C2B(char j) { return (byte)j; }
		public static boolean C2Z(char j) { return (((byte)j) & 1) == 1; }
		public static void C2V(char j) { }
		public static Object C2object(char j) { return Character.valueOf(j); }
		
		public static boolean I2Z(int i) {return (((byte)i) & 1) == 1; }
		public static byte I2B(int i) { return (byte)i; }
		public static short I2S(int i) { return (short)i; }
		public static char I2C(int i) { return (char)i; }
		public static float I2F(int i) { return i; }
		public static long I2J(int i) { return i; }
		public static double I2D(int i) { return i; }
		public static void I2V(int j) { }
		public static Object I2object(int j) { return Integer.valueOf(j); }
		
		public static float J2F(long j) { return j; }
		public static double J2D(long j) { return j; }
		public static int J2I(long j) { return (int)j; }
		public static char J2C(long j) { return (char)j; }
		public static short J2S(long j) { return (short)j; }
		public static byte J2B(long j) { return (byte)j; }
		public static boolean J2Z(long j) { return (((byte)j) & 1) == 1; }
		public static void J2V(long j) { }
		public static Object J2object(long j) { return Long.valueOf(j); }
		
		public static long F2J(float j) { return (long)j; }
		public static double F2D(float j) { return j; }
		public static int F2I(float j) { return (int)j; }
		public static char F2C(float j) { return (char)j; }
		public static short F2S(float j) { return (short)j; }
		public static byte F2B(float j) { return (byte)j; }
		public static boolean F2Z(float j) { return (((byte)j) & 1) == 1; }
		public static void F2V(float j) { }
		public static Object F2object(float j) { return Float.valueOf(j); }
		
		public static float D2F(double j) { return (float) j; }
		public static long D2J(double j) { return (long) j; }
		public static int D2I(double j) { return (int)j; }
		public static char D2C(double j) { return (char)j; }
		public static short D2S(double j) { return (short)j; }
		public static byte D2B(double j) { return (byte)j; }
		public static boolean D2Z(double j) { return (((byte)j) & 1) == 1; }
		public static void D2V(double j) { }
		public static Object D2object(double j) { return Double.valueOf(j); }

		static void load(){}
	}

	// {{{ JIT support
	static { FilterHelpers.load(); } // JIT will need FilterHelpers loaded to compile thunks
	// }}} JIT support

	final void compareWithConvert(ConvertHandle left, Comparator c) {
		c.compareChildHandle(left.next, this.next);
	}
}

