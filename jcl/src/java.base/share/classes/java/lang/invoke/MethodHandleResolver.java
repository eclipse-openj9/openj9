/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2020, 2021 IBM Corp. and others
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
import java.util.ArrayList;
import java.util.Objects;

import com.ibm.oti.util.Msg;
import com.ibm.oti.vm.VM;
import com.ibm.oti.vm.VMLangAccess;

/*[IF Sidecar19-SE]
import jdk.internal.misc.Unsafe;
import jdk.internal.reflect.ConstantPool;
/*[ELSE]*/
import sun.misc.Unsafe;
import sun.reflect.ConstantPool;
/*[ENDIF]*/

import com.ibm.jit.JITHelpers;

/**
 * Static methods for the MethodHandle class.
 */
final class MethodHandleResolver {
	static final Unsafe UNSAFE = Unsafe.getUnsafe();

	static final JITHelpers JITHELPERS = JITHelpers.getHelpers();
	
	private static final int BSM_ARGUMENT_SIZE = Short.SIZE / Byte.SIZE;
	private static final int BSM_ARGUMENT_COUNT_OFFSET = BSM_ARGUMENT_SIZE;
	private static final int BSM_ARGUMENTS_OFFSET = BSM_ARGUMENT_SIZE * 2;
	private static final int BSM_LOOKUP_ARGUMENT_INDEX = 0;
	private static final int BSM_NAME_ARGUMENT_INDEX = 1;
	private static final int BSM_TYPE_ARGUMENT_INDEX = 2;
	private static final int BSM_OPTIONAL_ARGUMENTS_START_INDEX = 3;

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

/*[IF JAVA_SPEC_VERSION >= 11]*/
	/*
	 * sun.reflect.ConstantPool doesn't have a getConstantDynamicAt method.  This is the 
	 * equivalent for ConstantDynamic.
	 */
	private static final native Object getCPConstantDynamicAt(Object internalRamClass, int index);

