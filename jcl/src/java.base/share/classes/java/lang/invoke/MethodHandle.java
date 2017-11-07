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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package java.lang.invoke;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.reflect.Modifier;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.util.Arrays;
import java.util.List;

import java.util.Objects;
// {{{ JIT support
import java.util.concurrent.ConcurrentHashMap;

/*[IF Sidecar19-SE]
import jdk.internal.misc.Unsafe;
import jdk.internal.misc.SharedSecrets;
import jdk.internal.reflect.ConstantPool;
/*[IF Sidecar18-SE-OpenJ9]
import java.lang.invoke.LambdaForm;
/*[ENDIF]*/
/*[ELSE]*/
import sun.misc.Unsafe;
import sun.misc.SharedSecrets;
import sun.reflect.ConstantPool;

/*[ENDIF]*/
import com.ibm.jit.JITHelpers;
import com.ibm.oti.util.Msg;
// }}} JIT support
import com.ibm.oti.vm.VM;
import com.ibm.oti.vm.VMLangAccess;

/**
 * A MethodHandle is a reference to a Java-level method.  It is typed according to the method's signature and can be
 * invoked in three ways:
 * <ol>
 * <li>invokeExact - using the exact signature match</li>
 * <li>invoke - using a signature with the same number of arguments</li>
 * <li>invokeWithArguments - using a Object array to hold the correct number of arguments</li>
 * </ol>
 * <p>
 * In the case of #invokeExact, if the arguments do not match, based on a check of the MethodHandle's {@link #type()}, 
 * a WrongMethodTypeException will be thrown.
 * <p>
 * In the case of #invoke, each of the arguments will be converted to the correct type, before the call is initiated. 
 * If the conversion cannot occur, a WrongMethodTypeException will be thrown.
 * <p>
 * Similar to #invoke, #invokeWithArguments will convert each of the arguments and place them on the stack before
 * the call is initiated. If the conversion cannot occur, a WrongMethodTypeException will be thrown.
 * <p>
 * A MethodHandle can be created using the MethodHandles factory.
 * 
 * @since 1.7
 */
@VMCONSTANTPOOL_CLASS
public abstract class MethodHandle {
	/* Ensure that these stay in sync with the MethodHandleInfo constants and VM constants in VM_MethodHandleKinds.h */
	/* Order matters here - static and special are direct pointers */
	static final byte KIND_BOUND = 0;
	static final byte KIND_GETFIELD = 1;
	static final byte KIND_GETSTATICFIELD = 2;
	static final byte KIND_PUTFIELD = 3;
	static final byte KIND_PUTSTATICFIELD = 4;
	static final byte KIND_VIRTUAL = 5;
	static final byte KIND_STATIC = 6;
	static final byte KIND_SPECIAL = 7;
	static final byte KIND_CONSTRUCTOR = 8;
	static final byte KIND_INTERFACE = 9;
	static final byte KIND_COLLECT = 10;
	static final byte KIND_INVOKEEXACT = 11;
	static final byte KIND_INVOKEGENERIC = 12;
	static final byte KIND_ASTYPE = 13;
	static final byte KIND_DYNAMICINVOKER = 14;
	static final byte KIND_FILTERRETURN = 15;
	static final byte KIND_EXPLICITCAST = 16;
	static final byte KIND_VARARGSCOLLECT = 17;
	static final byte KIND_PASSTHROUGH = 18;
	static final byte KIND_SPREAD = 19;
	static final byte KIND_INSERT = 20;
	static final byte KIND_PERMUTE = 21;
	static final byte KIND_CONSTANTOBJECT = 22;
	static final byte KIND_CONSTANTINT = 23;
	static final byte KIND_CONSTANTFLOAT = 24;
	static final byte KIND_CONSTANTLONG = 25;
	static final byte KIND_CONSTANTDOUBLE = 26;
	static final byte KIND_FOLDHANDLE = 27;
	static final byte KIND_GUARDWITHTEST = 28;
	static final byte KIND_FILTERARGUMENTS = 29;
	static final byte KIND_VARHANDLEINVOKEEXACT = 30;
	static final byte KIND_VARHANDLEINVOKEGENERIC = 31;
	/*[IF Panama]*/
	static final byte KIND_NATIVE = 32;
	/*[ENDIF]*/
	/*[IF Valhalla-MVT]*/
	static final byte KIND_DEFAULTVALUE = 33;
	/*[ENDIF]*/

/*[IF Sidecar18-SE-OpenJ9]
	MethodHandle asTypeCache = null;
	LambdaForm form = null;	
/*[ENDIF]*/	
	
	static final int PUBLIC_FINAL_NATIVE = Modifier.PUBLIC | Modifier.FINAL | Modifier.NATIVE | 0x1000 /* Synthetic */;
	
	static final Unsafe UNSAFE = Unsafe.getUnsafe();

	static final JITHelpers JITHELPERS = JITHelpers.getHelpers();

	private static final int CUSTOM_THUNK_INVOCATION_COUNT = 1000;
	private static final int DONT_CHECK_FOR_A_WHILE_COUNT  = -1000000000;

	final void doCustomizationLogic() {
		if (++invocationCount > CUSTOM_THUNK_INVOCATION_COUNT) {
			requestCustomThunk();
			invocationCount = DONT_CHECK_FOR_A_WHILE_COUNT;
			/*[IF ]*/
			/* Note: the way this is written, it can keep requesting custom
			thunks in a long-running server every few days indefinitely.
			We probably don't want to get any PMRs for a thunk compile that
			only occurs after a server has been up for 3 months, so there
			should probably be a way to stop compiling custom thunks forever. */
			/*[ENDIF]*/
		}
	}

	final void undoCustomizationLogic(MethodHandle callee) {
		--callee.invocationCount;
	}

	final void undoCustomizationLogic(MethodHandle callee1, MethodHandle callee2) {
		--callee1.invocationCount;
		--callee2.invocationCount;
	}

	final void undoCustomizationLogic(MethodHandle callee1, MethodHandle callee2, MethodHandle callee3) {
		--callee1.invocationCount;
		--callee2.invocationCount;
		--callee3.invocationCount;
	}

	final void undoCustomizationLogic(MethodHandle... callees) {
		for (MethodHandle callee: callees) {
			if (callee != null) {
				--callee.invocationCount;
			}
		}
	}

	final void requestCustomThunk() {
		thunks = ThunkTuple.copyOf(thunks);
		requestCustomThunkFromJit(thunks);
	}
	
	private native void requestCustomThunkFromJit(ThunkTuple tt);

	@VMCONSTANTPOOL_FIELD
	final MethodType type;		/* Type of the MethodHandle */
	@VMCONSTANTPOOL_FIELD
	final byte kind;				/* The kind (STATIC/SPECIAL/etc) of this MethodHandle */
	
	@VMCONSTANTPOOL_FIELD
	int invocationCount; /* used to determine how many times the MH has been invoked*/

	// {{{ JIT support

	ThunkTuple thunks;
	ThunkTable thunkTable() {
		throw new UnsupportedOperationException("Subclass must implement this"); //$NON-NLS-1$
	}
	ThunkTuple computeThunks(Object arg) {
		// Must be overridden for MethodHandle classes with different shareability criteria
		return thunkTable().get(new ThunkKey(ThunkKey.computeThunkableType(type())));
	}

	final long invokeExactTargetAddress() { return thunks.invokeExactThunk; }

	// TODO markers
	final static int VTABLE_ENTRY_SIZE = VM.ADDRESS_SIZE;
	final static int VTABLE_ENTRY_SHIFT = 31 - Integer.numberOfLeadingZeros(VTABLE_ENTRY_SIZE);
	final static int J9CLASS_OFFSET = vmRefFieldOffset(Class.class);
  	final static long INTRP_VTABLE_OFFSET = VM.J9CLASS_SIZE;
	final static int HEADER_SIZE = VM.OBJECT_HEADER_SIZE;

	static {
		// TODO:JSR292: Get rid of these if possible
		ComputedCalls.load();
		ThunkKey.load();
		ThunkTuple.load();
		ILGenMacros.load();
	}

	static long getJ9ClassFromClass(Class<?> c) {
		if (JITHELPERS.is32Bit()) {
			return JITHELPERS.getJ9ClassFromClass32(c);
		} else {
			return JITHELPERS.getJ9ClassFromClass64(c);
		}
	}

	/**
	 * Creates a new MethodHandle of the same kind that
	 * contains all the same fields, except the type is
	 * different.  Different from asType because no checks
	 * are done, the caller is responsible for determining
	 * if the operation will succeed.  Allows us to skip asType
	 * handles for reference castings we always know will succeed
	 * (such as Type to Object conversions)
	 * @param newType the new method type for this handle
	 * @returns a new MethodHandle with the newType expressed
	 */
	MethodHandle cloneWithNewType(MethodType newType) {
		throw new UnsupportedOperationException("Subclass must implement this"); //$NON-NLS-1$
	}

	/*[IF ]*/
	// Until the JIT can synthesize calls to virtual methods, we must synthesize calls to these static ones instead
	/*[ENDIF]*/
	private static MethodHandle asType(MethodHandle mh, MethodType newType) { return mh.asType(newType); }
	/*[IF Sidecar19-SE]*/
	private static MethodHandle asType(MethodHandle mh, MethodType newType, VarHandle varHandle) { return mh.asType(newType.appendParameterTypes(varHandle.getClass())); }
	/*[ENDIF]*/
	
	// Handy array to use to eliminate some corner cases without allocating new objects
	static final Class<?>[] EMPTY_CLASS_ARRAY = {};

	// }}} JIT support
	
	CacheKey cacheKey;			/* Strong reference to a CacheKey to ensure the WeakHashMap doesn't immediately collect the item */
	
