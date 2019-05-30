/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2012, 2019 IBM Corp. and others
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

import static java.lang.invoke.MethodType.methodType;

import java.io.InputStream;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.ref.WeakReference;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.security.ProtectionDomain;
import java.util.Collections;
import java.util.Map;
import java.util.WeakHashMap;

/*[IF Sidecar19-SE]
import jdk.internal.misc.Unsafe;
import sun.security.util.IOUtils;
/*[ELSE]*/
import sun.misc.Unsafe;
import sun.misc.IOUtils;
/*[ENDIF]*/

import com.ibm.oti.vm.VMLangAccess;

/*
 * A factory class that manages the relationship between ClassLoaders and the
 * injected SecurityFrame class, which acts as a trampoline to provide the 
 * correct view to caller sensitive methods.
 * This class also provides an API to determine which methods are sensitive to
 * their callers.
 */
final class SecurityFrameInjector {
	
	/* Map from ClassLoader to injected java.lang.invoke.SecurityFrame.  Map is required as findClass() will
	 * search the hierarchy.
	 */
	static Map<ClassLoader, WeakReference<Class<?>>> LoaderToSecurityFrameClassMap = Collections.synchronizedMap(new WeakHashMap<ClassLoader, WeakReference<Class<?>>>());
	
	static byte[] securityFrameClassBytes = null;
	
	/* Private class to act as a lock as using 'new Object()' for a lock 
	 * prevents lock word optimizations.
	 */
	static final class SecurityFrameInjectorLoaderLock { };
	/* Single shared lock to control attempts to define SecurityFrame in different loaders */
	static SecurityFrameInjectorLoaderLock loaderLock = new SecurityFrameInjectorLoaderLock();
	
	/*
	 * Helper method required by MethodHandleUtils.isCallerSensitive() to determine 
	 * if a methodhandle can be called virtually from the class that defines the
	 * caller-sensitive method.
	 */
	static boolean virtualCallAllowed(MethodHandle handle, Class<?> sensitiveMethodReferenceClass) {
		if (handle instanceof PrimitiveHandle) {
			Class<?> handleDefc = handle.getDefc();
			if (handleDefc == sensitiveMethodReferenceClass) {
				return true;
			}
			int modifiers = handle.getModifiers();
			if (Modifier.isPrivate(modifiers) || Modifier.isStatic(modifiers)) {
				return false;
			}
			if (sensitiveMethodReferenceClass.isAssignableFrom(handleDefc) || handleDefc.isInterface()) {
				return true;
			}
		}
		return false;
	}
	
	/*
	 * Reads and caches the classfile bytes for java.lang.invoke.SecurityFrame
	 */
	static byte[] initializeSecurityFrameClassBytes() {
		if (securityFrameClassBytes == null) {
			synchronized(SecurityFrameInjector.class) {
				if (securityFrameClassBytes == null) {
					securityFrameClassBytes = AccessController.doPrivileged(new PrivilegedAction<byte[]>() {
						public byte[] run() {
							try {
								/*[IF Sidecar19-SE]*/								
								return Lookup.class.getResourceAsStream("/java/lang/invoke/SecurityFrame.class").readAllBytes(); //$NON-NLS-1$
								/*[ELSE]*/
								InputStream is = Lookup.class.getResourceAsStream("/java/lang/invoke/SecurityFrame.class"); //$NON-NLS-1$
								return IOUtils.readFully(is, -1, true);
								/*[ENDIF]*/
							} catch(java.io.IOException e) {
								/*[MSG "K056A", "Unable to read java.lang.invoke.SecurityFrame.class bytes"]*/
								throw new Error(com.ibm.oti.util.Msg.getString("K056A"), e); //$NON-NLS-1$
							}
						}
					});
				}
			}
		}
		return securityFrameClassBytes;
	}
	
	static Class<?> probeLoaderToSecurityFrameMap(ClassLoader loader) {
		WeakReference<Class<?>> weakRef = LoaderToSecurityFrameClassMap.get(loader);
		if (weakRef != null) {
			return weakRef.get();
		}
		return null;
	}
			