	@SuppressWarnings("unused")
	private static final Object resolveConstantDynamic(long j9class, String name, String fieldDescriptor, long bsmData) throws Throwable {
		Object result = null;

		VMLangAccess access = VM.getVMLangAccess();
		Object internalRamClass = access.createInternalRamClass(j9class);
		Class<?> classObject = getClassFromJ9Class(j9class);

		Class<?> typeClass = fromFieldDescriptorString(fieldDescriptor, access.getClassloader(classObject));

		int bsmIndex = UNSAFE.getShort(bsmData);
		int bsmArgCount = UNSAFE.getShort(bsmData + BSM_ARGUMENT_COUNT_OFFSET);
		long bsmArgs = bsmData + BSM_ARGUMENTS_OFFSET;

		MethodHandle bsm = getCPMethodHandleAt(internalRamClass, bsmIndex);
		if (null == bsm) {
			/*[MSG "K05cd", "unable to resolve 'bootstrap_method_ref' in '{0}' at index {1}"]*/
			throw new NullPointerException(Msg.getString("K05cd", classObject.toString(), bsmIndex)); //$NON-NLS-1$
		}

		/* Static optional arguments */
		int bsmTypeArgCount = bsm.type().parameterCount();

		/* JVMS JDK11 5.4.3.6 Dynamically-Computed Constant and Call Site Resolution
		 * requires the first parameter of the bootstrap method to be java.lang.invoke.MethodHandles.Lookup
		 * else fail resolution with BootstrapMethodError
		 */
		if (bsmTypeArgCount < 1 || MethodHandles.Lookup.class != bsm.type().parameterType(0)) {
			/*[MSG "K0A01", "Constant_Dynamic references bootstrap method '{0}' does not have java.lang.invoke.MethodHandles.Lookup as first parameter."]*/
			throw new BootstrapMethodError(Msg.getString("K0A01", Lookup.IMPL_LOOKUP.revealDirect(bsm).getName())); //$NON-NLS-1$
		}

/*[IF OPENJDK_METHODHANDLES]*/
		Object[] staticArgs = new Object[bsmArgCount];
		for (int i = 0; i < bsmArgCount; i++) {
			staticArgs[i] = getAdditionalBsmArg(access, internalRamClass, classObject, bsm, bsmArgs, bsmTypeArgCount, i);
		}
/*[ELSE] OPENJDK_METHODHANDLES*/
		Object[] staticArgs = new Object[BSM_OPTIONAL_ARGUMENTS_START_INDEX + bsmArgCount];
		/* Mandatory arguments */
		staticArgs[BSM_LOOKUP_ARGUMENT_INDEX] = new MethodHandles.Lookup(classObject, false);
		staticArgs[BSM_NAME_ARGUMENT_INDEX] = name;
		staticArgs[BSM_TYPE_ARGUMENT_INDEX] = typeClass;
		for (int i = 0; i < bsmArgCount; i++) {
			staticArgs[BSM_OPTIONAL_ARGUMENTS_START_INDEX + i] = getAdditionalBsmArg(access, internalRamClass, classObject, bsm, bsmArgs, bsmTypeArgCount, i);
		}
/*[ENDIF] OPENJDK_METHODHANDLES*/

		/* JVMS JDK11 5.4.3.6 Dynamically-Computed Constant and Call Site Resolution
		 * requires that exceptions from BSM invocation be wrapped in a BootstrapMethodError
		 * unless the exception thrown is a sub-class of Error.
		 * Exceptions thrown before invocation should be passed through unwrapped.
		 */
		try {
/*[IF OPENJDK_METHODHANDLES]*/
			result = MethodHandleNatives.linkDynamicConstant(classObject, 0, bsm, name, typeClass, (Object)staticArgs);
/*[ELSE] OPENJDK_METHODHANDLES*/
			result = (Object)invokeBsm(bsm, staticArgs);
/*[ENDIF] OPENJDK_METHODHANDLES*/
			/* Result validation. */
			result = MethodHandles.identity(typeClass).invoke(result);
		} catch(Throwable e) {
			if (e instanceof Error) {
				throw e;
			} else {
				/*[MSG "K0A00", "Failed to resolve Constant Dynamic entry with j9class: {0}, name: {1}, descriptor: {2}, bsmData: {3}"]*/
				String msg = Msg.getString("K0A00", new Object[] {String.valueOf(j9class), name, fieldDescriptor, String.valueOf(bsmData)}); //$NON-NLS-1$
				throw new BootstrapMethodError(msg, e);
			}
		}
		
		return result;
	}
/*[ENDIF] JAVA_SPEC_VERSION >= 11*/

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

/*[IF !OPENJDK_METHODHANDLES]*/
	/**
	 * Invoke bootstrap method with its static arguments
	 * @param bsm
	 * @param staticArgs
	 * @return result of bsm invocation
	 * @throws Throwable any throwable will be handled by the caller
	 */
	private static final Object invokeBsm(MethodHandle bsm, Object[] staticArgs) throws Throwable {
		Object result = null;
		/* Take advantage of the per-MH asType cache */
		switch (staticArgs.length) {
		case 3:
			result = bsm.invoke(staticArgs[0], staticArgs[1], staticArgs[2]);
			break;
		case 4:
			result = bsm.invoke(staticArgs[0], staticArgs[1], staticArgs[2], staticArgs[3]);
			break;
		case 5:
			result = bsm.invoke(staticArgs[0], staticArgs[1], staticArgs[2], staticArgs[3], staticArgs[4]);
			break;
		case 6:
			result = bsm.invoke(staticArgs[0], staticArgs[1], staticArgs[2], staticArgs[3], staticArgs[4], staticArgs[5]);
			break;
		case 7:
			result = bsm.invoke(staticArgs[0], staticArgs[1], staticArgs[2], staticArgs[3], staticArgs[4], staticArgs[5], staticArgs[6]);
			break;
		default:
			result = bsm.invokeWithArguments(staticArgs);
			break;
		}
		return result;
	}
/*[ENDIF] !OPENJDK_METHODHANDLES*/
	