	MethodHandle(MethodType type, byte kind, Object thunkArg) {
		this.kind = kind;
		/* Must be called last as it may use previously set fields to modify the MethodType */
		this.type = type;
		enforceArityLimit(kind, this.type);
		/* Must be called even laster as it uses the method type */
		this.thunks = computeThunks(thunkArg);
	}
	
	MethodHandle(MethodHandle original, MethodType newType) {
		this.kind = original.kind;
		this.type = newType;
		enforceArityLimit(original.kind, newType);
		this.thunks = original.thunks;
		this.previousAsType = original.previousAsType;
	}
	
	Class<?> getDefc() throws InternalError {
		/*[MSG "K05da", "Method is invalid on non-primitive MethodHandles."]*/
		throw new InternalError(Msg.getString("K05da")); //$NON-NLS-1$
	}
	
	Class<?> getReferenceClass() throws InternalError {
		/*[MSG "K05da", "Method is invalid on non-primitive MethodHandles."]*/
		throw new InternalError(Msg.getString("K05da")); //$NON-NLS-1$
	}
	
	Class<?> getSpecialCaller() throws InternalError {
		/*[MSG "K05da", "Method is invalid on non-primitive MethodHandles."]*/
		throw new InternalError(Msg.getString("K05da")); //$NON-NLS-1$
	}
	
	String getMethodName() throws InternalError {
		/*[MSG "K05da", "Method is invalid on non-primitive MethodHandles."]*/
		throw new InternalError(Msg.getString("K05da")); //$NON-NLS-1$
	}
	
	int getModifiers() throws InternalError {
		/*[MSG "K05da", "Method is invalid on non-primitive MethodHandles."]*/
		throw new InternalError(Msg.getString("K05da")); //$NON-NLS-1$
	}
	
	/*
	 * Marker interface for javac to recognize the polymorphic signature of the annotated methods.
	 */
	@Retention(RetentionPolicy.RUNTIME)
	@interface PolymorphicSignature{};
	
	@Retention(RetentionPolicy.RUNTIME)
	@Target({ElementType.METHOD})
	@interface FrameIteratorSkip{};
	
	/**
	 * Invoke the receiver MethodHandle against the supplied arguments.  The types of the arguments 
	 * must be an exact match for the MethodType of the MethodHandle.
	 * 
	 * @return The return value of the method
	 * @throws Throwable - To ensure type safety, must be marked as throwing Throwable.
	 * @throws WrongMethodTypeException - If the resolved method type is not exactly equal to the MethodHandle's type
	 */
	public final native @PolymorphicSignature Object invokeExact(Object... args) throws Throwable, WrongMethodTypeException;
	
	/**
	 * Invoke the receiver MethodHandle against the supplied arguments.  If the types of the arguments 
	 * are not an exact match for the MethodType of the MethodHandle, conversions will be applied as 
	 * possible.  The signature and the MethodHandle's MethodType must have the same number of arguments.
	 * 
	 * @return The return value of the method.  May be converted according to the conversion rules.
	 * @throws Throwable - To ensure type safety, must be marked as throwing Throwable.
	 * @throws WrongMethodTypeException  - If the resolved method type cannot be converted to the MethodHandle's type
	 * @throws ClassCastException - if a conversion fails
	 */
	public final native @PolymorphicSignature Object invoke(Object... args) throws Throwable, WrongMethodTypeException, ClassCastException;
	
	/*
	 * Lookup all the VMDispatchTargets and store them into 'targets' array.
	 * 'targets' must be large enough to hold all the dispatch targets.
	 * 
	 */
	private static native boolean lookupVMDispatchTargets(long[] targets);
	
	/**
     * The MethodType of the MethodHandle.  Invocation must match this MethodType.
     *   
	 * @return the MethodType of the MethodHandle.  
     */
	public MethodType type() {
		return type;
	}

	/**
	 * Produce a MethodHandle that has an array of type <i>arrayClass</i> as its last argument and replaces the
	 * array with <i>spreadCount</i> arguments from the array before calling the original MethodHandle.  The 
	 * MethodType of new the methodhandle and the original methodhandle will differ only in the regards to the 
	 * last arguments.
	 * <p>
	 * The array must contain exactly <i>spreadCount</i> arguments to be passed to the original methodhandle.  The array
	 * may be null in the case when <i>spreadCount</i> is zero.  Incorrect argument array size will cause the method to 
	 * throw an <code>IllegalArgumentException</code> instead of invoking the target.
	 * 
	 * @param arrayClass - the source array for the spread arguments
	 * @param spreadCount - how many arguments to spread from the arrayClass
	 * @return a MethodHandle able to replace to the last parameter with <i>spreadCount</i> number of arguments
	 * @throws IllegalArgumentException - if arrayClass is not an array, the methodhandle has too few or too many parameters to satisfy spreadCount
	 * @throws WrongMethodTypeException - if it cannot convert from one MethodType to the new type. 
	 */
	public MethodHandle asSpreader(Class<?> arrayClass, int spreadCount) throws IllegalArgumentException, WrongMethodTypeException {
		return asSpreaderCommon(type.parameterCount() - spreadCount, arrayClass, spreadCount);
	}
	
	/*[IF Sidecar19-SE]*/
	/**
	 * Produce a MethodHandle that has an array of type <i>arrayClass</i> as its argument at the specified position 
	 * and replaces the array with <i>spreadCount</i> arguments from the array before calling the original MethodHandle.
	 * The MethodType of new the methodhandle and the original methodhandle will differ only in the regards to the 
	 * argument at the specified position.
	 * <p>
	 * The array must contain exactly <i>spreadCount</i> arguments to be passed to the original methodhandle.  The array
	 * may be null in the case when <i>spreadCount</i> is zero.  Incorrect argument array size will cause the method to 
	 * throw an <code>IllegalArgumentException</code> instead of invoking the target.
	 * 
	 * @param spreadPosition - the starting position to spread
	 * @param arrayClass - the source array for the spread arguments
	 * @param spreadCount - how many arguments to spread from the arrayClass
	 * @return a MethodHandle able to replace to the specified parameter with <i>spreadCount</i> number of arguments
	 * @throws IllegalArgumentException - if arrayClass is not an array, the methodhandle has too few or too many parameters to satisfy spreadCount
	 * @throws WrongMethodTypeException - if it cannot convert from one MethodType to the new type. 
	 */
	public MethodHandle asSpreader(int spreadPosition, Class<?> arrayClass, int spreadCount) throws IllegalArgumentException, WrongMethodTypeException {
		return asSpreaderCommon(spreadPosition, arrayClass, spreadCount);
	}
	/*[ENDIF]*/
	
	/* The common code shared by asSpreader() with the last argument and with the argument at the specified position */
	private final MethodHandle asSpreaderCommon(int spreadPosition, Class<?> arrayClass, int spreadCount) throws IllegalArgumentException, WrongMethodTypeException {
		final int length = type.parameterCount();
		
		if (!arrayClass.isArray() || (spreadPosition < 0) || (spreadPosition > length)) {
			throw new IllegalArgumentException();
		}
		if ((spreadCount < 0) || (spreadCount > length)) {
			throwIllegalArgumentExceptionForMHArgCount();
		}
		MethodHandle adapted = this;
		MethodType collectType;

		if (0 == spreadCount) {
			collectType = type.insertParameterTypes(spreadPosition, arrayClass);
		} else {
			Class<?> componentType = arrayClass.getComponentType();
			
			collectType = type.changeParameterType(spreadPosition, arrayClass);
			if (spreadCount > 1) {
				/* Drop the remaining parameters by (spreadCount - 1) right after the specified position */
				collectType = collectType.dropParameterTypes(spreadPosition + 1, spreadPosition + spreadCount);
			}
			
			Class<?>[] parameters = type.arguments.clone();
			Arrays.fill(parameters, spreadPosition, spreadPosition + spreadCount, componentType);
			adapted = asType(MethodType.methodType(type.returnType, parameters));
		}
		return new SpreadHandle(adapted, collectType, arrayClass, spreadCount, spreadPosition);
	}
	

	/**
	 * Returns a MethodHandle that collects the requested incoming arguments, which must match the
	 * types in MethodType incomingArgs, into an array of <i>arrayClass</i>, called T.
	 * 
	 * This method can only be called on MethodHandles that have type() such that their last parameter
	 * can be assigned to from an instance of <i>arrayClass</i>.  An IllegalArgumentException will be 
	 * thrown if this is not the case.
	 * 
	 * This take a MH with type (Something, Something, K)R and presents a MethodType with the form
	 * (Something, Something, T, T, T)R. Where K is assignable to from an array of <i>arrayClass</i> T.
	 * 
	 * @param arrayClass - the class of the collect array.  Usually matches the type of the last argument.
	 * @param collectCount - the number of arguments of type 'T' to collect
	 * @return a MethodHandle which will collect <i>collectCount</i> arguments and pass them as the final argument
	 * 
	 * @throws IllegalArgumentException if arrayClass is not an array or is not assignable to the last parameter of the MethodHandle, or collectCount is an invalid array size (less than 0 or more than 254)
	 * @throws WrongMethodTypeException if an asType call would fail when converting the final parameter to arrayClass
	 * @throws NullPointerException if arrayClass is null
	 */
	public MethodHandle asCollector(Class<?> arrayClass, int collectCount) throws IllegalArgumentException, WrongMethodTypeException, NullPointerException {
		return asCollectorCommon(type.parameterCount() - 1, arrayClass, collectCount);
	}
	
