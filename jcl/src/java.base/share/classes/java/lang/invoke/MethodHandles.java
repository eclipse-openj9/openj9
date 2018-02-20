/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2009, 2018 IBM Corp. and others
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

import java.lang.invoke.ConvertHandle.FilterHelpers;
import java.lang.ref.WeakReference;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.lang.reflect.ReflectPermission;

/*[IF Panama]*/
import java.nicl.*;
import java.nicl.types.*;
import jdk.internal.nicl.types.PointerTokenImpl;
/*[ENDIF]*/

import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.Objects;

import com.ibm.oti.util.Msg;
import com.ibm.oti.lang.ArgumentHelper;
import com.ibm.oti.vm.VM;
import com.ibm.oti.vm.VMLangAccess;

/*[IF Sidecar19-SE]
import java.util.AbstractList;
import java.util.Iterator;
import java.util.ArrayList;
import java.nio.ByteOrder;
import jdk.internal.reflect.CallerSensitive;
/*[IF Sidecar19-SE-B175]
import java.lang.Module;
import jdk.internal.misc.SharedSecrets;
import jdk.internal.misc.JavaLangAccess;
import java.security.ProtectionDomain;
import jdk.internal.org.objectweb.asm.ClassReader;
/*[ENDIF] Sidecar19-SE-B175*/
/*[ELSE]*/
import sun.reflect.CallerSensitive;
/*[ENDIF]*/

/**
 * Factory class for creating and adapting MethodHandles.
 * 
 * @since 1.7
 */
public class MethodHandles {
	@CallerSensitive
	static final native Class<?> getStackClass(int depth);
	
	/* An empty array used by foldArguments without argument indices specified */
	static final int[] EMPTY_ARG_POSITIONS = new int[0];
	
	/*[IF Panama]*/
	private native static long findNativeAddress(String methodName);
	/*[ENDIF]*/
	
	MethodHandles() {
		/*[IF ]*/
		// non public constructor so javac doesn't helpfully add a public one
		/*[ENDIF]*/
	}
	
	/*[IF Panama]*/
	static {
		com.ibm.oti.vm.VM.setVMLangInvokeAccess(new VMInvokeAccess());
	}
	/*[ENDIF]*/
	
	/**
	 * A factory for creating MethodHandles that require access-checking on creation.
	 * <p>
	 * Unlike Reflection, MethodHandle only requires access-checking when the MethodHandle
	 * is created, not when it is invoked.
	 * <p>
	 * This class provides the lookup authentication necessary when creating MethodHandles.  Any
	 * number of MethodHandles can be lookup using this token, and the token can be shared to provide
	 * others with the the "owner's" authentication level.  
	 * <p>
	 * Sharing {@link Lookup} objects should be done with care, as they may allow access to private
	 * methods. 
	 * <p>
	 * When using the lookup factory methods (find and unreflect methods), creating the MethodHandle 
	 * may fail because the method's arity is too high. 
	 *
	 */
	static final public class Lookup {
		
		/**
		 * Bit flag 0x1 representing <i>public</i> access.  See {@link #lookupModes()}.
		 */
		public static final int PUBLIC = Modifier.PUBLIC;
		
		/**
		 * Bit flag 0x2 representing <i>private</i> access.  See {@link #lookupModes()}.
		 */
		public static final int PRIVATE = Modifier.PRIVATE;
		
		/**
		 * Bit flag 0x4 representing <i>protected</i> access.  See {@link #lookupModes()}.
		 */
		public static final int PROTECTED = Modifier.PROTECTED;
		
		/**
		 * Bit flag 0x8 representing <i>package</i> access.  See {@link #lookupModes()}.
		 */
		public static final int PACKAGE = 0x8;

		/*[IF Sidecar19-SE]*/		
		/**
		 * Bit flag 0x10 representing <i>module</i> access.  See {@link #lookupModes()}.
		 */
		public static final int MODULE = 0x10;
		
		/**
		 * Bit flag 0x20, which contributes to a lookup, and indicates assumed readability.
		 * This bit, with the Public bit, indicates the lookup can access all public members
		 * where the public type is exported unconditionally. 
		 */
		public static final int UNCONDITIONAL = 0x20;
		/*[ENDIF] Sidecar19-SE*/
		
		static final int INTERNAL_PRIVILEGED = 0x40;

		/*[IF Sidecar19-SE-B175]
		private static final int FULL_ACCESS_MASK = PUBLIC | PRIVATE | PROTECTED | PACKAGE | MODULE;
		/*[ELSE]*/
		private static final int FULL_ACCESS_MASK = PUBLIC | PRIVATE | PROTECTED | PACKAGE;
		/*[ENDIF] Sidecar19-SE-B175*/
		
		private static final int NO_ACCESS = 0;
		
		private static final String INVOKE_EXACT = "invokeExact"; //$NON-NLS-1$
		private static final String INVOKE = "invoke"; //$NON-NLS-1$
		static final int VARARGS = 0x80;
		
		/* single cached value of public Lookup object */
		/*[IF Sidecar19-SE-B175]
		static Lookup PUBLIC_LOOKUP = new Lookup(Object.class, Lookup.PUBLIC | Lookup.UNCONDITIONAL);
		/*[ELSE]*/
		static Lookup PUBLIC_LOOKUP = new Lookup(Object.class, Lookup.PUBLIC);
		/*[ENDIF] Sidecar19-SE-B175*/
		
		/* single cached internal privileged lookup */
		static Lookup internalPrivilegedLookup = new Lookup(MethodHandle.class, Lookup.INTERNAL_PRIVILEGED);
		static Lookup IMPL_LOOKUP = internalPrivilegedLookup; /* hack for b56 of lambda-dev */
		
		/* Access token used in lookups - Object for public lookup */
		final Class<?> accessClass;
		final int accessMode;
		private final  boolean performSecurityCheck;
		
		Lookup(Class<?> lookupClass, int lookupMode, boolean doCheck) {
			this.performSecurityCheck = doCheck;
			if (doCheck && (INTERNAL_PRIVILEGED != lookupMode)) {
				if ( lookupClass.getName().startsWith("java.lang.invoke.")) {  //$NON-NLS-1$
					/*[MSG "K0588", "Illegal Lookup object - originated from java.lang.invoke: {0}"]*/
					throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0588", lookupClass.getName())); //$NON-NLS-1$
				}
			}

			accessClass = lookupClass;
			accessMode = lookupMode;
		}
		
		Lookup(Class<?> lookupClass, int lookupMode) {
			this(lookupClass, lookupMode, true);			
		}
		
		Lookup(Class<?> lookupClass) {
			this(lookupClass, FULL_ACCESS_MASK, true);
		}
		
		Lookup(Class<?> lookupClass, boolean performSecurityCheck) {
			 this(lookupClass, FULL_ACCESS_MASK, performSecurityCheck);
		} 
	
		/**
		 * A query to determine the lookup capabilities held by this instance.  
		 * 
		 * @return the lookup mode bit mask for this Lookup object
		 */
		public int lookupModes() {
			return accessMode;
		}
	

		/*
		 * Is the varargs bit set?
		 */
		static boolean isVarargs(int modifiers) {
			return (modifiers & VARARGS) != 0;
		}
		
		/* If the varargs bit is set, wrap the MethodHandle with an
		 * asVarargsCollector adapter.
		 * Last class type will be Object[] if not otherwise appropriate.
		 */
		private static MethodHandle convertToVarargsIfRequired(MethodHandle handle) {
			if (isVarargs(handle.getModifiers())) {
				Class<?> lastClass = handle.type.lastParameterType();
				return handle.asVarargsCollector(lastClass);
			}
			return handle;
		}
		
		/**
		 * Return an early-bound method handle to a non-static method.  The receiver must
		 * have a Class in its hierarchy that provides the virtual method named methodName.
		 * <p>
		 * The final MethodType of the MethodHandle will be identical to that of the method.
		 * The receiver will be inserted prior to the call and therefore does not need to be
		 * included in the MethodType.  
		 * 
		 * @param receiver - The Object to insert as a receiver.  Must implement the methodName 
		 * @param methodName - The name of the method
		 * @param type - The MethodType describing the method
		 * @return a MethodHandle to the required method.
		 * @throws IllegalAccessException if access is denied
		 * @throws NoSuchMethodException if the method doesn't exist
		 */
		public MethodHandle bind(Object receiver, String methodName, MethodType type) throws IllegalAccessException, NoSuchMethodException {
			nullCheck(receiver, methodName, type);
			Class<?> receiverClass = receiver.getClass();
			MethodHandle handle = handleForMHInvokeMethods(receiverClass, methodName, type);
			if (handle == null) {
				// Use the priviledgedLookup to allow probe the findSpecial cache without restricting the receiver
				handle = internalPrivilegedLookup.findSpecialImpl(receiverClass, methodName, type, receiverClass);
				handle = handle.asFixedArity(); // remove unnecessary varargsCollector from the middle of the MH chain.
				handle = convertToVarargsIfRequired(handle.bindTo(receiver));
			}
			checkAccess(handle);
			checkSecurity(handle.getDefc(), receiverClass, handle.getModifiers());
			
			handle = SecurityFrameInjector.wrapHandleWithInjectedSecurityFrameIfRequired(this, handle);

			return handle;
		}
		
		private static void nullCheck(Object a, Object b) {
			// use implicit null checks
			a.getClass();
			b.getClass();
		}
		
		private static void nullCheck(Object a, Object b, Object c) {
			// use implicit null checks
			a.getClass();
			b.getClass();
			c.getClass();
		}
		
		private static void nullCheck(Object a, Object b, Object c, Object d) {
			// use implicit null checks
			a.getClass();
			b.getClass();
			c.getClass();
			d.getClass();
		}
		
		private static void initCheck(String methodName) throws NoSuchMethodException {
			if ("<init>".equals(methodName) || "<clinit>".equals(methodName)) { //$NON-NLS-1$ //$NON-NLS-2$
				/*[MSG "K05ce", "Invalid method name: {0}"]*/
				throw new NoSuchMethodException(Msg.getString("K05ce", methodName)); //$NON-NLS-1$
			}
		}
		
		/* We use this because VM.javalangVMaccess is not set when this class is loaded.
		 * Delay grabbing it until we need it, which by then the value will be set. */
		private static final class VMLangAccessGetter {
			public static final VMLangAccess vma = VM.getVMLangAccess();
		}

		static final VMLangAccess getVMLangAccess() {
			return VMLangAccessGetter.vma;
		}
		
		/* Verify two classes share the same package in a way to avoid Class.getPackage()
		 * and the security checks that go with it.
		 */
		private static boolean isSamePackage(Class<?> a, Class<?> b){
			// Two of the same class share a package
			if (a == b){
				return true;
			}
			
			VMLangAccess vma = getVMLangAccess();
			
			String packageName1 = vma.getPackageName(a);
			String packageName2 = vma.getPackageName(b);
			// If the string value is different, they're definitely not related
			if((packageName1 == null) || (packageName2 == null) || !packageName1.equals(packageName2)) {
				return false;
			}
			
			ClassLoader cla = vma.getClassloader(a);
			ClassLoader clb = vma.getClassloader(b);
			
			// If both share the same classloader, then they are the same package
			if (cla == clb) {
				return true;
			}
			
			// If one is an ancestor of the other, they are also the same package
			if (vma.isAncestor(cla, clb) || vma.isAncestor(clb, cla)) {
				return true;
			}
			
			return false;
		}
		
		void checkAccess(MethodHandle handle) throws IllegalAccessException {
			if (INTERNAL_PRIVILEGED == accessMode) {
				// Full access for use by MH implementation.
				return;
			}

			if (NO_ACCESS == accessMode) {
				throw new IllegalAccessException(this.toString());
			}

			checkAccess(handle.getDefc(), handle.getMethodName(), handle.getModifiers(), handle);
		}
		
		/*[IF Sidecar19-SE]*/
		void checkAccess(VarHandle handle) throws IllegalAccessException {
			if (INTERNAL_PRIVILEGED == accessMode) {
				// Full access for use by MH implementation.
				return;
			}
			
			if (NO_ACCESS == accessMode) {
				throw new IllegalAccessException(this.toString());
			}
			
			checkAccess(handle.getDefiningClass(), handle.getFieldName(), handle.getModifiers(), null);
		}
		
		void checkAccess(Class<?> clazz) throws IllegalAccessException {
			if (INTERNAL_PRIVILEGED == accessMode) {
				// Full access for use by MH implementation.
				return;
			}
					
			if (NO_ACCESS == accessMode) {
				throw new IllegalAccessException(this.toString());
			}
			
			checkClassAccess(clazz);
		}
		/*[ENDIF]*/
						
		/**
		 * Checks whether {@link #accessClass} can access a specific member of the {@code targetClass}.
		 * Equivalent of visible.c checkVisibility();
		 * 
		 * @param targetClass The {@link Class} that owns the member being accessed.
		 * @param name The name of member being accessed.
		 * @param memberModifiers The modifiers of the member being accessed.
		 * @param handle A handle object (e.g. {@link MethodHandle} or {@link VarHandle}), if applicable.
		 * 				 The object is always {@link MethodHandle} in the current implementation, so in order 
		 * 				 to avoid extra type checking, the parameter is {@link MethodHandle}. If we need to
		 * 				 handle {@link VarHandle} in the future, the type can be {@link Object}.
		 * @throws IllegalAccessException If the member is not accessible.
		 */
		private void checkAccess(Class<?> targetClass, String name, int memberModifiers, MethodHandle handle) throws IllegalAccessException {
			checkClassAccess(targetClass);

			if (Modifier.isPublic(memberModifiers)) {
				/* checkClassAccess already determined that we have more than "no access" (public access) */
				return;
			} else if (Modifier.isPrivate(memberModifiers)) {
				if (Modifier.isPrivate(accessMode) && ((targetClass == accessClass)
/*[IF Valhalla-NestMates]*/
						|| targetClass.isNestmateOf(accessClass)
/*[ENDIF] Valhalla-NestMates*/	
				)) {
					return;
				}
			} else if (Modifier.isProtected(memberModifiers)) {
				/* Ensure that the accessMode is not restricted (public-only) */
				if (PUBLIC != accessMode) {
					if (targetClass.isArray()) {
						/* The only methods array classes have are defined on Object and thus accessible */
						return;
					}
					
					if (isSamePackage(accessClass, targetClass)) {
						/* Package access is enough if the classes are in the same package */
						return;
					}
					/*[IF ]*/
					/* 
					 * Interfaces are not classes, but types, and should therefore not have access to 
					 * protected methods in java.lang.Object as subclasses.
					 */
					/*[ENDIF]*/
					if (!accessClass.isInterface() && Modifier.isProtected(accessMode) && targetClass.isAssignableFrom(accessClass)) {
						/* Special handling for MethodHandles */
						if (null != handle) {
							byte kind = handle.kind;
						
							if ((MethodHandle.KIND_CONSTRUCTOR == kind)
							|| ((MethodHandle.KIND_SPECIAL == kind) && !handle.directHandleOriginatedInFindVirtual())
							) {
								/*[IF ]*/
								/* See also the code in vrfyhelp.c:isProtectedAccessPermitted() for the verifier's version of this
								 * check.  The classes used here map to the names in isProtectedAccessPermitted:
								 * 		accessClass 					=> currentClass
								 * 		defc 							=> declaringClass
								 * 		referenceClass / SpecialCaller	=> targetClass
								 */
								/*[ENDIF]*/
								Class<?> protectedTargetClass = handle.getReferenceClass();
								if (MethodHandle.KIND_SPECIAL == kind) {
									protectedTargetClass = handle.getSpecialCaller();
								}
								if (accessClass.isAssignableFrom(protectedTargetClass)) {
									return;
								}
							} else {
								/* MethodHandle.KIND_GETFIELD, MethodHandle.KIND_PUTFIELD & MethodHandle.KIND_VIRTUAL
								 * restrict the receiver to be a subclass under the current class and thus don't need
								 * additional access checks.
								 */
								return;
							}
						} else {
							/* Not a MethodHandle - already confirmed assignability and accessMode */
							return;
						}
					}
				}
			} else {
				/* default (package access) */
				if ((PACKAGE == (accessMode & PACKAGE)) && isSamePackage(accessClass, targetClass)){
					return;
				}
			}
			
			/* No access - throw exception */
			String extraInfo = ""; //$NON-NLS-1$
			if (null != handle) {
				extraInfo = name + ":" + handle.type + "/" + handle.mapKindToBytecode(); //$NON-NLS-1$ //$NON-NLS-2$
			} else {
				extraInfo = "." + name;  //$NON-NLS-1$
			}
			String errorMessage = com.ibm.oti.util.Msg.getString("K0587", this.toString(), targetClass.getName() + extraInfo);  //$NON-NLS-1$
			throw new IllegalAccessException(errorMessage);
		}
		
		/**
		 * Checks whether {@link #accessClass} can access a specific {@link Class}.
		 * 
		 * @param targetClass The {@link Class} being accessed.
		 * @throws IllegalAccessException If the {@link Class} is not accessible from {@link #accessClass}. 
		 */
		private void checkClassAccess(Class<?> targetClass) throws IllegalAccessException {
			if (NO_ACCESS != accessMode) {
				/* target class should always be accessible to the lookup class when they are the same class */
				if (accessClass == targetClass) {
					return;
				}
				
				int modifiers = targetClass.getModifiers();
				
				/* A protected class (must be a member class) is compiled to a public class as
				 * the protected flag of this class doesn't exist on the VM level (there is no 
				 * access flag in the binary form representing 'protected')
				 */
				if (Modifier.isProtected(modifiers)) {
					/* Interfaces are not classes but types, and should therefore not have access to 
					 * protected methods in java.lang.Object as subclasses.
					 */
					if (!accessClass.isInterface()) {
						return;
					}
				
				/* The following access checking is for a normal class (non-member class) */
				} else if (Modifier.isPublic(modifiers)) {
					/* Already determined that we have more than "no access" (public access) */
					return;
				} else {
					if (((PACKAGE == (accessMode & PACKAGE)) || Modifier.isPrivate(accessMode)) && isSamePackage(accessClass, targetClass)) {
						return;
					}
				}
			}
			
			/*[MSG "K0587", "'{0}' no access to: '{1}'"]*/
			throw new IllegalAccessException(com.ibm.oti.util.Msg.getString("K0587", accessClass.getName(), targetClass.getName()));  //$NON-NLS-1$
		}
		
		private void checkSpecialAccess(Class<?> callerClass) throws IllegalAccessException {
			if (INTERNAL_PRIVILEGED == accessMode) {
				// Full access for use by MH implementation.
				return;
			}
			if (isWeakenedLookup() || accessClass != callerClass) {
				/*[MSG "K0585", "{0} could not access {1} - private access required"]*/
				throw new IllegalAccessException(com.ibm.oti.util.Msg.getString("K0585", accessClass.getName(), callerClass.getName())); //$NON-NLS-1$
			}
		}
		
		/**
		 * Return a MethodHandle bound to a specific-implementation of a virtual method, as if created by an invokespecial bytecode 
		 * using the class specialToken.  The method and all Classes in its MethodType must be accessible to
		 * the caller.
		 * <p>
		 * The receiver class will be added to the MethodType of the handle, but dispatch will not occur based
		 * on the receiver.
		 *  
		 * @param clazz - the class or interface from which defines the method
		 * @param methodName - the method name
		 * @param type - the MethodType of the method
		 * @param specialToken - the calling class for the invokespecial
		 * @return a MethodHandle
		 * @throws IllegalAccessException - if the method is static or access checking fails
		 * @throws NullPointerException - if clazz, methodName, type or specialToken is null
		 * @throws NoSuchMethodException - if clazz has no instance method named methodName with signature matching type
		 * @throws SecurityException - if any installed SecurityManager denies access to the method
		 */
		public MethodHandle findSpecial(Class<?> clazz, String methodName, MethodType type, Class<?> specialToken) throws IllegalAccessException, NoSuchMethodException, SecurityException, NullPointerException {
			nullCheck(clazz, methodName, type, specialToken);
			checkSpecialAccess(specialToken);	/* Must happen before method resolution */
			MethodHandle handle = null;
			try {
				handle = findSpecialImpl(clazz, methodName, type, specialToken);
				Class<?> handleDefc = handle.getDefc();
				if ((handleDefc != accessClass) && !handleDefc.isAssignableFrom(accessClass)) {
					/*[MSG "K0586", "Lookup class ({0}) must be the same as or subclass of the current class ({1})"]*/
					throw new IllegalAccessException(com.ibm.oti.util.Msg.getString("K0586", accessClass, handleDefc)); //$NON-NLS-1$
				}
				checkAccess(handle);
				checkSecurity(handleDefc, clazz, handle.getModifiers());
				handle = SecurityFrameInjector.wrapHandleWithInjectedSecurityFrameIfRequired(this, handle);
			} catch (DefaultMethodConflictException e) {
				/* Method resolution revealed conflicting default method definitions. Return a MethodHandle that throws an
				 * IncompatibleClassChangeError when invoked, and who's type matches the requested type.
				 */
				handle = throwExceptionWithMethodTypeAndMessage(
						IncompatibleClassChangeError.class, 
						type.insertParameterTypes(0, specialToken), 
						e.getCause().getMessage());
				/*[IF ]*/
				/* 
				 * No need to check access or security here. 
				 * This matches the behaviour of reference implementation.
				 */
				/*[ENDIF]*/
			}
			return handle;
		}
		
		/*
		 * Lookup the findSpecial handle either from the special handle cache, or create a new handle and install it in the cache.
		 */
		private MethodHandle findSpecialImpl(Class<?> clazz, String methodName, MethodType type, Class<?> specialToken) throws IllegalAccessException, NoSuchMethodException, SecurityException, NullPointerException {
			Map<CacheKey, WeakReference<MethodHandle>> cache = HandleCache.getSpecialCache(clazz);
			MethodHandle handle = HandleCache.getMethodWithSpecialCallerFromPerClassCache(cache, methodName, type, specialToken);
			if (handle == null) {
				initCheck(methodName);
				handle = new DirectHandle(clazz, methodName, type, MethodHandle.KIND_SPECIAL, specialToken);
				if (internalPrivilegedLookup != this) {
					handle = restrictReceiver(handle);
				}
				handle = convertToVarargsIfRequired(handle);
				HandleCache.putMethodWithSpecialCallerInPerClassCache(cache, methodName, type, handle, specialToken);
			}
			return handle;
		}
			
		/**
		 * Return a MethodHandle to a static method.  The MethodHandle will have the same type as the
		 * method.  The method and all classes in its type must be accessible to the caller.
		 * 
		 * @param clazz - the class defining the method
		 * @param methodName - the name of the method
		 * @param type - the MethodType of the method
		 * @return A MethodHandle able to invoke the requested static method
		 * @throws IllegalAccessException - if the method is not static or access checking fails
		 * @throws NullPointerException - if clazz, methodName or type is null
		 * @throws NoSuchMethodException - if clazz has no static method named methodName with signature matching type
		 * @throws SecurityException - if any installed SecurityManager denies access to the method
		 */
		public MethodHandle findStatic(Class<?> clazz, String methodName, MethodType type) throws IllegalAccessException, NoSuchMethodException {
			nullCheck(clazz, methodName, type);
			Map<CacheKey, WeakReference<MethodHandle>> cache = HandleCache.getStaticCache(clazz);
			MethodHandle handle = HandleCache.getMethodFromPerClassCache(cache, methodName, type);
			if (handle == null) {
				initCheck(methodName);
				handle = new DirectHandle(clazz, methodName, type, MethodHandle.KIND_STATIC, null);
				handle = HandleCache.putMethodInPerClassCache(cache, methodName, type, convertToVarargsIfRequired(handle));
			}
			checkAccess(handle);
			checkSecurity(handle.getDefc(), clazz, handle.getModifiers());
			/* Static check is performed by native code */
			handle = SecurityFrameInjector.wrapHandleWithInjectedSecurityFrameIfRequired(this, handle);
			return handle;
		}
		
		/**
		 * Return a MethodHandle to a virtual method.  The method will be looked up in the first argument 
		 * (aka receiver) prior to dispatch.  The type of the MethodHandle will be that of the method
		 * with the receiver type prepended.
		 * 
		 * @param clazz - the class defining the method
		 * @param methodName - the name of the method
		 * @param type - the type of the method
		 * @return a MethodHandle that will do virtual dispatch on the first argument
		 * @throws IllegalAccessException - if method is static or access is refused
		 * @throws NullPointerException - if clazz, methodName or type is null
		 * @throws NoSuchMethodException - if clazz has no virtual method named methodName with signature matching type
		 * @throws SecurityException - if any installed SecurityManager denies access to the method
		 */
		public MethodHandle findVirtual(Class<?> clazz, String methodName, MethodType type) throws IllegalAccessException, NoSuchMethodException {
			nullCheck(clazz, methodName, type);
			
			Map<CacheKey, WeakReference<MethodHandle>> cache = HandleCache.getVirtualCache(clazz);
			MethodHandle handle = HandleCache.getMethodFromPerClassCache(cache, methodName, type);
			if (handle == null) {
				handle = handleForMHInvokeMethods(clazz, methodName, type);
			}
			if (handle == null) {
				initCheck(methodName);
				
				if (clazz.isInterface()) {
					handle = new InterfaceHandle(clazz, methodName, type);
					if (Modifier.isStatic(handle.getModifiers())) {
						throw new IllegalAccessException();
					}
					handle = adaptInterfaceLookupsOfObjectMethodsIfRequired(handle, clazz, methodName, type);
				} else {
					/*[IF ]*/
					/* Need to perform a findSpecial and use that to determine what kind of handle to return:
					 * 	- static method found	-> NoAccessException (thrown by the findSpecial)
					 *  - final method found	-> DirectHandle(KIND_SPECIAL)
					 *  - private method found	-> DirectHandle(KIND_SPECIAL)
					 *  - other					-> Convert DirectHandle to VirtualHandle
					 */
					/*[ENDIF]*/
					handle = new DirectHandle(clazz, methodName, type, MethodHandle.KIND_VIRTUAL, clazz, true);
					int handleModifiers = handle.getModifiers();
					/* Static check is performed by native code */
					if (!Modifier.isPrivate(handleModifiers) && !Modifier.isFinal(handleModifiers)) {
						handle = new VirtualHandle((DirectHandle) handle);
					}
				}
				handle = convertToVarargsIfRequired(handle);
				HandleCache.putMethodInPerClassCache(cache, methodName, type, handle);
			}
			handle = restrictReceiver(handle);
			checkAccess(handle);
			checkSecurity(handle.getDefc(), clazz, handle.getModifiers());
			handle = SecurityFrameInjector.wrapHandleWithInjectedSecurityFrameIfRequired(this, handle);
			return handle;
		}
		
