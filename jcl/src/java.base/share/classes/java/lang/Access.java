/*[INCLUDE-IF JAVA_SPEC_VERSION >= 8]*/
/*
 * Copyright IBM Corp. and others 2007
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package java.lang;

import java.lang.annotation.Annotation;
/*[IF JAVA_SPEC_VERSION >= 15]*/
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;
import jdk.internal.misc.Unsafe;
import java.lang.StringConcatHelper;
/*[ENDIF] JAVA_SPEC_VERSION >= 15 */
import java.lang.reflect.Constructor;
import java.lang.reflect.Executable;
import java.lang.reflect.Method;
import java.security.AccessControlContext;
import java.util.Map;

import com.ibm.oti.reflect.AnnotationParser;
import com.ibm.oti.reflect.TypeAnnotationParser;

/*[IF Sidecar19-SE]*/
/*[IF JAVA_SPEC_VERSION >= 20]*/
import java.io.InputStream;
/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
import java.io.IOException;
import java.lang.module.ModuleDescriptor;
import java.net.URL;
import java.net.URI;
import java.security.ProtectionDomain;
import java.util.Iterator;
import java.util.List;
/*[IF JAVA_SPEC_VERSION >= 15]*/
import java.util.Set;
/*[ENDIF] JAVA_SPEC_VERSION >= 15 */
import java.util.concurrent.ConcurrentHashMap;
import java.util.stream.Stream;
/*[IF JAVA_SPEC_VERSION >= 11]*/
import java.nio.charset.Charset;
import java.nio.charset.CharacterCodingException;
/*[ENDIF] JAVA_SPEC_VERSION >= 11 */
/*[IF JAVA_SPEC_VERSION >= 12]*/
import jdk.internal.access.JavaLangAccess;
/*[ELSE] JAVA_SPEC_VERSION >= 12 */
import jdk.internal.misc.JavaLangAccess;
/*[ENDIF] JAVA_SPEC_VERSION >= 12 */
import jdk.internal.module.ServicesCatalog;
import jdk.internal.reflect.ConstantPool;
/*[IF JAVA_SPEC_VERSION >= 19]*/
import java.util.concurrent.Callable;
import jdk.internal.misc.CarrierThreadLocal;
import jdk.internal.vm.Continuation;
import jdk.internal.vm.ContinuationScope;
import jdk.internal.vm.StackableScope;
import jdk.internal.vm.ThreadContainer;
/*[ENDIF] JAVA_SPEC_VERSION >= 19 */
/*[IF JAVA_SPEC_VERSION >= 20]*/
import jdk.internal.misc.CarrierThreadLocal;
/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
/*[ELSE] Sidecar19-SE */
import sun.misc.JavaLangAccess;
import sun.reflect.ConstantPool;
/*[ENDIF] Sidecar19-SE */
import sun.nio.ch.Interruptible;
import sun.reflect.annotation.AnnotationType;

/**
 * Helper class to allow privileged access to classes
 * from outside the java.lang package. The sun.misc.SharedSecrets class
 * uses an instance of this class to access private java.lang members.
 */
final class Access implements JavaLangAccess {

	/** Set thread's blocker field. */
	public void blockedOn(java.lang.Thread thread, Interruptible interruptable) {
		/*[IF JAVA_SPEC_VERSION >= 11]*/
		Thread.blockedOn(interruptable);
		/*[ELSE] JAVA_SPEC_VERSION >= 11 */
		thread.blockedOn(interruptable);
		/*[ENDIF] JAVA_SPEC_VERSION >= 11 */
	}

	/**
	 * Get the AnnotationType instance corresponding to this class.
	 * (This method only applies to annotation types.)
	 */
	public AnnotationType getAnnotationType(java.lang.Class<?> arg0) {
		return arg0.getAnnotationType();
	}

	/** Return the constant pool for a class. */
	public native ConstantPool getConstantPool(java.lang.Class<?> arg0);

