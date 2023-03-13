/*[INCLUDE-IF Sidecar18-SE & !OPENJDK_METHODHANDLES]*/
/*******************************************************************************
 * Copyright IBM Corp. and others 2009
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package java.lang.invoke;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.ObjectStreamException;
import java.io.ObjectStreamField;
import java.io.Serializable;
/*[IF JAVA_SPEC_VERSION >= 12]*/
import java.lang.constant.Constable;
import java.lang.constant.ClassDesc;
import java.lang.constant.MethodTypeDesc;
/*[ENDIF] JAVA_SPEC_VERSION >= 12 */
import java.lang.ref.Reference;
import java.lang.ref.WeakReference;
import java.lang.reflect.Field;
import java.security.*;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
/*[IF JAVA_SPEC_VERSION >= 12]*/
import java.util.NoSuchElementException;
import java.util.Optional;
/*[ENDIF] JAVA_SPEC_VERSION >= 12 */
import java.util.Set;
import java.util.WeakHashMap;

import com.ibm.oti.util.Msg;
import com.ibm.oti.vm.VM;

/*[IF CRIU_SUPPORT]*/
import openj9.internal.criu.NotCheckpointSafe;
/*[ENDIF] CRIU_SUPPORT*/

/*[IF Sidecar18-SE-OpenJ9]*/
import java.lang.invoke.MethodTypeForm;
/*[ENDIF]*/

/**
 * MethodTypes are immutable objects used to correlate MethodHandles with their invocation sites.
 * <p>
 * A MethodType is a composed of the return type and the parameter types.  These are represented using Class
 * objects of the corresponding type, be it primitive, void, or reference.
 * <p>
 * MethodTypes are interned and immutable.  As such they can be compared for instance equality (==).  Two 
 * MethodTypes are equal iff they have the same classes for the their return and parameter types.
 * 
 * @since 1.7
 */
@VMCONSTANTPOOL_CLASS
public final class MethodType implements Serializable 
/*[IF JAVA_SPEC_VERSION >= 12]*/
	, Constable, TypeDescriptor.OfMethod<Class<?>, MethodType>