		/*[IF Panama]*/
		/**
		 * Return a MethodHandle to a native method.  The MethodHandle will have the same type as the
		 * method.  The method and all classes in its type must be accessible to the caller.
		 * 
		 * @param methodName - the name of the method
		 * @param type - the MethodType of the method
		 * @return A MethodHandle able to invoke the requested native method
		 * @throws IllegalAccessException - if access checking fails
		 * @throws NullPointerException - if methodName or type is null
		 * @throws NoSuchMethodException - if no native method named methodName with signature matching type
		 */
		public MethodHandle findNative(String methodName, MethodType type) throws IllegalAccessException, NoSuchMethodException {
			nullCheck(methodName, type);

			long nativeAddress = findNativeAddress(methodName);
			if(0 == nativeAddress) {
				throw new NoSuchMethodException("Failed to look up " + methodName); //$NON-NLS-1$
			}
			
			MethodHandle handle = new NativeMethodHandle(methodName, type, nativeAddress);
			return handle;
		}
		
		/**
		 * Return a MethodHandle to a native method.  The MethodHandle will have the same type as the
		 * method.  The method and all classes in its type must be accessible to the caller.
		 * 
		 * @param lib - the library of the method
		 * @param methodName - the name of the method
		 * @param type - the MethodType of the method
		 * @return A MethodHandle able to invoke the requested native method
		 * @throws IllegalAccessException - if access checking fails
		 * @throws NullPointerException - if methodName or type is null
		 * @throws NoSuchMethodException - if no native method named methodName with signature matching type
		 */
		public MethodHandle findNative(Library lib, String methodName, MethodType type) throws IllegalAccessException, NoSuchMethodException {
			nullCheck(methodName, type);
			
			/* TODO revisit this code, it is likely to change in future versions of the API */
			PointerToken tok = new PointerTokenImpl();
			long nativeAddress = lib.lookup(methodName).getAddress().addr(tok);
			
			MethodHandle handle = new NativeMethodHandle(methodName, type, nativeAddress);
			return handle;
		}
		/*[ENDIF]*/
		
		/**
		 * Restrict the receiver as indicated in the JVMS for invokespecial and invokevirtual.
		 * <blockquote>
		 * Finally, if the resolved method is protected (4.6), and it is a member of a superclass of the current class, and
		 * the method is not declared in the same runtime package (5.3) as the current class, then the class of objectref 
		 * must be either the current class or a subclass of the current class.
		 * </blockquote>
		 */
		private MethodHandle restrictReceiver(MethodHandle handle) {
			int handleModifiers = handle.getModifiers();
			Class<?> handleDefc = handle.getDefc();
			if (!Modifier.isStatic(handleModifiers)
			&& Modifier.isProtected(handleModifiers) 
			&& (handleDefc != accessClass) 
			&& (handleDefc.isAssignableFrom(accessClass))
			&& (!isSamePackage(handleDefc, accessClass))
			) {
				handle = handle.cloneWithNewType(handle.type.changeParameterType(0, accessClass));
			}
			return handle;
		}
		
		/**
		 * Adapt InterfaceHandles on public Object methods if the method is not redeclared in the interface class.
		 * Public methods of Object are implicitly members of interfaces and do not receive iTable indexes.
		 * If the declaring class is Object, create a VirtualHandle and asType it to the interface class. 
		 * @param handle An InterfaceHandle
		 * @param clazz The lookup class
		 * @param methodName The lookup name
		 * @param type The lookup type
		 * @return Either the original handle or an adapted one for Object methods.
		 * @throws NoSuchMethodException
		 * @throws IllegalAccessException
		 */
		static MethodHandle adaptInterfaceLookupsOfObjectMethodsIfRequired(MethodHandle handle, Class<?> clazz, String methodName, MethodType type) throws NoSuchMethodException, IllegalAccessException {
			assert handle instanceof InterfaceHandle;
			/* Object methods need to be treated specially if the interface hasn't declared them itself */
			if (Object.class == handle.getDefc()) {
				if (!Modifier.isPublic(handle.getModifiers())) {
					/* Interfaces only inherit *public* methods from Object */
					throw new NoSuchMethodException(clazz + "." + methodName + type); //$NON-NLS-1$					
				}
				handle = new VirtualHandle(new DirectHandle(Object.class, methodName, type, MethodHandle.KIND_SPECIAL, Object.class));
				handle = handle.cloneWithNewType(handle.type.changeParameterType(0, clazz));
			}
			return handle;
		}

		/*
		 * Check for methods MethodHandle.invokeExact or MethodHandle.invoke.  
		 * Access checks are not required as these methods are public and therefore
		 * accessible to everyone.
		 */
		static MethodHandle handleForMHInvokeMethods(Class<?> clazz, String methodName, MethodType type) {
			if (MethodHandle.class.isAssignableFrom(clazz)) {
				if (INVOKE_EXACT.equals(methodName)) {
					return type.getInvokeExactHandle();
				} else if (INVOKE.equals(methodName))  {
					return new InvokeGenericHandle(type);
				}
			}
			return null;
		}

		
		/**
		 * Return a MethodHandle that provides read access to a field.
		 * The MethodHandle will have a MethodType taking a single 
		 * argument with type <code>clazz</code> and returning something of 
		 * type <code>fieldType</code>.
		 * 
		 * @param clazz - the class defining the field
		 * @param fieldName - the name of the field
		 * @param fieldType - the type of the field
		 * @return a MethodHandle able to return the value of the field
		 * @throws IllegalAccessException if access is denied or the field is static
		 * @throws NoSuchFieldException if no field is found with given name and type in clazz
		 * @throws SecurityException if the SecurityManager prevents access
		 * @throws NullPointerException if any of the arguments are null
		 */
		public MethodHandle findGetter(Class<?> clazz, String fieldName, Class<?> fieldType) throws IllegalAccessException, NoSuchFieldException, SecurityException, NullPointerException {
			nullCheck(clazz, fieldName, fieldType);
			Map<CacheKey, WeakReference<MethodHandle>> cache = HandleCache.getFieldGetterCache(clazz);
			MethodHandle handle = HandleCache.getFieldFromPerClassCache(cache, fieldName, fieldType);
			if (handle == null) {
				handle = new FieldGetterHandle(clazz, fieldName, fieldType, accessClass);
				HandleCache.putFieldInPerClassCache(cache, fieldName, fieldType, handle);
			}
			checkAccess(handle);
			checkSecurity(handle.getDefc(), clazz, handle.getModifiers());
			return handle;
		}
		
		/**
		 * Return a MethodHandle that provides read access to a field.
		 * The MethodHandle will have a MethodType taking no arguments
		 * and returning something of type <code>fieldType</code>.
		 * 
		 * @param clazz - the class defining the field
		 * @param fieldName - the name of the field
		 * @param fieldType - the type of the field
		 * @return a MethodHandle able to return the value of the field
		 * @throws IllegalAccessException if access is denied or the field is not static
		 * @throws NoSuchFieldException if no field is found with given name and type in clazz
		 * @throws SecurityException if the SecurityManager prevents access
		 * @throws NullPointerException if any of the arguments are null
		 */
		public MethodHandle findStaticGetter(Class<?> clazz, String fieldName, Class<?> fieldType) throws IllegalAccessException, NoSuchFieldException, SecurityException, NullPointerException {
			nullCheck(clazz, fieldName, fieldType);
			Map<CacheKey, WeakReference<MethodHandle>> cache = HandleCache.getStaticFieldGetterCache(clazz);
			MethodHandle handle = HandleCache.getFieldFromPerClassCache(cache, fieldName, fieldType);
			if (handle == null) {
				handle = new StaticFieldGetterHandle(clazz, fieldName, fieldType, accessClass);
				HandleCache.putFieldInPerClassCache(cache, fieldName, fieldType, handle);
			}
			checkAccess(handle);
			checkSecurity(handle.getDefc(), clazz, handle.getModifiers());
			return handle;
		}
		
		/**
		 * Return a MethodHandle that provides write access to a field.  
		 * The MethodHandle will have a MethodType taking a two 
		 * arguments, the first with type <code>clazz</code> and the second with 
		 * type <code>fieldType</code>, and returning void.
		 * 
		 * @param clazz - the class defining the field
		 * @param fieldName - the name of the field
		 * @param fieldType - the type of the field
		 * @return a MethodHandle able to set the value of the field
		 * @throws IllegalAccessException if access is denied
		 * @throws NoSuchFieldException if no field is found with given name and type in clazz
		 * @throws SecurityException if the SecurityManager prevents access
		 * @throws NullPointerException if any of the arguments are null
		 */
		public MethodHandle findSetter(Class<?> clazz, String fieldName, Class<?> fieldType) throws IllegalAccessException, NoSuchFieldException, SecurityException, NullPointerException {
			nullCheck(clazz, fieldName, fieldType);
			if (fieldType == void.class) {
				throw new NoSuchFieldException();
			}
			Map<CacheKey, WeakReference<MethodHandle>> cache = HandleCache.getFieldSetterCache(clazz);
			MethodHandle handle = HandleCache.getFieldFromPerClassCache(cache, fieldName, fieldType);
			if (handle == null) {
				handle = new FieldSetterHandle(clazz, fieldName, fieldType, accessClass);
				HandleCache.putFieldInPerClassCache(cache, fieldName, fieldType, handle);
			}
			if (Modifier.isFinal(handle.getModifiers())) {
				/*[MSG "K05cf", "illegal setter on final field"]*/
				throw new IllegalAccessException(Msg.getString("K05cf")); //$NON-NLS-1$
			}
			checkAccess(handle);
			checkSecurity(handle.getDefc(), clazz, handle.getModifiers());
			return handle;
		}
		
		/**
		 * Return a MethodHandle that provides write access to a field.
		 * The MethodHandle will have a MethodType taking one argument
		 * of type <code>fieldType</code> and returning void.
		 * 
		 * @param clazz - the class defining the field
		 * @param fieldName - the name of the field
		 * @param fieldType - the type of the field
		 * @return a MethodHandle able to set the value of the field
		 * @throws IllegalAccessException if access is denied
		 * @throws NoSuchFieldException if no field is found with given name and type in clazz
		 * @throws SecurityException if the SecurityManager prevents access
		 * @throws NullPointerException if any of the arguments are null
		 */
		public MethodHandle findStaticSetter(Class<?> clazz, String fieldName, Class<?> fieldType) throws IllegalAccessException, NoSuchFieldException, SecurityException, NullPointerException {
			nullCheck(clazz, fieldName, fieldType);
			if (fieldType == void.class) {
				throw new NoSuchFieldException();
			}
			Map<CacheKey, WeakReference<MethodHandle>> cache = HandleCache.getStaticFieldSetterCache(clazz);
			MethodHandle handle = HandleCache.getFieldFromPerClassCache(cache, fieldName, fieldType);
			if (handle == null) {
				handle = new StaticFieldSetterHandle(clazz, fieldName, fieldType, accessClass);
				HandleCache.putFieldInPerClassCache(cache, fieldName, fieldType, handle);
			}

			if (Modifier.isFinal(handle.getModifiers())) {
				/*[MSG "K05cf", "illegal setter on final field"]*/
				throw new IllegalAccessException(Msg.getString("K05cf")); //$NON-NLS-1$
			}
			checkAccess(handle);
			checkSecurity(handle.getDefc(), clazz, handle.getModifiers());
			return handle;
		}
		
		/**
		 * Create a lookup on the request class.  The resulting lookup will have no more 
		 * access privileges than the original.
		 * 
		 * @param lookupClass - the class to create the lookup on
		 * @return a new MethodHandles.Lookup object
		 */
		public MethodHandles.Lookup in(Class<?> lookupClass){
			lookupClass.getClass();	// implicit null check
			
			// If it's the same class as ourselves, return this
			if (lookupClass == accessClass) {
				return this;
			}
			
			/*[IF ]*/
			/* If the new lookup class differs from the old one, protected members will not be accessible by virtue of inheritance. (Protected members may continue to be accessible because of package sharing.) */
			/*[ENDIF]*/
			/*[IF !Sidecar19-SE-B175]
			int newAccessMode = accessMode & ~PROTECTED;
			/*[ELSE]*/
			/* The UNCONDITIONAL bit is discarded if the new lookup class differs from the old one in Java 9 */
			int newAccessMode = accessMode & ~UNCONDITIONAL;
			
			/* There are 3 cases to be addressed for the new lookup class from a different module:
			 * 1) There is no access if the package containing the new lookup class is not exported to 
			 *    the package containing the old one.
			 * 2) There is no access if the old lookup class is in a named module
			 *    Note: The public access will be reserved if the old lookup class is a public lookup.
			 * 3) The MODULE access is removed if the old lookup class is in an unnamed module.
			 */
			Module accessClassModule = accessClass.getModule();
			Module lookupClassModule = lookupClass.getModule();
			
			if (!Objects.equals(accessClassModule, lookupClassModule)) {
				if (!lookupClassModule.isExported(lookupClass.getPackageName(), accessClassModule)) {
					newAccessMode = NO_ACCESS;
				} else if (accessClassModule.isNamed()) {
					/* If the old lookup class is in a named module different from the new lookup class,
					 * we should keep the public access only when it is a public lookup.
					 */
					if (Lookup.PUBLIC_LOOKUP == this) {
						newAccessMode = PUBLIC;
					} else {
						newAccessMode = NO_ACCESS;
					}
				} else {
					newAccessMode &= ~MODULE;
				}
			}
			/*[ENDIF] Sidecar19-SE-B175*/
			
			/*[IF ]*/
			/* If the new lookup class is in a different package than the old one, protected and default (package) members will not be accessible. */
			/*[ENDIF]*/
			if (!isSamePackage(accessClass,lookupClass)) {
				newAccessMode &= ~(PACKAGE | PROTECTED);
			}
			
			if (PRIVATE == (newAccessMode & PRIVATE)){
				Class<?> a = getUltimateEnclosingClassOrSelf(accessClass);
				Class<?> l = getUltimateEnclosingClassOrSelf(lookupClass);
				if (a != l) {
				/*[IF Sidecar19-SE-B175]
				/* If the new lookup class is not within the same package member as the old one, private and protected members will not be accessible. */
					newAccessMode &= ~(PRIVATE | PROTECTED);
				/*[ELSE]*/
				/* If the new lookup class is not within the same package member as the old one, private members will not be accessible. */
					newAccessMode &= ~PRIVATE;
				/*[ENDIF] Sidecar19-SE-B175*/
				}
			}
			
			/*[IF ]*/
			/* If the new lookup class is not accessible to the old lookup class, then no members, not even public members, will be accessible. (In all other cases, public members will continue to be accessible.) */
			/*[ENDIF]*/
			/* Treat a protected class as public as the access flag of a protected class
			 * is set to public when compiled to a class file.
			 */
			int lookupClassModifiers = lookupClass.getModifiers();
			if(!Modifier.isPublic(lookupClassModifiers)
			&& !Modifier.isProtected(lookupClassModifiers)
			){
				if(isSamePackage(accessClass, lookupClass)) {
					if (0 == (accessMode & PACKAGE)) {
						newAccessMode = NO_ACCESS;
					}
				} else {
					newAccessMode = NO_ACCESS;
				}
			} else {
				VMLangAccess vma = getVMLangAccess();
				if (vma.getClassloader(accessClass) != vma.getClassloader(lookupClass)) {
					newAccessMode &= ~(PACKAGE | PRIVATE | PROTECTED);
				}
			}
			
			return new Lookup(lookupClass, newAccessMode); 
		}
		
		/*
		 * Get the top level class for a given class or return itself.
		 */
		private static Class<?> getUltimateEnclosingClassOrSelf(Class<?> c) {
			Class<?> enclosing = c.getEnclosingClass();
			Class<?> previous = c;
			
			while (enclosing != null) {
				previous = enclosing;
				enclosing = enclosing.getEnclosingClass();
			}
			return previous;
		}
		
		/*
		 * Determine if 'currentClassLoader' is the same or a child of the requestedLoader.  Necessary
		 * for access checking. 
		 */
		private static boolean doesClassLoaderDescendFrom(ClassLoader currentLoader, ClassLoader requestedLoader) {
			if (requestedLoader == null) {
				/* Bootstrap loader is parent of everyone */
				return true;
			}
			if (currentLoader != requestedLoader) {
				while (currentLoader != null) {
					if (currentLoader == requestedLoader) {
						return true;
					}
					currentLoader = currentLoader.getParent();
				}
				return false;
			}
			return true;
		}
		
		/**
		 * The class being used for visibility checks and access permissions.
		 * 
		 * @return The class used in by this Lookup object for access checking
		 */
		public Class<?> lookupClass() {
			return accessClass;
		}
		
		/**
		 * Make a MethodHandle to the Reflect method.  If the method is non-static, the receiver argument
		 * is treated as the intial argument in the MethodType.  
		 * <p>
		 * If m is a virtual method, normal virtual dispatch is used on each invocation.
		 * <p>
		 * If the <code>accessible</code> flag is not set on the Reflect method, then access checking
		 * is performed using the lookup class.
		 * 
		 * @param method - the reflect method
		 * @return A MethodHandle able to invoke the reflect method
		 * @throws IllegalAccessException - if access is denied
		 */
		public MethodHandle unreflect(Method method) throws IllegalAccessException{
			int methodModifiers = method.getModifiers();
			Class<?> declaringClass = method.getDeclaringClass();
			Map<CacheKey, WeakReference<MethodHandle>> cache;
			
			/* Determine which cache (static or virtual to use) */
			if (Modifier.isStatic(methodModifiers)) {
				cache = HandleCache.getStaticCache(declaringClass);
			} else {
				cache = HandleCache.getVirtualCache(declaringClass);
			}
			
			String methodName = method.getName();
			MethodType type = MethodType.methodType(method.getReturnType(), method.getParameterTypes());
			MethodHandle handle = HandleCache.getMethodFromPerClassCache(cache, methodName, type);
			if (handle == null) {
				if (Modifier.isStatic(methodModifiers)) {
					handle = new DirectHandle(method, MethodHandle.KIND_STATIC, null);
				} else if (declaringClass.isInterface()) {
					/*[PR 67085] Temporary fix to match RI's behavior pending 335 EG discussion */
					if ((Modifier.isPrivate(methodModifiers)) && !Modifier.isStatic(methodModifiers)) {
						return throwAbstractMethodErrorForUnreflectPrivateInterfaceMethod(method, type);
					} else {
						handle = new InterfaceHandle(method);
					}
					/* Note, it is not required to call adaptInterfaceLookupsOfObjectMethodsIfRequired() here 
					 * as Reflection will not return a j.l.r.Method for a public Object method with an interface
					 * as the declaringClass *unless* that the method is defined in the interface or superinterface.
					 */
				} else {
					/*[IF ]*/
					/* Need to perform a findSpecial and use that to determine what kind of handle to return:
					 * 	- static method found	-> NoAccessException (thrown by the findSpecial)
					 *  - final method found	-> DirectHandle(KIND_SPECIAL)
					 *  - private method found	-> DirectHandle(KIND_SPECIAL)
					 *  - other					-> Convert DirectHandle to VirtualHandle
					 */
					/*[ENDIF]*/
				
					/* Static check is performed by native code */
					if (!Modifier.isPrivate(methodModifiers) && !Modifier.isFinal(methodModifiers)) {
						handle = new VirtualHandle(method);
					} else {
						handle = new DirectHandle(method, MethodHandle.KIND_SPECIAL, declaringClass, true);
					}
				}
				handle = convertToVarargsIfRequired(handle);
				HandleCache.putMethodInPerClassCache(cache, methodName, type, handle);
			}
			
			if (!method.isAccessible()) {
				handle = restrictReceiver(handle);
				checkAccess(handle);
			}
			
			handle = SecurityFrameInjector.wrapHandleWithInjectedSecurityFrameIfRequired(this, handle);
			
			return handle;
		}
		
		/*[IF ]*/
		/** Private interface methods created by unreflect must throw AbstractMethodError 
		 * when invoked to match RI's behaviour.
		 *
		 * @param method The private interface method
		 * @param type The incoming method type without the receiver 
		 * @return A MethodHandle of the right signature that always throws AME
		 * @throws IllegalAccessException under the same conditions as checkAccess()
		 */
		/*[ENDIF]*/
		private MethodHandle throwAbstractMethodErrorForUnreflectPrivateInterfaceMethod(Method method, MethodType type) throws IllegalAccessException {
			Class<?> declaringClass = method.getDeclaringClass();
			int modifiers = method.getModifiers();
			
			if (!declaringClass.isInterface() || !Modifier.isPrivate(modifiers)) {
				throw new InternalError("Only applicable to private interface methods"); //$NON-NLS-1$
			}
			/* Inlined version of access checking.  This needs to cover the NO_ACCESS and private_access case
			 * as this method should only be called on a private interface method.
			 */
			if (!method.isAccessible()) {
				if (accessMode == NO_ACCESS) {
					throw new IllegalAccessException(this.toString());
				}
				if (declaringClass != accessClass || !Modifier.isPrivate(accessMode)) {
					/*[MSG "K0587", "'{0}' no access to: '{1}'"]*/
					String message = com.ibm.oti.util.Msg.getString("K0587", this.toString(), declaringClass + "." + method.getName() + ":" + MethodHandle.KIND_INTERFACE + "/invokeinterface");  //$NON-NLS-1$//$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$
					throw new IllegalAccessException(message);
				}
			}
			
			/* insert the 'receiver' into the MethodType just as is done by InterfaceHandle */
			type = type.insertParameterTypes(0, declaringClass);
			
			MethodHandle thrower = throwException(type.returnType(), AbstractMethodError.class);
			MethodHandle constructor;
			try {
				constructor = IMPL_LOOKUP.findConstructor(AbstractMethodError.class, MethodType.methodType(void.class));
			} catch (IllegalAccessException | NoSuchMethodException e) {
				throw new InternalError("Unable to find AbstractMethodError.<init>()");  //$NON-NLS-1$
			}
			MethodHandle handle = foldArguments(thrower, constructor);
			handle = dropArguments(handle, 0, type.parameterList());
			
			if (isVarargs(modifiers)) {
				Class<?> lastClass = handle.type.lastParameterType();
				handle = handle.asVarargsCollector(lastClass);
			}
			
			return handle;
		}

		/**
		 * Return a MethodHandle for the reflect constructor. The MethodType has a return type
		 * of the declared class, and the arguments of the constructor.  The MehtodHnadle
		 * creates a new object as through by newInstance.  
		 * <p>
		 * If the <code>accessible</code> flag is not set, then access checking
		 * is performed using the lookup class.
		 * 
		 * @param method - the Reflect constructor
		 * @return a Methodhandle that creates new instances using the requested constructor
		 * @throws IllegalAccessException - if access is denied
		 */
		public MethodHandle unreflectConstructor(Constructor<?> method) throws IllegalAccessException {
			String methodName = method.getName();
			Map<CacheKey, WeakReference<MethodHandle>> cache = HandleCache.getConstructorCache(method.getDeclaringClass());
			MethodType type = MethodType.methodType(void.class, method.getParameterTypes());
			MethodHandle handle = HandleCache.getMethodFromPerClassCache(cache, methodName, type);
			if (handle == null) {
				handle = new ConstructorHandle(method);
				handle = convertToVarargsIfRequired(handle);
				HandleCache.putMethodInPerClassCache(cache, methodName, type, handle);
			} 
			
			if (!method.isAccessible()) {
				checkAccess(handle);
			}
			
			return handle;
		}

		/**
		 * Return a MethodHandle that will create an object of the required class and initialize it using
		 * the constructor method with signature <i>type</i>.  The MethodHandle will have a MethodType
		 * with the same parameters as the constructor method, but will have a return type of the 
		 * <i>declaringClass</i> instead of void.
		 * 
		 * @param declaringClass - Class that declares the constructor
		 * @param type - the MethodType of the constructor.  Return type must be void.
		 * @return a MethodHandle able to construct and initialize the required class
		 * @throws IllegalAccessException if access is denied
		 * @throws NoSuchMethodException if the method doesn't exist
		 * @throws SecurityException if the SecurityManager prevents access
		 * @throws NullPointerException if any of the arguments are null
		 */
		public MethodHandle findConstructor(Class<?> declaringClass, MethodType type) throws IllegalAccessException, NoSuchMethodException {
			nullCheck(declaringClass, type);
			Map<CacheKey, WeakReference<MethodHandle>> cache = HandleCache.getConstructorCache(declaringClass);
			MethodHandle handle = HandleCache.getMethodFromPerClassCache(cache, "<init>", type); //$NON-NLS-1$
			if (handle == null) {
				handle = new ConstructorHandle(declaringClass, type);
				if (type.returnType() != void.class) {
					throw new NoSuchMethodException();
				}
				handle = HandleCache.putMethodInPerClassCache(cache, "<init>", type, convertToVarargsIfRequired(handle)); //$NON-NLS-1$
			}
			checkAccess(handle);
			checkSecurity(handle.getDefc(), declaringClass, handle.getModifiers());
			return handle;
		}
		