	/**
	 * Return the constant pool for a class. This will call the exact same
	 * native as 'getConstantPool(java.lang.Class<?> arg0)'. We need this
	 * version so we can call it with InternRamClass instead of j.l.Class
	 * (which is required by JAVALANGACCESS).
	 */
	public static native ConstantPool getConstantPool(Object arg0);

	/**
	 * Returns the elements of an enum class or null if the
	 * Class object does not represent an enum type;
	 * the result is uncloned, cached, and shared by all callers.
	 */
	public <E extends Enum<E>> E[] getEnumConstantsShared(java.lang.Class<E> arg0) {
		/*[PR CMVC 189091] Perf: EnumSet.allOf() is slow */
		return arg0.getEnumConstantsShared();
	}

	public void registerShutdownHook(int arg0, boolean arg1, Runnable arg2) {
		Shutdown.add(arg0, arg1, arg2);
	}

	/**
	 * Set the AnnotationType instance corresponding to this class.
	 * (This method only applies to annotation types.)
	 */
	public void setAnnotationType(java.lang.Class<?> arg0, AnnotationType arg1) {
		arg0.setAnnotationType(arg1);
	}

	/**
	 * Compare-And-Swap the AnnotationType instance corresponding to this class.
	 * (This method only applies to annotation types.)
	 */
	public boolean casAnnotationType(final Class<?> clazz, AnnotationType oldType, AnnotationType newType) {
		return clazz.casAnnotationType(oldType, newType);
	}

	/**
	 * @param clazz Class object
	 * @return the array of bytes for the Annotations for clazz, or null if clazz is null
	 */
	public byte[] getRawClassAnnotations(Class<?> clazz) {
		return AnnotationParser.getAnnotationsData(clazz);
	}

	public int getStackTraceDepth(java.lang.Throwable arg0) {
		return arg0.getInternalStackTrace().length;
	}

	public java.lang.StackTraceElement getStackTraceElement(java.lang.Throwable arg0, int arg1) {
		return arg0.getInternalStackTrace()[arg1];
	}

	/*[PR CMVC 199693] Prevent trusted method chain attack. */
	@SuppressWarnings("removal")
	public Thread newThreadWithAcc(Runnable runnable, AccessControlContext acc) {
		return new Thread(runnable, acc);
	}

	/**
	 * Returns a directly present annotation instance of annotationClass type from clazz.
	 *
	 * @param clazz Class that will be searched for given annotationClass
	 * @param annotationClass annotation class that is being searched on the clazz declaration
	 *
	 * @return declared annotation of annotationClass type for clazz, otherwise return null
	 */
	public <A extends Annotation> A getDirectDeclaredAnnotation(Class<?> clazz, Class<A> annotationClass) {
		return clazz.getDeclaredAnnotation(annotationClass);
	}

	@Override
	public Map<java.lang.Class<? extends Annotation>, Annotation> getDeclaredAnnotationMap(
			java.lang.Class<?> arg0) {
		throw new Error("getDeclaredAnnotationMap unimplemented"); //$NON-NLS-1$
	}

	@Override
	public byte[] getRawClassTypeAnnotations(java.lang.Class<?> clazz) {
		return TypeAnnotationParser.getTypeAnnotationsData(clazz);
	}

	@Override
	public byte[] getRawExecutableTypeAnnotations(Executable exec) {
		byte[] result = null;
		if (Method.class.isInstance(exec)) {
			Method jlrMethod = (Method) exec;
			result = TypeAnnotationParser.getTypeAnnotationsData(jlrMethod);
		} else if (Constructor.class.isInstance(exec)) {
			Constructor<?> jlrConstructor = (Constructor<?>) exec;
			result = TypeAnnotationParser.getTypeAnnotationsData(jlrConstructor);
		} else {
			throw new Error("getRawExecutableTypeAnnotations not defined for "+exec.getClass().getName()); //$NON-NLS-1$
		}
		return result;
	}