	/*[IF Sidecar19-SE]*/
	/**
	 * Returns a MethodHandle that collects the requested incoming arguments, which must match the
	 * types in MethodType incomingArgs, into an array of <i>arrayClass</i>, called T.
	 * 
	 * @param collectPosition - the starting position for the arguments to collect.
	 * @param arrayClass - the class of the collect array.  It matches the type of the argument at the specified position.
	 * @param collectCount - the number of arguments of type 'T' to collect
	 * @return a MethodHandle which will collect <i>collectCount</i> arguments and pass them as the final argument
	 * 
	 * @throws IllegalArgumentException if arrayClass is not an array or is not assignable to the last parameter of the MethodHandle, or collectCount is an invalid array size (less than 0 or more than 254)
	 * @throws WrongMethodTypeException if an asType call would fail when converting the final parameter to arrayClass
	 * @throws NullPointerException if arrayClass is null
	 */
	public MethodHandle asCollector(int collectPosition, Class<?> arrayClass, int collectCount) throws IllegalArgumentException, WrongMethodTypeException, NullPointerException {
		return asCollectorCommon(collectPosition, arrayClass, collectCount);
	}
	/*[ENDIF]*/
	
	/* The common code shared by asCollector() with the last argument and with the argument at the specified position */
	private final MethodHandle asCollectorCommon(int collectPosition, Class<?> arrayClass, int collectCount) throws IllegalArgumentException, WrongMethodTypeException, NullPointerException {
		int parameterCount = type.parameterCount();
		if ((0 == parameterCount) || (collectCount < 0) || (collectCount > 255)
			|| (collectPosition < 0) || (collectPosition >= parameterCount)
		) {
			throw new IllegalArgumentException();
		}
		arrayClass.getClass();  // Implicit Null check
		Class<?> positionClass = type.parameterType(collectPosition);
		if (!arrayClass.isArray() || !positionClass.isAssignableFrom(arrayClass)) {
			/*[MSG "K05cc", "Cannot assign '{0}' to methodtype '{1}'"]*/
			throw new IllegalArgumentException(Msg.getString("K05cc", arrayClass.toString(), type.toString())); //$NON-NLS-1$
		}
		return new CollectHandle(asType(type.changeParameterType(collectPosition, arrayClass)), collectCount, collectPosition);
	}
	
	@VMCONSTANTPOOL_FIELD
	private MethodHandle previousAsType;

	/**
	 * Returns a MethodHandle that presents as being of MethodType newType.  It will 
	 * convert the arguments used to match type().  If a conversion is invalid, a
	 * ClassCastException will be thrown.
	 * 
	 * If newType == type(), then the original MethodHandle may be returned. 
	 * 
	 * TODO: Describe the type conversion rules here.
	 * If the return type T1 is void, any returned value is discarded
	 * If the return type T0 is void and T1 a reference, a null value is introduced.
	 * If the return type T0 is void and T1 a primitive, a zero value is introduced. 
	 *  
	 * @param newType the MethodType for invoking this method with
	 * @return A MethodHandle with MethodType newType
	 * 
	 * @throws ClassCastException if any of the requested coercions are invalid.
	 */
	public MethodHandle asType(MethodType newType) throws ClassCastException {
		if (this.type.equals(newType)) {
			return this;
		}
		MethodHandle localPreviousAsType = previousAsType;
		if ((localPreviousAsType != null) && (localPreviousAsType.type == newType)) {
			return localPreviousAsType;
		}
		MethodHandle handle = this;
		Class<?> fromReturn = type.returnType;
		Class<?> toReturn = newType.returnType;
		if (fromReturn != toReturn) {
			if(toReturn.isAssignableFrom(fromReturn)){
				handle = cloneWithNewType(MethodType.methodType(toReturn, type.arguments));
			} else {
				MethodHandle filter = ConvertHandle.FilterHelpers.getReturnFilter(fromReturn, toReturn, false);
				handle = new FilterReturnHandle(this, filter);
			}
		}
		if (handle.type != newType) {
			handle = new AsTypeHandle(handle, newType);
		}
		previousAsType = handle;
		return handle;
	}

	/* Unused class parameter is necessary to work around a javac bug */
	private static final native int vmRefFieldOffset(Class<?> unused);
	
	
	/**
	 * Invoke the MethodHandle using an Object[] of arguments.  The array must contain at exactly type().parameterCount() arguments.
	 * 
	 * Each of the arguments in the array with be coerced to the appropriate type, if possible, based on the MethodType.
	 * 
	 * @param args An array of Arguments, with length at exactly type().parameterCount() to be used in the call.
	 * @return An Object
	 * 
	 * @throws Throwable May throw anything depending on the receiver MethodHandle.
	 * @throws WrongMethodTypeException if the target cannot be adjusted to the number of Objects being passed 
	 * @throws ClassCastException if an argument cannot be converted
	 */
	public Object invokeWithArguments(Object... args) throws Throwable, WrongMethodTypeException, ClassCastException {
		int argsLength = 0;
		if (args != null) {
			argsLength = args.length;
		}
		/* We know the only accepted argument length ahead of time,
		 * so reject all calls with the wrong argument count
		 */
		if (argsLength != type.arguments.length) {
			throw newWrongMethodTypeException(type, args, argsLength);
		}
		if (argsLength < 253) {
			return IWAContainer.getMH(type.arguments.length).invokeExact(this, args);
		}
		return this.asSpreader(Object[].class, argsLength).invoke(args);
	}
	
	private static WrongMethodTypeException newWrongMethodTypeException(MethodType type, Object[] args, int argsLength) {
		Class<?>[] classes = new Class<?>[argsLength];
		for (int i = 0; i < argsLength; i++) {
			Class<?> c = Void.class;
			Object o = args[i];
			if (o != null) {
				c = o.getClass();
			}
			classes[i] = c;
		}
		return WrongMethodTypeException.newWrongMethodTypeException(type, MethodType.methodType(type.returnType, classes));
	}
	static final class IWAContainer {
		static MethodHandle getMH(int len){
			switch(len){
				case 0:
					return spreader_0;
				case 1:
					return spreader_1;
				case 2:
					return spreader_2;
				case 3:
					return spreader_3;
				case 4:
					return spreader_4;
				case 5:
					return spreader_5;
				case 6:
					return spreader_6;
				case 7:
					return spreader_7;
				default:
					return MethodHandles.spreadInvoker(MethodType.genericMethodType(len), 0);
			}
		}
		
		final static MethodHandle spreader_0 = MethodHandles.spreadInvoker(MethodType.genericMethodType(0), 0);
		final static MethodHandle spreader_1 = MethodHandles.spreadInvoker(MethodType.genericMethodType(1), 0);
		final static MethodHandle spreader_2 = MethodHandles.spreadInvoker(MethodType.genericMethodType(2), 0);
		final static MethodHandle spreader_3 = MethodHandles.spreadInvoker(MethodType.genericMethodType(3), 0);
		final static MethodHandle spreader_4 = MethodHandles.spreadInvoker(MethodType.genericMethodType(4), 0);
		final static MethodHandle spreader_5 = MethodHandles.spreadInvoker(MethodType.genericMethodType(5), 0);
		final static MethodHandle spreader_6 = MethodHandles.spreadInvoker(MethodType.genericMethodType(6), 0);
		final static MethodHandle spreader_7 = MethodHandles.spreadInvoker(MethodType.genericMethodType(7), 0);
		
	}

	
	private static native Object invokeWithArgumentsHelper(MethodHandle mh, Object[] args);
	
	/**
	 * Helper method to call {@link #invokeWithArguments(Object[])}.
	 * 
	 * @param args - An array of arguments, with length at exactly type().parameterCount() to be used in the call.
	 * @return An Object
	 * @throws Throwable May throw anything depending on the receiver MethodHandle.
	 * @throws WrongMethodTypeException if the target cannot be adjusted to the number of Objects being passed 
	 * @throws ClassCastException if an argument cannot be converted
	 * @throws NullPointerException if the args list is null
	 */
	public Object invokeWithArguments(List<?> args) throws Throwable, WrongMethodTypeException, ClassCastException, NullPointerException {
		return invokeWithArguments(args.toArray());
	}
	
	/**
	 * Create an varargs collector adapter on this MethodHandle.
	 * 
	 * For {@link #asVarargsCollector(Class)} MethodHandles, <i>invokeExact</i> requires that the arguments
	 * exactly match the underlying MethodType.
	 * <p>
	 * <i>invoke</i> acts as normally unless the arities differ.  In that case, the trailing
	 * arguments are converted as though by a call to {@link #asCollector(int)} before invoking the underlying
	 * methodhandle.
	 * 
	 * @param arrayParameter - the type of the array to collect the arguments into
	 * @return a varargs-collector methodhandle.
	 * @throws IllegalArgumentException - if the arrayParameter is not an array class or cannot be assigned to the last parameter of the MethodType
	 */
	public MethodHandle	asVarargsCollector(Class<?> arrayParameter) throws IllegalArgumentException {
		if (!arrayParameter.isArray()) {
			throw new IllegalArgumentException();
		}
		Class<?> lastArgType = type().lastParameterType();
		if (!lastArgType.isAssignableFrom(arrayParameter)) {
			throw new IllegalArgumentException();
		}
		return new VarargsCollectorHandle(this, arrayParameter, false);
	}

	/**
	 * Determine whether this is an {@link #asVarargsCollector(Class)} MethodHandle.
	 * 
	 * @return true if an {@link #asVarargsCollector(Class)} handle, false otherwise.
	 */
	public boolean isVarargsCollector() {
		return (this instanceof VarargsCollectorHandle);
	}
	