		/**
		 * Return a MethodHandle for the Reflect method, that will directly call the requested method
		 * as through from the class <code>specialToken</code>.  The MethodType of the method handle 
		 * will be that of the method with the receiver argument prepended.
		 * 
		 * @param method - a Reflect method
		 * @param specialToken - the class the call is from
		 * @return a MethodHandle that directly dispatches the requested method
		 * @throws IllegalAccessException - if access is denied
		 */
		public MethodHandle unreflectSpecial(Method method, Class<?> specialToken) throws IllegalAccessException {
			nullCheck(method, specialToken);
			checkSpecialAccess(specialToken);	/* Must happen before method resolution */
			String methodName = method.getName();
			Map<CacheKey, WeakReference<MethodHandle>> cache = HandleCache.getSpecialCache(method.getDeclaringClass());
			MethodType type = MethodType.methodType(method.getReturnType(), method.getParameterTypes());
			MethodHandle handle = HandleCache.getMethodWithSpecialCallerFromPerClassCache(cache, methodName, type, specialToken);
			if (handle == null) {
				if (Modifier.isStatic(method.getModifiers())) {
					throw new IllegalAccessException();
				}
				/* Does not require 'restrictReceiver()'as DirectHandle(KIND_SPECIAL) sets receiver type to specialToken */
				handle = convertToVarargsIfRequired(new DirectHandle(method, MethodHandle.KIND_SPECIAL, specialToken));
				HandleCache.putMethodWithSpecialCallerInPerClassCache(cache, methodName, type, handle, specialToken);
			}
			
			if (!method.isAccessible()) {
				checkAccess(handle);
			}
			
			handle = SecurityFrameInjector.wrapHandleWithInjectedSecurityFrameIfRequired(this, handle);
									
			return handle;
		}
		
		/**
		 * Create a MethodHandle that returns the value of the Reflect field.  There are two cases:
		 * <ol>
		 * <li>a static field - which has the MethodType with only a return type of the field</li>
		 * <li>an instance field - which has the MethodType with a return type of the field and a 
		 * single argument of the object that contains the field</li>
		 * </ol>
		 * <p>
		 * If the <code>accessible</code> flag is not set, then access checking
		 * is performed using the lookup class.
		 * 
		 * @param field - a Reflect field
		 * @return a MethodHandle able to return the field value
		 * @throws IllegalAccessException - if access is denied
		 */
		public MethodHandle unreflectGetter(Field field) throws IllegalAccessException {
			int modifiers = field.getModifiers();
			String fieldName = field.getName();
			Class<?> declaringClass = field.getDeclaringClass();
			Class<?> fieldType = field.getType();
			Map<CacheKey, WeakReference<MethodHandle>> cache;
			if (Modifier.isStatic(modifiers)) {
				cache = HandleCache.getStaticFieldGetterCache(declaringClass);
			} else {
				cache = HandleCache.getFieldGetterCache(declaringClass);
			}
			
			MethodHandle handle = HandleCache.getFieldFromPerClassCache(cache, fieldName, fieldType);
			if (handle == null) {
				if (Modifier.isStatic(modifiers)) {
					handle = new StaticFieldGetterHandle(field);
				} else {
					handle = new FieldGetterHandle(field);
				}
				HandleCache.putFieldInPerClassCache(cache, fieldName, fieldType, handle);
			}			
			if (!field.isAccessible()) {
				checkAccess(handle);
			}
			return handle;
		}
		
		/**
		 * Create a MethodHandle that sets the value of the Reflect field.  All MethodHandles created
		 * here have a return type of void.  For the arguments, there are two cases:
		 * <ol>
		 * <li>a static field - which takes a single argument the same as the field</li>
		 * <li>an instance field - which takes two arguments, the object that contains the field, and the type of the field</li>
		 * </ol>
		 * <p>
		 * If the <code>accessible</code> flag is not set, then access checking
		 * is performed using the lookup class.
		 * 
		 * @param field - a Reflect field
		 * @return a MethodHandle able to set the field value
		 * @throws IllegalAccessException - if access is denied
		 */
		public MethodHandle unreflectSetter(Field field) throws IllegalAccessException {
			MethodHandle handle;
			int modifiers = field.getModifiers();
			Map<CacheKey, WeakReference<MethodHandle>> cache;
			Class<?> declaringClass = field.getDeclaringClass();
			Class<?> fieldType = field.getType();
			String fieldName = field.getName();
			
			if (Modifier.isFinal(modifiers)) {
				/*[MSG "K05cf", "illegal setter on final field"]*/
				throw new IllegalAccessException(Msg.getString("K05cf")); //$NON-NLS-1$
			}
			
			if (Modifier.isStatic(modifiers)) {
				cache = HandleCache.getStaticFieldSetterCache(declaringClass);
			} else {
				cache = HandleCache.getFieldSetterCache(declaringClass);
			}
			
			handle = HandleCache.getFieldFromPerClassCache(cache, fieldName, fieldType);
			if (handle == null) {
				if (Modifier.isStatic(modifiers)) {
					handle = new StaticFieldSetterHandle(field);
				} else {
					handle = new FieldSetterHandle(field);
				}

				HandleCache.putFieldInPerClassCache(cache, fieldName, fieldType, handle);
			}
			
			if (!field.isAccessible()) {
				checkAccess(handle);
			}
			return handle;
		}
		
		/**
		 * Cracks a MethodHandle, which allows access to its symbolic parts. 
		 * The MethodHandle must have been created by this Lookup object or one that is able to recreate the MethodHandle. 
		 * If the Lookup object is not able to recreate the MethodHandle, the cracking may fail.
		 * 
		 * @param target The MethodHandle to be cracked
		 * @return a MethodHandleInfo which provides access to the target's symbolic parts  
		 * @throws IllegalArgumentException if the target is not a direct handle, or if the access check fails 
		 * @throws NullPointerException if the target is null
		 * @throws SecurityException if a SecurityManager denies access
		 */
		public MethodHandleInfo revealDirect(MethodHandle target) throws IllegalArgumentException, NullPointerException, SecurityException {
			if (!target.canRevealDirect()) { // Implicit null check
				target = SecurityFrameInjector.penetrateSecurityFrame(target, this);
				if ((target == null) || !target.canRevealDirect()) {
					/*[MSG "K0584", "The target is not a direct handle"]*/
					throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0584")); //$NON-NLS-1$
				}
			}
			try {
				checkAccess(target);
				checkSecurity(target.getDefc(), target.getReferenceClass(), target.getModifiers());
			} catch (IllegalAccessException e) {
				throw new IllegalArgumentException(e);
			}
			if (target instanceof VarargsCollectorHandle) {
				target = ((VarargsCollectorHandle)target).next;
			}
			return new MethodHandleInfoImpl((PrimitiveHandle)target);
		}

		@Override
		public String toString() {
			String toString = accessClass.getName();
			switch(accessMode) {
			case NO_ACCESS:
				toString += "/noaccess"; //$NON-NLS-1$
				break;
			case PUBLIC:
				toString += "/public"; //$NON-NLS-1$
				break;
			/*[IF Sidecar19-SE-B175]
			case PUBLIC | UNCONDITIONAL:
				toString += "/publicLookup"; //$NON-NLS-1$
				break;
			case PUBLIC | MODULE:
				toString += "/module"; //$NON-NLS-1$
				break;
			case PUBLIC | PACKAGE | MODULE:
				toString += "/package"; //$NON-NLS-1$
				break;
			case PUBLIC | PACKAGE | PRIVATE | MODULE:
				toString += "/private"; //$NON-NLS-1$
				break;
			/*[ELSE]*/
			case PUBLIC | PACKAGE:
				toString += "/package"; //$NON-NLS-1$
				break;
			case PUBLIC | PACKAGE | PRIVATE:
				toString += "/private"; //$NON-NLS-1$
				break;
			/*[ENDIF] Sidecar19-SE-B175*/
			}
			return toString;
		}
		
		/*
		 * Determine if this lookup has been weakened.
		 * A lookup is weakened when it doesn't have private access.
		 * 
		 * return true if the lookup has been weakened.
		 */
		boolean isWeakenedLookup() {
			return PRIVATE != (accessMode & PRIVATE);
		}
		
		/*
		 * If there is a security manager, this performs 1 to 3 different security checks:
		 * 
		 * 1) secmgr.checkPackageAccess(refcPkg), if classloader of access class is not same or ancestor of classloader of reference class
		 * 2) secmgr.checkPermission(new RuntimePermission("accessDeclaredMembers")), if retrieved member is not public
		 * 3) secmgr.checkPackageAccess(defcPkg), if retrieved member is not public, and if defining class and reference class are in different classloaders, 
		 *    and if classloader of access class is not same or ancestor of classloader of defining class
		 */
		void checkSecurity(Class<?> definingClass, Class<?> referenceClass, int modifiers) throws IllegalAccessException {
			if (accessMode == INTERNAL_PRIVILEGED) {
				// Full access for use by MH implementation.
				return;
			}
			if (null == definingClass) {
				throw new IllegalAccessException();
			}
			
			if (performSecurityCheck) {
				SecurityManager secmgr = System.getSecurityManager();
				if (secmgr != null) {
					/* Use leaf-class in the case of arrays for security check */
					while (definingClass.isArray()) {
						definingClass = definingClass.getComponentType();
					}
					while (referenceClass.isArray()) {
						referenceClass = referenceClass.getComponentType();
					}

					VMLangAccess vma = getVMLangAccess();

					ClassLoader referenceClassLoader = referenceClass.getClassLoader();
					if (isWeakenedLookup() || !doesClassLoaderDescendFrom(referenceClassLoader, accessClass.getClassLoader())) {
						String packageName = vma.getPackageName(referenceClass);
						secmgr.checkPackageAccess(packageName);
					}

					if (!Modifier.isPublic(modifiers)) {
						if (vma.getClassloader(definingClass) != vma.getClassloader(accessClass)) {
							secmgr.checkPermission(com.ibm.oti.util.RuntimePermissions.permissionAccessDeclaredMembers);
						}

						/* third check */
						if (definingClass.getClassLoader() != referenceClassLoader) {
							if (!doesClassLoaderDescendFrom(definingClass.getClassLoader(), accessClass.getClassLoader())) {
								secmgr.checkPackageAccess(vma.getPackageName(definingClass));
							}
						}
					}
				}
			}
		}
		
		/*
		 * Return a MethodHandle that will throw the passed in Exception object with the passed in message. 
		 * The MethodType is of the returned MethodHandle is set to the passed in type.
		 * 
		 * @param exceptionClass The type of exception to throw
		 * @param type The MethodType of the generated MethodHandle
		 * @param msg The message to be used in the exception
		 * @return A MethodHandle with the requested MethodType that throws the passed in Exception 
		 * @throws IllegalAccessException If MethodHandle creation fails
		 * @throws NoSuchMethodException If MethodHandle creation fails
		 */
		private static MethodHandle throwExceptionWithMethodTypeAndMessage(Class<? extends Throwable> exceptionClass, MethodType type, String msg)  throws IllegalAccessException, NoSuchMethodException {
			MethodHandle thrower = throwException(type.returnType(), exceptionClass);
			MethodHandle constructor = IMPL_LOOKUP.findConstructor(exceptionClass, MethodType.methodType(void.class, String.class));
			MethodHandle result = foldArguments(thrower, constructor.bindTo(msg));
			
			/* Change result MethodType to the requested type */
			result = result.asType(MethodType.methodType(type.returnType));
			result = dropArguments(result, 0, type.parameterList());
			return result;
		}
		
		/*[IF Sidecar19-SE]*/
		/**
		 * Factory method for obtaining a VarHandle referencing a non-static field.
		 * 
		 * @param definingClass The class that contains the field
		 * @param name The name of the field
		 * @param type The exact type of the field
		 * @return A VarHandle that can access the field
		 * @throws NoSuchFieldException If the field doesn't exist
		 * @throws IllegalAccessException If the field is not accessible or doesn't exist
		 */
		public VarHandle findVarHandle(Class<?> definingClass, String name, Class<?> type) throws NoSuchFieldException, IllegalAccessException {
			return findVarHandleCommon(definingClass, name, type, false);
		}
		
		/**
		 * Factory method for obtaining a VarHandle referencing a static field.
		 * 
		 * @param definingClass The class that contains the field
		 * @param name The name of the field
		 * @param type The type of the field
		 * @return A VarHandle that can access the field
		 * @throws NoSuchFieldException If the field doesn't exist
		 * @throws IllegalAccessException If the field is not accessible or doesn't exist
		 */
		public VarHandle findStaticVarHandle(Class<?> definingClass, String name, Class<?> type) throws NoSuchFieldException, IllegalAccessException {
			return findVarHandleCommon(definingClass, name, type, true);
		}
		
		private VarHandle findVarHandleCommon(Class<?> definingClass, String name, Class<?> type, boolean isStatic) throws NoSuchFieldException, IllegalAccessException {
			Objects.requireNonNull(definingClass);
			Objects.requireNonNull(name);
			Objects.requireNonNull(type);
			
			try {
				VarHandle handle;
				if (isStatic) {
					handle = new StaticFieldVarHandle(definingClass, name, type, this.accessClass);
				} else {
					handle = new InstanceFieldVarHandle(definingClass, name, type, this.accessClass);
				}
				checkAccess(handle);
				checkSecurity(definingClass, definingClass, handle.getModifiers());
				return handle;
			} catch (NoSuchFieldError e) {
				/*[MSG "K0636", "No such field: {0}.{1}({2})"]*/
				throw new NoSuchFieldException(com.ibm.oti.util.Msg.getString("K0636", definingClass.getCanonicalName(), name, type.getCanonicalName())); //$NON-NLS-1$
			} catch (IncompatibleClassChangeError e) {
				if (isStatic) {
					/*[MSG "K0635", "{0}.{1}({2}) is non-static. Expected static field."]*/
					throw new IllegalAccessException(com.ibm.oti.util.Msg.getString("K0635", definingClass.getCanonicalName(), name, type.getCanonicalName())); //$NON-NLS-1$
				} else {
					/*[MSG "K0634", "{0}.{1}({2}) is static. Expected instance field."]*/
					throw new IllegalAccessException(com.ibm.oti.util.Msg.getString("K0634", definingClass.getCanonicalName(), name, type.getCanonicalName())); //$NON-NLS-1$
				}
			}
		}
		
		/**
		 * Create a {@link VarHandle} that operates on the field represented by the {@link java.lang.reflect.Field reflect field}.
		 * <p>
		 * Access checking is performed regardless of whether the reflect field's <code>accessible</code> flag is set.
		 * </p>
		 * @param field - a reflect field
		 * @return a VarHandle able to operate on the field
		 * @throws IllegalAccessException - if access is denied
		 */
		public VarHandle unreflectVarHandle(Field field) throws IllegalAccessException {
			VarHandle handle = null;

			// Implicit null-check of field argument
			if (Modifier.isStatic(field.getModifiers())) {
				handle = new StaticFieldVarHandle(field, field.getType());
			} else {
				handle = new InstanceFieldVarHandle(field, field.getDeclaringClass(), field.getType());
			}
			
			checkAccess(handle);
			checkSecurity(handle.getDefiningClass(), handle.getDefiningClass(), handle.modifiers);
			
			return handle;
		}
		
		/**
		 * Search the target class by class name via the lookup context.
		 * The target class is not statically initialized.
		 * <p>
		 * The lookup context depends upon the lookup class, its classloader and   
		 * its access modes. This method tells whether the target class is accessible 
		 * to this lookup class after successfully loading the target class.
		 * 
		 * @param targetName - the name of the target class
		 * @return the target class
		 * @throws ClassNotFoundException if the lookup class's classloader fails to load the target class.
		 * @throws IllegalAccessException if access is denied
		 * @throws SecurityException if the SecurityManager prevents access
		 * @throws LinkageError if it fails in linking
		 */
		public Class<?> findClass(String targetName) throws ClassNotFoundException, IllegalAccessException {
			Class<?> targetClass = Class.forName(targetName, false, accessClass.getClassLoader());
			return accessClass(targetClass);
		}
		
		/**
		 * Tells whether the target class is accessible to the lookup class.
		 * The target class is not statically initialized.
		 * <p>
		 * The lookup context depends upon the lookup class and its access modes.
		 * 
		 * @param targetClass - the target class to be checked
		 * @return the target class after check
		 * @throws IllegalAccessException if access is denied
		 * @throws SecurityException if the SecurityManager prevents access
		 */
		public Class<?> accessClass(Class<?> targetClass) throws IllegalAccessException {
			targetClass.getClass(); /* implicit null checks */
			int targetClassModifiers = targetClass.getModifiers();
			checkAccess(targetClass);
			checkSecurity(targetClass, targetClass, targetClassModifiers);
			return targetClass;
		}
		
