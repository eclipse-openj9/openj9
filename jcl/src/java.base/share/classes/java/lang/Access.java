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
/*[IF JAVA_SPEC_VERSION >= 22]*/
import java.lang.foreign.MemorySegment;
/*[ENDIF] JAVA_SPEC_VERSION >= 22 */
/*[IF JAVA_SPEC_VERSION >= 15]*/
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;
import jdk.internal.misc.Unsafe;
import java.lang.StringConcatHelper;
/*[ENDIF] JAVA_SPEC_VERSION >= 15 */
import java.lang.reflect.Constructor;
import java.lang.reflect.Executable;
import java.lang.reflect.Method;
import java.util.Map;

import com.ibm.oti.reflect.AnnotationParser;
import com.ibm.oti.reflect.TypeAnnotationParser;

/*[IF JAVA_SPEC_VERSION >= 9]*/
/*[IF JAVA_SPEC_VERSION >= 20]*/
import java.io.InputStream;
/*[ENDIF] JAVA_SPEC_VERSION >= 20 */
import java.io.IOException;
/*[IF JAVA_SPEC_VERSION >= 23]*/
import java.io.PrintStream;
/*[ENDIF] JAVA_SPEC_VERSION >= 23 */
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
/*[ELSE] JAVA_SPEC_VERSION >= 9 */
import sun.misc.JavaLangAccess;
import sun.reflect.ConstantPool;
/*[ENDIF] JAVA_SPEC_VERSION >= 9 */
import sun.nio.ch.Interruptible;
import sun.reflect.annotation.AnnotationType;
/*[IF JAVA_SPEC_VERSION >= 24]*/
import java.util.concurrent.Executor;
import java.util.concurrent.ScheduledExecutorService;
import jdk.internal.loader.NativeLibraries;
/*[ENDIF] JAVA_SPEC_VERSION >= 24 */

/**
 * Helper class to allow privileged access to classes
 * from outside the java.lang package. The sun.misc.SharedSecrets class
 * uses an instance of this class to access private java.lang members.
 */
final class Access implements JavaLangAccess {

	/*[IF JAVA_SPEC_VERSION == 8]*/
	/** Set thread's blocker field. */
	@Override
	public void blockedOn(java.lang.Thread thread, Interruptible interruptable) {
		thread.blockedOn(interruptable);
	}
	/*[ENDIF] JAVA_SPEC_VERSION == 8 */

	/**
	 * Get the AnnotationType instance corresponding to this class.
	 * (This method only applies to annotation types.)
	 */
	@Override
	public AnnotationType getAnnotationType(java.lang.Class<?> arg0) {
		return arg0.getAnnotationType();
	}

	/** Return the constant pool for a class. */
	@Override
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
	@Override
	public <E extends Enum<E>> E[] getEnumConstantsShared(java.lang.Class<E> arg0) {
		/*[PR CMVC 189091] Perf: EnumSet.allOf() is slow */
		return arg0.getEnumConstantsShared();
	}

	@Override
	public void registerShutdownHook(int arg0, boolean arg1, Runnable arg2) {
		Shutdown.add(arg0, arg1, arg2);
	}

	/**
	 * Compare-And-Swap the AnnotationType instance corresponding to this class.
	 * (This method only applies to annotation types.)
	 */
	@Override
	public boolean casAnnotationType(final Class<?> clazz, AnnotationType oldType, AnnotationType newType) {
		return clazz.casAnnotationType(oldType, newType);
	}

	/**
	 * @param clazz Class object
	 * @return the array of bytes for the Annotations for clazz, or null if clazz is null
	 */
	@Override
	public byte[] getRawClassAnnotations(Class<?> clazz) {
		return AnnotationParser.getAnnotationsData(clazz);
	}

	/*[IF JAVA_SPEC_VERSION == 8]*/
	@Override
	public int getStackTraceDepth(java.lang.Throwable arg0) {
		return arg0.getInternalStackTrace().length;
	}

	@Override
	public java.lang.StackTraceElement getStackTraceElement(java.lang.Throwable arg0, int arg1) {
		return arg0.getInternalStackTrace()[arg1];
	}
	/*[ENDIF] JAVA_SPEC_VERSION == 8 */