	/*[IF Sidecar19-SE]*/
	/**
	 * Return a MethodHandle that is wrapped with an asVarargsCollector adapter
	 * if the handle is allowed to be variable arity.
	 * 
	 * @param isVarArityNeeded - a boolean flag to tell whether the handle is converted to be variable arity or not.
	 * @return a MethodHandle with variable arity or fixed arity if isVarArityNeeded is false
	 * 
	 * @throws IllegalArgumentException if isVarArity is true but there is no trailing array parameter in the parameter list of the handle.
	 */
	public MethodHandle withVarargs(boolean isVarArityNeeded) throws IllegalArgumentException {
		MethodHandle result = this;
		
		if (isVarArityNeeded) {
			/* Convert the handle if initially not with variable arity */
			if (!this.isVarargsCollector()) {
				Class<?> lastClass = null;
				try {
					/* An exception is thrown if the MethodType has no arguments */
					lastClass = this.type.lastParameterType();
				} catch(RuntimeException e) {
					throw new IllegalArgumentException();
				}
				/* IllegalArgumentException will be thrown out from asVarargsCollector 
				 * if no trailing array parameter in this handle.
				 */
				result = this.asVarargsCollector(lastClass);
			}
		} else {
			result = this.asFixedArity();
		}
		
		return result;
	}
	/*[ENDIF]*/

	/**
	 * 
	 * @return Whether MethodHandles.revealDirect can be called on this MethodHandle
	 */
	boolean canRevealDirect() {
		return false;
	}

	/**
	 * @return If this DirectHandle was created for findVirtual / unreflect to handle
	 * non-vtable methods (ie: final or private)
	 */
	boolean directHandleOriginatedInFindVirtual() {
		return false;
	}
	
	/**
	 * Return a fixed arity version of the current MethodHandle.
	 * 
	 * <p>
	 * This is identical to the current method handle if {@link #isVarargsCollector()} is false.
	 * <p>
	 * If the current method is a varargs collector, then the returned handle will be the same 
	 * but without the varargs nature.  
	 * 
	 * @return a fixed arity version of the current method handle
	 */
	public MethodHandle asFixedArity() {
		return this;
	}
	
	public MethodHandle bindTo(Object value) throws IllegalArgumentException, ClassCastException {
		/*
		 * Check whether the first parameter has a reference type assignable from value. Note that MethodType.parameterType(0) will
		 * throw an IllegalArgumentException if type has no parameters.
		 */
		Class<?> firstParameterType = type().parameterType(0);
		if (firstParameterType.isPrimitive()) {
			throw new IllegalArgumentException();
		}

		/*
		 * Ensure type compatibility.
		 */
		value = firstParameterType.cast(value);

		/*
		 * A DirectHandle can be wrapped in a ReceiverBoundHandle, but a ReceiverBoundHandle cannot be wrapped in another ReceiverBoundHandle.
		 */
		if (getClass() == DirectHandle.class) {
			return new ReceiverBoundHandle((PrimitiveHandle)this, value, MethodHandles.insertArguments(this, 0, value));
		}

		/*
		 * Binding the first argument of any other kind of MethodHandle is equivalent to calling MethodHandles.insertArguments.
		 */
		return MethodHandles.insertArguments(this, 0, value);
	}

	/**
	 * Create an interception point to allow PermuteHandle to merge 
	 * permute(permute(originalHandle, ...), ...).
	 * This should only be directly called by:
	 * <ul>
	 * <li>{@link MethodHandles#permuteArguments(MethodHandle, MethodType, int...)}</i>
	 * <li>{@link MethodHandles#insertArguments(MethodHandle, int, Object...)}</i>
	 * </ul> 
	 */
	MethodHandle permuteArguments(MethodType permuteType, int... permute) {
		MethodHandle result = new PermuteHandle(permuteType, this, permute);
		if (true) {
			result = new BruteArgumentMoverHandle(permuteType, this, permute, EMPTY_CLASS_ARRAY, result);
		}
		return result;
	}

	MethodHandle insertArguments(MethodHandle equivalent, MethodHandle unboxingHandle, int location, Object... values) {
		MethodHandle result = equivalent;

		// Wrap result in an ArgumentMoverHandle
		int numValues = values.length;
		MethodType mtype = equivalent.type();
		int[] permute = ArgumentMoverHandle.insertPermute(ArgumentMoverHandle.identityPermute(mtype), location, numValues, -1);
		if (true) {
			result = new BruteArgumentMoverHandle(mtype, unboxingHandle, permute, values, result);
		}

		return result;
	}

	/*
	 * Return the result of J9_CP_TYPE(J9Class->romClass->cpShapeDescription, index)
	 */
	private static final native int getCPTypeAt(Object internalRamClass, int index);

	/*
	 * sun.reflect.ConstantPool doesn't have a getMethodTypeAt method.  This is the 
	 * equivalent for MethodType.
	 */
	private static final native MethodType getCPMethodTypeAt(Object internalRamClass, int index);

	/*
	 * sun.reflect.ConstantPool doesn't have a getMethodHandleAt method.  This is the 
	 * equivalent for MethodHandle.
	 */
	private static final native MethodHandle getCPMethodHandleAt(Object internalRamClass, int index);
	
	/**
	 * Get the class name from a constant pool class element, which is located
	 * at the specified <i>index</i> in <i>clazz</i>'s constant pool.
	 * 
	 * @param   an instance of class - its constant pool is accessed
	 * @param   the constant pool index
	 * 
	 * @return  instance of String which contains the class name or NULL in
	 *          case of error
	 * 
	 * @throws  NullPointerException if <i>clazz</i> is null
	 * @throws  IllegalArgumentException if <i>index</i> has wrong constant pool type
	 */
	private static final native String getCPClassNameAt(Class<?> clazz, int index);
	
	private static final int BSM_ARGUMENT_SIZE = Short.SIZE / Byte.SIZE;
	private static final int BSM_ARGUMENT_COUNT_OFFSET = BSM_ARGUMENT_SIZE;
	private static final int BSM_ARGUMENTS_OFFSET = BSM_ARGUMENT_SIZE * 2;
	private static final int BSM_LOOKUP_ARGUMENT_INDEX = 0;
	private static final int BSM_NAME_ARGUMENT_INDEX = 1;
	private static final int BSM_TYPE_ARGUMENT_INDEX = 2;
	private static final int BSM_OPTIONAL_ARGUMENTS_START_INDEX = 3;