	/*[IF JAVA_SPEC_VERSION < 10]*/
	/**
	 * Return a newly created String that uses the passed in char[]
	 * without copying. The array must not be modified after creating
	 * the String.
	 *
	 * @param data The char[] for the String
	 * @return a new String using the char[].
	 */
	@Override
	/*[PR Jazz 87855] Implement j.l.Access.newStringUnsafe()]*/
	public java.lang.String newStringUnsafe(char[] data) {
		return new String(data, true /*ignored*/);
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 10 */

	@Override
	public void invokeFinalize(java.lang.Object arg0)
			throws java.lang.Throwable {
		/*
		 * Not required for J9.
		 */
		throw new Error("invokeFinalize unimplemented"); //$NON-NLS-1$
	}

/*[IF Sidecar19-SE]*/
	public Class<?> findBootstrapClassOrNull(ClassLoader classLoader, String name) {
		return VMAccess.findClassOrNull(name, ClassLoader.bootstrapClassLoader);
	}

	public ModuleLayer getBootLayer() {
		return System.bootLayer;
	}

	public ServicesCatalog createOrGetServicesCatalog(ClassLoader classLoader) {
		return classLoader.createOrGetServicesCatalog();
	}

/*[IF JAVA_SPEC_VERSION < 10]*/
	@Deprecated
	public ServicesCatalog getServicesCatalog(ClassLoader classLoader) {
		return classLoader.getServicesCatalog();
	}
/*[ENDIF] JAVA_SPEC_VERSION < 10 */

/*[IF JAVA_SPEC_VERSION < 22]*/
	public String fastUUID(long param1, long param2) {
		return Long.fastUUID(param1, param2);
	}
/*[ENDIF] JAVA_SPEC_VERSION < 22 */

	public Package definePackage(ClassLoader classLoader, String name, Module module) {
		if (null == classLoader) {
			classLoader = ClassLoader.bootstrapClassLoader;
		}
		return classLoader.definePackage(name, module);
	}

	public URL findResource(ClassLoader classLoader, String moduleName, String name) throws IOException {
		if (null == classLoader) {
			classLoader = ClassLoader.bootstrapClassLoader;
		}
		return classLoader.findResource(moduleName, name);
	}

	public Stream<Package> packages(ClassLoader classLoader) {
		if (null == classLoader) {
			classLoader = ClassLoader.bootstrapClassLoader;
		}
		return classLoader.packages();
	}

	public ConcurrentHashMap<?, ?> createOrGetClassLoaderValueMap(
			java.lang.ClassLoader classLoader) {
		return classLoader.createOrGetClassLoaderValueMap();
	}

	public Method getMethodOrNull(Class<?> clz, String name, Class<?>... parameterTypes) {
		try {
			return clz.getMethodHelper(false, false, null, name, parameterTypes);
		} catch (NoSuchMethodException ex) {
			return null;
		}
	}

	@SuppressWarnings("removal")
	public void invalidatePackageAccessCache() {
/*[IF JAVA_SPEC_VERSION >= 10]*/
		java.lang.SecurityManager.invalidatePackageAccessCache();
/*[ELSE] JAVA_SPEC_VERSION >= 10 */
		return;
/*[ENDIF] JAVA_SPEC_VERSION >= 10 */
	}

	public Class<?> defineClass(ClassLoader classLoader, String className, byte[] classRep, ProtectionDomain protectionDomain, String str) {
		ClassLoader targetClassLoader = (null == classLoader) ? ClassLoader.bootstrapClassLoader : classLoader;
		return targetClassLoader.defineClassInternal(className, classRep, 0, classRep.length, protectionDomain, true /* allowNullProtectionDomain */);
	}

/*[IF Sidecar19-SE-OpenJ9]*/
	public Stream<ModuleLayer> layers(ModuleLayer ml) {
		return ml.layers();
	}

	public Stream<ModuleLayer> layers(ClassLoader cl) {
		return ModuleLayer.layers(cl);
	}

	public Module defineModule(ClassLoader cl, ModuleDescriptor md, URI uri) {
		return new Module(System.bootLayer, cl, md, uri);
	}

	public Module defineUnnamedModule(ClassLoader cl) {
		return new Module(cl);
	}

	public void addReads(Module fromModule, Module toModule) {
		fromModule.implAddReads(toModule);
	}

	public void addReadsAllUnnamed(Module module) {
		module.implAddReadsAllUnnamed();
	}

	public void addExports(Module fromModule, String pkg, Module toModule) {
		fromModule.implAddExports(pkg, toModule);
	}

	public void addExportsToAllUnnamed(Module fromModule, String pkg) {
		fromModule.implAddExportsToAllUnnamed(pkg);
	}

	public void addOpens(Module fromModule, String pkg, Module toModule) {
		fromModule.implAddOpens(pkg, toModule);
	}

	public void addOpensToAllUnnamed(Module fromModule, String pkg) {
		fromModule.implAddOpensToAllUnnamed(pkg);
	}

	public void addUses(Module fromModule, Class<?> service) {
		fromModule.implAddUses(service);
	}
	public ServicesCatalog getServicesCatalog(ModuleLayer ml) {
		return ml.getServicesCatalog();
	}

	@SuppressWarnings("removal")
	public void addNonExportedPackages(ModuleLayer ml) {
		SecurityManager.addNonExportedPackages(ml);
	}

	public List<Method> getDeclaredPublicMethods(Class<?> clz, String name, Class<?>... types) {
		return clz.getDeclaredPublicMethods(name, types);
	}

	/*[IF JAVA_SPEC_VERSION >= 15]*/
	public void addOpensToAllUnnamed(Module fromModule, Set<String> concealedPackages, Set<String> exportedPackages) {
		fromModule.implAddOpensToAllUnnamed(concealedPackages, exportedPackages);
	}
	/*[ELSE] JAVA_SPEC_VERSION >= 15 */
	public void addOpensToAllUnnamed(Module fromModule, Iterator<String> packages) {
		fromModule.implAddOpensToAllUnnamed(packages);
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 15 */

	public boolean isReflectivelyOpened(Module fromModule, String pkg, Module toModule) {
		return fromModule.isReflectivelyOpened(pkg, toModule);
	}

	public boolean isReflectivelyExported(Module fromModule, String pkg, Module toModule) {
		return fromModule.isReflectivelyExported(pkg, toModule);
	}
/*[ENDIF] Sidecar19-SE-OpenJ9 */

/*[IF JAVA_SPEC_VERSION >= 10]*/
	public String newStringUTF8NoRepl(byte[] bytes, int offset, int length) {
		/*[IF JAVA_SPEC_VERSION < 17]*/
		return StringCoding.newStringUTF8NoRepl(bytes, offset, length);
		/*[ELSE] JAVA_SPEC_VERSION < 17 */
		/*[IF JAVA_SPEC_VERSION < 21]*/
		return String.newStringUTF8NoRepl(bytes, offset, length);
		/*[ELSE] JAVA_SPEC_VERSION < 21 */
		return String.newStringUTF8NoRepl(bytes, offset, length, true);
		/*[ENDIF] JAVA_SPEC_VERSION < 21 */
		/*[ENDIF] JAVA_SPEC_VERSION < 17 */
	}
	public byte[] getBytesUTF8NoRepl(String str) {
		/*[IF JAVA_SPEC_VERSION < 17]*/
		return StringCoding.getBytesUTF8NoRepl(str);
		/*[ELSE] JAVA_SPEC_VERSION < 17 */
		return String.getBytesUTF8NoRepl(str);
		/*[ENDIF] JAVA_SPEC_VERSION < 17 */
	}
/*[ENDIF] JAVA_SPEC_VERSION >= 10 */

/*[IF JAVA_SPEC_VERSION >= 11]*/
	public void blockedOn(Interruptible interruptible) {
		Thread.blockedOn(interruptible);
	}
	public byte[] getBytesNoRepl(String str, Charset charset) throws CharacterCodingException {
		/*[IF JAVA_SPEC_VERSION < 17]*/
		return StringCoding.getBytesNoRepl(str, charset);
		/*[ELSE] JAVA_SPEC_VERSION < 17 */
		return String.getBytesNoRepl(str, charset);
		/*[ENDIF] JAVA_SPEC_VERSION < 17 */
	}
	public String newStringNoRepl(byte[] bytes, Charset charset) throws CharacterCodingException {
		/*[IF JAVA_SPEC_VERSION < 17]*/
		return StringCoding.newStringNoRepl(bytes, charset);
		/*[ELSE] JAVA_SPEC_VERSION < 17 */
		return String.newStringNoRepl(bytes, charset);
		/*[ENDIF] JAVA_SPEC_VERSION < 17 */
	}
/*[ENDIF] JAVA_SPEC_VERSION >= 11 */

/*[IF JAVA_SPEC_VERSION >= 12]*/
	public void setCause(Throwable throwable, Throwable cause) {
		throwable.setCause(cause);
	}
/*[ENDIF] JAVA_SPEC_VERSION >= 12 */

/*[IF JAVA_SPEC_VERSION >= 14]*/
	public void loadLibrary(Class<?> caller, String library) {
		System.loadLibrary(library);
	}
/*[ENDIF] JAVA_SPEC_VERSION >= 14 */

/*[IF JAVA_SPEC_VERSION >= 15]*/
	public Class<?> defineClass(ClassLoader classLoader, Class<?> clazz, String className, byte[] classRep, ProtectionDomain protectionDomain, boolean init, int flags, Object classData) {
		ClassLoader targetClassLoader = (null == classLoader) ? ClassLoader.bootstrapClassLoader : classLoader;
		return targetClassLoader.defineClassInternal(clazz, className, classRep, protectionDomain, init, flags, classData);
	}

	/**
	 * Returns the classData stored in the class.
	 *
	 * @param the class from where to retrieve the classData.
	 *
	 * @return the classData (Object).
	 */
	public Object classData(Class<?> clazz) {
		return clazz.getClassData();
	}

	public ProtectionDomain protectionDomain(Class<?> clazz) {
		return clazz.getProtectionDomainInternal();
	}

	public MethodHandle stringConcatHelper(String arg0, MethodType type) {
		return StringConcatHelper.lookupStatic(arg0, type);
	}

	public long stringConcatInitialCoder() {
		return StringConcatHelper.initialCoder();
	}

	public long stringConcatMix(long arg0, String string) {
		return StringConcatHelper.mix(arg0, string);
	}
/*[ENDIF] JAVA_SPEC_VERSION >= 15 */

/*[IF JAVA_SPEC_VERSION >= 16]*/
	public void bindToLoader(ModuleLayer ml, ClassLoader cl) {
		ml.bindToLoader(cl);
	}

	public void addExports(Module fromModule, String pkg) {
		fromModule.implAddExports(pkg);
	}
/*[ENDIF] JAVA_SPEC_VERSION >= 16 */

/*[IF JAVA_SPEC_VERSION >= 17]*/
	public int decodeASCII(byte[] srcBytes, int srcPos, char[] dstChars, int dstPos, int length) {
		return String.decodeASCII(srcBytes, srcPos, dstChars, dstPos, length);
	}

	public void inflateBytesToChars(byte[] srcBytes, int srcOffset, char[] dstChars, int dstOffset, int length) {
		StringLatin1.inflate(srcBytes, srcOffset, dstChars, dstOffset, length);
	}

	// The method findBootstrapClassOrNull(ClassLoader classLoader, String name) can be removed
	// after following API change is promoted into extension repo openj9 branch.
	public Class<?> findBootstrapClassOrNull(String name) {
		return VMAccess.findClassOrNull(name, ClassLoader.bootstrapClassLoader);
	}

	public String join(String prefix, String suffix, String delimiter, String[] elements, int size) {
		return String.join(prefix, suffix, delimiter, elements, size);
	}

/*[IF JAVA_SPEC_VERSION >= 20]*/
	public void ensureNativeAccess(Module mod, Class<?> clsOwner, String methodName) {
		mod.ensureNativeAccess(clsOwner, methodName);
	}

	public void addEnableNativeAccessToAllUnnamed() {
		Module.implAddEnableNativeAccessToAllUnnamed();
	}
/*[ELSE] JAVA_SPEC_VERSION >= 20 */
	public boolean isEnableNativeAccess(Module mod) {
		return mod.implIsEnableNativeAccess();
	}

	public void addEnableNativeAccessAllUnnamed() {
		Module.implAddEnableNativeAccessAllUnnamed();
	}
/*[ENDIF] JAVA_SPEC_VERSION >= 20 */

	public Module addEnableNativeAccess(Module mod) {
		return mod.implAddEnableNativeAccess();
	}

	public long findNative(ClassLoader loader, String entryName) {
		return ClassLoader.findNative(loader, entryName);
	}

	@Override
	public void exit(int status) {
		Shutdown.exit(status);
	}

	public int encodeASCII(char[] sa, int sp, byte[] da, int dp, int len) {
		return StringCoding.implEncodeAsciiArray(sa, sp, da, dp, len);
	}
/*[ENDIF] JAVA_SPEC_VERSION >= 17 */

/*[IF JAVA_SPEC_VERSION >= 19]*/
	public Thread currentCarrierThread() {
		return Thread.currentCarrierThread();
	}

	public <V> V executeOnCarrierThread(Callable<V> task) throws Exception {
		V result;
		Thread currentThread = Thread.currentThread();
		if (currentThread.isVirtual()) {
			Thread carrierThread = Thread.currentCarrierThread();
			carrierThread.setCurrentThread(carrierThread);
			try {
				result = task.call();
			} finally {
				carrierThread.setCurrentThread(currentThread);
			}
		} else {
			result = task.call();
		}
		return result;
	}

	public Continuation getContinuation(Thread thread) {
		return thread.getContinuation();
	}

	public void setContinuation(Thread thread, Continuation c) {
		thread.setContinuation(c);
	}

/*[IF JAVA_SPEC_VERSION >= 20]*/
	public Object[] scopedValueCache() {
		return Thread.scopedValueCache();
	}

	public void setScopedValueCache(Object[] cache) {
		Thread.setScopedValueCache(cache);
	}

	public Object scopedValueBindings() {
		return Thread.scopedValueBindings();
	}

	public Object findScopedValueBindings() {
		return Thread.findScopedValueBindings();
	}

	public void setScopedValueBindings(Object scopeValueBindings) {
		Thread.setScopedValueBindings(scopeValueBindings);
	}

	public void ensureMaterializedForStackWalk(Object obj) {
		Thread.ensureMaterializedForStackWalk(obj);
	}

	public InputStream initialSystemIn() {
		return System.initialIn;
	}

/*[IF JAVA_SPEC_VERSION >= 21]*/
	public char getUTF16Char(byte[] val, int index) {
		return StringUTF16.getChar(val, index);
	}

	public int countPositives(byte[] ba, int off, int len) {
		return StringCoding.countPositives(ba, off, len);
	}

	public long stringConcatCoder(char value) {
		return StringConcatHelper.coder(value);
	}

	public long stringBuilderConcatMix(long lengthCoder, StringBuilder sb) {
		return sb.mix(lengthCoder);
	}

	public long stringBuilderConcatPrepend(long lengthCoder, byte[] buf, StringBuilder sb) {
		return sb.prepend(lengthCoder, buf);
	}
/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
/*[ELSE] JAVA_SPEC_VERSION >= 20 */
	public Object[] extentLocalCache() {
		return Thread.extentLocalCache();
	}

	public void setExtentLocalCache(Object[] cache) {
		Thread.setExtentLocalCache(cache);
	}

	public Object extentLocalBindings() {
		return Thread.extentLocalBindings();
	}

	public void setExtentLocalBindings(Object bindings) {
		Thread.setExtentLocalBindings(bindings);
	}
/*[ENDIF] JAVA_SPEC_VERSION >= 20 */

	public StackableScope headStackableScope(Thread thread) {
		return thread.headStackableScopes();
	}

	public void setHeadStackableScope(StackableScope scope) {
		Thread.setHeadStackableScope(scope);
	}

	public ThreadContainer threadContainer(Thread thread) {
		return thread.threadContainer();
	}

	public void start(Thread thread, ThreadContainer container) {
		thread.start(container);
	}

	public Thread[] getAllThreads() {
		return Thread.getAllThreads();
	}

	public ContinuationScope virtualThreadContinuationScope() {
		return VirtualThread.continuationScope();
	}

	public void parkVirtualThread() {
		if (Thread.currentThread() instanceof BaseVirtualThread bvt) {
			bvt.park();
		} else {
			throw new WrongThreadException();
		}
	}

	public void parkVirtualThread(long nanos) {
		if (Thread.currentThread() instanceof BaseVirtualThread bvt) {
			bvt.parkNanos(nanos);
		} else {
			throw new WrongThreadException();
		}
	}

	public void unparkVirtualThread(Thread thread) {
		if (thread instanceof BaseVirtualThread bvt) {
			bvt.unpark();
		} else {
			throw new WrongThreadException();
		}
	}

	public StackWalker newStackWalkerInstance(Set<StackWalker.Option> options, ContinuationScope contScope, Continuation continuation) {
		return StackWalker.newInstance(options, null, contScope, continuation);
	}

	/*
	 * To access package-private methods in ThreadLocal, an
	 * (implicit) cast from CarrierThreadLocal is required.
	 */
	private static <T> ThreadLocal<T> asThreadLocal(CarrierThreadLocal<T> local) {
		return local;
	}

	public boolean isCarrierThreadLocalPresent(CarrierThreadLocal<?> carrierThreadlocal) {
		return asThreadLocal(carrierThreadlocal).isCarrierThreadLocalPresent();
	}

	public <T> T getCarrierThreadLocal(CarrierThreadLocal<T> carrierThreadlocal) {
		return asThreadLocal(carrierThreadlocal).getCarrierThreadLocal();
	}

	public void removeCarrierThreadLocal(CarrierThreadLocal<?> carrierThreadlocal) {
		asThreadLocal(carrierThreadlocal).removeCarrierThreadLocal();
	}

	public <T> void setCarrierThreadLocal(CarrierThreadLocal<T> carrierThreadlocal, T carrierThreadLocalvalue) {
		asThreadLocal(carrierThreadlocal).setCarrierThreadLocal(carrierThreadLocalvalue);
	}
/*[ENDIF] JAVA_SPEC_VERSION >= 19 */

/*[IF (JAVA_SPEC_VERSION >= 11) & (JAVA_SPEC_VERSION != 20)]*/
	public String getLoaderNameID(ClassLoader loader) {
		StringBuilder buffer = new StringBuilder();
		String name = loader.getName();

		if (name != null) {
			buffer.append("'").append(name).append("'");
		} else {
			buffer.append(loader.getClass().getName());
		}

		if (false == loader instanceof jdk.internal.loader.BuiltinClassLoader) {
			buffer.append(" @").append(Integer.toHexString(System.identityHashCode(loader)));
		}

		return buffer.toString();
	}
/*[ENDIF] (JAVA_SPEC_VERSION >= 11) & (JAVA_SPEC_VERSION != 20) */

/*[IF INLINE-TYPES]*/
	@Override
	public boolean isPrimitiveClass(Class<?> c) {
		return c.isPrimitiveClass();
	}

	@Override
	public Class<?> asPrimaryType(Class<?> c) {
		return c.asPrimaryType();
	}

	@Override
	public Class<?> asValueType(Class<?> c) {
		return c.asValueType();
	}

	@Override
	public boolean isPrimaryType(Class<?> c) {
		return c.isPrimaryType();
	}

	@Override
	public boolean isPrimitiveValueType(Class<?> c) {
		return c.isPrimitiveValueType();
	}

	@Override
	public int classFileFormatVersion(Class<?> c) {
		return c.getClassFileVersion();
	}
/*[ENDIF] INLINE-TYPES */

/*[ENDIF] Sidecar19-SE */
}