	@SuppressWarnings("unused")
	private static final Object resolveInvokeDynamic(long j9class, String name, String methodDescriptor, long bsmData) throws Throwable {
/*[IF OPENJDK_METHODHANDLES]*/
		VMLangAccess access = VM.getVMLangAccess();
		Object internalRamClass = access.createInternalRamClass(j9class);
		Class<?> classObject = getClassFromJ9Class(j9class);

		MethodType type = MethodTypeHelper.vmResolveFromMethodDescriptorString(methodDescriptor, access.getClassloader(classObject), null);
		final MethodHandles.Lookup lookup = new MethodHandles.Lookup(classObject);
		inaccessibleTypeCheck(lookup, type);

		int bsmIndex = UNSAFE.getShort(bsmData);
		int bsmArgCount = UNSAFE.getShort(bsmData + BSM_ARGUMENT_COUNT_OFFSET);
		long bsmArgs = bsmData + BSM_ARGUMENTS_OFFSET;
		MethodHandle bsm = getCPMethodHandleAt(internalRamClass, bsmIndex);
		if (null == bsm) {
			/*[MSG "K05cd", "unable to resolve 'bootstrap_method_ref' in '{0}' at index {1}"]*/
			throw new NullPointerException(Msg.getString("K05cd", classObject.toString(), bsmIndex)); //$NON-NLS-1$
		}

		Object[] staticArgs = new Object[bsmArgCount];
		/* Static optional arguments */
		int bsmTypeArgCount = bsm.type().parameterCount();
		for (int i = 0; i < bsmArgCount; i++) {
			staticArgs[i] = getAdditionalBsmArg(access, internalRamClass, classObject, bsm, bsmArgs, bsmTypeArgCount, i);
		}

		Object[] appendixResult = new Object[1];
		Object[] result = new Object[2];

		/* result[0] stores a MemberName object, which specifies the caller method (generated bytecodes), or a Throwable.
		 *
		 * This leads to a type check in the interpreter for the object stored in result[0].
		 *
		 * TODO: Investigate if the Throwable can be wrapped in a MemberName. This will help prevent type checks in the
		 * interpreter since result[0] will always be a MemberName. This will improve performance.
		 */
		result[0] = MethodHandleNatives.linkCallSite(classObject, 0, bsm, name, type, (Object)staticArgs, appendixResult);

		/* result[1] stores a MethodHandle object, which is used as the last argument to the caller method. */
		result[1] = appendixResult[0];

		return (Object)result;
/*[ELSE] OPENJDK_METHODHANDLES*/	
		MethodHandle result = null;
		MethodType type = null;

/*[IF JAVA_SPEC_VERSION < 11]*/
		try {
/*[ENDIF] JAVA_SPEC_VERSION < 11 */
			VMLangAccess access = VM.getVMLangAccess();
			Object internalRamClass = access.createInternalRamClass(j9class);
			Class<?> classObject = getClassFromJ9Class(j9class);
			
			type = MethodTypeHelper.vmResolveFromMethodDescriptorString(methodDescriptor, access.getClassloader(classObject), null);
			final MethodHandles.Lookup lookup = new MethodHandles.Lookup(classObject, false);
			try {
				lookup.accessCheckArgRetTypes(type);
			} catch (IllegalAccessException e) {
				IllegalAccessError err = new IllegalAccessError();
				err.initCause(e);
				throw err;
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
			staticArgs[BSM_LOOKUP_ARGUMENT_INDEX] = lookup;
			staticArgs[BSM_NAME_ARGUMENT_INDEX] = name;
			staticArgs[BSM_TYPE_ARGUMENT_INDEX] = type;
		
			/* Static optional arguments */
			int bsmTypeArgCount = bsm.type().parameterCount();
			for (int i = 0; i < bsmArgCount; i++) {
				staticArgs[BSM_OPTIONAL_ARGUMENTS_START_INDEX + i] = getAdditionalBsmArg(access, internalRamClass, classObject, bsm, bsmArgs, bsmTypeArgCount, i);
			}

/*[IF JAVA_SPEC_VERSION >= 11]*/
		/* JVMS JDK11 5.4.3.6 Dynamically-Computed Constant and Call Site Resolution
		 * requires that exceptions from BSM invocation be wrapped in a BootstrapMethodError
		 * unless the exception thrown is a sub-class of Error.
		 * Exceptions thrown before invocation should be passed through unwrapped.
		 */
		try {
/*[ENDIF] JAVA_SPEC_VERSION >= 11 */
			CallSite cs = (CallSite)invokeBsm(bsm, staticArgs);
			if (cs != null) {
				MethodType callsiteType = cs.type();
				if (callsiteType != type) {
					throw WrongMethodTypeException.newWrongMethodTypeException(type, callsiteType);
				}
				result = cs.dynamicInvoker();
			}
/*[IF JAVA_SPEC_VERSION >= 11]*/
			else {
				/* The result of the resolution of a dynamically-computed call site must not be null. */
				/*[MSG "K0A02", "Bootstrap method returned null."]*/
				throw new ClassCastException(Msg.getString("K0A02")); //$NON-NLS-1$
			}
/*[ENDIF] JAVA_SPEC_VERSION >= 11 */
		} catch(Throwable e) {

/*[IF Sidecar19-SE]*/
			if (e instanceof Error) {
				throw e;
			}
/*[ENDIF] Sidecar19-SE */

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
/*[ENDIF] OPENJDK_METHODHANDLES*/
	}

/*[IF OPENJDK_METHODHANDLES]*/
	/**
	 * Check access to the parameter and return classes within incoming MethodType.
	 * 
	 * @param lookup the Lookup object used for the access check.
	 * @param type the incoming MethodType.
	 * 
	 * @throws IllegalAccessError if an inaccessible class is found.
	 */
	private static final void inaccessibleTypeCheck(Lookup lookup, MethodType type) throws IllegalAccessError {
		Class<?>[] ptypes = type.ptypes();
		for (Class<?> ptype : ptypes) {
			if (!ptype.isPrimitive()) {
				if (!lookup.isClassAccessible(ptype)) {
					/*[MSG "K0680", "Class '{0}' no access to: class '{1}'"]*/
					throw new IllegalAccessError(Msg.getString("K0680", lookup.lookupClass(), ptype));
				}
			}
		}

		Class<?> rtype = type.rtype();
		if (!rtype.isPrimitive()) {
			if (!lookup.isClassAccessible(rtype)) {
				/*[MSG "K0680", "Class '{0}' no access to: class '{1}'"]*/
				throw new IllegalAccessError(Msg.getString("K0680", lookup.lookupClass(), rtype));
			}
		}
	}
/*[ENDIF] OPENJDK_METHODHANDLES*/

	@SuppressWarnings("unused")
	@VMCONSTANTPOOL_METHOD
	private static final MethodHandle sendResolveMethodHandle(
			int cpRefKind,
			Class<?> currentClass,
			Class<?> referenceClazz,
			String name,
			String typeDescriptor,
			ClassLoader loader) throws Throwable {
		MethodType type = null;
/*[IF OPENJDK_METHODHANDLES]*/
		switch (cpRefKind) {
		case 1: /* getField */
		case 2: /* getStatic */
		case 3: /* putField */
		case 4: /* putStatic */
			type = MethodTypeHelper.vmResolveFromMethodDescriptorString("(" + typeDescriptor + ")V", loader, null);
			break;
		case 5: /* invokeVirtual */
		case 6: /* invokeStatic */
		case 7: /* invokeSpecial */
		case 8: /* newInvokeSpecial */
		case 9: /* invokeInterface */
			type = MethodTypeHelper.vmResolveFromMethodDescriptorString(typeDescriptor, loader, null);
			break;
		default:
			/*[MSG "K0686", "Unknown reference kind: '{0}'"]*/
			throw new InternalError(com.ibm.oti.util.Msg.getString("K0686", cpRefKind)); //$NON-NLS-1$
		}

		final MethodHandles.Lookup lookup = new MethodHandles.Lookup(currentClass);
		inaccessibleTypeCheck(lookup, type);
		
		return MethodHandleNatives.linkMethodHandleConstant(currentClass, cpRefKind, referenceClazz, name, type);
/*[ELSE] OPENJDK_METHODHANDLES*/
		try {
			MethodHandles.Lookup lookup = new MethodHandles.Lookup(currentClass, false);
			MethodHandle result = null;

			switch (cpRefKind) {
			case 1: /* getField */
				result = lookup.findGetter(referenceClazz, name, resolveFieldHandleHelper(typeDescriptor, lookup, loader));
				break;
			case 2: /* getStatic */
				result = lookup.findStaticGetter(referenceClazz, name, resolveFieldHandleHelper(typeDescriptor, lookup, loader));
				break;
			case 3: /* putField */
				result = lookup.findSetter(referenceClazz, name, resolveFieldHandleHelper(typeDescriptor, lookup, loader));
				break;
			case 4: /* putStatic */
				result = lookup.findStaticSetter(referenceClazz, name, resolveFieldHandleHelper(typeDescriptor, lookup, loader));
				break;
			case 5: /* invokeVirtual */
				type = MethodTypeHelper.vmResolveFromMethodDescriptorString(typeDescriptor, loader, null);
				lookup.accessCheckArgRetTypes(type);
				result = lookup.findVirtual(referenceClazz, name, type);
				break;
			case 6: /* invokeStatic */
				type = MethodTypeHelper.vmResolveFromMethodDescriptorString(typeDescriptor, loader, null);
				lookup.accessCheckArgRetTypes(type);
				result = lookup.findStatic(referenceClazz, name, type);
				break;
			case 7: /* invokeSpecial */ 
				type = MethodTypeHelper.vmResolveFromMethodDescriptorString(typeDescriptor, loader, null);
				lookup.accessCheckArgRetTypes(type);
				result = lookup.findSpecial(referenceClazz, name, type, currentClass);
				break;
			case 8: /* newInvokeSpecial */
				type = MethodTypeHelper.vmResolveFromMethodDescriptorString(typeDescriptor, loader, null);
				lookup.accessCheckArgRetTypes(type);
				result = lookup.findConstructor(referenceClazz, type);
				break;
			case 9: /* invokeInterface */
				type = MethodTypeHelper.vmResolveFromMethodDescriptorString(typeDescriptor, loader, null);
				lookup.accessCheckArgRetTypes(type);
				result = lookup.findVirtual(referenceClazz, name, type);
				break;
			default:
				/* Can never happen */
				throw new UnsupportedOperationException();
			}
			return result;
		} catch (IllegalAccessException iae) {
			/* Java spec expects an IllegalAccessError instead of IllegalAccessException thrown when an application attempts
			 * (not reflectively) to access or modify a field, or to invoke a method that it doesn't have access to.
			 */
			throw new IllegalAccessError(iae.getMessage()).initCause(iae);
		}
/*[ENDIF] OPENJDK_METHODHANDLES*/
	}
	
/*[IF !OPENJDK_METHODHANDLES]*/
	/* Convert the field type descriptor into a MethodType so we can reuse the parsing logic in 
	 * #fromMethodDescriptorString().  The verifier checks to ensure that the typeDescriptor is
	 * a valid field descriptor so adding the "()V" around it is valid.
	 */
	private static final Class<?> resolveFieldHandleHelper(String typeDescriptor, Lookup lookup, ClassLoader loader) throws Throwable {
		MethodType mt = MethodTypeHelper.vmResolveFromMethodDescriptorString("(" + typeDescriptor + ")V", loader, null); //$NON-NLS-1$ //$NON-NLS-2$
		lookup.accessCheckArgRetTypes(mt);
		return mt.parameterType(0);
	}
/*[ENDIF] !OPENJDK_METHODHANDLES*/

	
	/**
	 * Gets class object from RAM class address
	 * @param j9class RAM class address
	 * @return class object
	 * @throws Throwable any throwable will be handled by the caller
	 */
	private static final Class<?> getClassFromJ9Class(long j9class) throws Throwable {
		Class<?> classObject = null;
		if (JITHELPERS.is32Bit()) {
			classObject = JITHELPERS.getClassFromJ9Class32((int)j9class);
		} else {
			classObject = JITHELPERS.getClassFromJ9Class64(j9class);
		}
		Objects.requireNonNull(classObject);
		return classObject;
	}
	
	private static final Class<?> fromFieldDescriptorString(String fieldDescriptor, ClassLoader classLoader) {
		ArrayList<Class<?>> classList = new ArrayList<Class<?>>();
		int length = fieldDescriptor.length();
		if (length == 0) {
			/*[MSG "K05d3", "invalid descriptor: {0}"]*/
			throw new IllegalArgumentException(Msg.getString("K05d3", fieldDescriptor)); //$NON-NLS-1$
		}

		char[] signature = new char[length];
		fieldDescriptor.getChars(0, length, signature, 0);

		MethodTypeHelper.parseIntoClass(signature, 0, classList, classLoader, fieldDescriptor);

		return classList.get(0);
	}
	
	/**
	 * Retrieve a static argument of the bootstrap method at argIndex from constant pool
	 * @param access
	 * @param internalRamClass
	 * @param classObject RAM class object
	 * @param bsm bootstrap method
	 * @param bsmArgs starting address of bootstrap arguments in the RAM class call site
	 * @param bsmTypeArgCount number of bootstrap arguments
	 * @param argIndex index of bsm argument starting from zero. Argument should be in addition to the first three 
	 * 	mandatory arguments (3+): MethodHandles.Lookup, String, MethodType
	 * @return additional argument from the constant pool
	 * @throws Throwable any throwable will be handled by the caller
	 */
	private static final Object getAdditionalBsmArg(VMLangAccess access, Object internalRamClass, Class<?> classObject, MethodHandle bsm, long bsmArgs, int bsmTypeArgCount, int argIndex) throws Throwable {
		/* Check if we need to treat the last parameter specially when handling primitives.
		 * The type of the varargs array will determine how primitive ints from the constantpool
		 * get boxed: {Boolean, Byte, Short, Character or Integer}.
		 */ 
		boolean treatLastArgAsVarargs = bsm.isVarargsCollector();

		/* internalRamClass is not a j.l.Class object but the ConstantPool natives know how to
		* get the internal constantPool from the j9class
		*/
		ConstantPool cp = access.getConstantPool(internalRamClass);

		int staticArgIndex = BSM_OPTIONAL_ARGUMENTS_START_INDEX + argIndex;
		short index = UNSAFE.getShort(bsmArgs + (argIndex * BSM_ARGUMENT_SIZE));
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
			Class<?> argClass = null;
			if (treatLastArgAsVarargs && (staticArgIndex >= (bsmTypeArgCount - 1))) {
				argClass = bsm.type().lastParameterType().getComponentType(); /* varargs component type */
			} else {
				/* Verify that a call to MethodType.parameterType will not cause an ArrayIndexOutOfBoundsException. 
				* If the number of static arguments is greater than the number of argument slots in the bsm
				* leave argClass unset. A more meaningful user error WrongMethodTypeException will be thrown later on.
				*/
				if (staticArgIndex < bsmTypeArgCount) {
					argClass = bsm.type().parameterType(staticArgIndex);
				}
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
			cpEntry = getCPMethodTypeAt(internalRamClass, index);
			break;
		case 14:
			cpEntry = getCPMethodHandleAt(internalRamClass, index);
			break;
/*[IF JAVA_SPEC_VERSION >= 11]*/
		case 17:
			cpEntry = getCPConstantDynamicAt(internalRamClass, index);
			break;
/*[ENDIF] JAVA_SPEC_VERSION >= 11 */
		default:
			// Do nothing. The null check below will throw the appropriate exception.
		}
		
		cpEntry.getClass();	// Implicit NPE
		return cpEntry;
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
	private static final Throwable throwNoClassDefFoundError(Class<?> clazz, int index) {
		String className = getCPClassNameAt(clazz, index);
		NoClassDefFoundError noClassDefFoundError = new NoClassDefFoundError(className);
		noClassDefFoundError.initCause(new ClassNotFoundException(className));
		throw noClassDefFoundError;	
	}
	
	static long getJ9ClassFromClass(Class<?> c) {
		if (JITHELPERS.is32Bit()) {
			return JITHELPERS.getJ9ClassFromClass32(c);
		} else {
			return JITHELPERS.getJ9ClassFromClass64(c);
		}
	}

	/**
	 * Used during the invokehandle bytecode for resolving calls to the polymorphic
	 * MethodHandle and VarHandle methods. The resolution yields two values, which
	 * are returned in a two element array. The first array element is a MemberName
	 * object, which specifies the caller method to be invoked. The second array
	 * element is a MethodType object if the defining class is a MethodHandle and
	 * an AccessDescriptor object if the defining class is a VarHandle.
	 *
	 * This is only used for the OpenJDK MethodHandles, and an InternalError will
	 * be thrown if it is used for the OpenJ9 MethodHandles.
	 *
	 * @param callerClass the caller class
	 * @param refKind the reference kind used by the CONSTANT_MethodHandle entries
	 * @param definingClass the defining class
	 * @param name contains the method name
	 * @param type contains the method description
	 *
	 * @return a two element array, which contains the resolved values
	 *
	 * @throws InternalError if invoked for the OpenJ9 MethodHandles
	 */
	@SuppressWarnings("unused")
	private static final Object linkCallerMethod(Class<?> callerClass, int refKind, Class<?> definingClass, String name, String type) throws Throwable {
/*[IF OPENJDK_METHODHANDLES]*/
		VMLangAccess access = VM.getVMLangAccess();
		MethodType mt = MethodType.fromMethodDescriptorString(type, access.getClassloader(callerClass));

		Object[] result = new Object[2];
		Object[] appendixResult = new Object[1];

		/* result[0] contains the MemberName object, which specifies the caller method (generated bytecodes). */
		result[0] = MethodHandleNatives.linkMethod(callerClass, refKind, definingClass, name, mt, appendixResult);

		/* result[1] contains a MethodType object for MethodHandles and AccessDescriptor object for VarHandles. */
		result[1] = appendixResult[0];

		return (Object)result;
/*[ELSE] OPENJDK_METHODHANDLES*/
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
/*[ENDIF] OPENJDK_METHODHANDLES*/
	}
}