	@SuppressWarnings("unused")
	private static final MethodHandle resolveInvokeDynamic(long j9class, String name, String methodDescriptor, long bsmData) throws Throwable {
		MethodHandle result = null;
		MethodType type = null;

		try {
			VMLangAccess access = VM.getVMLangAccess();
			Object internalRamClass = access.createInternalRamClass(j9class);
			Class<?> classObject = null;
			if (JITHELPERS.is32Bit()) {
				classObject = JITHELPERS.getClassFromJ9Class32((int)j9class);
			} else {
				classObject = JITHELPERS.getClassFromJ9Class64(j9class);
			}
			
			Objects.requireNonNull(classObject);
			
			try { 
				type = MethodType.fromMethodDescriptorString(methodDescriptor, access.getClassloader(classObject));
			} catch (TypeNotPresentException e) {
				throw throwNoClassDefFoundError(e);
			}
			int bsmIndex = UNSAFE.getShort(bsmData);
			int bsmArgCount = UNSAFE.getShort(bsmData + BSM_ARGUMENT_COUNT_OFFSET);
			long bsmArgs = bsmData + BSM_ARGUMENTS_OFFSET;
			MethodHandle bsm = getCPMethodHandleAt(internalRamClass, bsmIndex);
			if (null == bsm) {
				/*[MSG "K05cd", "unable to resolve 'bootstrap_method_ref' in '{0}' at index {1}"]*/
				throw new NullPointerException(Msg.getString("K05cd", classObject.toString(), bsmIndex)); //$NON-NLS-1$
			}
			Object[] staticArgs = new Object[BSM_OPTIONAL_ARGUMENTS_START_INDEX + bsmArgCount];
			/* Mandatory arguments */
			staticArgs[BSM_LOOKUP_ARGUMENT_INDEX] = new MethodHandles.Lookup(classObject, false);
			staticArgs[BSM_NAME_ARGUMENT_INDEX] = name;
			staticArgs[BSM_TYPE_ARGUMENT_INDEX] = type;
		
			/* Static optional arguments */
			/* internalRamClass is not a j.l.Class object but the ConstantPool natives know how to
			 * get the internal constantPool from the j9class
			 */
			ConstantPool cp = access.getConstantPool(internalRamClass);

			/* Check if we need to treat the last parameter specially when handling primitives.
			 * The type of the varargs array will determine how primitive ints from the constantpool
			 * get boxed: {Boolean, Byte, Short, Character or Integer}.
			 */ 
			boolean treatLastArgAsVarargs = bsm.isVarargsCollector();
			Class<?> varargsComponentType = bsm.type.lastParameterType().getComponentType();
			int bsmTypeArgCount = bsm.type.parameterCount();

			for (int i = 0; i < bsmArgCount; i++) {
				int staticArgIndex = BSM_OPTIONAL_ARGUMENTS_START_INDEX + i;
				short index = UNSAFE.getShort(bsmArgs + (i * BSM_ARGUMENT_SIZE));
				int cpType = getCPTypeAt(internalRamClass, index);
				Object cpEntry = null;
				switch (cpType) {
				case 1:
					cpEntry = cp.getClassAt(index);
					if (cpEntry == null) {
						throw throwNoClassDefFoundError(classObject, index);
					}
					break;
				case 2:
					cpEntry = cp.getStringAt(index);
					break;
				case 3: {
					int cpValue = cp.getIntAt(index);
					Class<?> argClass;
					if (treatLastArgAsVarargs && (staticArgIndex >= (bsmTypeArgCount - 1))) {
						argClass = varargsComponentType;
					} else {
						argClass = bsm.type.parameterType(staticArgIndex);
					}
					if (argClass == Short.TYPE) {
						cpEntry = (short) cpValue;
					} else if (argClass == Boolean.TYPE) {
						cpEntry = cpValue == 0 ? Boolean.FALSE : Boolean.TRUE;
					} else if (argClass == Byte.TYPE) {
						cpEntry = (byte) cpValue;
					} else if (argClass == Character.TYPE) {
						cpEntry = (char) cpValue;
					} else {
						cpEntry = cpValue;
					}
					break;
				}
				case 4:
					cpEntry = cp.getFloatAt(index);
					break;
				case 5:
					cpEntry = cp.getLongAt(index);
					break;
				case 6:
					cpEntry = cp.getDoubleAt(index);
					break;
				case 13:
					try {
						cpEntry = getCPMethodTypeAt(internalRamClass, index);
					} catch (TypeNotPresentException e) {
						throw throwNoClassDefFoundError(e);
					}
					break;
				case 14:
					cpEntry = getCPMethodHandleAt(internalRamClass, index);
					break;
				default:
					// Do nothing. The null check below will throw the appropriate exception.
				}
				
				cpEntry.getClass();	// Implicit NPE
				staticArgs[staticArgIndex] = cpEntry;
			}

			/* Take advantage of the per-MH asType cache */
			CallSite cs = null;
			switch(staticArgs.length) {
			case 3:
				cs = (CallSite) bsm.invoke(staticArgs[0], staticArgs[1], staticArgs[2]);
				break;
			case 4:
				cs = (CallSite) bsm.invoke(staticArgs[0], staticArgs[1], staticArgs[2], staticArgs[3]);
				break;
			case 5:
				cs = (CallSite) bsm.invoke(staticArgs[0], staticArgs[1], staticArgs[2], staticArgs[3], staticArgs[4]);
				break;
			case 6:
				cs = (CallSite) bsm.invoke(staticArgs[0], staticArgs[1], staticArgs[2], staticArgs[3], staticArgs[4], staticArgs[5]);
				break;
			case 7:
				cs = (CallSite) bsm.invoke(staticArgs[0], staticArgs[1], staticArgs[2], staticArgs[3], staticArgs[4], staticArgs[5], staticArgs[6]);
				break;
			default:
				cs = (java.lang.invoke.CallSite) bsm.invokeWithArguments(staticArgs);
				break;
			}
			
			if (cs != null) {
				MethodType callsiteType = cs.type();
				if (callsiteType != type) {
					throw WrongMethodTypeException.newWrongMethodTypeException(type, callsiteType);
				}
				result = cs.dynamicInvoker();
			}
		} catch(Throwable e) {

			/*[IF Sidecar19-SE]*/
			if (e instanceof Error) {
				throw e;
			}
			/*[ENDIF]*/

			if (type == null) {
				throw new BootstrapMethodError(e);
			}
			
			/* create an exceptionHandle with appropriate drop adapter and install that */
			try {
				MethodHandle thrower = MethodHandles.throwException(type.returnType(), BootstrapMethodError.class);
				MethodHandle constructor = MethodHandles.Lookup.IMPL_LOOKUP.findConstructor(BootstrapMethodError.class, MethodType.methodType(void.class, Throwable.class));
				result = MethodHandles.foldArguments(thrower, constructor.bindTo(e));
				result = MethodHandles.dropArguments(result, 0, type.parameterList()); 
			} catch (IllegalAccessException iae) {
				throw new Error(iae);
			} catch (NoSuchMethodException nsme) {
				throw new Error(nsme);
			}
		}
		
		return result;
	}
	
	/**
	 * Helper method to throw NoClassDefFoundError if the cause of TypeNotPresentException 
	 * is ClassNotFoundException. Otherwise, re-throw TypeNotPresentException.
	 * 
	 * @param   an instance of TypeNotPresentException
	 * 
	 * @return  Throwable to prevent any fall through case
	 * 
	 * @throws  NoClassDefFoundError if the cause of TypeNotPresentException is 
	 *          ClassNotFoundException. Otherwise, re-throw TypeNotPresentException.
	 */
	private static Throwable throwNoClassDefFoundError(TypeNotPresentException e) {
		Throwable cause = e.getCause();
		if (cause instanceof ClassNotFoundException) {
			NoClassDefFoundError noClassDefFoundError = new NoClassDefFoundError(cause.getMessage());
			noClassDefFoundError.initCause(cause);
			throw noClassDefFoundError;
		}
		throw e;
	}

	/**
	 * Retrieve the class name of the constant pool class element located at the specified
	 * index in clazz's constant pool. Then, throw a NoClassDefFoundError with the cause
	 * set as ClassNotFoundException. The message of NoClassDefFoundError and 
	 * ClassNotFoundException contains the name of the class, which couldn't be found.
	 * 
	 * @param   an instance of Class - its constant pool is accessed
	 * @param   integer value of the constant pool index
	 * 
	 * @return  Throwable to prevent any fall through case
	 * 
	 * @throws  NoClassDefFoundError with the cause set as ClassNotFoundException
	 */
	private static Throwable throwNoClassDefFoundError(Class<?> clazz, int index) {
		String className = getCPClassNameAt(clazz, index);
		NoClassDefFoundError noClassDefFoundError = new NoClassDefFoundError(className);
		noClassDefFoundError.initCause(new ClassNotFoundException(className));
		throw noClassDefFoundError;	
	}
	
	@Override
	public String toString() {
		return "MethodHandle" + type.toString(); //$NON-NLS-1$
	}

	/*[IF ]*/
	final void dumpTo(java.io.PrintStream out) {
		dumpTo(out, "", "", new java.util.HashSet()); //$NON-NLS-1$ //$NON-NLS-2$
	}
	
	final void dumpTo(java.io.PrintStream out, String prefix, String label, java.util.Set<MethodHandle> alreadyDumped) {
		boolean isRepeat = alreadyDumped.contains(this);
		String id = super.toString();
		id = id.substring(id.lastIndexOf('.'));
		out.println(prefix + label + (isRepeat? "^" : " ") + id + type.toString()); //$NON-NLS-1$ //$NON-NLS-2$
		if (!isRepeat)
			{
			alreadyDumped.add(this);
			dumpDetailsTo(out, prefix + indent, alreadyDumped);
			}
	}

	private final static String indent = "  "; //$NON-NLS-1$
	void dumpDetailsTo(java.io.PrintStream out, String prefix, java.util.Set<MethodHandle> alreadyDumped){}

	private static void print(Object[] args) {
		StringBuilder sb = new StringBuilder();
		String sep = ""; //$NON-NLS-1$
		for (Object x: args) {
			sb.append(sep).append(x.toString());
			sep = ", "; //$NON-NLS-1$
		}
		System.out.println(sb);
	}

	static MethodHandle printer() {
		try {
			return Lookup.internalPrivilegedLookup
				.findStatic(MethodHandle.class, "print",
					MethodType.fromMethodDescriptorString("([Ljava/lang/Object;)V", null))
				.asVarargsCollector(Object[].class); //$NON-NLS-1$ //$NON-NLS-2$
		} catch (Exception e){ throw new Error(e); }
	}

	/*[ENDIF]*/
	
	/*
	 * Used to convert an invokehandlegeneric bytecode into an AsTypeHandle + invokeExact OR to
	 * convert an InvokeGenericHandle into an AsTypeHandle.
	 * 
	 * Allows us to only have the conversion logic for the AsTypeHandle and not worry about any
	 * other similar conversions.
	 */
	@SuppressWarnings("unused")  /* Used by builder */
	@VMCONSTANTPOOL_METHOD
	private final MethodHandle forGenericInvoke(MethodType newType, boolean dropFirstArg){
		if (this.type == newType) {
			return this;
		}
		if (dropFirstArg) {
			return asType(newType.dropFirstParameterType());
		}
		return asType(newType);
	}
	
	@SuppressWarnings("unused")
	@VMCONSTANTPOOL_METHOD
	private static final MethodHandle sendResolveMethodHandle(
			int cpRefKind,
			Class<?> currentClass,
			Class<?> referenceClazz,
			String name,
			String typeDescriptor,
			ClassLoader loader) throws ClassNotFoundException, IllegalAccessException, NoSuchFieldException, NoSuchMethodException {
		MethodHandles.Lookup lookup = new MethodHandles.Lookup(currentClass, false);
		MethodType type = null;
		
		switch(cpRefKind){
		case 1: /* getField */
			return lookup.findGetter(referenceClazz, name, resolveFieldHandleHelper(typeDescriptor, loader));
		case 2: /* getStatic */
			return lookup.findStaticGetter(referenceClazz, name, resolveFieldHandleHelper(typeDescriptor, loader));
		case 3: /* putField */
			return lookup.findSetter(referenceClazz, name, resolveFieldHandleHelper(typeDescriptor, loader));
		case 4: /* putStatic */
			return lookup.findStaticSetter(referenceClazz, name, resolveFieldHandleHelper(typeDescriptor, loader));
		case 5: /* invokeVirtual */
			type = MethodType.fromMethodDescriptorString(typeDescriptor, loader);
			return lookup.findVirtual(referenceClazz, name, type);
		case 6: /* invokeStatic */
			type = MethodType.fromMethodDescriptorString(typeDescriptor, loader);
			return lookup.findStatic(referenceClazz, name, type);
		case 7: /* invokeSpecial */ 
			type = MethodType.fromMethodDescriptorString(typeDescriptor, loader);
			return lookup.findSpecial(referenceClazz, name, type, currentClass);
		case 8: /* newInvokeSpecial */
			type = MethodType.fromMethodDescriptorString(typeDescriptor, loader);
			return lookup.findConstructor(referenceClazz, type);
		case 9: /* invokeInterface */
			type = MethodType.fromMethodDescriptorString(typeDescriptor, loader);
			return lookup.findVirtual(referenceClazz, name, type);
		}
		/* Can never happen */
		throw new UnsupportedOperationException();
	}
	