	/*
	 * Wraps the passed in MethodHandle with a trampoline MethodHandle.  The trampoline handle
	 * is from the same classloader & protection domain as the context class argument.
	 * 
	 * Ie: Class A does lookup().in(Object.class).find.... The trampoline will be built against
	 * class A, not Object.
	 */
	static MethodHandle wrapHandleWithInjectedSecurityFrame(MethodHandle handle, final Class<?> context) {
		initializeSecurityFrameClassBytes();
		
		/* preserve varargs status so that the wrapper handle is also correctly varargs/notvarargs */
		boolean isVarargs = handle.isVarargsCollector();
		if (handle instanceof PrimitiveHandle) {
			isVarargs |= Lookup.isVarargs(handle.getModifiers());
		}
		
		final MethodHandle tempFinalHandle = handle;
		MethodType originalType = handle.type;
		
		try {
			Object o = AccessController.doPrivileged(new PrivilegedAction<Object>() {
				
				/* Helper method to Unsafe.defineClass the securityFrame into the 
				 * provided ClassLoader and ProtectionDomain.
				 */
				private Class<?> injectSecurityFrameIntoLoader(ClassLoader loader, ProtectionDomain pd) {
					return Unsafe.getUnsafe().defineClass(
							"java.lang.invoke.SecurityFrame", //$NON-NLS-1$
							securityFrameClassBytes,
							0,
							securityFrameClassBytes.length,
							loader,
							pd);
				}
				
				public Object run() {
					Class<?> injectedSecurityFrameClass;
					VMLangAccess vma = Lookup.getVMLangAccess();
					ClassLoader rawLoader = vma.getClassloader(context);
					/* Probe map to determine if java.lang.invoke.SecurityFrame has already been injected
					 * into this ClassLoader.  If it hasn't, use Unsafe to define it.  We can't use
					 * Class.forName() here as it will crawl the class hierarchy and SecurityFrame must
					 * be included in each loader / protection domain.
					 */
					injectedSecurityFrameClass = probeLoaderToSecurityFrameMap(rawLoader);
					if (injectedSecurityFrameClass == null) {
						synchronized(loaderLock) {
							injectedSecurityFrameClass = probeLoaderToSecurityFrameMap(rawLoader);
							if (injectedSecurityFrameClass == null) {
								injectedSecurityFrameClass = injectSecurityFrameIntoLoader(rawLoader, context.getProtectionDomain());
								LoaderToSecurityFrameClassMap.put(rawLoader, new WeakReference<Class<?>>(injectedSecurityFrameClass));
							}
						}
					}
										
					try {
						Constructor<?> constructor = injectedSecurityFrameClass.getConstructor(MethodHandle.class, Class.class);
						constructor.setAccessible(true);
						return constructor.newInstance(tempFinalHandle, context);
					} catch (SecurityException | ReflectiveOperationException e) {
						throw new Error(e);								
					}
				}
			});
			handle = Lookup.internalPrivilegedLookup.bind(o, "invoke", methodType(Object.class, Object[].class)); //$NON-NLS-1$
			handle = handle.asType(originalType);
			if (isVarargs) {
				handle = handle.asVarargsCollector(originalType.lastParameterType());
			}
		} catch (IllegalAccessException | NoSuchMethodException e) {
			throw new Error(e);
		}
		return handle;
	}
	
	/*
	 * Helper method to wrap a MethodHandle with an injected SecurityFrame if required.  The resulting MH chain will be:
	 * AsType(type) -> ReceiverBoundHandle(Object[]) --invokeWArgs-> original handle
	 * VarargsCollector nature will be preserved.
	 */
	static MethodHandle wrapHandleWithInjectedSecurityFrameIfRequired(Lookup lookup, MethodHandle handle) throws IllegalAccessException {
		if (isCallerSensitive(handle)) {
			if (lookup.isWeakenedLookup()) {
				/*[MSG "K0589", "Caller-sensitive method cannot be looked up using a restricted lookup object"]*/
				throw new IllegalAccessException(com.ibm.oti.util.Msg.getString("K0589")); //$NON-NLS-1$
			}
			handle = wrapHandleWithInjectedSecurityFrame(handle, lookup.accessClass);
		}
		return handle;
	}
	
	private static final int CALLER_SENSITIVE_BIT = 0x100000;
	/*
	 * Determine if a method is caller-sensitive based on whether it has the 
	 * sun.reflect.CallerSensitive annotation.
	 */
	static boolean isCallerSensitive(MethodHandle mh) {
		return CALLER_SENSITIVE_BIT == (mh.getModifiers() & CALLER_SENSITIVE_BIT);
	}
	