		/*[IF Sidecar19-SE-B175]*/
		/**
		 * Return a class object with the same class loader, the same package and
		 * the same protection domain as the lookup's lookup class.
		 * 
		 * @param classBytes the requested class bytes from a valid class file
		 * @return a class object for the requested class bytes
		 * @throws NullPointerException if the requested class bytes is null
		 * @throws IllegalArgumentException if the requested class belongs to another package different from the lookup class
		 * @throws IllegalAccessException if the access mode of the lookup doesn't include PACKAGE
		 * @throws SecurityException if the SecurityManager prevents access
		 * @throws LinkageError if the requested class is malformed, or is already defined, or fails in bytecode verification or linking
		 */
		public Class<?> defineClass(byte[] classBytes) throws NullPointerException, IllegalAccessException, IllegalArgumentException, SecurityException, LinkageError {
			if (Objects.isNull(classBytes)) {
				/*[MSG "K065X", "The class byte array must not be null"]*/
				throw new NullPointerException(com.ibm.oti.util.Msg.getString("K065X")); //$NON-NLS-1$
			}
			
			if (PACKAGE != (PACKAGE & accessMode)) {
				/*[MSG "K065Y1", "The access mode: 0x{0} of the lookup class doesn't have the PACKAGE mode: 0x{1}"]*/
				throw new IllegalAccessException(com.ibm.oti.util.Msg.getString("K065Y1", Integer.toHexString(accessMode), Integer.toHexString(Lookup.PACKAGE))); //$NON-NLS-1$
			}
			
			SecurityManager secmgr = System.getSecurityManager();
			if (null != secmgr) {
				secmgr.checkPermission(com.ibm.oti.util.RuntimePermissions.permissionDefineClass);
			}
			
			/* Extract the package name from the class bytes so as to check whether
			 * the requested class shares the same package as the lookup class.
			 * Note: the class bytes should be considered as corrupted if any exception
			 *       is captured in ASM class reader.
			 */
			ClassReader cr = null;
			try {
				cr = new ClassReader(classBytes);
			} catch (ArrayIndexOutOfBoundsException e) {
				/*[MSG "K065Y2", "The class byte array is corrupted"]*/
				throw new ClassFormatError(com.ibm.oti.util.Msg.getString("K065Y2")); //$NON-NLS-1$
			}
			
			String targetClassName = cr.getClassName().replace('/', '.');
			int separatorIndex = targetClassName.lastIndexOf('.');
			String targetClassPackageName = ""; //$NON-NLS-1$
			
			if (separatorIndex > 0) {
				targetClassPackageName = targetClassName.substring(0, separatorIndex);
			}
			
			String lookupClassPackageName = accessClass.getPackageName();
			if (!Objects.equals(lookupClassPackageName, targetClassPackageName)) {
				/*[MSG "K065Y3", "The package of the requested class: {0} is different from the package of the lookup class: {1}"]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K065Y3", targetClassPackageName, lookupClassPackageName)); //$NON-NLS-1$
			}
			
			JavaLangAccess jlAccess = SharedSecrets.getJavaLangAccess();
			Class<?> targetClass = jlAccess.defineClass(accessClass.getClassLoader(), targetClassName, classBytes, accessClass.getProtectionDomain(), null);
			
			return targetClass;
		}
		/*[ENDIF] Sidecar19-SE-B175*/
		
		/**
		 * Return a MethodHandles.Lookup object without the requested lookup mode.
		 * 
		 * @param dropMode the mode to be dropped
		 * @return a MethodHandles.Lookup object without the requested lookup mode
		 * @throws IllegalArgumentException - if the requested lookup mode is not one of the existing access modes
		 */
		public MethodHandles.Lookup dropLookupMode(int dropMode) {
			/* The UNCONDITIONAL bit is not included in FULL_ACCESS_MASK 
			 * as it is not set up for lookup objects by default.
			 */
			int fullAccessMode = FULL_ACCESS_MASK | MODULE | UNCONDITIONAL;
			
			if (0 == (fullAccessMode & dropMode)) {
				/*[MSG "K065R", "The requested lookup mode: 0x{0} is not one of the existing access modes: 0x{1}"]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K065R", Integer.toHexString(dropMode), Integer.toHexString(fullAccessMode))); //$NON-NLS-1$
			}
			
			/* The lookup object has to discard the protected and unconditional access by default */
			int newAccessMode = accessMode & ~(PROTECTED | UNCONDITIONAL);
			
			if (PRIVATE == (PRIVATE & dropMode)) {
				newAccessMode &= ~PRIVATE;
			}
			
			/* The lookup object has no package and private access if the PACKAGE bit is dropped */
			if (PACKAGE == (PACKAGE & dropMode)) {
				newAccessMode &= ~(PACKAGE | PRIVATE);
			}
			
			/* The lookup object has no module, package and private access if the MODULE bit is dropped */
			if (MODULE == (MODULE & dropMode)) {
				newAccessMode &= ~(MODULE | PACKAGE | PRIVATE);
			}
			
			/* The lookup object has no access at all if the PUBLIC bit is dropped */
			if (PUBLIC == (PUBLIC & dropMode)) {
				newAccessMode = NO_ACCESS;
			}
			
			return new Lookup(accessClass, newAccessMode); 
		}
		
		/**
		 * Return true if the lookup class has private access
		 * 
		 * @return a boolean type indicating whether the lookup class has private access
		 */
		public boolean hasPrivateAccess() {
			/* Full access for use by MH implementation */
			if (INTERNAL_PRIVILEGED == accessMode) {
				return true;
			}
			
			return !isWeakenedLookup();
		}
		/*[ENDIF]*/
	}
	
	static MethodHandle filterArgument(MethodHandle target, int pos, MethodHandle filter) {
		return filterArguments(target, pos, new MethodHandle[] { filter });
	}
	
	/**
	 * Return a MethodHandles.Lookup object for the caller.
	 * 
	 * @return a MethodHandles.Lookup object
	 */
	@CallerSensitive
	public static MethodHandles.Lookup lookup() {
		/*[IF ]*/
		// Use getStackClass(1) as getStackClass is an INL and doesn't build a frame
		// The JIT is being modified to enable getStackClass to be inlined.  This is
		// "easy" to do as they already support inlining sun.reflect.Reflection.getCallerClass(2).
		// We use the INL so it avoids the 4 atomic operations at the expensive of a slightly longer
		// path length for the JIT to get to the INL when not inlined.
		/*[ENDIF]*/
		Class<?> c = getStackClass(1);
		return new Lookup(c);
	}
	
	/**
	 * Return a MethodHandles.Lookup object that is only able to access <code>public</code> members.
	 * 
	 * @return a MethodHandles.Lookup object
	 */
	public static MethodHandles.Lookup publicLookup() {
		return Lookup.PUBLIC_LOOKUP;
	}
	
	/*[IF Sidecar19-SE-B175]*/
	/**
	 * Return a MethodHandles.Lookup object with full capabilities including the access 
	 * to the <code>private</code> members in the requested class
	 * 
	 * @param targetClass - the requested class containing private members
	 * @param callerLookup - a Lookup object specific to the caller
	 * @return a MethodHandles.Lookup object with private access to the requested class
	 * @throws NullPointerException - if targetClass or callerLookup is null
	 * @throws IllegalArgumentException - if the requested Class is a primitive type or an array class
	 * @throws IllegalAccessException - if access checking fails
	 * @throws SecurityException - if the SecurityManager prevents access
	 */
	public static MethodHandles.Lookup privateLookupIn(Class<?> targetClass, MethodHandles.Lookup callerLookup) throws NullPointerException, IllegalArgumentException, IllegalAccessException, SecurityException {
		if (Objects.isNull(targetClass) || Objects.isNull(callerLookup)) {
			/*[MSG "K065S", "Both the requested class and the caller lookup must not be null"]*/
			throw new NullPointerException(com.ibm.oti.util.Msg.getString("K065S")); //$NON-NLS-1$
		}
		
		if (targetClass.isPrimitive() || targetClass.isArray()) {
			/*[MSG "K065T", "The target class: {0} must not be a primitive type or an array class"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K065T", targetClass.getCanonicalName())); //$NON-NLS-1$
		}
		
		Module targetClassModule = targetClass.getModule();
		String targetClassPackageName = targetClass.getPackageName();
		Module accessClassModule = callerLookup.accessClass.getModule();
		
		/* Check whether the named module containing the old lookup can read the module containing the target class.
		 * Note: an unnamed module can read any module.
		 */
		if (!accessClassModule.canRead(targetClassModule)) {
			/*[MSG "K065U", "The module: {0} containing the old lookup can't read the module: {1}"]*/
			throw new IllegalAccessException(com.ibm.oti.util.Msg.getString("K065U", accessClassModule.getName(), targetClassModule.getName())); //$NON-NLS-1$
		}
		
		/* Check whether the module has the package (containing the target class) opened to
		 * the module containing the old lookup.
		 */
		if (!targetClassModule.isOpen(targetClassPackageName, accessClassModule)) {
			/*[MSG "K065V", "The package: {0} containing the target class is not opened to the module: {1}"]*/
			throw new IllegalAccessException(com.ibm.oti.util.Msg.getString("K065V", targetClassPackageName, accessClassModule.getName())); //$NON-NLS-1$
		}
		
		int callerLookupMode = callerLookup.lookupModes();
		if (Lookup.MODULE != (Lookup.MODULE & callerLookupMode)) {
			/*[MSG "K065W", "The access mode: 0x{0} of the caller lookup doesn't have the MODULE mode : 0x{1}"]*/
			throw new IllegalAccessException(com.ibm.oti.util.Msg.getString("K065W", Integer.toHexString(callerLookupMode), Integer.toHexString(Lookup.MODULE))); //$NON-NLS-1$
		}
		
		SecurityManager secmgr = System.getSecurityManager();
		if (null != secmgr) {
			secmgr.checkPermission(com.ibm.oti.util.ReflectPermissions.permissionSuppressAccessChecks);
		}
		
		return new Lookup(targetClass);
	}
	/*[ENDIF] Sidecar19-SE-B175*/
	
	/**
	 * Gets the underlying Member of the provided <code>target</code> MethodHandle. This is done through an unchecked crack of the MethodHandle.
	 * Calling this method is equivalent to obtaining a lookup object capable of cracking the <code>target</code> MethodHandle, calling
	 * <code>Lookup.revealDirect</code> on the <code>target</code> MethodHandle and then calling <code>MethodHandleInfo.reflectAs</code>. 
	 * 
	 * If a SecurityManager is present, this method requires <code>ReflectPermission("suppressAccessChecks")</code>.
	 * 
	 * @param expected the expected type of the underlying member
	 * @param target the direct MethodHandle to be cracked
	 * @return the underlying member of the <code>target</code> MethodHandle
	 * @throws SecurityException if the caller does not have the required permission (<code>ReflectPermission("suppressAccessChecks")</code>)
	 * @throws NullPointerException if either of the arguments are <code>null</code>
	 * @throws IllegalArgumentException if the <code>target</code> MethodHandle is not a direct MethodHandle
	 * @throws ClassCastException if the underlying member is not of the <code>expected</code> type
	 */
	public static <T extends Member> T reflectAs(Class<T> expected, MethodHandle target) throws SecurityException, NullPointerException, IllegalArgumentException, ClassCastException {
		if ((null == expected) || (null == target)) {
			throw new NullPointerException();
		}
		SecurityManager secmgr = System.getSecurityManager();
		if (null != secmgr) {
			secmgr.checkPermission(com.ibm.oti.util.ReflectPermissions.permissionSuppressAccessChecks);
		}
		MethodHandleInfo mhi = Lookup.IMPL_LOOKUP.revealDirect(target);
		T result = mhi.reflectAs(expected, Lookup.IMPL_LOOKUP);
		return result;
	}
	
	/**
	 * Return a MethodHandle that is the equivalent of calling 
	 * MethodHandles.lookup().findVirtual(MethodHandle.class, "invokeExact", type).
	 * <p>
	 * The MethodHandle has a method type that is the same as type except that an additional 
	 * argument of MethodHandle will be added as the first parameter.
	 * <p>
	 * This method is not subject to the same security checks as a findVirtual call.
	 * 
	 * @param type - the type of the invokeExact call to lookup
	 * @return a MethodHandle equivalent to calling invokeExact on the first argument.
	 * @throws IllegalArgumentException if the resulting MethodHandle would take too many parameters.
	 */
	public static MethodHandle exactInvoker(MethodType type) throws IllegalArgumentException {
		return type.getInvokeExactHandle();
	}
	
	/**
	 * Return a MethodHandle that is the equivalent of calling 
	 * MethodHandles.lookup().findVirtual(MethodHandle.class, "invoke", type).
	 * <p>
	 * The MethodHandle has a method type that is the same as type except that an additional 
	 * argument of MethodHandle will be added as the first parameter.
	 * <p>
	 * This method is not subject to the same security checks as a findVirtual call.
	 * 
	 * @param type - the type of the invoke call to lookup
	 * @return a MethodHandle equivalent to calling invoke on the first argument.
	 * @throws IllegalArgumentException if the resulting MethodHandle would take too many parameters.
	 */
	public static MethodHandle invoker(MethodType type) throws IllegalArgumentException {
		type.getClass(); // implicit nullcheck
		return new InvokeGenericHandle(type);
	}
	
	/**
	 * Return a MethodHandle that is able to invoke a MethodHandle of <i>type</i> as though by 
	 * invoke after spreading the final Object[] parameter.
	 * <p>
	 * When the <code>MethodHandle</code> is invoked, the argument array must contain exactly <i>spreadCount</i> arguments 
	 * to be passed to the original <code>MethodHandle</code>.  The array may be null in the case when <i>spreadCount</i> is zero.
	 * Incorrect argument array size will cause the method to throw an <code>IllegalArgumentException</code> instead of invoking the target.
	 * </p>
	 * @param type - the type of the invoke method to look up
	 * @param fixedArgCount - the number of fixed arguments in the methodtype
	 * @return a MethodHandle that invokes its first argument after spreading the Object array
	 * @throws IllegalArgumentException if the fixedArgCount is less than 0 or greater than type.ParameterCount(), or if the resulting MethodHandle would take too many parameters.
	 * @throws NullPointerException if the type is null
	 */
	public static MethodHandle spreadInvoker(MethodType type, int fixedArgCount) throws IllegalArgumentException, NullPointerException {
		int typeParameterCount = type.parameterCount();
		if ((fixedArgCount < 0) || (fixedArgCount > typeParameterCount)) {
			/*[MSG "K039c", "Invalid parameters"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K039c")); //$NON-NLS-1$
		}
		MethodHandle invoker = invoker(type);
		int spreadArgCount = typeParameterCount - fixedArgCount;
		return invoker.asSpreader(Object[].class, spreadArgCount);
	}

	/**
	 * Produce a MethodHandle that implements an if-else block.
	 * 
	 * This MethodHandle is composed from three handles:
	 * <ul>
	 * <li>guard - a boolean returning handle that takes a subset of the arguments passed to the true and false targets</li>
	 * <li>trueTarget - the handle to call if the guard returns true</li>
	 * <li>falseTarget - the handle to call if the guard returns false</li>
	 * </ul>
	 * 
	 * @param guard - method handle returning boolean to determine which target to call
	 * @param trueTarget - target method handle to call if guard is true
	 * @param falseTarget - target method handle to call if guard is false
	 * @return A MethodHandle that implements an if-else block.
	 * 
	 * @throws NullPointerException - if any of the three method handles are null
	 * @throws IllegalArgumentException - if any of the following conditions are true:
	 * 				1) trueTarget and falseTarget have different MethodTypes
	 * 				2) the guard handle doesn't have a boolean return value
	 * 				3) the guard handle doesn't take a subset of the target handle's arguments
	 */
	public static MethodHandle guardWithTest(MethodHandle guard, MethodHandle trueTarget, MethodHandle falseTarget) throws NullPointerException, IllegalArgumentException{
		MethodType guardType = guard.type;
		MethodType trueType = trueTarget.type;
		MethodType falseType = falseTarget.type;
		if (trueType != falseType) {
			throw new IllegalArgumentException();
		}
		int testArgCount = guardType.parameterCount();
		if ((guardType.returnType != boolean.class) || (testArgCount > trueType.parameterCount())) {
			throw new IllegalArgumentException();
		}
		for (int i = 0; i < testArgCount; i++) {
			if (guardType.arguments[i] != trueType.arguments[i]) {
				throw new IllegalArgumentException();
			}
		}
		
		MethodHandle result = GuardWithTestHandle.get(guard, trueTarget, falseTarget);
		return result;
	}
	
	/**
	 * Produce a MethodHandle that implements a try-catch block.
	 * 
	 * This adapter acts as though the <i>tryHandle</i> where run inside a try block.  If <i>tryHandle</i>
	 * throws an exception of type <i>throwableClass</i>, the <i>catchHandle</i> is invoked with the 
	 * exception instance and the original arguments.
	 * <p>
	 * The catchHandle may take a subset of the original arguments rather than the full set.  Its first
	 * argument will be the exception instance.
	 * <p>
	 * Both the catchHandle and the tryHandle must have the same return type.
	 * 
	 * @param tryHandle - the method handle to wrap with the try block
	 * @param throwableClass - the class of exception to be caught and handled by catchHandle
	 * @param catchHandle - the method handle to call if an exception of type throwableClass is thrown by tryHandle
	 * @return a method handle that will call catchHandle if tryHandle throws an exception of type throwableClass
	 * 
	 * @throws NullPointerException - if any of the parameters are null
	 * @throws IllegalArgumentException - if tryHandle and catchHandle have different return types, 
	 * or the catchHandle doesn't take a throwableClass as its first argument,
	 * of if catchHandle arguments[1-N] differ from tryHandle arguments[0-(N-1)] 
	 */
	public static MethodHandle catchException(MethodHandle tryHandle, Class<? extends Throwable> throwableClass, MethodHandle catchHandle) throws NullPointerException, IllegalArgumentException{
		if ((tryHandle == null) || (throwableClass == null) || (catchHandle == null)) {
			throw new NullPointerException();
		}
		MethodType tryType = tryHandle.type;
		MethodType catchType = catchHandle.type;
		if (tryType.returnType != catchType.returnType) {
			throw new IllegalArgumentException();
		}
		if (catchType.parameterType(0) != throwableClass) {
			throw new IllegalArgumentException();
		}
		int catchArgCount =  catchType.parameterCount();
		if ((catchArgCount - 1) > tryType.parameterCount()) {
			throw new IllegalArgumentException();
		}
		Class<?>[] tryParams = tryType.arguments;
		Class<?>[] catchParams = catchType.arguments;
		for (int i = 1; i < catchArgCount; i++) {
			if (catchParams[i] != tryParams[i - 1]) {
				throw new IllegalArgumentException();
			}
		}
		
		MethodHandle result = buildTransformHandle(new CatchHelper(tryHandle, catchHandle, throwableClass), tryType);
		if (true) {
			MethodHandle thunkable = CatchHandle.get(tryHandle, throwableClass, catchHandle, result);
			assert(thunkable.type() == result.type());
			result = thunkable;
		}
		return result;
	}
	
	/*[IF Sidecar19-SE]*/
	/**
	 * Produce a MethodHandle that implements a try-finally block.
	 * 
	 * This adapter acts as though the <i>tryHandle</i> runs inside a try block.
	 * If <i>tryHandle</i> throws an exception or returns as normal, the <i>finallyHandle</i> 
	 * is invoked with the exception (if thrown out from tryHandle), the result from tryHandle 
	 * plus the original arguments of tryHandle.
	 * <p>
	 * The finallyHandle may take a subset of the original arguments rather than the full set. 
	 * Its leading arguments will be the exception and the result from tryHandle which will 
	 * be ignored if the return type of tryHandle is void.
	 * <p>
	 * Both the tryHandle and the finallyHandle must have the same return type.
	 * 
	 * @param tryHandle - the method handle to wrap with the try block
	 * @param finallyHandle - the method handle to wrap with the finally block
	 * @return a method handle that calls finallyHandle when tryHandle returns normally or throws out an exception
	 * 
	 * @throws NullPointerException - if any of the parameters are null
	 * @throws IllegalArgumentException - if tryHandle and finallyHandle have different return types, 
	 * or the finallyHandle doesn't take a Throwable type as its first argument,
	 * or the second argument of finallyHandle is inconsistent with the result type of tryHandle,
	 * of if finallyHandle arguments[1-N] differ from tryHandle arguments[0-(N-1)] in the case of the void result type of tryHandle,
	 * of if finallyHandle arguments[2-N] differ from tryHandle arguments[0-(N-2)] in the case of the non-void result type of tryHandle.
	 */
	public static MethodHandle tryFinally(MethodHandle tryHandle, MethodHandle finallyHandle) throws NullPointerException, IllegalArgumentException{
		Objects.requireNonNull(tryHandle, "The try handle must not be null"); //$NON-NLS-1$
		Objects.requireNonNull(finallyHandle, "The finally handle must not be null"); //$NON-NLS-1$
		
		MethodType tryType = tryHandle.type;
		MethodType finallyType = finallyHandle.type;
		
		if (tryType.returnType != finallyType.returnType) {
			/*[MSG "K063B", "The return type of the try handle: {0} is inconsistent with the return type of the finally handle: {1}"]*/
			throw new IllegalArgumentException(Msg.getString("K063B", new Object[] { //$NON-NLS-1$
							tryType.returnType.getSimpleName(),
							finallyType.returnType.getSimpleName(),}));
		}
		if (Throwable.class != finallyType.parameterType(0)) {
			/*[MSG "K063C", "The 1st parameter type of the finally handle: {0} is not {1}"]*/
			throw new IllegalArgumentException(Msg.getString("K063C", new Object[] { //$NON-NLS-1$
							finallyType.parameterType(0).getSimpleName(),
							Throwable.class.getSimpleName()}));
		}
		if ((void.class != tryType.returnType) && (finallyType.parameterType(1) != tryType.returnType)) {
			/*[MSG "K063D", "The 2nd parameter type of the finally handle: {0} is inconsistent with the return type of the try handle: {1}"]*/
			throw new IllegalArgumentException(Msg.getString("K063D", new Object[] { //$NON-NLS-1$
							finallyType.parameterType(1).getSimpleName(),
							tryType.returnType.getSimpleName()}));
		}
		
		int finallyParamCount =  finallyType.parameterCount();
		Class<?>[] tryParams = tryType.arguments;
		Class<?>[] finallyParams = finallyType.arguments;
		
		if (void.class == tryType.returnType) {
			validateParametersOfMethodTypes(finallyParamCount, tryType, finallyParams, tryParams, 1);
		} else {
			validateParametersOfMethodTypes(finallyParamCount, tryType, finallyParams, tryParams, 2);
		}
		
		MethodHandle result = buildTransformHandle(new FinallyHelper(tryHandle, finallyHandle), tryType);
		MethodHandle thunkable = FinallyHandle.get(tryHandle, finallyHandle, result);
		assert(thunkable.type() == result.type());
		result = thunkable;
		return result;
	}
	
	private static void validateParametersOfMethodTypes(int finallyParamCount, MethodType tryType, Class<?>[] finallyParams, Class<?>[] tryParams, int extraParamNum) {
		int actualfinallyParamCount = finallyParamCount - extraParamNum;
		int tryParamCount = tryType.parameterCount();
		if (actualfinallyParamCount > tryParamCount) {
			/*[MSG "K063E", "The parameter count of the finally handle (excluding the result type and exception): {0} must be <= the parameter count of the try handle: {1}"]*/
			throw new IllegalArgumentException(Msg.getString("K063E", new Object[] { //$NON-NLS-1$
							Integer.toString(actualfinallyParamCount),
							Integer.toString(tryParamCount)}));
		}
		for (int i = extraParamNum; i < finallyParamCount; i++) {
			int indexTryParams = i - extraParamNum;
			if (finallyParams[i] != tryParams[indexTryParams]) {
				/*[MSG "K063F", "The parameter type of the finally handle: {0} at index {1} is inconsistent with the parameter type of the try handle: {2} at index {3}"]*/
				throw new IllegalArgumentException(Msg.getString("K063F", new Object[] { //$NON-NLS-1$
								finallyParams[i].getSimpleName(),
								Integer.toString(i),
								tryParams[indexTryParams].getSimpleName(),
								Integer.toString(indexTryParams)}));
			}
		}
	}
	/*[ENDIF]*/
	
	/**
	 * Produce a MethodHandle that acts as an identity function.  It will accept a single
	 * argument and return it.
	 * 
	 * @param classType - the type to use for the return and parameter types
	 * @return an identity MethodHandle that returns its argument
	 * @throws NullPointerException - if the classType is null
	 * @throws IllegalArgumentException - if the the classType is void.
	 */
	public static MethodHandle identity(Class<?> classType) throws NullPointerException, IllegalArgumentException {
		if (classType == void.class) {
			throw new IllegalArgumentException();
		}
		try {
			MethodType methodType = MethodType.methodType(classType, classType);
			if (classType.isPrimitive()){
				return Lookup.internalPrivilegedLookup.findStatic(MethodHandles.class, "identity", methodType); //$NON-NLS-1$
			}
			
			MethodHandle handle = Lookup.internalPrivilegedLookup.findStatic(MethodHandles.class, "identity", MethodType.methodType(Object.class, Object.class)); //$NON-NLS-1$
			return handle.cloneWithNewType(methodType);
		} catch(IllegalAccessException | NoSuchMethodException e) {
			throw new Error(e);
		}
	}
	
	@SuppressWarnings("unused")
	private static boolean identity(boolean x) {
		return x;
	}
	@SuppressWarnings("unused")
	private static byte identity(byte x) {
		return x;
	}
	@SuppressWarnings("unused")
	private static short identity(short x) {
		return x;
	}
	@SuppressWarnings("unused")
	private static char identity(char x) {
		return x;
	}
	@SuppressWarnings("unused")
	private static int identity(int x) {
		return x;
	}
	@SuppressWarnings("unused")
	private static float identity(float x) {
		return x;
	}
	@SuppressWarnings("unused")
	private static double identity(double x) {
		return x;
	}
	@SuppressWarnings("unused")
	private static long identity(long x) {
		return x;
	}
	@SuppressWarnings("unused")
	private static Object identity(Object x) {
		return x;
	}
	
	/**
	 * Create a MethodHandle that returns the <i>constantValue</i> on each invocation.
	 * <p>
	 * Conversions of the <i>constantValue</i> to the <i>returnType</i> occur if possible, otherwise 
	 * a ClassCastException is thrown.  For primitive <i>returnType</i>, widening primitive conversions are
	 * attempted.  Otherwise, reference conversions are attempted.
	 *  
	 * @param returnType - the return type of the MethodHandle.
	 * @param constantValue - the value to return from the MethodHandle when invoked
	 * @return a MethodHandle that always returns the <i>constantValue</i>
	 * @throws NullPointerException - if the returnType is null
	 * @throws ClassCastException - if the constantValue cannot be converted to returnType
	 * @throws IllegalArgumentException - if the returnType is void
	 */
	public static MethodHandle constant(Class<?> returnType, Object constantValue) throws NullPointerException, ClassCastException, IllegalArgumentException {
		returnType.getClass();	// implicit null check
		if (returnType == void.class) {
			throw new IllegalArgumentException();
		}
		if (returnType.isPrimitive()) {
			if (constantValue == null) {
				throw new IllegalArgumentException();
			}
			Class<?> unwrapped = MethodType.unwrapPrimitive(constantValue.getClass());
			if ((returnType != unwrapped) && !FilterHelpers.checkIfWideningPrimitiveConversion(unwrapped, returnType)) {
				throw new ClassCastException();
			}
		} else {
			returnType.cast(constantValue);
		}
		return ConstantHandle.get(returnType, constantValue);
	}
	
	/**
	 * Return a MethodHandle able to read from the array.  The MethodHandle's return type will be the same as 
	 * the elements of the array.  The MethodHandle will also accept two arguments - the first being the array, typed correctly, 
	 * and the second will be the the <code>int</code> index into the array.
	 * 
	 * @param arrayType - the type of the array
	 * @return a MethodHandle able to return values from the array
	 * @throws IllegalArgumentException - if arrayType is not actually an array
	 */
	public static MethodHandle arrayElementGetter(Class<?> arrayType) throws IllegalArgumentException {
		if (!arrayType.isArray()) {
			throw new IllegalArgumentException();
		}
		
		try {
			Class<?> componentType = arrayType.getComponentType();
			if (componentType.isPrimitive()) {
				// Directly lookup the appropriate helper method
				String name = componentType.getCanonicalName();
				MethodType type = MethodType.methodType(componentType, arrayType, int.class);
				return Lookup.internalPrivilegedLookup.findStatic(MethodHandles.class, name + "ArrayGetter", type); //$NON-NLS-1$
			}
			
			// Lookup the "Object[]" case and use some asTypes() to get the right MT and return type.
			MethodType type = MethodType.methodType(Object.class, Object[].class, int.class);
			MethodHandle mh = Lookup.internalPrivilegedLookup.findStatic(MethodHandles.class, "objectArrayGetter", type); //$NON-NLS-1$
			MethodType realType = MethodType.methodType(componentType, arrayType, int.class); 
			return mh.cloneWithNewType(realType);
		} catch(IllegalAccessException | NoSuchMethodException e) {
			throw new Error(e);
		}
	}
	
	/**
	 * Return a MethodHandle able to write to the array.  The MethodHandle will have a void return type and take three
	 * arguments: the first being the array, typed correctly, the second will be the the <code>int</code> index into the array,
	 * and the third will be the item to write into the array
	 * 
	 * @param arrayType - the type of the array
	 * @return a MehtodHandle able to write into the array
	 * @throws IllegalArgumentException - if arrayType is not actually an array
	 */
	public static MethodHandle arrayElementSetter(Class<?> arrayType) throws IllegalArgumentException {
		if (!arrayType.isArray()) {
			throw new IllegalArgumentException();
		}
		
		try {
			Class<?> componentType = arrayType.getComponentType();
			if (componentType.isPrimitive()) {
				// Directly lookup the appropriate helper method
				String name = componentType.getCanonicalName();
				MethodType type = MethodType.methodType(void.class, arrayType, int.class, componentType);
				return Lookup.internalPrivilegedLookup.findStatic(MethodHandles.class, name + "ArraySetter", type); //$NON-NLS-1$
			}
	
			// Lookup the "Object[]" case and use some asTypes() to get the right MT and return type.
			MethodType type = MethodType.methodType(void.class, Object[].class, int.class, Object.class);
			MethodHandle mh = Lookup.internalPrivilegedLookup.findStatic(MethodHandles.class, "objectArraySetter", type); //$NON-NLS-1$
			MethodType realType = MethodType.methodType(void.class, arrayType, int.class, componentType); 
			return mh.cloneWithNewType(realType);
		} catch(IllegalAccessException | NoSuchMethodException e) {
			throw new Error(e);
		}
	}
	
	/*[IF Sidecar19-SE]*/
	/**
	 * Factory method for creating a VarHandle for accessing elements of an array.
	 * 
	 * @param arrayClass The array type (not the component type)
	 * @return A VarHandle that can access elements of arrays of type {@code arrayClass}
	 * @throws NullPointerException If {@code arrayClass} is null
	 * @throws IllegalArgumentException If {@code arrayClass} is not an array type
	 */
	public static VarHandle arrayElementVarHandle(Class<?> arrayClass) throws IllegalArgumentException {
		checkArrayClass(arrayClass);
		return new ArrayVarHandle(arrayClass);
	}
	
	/**
	 * Factory method for creating a VarHandle for accessing elements of a byte array using a view type.
	 * 
	 * @param viewArrayClass The view type to convert byte elements to.
	 * @param byteOrder The byte order to use when converting elements from byte to the view type 
	 * @return A VarHandle that can access elements of a byte array through a view type
	 * @throws IllegalArgumentException If {@code viewArrayClass} is not an array type
	 * @throws NullPointerException If {@code viewArrayClass} or {@code byteOrder} is null
	 */
	public static VarHandle byteArrayViewVarHandle(Class<?> viewArrayClass, ByteOrder byteOrder) throws IllegalArgumentException {
		checkArrayClass(viewArrayClass);
		Objects.requireNonNull(byteOrder);
		return new ByteArrayViewVarHandle(viewArrayClass.getComponentType(), byteOrder);
	}
	
	/**
	 * Factory method for creating a VarHandle for accessing elements of a {@link java.nio.ByteBuffer ByteBuffer} using a view type.
	 * 
	 * @param viewArrayClass The view type to convert byte elements to.
	 * @param byteOrder The byte order to use when converting elements from byte to the view type 
	 * @return A VarHandle that can access elements of a {@link java.nio.ByteBuffer ByteBuffer} through a view type
	 * @throws IllegalArgumentException If {@code viewArrayClass} is not an array type
	 * @throws NullPointerException If {@code viewArrayClass} or {@code byteOrder} is null
	 */
	public static VarHandle byteBufferViewVarHandle(Class<?> viewArrayClass, ByteOrder byteOrder) throws IllegalArgumentException {
		checkArrayClass(viewArrayClass);
		Objects.requireNonNull(byteOrder);
		return new ByteBufferViewVarHandle(viewArrayClass.getComponentType(), byteOrder);
	}
	
	private static void checkArrayClass(Class<?> arrayClass) throws IllegalArgumentException {
		if (null == arrayClass) {
			/*[MSG "K0625", "{0} is not an array type.""]*/
			throw new NullPointerException(com.ibm.oti.util.Msg.getString("K0625", "null")); //$NON-NLS-1$ //$NON-NLS-2$
		}
		if (!arrayClass.isArray()) {
			/*[MSG "K0625", "{0} is not an array type.""]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0625", arrayClass.getCanonicalName())); //$NON-NLS-1$
		}
	}
	
	/**
	 * Create a {@link MethodHandle} that is able to invoke a particular {@link VarHandle.AccessMode} on a {@link VarHandle}.
	 * <p>
	 * When the {@link MethodHandle} is invoked, the arguments must match the {@link MethodType} provided when the {@link MethodHandle}
	 * was generated, plus the required preceding argument of type {@link VarHandle}.
	 * </p>
	 * @param accessMode - the {@link VarHandle.AccessMode} to invoke when the generated {@link MethodHandle} is invoked.
	 * @param expectedType - the type to expect when invoking the {@link VarHandle.AccessMode}.
	 * @return a MethodHandle that invokes a particular {@link VarHandle.AccessMode} on a {@link VarHandle}.
	 */
	public static MethodHandle varHandleExactInvoker(VarHandle.AccessMode accessMode, MethodType expectedType) {
		return new VarHandleInvokeExactHandle(accessMode, expectedType);
	}
	
	/**
	 * Create a {@link MethodHandle} that is able to invoke a particular {@link VarHandle.AccessMode} on a {@link VarHandle}.
	 * <p>
	 * When the {@link MethodHandle} is invoked, the arguments must be convertible to the {@link MethodType} provided when the {@link MethodHandle}
	 * was generated, plus the required preceding argument of type {@link VarHandle}.
	 * </p>
	 * @param accessMode - the {@link VarHandle.AccessMode} to invoke when the generated {@link MethodHandle} is invoked.
	 * @param expectedType - the type to expect when invoking the {@link VarHandle.AccessMode}.
	 * @return a MethodHandle that invokes a particular {@link VarHandle.AccessMode} on a {@link VarHandle}.
	 */
	public static MethodHandle varHandleInvoker(VarHandle.AccessMode accessMode, MethodType expectedType) {
		return new VarHandleInvokeGenericHandle(accessMode, expectedType);
	}
	/*[ENDIF]*/

	/**
	 * Return a MethodHandle that will throw the passed in Exception object.  The return type is largely
	 * irrelevant as the method never completes normally.  Any return type that is convenient can be
	 * used.
	 * 
	 * @param returnType - The return type for the method
	 * @param exception - the type of Throwable to accept as an argument
	 * @return a MethodHandle that throws the passed in exception object
	 */
	public static MethodHandle throwException(Class<?> returnType, Class<? extends Throwable> exception) {
		MethodType realType = MethodType.methodType(returnType, exception);	
		MethodHandle handle;
		
		try {
			if (returnType.isPrimitive() || returnType.equals(void.class)) {
				// Directly lookup the appropriate helper method
				MethodType type = MethodType.methodType(returnType, Throwable.class);
				String name = returnType.getCanonicalName();
				handle = Lookup.internalPrivilegedLookup.findStatic(MethodHandles.class, name + "ExceptionThrower", type); //$NON-NLS-1$
			} else {
				MethodType type = MethodType.methodType(Object.class, Throwable.class);
				handle = Lookup.internalPrivilegedLookup.findStatic(MethodHandles.class, "objectExceptionThrower", type); //$NON-NLS-1$	
			}
			return handle.cloneWithNewType(realType);
		} catch(IllegalAccessException | NoSuchMethodException e) {
			throw new Error(e);
		}
	}
	
	/**
	 * Return a MethodHandle that will adapt the return value of <i>handle</i> by running the <i>filter</i>
	 * on it and returning the result of the filter.
	 * <p>
	 * If <i>handle</i> has a void return, <i>filter</i> must not take any parameters.
	 * 
	 * @param handle - the MethodHandle that will have its return value adapted
	 * @param filter - the MethodHandle that will do the return adaption.
	 * @return a MethodHandle that will run the filter handle on the result of handle.
	 * @throws NullPointerException - if handle or filter is null
	 * @throws IllegalArgumentException - if the return type of <i>handle</i> differs from the type of the only argument to <i>filter</i>
	 */
	public static MethodHandle filterReturnValue(MethodHandle handle, MethodHandle filter) throws NullPointerException, IllegalArgumentException {
		MethodType filterType = filter.type;
		int filterArgCount = filterType.parameterCount();
		Class<?> handleReturnType = handle.type.returnType;
		
		if ((handleReturnType == void.class) && (filterArgCount == 0)) {
			// filter handle must not take any parameters as handle doesn't return anything
			return new FilterReturnHandle(handle, filter);
		}
		if ((filterArgCount == 1) && (filterType.parameterType(0) == handle.type.returnType)) {
			// filter handle must accept single parameter of handle's returnType
			return new FilterReturnHandle(handle, filter);
		}
		throw new IllegalArgumentException();
	}
	
	
	@SuppressWarnings("unused")
	private static void voidExceptionThrower(Throwable t) throws Throwable {
		throw t;
	}
	
	@SuppressWarnings("unused")
	private static int intExceptionThrower(Throwable t) throws Throwable {
		throw t;
	}
	
	@SuppressWarnings("unused")
	private static char charExceptionThrower(Throwable t) throws Throwable {
		throw t;
	}
	
	@SuppressWarnings("unused")
	private static byte byteExceptionThrower(Throwable t) throws Throwable {
		throw t;
	}
	
	@SuppressWarnings("unused")
	private static boolean booleanExceptionThrower(Throwable t) throws Throwable {
		throw t;
	}
	
	@SuppressWarnings("unused")
	private static short shortExceptionThrower(Throwable t) throws Throwable {
		throw t;
	}
	
	@SuppressWarnings("unused")
	private static long longExceptionThrower(Throwable t) throws Throwable {
		throw t;
	}
	
	@SuppressWarnings("unused")
	private static double doubleExceptionThrower(Throwable t) throws Throwable {
		throw t;
	}
	
	@SuppressWarnings("unused")
	private static float floatExceptionThrower(Throwable t) throws Throwable {
		throw t;
	}

	@SuppressWarnings("unused")
	private static Object objectExceptionThrower(Throwable t) throws Throwable {
		throw t;
	}

	@SuppressWarnings("unused")
	private static  int intArrayGetter(int[] array, int index) {
		return array[index];
	}
	
	@SuppressWarnings("unused")
	private static char charArrayGetter(char[] array, int index) {
		return array[index];
	}
	
	@SuppressWarnings("unused")
	private static short shortArrayGetter(short[] array, int index) {
		return array[index];
	}
	
	@SuppressWarnings("unused")
	private static byte byteArrayGetter(byte[] array, int index) {
		return array[index];
	}
	
	@SuppressWarnings("unused")
	private static long longArrayGetter(long[] array, int index) {
		return array[index];
	}
	
	@SuppressWarnings("unused")
	private static double doubleArrayGetter(double[] array, int index) {
		return array[index];
	}
	
	@SuppressWarnings("unused")
	private static float floatArrayGetter(float[] array, int index) {
		return array[index];
	}
	
	@SuppressWarnings("unused")
	private static boolean booleanArrayGetter(boolean[] array, int index) {
		return array[index];
	}
	
	@SuppressWarnings("unused")
	private static Object objectArrayGetter(Object[] array, int index) {
		return array[index];
	}
	
	@SuppressWarnings("unused")
	private static void intArraySetter(int[] array, int index, int value) {
		array[index] = value;
	}
	
	@SuppressWarnings("unused")
	private static void charArraySetter(char[] array, int index, char value) {
		array[index] = value;
	}
	
	@SuppressWarnings("unused")
	private static void shortArraySetter(short[] array, int index, short value) {
		array[index] = value;
	}
	
	@SuppressWarnings("unused")
	private static void byteArraySetter(byte[] array, int index, byte value) {
		array[index] = value;
	}
	
	@SuppressWarnings("unused")
	private static void longArraySetter(long[] array, int index, long value) {
		array[index] = value;
	}
	
	@SuppressWarnings("unused")
	private static void doubleArraySetter(double[] array, int index, double value) {
		array[index] = value;
	}
	
	@SuppressWarnings("unused")
	private static void floatArraySetter(float[] array, int index, float value) {
		array[index] = value;
	}
	
	@SuppressWarnings("unused")
	private static void booleanArraySetter(boolean[] array, int index, boolean value) {
		array[index] = value;
	}
	
	@SuppressWarnings("unused")
	private static void objectArraySetter(Object[] array, int index, Object value) {
		array[index] = value;
	}
	
	/**
	 * Produce a MethodHandle that adapts its arguments using the filter methodhandles before calling the underlying handle.
	 * <p>
	 * The type of the adapter is the type of the original handle with the filter argument types replacing their corresponding
	 * arguments.  Each of the adapters must return the correct type for their corresponding argument.
	 * <p>
	 * If the filters array is empty or contains only null filters, the original handle will be returned.
	 *  
	 * @param handle - the underlying methodhandle to call with the filtered arguments
	 * @param startPosition - the position to start applying the filters at
	 * @param filters - the array of adapter handles to apply to the arguments
	 * @return a MethodHandle that modifies the arguments by applying the filters before calling the underlying handle
	 * @throws NullPointerException - if handle or filters is null
	 * @throws IllegalArgumentException - if one of the filters is not applicable to the corresponding handle argument 
	 * 				or there are more filters then arguments when starting at startPosition
	 * 				or startPosition is invalid
	 * 				or if the resulting MethodHandle would take too many parameters
	 * 
	 */
	public static MethodHandle filterArguments(MethodHandle handle, int startPosition, MethodHandle... filters) throws NullPointerException, IllegalArgumentException {
		filters.getClass();	// implicit null check
		MethodType handleType = handle.type;	// implicit null check
		if ((startPosition < 0) || ((startPosition + filters.length) > handleType.parameterCount())) {
			throw new IllegalArgumentException();
		}
		if (filters.length == 0) {
			return handle;
		}
		// clone the filter array so it can't be modified after the filters have been validated
		filters = filters.clone();
		
		// clone the parameter array
		Class<?>[] newArgTypes = handleType.parameterArray();
		boolean containsNonNullFilters = false;
		for (int i = 0; i < filters.length; i++) {
			MethodHandle filter = filters[i];
			if (filter != null) {
				containsNonNullFilters = true;
				MethodType filterType = filter.type;
				if (newArgTypes[startPosition + i] != filterType.returnType) {
					throw new IllegalArgumentException();
				}
				if (filterType.parameterCount() != 1) {
					throw new IllegalArgumentException();
				}
				newArgTypes[startPosition + i] = filterType.arguments[0];
			}
		}
		if (!containsNonNullFilters) {
			return handle;
		}

		// Remove any leading null filters
		for (int i = 0; i < filters.length; i++) {
			MethodHandle filter = filters[i];
			if (filter != null) {
				if (i != 0) {
					filters = Arrays.copyOfRange(filters, i, filters.length);
				}
				break;
			}
			startPosition += 1;
		}
		
		MethodType newType = MethodType.methodType(handleType.returnType, newArgTypes);
		MethodHandle result = FilterArgumentsHandle.get(handle, startPosition, filters, newType);
		return result;
	}
	
	/**
	 * Produce a MethodHandle that preprocesses some of the arguments by calling the preprocessor handle.
	 * 
	 * If the preprocessor handle has a return type, it must be the same as the first argument type of the <i>handle</i>.
	 * If the preprocessor returns void, it does not contribute the first argument to the <i>handle</i>.
	 * In all cases, the preprocessor handle accepts a subset of the arguments for the handle.
	 * 
	 * @param handle - the handle to call after preprocessing
	 * @param preprocessor - a methodhandle that preprocesses some of the incoming arguments
	 * @return a MethodHandle that preprocesses some of the arguments to the handle before calling the next handle, possibly with an additional first argument
	 * @throws NullPointerException - if any of the arguments are null
	 * @throws IllegalArgumentException - if the preprocessor's return type is not void and it differs from the first argument type of the handle,
	 * 			or if the arguments taken by the preprocessor isn't a subset of the arguments to the handle 
	 */
	public static MethodHandle foldArguments(MethodHandle handle, MethodHandle preprocessor) throws NullPointerException, IllegalArgumentException {
		return foldArgumentsCommon(handle, 0, preprocessor, EMPTY_ARG_POSITIONS);
	}
	
	/*[IF Sidecar19-SE]*/
	/**
	 * Produce a MethodHandle that preprocesses some of the arguments by calling the preprocessor handle.
	 * 
	 * If the preprocessor handle has a return type, it must be the same as the first argument type of the <i>handle</i>.
	 * If the preprocessor returns void, it does not contribute the first argument to the <i>handle</i>.
	 * In all cases, the preprocessor handle accepts a subset of the arguments for the handle.
	 * 
	 * @param handle - the handle to call after preprocessing
	 * @param handle - the starting position to fold arguments
	 * @param preprocessor - a methodhandle that preprocesses some of the incoming arguments
	 * @return a MethodHandle that preprocesses some of the arguments to the handle before calling the next handle, possibly with an additional first argument
	 * @throws NullPointerException - if any of the arguments are null
	 * @throws IllegalArgumentException - if the preprocessor's return type is not void and it differs from the first argument type of the handle,
	 * 			or if the arguments taken by the preprocessor isn't a subset of the arguments to the handle 
	 */
	public static MethodHandle foldArguments(MethodHandle handle, int foldPosition, MethodHandle preprocessor) throws NullPointerException, IllegalArgumentException {
		return foldArgumentsCommon(handle, foldPosition, preprocessor, EMPTY_ARG_POSITIONS);
	}
	
	/**
	 * Produce a MethodHandle that preprocesses some of the arguments by calling the preprocessor handle.
	 * 
	 * If the preprocessor handle has a return type, it must be the same as the first argument type of the <i>handle</i>.
	 * If the preprocessor returns void, it does not contribute the first argument to the <i>handle</i>.
	 * In all cases, the preprocessor handle accepts a subset of the arguments for the handle.
	 * 
	 * @param handle - the handle to call after preprocessing
	 * @param foldPosition - the starting position to fold arguments
	 * @param preprocessor - a methodhandle that preprocesses some of the incoming arguments
	 * @param argumentIndices - an array of indices of incoming arguments from the handle
	 * @return a MethodHandle that preprocesses some of the arguments to the handle before calling the next handle, possibly with an additional first argument
	 * @throws NullPointerException - if any of the arguments are null
	 * @throws IllegalArgumentException - if the preprocessor's return type is not void and it differs from the first argument type of the handle,
	 * 			or if the arguments taken by the preprocessor isn't a subset of the arguments to the handle
	 * 			or if the element of argumentIndices is outside of the range of the handle's argument list
	 * 			or if the arguments specified by argumentIndices from the handle doesn't exactly match the the arguments taken by the preprocessor
	 */
	static MethodHandle foldArguments(MethodHandle handle, int foldPosition, MethodHandle preprocessor, int... argumentIndices) throws NullPointerException, IllegalArgumentException {
		int[] passedInargumentIndices = EMPTY_ARG_POSITIONS;
		if (0 != argumentIndices.length) {
			passedInargumentIndices = argumentIndices.clone();
		}
		return foldArgumentsCommon(handle, foldPosition, preprocessor, passedInargumentIndices);
	}
	/*[ENDIF]*/
	
	/* The common code shared by foldArguments with and without the fold position specified */
	private static final MethodHandle foldArgumentsCommon(MethodHandle handle, int foldPosition, MethodHandle preprocessor, int... argumentIndices) throws NullPointerException, IllegalArgumentException {
		MethodType handleType = handle.type; // implicit nullcheck
		MethodType preprocessorType = preprocessor.type; // implicit nullcheck
		Class<?> preprocessorReturnClass = preprocessorType.returnType;
		final int handleTypeParamCount = handleType.parameterCount();
		final int preprocessorTypeParamCount = preprocessorType.parameterCount();
		final int argIndexCount = argumentIndices.length;
		int[] passedInArgumentIndices = EMPTY_ARG_POSITIONS;
		
		if ((foldPosition < 0) || (foldPosition >= handleTypeParamCount)) {
			/*[MSG "K0637", "The value of {0}: {1} must be in a range from 0 to {2}"]*/
			throw new IllegalArgumentException(Msg.getString("K0637", new Object[] { //$NON-NLS-1$
							"the fold position", Integer.toString(foldPosition), //$NON-NLS-1$
							Integer.toString(handleTypeParamCount)}));
		}
		
		if ((argIndexCount > 0) && (preprocessorTypeParamCount != argIndexCount)) {
			/*[MSG "K0638", "The count of argument indices: {0} must be equal to the parameter count of the combiner: {1}"]*/
			throw new IllegalArgumentException(Msg.getString("K0638", new Object[] { //$NON-NLS-1$
							Integer.toString(argIndexCount),
							Integer.toString(preprocessorTypeParamCount)}));
		}
		
		/* We need to check one case that the the argument indices of the array are entirely equal
		 * to the argument indices starting from the fold position, which means it can be 
		 * treated as the same case as an empty array.
		 * The reason for doing this is to ensure it can share the same thunk in JIT during compilation
		 * and have no impact on VM in terms of keeping the existing code in use to address this case.
		 * e.g.
		 * target args: {0, 1, 2, 3}
		 * combiner args: {1, 2}
		 * MH1=foldArguments(target, 1, combiner, {2, 3})
		 * MH2=foldArguments(target, 1, combiner)
		 * In such case, MH1 and MH2 should be considered as the same case to be addressed in code.
		 */
		for (int i = 0; i < argIndexCount; i++) {
			if ((argumentIndices[i] < 0) || (argumentIndices[i] >= handleTypeParamCount)) {
				/*[MSG "K0637", "The value of {0}: {1} must be in a range from 0 to {2}"]*/
				throw new IllegalArgumentException(Msg.getString("K0637", new Object[] { //$NON-NLS-1$
								"argument index", Integer.toString(argumentIndices[i]),  //$NON-NLS-1$
								Integer.toString(handleTypeParamCount)}));
			}
			if ((foldPosition + i + 1) != argumentIndices[i]) {
				passedInArgumentIndices = argumentIndices;
				break;
			}
		}
		
		if (void.class == preprocessorReturnClass) {
			// special case: a preprocessor handle that returns void doesn't provide an argument to the underlying handle
			if (handleTypeParamCount < preprocessorTypeParamCount) {
				/*[MSG "K0650", "The parameter count of the combiner: {0} must be {1} the parameter count of the fold handle: {2}"]*/
				throw new IllegalArgumentException(Msg.getString("K0650", new Object[] { //$NON-NLS-1$
								Integer.toString(preprocessorTypeParamCount), 
								"<=", Integer.toString(handleTypeParamCount)})); //$NON-NLS-1$
			}
			validateParametersOfCombiner(argIndexCount, preprocessorTypeParamCount, preprocessorType, handleType, argumentIndices, foldPosition, 0);
			MethodHandle result = FoldHandle.get(handle, preprocessor, handleType, foldPosition, passedInArgumentIndices);
			return result;
		}
		
		if (handleTypeParamCount <= preprocessorTypeParamCount) {
			/*[MSG "K0650", "The parameter count of the combiner: {0} must be {1} the parameter count of the fold handle: {2}"]*/
			throw new IllegalArgumentException(Msg.getString("K0650", new Object[] { //$NON-NLS-1$
							Integer.toString(preprocessorTypeParamCount), 
							"<", Integer.toString(handleTypeParamCount)})); //$NON-NLS-1$
		}
		if (preprocessorReturnClass != handleType.arguments[foldPosition]) {
			/*[MSG "K063A", "The return type of combiner: {0} is inconsistent with the argument type of fold handle: {1} at the fold position: {2}"]*/
			throw new IllegalArgumentException(Msg.getString("K063A", new Object[] { //$NON-NLS-1$
							preprocessorReturnClass.getSimpleName(),  
							handleType.arguments[foldPosition].getSimpleName(), 
							Integer.toString(foldPosition)}));
		}
		validateParametersOfCombiner(argIndexCount, preprocessorTypeParamCount, preprocessorType, handleType, argumentIndices, foldPosition, 1);
		MethodType newType = handleType.dropParameterTypes(foldPosition, foldPosition + 1);
		MethodHandle result = FoldHandle.get(handle, preprocessor, newType, foldPosition, passedInArgumentIndices);
		return result;
	}
	
	private static void validateParametersOfCombiner(int argIndexCount, int preprocessorTypeParamCount, MethodType preprocessorType, MethodType handleType, int[] argumentIndices, int foldPosition, int foldPlaceHolder) {
		if (0 == argIndexCount) {
			for (int i = 0; i < preprocessorTypeParamCount; i++) {
				if (preprocessorType.arguments[i]  != handleType.arguments[foldPosition + i + foldPlaceHolder]) {
					/*[MSG "K05d0", "Can't apply fold of type: {0} to handle of type: {1} starting at {2} "]*/
					throw new IllegalArgumentException(Msg.getString("K05d0", preprocessorType.toString(), handleType.toString(), Integer.toString(foldPosition))); //$NON-NLS-1$
				}
			}
		} else {
			for (int i = 0; i < argIndexCount; i++) {
				if (preprocessorType.arguments[i]  != handleType.arguments[argumentIndices[i]]) {
					/*[MSG "K05d0", "Can't apply fold of type: {0} to handle of type: {1} starting at {2} "]*/
					throw new IllegalArgumentException(Msg.getString("K05d0", preprocessorType.toString(), handleType.toString(), Integer.toString(foldPosition))); //$NON-NLS-1$
				}
			}
		}
	}

	/**
	 * Produce a MethodHandle that will permute the incoming arguments according to the 
	 * permute array.  The new handle will have a type of permuteType.  
	 * <p>
	 * The permutations can include duplicating or rearranging the arguments.  The permute
	 * array must have the same number of items as there are parameters in the 
	 * handle's type.
	 * <p>
	 * Each argument type must exactly match - no conversions are applied.
	 * 
	 * @param handle - the original handle to call after permuting the arguments
	 * @param permuteType - the new type of the adapter handle
	 * @param permute - the reordering from the permuteType to the handle type
	 * @return a MethodHandle that rearranges the arguments before calling the original handle
	 * @throws NullPointerException - if any of the arguments are null
	 * @throws IllegalArgumentException - if permute array is not the same length as handle.type().parameterCount() or
	 * 			if handle.type() and permuteType have different return types, or
	 * 			if the permute arguments don't match the handle.type()
	 */
	public static MethodHandle permuteArguments(MethodHandle handle, MethodType permuteType, int... permute) throws NullPointerException, IllegalArgumentException {
		// TODO: If permute is the identity permute, return this
		MethodType handleType = handle.type;	// implicit null check
		Class<?> permuteReturnType = permuteType.returnType; // implicit null check
		if (permute.length != handleType.parameterCount()) { // implicit null check on permute
			throw new IllegalArgumentException();
		}
		if (permuteReturnType != handleType.returnType) {
			throw new IllegalArgumentException();
		}
		permute = permute.clone();	// ensure the permute[] can't be modified during/after validation
		// validate permuted args are an exact match
		validatePermutationArray(permuteType, handleType, permute);
		return handle.permuteArguments(permuteType, permute);
	}
	
	/**
	 * Produce a MethodHandle that preprocesses some of the arguments by calling the filter handle.
	 * 
	 * If the <i>filter</i> handle has a return type, it must be the same as the argument type at pos in the <i>target</i> arguments.
	 * If the <i>filter</i> returns void, it does not contribute an argument to the <i>target</i> arguments at pos.
	 * The <i>filter</i> handle consumes a subset (size equal to the <i>filter's</i> arity) of the returned handle's arguments, starting at pos.
	 * 
	 * @param target - the target to call after preprocessing the arguments
	 * @param pos - argument index in handle arguments where the filter will collect its arguments and/or insert its return value as an argument to the target
	 * @param filter - a MethodHandle that preprocesses some of the incoming arguments
	 * @return a MethodHandle that preprocesses some of the arguments to the handle before calling the target with the new arguments
	 * @throws NullPointerException - if either target or filter are null
	 * @throws IllegalArgumentException - if the preprocessor's return type is not void and it differs from the target argument type at pos,
	 * 			if pos is not between 0 and the target's arity (exclusive for non-void filter, inclusive for void filter), or if the generated handle would
	 * 			have too many parameters 
	 */
	public static MethodHandle collectArguments(MethodHandle target, int pos, MethodHandle filter) throws NullPointerException, IllegalArgumentException {
		MethodType targetType = target.type; // implicit nullcheck
		MethodType filterType = filter.type; // implicit nullcheck
		Class<?> filterReturnClass = filterType.returnType;
		
		if (filterReturnClass == void.class) {
			// special case: a filter handle that returns void doesn't provide an argument to the target handle
			if ((pos < 0) || (pos > targetType.argSlots)) {
				/*[MSG "K0580", "Filter argument index (pos) is not between 0 and target arity (\"{0}\")"]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0580", targetType.argSlots)); //$NON-NLS-1$
			}
			MethodType resultType = targetType.insertParameterTypes(pos, filterType.arguments);
			MethodHandle result = buildTransformHandle(new VoidCollectHelper(target, pos, filter), resultType);
			return result;
		}
		
		if ((pos < 0) || (pos >= targetType.argSlots)) {
			/*[MSG "K0580", "Filter argument index (pos) is not between 0 and target arity (\"{0}\")"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0580", targetType.argSlots)); //$NON-NLS-1$
		}
		
		if (filterReturnClass != targetType.arguments[pos]) {
			/*[MSG "K0581", "Filter return type does not match target argument at position \"{0}\""]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0581", pos)); //$NON-NLS-1$
		}
		MethodType resultType = targetType.dropParameterTypes(pos, pos + 1).insertParameterTypes(pos, filterType.arguments);
		MethodHandle result = buildTransformHandle(new CollectHelper(target, pos, filter), resultType);
		return result;
	}
	
	/* 
	 * Helper method to validate that the permute[] specifies a valid permutation from permuteType
	 * to handleType.
	 * 
	 * This method throws IllegalArgumentException on failure and returns true on success.  This 
	 * disjointness allows it to be used in asserts.
	 */
	private static boolean validatePermutationArray(MethodType permuteType, MethodType handleType, int[] permute) throws IllegalArgumentException {
		/*[IF ]*/
		/////////////// TODO: would it be faster to create the new MethodType using the handle.type & permute array and then use newType == permuteType?
		/*[ENDIF]*/
		Class<?>[] permuteArgs = permuteType.arguments;
		Class<?>[] handleArgs = handleType.arguments;
		for (int i = 0; i < permute.length; i++) {
			int permuteIndex = permute[i];
			if ((permuteIndex < 0) || (permuteIndex >= permuteArgs.length)){
				throw new IllegalArgumentException();
			}
			if (handleArgs[i] != permuteArgs[permuteIndex]) {
				throw new IllegalArgumentException();
			}
		}
		return true;
	}
	
	
	/* 
	 * Helper method to dropArguments(). This method must be called with cloned array Class<?>... valueType. 
	 */
	private static MethodHandle dropArgumentsUnsafe(MethodHandle originalHandle, int location, Class<?>... valueTypes) {
		int valueTypeLength = valueTypes.length; // implicit null check
		MethodType originalType = originalHandle.type;	// implicit null check
		if ((location < 0) || (location > originalType.parameterCount())) {
			/*[MSG "K039c", "Invalid parameters"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K039c")); //$NON-NLS-1$
		}
		
		MethodType permuteType = originalType.insertParameterTypes(location, valueTypes);
		/* Build equivalent permute array */
		int[] permute = new int[originalType.parameterCount()];
		int originalIndex = 0;
		for (int i = 0; i < permute.length; i++) {
			if (originalIndex == location) {
				originalIndex += valueTypeLength;
			}
			permute[i] = originalIndex++;
		}
		assert(validatePermutationArray(permuteType, originalType, permute));
		return originalHandle.permuteArguments(permuteType, permute);
	}
	
	
	/**
	 * This method returns a method handle that delegates to the original method handle,
	 * ignoring a particular range of arguments (starting at a given location and
	 * with given types).  The type of the returned method handle is the type of the original handle
	 * with the given types inserted in the parameter type list at the given location.  
	 * 
	 * @param originalHandle - the original method handle to be transformed
	 * @param location -  the location of the first argument to be removed
	 * @param valueTypes - an array of the argument types to be removed 
	 * @return a MethodHandle - representing a transformed handle as described above
	 */
	public static MethodHandle dropArguments(MethodHandle originalHandle, int location, Class<?>... valueTypes) {
		valueTypes = valueTypes.clone();
		return dropArgumentsUnsafe(originalHandle, location, valueTypes);
	}
	
	
	/**
	 * This method returns a method handle that delegates to the original method handle,
	 * ignoring a particular range of arguments (starting at a given location and
	 * with given types).  The type of the returned method handle is the type of the original handle
	 * with the given types inserted in the parameter type list at the given location.  
	 * 
	 * @param originalHandle - the original method handle to be transformed
	 * @param location -  the location of the first argument to be removed
	 * @param valueTypes - a List of the argument types to be removed 
	 * @return a MethodHandle - representing a transformed handle as described above
	 */
	public static MethodHandle dropArguments(MethodHandle originalHandle, int location, List<Class<?>> valueTypes) {
		valueTypes.getClass();	// implicit null check
		Class<?>[] valueTypesCopy = new Class<?>[valueTypes.size()];
		for(int i = 0; i < valueTypes.size(); i++) {
			valueTypesCopy[i] = valueTypes.get(i);
		}
		return dropArgumentsUnsafe(originalHandle, location, valueTypesCopy);
	}
	
	/*[IF Sidecar19-SE]*/
	/**
	 * This method returns a method handle that delegates to the original method handle,
	 * skipping over a specified number of arguments at the given location. The type of the 
	 * returned method handle is the type of the original handle with the given types
	 * inserted in the parameter type list at the location after the skipped arguments.  
	 * 
	 * @param originalHandle the original method handle to be transformed
	 * @param skippedArgumentCount the number of argument to be skipped from the original method handle
	 * @param valueTypes a List of the argument types to be inserted
	 * @param location the (zero-indexed) location of the first argument to be removed
	 * @return a MethodHandle representing a transformed handle as described above
	 */
	public static MethodHandle dropArgumentsToMatch(MethodHandle originalHandle, int skippedArgumentCount, List<Class<?>> valueTypes, int location) {
		/* implicit null checks */
		MethodType originalType = originalHandle.type;
		Class<?>[] valueTypesCopy = valueTypes.toArray(new Class<?>[valueTypes.size()]);
		Class<?>[] ptypes = originalType.parameterArray();

		int valueTypesSize = valueTypesCopy.length;
		int originalParameterCount = ptypes.length;

		/* check if indexing is in range */
		if ((skippedArgumentCount < 0) ||
			(skippedArgumentCount > originalParameterCount)) {
			/*[MSG "K0670", "Variable skippedArgumentCount out of range"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0670")); //$NON-NLS-1$
		}
		if ((location < 0) ||
			(valueTypesSize < location + (originalParameterCount - skippedArgumentCount))) {
			/*[MSG "K0671", "Index location out of range"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0671")); //$NON-NLS-1$
		}

		/* check for void.class in list during clone process */
		for (int i = 0; i < valueTypesSize; i++) {
			if (valueTypesCopy[i] == void.class) {
				/*[MSG "K0672", "Invalid entry void.class found in argument list valueTypes"]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0672")); //$NON-NLS-1$
			} else if (valueTypesCopy[i] == null) {
				/*[MSG "K0673", "Invalid entry null found in argument list valueTypes"]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0673")); //$NON-NLS-1$
			}
		}

		/* check if sublist match */
		for (int i = skippedArgumentCount; i < ptypes.length; i++) {
			if (ptypes[i] != valueTypesCopy[i + location - skippedArgumentCount]) {
				/*[MSG "K0674", "Original handle types does not match given types at location"]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0674")); //$NON-NLS-1$
			}
		}

		/* replace the sublist with new valueTypes */
		MethodType permuteType = originalType.dropParameterTypes(skippedArgumentCount, originalParameterCount).appendParameterTypes(valueTypesCopy);

		/* construct permutation indexes */
		int[] permute = new int[originalParameterCount];
		int originalIndex = 0;
		for (int i = 0; i < originalParameterCount; i++) {
			if (originalIndex == skippedArgumentCount) {
				originalIndex += location;
			}
			permute[i] = originalIndex++;
		}

		assert(validatePermutationArray(permuteType, originalType, permute));
		return originalHandle.permuteArguments(permuteType, permute);
	}
	/*[ENDIF]*/
	
	/* A helper method to invoke argument transformation helpers */
	private static MethodHandle buildTransformHandle(ArgumentHelper helper, MethodType mtype){
		MethodType helperType = MethodType.methodType(Object.class, Object[].class);
		try {
			return Lookup.internalPrivilegedLookup.bind(helper, "helper", helperType).asCollector(Object[].class, mtype.parameterCount()).asType(mtype); //$NON-NLS-1$
		} catch(IllegalAccessException | NoSuchMethodException e) {
			throw new Error(e);
		}
	}
	
	/**
	 * Produce an adapter that converts the incoming arguments from <i>type</i> to the underlying MethodHandle's type 
	 * and converts the return value as required.
	 * <p>
	 * The following conversions, beyond those allowed by {@link MethodHandle#asType(MethodType)} are also allowed:
	 * <ul>
	 * <li>A conversion to an interface is done without a cast</li>
	 * <li>A boolean is treated as a single bit unsigned integer and may be converted to other primitive types</li>
	 * <li>A primitive can also be cast using Java casting conversion if asType would have allowed Java method invocation conversion</li>
	 * <li>An unboxing conversion, possibly followed by a widening primitive conversion</li>
	 * </ul>
	 * These additional rules match Java casting conversion and those of the bytecode verifier.
	 * 
	 * @param handle - the MethodHandle to invoke after converting the arguments to its type
	 * @param type - the type to convert from
	 * @return a MethodHandle which does the required argument and return conversions, if possible
	 * @throws NullPointerException - if either of the arguments are null
	 * @throws WrongMethodTypeException - if an illegal conversion is requested
	 */
	public static MethodHandle explicitCastArguments(MethodHandle handle, MethodType type) throws NullPointerException, WrongMethodTypeException {
		MethodType handleType = handle.type;	// implicit null check
		Class<?> newReturnType = type.returnType; // implicit null check
		
		if (handleType == type) {
			return handle;
		}
		MethodHandle mh = handle;
		if (handleType.returnType != newReturnType) {
			MethodHandle filter = FilterHelpers.getReturnFilter(handleType.returnType, newReturnType, true);
			mh = new FilterReturnHandle(handle, filter);
			/* Exit early if only return types differ */
			if (mh.type == type) {
				return mh;
			}
		}
		return new ExplicitCastHandle(mh, type);
	}

	/**
	 * This method returns a method handle that delegates to the original method handle,
	 * adding a particular range of arguments (starting at a given location and
	 * with given types).  The type of the returned method handle is the type of the original handle
	 * with the given types dropped from the parameter type list at the given location.  
	 * 
	 * @param originalHandle - the original method handle to be transformed
	 * @param location -  the location of the first argument to be inserted
	 * @param values - an array of the argument types to be inserted 
	 * @return a MethodHandle - representing a transformed handle as described above
	 */
	public static MethodHandle insertArguments(MethodHandle originalHandle, int location, Object... values) {
		MethodType originalType = originalHandle.type; // expected NPE if originalHandle is null
		Class<?>[] arguments = originalType.parameterArray();

		boolean noValuesToInsert = values.length == 0;  // expected NPE.  Must be null checked before location is checked.

		if ((location < 0) || (location >= originalType.parameterCount())) {
			throw new IllegalArgumentException();
		}

		if (noValuesToInsert) {
			// No values to insert
			return originalHandle;
		}
		
		// clone the values[] so it can't be modified after validation occurs
		values = values.clone();  

		/* This loop does two things:
		 * 1) Validates that the 'values' can be legitimately inserted in originalHandle
		 * 2) Builds up the parameter array for the asType operation
		 */
		for (int i = 0; i < values.length; i++) { // expected NPE if values is null
			Class<?> clazz = arguments[location + i];
			Object value = values[i];
			Class<?> valueClazz = clazz;
			if (value != null) {
				valueClazz = value.getClass();
			}
			if (clazz.isPrimitive()) {
				Objects.requireNonNull(value);
				Class<?> unwrapped = MethodType.unwrapPrimitive(valueClazz);
				if ((clazz != unwrapped) && !FilterHelpers.checkIfWideningPrimitiveConversion(unwrapped, clazz)) {
					clazz.cast(value);	// guaranteed to throw ClassCastException
				}
			} else {
				clazz.cast(value);
			}
			// overwrite the original argument with the new class from the values[]
			arguments[location + i] = valueClazz;
		}
		MethodHandle asTypedOriginalHandle = originalHandle.asType(MethodType.methodType(originalType.returnType, arguments)); 

		MethodType mtype = originalType.dropParameterTypes(location, location + values.length);
		MethodHandle insertHandle;
		switch(values.length) {
		case 1: {
			if (originalType.parameterType(location) == int.class) {
				insertHandle = new Insert1IntHandle(mtype, asTypedOriginalHandle, location, values, originalHandle);
			} else {
				insertHandle = new Insert1Handle(mtype, asTypedOriginalHandle, location, values);
			}
			break;
		}
		case 2: 
			insertHandle = new Insert2Handle(mtype, asTypedOriginalHandle, location, values);
			break;
		case 3: 
			insertHandle = new Insert3Handle(mtype, asTypedOriginalHandle, location, values);
			break;
		default:
			insertHandle = new InsertHandle(mtype, asTypedOriginalHandle, location, values);
		}

		// Now give originalHandle a chance to supply a handle with a better thunk archetype
		return originalHandle.insertArguments(insertHandle, asTypedOriginalHandle, location, values);
	}
	
	/*[IF Sidecar19-SE]*/
	/**
	 * Return a MethodHandle that produces an array of the requested type with the passed-in length.  
	 * The MethodHandle will have a MethodType of '(I)arrayType'.
	 * 
	 * @param arrayType The array class
	 * @return a MethodHandle that produces an array of the requested type
	 * @throws NullPointerException if arrayType is null
	 * @throws IllegalArgumentException if arrayType is not an array
	 * @since 9
	 */
	public static MethodHandle arrayConstructor(Class<?> arrayType) throws NullPointerException, IllegalArgumentException {
		checkArrayClass(arrayType);
		
		MethodHandle arrayConstructorHandle = null;
		Class<?> componentType = arrayType.getComponentType();
		MethodType realArrayType = MethodType.methodType(arrayType, int.class);
		
		try {
			/* Directly look up the appropriate helper method for the given primitive type */
			if (componentType.isPrimitive()) {
				String typeName = componentType.getCanonicalName();
				arrayConstructorHandle = Lookup.internalPrivilegedLookup.findStatic(MethodHandles.class, typeName + "ArrayConstructor", realArrayType); //$NON-NLS-1$
			} else {
				/* Look up the "Object[]" case and convert the corresponding handle to the one with the correct MethodType */
				MethodType objectArrayType = MethodType.methodType(Object[].class, int.class);
				arrayConstructorHandle = Lookup.internalPrivilegedLookup.findStatic(MethodHandles.class, "objectArrayConstructor", objectArrayType); //$NON-NLS-1$
				if (Object[].class != arrayType) {
					arrayConstructorHandle = arrayConstructorHandle.cloneWithNewType(realArrayType);
				}
			}
		} catch(IllegalAccessException | NoSuchMethodException e) {
			throw new InternalError("The method retrieved by lookup doesn't exit or it fails in the access checking", e); //$NON-NLS-1$
		}
		
		return arrayConstructorHandle;
	}
	
	private static boolean[] booleanArrayConstructor(int arraySize) {
		return new boolean[arraySize];
	}
	
	private static char[] charArrayConstructor(int arraySize) {
		return new char[arraySize];
	}
	
	private static byte[] byteArrayConstructor(int arraySize) {
		return new byte[arraySize];
	}
	
	private static short[] shortArrayConstructor(int arraySize) {
		return new short[arraySize];
	}
	
	private static int[] intArrayConstructor(int arraySize) {
		return new int[arraySize];
	}
	
	private static long[] longArrayConstructor(int arraySize) {
		return new long[arraySize];
	}
	
	private static float[] floatArrayConstructor(int arraySize) {
		return new float[arraySize];
	}
	
	private static double[] doubleArrayConstructor(int arraySize) {
		return new double[arraySize];
	}
	
	private static Object[] objectArrayConstructor(int arraySize) {
		return new Object[arraySize];
	}
	
	/**
	 * Return a MethodHandle that fetches the length from the passed-in array.  
	 * The MethodHandle will have a MethodType of '(arrayType)I'.
	 * 
	 * @param arrayType The array class
	 * @return a MethodHandle that returns the length of the passed-in array
	 * @throws NullPointerException if arrayType is null
	 * @throws IllegalArgumentException if arrayType is not an array
	 */
	public static MethodHandle arrayLength(Class<?> arrayType) throws NullPointerException, IllegalArgumentException {
		checkArrayClass(arrayType);
		
		MethodHandle arrayLengthHandle = null;
		Class<?> componentType = arrayType.getComponentType();
		MethodType realArrayType = MethodType.methodType(int.class, arrayType);
		
		try {
			/* Directly look up the appropriate helper method for the given primitive type */
			if (componentType.isPrimitive()) {
				arrayLengthHandle = Lookup.internalPrivilegedLookup.findStatic(MethodHandles.class, "arrayLength", realArrayType); //$NON-NLS-1$
			} else {
				/* Look up the "Object[]" case and convert the corresponding handle to the one with the correct MethodType */
				MethodType objectArrayType = MethodType.methodType(int.class, Object[].class);
				arrayLengthHandle = Lookup.internalPrivilegedLookup.findStatic(MethodHandles.class, "arrayLength", objectArrayType); //$NON-NLS-1$
				if (Object[].class != arrayType) {
					arrayLengthHandle = arrayLengthHandle.cloneWithNewType(realArrayType);
				}
			}
		} catch(IllegalAccessException | NoSuchMethodException e) {
			throw new InternalError("The method retrieved by lookup doesn't exit or it fails in the access checking", e); //$NON-NLS-1$
		}
		
		return arrayLengthHandle;
	}
	
	private static int arrayLength(boolean[] array) {
		return array.length;
	}
	
	private static int arrayLength(char[] array) {
		return array.length;
	}
	
	private static int arrayLength(byte[] array) {
		return array.length;
	}
	
	private static int arrayLength(short[] array) {
		return array.length;
	}
	
	private static int arrayLength(int[] array) {
		return array.length;
	}
	
	private static int arrayLength(long[] array) {
		return array.length;
	}
	
	private static int arrayLength(float[] array) {
		return array.length;
	}
	
	private static int arrayLength(double[] array) {
		return array.length;
	}
	
	private static int arrayLength(Object[] array) {
		return array.length;
	}
	
	/**
	 * Produces a constant method handle that ignores arguments and returns the default value
	 * for the return type of the requested MethodType.
	 * 
	 * @param targetMethodType the requested MethodType
	 * @return a MethodHandle returning the default value for the return type of the requested MethodType
	 * @throws NullPointerException - if the requested MethodType is null
	 */
	public static MethodHandle empty(MethodType targetMethodType) throws NullPointerException {
		MethodHandle constantHandle = zero(targetMethodType.returnType);
		return dropArgumentsUnsafe(constantHandle, 0, targetMethodType.arguments);
	}
	
	/**
	 * Produces a constant method handle with the default value for the requested target type.
	 * 
	 * @param targetType the requested target type
	 * @return a MethodHandle without arguments that returns the default value of the requested target type
	 * @throws NullPointerException - if the requested target type is null
	 */
	public static MethodHandle zero(Class<?> targetType) throws NullPointerException {
		/* return the method handle with the void return type */
		if (void.class == targetType) {
			try {
				return Lookup.internalPrivilegedLookup.findStatic(MethodHandles.class, "nop", MethodType.methodType(void.class));
			} catch (IllegalAccessException | NoSuchMethodException e) {
				throw new InternalError("The 'nop' method doesn't exit or it fails in the access checking", e); //$NON-NLS-1$
			}
		}
		
		Object defaultValue = null;
		if (targetType.isPrimitive()) {
			if (boolean.class == targetType) {
				defaultValue = Boolean.valueOf(false);
			} else if (char.class == targetType) {
				defaultValue = Character.valueOf((char) 0);
			} else  {
				/* rely on widening conversion from byte to everything */
				defaultValue = Byte.valueOf((byte) 0);
			}
		}
		return constant(targetType, defaultValue);
	}
	
	/* internally used by zero() with the void type */
	private static void nop() {
	}

	/**
	 * Produce a loop handle that wraps the logic of loop with an array of MethodHandle clauses
	 * 
	 * @param handleClauses an array of MethodHandle clauses and each clause consists of initializer, step, predicate and finalizer.
	 * @return a MethodHandle that represents the loop operation
	 * @throws IllegalArgumentException - if passed-in arguments are invalid or any constraint for clause is violated
	 */
	public static MethodHandle loop(MethodHandle[]... handleClauses) throws IllegalArgumentException {
		if (null == handleClauses) {
			/*[MSG "K0651", "The clause array must be non-null: {0}"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0651", Arrays.toString(handleClauses))); //$NON-NLS-1$
		}
		
		int clauseCount = handleClauses.length;
		if (0 == clauseCount) {
			/*[MSG "K0652", "No clause exists in the clause array: {0}"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0652", Arrays.toString(handleClauses))); //$NON-NLS-1$
		}
		
		LoopHandleBuilder builder = new LoopHandleBuilder(clauseCount);
		
		/* Create an internal clause array with all passed-in clauses for later manipulation */
		builder.createInternalClauses(handleClauses);
		
		/* Create the resulting loop handle with the internal clause array */
		MethodHandle result = builder.buildLoopHandle();
		return result;
	}
	
	/**
	 * Produce a loop handle that wraps an initializer, a loop body and a predicate to execute a while loop.
	 * Loop variables are updated by the return values of the corresponding step handles (including the loop body)
	 * in each iteration.
	 * 
	 * @param initHandle a MethodHandle that represents the initial value of loop variable
	 * @param predHandle a MethodHandle that represents the loop condition
	 * @param bodyHandle a MethodHandle that wraps the loop body to update the loop variable with its return value during iteration
	 * @return a MethodHandle that represents the loop operation
	 * @throws NullPointerException - if the body handle or the predicate handle are null
	 * @throws IllegalArgumentException - if passed-in arguments are invalid or any constraint for while loop is violated
	 */
	public static MethodHandle whileLoop(MethodHandle initHandle, MethodHandle predHandle, MethodHandle bodyHandle) throws NullPointerException, IllegalArgumentException {
		
		/* Check the validity of the predicate (must be non-null) and body handle. Also
		 * verify that handles are effectively identical and constrained by body. Note:
		 * The constraints of initializer and predicate at other aspects will be checked
		 * later in the generic loop when adding clauses.
		 */
		MethodHandle finiHandle = validateArgumentsOfWhileLoop(initHandle, predHandle, bodyHandle);
		
		/* Create the clause array with all these handles to be used by the generic loop */
		MethodHandle[] exitConditionClause = {null, null, predHandle, finiHandle};
		MethodHandle[] iterationClause = {initHandle, bodyHandle};
		return loop(exitConditionClause, iterationClause);
	}
	
	/**
	 * Produce a loop handle that wraps an initializer, a loop body and a predicate to execute a do-while loop.
	 * Loop variables are updated by the return values of the corresponding step handles (including the loop body)
	 * in each iteration.
	 * 
	 * @param initHandle a MethodHandle that represents the initial value of loop variable
	 * @param bodyHandle a MethodHandle that wraps the loop body to update the loop variable with its return value during iteration
	 * @param predHandle a MethodHandle that represents the loop condition
	 * @return a MethodHandle that represents the loop operation
	 * @throws NullPointerException - if the body handle or the predicate handle are null
	 * @throws IllegalArgumentException - if passed-in arguments are invalid or any constraint for while loop is violated
	 */
	public static MethodHandle doWhileLoop(MethodHandle initHandle, MethodHandle bodyHandle, MethodHandle predHandle) throws NullPointerException, IllegalArgumentException {
		
		/* Check the validity of the predicate (must be non-null) and body handle. Also
		 * verify that handles are effectively identical and constrained by body. Note:
		 * The constraints of initializer and predicate at other aspects will be checked
		 * later in the generic loop when adding clauses.
		 */
		MethodHandle finiHandle = validateArgumentsOfWhileLoop(initHandle, predHandle, bodyHandle);
		
		/* Create the clause array with all these handles to be used by the generic loop */
		MethodHandle[] handleClause = {initHandle, bodyHandle, predHandle, finiHandle};
		return loop(handleClause);
	}
	
	/* Validate the body handle against the constraints of a while/do-while loop and generate the 'fini' handle from it */
	private static MethodHandle validateArgumentsOfWhileLoop(MethodHandle initHandle, MethodHandle predHandle, MethodHandle bodyHandle) throws NullPointerException, IllegalArgumentException {
		if ((null == predHandle) || (null == bodyHandle)) {
			/*[MSG "K065C", "Both The predicate and the loop body must be non-null"]*/
			throw new NullPointerException(com.ibm.oti.util.Msg.getString("K065C")); //$NON-NLS-1$
		}
		
		MethodType bodyType = bodyHandle.type;
		Class<?> bodyReturnType = bodyType.returnType;
		int bodyParamLength = bodyType.parameterCount();
		
		/* The signature of the body handle must be either (V, A...)V or (A...)void */
		int bodyIterationVarLength = 0;
		if (void.class != bodyReturnType) {
			if (hasNoArgs(bodyType)) {
				/*[MSG "K065D3", "loop body has a non-void return type and must contain at least one argument. First argument must be {0}"]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K065D3", bodyReturnType)); //$NON-NLS-1$
			}
			
			if (bodyReturnType != bodyType.arguments[0]) {
				/*[MSG "K065D2", "The return type of loop body: {0} does not match the type of the first argument: {1}"]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K065D2", bodyReturnType, bodyType.arguments[0])); //$NON-NLS-1$
			}
			
			bodyIterationVarLength = 1;
		}
		
		/* Verify that handles are effectively identical and constrained by body. All we
		 * need to do here is verify that no parameters have longer argument lists than
		 * body. The generic loop will compare arguments to ensure all clauses are
		 * effectively identical.
		 */
		
		/* The predicate parameter list may be either empty or of the form (V A...) and should be
		 * compared to the full length of the loop body.
		 */ 
		if (predHandle.type.parameterCount() > bodyParamLength) {
			/*[MSG "K06542", "The count of loop predicate's parameters must be less than the body parameters."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K06542")); //$NON-NLS-1$
		}
		
		int bodyExternalParamLength = bodyParamLength - bodyIterationVarLength;
		if ((initHandle != null) && (initHandle.type.parameterCount() > bodyExternalParamLength)) {
			/*[MSG "K06541", "The count of {0}'s parameters must be less than the external parameters of body."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K06541", "loop initializer")); //$NON-NLS-2$
		}
		
		/* The Java 9 doc says the loop result type is the result type V of the body handle,
		 * and the loop return type in the generic loop is only determined by 'fini' handles.
		 * In such case, the 'fini' handle must be generated from the return type of body
		 * so as to determine the final loop return type in the generic loop.
		 */
		MethodHandle finiHandle = null;
		if (void.class != bodyReturnType) {
			finiHandle = identity(bodyReturnType);
		}
		
		return finiHandle;
	}
	
	/**
	 * Produce a loop handle that iterates over a range of numbers by specifying the start value
	 * and the end value of the loop counter. Loop variables are updated by the return values of 
	 * the corresponding step handles (including the loop body) in each iteration.
	 * 
	 * @param startHandle a MethodHandle that returns the start value (inclusive) of the counter in a loop
	 * @param endHandle a MethodHandle that returns the end value (exclusive) of the counter in a loop
	 * @param initHandle a MethodHandle that represents the initial value of loop variable
	 * @param bodyHandle a MethodHandle that wraps the loop body to update the loop variable with its return value during iteration
	 * @return a MethodHandle that represents the loop operation
	 * @throws NullPointerException - if any of the start, end and body handle is null
	 * @throws IllegalArgumentException - if passed-in arguments are invalid
	 */
	public static MethodHandle countedLoop(MethodHandle startHandle, MethodHandle endHandle, MethodHandle initHandle, MethodHandle bodyHandle) throws NullPointerException, IllegalArgumentException {
		validateArgumentsOfCountedLoop(startHandle, endHandle, initHandle, bodyHandle);
		
		/* From the generic loop perspective, the passed-in 'end' handle needs one more
		 * argument slot to be added to the internal parameter list [V, I, A...]
		 * in order for the end condition to be passed around.
		 * 
		 * There are three potential solutions to this issue as follows:
		 * 1) Adding the slot after 'I' doesn't work because all parameters after 'I' 
		 *    must belong to the external parameter list [A...]
		 * 2) Inserting the slot between 'V' and 'I' doesn't work because [V, I] is fixed
		 *    in terms of type and ordering.
		 * 3) Adding the slot to the front of 'V' doesn't violate the required ordering
		 *    of the existing parameter list [V, I, A...] and is therefore a valid solution.
		 * 
		 * Thus, the parameter list of non-initializer handles ('start', 'end' and 'init' are
		 * the initializer handles) should be modified by invoking dropArguments() to be 
		 * effectively identical to the complete internal parameter list [I, V, I, A...]
		 * in terms of type and ordering.
		 */
		
		/* Internally construct step and predicate handles for counter */
		MethodHandle counterIncr = null;
		MethodHandle counterPred = null;
		try {
			counterIncr = Lookup.internalPrivilegedLookup.findStatic(MethodHandles.class, "counterIncrement", MethodType.methodType(int.class, int.class, int.class)); //$NON-NLS-1$
			counterPred = Lookup.internalPrivilegedLookup.findStatic(MethodHandles.class, "counterPredicate", MethodType.methodType(boolean.class, int.class, int.class)); //$NON-NLS-1$
		} catch (IllegalAccessException | NoSuchMethodException e) {
			throw new InternalError("The method doesn't exit or it fails in the access checking", e); //$NON-NLS-1$
		}
		Class<?> bodyReturnType = bodyHandle.type.returnType;
		
		/* Initially the leading parameter type of body handle is 'V',
		 * so insert the counter argument 'I' (int type) for 'end' before V.
		 */
		bodyHandle = dropArguments(bodyHandle, 0, int.class);
		
		/* If the body handle's return type is non-void, then all non-initializer 
		 * handles with missing parameters should be correctly filled up according
		 * to the complete internal parameter list in the generic loop.
		 */
		MethodHandle loopReturnHandle = null;
		if (void.class != bodyReturnType) {
			/* Initially the parameter types of counterIncr/counterPred are [I, I] as
			 * the internal method handle has no idea of the parameter type of 'V' until the
			 * API gets invoked. So insert the parameter type 'V' after the 1st 'I' to 
			 * align with the internal parameter list.
			 */
			counterIncr = dropArguments(counterIncr, 1, bodyReturnType);  /* [I, V, I] */
			counterPred = dropArguments(counterPred, 1, bodyReturnType);  /* [I, V, I] */
			
			/* Insert the parameter type 'I' (int type) for 'end' before V
			 * Note: the missing parameters will be fill up in the generic loop.
			 */
			loopReturnHandle = dropArguments(identity(bodyReturnType), 0, int.class); /* [I, V] */
		}
		
		/* Assign each initializer handle to its associated clause */
		MethodHandle[] exitConditionClause = {endHandle, null, counterPred, loopReturnHandle};
		MethodHandle[] loopBodyClause = {initHandle, bodyHandle};
		MethodHandle[] counterClause = {startHandle, counterIncr};
		
		return loop(exitConditionClause, loopBodyClause, counterClause);
	}

	/**
	 * Produce a loop handle that executes a given number of iterations.
	 * 
	 * @param loopCountHandle a MethodHandle that returns the number of iterations for loop counter
	 * @param initHandle a MethodHandle that represents the initial value of loop variable
	 * @param bodyHandle a MethodHandle that wraps the loop body to update the loop variable with its return value during iteration
	 * @return a MethodHandle that represents the loop operation
	 * @throws NullPointerException - if any of the loop count and body handle is null
	 * @throws IllegalArgumentException - if passed-in arguments are invalid
	 */
	public static MethodHandle countedLoop(MethodHandle loopCountHandle, MethodHandle initHandle, MethodHandle bodyHandle) throws NullPointerException, IllegalArgumentException {
		return countedLoop(zero(int.class), loopCountHandle, initHandle, bodyHandle);
	}
	
	/* Validate all argument handles against the constraints of countedLoop */
	private static final void validateArgumentsOfCountedLoop(MethodHandle startHandle, MethodHandle endHandle, MethodHandle initHandle, MethodHandle bodyHandle) throws NullPointerException, IllegalArgumentException {
		if ((null == startHandle) || (null == endHandle) || (null == bodyHandle)) {
			/*[MSG "K065E", "The start/end/body handles must be non-null"]*/
			throw new NullPointerException(com.ibm.oti.util.Msg.getString("K065E")); //$NON-NLS-1$
		}
		
		Class<?> startReturnType = startHandle.type.returnType;
		Class<?> endReturnType = endHandle.type.returnType;
		if ((startReturnType != endReturnType)
			|| (int.class != startReturnType)
			|| (int.class != endReturnType)
		) {
			/*[MSG "K065F", "The return type of the start/end handles must be int"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K065F")); //$NON-NLS-1$
		}
		
		MethodType bodyType = bodyHandle.type;
		Class<?> bodyReturnType = bodyType.returnType;
		Class<?>[] bodyParamTypes = bodyType.arguments;
		int bodyParamLength = bodyParamTypes.length;
		
		/* The signature of the body handle must be either (V,I,A...)V or (I,A...)void */
		if (hasNoArgs(bodyType)) {
			/*[MSG "K065D1", "loop body must contain at least one argument."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K065D1")); //$NON-NLS-1$
		}
		
		int bodyIterationVarLength;
		if (void.class == bodyReturnType) {
			if (int.class != bodyParamTypes[0]) {
				/*[MSG "K065G", "The 1st parameter type of the body handle with a void return type must be int."]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K065G")); //$NON-NLS-1$
			}
			
			bodyIterationVarLength = 1;
		} else {
			if (bodyReturnType != bodyParamTypes[0]) {
				/*[MSG "K065D2", "The return type of loop body: {0} does not match the type of the first argument: {1}"]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K065D2", bodyReturnType, bodyParamTypes[0])); //$NON-NLS-1$
			}
			
			if (bodyParamLength < 2) {
				/*[MSG "K065H", "The parameters types of the body handle with the non-void return type include at least 2 loop variables: {0}"]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K065H", bodyType.toMethodDescriptorString())); //$NON-NLS-1$
			}
			
			if (int.class != bodyParamTypes[1]) {
				/*[MSG "K065I", "The 2nd parameter type of the body handle with a non-void return type must be int rather than {0}"]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K065I", bodyType.toMethodDescriptorString())); //$NON-NLS-1$
			}
			
			bodyIterationVarLength = 2;
		}

		/* As a special case, if the body contributes only V and I types, with no
		 * additional A types, then the internal parameter list is extended by the
		 * argument types A... of the end handle. Adjust A count to correctly verify
		 * parameters are effectively identical. Body will be altered outside of
		 * verification.
		 */
		int externalParamLength = bodyParamLength - bodyIterationVarLength;
		if (bodyParamLength == bodyIterationVarLength) {
			externalParamLength = endHandle.type.parameterCount();
		}
		
		/* Verify that handles are effectively identical and constrained by body. All we
		 * need to do here is verify that no parameters have longer argument lists than
		 * body. The generic loop will compare arguments to ensure all clauses are
		 * effectively identical.
		 */
		if (startHandle.type.parameterCount() > externalParamLength) {
			/*[MSG "K06541", "The count of {0}'s parameters must be less than the external parameters of body."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K06541", "start handle"));  //$NON-NLS-2$
		}
		
		if (endHandle.type.parameterCount() > externalParamLength) {
			/*[MSG "K06541", "The count of {0}'s parameters must be less than the external parameters of body."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K06541", "end handle"));  //$NON-NLS-2$
		}
		
		if ((null != initHandle) && (initHandle.type.parameterCount() > externalParamLength)) {
			/*[MSG "K06541", "The count of {0}'s parameters must be less than the external parameters of body."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K06541", "loop initializer"));  //$NON-NLS-2$
		}
	}
	
	private static int counterIncrement(int endValue, int counter) {
		return (counter + 1);
	}
	
	private static boolean counterPredicate(int endValue, int counter) {
		return (counter < endValue);
	}
	
	/**
	 * Produce a loop handle that iterates over a range of values produced by an Iterator<T>
	 * Loop variables are updated by the return values of the corresponding step handles 
	 * (including the loop body) in each iteration.
	 * 
	 * @param iteratorHandle a MethodHandle that returns Iterator or a subtype to start the loop
	 * @param initHandle a MethodHandle that represents the initial value of loop variable
	 * @param bodyHandle a MethodHandle that wraps the loop body to update the loop variable with its return value during iteration
	 * @return a MethodHandle that represents the Iterator-based loop operation
	 * @throws NullPointerException - if the loop body is null
	 * @throws IllegalArgumentException - if passed-in arguments are invalid
	 */
	public static MethodHandle iteratedLoop(MethodHandle iteratorHandle, MethodHandle initHandle, MethodHandle bodyHandle) throws NullPointerException, IllegalArgumentException {
		validateArgumentsOfIteratedLoop(iteratorHandle, initHandle, bodyHandle);
		
		MethodType bodyType = bodyHandle.type;
		Class<?> bodyReturnType = bodyType.returnType;
		
		/* The init handle for iterator is set to the method handle to Iterable.iterator()
		 * by default if it is null. The default iterator handle parameters are adjusted 
		 * to accept the leading A type.
		 */
		MethodHandle initIterator = iteratorHandle;
		if (null == initIterator) {
			int bodyParamTypesLength = bodyHandle.type.arguments.length;
			int bodyIterationVarLength = 2;
			if (void.class == bodyReturnType) {
				bodyIterationVarLength = 1;
			}
			
			/* If body has an A type, the default iterator handle is adjusted to accept the leading A type. 
			 * Validation has already confirmed that the leading A type is an Iterable or subtype thereof.
			 */
			Class<?> iterableType = Iterable.class;
			if (bodyParamTypesLength > bodyIterationVarLength) {
				iterableType = bodyHandle.type.arguments[bodyIterationVarLength];
			}
			
			try {
				initIterator = Lookup.internalPrivilegedLookup.findVirtual(iterableType, "iterator", MethodType.methodType(Iterator.class)); //$NON-NLS-1$
			} catch (IllegalAccessException | NoSuchMethodException e) {
				throw new InternalError("The iterator method doesn't exist or it fails in the access checking", e); //$NON-NLS-1$
			}
		}
		
		/* Method handles to Iterator.hasNext(), Iterator.next() and Iterable.iterator() are required
		 * to construct a clause array in the the generic loop.
		 */
		MethodHandle iteratorNextElement = null;
		MethodHandle iteratorHasNextElement = null;
		Class<?> iteratorType = initIterator.type.returnType;

		try {
			iteratorNextElement = Lookup.internalPrivilegedLookup.findVirtual(iteratorType, "next", MethodType.methodType(Object.class)); //$NON-NLS-1$
			iteratorHasNextElement = Lookup.internalPrivilegedLookup.findVirtual(iteratorType, "hasNext", MethodType.methodType(boolean.class)); //$NON-NLS-1$
		} catch (IllegalAccessException | NoSuchMethodException e) {
			throw new InternalError("The next/hasNext method doesn't exist or it fails in the access checking", e); //$NON-NLS-1$
		}

		MethodHandle returnValueFromBody = null;
		
		/* To accommodate the order of the following clauses in the generic loop:
		 * clause1 = {it = iterator(a...), null, it.hasNext(), return from the loop body)}
		 * clause2 = {v = init(a...), v = body(t = it.next(), v, a...)},
		 * 
		 * the order and type of internal parameter type list are changed
		 * from (V(clause2), T(returned from iterator in clause1), A...)
		 * to (I(iterator in clause1), V(clause2 for the loop body), A...)
		 * if the resulting loop handle returns the non-void value.
		 * 
		 * As a result, all non-initializer handles should be modified against
		 * the new internal parameter type list (I, V, A...).
		 * Note: iterator and init are the initializer handles in this API.
		 */
		
		/* Change the parameter list of the handle for the return value (clause1) 
		 * to (I, V) against the new internal parameter list (I, V, A...).
		 * Note: the missing parameters after V are handled in the generic loop.
		 */
		if (void.class != bodyReturnType) {
			returnValueFromBody = dropArguments(identity(bodyReturnType), 0, iteratorType);
		}
		
		/* Change the parameter list of loop body from (V, T, A...) to (T, V, A...)
		 * against the internal parameter list (I (Iterator returns T), V, A...).
		 * Note: the loop body needs to be specially addressed as it is not passed
		 * over to the generic loop.
		 */
		MethodHandle loopBody = recreateIteratedBodyHandle(iteratorHandle, bodyHandle);
		
		/* 1) For the void return type of the loop body, the parameter list should be (T, A...).
		 * 2) For the non-void return type of the loop body, the parameter list should be (T, V, A...)
		 * So the first parameter type is T returned from Iterator in both cases above.
		 */
		Class<?> typeTforIterator = loopBody.type.parameterType(0);
		
		/* Change the return type of Iterator.next() to the specific type T (the first parameter type of the loop body) */
		MethodType newTypeOfNextElementFromIterator = iteratorNextElement.type.changeReturnType(typeTforIterator);
		MethodHandle nextValueFromIterator = iteratorNextElement.asType(newTypeOfNextElementFromIterator);
		
		/* Create the first clause for Iterator */
		MethodHandle[] iteratorClause = {initIterator, null, iteratorHasNextElement, returnValueFromBody};
		
		/* In creating the second clause for the loop body, filterArguments() is invoked
		 * to implement the following 2 steps for the operations in "v = body(t = it.next(), v, a...)":
		 * 1) call Iterator.next() to return the value t (type T).
		 * 2) t is used by the loop body as the first parameter.
		 */
		MethodHandle loopStepHandle = filterArguments(loopBody, 0, nextValueFromIterator);
		MethodHandle[] loopBodyClause = {initHandle, loopStepHandle};
		
		return loop(iteratorClause, loopBodyClause);
	}
	
	/* Validate all argument handles against the constraints specific to iteratedLoop */
	private static final void validateArgumentsOfIteratedLoop(MethodHandle iteratorHandle, MethodHandle initHandle, MethodHandle bodyHandle) throws IllegalArgumentException {
		if (null == bodyHandle) {
			/*[MSG "K065Q", "The loop body must be non-null"]*/
			throw new NullPointerException(com.ibm.oti.util.Msg.getString("K065Q")); //$NON-NLS-1$
		}
		
		MethodType bodyType = bodyHandle.type;
		Class<?> bodyReturnType = bodyType.returnType;
		Class<?>[] bodyParamTypes = bodyType.arguments;
		int bodyParamTypesLength = bodyParamTypes.length;
		int bodyIterationVarLength = 1;
		
		if (hasNoArgs(bodyType)) {
			/*[MSG "K065D1", "loop body must contain at least one argument."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K065D1")); //$NON-NLS-1$
		}
		
		if (void.class != bodyReturnType) {
			if (bodyParamTypesLength < 2) {
				/*[MSG "K065H", "The parameters types of the body handle with the non-void return type include at least 2 loop variables: {0}"]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K065H", bodyType.toMethodDescriptorString())); //$NON-NLS-1$
			}
			
			/* The signature of the passed-in loop body must be either (V, T, A...)V or (T, A...)void */
			if (bodyReturnType != bodyParamTypes[0]) {
				/*[MSG "K065D2", "The return type of loop body: {0} does not match the type of the first argument: {1}"]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K065D2", bodyReturnType, bodyParamTypes[0])); //$NON-NLS-1$
			}
			
			bodyIterationVarLength = 2;
		}
		
		/* If the iterator handle is non-null, it must have the return type java.util.Iterator or a subtype thereof.
		 * If null, the internal parameter list must have at least one A type that is java.util.Iterator or a subtype.
		 * Otherwise default A will be set to java.util.Iterator.
		 */
		if (null != iteratorHandle) {
			if (!Iterator.class.isAssignableFrom(iteratorHandle.type.returnType)) {
				/*[MSG "K065J", "The return type of the iterator handle must be Iterator or its subtype rather than {0}"]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K065J", iteratorHandle.type.returnType.getName())); //$NON-NLS-1$
			}
		} else if (bodyParamTypesLength > bodyIterationVarLength) {
			if (!Iterable.class.isAssignableFrom(bodyParamTypes[bodyIterationVarLength])) {
				/*[MSG "K065L", "If the iterator handle is null, the leading external parameter type of loop body must be Iterable or its subtype rather than {0}"]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K065L", bodyParamTypes[bodyIterationVarLength].getName())); //$NON-NLS-1$
			}
		}
		
		if (null != initHandle) {
			MethodType initType = initHandle.type;
			Class<?> initReturnType = initType.returnType;
			if (initReturnType != bodyReturnType) {
				/*[MSG "K065M", "The return type of init and loop body doesn't match: {0} != {1}"]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K065M", initReturnType.getName(), bodyReturnType.getName())); //$NON-NLS-1$
			}
			
			Class<?>[] initParamTypes = initType.arguments;
			int initParamTypesLength = initParamTypes.length;
			if ((initParamTypesLength > 0) && (bodyParamTypesLength == bodyIterationVarLength)) {
				if (null == iteratorHandle) {
					if (!Iterable.class.isAssignableFrom(initParamTypes[0])) {
						/*[MSG "K065N", "If the iterator handle is null, the leading parameter type of init must be Iterable or its subtype rather than {0}"]*/
						throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K065N", initParamTypes[0].getName())); //$NON-NLS-1$
					}

				} else {
					/* Given that the external parameter list is determined by iterator(non-null) against the Java9 doc,
					 * we need to check the init handle to see whether it has more parameter types than iterator.
					 * Note: other cases will be addressed later in the generic loop.
					 */
					Class<?>[] iteratorParamTypes =  iteratorHandle.type.arguments;
					if (initParamTypesLength > iteratorParamTypes.length) {
						/*[MSG "K065O", "The parameter types of init doesn't match that of iterator: {0} != {1}"]*/
						throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K065O",  //$NON-NLS-1$
							Arrays.toString(initParamTypes), Arrays.toString(iteratorParamTypes)));
					}
				}
			}
		}
		
		/* All iteratedLoop parameters must be effectively identical to the body handle. We need to check
		 * the external parameter list of loop body here as it won't be directly handled by the generic loop.
		 */
		compareExternalParamTypesOfIteratedBodyHandles(bodyHandle, iteratorHandle);
		compareExternalParamTypesOfIteratedBodyHandles(bodyHandle, initHandle);
	}
	
	/* Compare with the external parameter types (except (V,T)) of the loop body to see whether they are identical to each other */
	private static final void compareExternalParamTypesOfIteratedBodyHandles(MethodHandle bodyHandle, MethodHandle targetHandle) {
		/* If the target handle is null validation is not necessary. */
		if (null == targetHandle) {
			return;
		}
		
		MethodType bodyType = bodyHandle.type;
		Class<?>[] bodyParamTypes = bodyType.arguments;
		MethodType targetType = targetHandle.type;
		Class<?>[] targetParamTypes = targetType.arguments;
		
		/* Remove V (if exist) and T from the parameter list of loop body so as to
		 * obtain its external parameter types.
		 */
		int fixedParamType = 1;
		if (void.class != bodyType.returnType) {
			fixedParamType += 1;
		}
		
		/* No need to compare if there is no external parameters for the loop body
		 * or the target handle.
		  */
		int bodyExternalParamTypesLength = bodyParamTypes.length - fixedParamType;
		int targetParamTypeLength = targetParamTypes.length;
		if ((0 == bodyExternalParamTypesLength) || (0 == targetParamTypeLength)) {
			return;
		}
		
		Class<?>[] bodyExternalParamTypes = new Class<?>[bodyExternalParamTypesLength];
		System.arraycopy(bodyParamTypes, fixedParamType, bodyExternalParamTypes, 0, bodyExternalParamTypesLength);
		
		if (targetParamTypeLength > bodyExternalParamTypesLength) {
			/*[MSG "K065P", "The external parameter types of body: {0} doesn't match the parameter types of iterator/init: {1}"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K065P",  //$NON-NLS-1$
												Arrays.toString(bodyExternalParamTypes), Arrays.toString(targetParamTypes)));
		}
		
		for (int paramIndex = 0; paramIndex < targetParamTypeLength; paramIndex++) {
			if (bodyExternalParamTypes[paramIndex] != targetParamTypes[paramIndex]) {
				/*[MSG "K065P", "The external parameter types of body: {0} doesn't match the parameter types of iterator/init: {1}"]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K065P",  //$NON-NLS-1$
													Arrays.toString(bodyExternalParamTypes), Arrays.toString(targetParamTypes)));
			}
		}
	}
	
	/* Recreate a loop body with a permuted parameter list (T, V, A...) */
	private static MethodHandle recreateIteratedBodyHandle(MethodHandle iteratorHandle, MethodHandle bodyHandle) {
		MethodHandle loopBody = bodyHandle;
		MethodType bodyType = loopBody.type;
		Class<?> bodyReturnType = bodyType.returnType;
		Class<?>[] bodyParamTypes = bodyType.parameterArray();
		int bodyParamLength = bodyParamTypes.length;
		int bodyIterationVarLength = 1;
		
		if (void.class != bodyReturnType) {
			Class<?> paramType1 = bodyParamTypes[0];
			Class<?> paramType2 = bodyParamTypes[1];
			bodyParamTypes[0] = paramType2;
			bodyParamTypes[1] = paramType1;
			MethodType newBodyType = MethodType.methodType(bodyReturnType, bodyParamTypes);
			
			int[] paramIndexReorder = new int[bodyParamLength];
			paramIndexReorder[0] = 1;
			paramIndexReorder[1] = 0;
			for (int i = 2; i < bodyParamLength; i++) {
				paramIndexReorder[i] = i;
			}
			
			loopBody = MethodHandles.permuteArguments(loopBody, newBodyType, paramIndexReorder);
			
			bodyIterationVarLength = 2;
		}
		
		/* Insert the external parameter types if the loop body only has V and T */
		if (bodyIterationVarLength == bodyParamTypes.length) {
			/* Insert the class Iterable to the external parameter part of the loop body if iterator is null */
			if (null == iteratorHandle) {
				loopBody = MethodHandles.dropArguments(loopBody, bodyIterationVarLength, Iterable.class);
			} else {
				/* Append all parameter types of iterator to the end of the parameter types of loop body */
				Class<?>[] bodyExternalParameterTypes = iteratorHandle.type.parameterArray();
				loopBody = MethodHandles.dropArguments(loopBody, bodyIterationVarLength, bodyExternalParameterTypes);
			}
		}
		
		return loopBody;
	}
	
	private static boolean hasNoArgs(MethodType type) {
		return (0 == type.arguments.length);
	}
	
	/* Wrap the logic of clause validation, construction of parameter list for each handle,
	 * as well as the population of missing handles/parameters in handle into a builder pattern.
	 */
	static class LoopHandleBuilder {
		/* Each clause typically consists of 4 handles including init, step, predicate and 'fini' */
		private static final int COUNT_OF_HANDLES = 4;
		
		private ArrayList<MethodHandle[]> clausesList = null;
		private int countOfClauses = 0;
		private ArrayList<Class<?>> iterationVarTypes = null;
		private int iterationVarTypesLength = 0;
		private boolean atLeastOnePredicateFound = false;
		private Class<?> loopReturnType = null;
		private Class<?>[] longestLoopParamTypes = null;
		private Class<?>[] fullLengthParamTypes = null;
		
		/* We need a full-length iteration variable type list (including the void type)
		 * for all clauses to deal with the omitted init/step handles in each clause.
		 */
		private ArrayList<Class<?>> iterationVarTypesOfAllClauses = null;
		
		/* Only initialize the the internal clause array and the iteration type list as
		 * the passed-in clauses need to be validated at first to guarantee
		 * they don't violate any constraint of loop before use.
		 */
		LoopHandleBuilder(int expectedSize) {
			clausesList = new ArrayList<>(expectedSize);
			iterationVarTypes = new ArrayList<>(expectedSize);
			iterationVarTypesOfAllClauses = new ArrayList<>(expectedSize);
		}
		
		/* Create an internal clause array by adding all passed-in clauses to it after the validation of constraints */
		private void createInternalClauses(MethodHandle[]... handleClauses) {
			/* Add all passed-in clause to an internal clause array if valid */
			for (MethodHandle[] clause : handleClauses) {
				addClause(clause);
			}
			
			if (!foundNonNullPredicate()) {
				/*[MSG "K0657", "There must be at least one predicate in the clause array"]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0657")); //$NON-NLS-1$	
			}
			
			/* Set the return type to void for the moment in case there is no 'fini' handle in all clauses */
			if (null == loopReturnType) {
				loopReturnType = void.class;
			}
			
			countOfClauses = clausesList.size();
			iterationVarTypesLength = iterationVarTypes.size();
		}
		
		/* Add a clause to an internal clause list if valid against the constraints of loop */
		public void addClause(MethodHandle[] currentClause) {
			if (null == currentClause) {
				/*[MSG "K0653", "Each clause in the clause array must be non-null: {0}"]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0653", Arrays.toString(currentClause))); //$NON-NLS-1$
			}
			
			int handleCount = currentClause.length;
			if (handleCount > COUNT_OF_HANDLES) {
				/*[MSG "K0655", "At most 4 method handles are allowed in each clause: {0}"]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0655", Arrays.toString(currentClause))); //$NON-NLS-1$	
			}
			
			MethodHandle[] newClause = new MethodHandle[COUNT_OF_HANDLES];
			
			MethodHandle initHandle = (handleCount >= 1) ? currentClause[0] : null;
			MethodHandle stepHandle = (handleCount >= 2) ? currentClause[1] : null;
			MethodHandle predHandle = (handleCount >= 3) ? currentClause[2] : null;
			MethodHandle finiHandle = (COUNT_OF_HANDLES == handleCount) ? currentClause[3] : null;
			
			/* The following check verifies two things:
			 * 1) Java 9 doc says clauses with all nulls are disregarded.
			 * So such clause must be skipped over.
			 * 
			 * 2) A clause with zero method handles should be skipped.
			 */
			if ((null == initHandle)
				&& (null == stepHandle)
				&& (null == predHandle)
				&& (null == finiHandle)
			) {
				return;
			}
			
			/* Create the longest parameter type list (A*) from all init handles which is
			 * used later to generate the final parameter type list (A...) of loop handle.
			 */
			if (null != initHandle) {
				updateLongerLoopParamTypes(initHandle.type.arguments);
			}
			
			/* Ensure the init handle and the step handle have compatible return types */
			Class<?> commonReturnType = getCommonReturnType(initHandle, stepHandle);
			iterationVarTypesOfAllClauses.add(commonReturnType);
			if (void.class != commonReturnType) {
				iterationVarTypes.add(commonReturnType);
			}
			newClause[0] = initHandle;
			newClause[1] = stepHandle;
			
			/* Check whether at least one predicate handle exists in the clause array */
			if (null != predHandle) {
				atLeastOnePredicateFound = true;
				if (boolean.class != predHandle.type.returnType) {
					/*[MSG "K0656", "The return type of predicate must be boolean: {0}"]*/
					throw new IllegalArgumentException(Msg.getString("K0656", Arrays.toString(currentClause))); //$NON-NLS-1$
				}
			}
			newClause[2] = predHandle;
			
			/* Validate all 'fini' handles have the same return type */
			if (null != finiHandle) {
				Class<?> finiReturnType = finiHandle.type.returnType;
				if (null == loopReturnType) {
					loopReturnType = finiReturnType;
				} else if (loopReturnType != finiReturnType) {
					/*[MSG "K0659", "The return type of all 'fini' handles must be identical"]*/
					throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0659")); //$NON-NLS-1$
				}
			}
			newClause[3] = finiHandle;
			
			/* Add the clause as it passes basic validity checks */
			clausesList.add(newClause);
		}
		
		/* Obtain the common return type from the init handle and the step handle if identical */
		private Class<?> getCommonReturnType(MethodHandle handle1, MethodHandle handle2) {
			Class<?> returnType1 = null;
			Class<?> returnType2 = null;
			Class<?> commonReturnType = void.class;
			int notNullCount = 0;
			
			if (null != handle1) {
				returnType1 = handle1.type.returnType;
				commonReturnType = returnType1;
				notNullCount += 1;
			}
			
			if (null != handle2) {
				returnType2 = handle2.type.returnType;
				commonReturnType = returnType2;
				notNullCount += 1;
			}
			
			/* The common return type can't be null here as long as one of the handles isn't null.
			 * Note:
			 * 1) notNullCount is 0: If both handles are null, then their return type will match (both are null).
			 * 2) notNullCount is 1: If one of the handles is null, then their return type will differ between {null, X} 
			 *    which is valid to use notNullCount to cover that.
			 * 3) notNullCount is 2: if both handles are non-null, then their return types must be identical;
			 *    otherwise an exception will be thrown out for the purpose of mismatch.
			 */
			if ((returnType1 != returnType2) && (2 == notNullCount)) {
				/*[MSG "K0658", "The return type of init and step doesn't match: {0} != {1}"]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0658",  //$NON-NLS-1$
													new Object[] {returnType1.getName(), returnType2.getName()}));

			}
			
			return commonReturnType;
		}
		
		/* Check whether at least one non-omitted predicate handle exists in the clause array */
		private boolean foundNonNullPredicate() {
			return atLeastOnePredicateFound;
		}
		
		/* Build a loop handle with a fully established internal clause array in which only 4 handles
		 * with the full-length parameter list exist in each clause.
		 */
		public MethodHandle buildLoopHandle() throws NullPointerException, IllegalArgumentException {
			/* Mainly obtain the parameter type list of the resulting loop handle */
			MethodType loopHandleType = getLoopHandleType();
			
			/* Create a full-length of parameter types (V...,A...) of non-init handles for comparison*/
			createFullLengthParameterTypesForNonInitHandle();
			
			/* There includes two missing parts to be addressed in each clause:
			 * 1) Fill in the omitted handles (set to null initially) in the clause array to ensure
			 *    each clause only consists of four handles including init, step, pred and 'fini'.
			 * 2) Fill in the missing parameter types of all handles in the clause array
			 */
			fillingInMissingPartsOfClause();
			
			MethodHandle[][] handleClauses = clausesList.toArray(new MethodHandle[countOfClauses][]);
			MethodHandle result = buildTransformHandle(new LoopHelper(handleClauses, iterationVarTypesLength), loopHandleType);
			MethodHandle thunkable = LoopHandle.get(result, handleClauses);
			assert(thunkable.type() == result.type());
			result = thunkable;
			return result;
		}
		
		/* Get the invariable parameter types (A...) plus the return type of the resulting loop handle */
		private MethodType getLoopHandleType() throws IllegalArgumentException {
			/* The loop parameters (A...) are determined by all handles in the clause array.
			 * To be specific, (A...) = max {the longest parameter types of all init handles,
			 * the remaining parameter types of all non-init handles after deleting (V...) }
			 * Note: 
			 * 1) the full-length parameter type list of all non-init handles is (V..., A...).
			 * 2) the return type of the resulting loop handle is only determined by all 'fini' handles
			 *    in the clause array (set to void if there is no 'fini' handle existing in this clause).
			 */
			for (int clauseIndex = 0; clauseIndex < countOfClauses; clauseIndex++) {
				MethodHandle[] currentClause = clausesList.get(clauseIndex);
				for (int handleIndex = 1; handleIndex < COUNT_OF_HANDLES; handleIndex++) {
					MethodHandle currrentHandle = currentClause[handleIndex];
					if ((null != currrentHandle)
						&& (currrentHandle.type.parameterCount() > 0)
					) {
						/* Remove the iteration variable types (V...) of non-init handles to get the remaining parameter types */
						Class<?>[] suffixOfParamTypes = getSuffixOfParamTypesFromNonInitHandle(currrentHandle.type.arguments);
						
						/* Compared with the existing longest parameter types to determine
						 * the longer parameter types for use in the next non-init handle.
						 * Note: the initial longest parameter types is generated among
						 * the parameter types of all init handles. 
						 */
						updateLongerLoopParamTypes(suffixOfParamTypes);
					}
				}
			}
			
			/* only set the method type if there is no parameter of loop handle */
			if (null == longestLoopParamTypes) {
				return MethodType.methodType(loopReturnType);
			}
			
			return MethodType.methodType(loopReturnType, longestLoopParamTypes);
		}
		
		/* Get the suffixes (A*) of parameter types from a non-init handle by truncating
		 * the iteration variable types (V...). Make sure iteration variables are
		 * effectively identical.
		 */
		private Class<?>[] getSuffixOfParamTypesFromNonInitHandle(Class<?>[] handleParamTypes) throws IllegalArgumentException {
			/* There are three cases in terms of a non-init handle as follows:
			 * (1) If suffixLength > 0, it means the handle comes with all iteration variables (V...)
			 *     and probably part of loop variables (A*) that should be extracted from here.
			 * (2) If suffixLength == 0, it means the handle comes with all iteration variables (V...) 
			 *     and without loop variable (A...)
			 * (3) If suffixLength < 0, it means it means the handle only comes with part of iteration 
			 *     variables (V...) and without loop variable (A...).
			 * In all these cases above, all missing variables will be filled up against (V..., A...) 
			 * later in setMissingParamTypesOfHandle().
			 */
			int suffixLength = handleParamTypes.length - iterationVarTypesLength;
			
			/* The handle may have either some or all iteration variables. Regardless of the case those iteration variables
			 * that it does contain must be validated. The handle will be filled in later with a complete list.
			 */
			int minVarTypesLength = iterationVarTypesLength;
			if (minVarTypesLength > handleParamTypes.length) {
				minVarTypesLength = handleParamTypes.length;
			}
			
			for (int loopVarIndex = 0; loopVarIndex < minVarTypesLength; loopVarIndex++) {
				if (handleParamTypes[loopVarIndex] != iterationVarTypes.get(loopVarIndex)) {
					/*[MSG "K065A", "The prefixes of parameter types of a non-init handle: {0} is not effectively identical to the iteration variable types: {1}"]*/
					throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K065A",  //$NON-NLS-1$
							new Object[] {Arrays.toString(handleParamTypes), iterationVarTypes.toArray()}));
				}
			}
			
			/* If handle contains all iteration variables (V...) and some external variables (A..)
			 * the external variables should be returned as the parameter suffix.
			 */
			Class<?>[] suffixOfParamTypes = null;
			if (suffixLength > 0) {
				suffixOfParamTypes = new Class<?>[suffixLength];
				System.arraycopy(handleParamTypes, iterationVarTypesLength, suffixOfParamTypes, 0, suffixLength);
			}
			
			return suffixOfParamTypes;
		}
		
		/* Compare the existing longest parameter types with the parameter types from handles (must be effectively identical) so as to determine the longer one */
		private void updateLongerLoopParamTypes(Class<?>[] paramTypes) throws IllegalArgumentException {
			/* No need to compare if one of the parameter types is null */
			if (null == longestLoopParamTypes) {
				longestLoopParamTypes = paramTypes;
				return;
			} else if (null == paramTypes) {
				return;
			}
			
			int longestLoopParamTypesLength = longestLoopParamTypes.length;
			int paramTypesLength = paramTypes.length;
			
			int minParamTypeLength = longestLoopParamTypesLength;
			if (minParamTypeLength > paramTypesLength) {
				minParamTypeLength = paramTypesLength;
			}
			
			for (int paramTypeIndex = 0; paramTypeIndex < minParamTypeLength; paramTypeIndex++) {
				if (longestLoopParamTypes[paramTypeIndex] != paramTypes[paramTypeIndex]) {
					/*[MSG "K065B", "Both of the parameter types are not effectively identical to each other: {0} != {1}"]*/
					throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K065B",  //$NON-NLS-1$
							new Object[] {Arrays.toString(longestLoopParamTypes), Arrays.toString(paramTypes)}));	
				}
			}
			
			if (paramTypesLength > longestLoopParamTypesLength) {
				longestLoopParamTypes = paramTypes;
			}
		}
		
		/* Set a full-length of parameter types for non-init handles including iteration variables (V...) and loop parameters (A...) */
		private void createFullLengthParameterTypesForNonInitHandle() {
			Class<?>[] iterationVarTypesArray = iterationVarTypes.toArray(new Class<?>[iterationVarTypesLength]);
			
			int loopHandleParamLenth = 0;
			/* The resulting loop handle has no parameters if null */
			if (null != longestLoopParamTypes) {
				loopHandleParamLenth = longestLoopParamTypes.length;
			}
			
			/* From the full-length of parameter list (V..., A...) for all non-init handles
			 * by copying (V...) and (A...) to an array.
			 */
			fullLengthParamTypes = new Class<?>[iterationVarTypesLength + loopHandleParamLenth];
			System.arraycopy(iterationVarTypesArray, 0, fullLengthParamTypes, 0, iterationVarTypesLength);
			if (loopHandleParamLenth > 0) {
				System.arraycopy(longestLoopParamTypes, 0, fullLengthParamTypes, iterationVarTypesLength, loopHandleParamLenth);
			}
		}
		
		/* Fill in all missing parts including omitted handles and the incomplete parameter types of handle in each clause */
		private void fillingInMissingPartsOfClause() throws NullPointerException {
			for (int clauseIndex = 0; clauseIndex < countOfClauses; clauseIndex++) {
				for (int handleIndex = 0; handleIndex < COUNT_OF_HANDLES; handleIndex++) {
					MethodHandle[] currentClause = clausesList.get(clauseIndex);
					Class<?> iterationVarType = iterationVarTypesOfAllClauses.get(clauseIndex);
					
					/* 1) if the handle at the index doesn't exist, then fill it in against Java 9 doc's explanation.
					 * 2) if not the case, then check to ensure that it ends up with a full-length of parameter types
					 */
					currentClause[handleIndex] = setOmittedHandlesOfClause(currentClause, handleIndex, iterationVarType);
					currentClause[handleIndex] = setMissingParamTypesOfHandle(currentClause, handleIndex);
				}
			}
		}
		
		/* Set the omitted handles in all clauses to handles with a default type value */
		private MethodHandle setOmittedHandlesOfClause(MethodHandle[] currentClause, int handleIndex, Class<?> iterationVarType) throws NullPointerException {
			MethodHandle currentHandle = currentClause[handleIndex];
			if (null != currentHandle) {
				return currentHandle;
			}
			
			switch (handleIndex) {
			case 0:
				/* For an init handle, set to a handle with the default value
				 * of the iteration variable type in this clause (V[index of clause]).
				 */
				currentHandle = zero(iterationVarType);
				break;
			case 1:
				/* Note: there is a flaw in Java 9 doc in explaining the step handle as follows:
				 * 1) If both init and step are omitted (null), there is no iteration variable for the corresponding clause 
				 *    (void is used as the type to indicate that).
				 * 2) When filling in the omitted functions, if a step function is omitted (set to null initially), 
				 *    use an identity function of the clause's iteration variable type.
				 * However, the explanation of step above doesn't explain how to deal with the omitted step when void is 
				 * used as the temporary type of iteration variable in this clause (both init and step are omitted).
				 * e.g. {null, null, pred, fini}
				 * Under such circumstance, invoking identity(void.class) will end up with IllegalArgumentException.
				 * 
				 * The corner test case on RI JDK 9 indicates that it addresses the situation in the same way as init.
				 * Given that there is no further explanation in Java 9 doc, we choose to invoke zero(void.class) here
				 * so as to match RI's final behavior in tests.
				 */
				if (void.class == iterationVarType) {
					currentHandle = zero(iterationVarType);
				} else {
					currentHandle = identity(iterationVarType);
				}
				break;
			case 2:
				/* for a predicate handle, set to a constant handle with true as
				 * the default return value.
				 */
				currentHandle = constant(boolean.class, Boolean.valueOf(true));
				break;
			case 3:
				/* For a 'fini' handle, set to a handle with the default value
				 * of the return type from the resulting loop handle.
				 */
				currentHandle = zero(loopReturnType);
				break;
			default:
				break;
			}
			
			return currentHandle;
		}
		
		/* Set all missing parameter types to the full length for all handles in the clause array */
		private MethodHandle setMissingParamTypesOfHandle(MethodHandle[] currentClause, int handleIndex) {
			MethodHandle currentHandle = currentClause[handleIndex];
			Class<?>[] parameterTypes = null;
			
			/* Fill in the missing parameter types to the full length for each handle in each clause as follows:
			* 1) For the init handle, fill in all missing parameter types against the parameter 
			*    type list of the resulting loop handle (A...).
			*    e.g. if the parameter type of an init handle is {int} and
			*    the parameter types of the loop handle (A...) are {int, int},
			*    then the final parameter types of the init handle should be {int, int},
			*    which is the same as (A...) in terms of count and parameter types.
			*
			* 2) For non-init handles, fill in all missing parameter types against
			*    the iteration variable types (V...) plus the parameter type list
			*    of the resulting loop handle (A...).
			*    e.g. 
			*    if the parameter types of the predicate handle is {int, long}
			*    and (V..., A...) is {int, long, int, long, int},
			*    then the final parameter types of the predicate handle should be 
			*    {int, long, int, long, int}, which is the same as (V..., A...)
			*    in terms of count and parameter types.
			 */
			switch (handleIndex) {
			case 0:
				/* (A...) used for init handles with missing parameter types */
				parameterTypes = longestLoopParamTypes;
				break;
			default:
				/* (V...,A...) used for non-init handles with missing parameter types */
				parameterTypes = fullLengthParamTypes;
				break;
			
			}
			currentHandle = getHandleWithFullLengthParamTypes(currentHandle, parameterTypes);
			
			/* Note: According to the description of Java 9 Doc, the implementation of loop is only
			 * responsible for populating the missing suffixes of parameter types for all handles
			 * automatically for users. Apart from that, Users must explicitly call dropArguments()
			 * to populate the missing types in the following situations:
			 * 1) the prefixes are missing for the full-length parameter type list of any handle.
			 *    e.g. the parameter types of the step handle are {int, long, long}
			 *    while the full-length parameter type list is {INT, int, long, long}. (where INT means the missing int type)
			 * 2) some of parameter types are missing between (V...) and (A...).
			 *    e.g. the parameter types of the predicate handle are {int, int, long, long, int, int}
			 *    while the full-length parameter type list is {int, int, long, INT, long, int, int}.
			 */
			
			return currentHandle;
		}
		
		/* Generate a clause handle with a full-length array of parameter types */
		private MethodHandle getHandleWithFullLengthParamTypes(MethodHandle handleOfClause, Class<?>[] expectedParamTypes) {
			MethodHandle currentHandle = handleOfClause;
			int handleParamLength = handleOfClause.type.arguments.length;
			
			/* 1) No need to update the parameter types of the handle if (V..., A...) or (A...) doesn't exist.
			 *    It means that neither init handles nor non-init handles have parameters.
			 */
			if (null == expectedParamTypes) {
				return currentHandle;
			}
			
			/* No need to update the parameter types of the handle if it
			 * already holds the full-length of parameter types.
			 */
			int missingParamTypesLength = expectedParamTypes.length - handleParamLength;
			if (0 == missingParamTypesLength) {
				return currentHandle;
			}
			
			/* Append the missing suffixes against the requested parameter type list intended for this handle */
			Class<?>[] missingParamTypes = new Class<?>[missingParamTypesLength];
			System.arraycopy(expectedParamTypes, handleParamLength, missingParamTypes, 0, missingParamTypesLength);
			return MethodHandles.dropArguments(currentHandle, handleParamLength, missingParamTypes);
		}
	}
	
	/* A helper class that implements the loop operation */
	private static final class LoopHelper implements ArgumentHelper {
		private final MethodHandle[][] handleClauses;
		private final int iterationVarTypesLength;
		
		LoopHelper(MethodHandle[][] handleClauses, int iterationVarTypesLength) {
			this.handleClauses = handleClauses;
			this.iterationVarTypesLength = iterationVarTypesLength;
		}
		
		@Override
		@SuppressWarnings("boxing")
		public Object helper(Object[] arguments) throws Throwable {
			int countOfClause = handleClauses.length;
			int argumentCount = arguments.length;
			int loopParamTypeIndex = 0;
			Object[] loopParamTypes = new Object[iterationVarTypesLength + argumentCount];
			
			/* Set the initial value of iteration variables (v...) */
			for (int clauseIndex = 0; clauseIndex < countOfClause; clauseIndex++) {
				MethodHandle[] currentClause = handleClauses[clauseIndex];
				
				/* Given that the Java 9 doesn't explain how to deal with the void return type
				 * for the init and step handles in implementation and the test on RI indicates
				 * that the init/step handle does get executed but ends up with nothing,
				 * we choose to invoke the init/step handle without any return value so as to
				 * match RI's behavior even though there is nothing to return/affect the final
				 * result.
				 */
				if (void.class == currentClause[0].type.returnType){
					currentClause[0].invokeWithArguments(arguments);
				} else {
					/* Update the the corresponding iteration variable in the internal parameter list
					 * if the init handle returns non-void value.
					 */
					loopParamTypes[loopParamTypeIndex] = currentClause[0].invokeWithArguments(arguments);
					loopParamTypeIndex += 1;
				}
			}
			/* Set the value of loop invariant (a...) so the whole parameter list of non-init handles should be (v..., a...) */
			System.arraycopy(arguments, 0, loopParamTypes, loopParamTypeIndex, argumentCount);
			
			while (true) {
				loopParamTypeIndex = 0;
				for (int clauseIndex = 0; clauseIndex < countOfClause; clauseIndex++) {
					MethodHandle[] currentClause = handleClauses[clauseIndex];
					
					/* As with init, the step handle with the void return type only gets executed
					 * and returns nothing.
					 */
					if (void.class == currentClause[1].type.returnType) {
						currentClause[1].invokeWithArguments(loopParamTypes);
					} else {
						/* Call the step handle to update the value of iteration variable for each clause */
						loopParamTypes[loopParamTypeIndex] = currentClause[1].invokeWithArguments(loopParamTypes);
						loopParamTypeIndex += 1;
					}
					
					boolean predicateCondition = (boolean)currentClause[2].invokeWithArguments(loopParamTypes);
					if (!predicateCondition) {
						/* Call the 'fini' handle to exit if the condition set by the predicate handle
						 * is violated/exceeded with the updated iteration variable for this clause.
						 * Note: according to the Java 9 doc, there would be an infinite loop
						 * if the condition returned from the predicate handle is always true.
						 * Under such circumstance, the implementation of loop is not responsible for
						 * terminating the infinite loop until the user forces the JVM to exit.
						 */
						return currentClause[3].invokeWithArguments(loopParamTypes);
					}
				}
			}
		}
	}
	/*[ENDIF]*/
	
	private static final class CatchHelper implements ArgumentHelper {
		private final MethodHandle tryTarget;
		private final MethodHandle catchTarget;
		private final Class<? extends Throwable> exceptionClass;
		
		CatchHelper(MethodHandle tryTarget, MethodHandle catchTarget, Class<? extends Throwable> exceptionClass) {
			this.tryTarget = tryTarget;
			this.catchTarget = catchTarget;
			this.exceptionClass = exceptionClass;
		}
		
		public Object helper(Object[] arguments) throws Throwable {
			try {
				return tryTarget.invokeWithArguments(arguments);
			} catch(Throwable t) {
				if (exceptionClass.isInstance(t)) {
					int catchArgCount = catchTarget.type.parameterCount();
					Object[] amendedArgs = new Object[catchArgCount];
					amendedArgs[0] = t;
					System.arraycopy(arguments, 0, amendedArgs, 1, catchArgCount - 1);
					return catchTarget.invokeWithArguments(amendedArgs);
				}
				throw t;
			}
		}
	}
	
	/*[IF Sidecar19-SE]*/
	private static final class FinallyHelper implements ArgumentHelper {
		private final MethodHandle tryTarget;
		private final MethodHandle finallyTarget;
		
		FinallyHelper(MethodHandle tryTarget, MethodHandle finallyTarget) {
			this.tryTarget = tryTarget;
			this.finallyTarget = finallyTarget;
		}
		
		@Override
		@SuppressWarnings("boxing")
		public Object helper(Object[] arguments) throws Throwable {
			Throwable finallyThrowable = null;
			Object result = null;
			try {
				result = tryTarget.invokeWithArguments(arguments);
			} catch (Throwable tryThrowable) {
				finallyThrowable = tryThrowable;
				throw tryThrowable;
			} finally {
				int finallyParamCount = finallyTarget.type.parameterCount();
				Class<?> tryTargetReturnType = tryTarget.type.returnType;
				Object[] finallyParams = new Object[finallyParamCount];
				finallyParams[0] = finallyThrowable;
				
				/* Ignore the result in the case of the void return type of tryTarget */
				if (void.class == tryTargetReturnType) {
					System.arraycopy(arguments, 0, finallyParams, 1, finallyParamCount - 1);
				} else {
					finallyParams[1] = result;
					/* According to the JDK 9 doc,the second parameter of the finally handle 
					 * should be assigned with the following value as placeholder when 
					 * an exception is thrown out from the try handle:
					 * 1) Set to false if the return type of the try handle is boolean;
					 * 2) Set to 0 if the return type of the try handle is primitive;
					 * 3) Otherwise, it will be null when an exception occurs.
					 */
					if (null != finallyThrowable) {
						if (boolean.class == tryTargetReturnType) {
							finallyParams[1] = false;
						} else if (tryTargetReturnType.isPrimitive()) {
							finallyParams[1] = (byte)0;
						}
					}
					System.arraycopy(arguments, 0, finallyParams, 2, finallyParamCount - 2);
				}
				result = finallyTarget.invokeWithArguments(finallyParams);
			}
			return result;
		}
	}
	/*[ENDIF]*/
	
	/**
	 * Helper class used by collectArguments.
	 */
	private static final class CollectHelper implements ArgumentHelper {
		private final MethodHandle target;
		private final MethodHandle filter;
		private final int pos;
		
		CollectHelper(MethodHandle target, int pos, MethodHandle filter) {
			this.target = target;
			this.filter = filter;
			this.pos = pos;
		}
		
		public Object helper(Object[] arguments) throws Throwable {
			// Invoke filter
			int filterArity = filter.type.parameterCount();
			Object filterReturn = filter.invokeWithArguments(Arrays.copyOfRange(arguments, pos, pos + filterArity));
			
			// Construct target arguments
			Object[] newArguments = new Object[(arguments.length - filterArity) + 1];
			System.arraycopy(arguments, 0, newArguments, 0, pos);
			int resumeTargetArgs = pos + filterArity;
			newArguments[pos] = filterReturn;
			System.arraycopy(arguments, resumeTargetArgs, newArguments, (pos + 1), arguments.length - (resumeTargetArgs));
			
			// Invoke target
			return target.invokeWithArguments(newArguments);
		}
	}
	
	/**
	 * Helper class used by collectArguments when the filter handle returns void.
	 */
	private static final class VoidCollectHelper implements ArgumentHelper {
		private final MethodHandle target;
		private final MethodHandle filter;
		private final int pos;
		
		VoidCollectHelper(MethodHandle target, int pos, MethodHandle filter) {
			this.target = target;
			this.filter = filter;
			this.pos = pos;
		}
		
		public Object helper(Object[] arguments) throws Throwable {
			// Invoke filter
			int filterArity = filter.type.parameterCount();
			filter.invokeWithArguments(Arrays.copyOfRange(arguments, pos, pos + filterArity));
			
			// Construct target arguments
			Object[] newArguments = new Object[arguments.length - filterArity];
			System.arraycopy(
					arguments, 0,
					newArguments, 0,
					pos);
			int resumeTargetArgs = pos + filterArity;
			System.arraycopy(
					arguments, resumeTargetArgs,
					newArguments, pos,
					arguments.length - (resumeTargetArgs));
			
			// Invoke target
			return target.invokeWithArguments(newArguments);
		}
	}

/*[IF Sidecar18-SE-OpenJ9]*/	
	static MethodHandle basicInvoker(MethodType mt) {
		throw OpenJDKCompileStub.OpenJDKCompileStubThrowError();
	}
/*[ENDIF]*/	
}