/*[ENDIF] JAVA_SPEC_VERSION >= 12 */
{
	static final Class<?>[] EMTPY_PARAMS = new Class<?>[0];

/*[IF Sidecar19-SE-OpenJ9]*/	
	private MethodTypeForm form;
/*[ENDIF]*/	

	private static Map<MethodType, WeakReference<MethodType>> internTable = Collections.synchronizedMap(new WeakHashMap<MethodType, WeakReference<MethodType>>());
	// lock object to control insertion of new MTs
	static final class InternTableAddLock { }
	private static InternTableAddLock internTableAddLock = new InternTableAddLock();
	
	@VMCONSTANTPOOL_FIELD
	final Class<?> rtype;
	
	@VMCONSTANTPOOL_FIELD
	final Class<?>[] ptypes;

	@VMCONSTANTPOOL_FIELD
	int argSlots;	// Number of stack slots used by the described args in the MethodType
	
	/*[IF ]*/
	/*
	 * int[] describing which arguments on the stack are objects.
	 * Format: { argByte1, argByte2, ... }  Each bit set is an
	 * O-slot.  These ints get copied into the MethodType stack
	 * frame.
	 */
	/*[ENDIF]*/
	@VMCONSTANTPOOL_FIELD
	int[] stackDescriptionBits;

	private String methodDescriptor;
	private int hashcode = 0;
	private InvokeExactHandle invoker;

	/* Indicate that there are no persistent/serializable fields in MethodType */
	private static final ObjectStreamField[] serialPersistentFields = new ObjectStreamField[0];
	private static final long serialVersionUID = 292L;
	private DeserializedFieldsHolder deserializedFields;

	/**
	 * Holder for the deserialized arguments[] and returnType values
	 * from #readObject() so they can be passed to #readResolve()
	 */
	private static class DeserializedFieldsHolder {
		Class<?> returnType;
		Class<?>[] arguments;

		DeserializedFieldsHolder(Class<?> rtype, Class<?>[] args) {
			returnType = (rtype == null) ? void.class : rtype;
			arguments = (args == null) ? EMTPY_PARAMS : args;
		}
	}

	/*
	 * Private constructor as MethodTypes need to be interned.  
	 */
	private MethodType(Class<?> rtype, Class<?>[] ptypes, boolean copyArguments) {
		this.rtype = rtype;
		if (copyArguments) {
			this.ptypes = new Class<?>[ptypes.length];
			System.arraycopy(ptypes, 0, this.ptypes, 0, ptypes.length);
		} else {
			this.ptypes = ptypes;
		}
	}
		
	/*[IF ]*/
	/*
	 * Produce an int[] that describes the arguments on the stack.  Each bit in 
	 * the int describes a stack slot and marks it either as an O-Slot (1) or 
	 * an I-Slot (0).  
	 * 
	 * The first bit is always marked as it is assumed that there is either a 
	 * MethodHandle or a null on the stack occupying that first slot.
	 * 
	 * This is based on argBitsFromSignature(J9UTF8*) in argbits.c
	 */
	/*[ENDIF]*/
	private final int[] stackDescriptionBits(Class<?>[] args, int stackSlots) {
		/* neededInts is the number of stackslots + 1 for the receiver (MH or NULL) + 31 so that we round up */
		int neededInts = (stackSlots + 1 + 31) / 32;
		int[] ints = new int[neededInts];
				
		int index = 0;
		int argBit = 1;
		int curr = 0;
		
		/* Mark the "receiver" bit.  This is the mandatory bit */
		curr |= argBit;
		argBit <<= 1;
		
		for (Class<?> c : args) {
			if (c.isPrimitive()) {
				/* primitive double and long take 2 stack slots */
				if (c.equals(double.class) || c.equals(long.class)) {
					argBit <<= 1;
					/* If we shifted far enough to remove the bit from the int
					 * then start assigning bits to the next int in the array
					 */
					if (argBit == 0) {
						argBit = 1;
						ints[index] = curr;
						index++;
						curr = 0;
					}
				}
			} else {
				/* Found an Object - mark as a O-Slot */
				curr |= argBit;
			}
			/* Shift to the next argument */
			argBit <<= 1;
			/* If we shifted far enough to remove the bit from the int
			 * then start assigning bits to the next int in the array
			 */
			if (argBit == 0) {
				argBit = 1;
				ints[index] = curr;
				index++;
				curr = 0;
			}
		}
		
		/* Ensure last int updated */
		if (index < ints.length) {
			ints[index] = curr;
		}
		return ints;
	}

	/**
	 * Convenience method to create a new MethodType with only the parameter at 
	 * position changed to the new type.
	 * 
	 * @param position - the position, starting from zero, of the parameter to be changed.  
	 * @param type - the Class to change the parameter to
	 * @return a new MethodType with the parameter at position changed
	 */
	public MethodType changeParameterType(int position, Class<?> type) {
		//TODO: Should check that position is valid.
		Class<?>[] newParameters = ptypes.clone();
		newParameters[position] = type;
		return MethodType.methodType(rtype, newParameters, false); 
	 }
	
	/**
	 * Convenience method to create a new MethodType with a changed return type.
	 * 
	 * @param type - the Class that the return type should be changed to. 
	 * @return a new MethodType with a changed return type
	 */
	public MethodType changeReturnType(Class<?> type){
		/*[IF ]*/
		//TODO: potential optimization - don't need to check the args, recalculate stack slots etc.
		/*[ENDIF]*/
		return methodType(type, ptypes, false);
	}
   
	/**
     * Convenience method to create a new MethodType after dropping the
     * parameters between startPosition and endPosition.
     * 
     * @param startPosition - the position, starting from zero, from which to start dropping parameters 
     * @param endPosition - the position of the first parameter not to drop.  Must be greater than startPosition.
     * @return a new MethodType with parameters between start and end-1 position (inclusive) removed
	 * @throws IndexOutOfBoundsException if the startPosition or endPosition are not valid indexes or if the startPosition is greater than the endPosition
     */
    public MethodType dropParameterTypes(int startPosition, int endPosition) throws IndexOutOfBoundsException {
    	/*[IF ]*/
    	/* Throws: IndexOutOfBoundsException - if start is negative or greater than parameterCount() 
    	 * or if end is negative or greater than parameterCount() or if start is greater than end.
    	 */
    	/*[ENDIF]*/
		if ((startPosition >= 0) && (endPosition >= 0) && (startPosition <= endPosition) && (endPosition <= ptypes.length)) {
    		int delta = endPosition - startPosition;
    		Class<?>[] newParameters = new Class<?>[ptypes.length - delta];
    		System.arraycopy(ptypes, 0, newParameters, 0, startPosition);
    		System.arraycopy(ptypes, endPosition, newParameters, startPosition, ptypes.length - endPosition);
    		return methodType(rtype, newParameters, false);
		}
    	throw new IndexOutOfBoundsException("'" + this + "' startPosition=" + startPosition + " endPosition=" + endPosition);  //$NON-NLS-1$//$NON-NLS-2$ //$NON-NLS-3$
	}
    
    /* Internal helper method that is equivalent to dropParameterTypes(0, 1) */
    MethodType dropFirstParameterType() throws IndexOutOfBoundsException {
    	if (ptypes.length == 0) {
    		throw new IndexOutOfBoundsException();
    	}
    	return methodType(rtype, Arrays.copyOfRange(ptypes, 1, ptypes.length), false);
	}
    
	  
	/**
	 * Compares the specified object with this type for equality. 
	 * That is, it returns true if and only if the specified object 
	 * is also a method type with exactly the same parameters and return type.   
	 */
	public boolean equals(Object x) {
		// This is interned so we only need a pointer check
		if (this == x) {
			return true;
		}
		// Need to actually check to be able to use a weak hashset
		if (!(x instanceof MethodType)) {
			return false;
		}

		MethodType that = (MethodType) x;
		if ((this.ptypes.length != that.ptypes.length) || (this.rtype != that.rtype)) {
			return false;
		}
		return Arrays.equals(this.ptypes, that.ptypes);
	}
	
	/**
	 * Convenience method erase all reference types to Object.  Primitive and void 
	 * remain unchanged. 
	 * 
	 * @return a new MethodType with all non-primitive or void return type and parameters changed to Object.
	 */
	public MethodType erase() {
		Class<?>[] newParameters = ptypes.clone();
		for (int i= 0; i < newParameters.length; i++){
			if (!newParameters[i].isPrimitive()) {
				newParameters[i] = Object.class;
			}
		}
		return methodType(rtype.isPrimitive() ? rtype : Object.class, newParameters, false);
	}
	
	
	/**
	 * Convenience Method to create a MethodType from bytecode-level method descriptor. 
	 * (See JVM Spec 2nd Ed. section 4.4.3).
	 * <p>
	 * All of the classes used in the method descriptor string must be reachable from a 
	 * common ClassLoader or an exception will result.
	 * <p>
	 * The ClassLoader parameter may be null, in which case the System ClassLoader will be used.
	 * <p>
	 * Note, the Class names must use JVM syntax in the method descriptor String and therefore
	 * <i>java.lang.Class</i> will be represented as <i>Ljava/lang/Class;</i>
	 * <p>
	 * <b>Example method descriptors</b>
	 * <ul>
	 * <li>(II)V - method taking two ints and return void</li>
	 * <li>(I)Ljava/lang/Integer; - method taking an int and returning an Integer</li>
	 * <li>([I)I - method taking an array of ints and returning an int</li>
	 * </ul>
	 * 
	 * @param methodDescriptor - the method descriptor string 
	 * @param loader - the ClassLoader to be used or null for System ClassLoader 
	 * @return a MethodType object representing the method descriptor string
	 * @throws IllegalArgumentException - if the string is not well-formed
	 * @throws TypeNotPresentException - if a named type cannot be found
	 */
	@VMCONSTANTPOOL_METHOD
	public static MethodType fromMethodDescriptorString(String methodDescriptor, ClassLoader loader) {
		ClassLoader classLoader = loader; 
		if (classLoader == null) {
			/*[IF JAVA_SPEC_VERSION >= 14]*/
			@SuppressWarnings("removal")
			SecurityManager security = System.getSecurityManager();
			if (security != null) {
				security.checkPermission(sun.security.util.SecurityConstants.GET_CLASSLOADER_PERMISSION);
			}
			/*[ENDIF] JAVA_SPEC_VERSION >= 14 */
			classLoader = ClassLoader.getSystemClassLoader();
		}
		
		// Check cache
		Map<String, MethodType> classLoaderMethodTypeCache = VM.getVMLangAccess().getMethodTypeCache(classLoader);
		MethodType mt = classLoaderMethodTypeCache != null ? classLoaderMethodTypeCache.get(methodDescriptor) : null;

		// MethodDescriptorString is not in cache
		if (null == mt) {
			// ensure '.' is not included in the descriptor
			if (methodDescriptor.indexOf((int)'.') != -1) {
				throw new IllegalArgumentException(methodDescriptor);
			}
			
			// split descriptor into classes - last one is the return type
			ArrayList<Class<?>> classes = parseIntoClasses(methodDescriptor, classLoader);
			if (classes.size() == 0) {
				throw new IllegalArgumentException(methodDescriptor);
			}
			
			Class<?> returnType = classes.remove(classes.size() - 1);
			mt = methodType(returnType, classes);
			if (classLoaderMethodTypeCache != null) {
				classLoaderMethodTypeCache.put(mt.methodDescriptor, mt);
			}
		}
		
		return mt;
	}
	
	/**
	 * Internal helper method for generating a MethodType from a descriptor string,
	 * and append an extra argument type.
	 * 
	 * VarHandle call sites use this method to generate the MethodType that matches
	 * the VarHandle method implementation (extra VarHandle argument).
	 */
	@SuppressWarnings("unused")  /* Used by native code */
	private static final MethodType fromMethodDescriptorStringAppendArg(String methodDescriptor, ClassLoader loader, Class<?> appendArgumentType) {
		List<Class<?>> types = parseIntoClasses(methodDescriptor, loader);
		Class<?> returnType = types.remove(types.size() - 1);
		types.add(appendArgumentType);
		
		return methodType(returnType, types);
	}

	/**
	 * Helper method to throw NoClassDefFoundError if the cause of TypeNotPresentException 
	 * is ClassNotFoundException. Otherwise, re-throw TypeNotPresentException.
	 * 
	 * @param e - an instance of TypeNotPresentException
	 * 
	 * @return a Throwable object to prevent any fall through case
	 * 
	 * @throws NoClassDefFoundError - if the cause of e is ClassNotFoundException
	 * @throws TypeNotPresentException - if the cause of e is not ClassNotFoundException
	 */
	private static final Throwable throwNoClassDefFoundError(TypeNotPresentException e) {
		Throwable cause = e.getCause();
		if (cause instanceof ClassNotFoundException) {
			NoClassDefFoundError noClassDefFoundError = new NoClassDefFoundError(cause.getMessage());
			noClassDefFoundError.initCause(cause);
			throw noClassDefFoundError;
		}
		throw e;
	}

	/*
	 * Parse the MethodDescriptor string into a list of Class objects.  The last class in the list
	 * is the return type.
	 */
	private static final ArrayList<Class<?>> parseIntoClasses(String methodDescriptor, ClassLoader classLoader) {
		int length = methodDescriptor.length();
		if (length == 0) {
			/*[MSG "K05d3", "invalid descriptor: {0}"]*/
			throw new IllegalArgumentException(Msg.getString("K05d3", methodDescriptor)); //$NON-NLS-1$
		}
		
		char[] signature = new char[length];
		methodDescriptor.getChars(0, length, signature, 0);
		int index = 0;
		boolean closeBracket = false;
		
		if (signature[index] != '(') {
			/*[MSG "K05d4", "missing opening '(': {0}"]*/
			throw new IllegalArgumentException(Msg.getString("K05d4", methodDescriptor)); //$NON-NLS-1$
		}
		index++;
		
		ArrayList<Class<?>> args = new ArrayList<Class<?>>();
		
		while(index < length) {
			/* Ensure we only see one ')' closing bracket */
			if ((signature[index] == ')')) {
				if (closeBracket) {
					/*[MSG "K05d5", "too many ')': {0}"]*/
					throw new IllegalArgumentException(Msg.getString("K05d5", methodDescriptor)); //$NON-NLS-1$
				}
				closeBracket = true;
				index++;
				continue;
			}

			index = MethodTypeHelper.parseIntoClass(signature, index, args, classLoader, methodDescriptor);
			index++;
		}
		return args;
	}
	
	
	/**
	 * Convenience method to convert all types to Object. 
	 * 
	 * @return a new MethodType with both return and parameter types changed to Object
	 */
	public MethodType generic() {
		return genericMethodType(ptypes.length);
	}
	
	
	/**
	 * Returns the MethodType's hash code, which is defined to be
	 * the same as the hash code of a List composed of the return type 
	 * followed by the parameter types.
	 * 
	 * @return the MethodType's hash code
	 */
	public int hashCode(){
		if (hashcode == 0) {
			int hash = 31 + rtype.hashCode();
			for (Class<?> c : ptypes) {
				hash = 31 * hash + c.hashCode();
			}
			hashcode = hash;
		}
		return hashcode;
	}
	
	
	/**
	 * Helper method to determine if the return type or any of the parameter types
	 * are primitives.
	 * 
	 * @return whether the MethodType contains any primitive types
	 */
	public boolean hasPrimitives(){
		if (rtype.isPrimitive()){
			return true;
		}
		for(Class<?> c : ptypes){
			if (c.isPrimitive()) {
				return true;
			}
		}
		return false;
	}
	
	/**
	 * Helper method to determine if the return type or any of the parameter types
	 * are wrappers.  Wrappers are the boxed versions of primitives.
	 * <p>
	 * java.lang.Void is only treated as a wrapper if it occurs as the class of the
	 * return.
	 * 
	 * @return whether the MethodType contains any wrapper types
	 */
	public boolean hasWrappers() {
		if (MethodTypeHelper.WRAPPER_SET.contains(rtype) || (rtype == Void.class)){
			return true;
		}
		for(Class<?> c : ptypes){
			if (MethodTypeHelper.WRAPPER_SET.contains(c)){
				return true;
			}
		}
		return false;
	}
	
	/**
	 * Return a new MethodType with an additional parameters inserted at position, which
	 * is a zero based index.
	 * 
	 * @param position - the position to insert into 
	 * @param types - zero or mores types for the new parameters
	 * @return a new MethodType with the additional parameters at position
	 * @throws IndexOutOfBoundsException if position is less than 0 or greater than the number of arguments
	 */
	public MethodType insertParameterTypes(int position, Class<?>... types) throws IndexOutOfBoundsException {
		if (position < 0 || position > ptypes.length) {
			throw new IndexOutOfBoundsException();
		}
		int typesLength = types.length;
		if (typesLength == 0) {
			/* Not inserting any additional types - therefore no change necessary */
			return this;
		}
		
		Class<?>[] params = new Class<?>[ptypes.length + typesLength];
		// Copy from 0 -> position out of the existing MethodType arguments
		System.arraycopy(ptypes, 0, params, 0, position);
		// Copy from position -> types.length out of the types[] array
		System.arraycopy(types, 0, params, position, typesLength);
		// Copy from position + types.length -> params.length out of the existing MT arguments
		System.arraycopy(ptypes, position, params, position + typesLength, ptypes.length - position);
		return methodType(rtype, params, false);
	}
	
	/**
	 * Return a new MethodType with an additional parameters inserted at position, which
	 * is a zero based index.
	 * 
	 * @param position - the position to insert into 
	 * @param types - zero or mores types for the new parameters
	 * @return a new MethodType with the additional parameters at position
	 * @throws IllegalArgumentException if position is less than 0 or greater than the number of arguments
	 */
	public MethodType insertParameterTypes(int position, List<Class<?>> types) {
		return insertParameterTypes(position, types.toArray(new Class[types.size()]));
	}
	
	/**
	 * Create a MethodType object with the specified return type and no parameters
	 * 
	 * @param type - the return type of the MethodHandle
	 * @return a new MethodHandle object
	 */
	public static MethodType methodType(Class<?> type){
		return methodType(type, EMTPY_PARAMS, false);
	}
	     
	/**
	 * Return a MethodType object with the specified return type and a single parameter
	 * of type 'parameter0'.
	 * 
	 * @param type - the return type of the MethodHandle
	 * @param parameter0 - the type of the single parameter
	 * @return a new MethodHandle object
	 */
	public static MethodType methodType(Class<?> type, Class<?> parameter0) {
		return methodType(type, new Class<?>[] { parameter0 }, false);
	}
	
	/**
	 * Return a MethodType object with the parameter and return types as requested.
	 * 
	 * @param returnType - the MethodType's return type
	 * @param parameters - the MethodType's parameters
	 * @return the interned MethodType
	 * 
	 * @throws NullPointerException - if the return type or parameters are null 
	 * @throws IllegalArgumentException - if any of the parameters is void
	 */
	public static MethodType methodType(Class<?> returnType, Class<?>[] parameters){
		return methodType(returnType, parameters, true);
	}

	private static MethodType methodType(Class<?> returnType, Class<?>[] parameters, boolean copyArguments) {
		returnType.getClass();	// null check
		
		// assume that most MethodTypes have already been interned and simply
		// create a MT for probing the table.  This saves calculating the 
		// expensive internal state (stack slots, etc) when the intern probe would hit.
		MethodType type = new MethodType(returnType, parameters, copyArguments);
		return type.intern();
	}

	/*[IF ]*/
	/* This method should be replaced by a native once jazz design 30600 is implemented. */
	/*[ENDIF]*/
	/*
	 * Intern the MethodType in the internTable.
	 *  
	 * @param type - the MethodType to be interned
	 * @return - the interned MethodType.
	 */
	/*[IF CRIU_SUPPORT]*/
	@NotCheckpointSafe
	/*[ENDIF] CRIU_SUPPORT */
	private MethodType intern() {
		
		MethodType type = probeTable();
		if (type != null) {
			return type;
		}
	
		/* Check for the item under lock to prevent multiple threads from 
		 * successfully interning the same MethodType.  Use a lock object
		 * other than the intern table so other threads can continue to
		 * probe the table without waiting for unrelated MTs to be interned.
		 */
		synchronized(internTableAddLock) {
			type = probeTable();
			if (type != null) {
				return type;
			}
			
			int stackSlots = ptypes.length;
			
			for(Class<?> c : ptypes) {
				/*[IF ]*/
				/* getClass() gets compiled to just a NULLCHK and consumes fewer bytecodes than 'if (c == null) throw ...' */
				/*[ENDIF]*/
				c.getClass();	// Implicit nullcheck
				if ((c == double.class) || (c == long.class)) {
					stackSlots++;
				} else if (c == void.class){
					/*[MSG "K05d9", "invalid parameter: {}"]*/
					throw new IllegalArgumentException(Msg.getString("K05d9", void.class)); //$NON-NLS-1$
				}
			}
			if (stackSlots > 255) {
				/*[MSG "K05d8", "MethodType would consume more than 255 argument slots: {0}"]*/
				throw new IllegalArgumentException(Msg.getString("K05d8", stackSlots)); //$NON-NLS-1$
			}
			argSlots = stackSlots;

			/* initialize expensive state */
			stackDescriptionBits = stackDescriptionBits(ptypes, argSlots);
			methodDescriptor = createMethodDescriptorString();

			MethodType tenured = makeTenured(this);
			internTable.put(tenured, new WeakReference<MethodType>(tenured));
			return tenured;
		}
	}
	
	/* Check if the current MethodType is already cached */
	private MethodType probeTable() {
		Reference<MethodType> reference = internTable.get(this);
		if (reference != null) {
			return reference.get();
		}
		return null;
	}
	
	/*[IF ]*/
	/*
	 * Native method that creates a copy of the object in old-space
	 * so that the ConstantPool entries do not have to be walked by 
	 * the gencon gc policy.
	 * This treats interned MethodTypes the same way String constants
	 * in the ConstantPool are treated.
	 */
	/*[ENDIF]*/
	private static native MethodType makeTenured(MethodType toTenure);

	/**
	 * Wrapper on {@link MethodType#methodType(Class, Class[])}.
	 * <br>
	 * parameter0 is appended to the remaining parameters.
	 * 
	 * @param type - the return type
	 * @param parameter0 - the first parameter
	 * @param parameters - the remaining parameters
	 * 
	 * @return a MethodType object
	 */
	public static MethodType methodType(Class<?> type, Class<?> parameter0, Class<?>... parameters){
		Class<?>[] params = new Class<?>[parameters.length + 1];
		params[0] = parameter0;
		System.arraycopy(parameters, 0, params, 1, parameters.length);
		return methodType(type, params, false);
	}
	     
	/**
	 * Wrapper on {@link MethodType#methodType(Class, Class[])}
	 * 
	 * @param type - the return type
	 * @param parameters - the parameter types
	 * 
	 * @return a MethodType object
	 */
	public static MethodType methodType(Class<?> type, List<Class<?>> parameters){
		return methodType(type, parameters.toArray(new Class<?>[parameters.size()]), false);
	}
	     
	/**
	 * Wrapper on {@link #methodType(Class, Class[])}.
	 * Return a MethodType made from the returnType and parameters of the passed in MethodType.
	 *   
	 * @param returnType - the return type of the new MethodHandle
	 * @param methodType - the MethodType to take the parameter types from 
	 * @return a MethodType object made by changing the return type of the passed in MethodType
	 */
	public static MethodType methodType(Class<?> returnType, MethodType methodType) {
		return methodType(returnType, methodType.ptypes, false);
	}

	/**
	 * Static helper method to create a MethodType with only Object return type and parameters.
	 * 
	 * @param numParameters - number of parameters 
	 * @return a MethodType using only Object for return and parameter types.
	 * @throws IllegalArgumentException if numParameters is less than 0 or greater than the allowed number of arguments
	 */
	public static MethodType genericMethodType(int numParameters) throws IllegalArgumentException {
		return genericMethodType(numParameters, false);
	}
	     
	/**
	 * Wrapper on {@link #methodType(Class, Class[])}.
	 * <br>
	 * Return a MethodType with an Object return and only Object parameters.  If isVarargs is true, the
	 * final parameter will be an Object[].  This Object[] parameter is NOT included in the numParameters
	 * count. 
	 * 
	 * @param numParameters - number of parameters not including the isVarargs parameter (if requested)
	 * @param isVarargs - if the Object[] parameter should be added
	 * @return the requested MethodType object
	 * @throws IllegalArgumentException if numParameters is less than 0 or greater than the allowed number of arguments (255 or 254 if isVarargs)
	 * 
	 */
	public static MethodType genericMethodType(int numParameters, boolean isVarargs) throws IllegalArgumentException {
		if ((numParameters < 0) || (numParameters > (isVarargs ? 254 : 255))) {
			throw new IllegalArgumentException();
		}
		int count = numParameters;
		if (isVarargs) {
			count++;
		}
		Class<?>[] params = new Class<?>[count];
		Arrays.fill(params, Object.class);
		if (isVarargs){
			params[numParameters] = Object[].class;
		}
		return methodType(Object.class, params, false);	
	}
		
	/**
	 * Helper method to return the parameter types in an array.
	 * <br>  
	 * Changes to the array do not affect the MethodType object.
	 * 
	 * @return the parameter types as an array
	 */
	public Class<?>[] parameterArray() {
		return ptypes.clone();
	}

	/**
	 * Helper method to return the number of parameters
	 * 
	 * @return the number of parameters
	 */
	public int parameterCount() {
		return ptypes.length;
	}

	/**
	 * Method to return array of arguments
	 *
	 * @return array of arguments
	 */
	Class<?>[] ptypes() {
		return ptypes;
	}

	/**
	 * Helper method to return the parameter types in a List.
	 * <br>  
	 * Changes to the List do not affect the MethodType object.
	 * 
	 * @return the parameter types as a List
	 */
	public List<Class<?>> parameterList() {
		List<Class<?>> list = Arrays.asList(ptypes.clone()); 
		return Collections.unmodifiableList(list);
	}     

	/**
	 * Return the type of the parameter at position.
	 * 
	 * @param position - the parameter to get the type of
	 * @return the parameter's type
	 * @throws IndexOutOfBoundsException if position is less than 0 or an invalid argument index.
	 */
	public Class<?> parameterType(int position) throws IndexOutOfBoundsException {
		/*[IF ]*/
		// IndexOutOfBoundsException is a superclass of ArrayIndexOutOfBoundsException so we are
		// able to remove the explicit bounds checking.
		/*[ENDIF]*/
		return ptypes[position];
	}
	
	/**
	 * @return the type of the return
	 */
	public Class<?> returnType() {
		return rtype;
	}
	
	/**
	 * Return the last class in the parameterType array.
	 * This is equivalent to:
	 * 		type.parameterType(type.parameterCount() - 1)
	 * @return class of final parameter, or void.class if there are no parameters
	 */
	/*[IF JAVA_SPEC_VERSION >= 10]*/
	public
	/*[ENDIF] JAVA_SPEC_VERSION >= 10 */
	Class<?> lastParameterType() {
		Class<?> result = void.class;
		if (ptypes.length > 0) {
			result = ptypes[ptypes.length - 1];
		}
		return result;
	}
	
	/**
	 * Create a method descriptor string for this MethodType.
	 * 
	 * @return a method descriptor string
	 * @see #fromMethodDescriptorString(String, ClassLoader)
	 */
	public String toMethodDescriptorString(){
		return methodDescriptor;
	}
	
	private String createMethodDescriptorString(){
		StringBuilder sb= new StringBuilder("("); //$NON-NLS-1$
		for (Class<?> c : ptypes){
			sb.append(MethodTypeHelper.getBytecodeStringName(c));
		}
		sb.append(")"); //$NON-NLS-1$
		sb.append(MethodTypeHelper.getBytecodeStringName(rtype));
		return sb.toString();
	}
	
	
	/**
	 * Return a string representation of the MethodType in the form: '(A0,A2,A3...)R'.
	 * The simple name of each class is used.
	 * <p>
	 * Note that this is not the same as {@link #toMethodDescriptorString()}
	 */
	@Override
	public String toString(){
		StringBuilder sb = new StringBuilder("("); //$NON-NLS-1$
		for (int i = 0; i < ptypes.length; i++){
			sb.append(ptypes[i].getSimpleName());
			if (i < (ptypes.length - 1)) {
				sb.append(","); //$NON-NLS-1$
			}
		}
		sb.append(")"); //$NON-NLS-1$
		sb.append(rtype.getSimpleName());
		return sb.toString();
	}

	/**
	 * Wrapper method on {@link #methodType(Class, Class[])}.  Replaces all wrapper types with
	 * the appropriate primitive types, including changing {@link java.lang.Void} to 
	 * void.
	 * 
	 * @return a MethodType without any wrapper types
	 * @see #wrap()
	 */
	public MethodType unwrap(){
		Class<?>[] args = ptypes.clone();
		for(int i = 0; i < args.length; i++){
			args[i] = MethodTypeHelper.unwrapPrimitive(args[i]);
		}
		Class<?> unwrappedReturnType = MethodTypeHelper.unwrapPrimitive(rtype);
		if (MethodTypeHelper.unwrapPrimitive(rtype) == Void.class) {
			unwrappedReturnType = void.class;
		}
		return methodType(unwrappedReturnType, args, false);
	}
	
    
	/**
	 * Wrapper method on {@link #methodType(Class, Class[])}.  Replaces all primitive types with
	 * the appropriate wrapper types, including changing void to {@link java.lang.Void}.
	 * 
	 * @return a MethodType without any primitive types
	 * @see #unwrap()
	 */
	public MethodType wrap(){
		Class<?>[] args = ptypes.clone();
		for(int i = 0; i < args.length; i++){
			args[i] = MethodTypeHelper.wrapPrimitive(args[i]);
		}
		return methodType(MethodTypeHelper.wrapPrimitive(rtype), args, false);
	}
	
	/**
	 * Returns a MethodType with the additional class types appended to the end.
	 * 
	 * @param classes - the new parameter types to add to the end of the MethodType's argument types
	 * @return a MethodType with the additional argument types
	 * @throws IllegalArgumentException - if void.class is one of the classes or if the resulting MethodType would have more then 255 arguments
	 * @throws NullPointerException - if the <i>classes</i> array is null or contains null
	 */
	public MethodType appendParameterTypes(Class<?>... classes) throws IllegalArgumentException, NullPointerException {
		if (classes == null) {
			throw new NullPointerException();
		}
		return appendParameterTypes(Arrays.asList(classes));
	}
	
	/**
	 * Returns a MethodType with the additional class types appended to the end.
	 * 
	 * @param classes - the new parameter types to add to the end of the MethodType's argument types
	 * @return a MethodType with the additional argument types
	 * @throws IllegalArgumentException - if void.class is one of the classes or if the resulting MethodType would have more then 255 arguments
	 * @throws NullPointerException - if the <i>classes</i> is null or contains null
	 */
	public MethodType appendParameterTypes(List<Class<?>> classes) throws IllegalArgumentException, NullPointerException {
		if (classes == null) {
			throw new NullPointerException();
		}
		ArrayList<Class<?>> combinedParameters = new ArrayList<Class<?>>(Arrays.asList(ptypes));
		combinedParameters.addAll(classes);
		return methodType(rtype, combinedParameters);
	}

	/*
	 * Implements JSR 292 serialization spec.
	 * Write the MethodType without writing the field names.
	 * Format:
	 * { returnType class
	 *   arguments[]
	 * }
	 */
	private void writeObject(ObjectOutputStream out) throws IOException {
		out.defaultWriteObject();
		out.writeObject(returnType());
		/*[PR CMVC 197791] Must pass a copy of arguments array to prevent changes to arguments[] */ 
		out.writeObject(parameterArray());
	}
	
	/*
	 * Read in the serialized MethodType.  This instance will never escape.  It will
	 * be replaced by the MethodType created in readResolve() which does the proper
	 * validation.
	 */
	private void readObject(ObjectInputStream in) throws IOException, ClassNotFoundException {
		try {
			final Field fReturnType = getClass().getDeclaredField("rtype"); //$NON-NLS-1$
			final Field fArguments = getClass().getDeclaredField("ptypes"); //$NON-NLS-1$
			AccessController.doPrivileged(new PrivilegedAction<Object>() {
				public Object run() {
						fReturnType.setAccessible(true);
						fArguments.setAccessible(true);
						return null;
					}
				}
			);
			fReturnType.set(this, void.class);
			fArguments.set(this, EMTPY_PARAMS);
			methodDescriptor = "()V";
			stackDescriptionBits = stackDescriptionBits(EMTPY_PARAMS, 0);
			Class<?> serialReturnType = (Class<?>) in.readObject();
			Class<?>[] serialArguments = (Class<?>[]) in.readObject();
			deserializedFields = new DeserializedFieldsHolder(serialReturnType, serialArguments);
		} catch (IllegalAccessException e) {
		} catch (NoSuchFieldException e) {
		}
	}
	
	/* Hook to ensure that all objects go through the factory */
	@SuppressWarnings("unused")
	private Object readResolve() throws ObjectStreamException {
		Class<?> ret = void.class;
		Class<?>[] args = EMTPY_PARAMS;
		DeserializedFieldsHolder localArgs = deserializedFields;
		if (localArgs != null) {
			ret = deserializedFields.returnType;
			args = deserializedFields.arguments;
		}
		return MethodType.methodType(ret, args);
	}
	
	/*
	 * Return a cached instance of InvokeExactHandle for a given MethodType(this).
	 * If no cached instance exists, then create one.
	 * This method is meant to reduce any unnecessary allocations for InvokeExactHandle.
	 */
	InvokeExactHandle getInvokeExactHandle() {
		if (null == invoker) {
			synchronized(this) {
				if (null == invoker) {
					invoker = new InvokeExactHandle(this);
				}
			}
		}
		return invoker;
	}

/*[IF JAVA_SPEC_VERSION >= 12]*/
	static MethodType makeImpl(Class<?> rtype, Class<?>[] ptypes, boolean arg) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
/*[ENDIF] JAVA_SPEC_VERSION >= 12 */

/*[IF Sidecar18-SE-OpenJ9]*/	
	MethodType basicType() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	MethodType invokerType() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	MethodType replaceParameterTypes(int num1, int num2, Class<?>... clzs) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	MethodTypeForm form() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	Class<?> rtype() {
		return rtype;
	}

/*[IF Sidecar19-SE-OpenJ9]*/	
	MethodType asCollectorType(Class<?> clz, int num1, int num2) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}

	/*[IF JAVA_SPEC_VERSION >= 10]*/
	/**
	 * Returns the number of stack slots used by the described args in the MethodType.
	 * @return The number of stack slots
	 */
	int parameterSlotCount() {
		return argSlots;
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 10 */
/*[ELSE]*/
	MethodType asCollectorType(Class<?> clz, int num1) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
/*[ENDIF]*/
/*[ENDIF]*/

/*[IF JAVA_SPEC_VERSION >= 12]*/
	/**
	 * Return field descriptor of MethodType instance.
	 * 
	 * @return field descriptor of MethodType instance
	 */
	public String descriptorString() {
		return methodDescriptor;
	}

	/**
	 * Returns the nominal descriptor of this MethodType instance, or an empty Optional 
	 * if construction is not possible.
	 * 
	 * @return Optional with a nominal descriptor of MethodType instance
	 */
	public Optional<MethodTypeDesc> describeConstable() {
		try {
			ClassDesc returnDesc = rtype.describeConstable().orElseThrow();

			/* convert parameter classes to ClassDesc */
			final int argumentsLength = ptypes.length;
			ClassDesc[] paramDescs = new ClassDesc[argumentsLength];
			for (int i = 0; i < argumentsLength; i++) {
				paramDescs[i] = ptypes[i].describeConstable().orElseThrow();
			}

			/* create MethodTypeDesc */
			MethodTypeDesc typeDesc = MethodTypeDesc.of(returnDesc, paramDescs);
			return Optional.of(typeDesc);
		} catch(NoSuchElementException e) {
			return Optional.empty();
		}
	}
/*[ENDIF] JAVA_SPEC_VERSION >= 12 */
}