	/*
	 * Attempt to unwrap a MethodHandle that has been wrapped in a SecurityFrame.
	 * 
	 * @param potentialInjectFrame The wrapped MethodHandle
	 * @param lookup A Lookup object with the same lookup class as the Lookup object used to create the MethodHandle
	 * 
	 * @return The unwrapped MethodHandle if successfully unwrapped, null if not.
	 */
	static MethodHandle penetrateSecurityFrame(MethodHandle potentialInjectFrame, Lookup lookup) {
		final MethodType originalMT = potentialInjectFrame.type;
		
		// SecurityFrames are always represented as:
		// 1) VarargsCollectHandle -> AsTypeHandle -> RBH with bound value being an instance of SecurityFrame
		// 2) AsTypeHandle -> RBH with bound value being an instance of SecurityFrame
		// 3) RBH with bound value being an instance of SecurityFrame if signature is (Object[])Object
		boolean mustBeVarags = false;
		if (potentialInjectFrame.kind == MethodHandle.KIND_VARARGSCOLLECT) {
			mustBeVarags = true;
			potentialInjectFrame = ((VarargsCollectorHandle)potentialInjectFrame).next;
		}
		if (potentialInjectFrame.kind == MethodHandle.KIND_ASTYPE) {
			potentialInjectFrame = ((AsTypeHandle)potentialInjectFrame).next;
		}
		if (potentialInjectFrame.kind == MethodHandle.KIND_FILTERRETURN) {
			potentialInjectFrame = ((FilterReturnHandle)potentialInjectFrame).next;
		}
		if (potentialInjectFrame.kind == MethodHandle.KIND_COLLECT) {
			potentialInjectFrame = ((CollectHandle)potentialInjectFrame).next;
		}
		if (potentialInjectFrame.kind == MethodHandle.KIND_ASTYPE) {
			potentialInjectFrame = ((AsTypeHandle)potentialInjectFrame).next;
		}
		if (potentialInjectFrame.kind == MethodHandle.KIND_FILTERRETURN) {
			potentialInjectFrame = ((FilterReturnHandle)potentialInjectFrame).next;
		}
		
		if (potentialInjectFrame.kind == MethodHandle.KIND_BOUND) {
			ReceiverBoundHandle rbh = (ReceiverBoundHandle)potentialInjectFrame;
			final Object receiver = rbh.receiver;
			
			VMLangAccess vma = Lookup.getVMLangAccess();
			ClassLoader rawLoader = vma.getClassloader(receiver.getClass());
			Class<?> injectedSecurityFrame = null;
			synchronized (loaderLock) {
				injectedSecurityFrame = probeLoaderToSecurityFrameMap(rawLoader);
			}
			if ((injectedSecurityFrame == null) || !injectedSecurityFrame.isInstance(receiver)) {
				/* receiver object cannot be an instance of SecurityFrame as its classloader 
				 * doesn't have an injected security frame class
				 */
				return null;
			}
			final Class<?> finalInjectedSecurityFrame = injectedSecurityFrame;
			MethodHandle target = AccessController.doPrivileged(new PrivilegedAction<MethodHandle>() {
				public MethodHandle run() {
					try {
						Field targetField = finalInjectedSecurityFrame.getDeclaredField("target"); //$NON-NLS-1$
						targetField.setAccessible(true);
						return (MethodHandle)targetField.get(receiver);
					} catch (NoSuchFieldException | SecurityException | IllegalArgumentException | IllegalAccessException e) {
						throw (InternalError)new InternalError().initCause(e);
					}
				}
			});
			Class<?> targetAccessClass = AccessController.doPrivileged(new PrivilegedAction<Class<?>>() {
				public Class<?> run() {
					try {
						Field targetField = finalInjectedSecurityFrame.getDeclaredField("accessClass"); //$NON-NLS-1$
						targetField.setAccessible(true);
						return (Class<?>)targetField.get(receiver);
					} catch (NoSuchFieldException | SecurityException | IllegalArgumentException | IllegalAccessException e) {
						throw (InternalError)new InternalError().initCause(e);
					}
				}
			});
			if (lookup.accessMode != MethodHandles.Lookup.INTERNAL_PRIVILEGED) {
				if (targetAccessClass != lookup.accessClass) {
					return null;
				}
			}
			if (target.type == originalMT) {
				if ((target instanceof PrimitiveHandle) && (Lookup.isVarargs(target.getModifiers()) == mustBeVarags)) {
					return target;
				}
			}
		}
		// Not a directHandle - someone added wrapping to it.
		return null;
	}
	
}