	/* Convert the field typedescriptor into a MethodType so we can reuse the parsing logic in 
	 * #fromMethodDescritorString().  The verifier checks to ensure that the typeDescriptor is
	 * a valid field descriptor so adding the "()V" around it is valid.
	 */
	private static final Class<?> resolveFieldHandleHelper(String typeDescriptor, ClassLoader loader) {
		MethodType mt = MethodType.fromMethodDescriptorString("(" + typeDescriptor + ")V", loader); //$NON-NLS-1$ //$NON-NLS-2$
		return mt.parameterType(0);
	}
	
	/*[IF ]*/
	/*
	 * Used to preserve the MH on the stack when avoiding the call-in for
	 * filterReturn.  Must return 'this' so stackmapper will keep the MH
	 * alive.
	 */
	/*[ENDIF]*/
	@SuppressWarnings("unused")
	@VMCONSTANTPOOL_METHOD
	private MethodHandle returnFilterPlaceHolder() {
		return this;
	}
	
	/*[IF ]*/
	/*
	 * Used to preserve the MH on the stack when avoiding the call-in for
	 * foldHandle.  Must return 'this' so stackmapper will keep the MH
	 * alive.
	 */
	/*[ENDIF]*/
	@SuppressWarnings("unused")
	@VMCONSTANTPOOL_METHOD
	private MethodHandle foldHandlePlaceHolder() {
		return this;
	}
	
	/*[IF ]*/
	/*
	 * Used to preserve the MH on the stack when avoiding the call-in for
	 * guardWithTestHandle.  Must return 'this' so stackmapper will keep the MH
	 * alive.
	 */
	/*[ENDIF]*/
	@SuppressWarnings("unused")
	@VMCONSTANTPOOL_METHOD
	private MethodHandle guardWithTestPlaceHolder() {
		return this;
	}
	
	/*[IF ]*/
	/*
	 * Used to preserve the MH, index, parentOffset and nextOffset on the 
	 * stack when avoiding the call-in for FilterArgumentHandle.  Must   
	 * return 'this' so stackmapper will keep the MH and index alive.
	 */
	/*[ENDIF]*/
	@SuppressWarnings("unused")
	@VMCONSTANTPOOL_METHOD
	private MethodHandle filterArgumentsPlaceHolder(int index, int parentOffset, int nextOffset) {
		return this;
	}
	
	/*[IF ]*/
	/*
	 * Used to preserve the new objectRef on the stack when avoiding the call-in for
	 * constructorHandles.  Must return 'this' so stackmapper will keep the object
	 * alive.
	 */
	/*[ENDIF]*/
	@SuppressWarnings("unused")
	@VMCONSTANTPOOL_METHOD
	private static Object constructorPlaceHolder(Object newObjectRef) {
		return newObjectRef;
	}

	/*
	 * TODO: Javadoc this and it's contract
	 */
	void compareWith(MethodHandle other, Comparator c) {
		throw new UnsupportedOperationException("Subclass must implement this"); //$NON-NLS-1$
	}

	static final void enforceArityLimit(byte kind, MethodType type) {
		int argumentSlots = type.argSlots;
		
		/* The upper limit of argument slots is 255. For a constructor, 
		 * there should be at most 253 argument slots, one slot for
		 * MethodHandle itself and one slot for the placeholder of a 
		 * newly created object at the native level. So the slot occupied
		 * by the placeholder must be counted in when doing the arity check.
		 */
		if (KIND_CONSTRUCTOR == kind) {
			argumentSlots += 1;
		}
		
		if (argumentSlots > 254) {
			throwIllegalArgumentExceptionForMTArgCount(argumentSlots);
		}
	}
	
	static void throwIllegalArgumentExceptionForMTArgCount(int argSlots) {
		/*[MSG "K0578", "MethodHandle would consume more than 255 argument slots (\"{0}\")"]*/
		throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0578", argSlots + 1)); //$NON-NLS-1$
	}
	
	static void throwIllegalArgumentExceptionForMHArgCount() {
		/*[MSG "K0579", "The MethodHandle has too few or too many parameters"]*/
		throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0579")); //$NON-NLS-1$
	}
	
	String mapKindToBytecode() {
		switch(kind) {
		case KIND_GETFIELD: 			return "getField"; //$NON-NLS-1$
		case KIND_GETSTATICFIELD: 		return "getStatic"; //$NON-NLS-1$
		case KIND_PUTFIELD: 			return "putField"; //$NON-NLS-1$
		case KIND_PUTSTATICFIELD: 		return "putStatic"; //$NON-NLS-1$
		case KIND_BOUND:
		case KIND_VIRTUAL: 				return "invokeVirtual"; //$NON-NLS-1$
		case KIND_STATIC: 				return "invokeStatic"; //$NON-NLS-1$
		case KIND_SPECIAL: 				return "invokeSpecial"; //$NON-NLS-1$
		case KIND_CONSTRUCTOR: 			return "newInvokeSpecial"; //$NON-NLS-1$
		case KIND_INTERFACE: 			return "invokeInterface"; //$NON-NLS-1$
		case KIND_VARARGSCOLLECT:
			return ((VarargsCollectorHandle)this).next.mapKindToBytecode();
		}
		return "KIND_#"+kind; //$NON-NLS-1$
	}

/*[IF Sidecar18-SE-OpenJ9]*/
	MethodHandle(MethodType mt, LambdaForm lf) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	protected MethodHandle getTarget() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	MethodHandle setVarargs(MemberName member) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	public MethodHandle asTypeUncached(MethodType newType) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	boolean viewAsTypeChecks(MethodType newType, boolean strict) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	MethodHandle viewAsType(MethodType mt, boolean flag) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	MemberName internalMemberName() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	BoundMethodHandle rebind() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
/*[IF Sidecar19-SE-OpenJ9]*/
	void customize() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
/*[ENDIF]*/
	