	/*[IF JAVA_SPEC_VERSION < 24]*/
	/*[PR CMVC 199693] Prevent trusted method chain attack. */
	@SuppressWarnings("removal")
	@Override
	public Thread newThreadWithAcc(Runnable runnable, java.security.AccessControlContext acc) {
		return new Thread(runnable, acc);
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */

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

	/*[IF JAVA_SPEC_VERSION >= 9]*/
	/*[IF JAVA_SPEC_VERSION == 11]*/
	@Override
	public Class<?> findBootstrapClassOrNull(ClassLoader classLoader, String name) {
		return VMAccess.findClassOrNull(name, ClassLoader.bootstrapClassLoader);
	}
	/*[ENDIF] JAVA_SPEC_VERSION == 11 */

	/*[IF JAVA_SPEC_VERSION < 22]*/
	@Override
	public String fastUUID(long param1, long param2) {
		return Long.fastUUID(param1, param2);
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 22 */

	@Override
	public Package definePackage(ClassLoader classLoader, String name, Module module) {
		if (null == classLoader) {
			classLoader = ClassLoader.bootstrapClassLoader;
		}
		return classLoader.definePackage(name, module);
	}

	@Override
	public ConcurrentHashMap<?, ?> createOrGetClassLoaderValueMap(
			java.lang.ClassLoader classLoader) {
		return classLoader.createOrGetClassLoaderValueMap();
	}

	/*[IF (11 <= JAVA_SPEC_VERSION) & (JAVA_SPEC_VERSION < 24)]*/
	@SuppressWarnings("removal")
	@Override
	public void invalidatePackageAccessCache() {
		SecurityManager.invalidatePackageAccessCache();
	}
	/*[ENDIF] (11 <= JAVA_SPEC_VERSION) & (JAVA_SPEC_VERSION < 24) */

	@Override
	public Class<?> defineClass(ClassLoader classLoader, String className, byte[] classRep, ProtectionDomain protectionDomain, String str) {
		ClassLoader targetClassLoader = (null == classLoader) ? ClassLoader.bootstrapClassLoader : classLoader;
		return targetClassLoader.defineClassInternal(className, classRep, 0, classRep.length, protectionDomain, true /* allowNullProtectionDomain */);
	}

	@Override
	public Stream<ModuleLayer> layers(ModuleLayer ml) {
		return ml.layers();
	}

	@Override
	public Stream<ModuleLayer> layers(ClassLoader cl) {
		return ModuleLayer.layers(cl);
	}

	@Override
	public Module defineModule(ClassLoader cl, ModuleDescriptor md, URI uri) {
		return new Module(System.bootLayer, cl, md, uri);
	}

	@Override
	public Module defineUnnamedModule(ClassLoader cl) {
		return new Module(cl);
	}

	@Override
	public void addReads(Module fromModule, Module toModule) {
		fromModule.implAddReads(toModule);
	}

	@Override
	public void addReadsAllUnnamed(Module module) {
		module.implAddReadsAllUnnamed();
	}

	@Override
	public void addExports(Module fromModule, String pkg, Module toModule) {
		fromModule.implAddExports(pkg, toModule);
	}

	@Override
	public void addExportsToAllUnnamed(Module fromModule, String pkg) {
		fromModule.implAddExportsToAllUnnamed(pkg);
	}

	@Override
	public void addOpens(Module fromModule, String pkg, Module toModule) {
		fromModule.implAddOpens(pkg, toModule);
	}

	@Override
	public void addOpensToAllUnnamed(Module fromModule, String pkg) {
		fromModule.implAddOpensToAllUnnamed(pkg);
	}

	@Override
	public void addUses(Module fromModule, Class<?> service) {
		fromModule.implAddUses(service);
	}

	@Override
	public ServicesCatalog getServicesCatalog(ModuleLayer ml) {
		return ml.getServicesCatalog();
	}

	/*[IF JAVA_SPEC_VERSION < 24]*/
	@SuppressWarnings("removal")
	@Override
	public void addNonExportedPackages(ModuleLayer ml) {
		SecurityManager.addNonExportedPackages(ml);
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */

	@Override
	public List<Method> getDeclaredPublicMethods(Class<?> clz, String name, Class<?>... types) {
		return clz.getDeclaredPublicMethods(name, types);
	}

	/*[IF JAVA_SPEC_VERSION < 15]*/
	@Override
	public void addOpensToAllUnnamed(Module fromModule, Iterator<String> packages) {
		fromModule.implAddOpensToAllUnnamed(packages);
	}
	/*[ELSEIF JAVA_SPEC_VERSION < 25]*/
	@Override
	public void addOpensToAllUnnamed(Module fromModule, Set<String> concealedPackages, Set<String> exportedPackages) {
		fromModule.implAddOpensToAllUnnamed(concealedPackages, exportedPackages);
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 15 */

	@Override
	public boolean isReflectivelyOpened(Module fromModule, String pkg, Module toModule) {
		return fromModule.isReflectivelyOpened(pkg, toModule);
	}

	@Override
	public boolean isReflectivelyExported(Module fromModule, String pkg, Module toModule) {
		return fromModule.isReflectivelyExported(pkg, toModule);
	}

	/*[IF (10 <= JAVA_SPEC_VERSION) & (JAVA_SPEC_VERSION < 26)]*/
	@Override
	public String newStringUTF8NoRepl(byte[] bytes, int offset, int length) {
		/*[IF JAVA_SPEC_VERSION < 17]*/
		return StringCoding.newStringUTF8NoRepl(bytes, offset, length);
		/*[ELSEIF JAVA_SPEC_VERSION < 21]*/
		return String.newStringUTF8NoRepl(bytes, offset, length);
		/*[ELSE] JAVA_SPEC_VERSION < 21 */
		return String.newStringUTF8NoRepl(bytes, offset, length, true);
		/*[ENDIF] JAVA_SPEC_VERSION < 17 */
	}
	/*[ENDIF] (10 <= JAVA_SPEC_VERSION) & (JAVA_SPEC_VERSION < 26) */

	/*[IF (10 <= JAVA_SPEC_VERSION) & (JAVA_SPEC_VERSION < 26)]*/
	@Override
	public byte[] getBytesUTF8NoRepl(String str) {
		/*[IF JAVA_SPEC_VERSION < 17]*/
		return StringCoding.getBytesUTF8NoRepl(str);
		/*[ELSE] JAVA_SPEC_VERSION < 17 */
		return String.getBytesUTF8NoRepl(str);
		/*[ENDIF] JAVA_SPEC_VERSION < 17 */
	}
	/*[ENDIF] (10 <= JAVA_SPEC_VERSION) & (JAVA_SPEC_VERSION < 26) */

	/*[IF JAVA_SPEC_VERSION >= 11]*/
	@Override
	public void blockedOn(Interruptible interruptible) {
		/*[IF JAVA_SPEC_VERSION >= 23]*/
		Thread.currentThread().blockedOn(interruptible);
		/*[ELSE] JAVA_SPEC_VERSION >= 23 */
		Thread.blockedOn(interruptible);
		/*[ENDIF] JAVA_SPEC_VERSION >= 23 */
	}

	/*[IF JAVA_SPEC_VERSION < 25]*/
	@Override
	public byte[] getBytesNoRepl(String str, Charset charset) throws CharacterCodingException {
		/*[IF JAVA_SPEC_VERSION < 17]*/
		return StringCoding.getBytesNoRepl(str, charset);
		/*[ELSE] JAVA_SPEC_VERSION < 17 */
		return String.getBytesNoRepl(str, charset);
		/*[ENDIF] JAVA_SPEC_VERSION < 17 */
	}

	@Override
	public String newStringNoRepl(byte[] bytes, Charset charset) throws CharacterCodingException {
		/*[IF JAVA_SPEC_VERSION < 17]*/
		return StringCoding.newStringNoRepl(bytes, charset);
		/*[ELSE] JAVA_SPEC_VERSION < 17 */
		return String.newStringNoRepl(bytes, charset);
		/*[ENDIF] JAVA_SPEC_VERSION < 17 */
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 25 */

	/*[IF JAVA_SPEC_VERSION == 25]*/
	@Override
	public byte[] uncheckedGetBytesNoRepl(String str, Charset charset) throws CharacterCodingException {
		return String.getBytesNoRepl(str, charset);
	}

	@Override
	public String uncheckedNewStringNoRepl(byte[] bytes, Charset charset) throws CharacterCodingException {
		return String.newStringNoRepl(bytes, charset);
	}
	/*[ENDIF] JAVA_SPEC_VERSION == 25 */
	/*[ENDIF] JAVA_SPEC_VERSION >= 11 */

	/*[IF JAVA_SPEC_VERSION >= 12]*/
	@Override
	public void setCause(Throwable throwable, Throwable cause) {
		throwable.setCause(cause);
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 12 */

	/*[IF JAVA_SPEC_VERSION >= 15]*/
	@Override
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
	@Override
	public Object classData(Class<?> clazz) {
		return clazz.getClassData();
	}

	@Override
	public ProtectionDomain protectionDomain(Class<?> clazz) {
		return clazz.getProtectionDomainInternal();
	}

	@Override
	public MethodHandle stringConcatHelper(String arg0, MethodType type) {
		return StringConcatHelper.lookupStatic(arg0, type);
	}

	/*[IF JAVA_SPEC_VERSION < 26]*/
	@Override
	public long stringConcatInitialCoder() {
		return StringConcatHelper.initialCoder();
	}

	@Override
	public long stringConcatMix(long arg0, String string) {
		return StringConcatHelper.mix(arg0, string);
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 26 */
	/*[ENDIF] JAVA_SPEC_VERSION >= 15 */

	/*[IF JAVA_SPEC_VERSION >= 16]*/
	@Override
	public void bindToLoader(ModuleLayer ml, ClassLoader cl) {
		ml.bindToLoader(cl);
	}

	@Override
	public void addExports(Module fromModule, String pkg) {
		fromModule.implAddExports(pkg);
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 16 */

	/*[IF JAVA_SPEC_VERSION >= 17]*/
	@Override
	/*[IF JAVA_SPEC_VERSION == 25]*/
	public int uncheckedDecodeASCII(byte[] srcBytes, int srcPos, char[] dstChars, int dstPos, int length) {
	/*[ELSE] JAVA_SPEC_VERSION == 25 */
	public int decodeASCII(byte[] srcBytes, int srcPos, char[] dstChars, int dstPos, int length) {
	/*[ENDIF] JAVA_SPEC_VERSION == 25 */
		return String.decodeASCII(srcBytes, srcPos, dstChars, dstPos, length);
	}

	@Override
	/*[IF JAVA_SPEC_VERSION == 25]*/
	public void uncheckedInflateBytesToChars(byte[] srcBytes, int srcOffset, char[] dstChars, int dstOffset, int length) {
	/*[ELSE] JAVA_SPEC_VERSION == 25 */
	public void inflateBytesToChars(byte[] srcBytes, int srcOffset, char[] dstChars, int dstOffset, int length) {
	/*[ENDIF] JAVA_SPEC_VERSION == 25 */
		StringLatin1.inflate(srcBytes, srcOffset, dstChars, dstOffset, length);
	}

	@Override
	public Class<?> findBootstrapClassOrNull(String name) {
		return VMAccess.findClassOrNull(name, ClassLoader.bootstrapClassLoader);
	}

	@Override
	public String join(String prefix, String suffix, String delimiter, String[] elements, int size) {
		return String.join(prefix, suffix, delimiter, elements, size);
	}

	/*[IF JAVA_SPEC_VERSION >= 24]*/
	@Override
	public void ensureNativeAccess(Module module, Class<?> ownerClass, String methodName, Class<?> callerClass, boolean isJNI) {
		module.ensureNativeAccess(ownerClass, methodName, callerClass, isJNI);
	}
	/*[ELSEIF JAVA_SPEC_VERSION >= 20]*/
	@Override
	public void ensureNativeAccess(Module module, Class<?> ownerClass, String methodName) {
		module.ensureNativeAccess(ownerClass, methodName);
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 24 */

	/*[IF JAVA_SPEC_VERSION >= 20]*/
	@Override
	public void addEnableNativeAccessToAllUnnamed() {
		/*[IF JAVA_SPEC_VERSION >= 26]*/
		Module.addEnableNativeAccessToAllUnnamed();
		/*[ELSE] JAVA_SPEC_VERSION >= 26 */
		Module.implAddEnableNativeAccessToAllUnnamed();
		/*[ENDIF] JAVA_SPEC_VERSION >= 26 */
	}
	/*[ELSE] JAVA_SPEC_VERSION >= 20 */
	@Override
	public boolean isEnableNativeAccess(Module mod) {
		return mod.implIsEnableNativeAccess();
	}

	@Override
	public void addEnableNativeAccessAllUnnamed() {
		Module.implAddEnableNativeAccessAllUnnamed();
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 20 */

	@Override
	/*[IF JAVA_SPEC_VERSION >= 26]*/
	public void addEnableNativeAccess(Module mod) {
		mod.implAddEnableNativeAccess();
	}
	/*[ELSE] JAVA_SPEC_VERSION >= 26 */
	public Module addEnableNativeAccess(Module mod) {
		return mod.implAddEnableNativeAccess();
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 26 */

	/*[IF (21 <= JAVA_SPEC_VERSION) & (JAVA_SPEC_VERSION < 24)]*/
	@Override
	public boolean allowSecurityManager() {
		return System.allowSecurityManager();
	}
	/*[ENDIF] (21 <= JAVA_SPEC_VERSION) & (JAVA_SPEC_VERSION < 24) */

	/*[IF JAVA_SPEC_VERSION >= 23]*/
	@Override
	public boolean addEnableNativeAccess(ModuleLayer moduleLayer, String moduleName) {
		return moduleLayer.addEnableNativeAccess(moduleName);
	}

	/*[IF JAVA_SPEC_VERSION < 25]*/
	@Override
	public int getCharsLatin1(long i, int index, byte[] buf) {
		return StringLatin1.getChars(i, index, buf);
	}

	@Override
	public int getCharsUTF16(long i, int index, byte[] buf) {
		return StringUTF16.getChars(i, index, buf);
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 25 */

	@Override
	/*[IF JAVA_SPEC_VERSION >= 25]*/
	public void uncheckedPutCharUTF16(byte[] val, int index, int c) {
	/*[ELSE] JAVA_SPEC_VERSION >= 25 */
	public void putCharUTF16(byte[] val, int index, int c) {
	/*[ENDIF] JAVA_SPEC_VERSION >= 25 */
		StringUTF16.putChar(val, index, c);
	}

	/*[IF JAVA_SPEC_VERSION < 24]*/
	@Override
	public long stringConcatHelperPrepend(long indexCoder, byte[] buf, String value) {
		return StringConcatHelper.prepend(indexCoder, buf, value);
	}

	@Override
	public int stringSize(long x) {
		return Long.stringSize(x);
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */

	/*[IF JAVA_SPEC_VERSION < 26]*/
	@Override
	public long stringConcatMix(long lengthCoder, char value) {
		return StringConcatHelper.mix(lengthCoder, value);
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 26 */

	@Override
	public PrintStream initialSystemErr() {
		return System.initialErr;
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 23 */

	/*[IF JAVA_SPEC_VERSION < 25]*/
	@Override
	public long findNative(ClassLoader loader, String entryName) {
		return ClassLoader.findNative0(loader, entryName);
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 25 */

	/*[IF JAVA_SPEC_VERSION < 25]*/
	@Override
	public void exit(int status) {
		Shutdown.exit(status);
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 25 */

	@Override
	/*[IF JAVA_SPEC_VERSION == 25]*/
	public int uncheckedEncodeASCII(char[] sa, int sp, byte[] da, int dp, int len) {
	/*[ELSE] JAVA_SPEC_VERSION == 25 */
	public int encodeASCII(char[] sa, int sp, byte[] da, int dp, int len) {
	/*[ENDIF] JAVA_SPEC_VERSION == 25 */
		/*[IF JAVA_SPEC_VERSION >= 26]*/
		return StringCoding.encodeAsciiArray(sa, sp, da, dp, len);
		/*[ELSE] JAVA_SPEC_VERSION >= 26 */
		return StringCoding.implEncodeAsciiArray(sa, sp, da, dp, len);
		/*[ENDIF] JAVA_SPEC_VERSION >= 26 */
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 17 */

	/*[IF JAVA_SPEC_VERSION >= 19]*/
	@Override
	public Thread currentCarrierThread() {
		return Thread.currentCarrierThread();
	}

	/*[IF JAVA_SPEC_VERSION < 24]*/
	@Override
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
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */

	@Override
	public Continuation getContinuation(Thread thread) {
		return thread.getContinuation();
	}

	@Override
	public void setContinuation(Thread thread, Continuation c) {
		thread.setContinuation(c);
	}

	/*[IF JAVA_SPEC_VERSION >= 20]*/
	@Override
	public Object[] scopedValueCache() {
		return Thread.scopedValueCache();
	}

	@Override
	public void setScopedValueCache(Object[] cache) {
		Thread.setScopedValueCache(cache);
	}

	@Override
	public Object scopedValueBindings() {
		return Thread.scopedValueBindings();
	}

	@Override
	public InputStream initialSystemIn() {
		return System.initialIn;
	}

	/*[IF JAVA_SPEC_VERSION >= 21]*/
	@Override
	/*[IF JAVA_SPEC_VERSION >= 25]*/
	public char uncheckedGetUTF16Char(byte[] val, int index) {
	/*[ELSE] JAVA_SPEC_VERSION >= 25 */
	public char getUTF16Char(byte[] val, int index) {
	/*[ENDIF] JAVA_SPEC_VERSION >= 25 */
		return StringUTF16.getChar(val, index);
	}

	@Override
	/*[IF JAVA_SPEC_VERSION == 25]*/
	public int uncheckedCountPositives(byte[] ba, int off, int len) {
	/*[ELSE] JAVA_SPEC_VERSION == 25 */
	public int countPositives(byte[] ba, int off, int len) {
	/*[ENDIF] JAVA_SPEC_VERSION == 25 */
		return StringCoding.countPositives(ba, off, len);
	}

	/*[IF JAVA_SPEC_VERSION < 23]*/
	@Override
	public long stringConcatCoder(char value) {
		return StringConcatHelper.coder(value);
	}

	@Override
	public long stringBuilderConcatMix(long lengthCoder, StringBuilder sb) {
		return sb.mix(lengthCoder);
	}

	@Override
	public long stringBuilderConcatPrepend(long lengthCoder, byte[] buf, StringBuilder sb) {
		return sb.prepend(lengthCoder, buf);
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 23 */
	/*[ENDIF] JAVA_SPEC_VERSION >= 21 */
	/*[ELSE] JAVA_SPEC_VERSION >= 20 */
	@Override
	public Object[] extentLocalCache() {
		return Thread.extentLocalCache();
	}

	@Override
	public void setExtentLocalCache(Object[] cache) {
		Thread.setExtentLocalCache(cache);
	}

	@Override
	public Object extentLocalBindings() {
		return Thread.extentLocalBindings();
	}

	@Override
	public void setExtentLocalBindings(Object bindings) {
		Thread.setExtentLocalBindings(bindings);
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 20 */

	@Override
	public StackableScope headStackableScope(Thread thread) {
		return thread.headStackableScopes();
	}

	@Override
	public void setHeadStackableScope(StackableScope scope) {
		Thread.setHeadStackableScope(scope);
	}

	@Override
	public ThreadContainer threadContainer(Thread thread) {
		return thread.threadContainer();
	}

	@Override
	public void start(Thread thread, ThreadContainer container) {
		thread.start(container);
	}

	@Override
	public Thread[] getAllThreads() {
		return Thread.getAllThreads();
	}

	@Override
	public ContinuationScope virtualThreadContinuationScope() {
		return VirtualThread.continuationScope();
	}

	@Override
	public void parkVirtualThread() {
		if (Thread.currentThread() instanceof BaseVirtualThread bvt) {
			bvt.park();
		} else {
			throw new WrongThreadException();
		}
	}

	@Override
	public void parkVirtualThread(long nanos) {
		if (Thread.currentThread() instanceof BaseVirtualThread bvt) {
			bvt.parkNanos(nanos);
		} else {
			throw new WrongThreadException();
		}
	}

	@Override
	public void unparkVirtualThread(Thread thread) {
		if (thread instanceof BaseVirtualThread bvt) {
			bvt.unpark();
		} else {
			throw new WrongThreadException();
		}
	}

	@Override
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

	/*[IF JAVA_SPEC_VERSION < 25]*/
	@Override
	public boolean isCarrierThreadLocalPresent(CarrierThreadLocal<?> carrierThreadlocal) {
		return asThreadLocal(carrierThreadlocal).isCarrierThreadLocalPresent();
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 25 */

	@Override
	public <T> T getCarrierThreadLocal(CarrierThreadLocal<T> carrierThreadlocal) {
		return asThreadLocal(carrierThreadlocal).getCarrierThreadLocal();
	}

	@Override
	public void removeCarrierThreadLocal(CarrierThreadLocal<?> carrierThreadlocal) {
		asThreadLocal(carrierThreadlocal).removeCarrierThreadLocal();
	}

	@Override
	public <T> void setCarrierThreadLocal(CarrierThreadLocal<T> carrierThreadlocal, T carrierThreadLocalvalue) {
		asThreadLocal(carrierThreadlocal).setCarrierThreadLocal(carrierThreadLocalvalue);
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 19 */

	/*[IF JAVA_SPEC_VERSION >= 11]*/
	@Override
	public String getLoaderNameID(ClassLoader loader) {
		if (loader == null) {
			return "null";
		}
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
	/*[ENDIF] JAVA_SPEC_VERSION >= 11 */

	/*[IF JAVA_SPEC_VERSION >= 22]*/
	@Override
	public boolean bytesCompatible(String string, Charset charset) {
		return string.bytesCompatible(charset);
	}

	@Override
	public void copyToSegmentRaw(String string, MemorySegment segment, long offset) {
		string.copyToSegmentRaw(segment, offset);
	}

	@Override
	public Method findMethod(Class<?> clazz, boolean publicOnly, String methodName, Class<?>... parameterTypes) {
		return clazz.findMethod(publicOnly, methodName, parameterTypes);
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 22 */

	/*[IF INLINE-TYPES]*/
	@Override
	public int classFileFormatVersion(Class<?> c) {
		return c.getClassFileVersion();
	}
	/*[ENDIF] INLINE-TYPES */

	/*[IF JAVA_SPEC_VERSION >= 24]*/
	@Override
	/*[IF JAVA_SPEC_VERSION >= 25]*/
	public Object uncheckedStringConcat1(String[] constants) {
	/*[ELSE] JAVA_SPEC_VERSION >= 25 */
	public Object stringConcat1(String[] constants) {
	/*[ENDIF] JAVA_SPEC_VERSION >= 25 */
		return new StringConcatHelper.Concat1(constants);
	}

	@Override
	public String concat(String prefix, Object value, String suffix) {
		return StringConcatHelper.concat(prefix, value, suffix);
	}

	@Override
	public int countNonZeroAscii(String string) {
		return StringCoding.countNonZeroAscii(string);
	}

	@Override
	public NativeLibraries nativeLibrariesFor(ClassLoader loader) {
		return ClassLoader.nativeLibrariesFor(loader);
	}

	@Override
	public byte stringCoder(String string) {
		return string.coder();
	}

	@Override
	public byte stringInitCoder() {
		return String.COMPACT_STRINGS ? String.LATIN1 : String.UTF16;
	}

	@Override
	public Executor virtualThreadDefaultScheduler() {
		return VirtualThread.defaultScheduler();
	}

	/*[IF JAVA_SPEC_VERSION < 25]*/
	@Override
	public Stream<ScheduledExecutorService> virtualThreadDelayedTaskSchedulers() {
		return VirtualThread.delayedTaskSchedulers();
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 25 */
	/*[ENDIF] JAVA_SPEC_VERSION >= 24 */
	/*[ENDIF] JAVA_SPEC_VERSION >= 9 */

	/*[IF JAVA_SPEC_VERSION >= 25]*/
	@Override
	public int classFileVersion(Class<?> clazz) {
		return clazz.getClassFileVersion();
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 25 */

	/*[IF JAVA_SPEC_VERSION >= 26]*/
	@Override
	public int getClassFileAccessFlags(Class<?> clazz) {
		return clazz.getClassFileAccessFlags();
	}

	@Override
	public byte[] getBytesUTF8OrThrow(String s) throws CharacterCodingException {
		return String.getBytesUTF8OrThrow(s);
	}

	@Override
	public byte[] uncheckedGetBytesOrThrow(String s, Charset cs) throws CharacterCodingException {
		return String.getBytesOrThrow(s, cs);
	}

	@Override
	public String uncheckedNewStringOrThrow(byte[] bytes, Charset cs) throws CharacterCodingException {
		return String.newStringOrThrow(bytes, cs);
	}

	@Override
	public String uncheckedNewStringWithLatin1Bytes(byte[] src) {
		return String.newStringWithLatin1Bytes(src);
	}

	@Override
	public void addEnableFinalMutationToAllUnnamed() {
		Module.addEnableFinalMutationToAllUnnamed();
	}

	@Override
	public boolean isFinalMutationEnabled(Module module) {
		return module.isFinalMutationEnabled();
	}

	@Override
	public boolean isStaticallyExported(Module module, String packageName, Module other) {
		return module.isStaticallyExported(packageName, other);
	}

	@Override
	public boolean isStaticallyOpened(Module module, String packageName, Module other) {
		return module.isStaticallyOpened(packageName, other);
	}

	@Override
	public boolean tryEnableFinalMutation(Module module) {
		return module.tryEnableFinalMutation();
	}
	/*[ENDIF] JAVA_SPEC_VERSION >= 26 */
}