	void updateForm(LambdaForm lf) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	final Object invokeBasic(Object... objs) throws java.lang.Throwable {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	MethodHandle withInternalMemberName(MemberName mn, boolean flag) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	LambdaForm internalForm() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	boolean isInvokeSpecial() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	Class<?> internalCallerClass() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	MethodHandleImpl.Intrinsic intrinsicName() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
	
	String internalProperties() {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
/*[ENDIF]*/
}

// {{{ JIT support
final class ComputedCalls {
	// Calls to these methods will get a "Computed Method" symref, and will in general have different arguments from those shown below

	public static native void     dispatchDirect_V(long address, int argPlaceholder);
	public static native int      dispatchDirect_I(long address, int argPlaceholder);
	public static native long     dispatchDirect_J(long address, int argPlaceholder);
	public static native float    dispatchDirect_F(long address, int argPlaceholder);
	public static native double   dispatchDirect_D(long address, int argPlaceholder);
	public static native Object   dispatchDirect_L(long address, int argPlaceholder);
	public static native void     dispatchDirect_V(long address, Object objectArg, int argPlaceholder);
	public static native int      dispatchDirect_I(long address, Object objectArg, int argPlaceholder);
	public static native long     dispatchDirect_J(long address, Object objectArg, int argPlaceholder);
	public static native float    dispatchDirect_F(long address, Object objectArg, int argPlaceholder);
	public static native double   dispatchDirect_D(long address, Object objectArg, int argPlaceholder);
	public static native Object   dispatchDirect_L(long address, Object objectArg, int argPlaceholder);

	public static native void     dispatchVirtual_V(long address, long vtableIndex, Object receiver, int argPlaceholder);
	public static native int      dispatchVirtual_I(long address, long vtableIndex, Object receiver, int argPlaceholder);
	public static native long     dispatchVirtual_J(long address, long vtableIndex, Object receiver, int argPlaceholder);
	public static native float    dispatchVirtual_F(long address, long vtableIndex, Object receiver, int argPlaceholder);
	public static native double   dispatchVirtual_D(long address, long vtableIndex, Object receiver, int argPlaceholder);
	public static native Object   dispatchVirtual_L(long address, long vtableIndex, Object receiver, int argPlaceholder);

	// These ones aren't computed calls, but are so similar to the computed calls, I thought I'd throw them in here.
	public static native void     dispatchJ9Method_V(long j9method, int argPlaceholder);
	public static native int      dispatchJ9Method_I(long j9method, int argPlaceholder);
	public static native long     dispatchJ9Method_J(long j9method, int argPlaceholder);
	public static native float    dispatchJ9Method_F(long j9method, int argPlaceholder);
	public static native double   dispatchJ9Method_D(long j9method, int argPlaceholder);
	public static native Object   dispatchJ9Method_L(long j9method, int argPlaceholder);
	public static native void     dispatchJ9Method_V(long j9method, Object objectArg, int argPlaceholder);
	public static native int      dispatchJ9Method_I(long j9method, Object objectArg, int argPlaceholder);
	public static native long     dispatchJ9Method_J(long j9method, Object objectArg, int argPlaceholder);
	public static native float    dispatchJ9Method_F(long j9method, Object objectArg, int argPlaceholder);
	public static native double   dispatchJ9Method_D(long j9method, Object objectArg, int argPlaceholder);
	public static native Object   dispatchJ9Method_L(long j9method, Object objectArg, int argPlaceholder);


	public static void load(){}
}

class ThunkKey {
	private static Class<?> normalize(Class<?> c) {
		// "Normalize" here means:
		// - All object types -> java.lang.Object
		// - Small int types  -> int
		//
		if (c.isPrimitive()) {
			if (  c == boolean.class
				|| c == byte.class
				|| c == char.class
				|| c == short.class){
				return int.class;
			} else {
				return c;
			}
		} else {
			return Object.class;
		}
	}

	public static MethodType computeThunkableType(MethodType type) // Normalize all arguments and the return type
		{ return computeThunkableType(type, type.parameterCount()); }

	public static MethodType computeThunkableType(MethodType type, int stopIndex) // stop upcasting arguments at stopIndex, but Normalize return type
		{ return computeThunkableType(type, stopIndex, type.parameterCount()); }

	public static MethodType computeThunkableType(MethodType type, int stopIndex, int restartIndex) { // don't Normalize anything between stopIndex and restartIndex-1
		Class<?> parameterTypes[] = new Class<?>[type.parameterCount()];
		int i;
		for (i=0; i < stopIndex; i++)
			parameterTypes[i] = normalize(type.parameterType(i));
		for (; i < restartIndex; i++)
			parameterTypes[i] = type.parameterType(i);
		for (; i < type.parameterCount(); i++)
			parameterTypes[i] = normalize(type.parameterType(i));
		MethodType result = MethodType.methodType(normalize(type.returnType()), parameterTypes);
		return result;
	}

	public boolean equals(Object other) {
		// Note the double-dispatch pattern here.  We effectively consult both
		// ThunkKeys to give either of them a chance to declare themselves unequal.
		if (other instanceof ThunkKey)
			return ((ThunkKey)other).equalsThunkKey(this);
		else
			return false;
	}

	public int hashCode() {
		/*[IF ]*/ 
		/* Rely on the fact that MethodType caches its hashcode */
		/*[ENDIF]*/
		return thunkableType.hashCode();
	}

	boolean equalsThunkKey(ThunkKey other) {
		return thunkableType.equals(other.thunkableType);
	}

	final MethodType thunkableType;

	public ThunkKey(MethodType t){ this.thunkableType = t; }

	public static void load(){}
}

final class UnshareableThunkKey extends ThunkKey {

	public UnshareableThunkKey(MethodType thunkableType){ super(thunkableType); }

	// Whether this ThunkKey is on the left or right side of the equals call,
	// we want it to return false.
	public    boolean equals(Object other) { return false; }
	boolean equalsThunkKey(ThunkKey other) { return false; }

}

final class ThunkKeyWithInt extends ThunkKey {
	private final int extraInt;
	public ThunkKeyWithInt(MethodType thunkableType, int extraInt){ super(thunkableType); this.extraInt = extraInt; }
	private int hashcode = 0;
	
	public boolean equals(Object other) {
		if (other instanceof ThunkKeyWithInt)
			return ((ThunkKeyWithInt)other).equalsThunkKeyWithInt(this);
		else
			return false;
	}

	final boolean equalsThunkKeyWithInt(ThunkKeyWithInt other) {
		return equalsThunkKey(other) && extraInt == other.extraInt;
	}

	public int hashCode() {
		if (hashcode == 0) {
			hashcode = super.hashCode() ^ (int)extraInt;
		}
		return hashcode;
	}
}

final class ThunkKeyWithLong extends ThunkKey {
	private final long extraLong;
	public ThunkKeyWithLong(MethodType thunkableType, long extraLong){ super(thunkableType); this.extraLong = extraLong; }
	private int hashcode = 0;
	
	public boolean equals(Object other) {
		if (other instanceof ThunkKeyWithLong)
			return ((ThunkKeyWithLong)other).equalsThunkKeyWithLong(this);
		else
			return false;
	}

	final boolean equalsThunkKeyWithLong(ThunkKeyWithLong other) {
		return equalsThunkKey(other) && extraLong == other.extraLong;
	}

	public int hashCode() {
		if (hashcode == 0) {
			hashcode = super.hashCode() ^ (int)extraLong ^ (int)(extraLong >> 32);
		}
		return hashcode;
	}
}

final class ThunkKeyWithObject extends ThunkKey {
	private final Object extraObject;
	public ThunkKeyWithObject(MethodType thunkableType, Object extraObject){ super(thunkableType); this.extraObject = extraObject; }
	private int hashcode = 0;
	
	public boolean equals(Object other) {
		if (other instanceof ThunkKeyWithObject)
			return ((ThunkKeyWithObject)other).equalsThunkKeyWithObject(this);
		else
			return false;
	}

	final boolean equalsThunkKeyWithObject(ThunkKeyWithObject other) {
		return equalsThunkKey(other) && extraObject.equals(other.extraObject);
	}

	public int hashCode() {
		if (hashcode == 0) {
			hashcode = super.hashCode() ^ extraObject.hashCode();
		}
		return hashcode;
	}
}

final class ThunkKeyWithObjectArray extends ThunkKey {
	private final Object[] extraObjectArray;
	public ThunkKeyWithObjectArray(MethodType thunkableType, Object[] extraObjectArray){ super(thunkableType); this.extraObjectArray = extraObjectArray; }
	private int hashcode = 0;
	
	public boolean equals(Object other) {
		if (other instanceof ThunkKeyWithObjectArray) {
			return ((ThunkKeyWithObjectArray)other).equalsThunkKeyWithObjectArray(this);
		} else {
			return false;
		}
	}

	final boolean equalsThunkKeyWithObjectArray(ThunkKeyWithObjectArray other) {
		return equalsThunkKey(other) && Arrays.deepEquals(extraObjectArray, other.extraObjectArray);
	}

	public int hashCode() {
		if (hashcode == 0) {
			hashcode = super.hashCode() ^ Arrays.deepHashCode(extraObjectArray);
		}
		return hashcode;
	}
}

final class ThunkKeyWithIntArray extends ThunkKey {
	private final int[] extraIntArray;
	public ThunkKeyWithIntArray(MethodType thunkableType, int[] extraIntArray){ super(thunkableType); this.extraIntArray = extraIntArray; }
	private int hashcode = 0;
	
	public boolean equals(Object other) {
		if (other instanceof ThunkKeyWithIntArray) {
			return ((ThunkKeyWithIntArray)other).equalsThunkKeyWithIntArray(this);
		} else {
			return false;
		}
	}

	final boolean equalsThunkKeyWithIntArray(ThunkKeyWithIntArray other) {
		return equalsThunkKey(other) && Arrays.equals(extraIntArray, other.extraIntArray);
	}

	public int hashCode() {
		if (hashcode == 0) {
			hashcode = super.hashCode() ^ Arrays.hashCode(extraIntArray);
		}
		return hashcode; 
	}
}

final class ThunkTuple {
	final String thunkableSignature;
	@VMCONSTANTPOOL_FIELD
	int invocationCount; /* used to determine how many times the thunkTuple has been invoked */
	
	/*[IF ]*/
	// Support natives defined in the JIT dll
	/*[ENDIF]*/
	static native void registerNatives();
	static {
		registerNatives();
	}
	
	/*[IF ]*/
	// Mutable fields updated by the JIT
	// These don't need to be volatile:
	// - We don't need any kind of memory barrier here
	// - On 32-bit, we don't care about word tearing because it's a 32-bit address
	// - On 64-bit, aligned 64-bit values won't get word tearing on any hardware we support
	//
	/*[ENDIF]*/
	long invokeExactThunk;
	@VMCONSTANTPOOL_FIELD
	long i2jInvokeExactThunk;

	static ThunkTuple newShareable(MethodType thunkableType) {
		return new ThunkTuple(thunkableType.toMethodDescriptorString(), initialInvokeExactThunk());
	}

	static ThunkTuple newCustom(MethodType thunkableType, long invokeExactThunk) {
		return new ThunkTuple(thunkableType.toMethodDescriptorString(), invokeExactThunk);
	}

 	static ThunkTuple copyOf(ThunkTuple tt){
 		ThunkTuple t = new ThunkTuple(tt.thunkableSignature, tt.invokeExactThunk);
 		t.i2jInvokeExactThunk = tt.i2jInvokeExactThunk;
 		return t;
	}

	private ThunkTuple(String thunkableSignature, long invokeExactThunk){
		this.thunkableSignature = thunkableSignature;
		this.invokeExactThunk   = invokeExactThunk;
	}

	private static native long initialInvokeExactThunk();

	public static void load(){}

	private static native void finalizeImpl(long invokeExactThunk);
	
	@Override
	protected void finalize() throws Throwable {
		finalizeImpl(invokeExactThunk);
	}
}

final class ThunkTable {

	private final ConcurrentHashMap<ThunkKey, ThunkTuple> tuples; // TODO:JSR292: use soft references

	static final class Properties {
		static final boolean DISABLE_THUNK_SHARING;
		static {
			DISABLE_THUNK_SHARING = AccessController.doPrivileged(new PrivilegedAction<Boolean>() {
				public Boolean run() {
					return Boolean.valueOf(Boolean.getBoolean("com.ibm.jsr292.disableThunkSharing")); //$NON-NLS-1$
				}
			}).booleanValue();
		}
	}

	ThunkTable(){
		this.tuples = new ConcurrentHashMap<ThunkKey, ThunkTuple>();
	}

	public ThunkTuple get(ThunkKey key) {
		ThunkTuple result = tuples.get(key);
		if (result == null) {
			result = ThunkTuple.newShareable(key.thunkableType); // Thunk tables only contain shareable thunks; no need to put custom thunks in a table
			if (!Properties.DISABLE_THUNK_SHARING) {
				ThunkTuple existingEntry = tuples.putIfAbsent(key, result);
				if (existingEntry != null)
					result = existingEntry;
			}
		}
		assert(result != null);
		assert(key.thunkableType.toMethodDescriptorString() == result.thunkableSignature);
		return result;
	}

	public static void load(){}
}

/**
 * An ILGenMacro method is executed at ilgen time by TR_ByteCodeIlGenerator::runMacro.
 *
 * @param argPlaceholder can be passed to other archetype calls.
 */
final class ILGenMacros {
	public static final native int    numArguments    (int argPlaceholder);
	public static final native Object populateArray   (Object array, int argPlaceholder); // returns the array
	public static final native int    arrayElements   (Object array,       int firstIndex, int numElements);
	public static final native int    arrayElements   (int arrayPlaceholder, int firstIndex, int numElements);
	public static final native int    firstN          (int n, int argPlaceholder);
	public static final native int    dropFirstN      (int n, int argPlaceholder);
	public static final native int    lastN           (int n, int argPlaceholder);
	public static final native int    middleN         (int startIndex, int n, int argPlaceholder); 
	public static final native Object rawNew          (Class<?> c);

	// Calls to these will disappear and be replaced by their arguments,
	// and will edit the parent call's signature appropriately.
	//
	// Add more overloads here as needed; all that matters is that it's called
	// ILGenMacros.placeholder and returns an int.
	//
	public static final native int    placeholder         (int argPlaceholder);
	public static final native int    placeholder         (Object arg1, int arg2);
	public static final native int    placeholder         (Object arg1, boolean arg2, int arg3);
	public static final native int    placeholder         (Object arg1, byte arg2, int arg3);
	public static final native int    placeholder         (Object arg1, char arg2, int arg3);
	public static final native int    placeholder         (Object arg1, short arg2, int arg3);
	public static final native int    placeholder         (Object arg1, int arg2, int arg3);
	public static final native int    placeholder         (Object arg1, long arg2, int arg3);
	public static final native int    placeholder         (Object arg1, float arg2, int arg3);
	public static final native int    placeholder         (Object arg1, double arg2, int arg3);
	public static final native int    placeholder         (Object arg1, Object arg2, int arg3);
	public static final native int    placeholder         (int arg1, Object arg2);
	public static final native int    placeholder         (int arg1, int arg2);
	public static final native int    placeholder         (int arg1, int arg2, int arg3);
	public static final native int    placeholder         (int arg1, Object arg2, int arg3);
	public static final native int    placeholder         (int arg1, Object arg2, Object arg3, int arg4);
	public static final native int    placeholder         (int arg1, Object arg2, Object arg3, Object arg4, int arg5);
	
	// Macros that take object expressions as arguments.
	// Only certain kinds of expressions are permitted: either 'this', or 'this' followed by a sequence of getfields.
	public static final native int    parameterCount  (MethodHandle handle);
	public static final native int    invokeExact     (MethodHandle handle, int argPlaceholder); // Does no null check or type check
	public static final native int    arrayLength     (Object array);

	// Operations manipulating the cold end of the operand stack (making it the "operand dequeue")
	public static final native int    push(int    arg);
	public static final native long   push(long   arg);
	public static final native float  push(float  arg);
	public static final native double push(double arg);
	public static final native Object push(Object arg);
	public static final native int    pop_I();
	public static final native long   pop_J();
	public static final native float  pop_F();
	public static final native double pop_D();
	public static final native Object pop_L();

	// MethodHandle semantics
	static native void typeCheck(MethodHandle handle, MethodType desiredType) throws WrongMethodTypeException;

	// MethodHandle.invokeExact without null checks or type checks, with explicit return types.
	// (These don't do the magic return-type fixing that the other invokeExact macro does.)
	static native int     invokeExact_X(MethodHandle handle, int argPlaceholder); // Takes on the return type of the containing archetype
	static native void    invokeExact_V(MethodHandle handle, int argPlaceholder);
	static native boolean invokeExact_Z(MethodHandle handle, int argPlaceholder);
	static native byte    invokeExact_B(MethodHandle handle, int argPlaceholder);
	static native short   invokeExact_S(MethodHandle handle, int argPlaceholder);
	static native char    invokeExact_C(MethodHandle handle, int argPlaceholder);
	static native int     invokeExact_I(MethodHandle handle, int argPlaceholder);
	static native long    invokeExact_J(MethodHandle handle, int argPlaceholder);
	static native float   invokeExact_F(MethodHandle handle, int argPlaceholder);
	static native double  invokeExact_D(MethodHandle handle, int argPlaceholder);
	static native Object  invokeExact_L(MethodHandle handle, int argPlaceholder);

	// Called from within thunk archetypes, these will indicate whether it's
	// safe to hard-code details of the specific receiver handle into the thunk,
	// or whether it should be kept generalized so it works on all handles
	// sharing the same ThunkTuple.
	static final native boolean isCustomThunk();
	static final native boolean isShareableThunk();

	public static void load(){}
}

// }}} JIT support


// {{{ Comparator support

/*
 * Comparators are used by MutableCallSite to determine if the newTarget is
 * "equivalent" to the original target.
 * 
 * Comparators currently only stop comparing at MethodHandle boundaries.
 */
abstract class Comparator {
	/*[IF ]*/
	// TODO: Separate the comparator object (which defines the kind of
	// comparison being performed) from the comparison state.  A MCS should be
	// able to have a comparator object that defines the circumstances in which
	// invalidation are necessary, but such an object is not thread-safe if it
	// also contains the state of the comparison operation.
	/*[ENDIF]*/
	
	public abstract void fail();
	public abstract boolean failed();

	abstract void compareUserSuppliedParameter (Object p1, Object p2);
	void compareUserSuppliedParameter (int    p1, int    p2){ compareUserSuppliedParameter((Integer)p1, (Integer)p2); }
	void compareUserSuppliedParameter (long   p1, long   p2){ compareUserSuppliedParameter((Long)p1, (Long)p2); }
	void compareUserSuppliedParameter (float  p1, float  p2){ compareUserSuppliedParameter((Float)p1, (Float)p2); }
	void compareUserSuppliedParameter (double p1, double p2){ compareUserSuppliedParameter((Double)p1, (Double)p2); }

	abstract void compareStructuralParameter (Object p1, Object p2);
	void compareStructuralParameter (int    p1, int    p2){ compareStructuralParameter((Integer)p1, (Integer)p2); }
	void compareStructuralParameter (long   p1, long   p2){ compareStructuralParameter((Long)p1, (Long)p2); }
	void compareStructuralParameter (float  p1, float  p2){ compareStructuralParameter((Float)p1, (Float)p2); }
	void compareStructuralParameter (double p1, double p2){ compareStructuralParameter((Double)p1, (Double)p2); }
	@SuppressWarnings("boxing")
 	void compareStructuralParameter (int[]  p1, int[]  p2){
 		compareStructuralParameter((Integer)p1.length, (Integer)p2.length);
 		int p1_length = p1.length;
 		for (int i = 0; i < p1_length; i++) {
 			compareStructuralParameter((Integer)p1[i], (Integer)p2[i]);	
 		}
 	}

	abstract void compareChildHandle (MethodHandle p1, MethodHandle p2);
}

/*
 * StructuralComparator is a comparator that only validates the final
 * fields of a MethodHandle subclass.  User-supplied values are ignored.
 */
final class StructuralComparator extends Comparator {

	final java.util.ArrayList<MethodHandle> visitQ = new java.util.ArrayList<MethodHandle>();
	boolean hasFailed = false;

	public void fail() { 
		hasFailed = true; 
	}
	public boolean failed() {
		return hasFailed; 
	}
	public void failUnless(boolean condition) {
		if (!condition) {
			hasFailed = true; 
		}
	}

	StructuralComparator() {}
	public static StructuralComparator get() { 
		return new StructuralComparator();
	}

	public boolean handlesAreEquivalent(MethodHandle leftRoot, MethodHandle rightRoot) {
		assert (visitQ.isEmpty());
		hasFailed = false;
		visitQ.add(leftRoot);
		visitQ.add(rightRoot);
		while (!failed() && !visitQ.isEmpty()) {
			MethodHandle left  = visitQ.get(0);
			MethodHandle right = visitQ.get(1);
			visitQ.subList(0,2).clear();
			if (left != right) {
				left.compareWith(right, this);
			}
		}
		return !failed();
	}

	// User-supplied parameters can be safely ignored
	void compareUserSuppliedParameter (Object p1, Object p2){}
	void compareUserSuppliedParameter (int    p1, int    p2){}
	void compareUserSuppliedParameter (long   p1, long   p2){}
	void compareUserSuppliedParameter (float  p1, float  p2){}
	void compareUserSuppliedParameter (double p1, double p2){}

	// Structural parameters must be identical
	void compareStructuralParameter (Object p1, Object p2){ failUnless(p1 == p2); } // TODO: could this be equals?
	void compareStructuralParameter (int    p1, int    p2){ failUnless(p1 == p2); }
	void compareStructuralParameter (long   p1, long   p2){ failUnless(p1 == p2); }
	void compareStructuralParameter (float  p1, float  p2){ failUnless(p1 == p2); }
	void compareStructuralParameter (double p1, double p2){ failUnless(p1 == p2); }
	void compareStructuralParameter (int[]  p1, int[]  p2){
		compareStructuralParameter(p1.length, p2.length);
		int p1_length = p1.length;
		for (int i = 0; i < p1_length; i++) {
			compareStructuralParameter(p1[i], p2[i]);	
		}
	}
	
	// Queue child handles rather than recursing
	void compareChildHandle(MethodHandle p1, MethodHandle p2) {
		if (!failed()) {
			if (p1 != p2) {
				visitQ.add(p1);
				visitQ.add(p2);
			}
		}
	}
}

// }}} Comparator support

