/*[INCLUDE-IF Sidecar16]*/
package java.lang;

import java.io.*;
import java.security.CodeSource;
import java.security.ProtectionDomain;

/*[IF !Sidecar19-SE]
import com.ibm.oti.vm.AbstractClassLoader;
/*[ENDIF]*/
import com.ibm.oti.vm.VM;

import java.net.URL;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Hashtable;
import java.util.LinkedList;
import java.util.Map;
import java.util.NoSuchElementException;
import java.util.Queue;
import java.util.Vector;
import java.util.Collections;
import java.util.WeakHashMap;
import java.util.stream.Stream;
import java.lang.ref.ReferenceQueue;
import java.lang.ref.WeakReference;
import java.lang.reflect.*;
import java.security.cert.Certificate;

/*[IF Sidecar19-SE]
import java.util.Spliterator;
import java.util.Spliterators;
import java.util.stream.StreamSupport;
import jdk.internal.module.ServicesCatalog;
import jdk.internal.misc.SharedSecrets;
import java.util.concurrent.ConcurrentHashMap;
import jdk.internal.reflect.CallerSensitive;
import jdk.internal.loader.ClassLoaders;
import jdk.internal.loader.BootLoader;
/*[ELSE]*/
import sun.reflect.CallerSensitive;
/*[ENDIF]*/

/*******************************************************************************
 * Copyright (c) 1998, 2018 IBM Corp. and others
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
 
/**
 * ClassLoaders are used to dynamically load, link and install
 * classes into a running image.
 *
 * @version		initial
 */
public abstract class ClassLoader {
	private static CodeSource defaultCodeSource = new CodeSource(null, (Certificate[])null);
	

	/**
	 * This is the bootstrap ClassLoader
	 */
	static ClassLoader bootstrapClassLoader;
	/*[IF Sidecar19-SE]*/
	private ServicesCatalog servicesCatalog;
	private final Module unnamedModule;
	private final String classLoaderName;
	private final static String DELEGATING_CL = "jdk.internal.reflect.DelegatingClassLoader"; //$NON-NLS-1$
	/*[ELSE]
	private final static String DELEGATING_CL = "sun.reflect.DelegatingClassLoader"; //$NON-NLS-1$
	/*[ENDIF]*/
	private boolean isDelegatingCL = false;

	/*
	 * This is the application ClassLoader
	 */
	private static ClassLoader applicationClassLoader;
	private static boolean initSystemClassLoader;
	private final static boolean isAssertOptionFound = foundJavaAssertOption();

	private long vmRef;
	ClassLoader parent;
	
	/*[IF Panama]*/
	private static String[]	usr_paths = new String[1];
	/*[ENDIF]*/

	/*[PR CMVC 130382] Optimize checking ClassLoader assertion status */
	private static boolean checkAssertionOptions;
	/*[PR CMVC 94437] fix deadlocks */
	/*[PR 122459] LIR646 - Remove use of generic object for synchronization */
	private static final class AssertionLock { AssertionLock() {} }
	private final Object assertionLock = new AssertionLock();
  	private boolean defaultAssertionStatus;
  	private Map<String, Boolean> packageAssertionStatus;
  	private Map<String, Boolean> classAssertionStatus;
  	/*[IF Sidecar19-SE]*/
  	private final Hashtable<String, NamedPackage> packages = new Hashtable();
  	private volatile ConcurrentHashMap<?, ?> classLoaderValueMap;
	/*[ELSE]
  	private final Hashtable<String, Package> packages = new Hashtable();
	/*[ENDIF] Sidecar19-SE*/
	/*[PR CMVC 94437] fix deadlocks */
	/*[PR 122459] LIR646 - Remove use of generic object for synchronization */
	private static final class LazyInitLock { LazyInitLock() {} }
  	private final Object lazyInitLock = new LazyInitLock();
	private volatile Hashtable<Class<?>, Object[]> classSigners; // initialized if needed
	private volatile Hashtable<String, Certificate[]> packageSigners;
	private static Certificate[] emptyCertificates = new Certificate[0];
	private volatile ProtectionDomain defaultProtectionDomain;

	//	store parallel capable classloader classes
	private static Map<Class<?>, Object> parallelCapableCollection;
	//	store class binary name based lock
	private volatile Hashtable<String, ClassNameLockRef> classNameBasedLock;
	//	for performance purpose, only check once if registered as parallel capable
	//	assume customer classloader follow Java specification requirement 
	//	in which registerAsParallelCapable shall be invoked during initialization
	private boolean isParallelCapable;
	private static final class ClassNameBasedLock { ClassNameBasedLock() {} }
	private static final Package[] EMPTY_PACKAGE_ARRAY = new Package[0];
	
	// Cache instances of java.lang.invoke.MethodType generated from method descriptor strings
	private Map<String, java.lang.invoke.MethodType> methodTypeFromMethodDescriptorStringCache;
	
	private static boolean allowArraySyntax;
/*[IF Sidecar19-SE]*/	
	private static boolean lazyClassLoaderInit = true;
/*[ELSE]	
	private static boolean lazyClassLoaderInit = false;
/*[ENDIF]*/	
	private static boolean specialLoaderInited = false;
	private static InternalAnonymousClassLoader internalAnonClassLoader;
	private static native void initAnonClassLoader(InternalAnonymousClassLoader anonClassLoader);	
	
	/*[PR JAZZ 73143]: ClassLoader incorrectly discards class loading locks*/
	static final class ClassNameLockRef extends WeakReference<Object> implements Runnable {
		private static final ReferenceQueue<Object> queue = new ReferenceQueue<>();
		private final String key;
		private final Hashtable<?, ?> classNameLockHT;
		public ClassNameLockRef(Object referent, String keyValue, Hashtable<?, ?> classNameLockHTValue) {
			super(referent, queue);
			key = keyValue;
			classNameLockHT = classNameLockHTValue;
		}
		@Override
		public void run() {
			synchronized(classNameLockHT) {
				if (classNameLockHT.get(key) == this) {
					classNameLockHT.remove(key);
				}
			}
		}
	}
	
	static final void initializeClassLoaders() {
		if (null != bootstrapClassLoader) {
			return;
		}
		parallelCapableCollection = Collections.synchronizedMap(new WeakHashMap<>());
		
		allowArraySyntax = "true".equalsIgnoreCase(	//$NON-NLS-1$
				System.internalGetProperties().getProperty("sun.lang.ClassLoader.allowArraySyntax"));	//$NON-NLS-1$
		
		/*[PR CMVC 193184] reflect.cache must be enabled here, otherwise performance slow down is experienced */
		String propValue = System.internalGetProperties().getProperty("reflect.cache"); //$NON-NLS-1$
		if (propValue != null) propValue = propValue.toLowerCase();
		/* Do not enable reflect cache if -Dreflect.cache=false is in commandline */
		boolean reflectCacheEnabled = false;
		boolean reflectCacheDebug = false;
		if (!"false".equals(propValue)) { //$NON-NLS-1$
			/*JAZZ 42080: Turning off reflection caching for cloud to reduce Object Leaks*/	
			reflectCacheEnabled = true;
			if (propValue != null) {
				int debugIndex = propValue.indexOf("debug"); //$NON-NLS-1$
				if (debugIndex >= 0) {
					/*[PR 125873] Improve reflection cache */
					/* reflect.cache=boot is handled in completeInitialization() */
					reflectCacheDebug = true;
				}
			}
		}

		try {
			/* CMVC 179008 - b143 needs ProtectionDomain initialized here */
			Class.forName("java.security.ProtectionDomain"); //$NON-NLS-1$
		} catch(ClassNotFoundException e) {
			// ignore
		}

		ClassLoader sysTemp = null;
		// Proper initialization requires BootstrapLoader is the first loader instantiated
		String systemLoaderString = System.internalGetProperties().getProperty("systemClassLoader"); //$NON-NLS-1$
		if(null == systemLoaderString) {
			/*[IF Sidecar19-SE]*/
			// This static method call ensures jdk.internal.loader.ClassLoaders.BOOT_LOADER initialization first
			jdk.internal.loader.ClassLoaders.platformClassLoader();
			if (bootstrapClassLoader.servicesCatalog != null) {
				throw new InternalError("bootstrapClassLoader.servicesCatalog is NOT null "); //$NON-NLS-1$
			}
			bootstrapClassLoader.servicesCatalog = BootLoader.getServicesCatalog();
			if (bootstrapClassLoader.classLoaderValueMap != null) {
				throw new InternalError("bootstrapClassLoader.classLoaderValueMap is NOT null "); //$NON-NLS-1$
			}
			bootstrapClassLoader.classLoaderValueMap = BootLoader.getClassLoaderValueMap();
			applicationClassLoader = ClassLoaders.appClassLoader();
			/*[ELSE]*/
			sysTemp = com.ibm.oti.vm.BootstrapClassLoader.singleton();
			/*[ENDIF]*/
		} else {
			try {
				sysTemp = (ClassLoader)Class.forName(systemLoaderString,true,null).newInstance();
			} catch(Throwable x) {
				x.printStackTrace();
				System.exit(1);
			}
		}
/*[IF !Sidecar19-SE]*/		
		bootstrapClassLoader = sysTemp;
		AbstractClassLoader.setBootstrapClassLoader(bootstrapClassLoader);
		lazyClassLoaderInit = true;
		applicationClassLoader = bootstrapClassLoader;
/*[ENDIF]*/

		/* [PR 78889] The creation of this classLoader requires lazy initialization. The internal classLoader struct
		 * is created in the initAnonClassLoader call. The "new InternalAnonymousClassLoader()" call must be 
		 * done exactly after lazyClassLoaderInit is set and before the "java.lang.ClassLoader.lazyInitialization" 
		 * is read in. This is the only way to guarantee that ClassLoader will be created with lazy initialization. */
		internalAnonClassLoader = new InternalAnonymousClassLoader();
		initAnonClassLoader(internalAnonClassLoader);
		
		String lazyValue = System.internalGetProperties().getProperty("java.lang.ClassLoader.lazyInitialization"); //$NON-NLS-1$
		if (null != lazyValue) {
			lazyValue = lazyValue.toLowerCase();
			if ("false".equals(lazyValue)) { //$NON-NLS-1$
				lazyClassLoaderInit = false;
			}
		}
		
		/*[IF Sidecar19-SE]*/
		jdk.internal.misc.VM.initLevel(1);
		/*[IF Java10]*/
		try {
		/*[ENDIF]*/
			System.bootLayer = jdk.internal.module.ModuleBootstrap.boot();
		/*[IF Java10]*/
		} catch (Exception ex) {
			ex.printStackTrace();
			System.exit(1);
		}
		/*[ENDIF]*/
		jdk.internal.misc.VM.initLevel(2);
		String javaSecurityManager = System.internalGetProperties().getProperty("java.security.manager"); //$NON-NLS-1$
		if (null != javaSecurityManager) {
			if (javaSecurityManager.isEmpty()) {
				System.setSecurityManager(new SecurityManager());
			} else {
				try {
					System.setSecurityManager((SecurityManager)Class.forName(javaSecurityManager, true, applicationClassLoader).newInstance());
				} catch (Throwable e) {
					/*[MSG "K0631", "JVM can't set custom SecurityManager due to {0}"]*/
					throw new Error(com.ibm.oti.util.Msg.getString("K0631", e.toString()), e); //$NON-NLS-1$
				}
			}
		}
		jdk.internal.misc.VM.initLevel(3);
		/*[ELSE]*/
		applicationClassLoader = sun.misc.Launcher.getLauncher().getClassLoader();
		/*[ENDIF]*/

		/*[IF Panama]*/
		/* RI jdk.internal.nicl.LdLoader uses reflection to lookup this field */
		usr_paths[0] = System.getProperty("user.dir");
		/*[ENDIF]*/
		
		/* Find the extension class loader */
		ClassLoader tempLoader = applicationClassLoader;
		while (null != tempLoader.parent) {
			tempLoader = tempLoader.parent;
		}
		VMAccess.setExtClassLoader(tempLoader);
		
		/*[PR 125932] Reflect cache may be initialized by multiple Threads */
		/*[PR JAZZ 107786] constructorParameterTypesField should be initialized regardless of reflectCacheEnabled or not */
		Class.initCacheIds(reflectCacheEnabled, reflectCacheDebug);
	}	

/**
 * Constructs a new instance of this class with the system
 * class loader as its parent.
 *
 * @exception	SecurityException
 *					if a security manager exists and it does not
 *					allow the creation of new ClassLoaders.
 */
protected ClassLoader() {
	this(checkSecurityPermission(), null, applicationClassLoader);
}

/**
 * This is a static helper method to perform security check earlier such that current ClassLoader object 
 * can't be resurrected when there is a SecurityException thrown.
 *
 * @return Void a unused reference passed to the Constructor
 */
private static Void checkSecurityPermission() {
	SecurityManager security = System.getSecurityManager();
	if (security != null) {
		security.checkCreateClassLoader();
	}
	return null;
}

/**
 * Constructs a new instance of this class with the given
 * class loader as its parent.
 *
 * @param		parentLoader ClassLoader
 *					the ClassLoader to use as the new class
 *					loaders parent.
 * @exception	SecurityException
 *					if a security manager exists and it does not
 *					allow the creation of new ClassLoaders.
 */
protected ClassLoader(ClassLoader parentLoader) {
	this(checkSecurityPermission(), null, parentLoader);
}

/*[IF Sidecar19-SE]*/
/**
 * Constructs a class loader with the specified name and the given
 * class loader as its parent.
 *
 * @param		classLoaderName name of this ClassLoader
 * 					or null if not named.
 * @param		parentLoader ClassLoader
 *					the ClassLoader to use as the new class
 *					loaders parent.
 * @exception	IllegalArgumentException
 *					if the name of this class loader is empty.
 * @exception	SecurityException
 *					if a security manager exists and it does not
 *					allow the creation of new ClassLoaders.
 *@since 9
 */
protected ClassLoader(String classLoaderName, ClassLoader parentLoader) {
	this(checkSecurityPermission(), classLoaderName, parentLoader);
}
/*[ENDIF]*/

private ClassLoader(Void staticMethodHolder, String classLoaderName, ClassLoader parentLoader) {
	// This assumes that DelegatingClassLoader is constructed via ClassLoader(parentLoader)
	isDelegatingCL = DELEGATING_CL.equals(this.getClass().getName());

/*[IF Sidecar19-SE]*/
	if ((classLoaderName != null) && classLoaderName.isEmpty()) {
		/*[MSG "K0645", "The given class loader name can't be empty."]*/
		throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0645")); //$NON-NLS-1$
	}
/*[ENDIF]*/

	if (parallelCapableCollection.containsKey(this.getClass())) {
		isParallelCapable = true;
	}
	
	// VM Critical: must set parent before calling initializeInternal()
	parent = parentLoader;
/*[IF !Sidecar19-SE]*/
	specialLoaderInited = (bootstrapClassLoader != null);
/*[ENDIF]*/
	if (specialLoaderInited) {
		if (!lazyClassLoaderInit) {
			VM.initializeClassLoader(this, VM.J9_CLASSLOADER_TYPE_OTHERS, isParallelCapable);
		}
/*[IF Sidecar19-SE]*/
/*[IF Sidecar19-SE-OpenJ9]
		unnamedModule = new Module(this);
/*[ELSE]*/
		unnamedModule = SharedSecrets.getJavaLangReflectModuleAccess().defineUnnamedModule(this);
/*[ENDIF]*/
/*[ENDIF]*/
	} 
/*[IF Sidecar19-SE]*/	
	else {
		if (bootstrapClassLoader == null) {
			// BootstrapClassLoader.unnamedModule is set by JVM_SetBootLoaderUnnamedModule
			unnamedModule = null;
			bootstrapClassLoader = this;
			VM.initializeClassLoader(bootstrapClassLoader, VM.J9_CLASSLOADER_TYPE_BOOT, false);
		} else {
			// Assuming the second classloader initialized is platform classloader
			VM.initializeClassLoader(this, VM.J9_CLASSLOADER_TYPE_PLATFORM, false);
			specialLoaderInited = true;
/*[IF Sidecar19-SE-OpenJ9]
			unnamedModule = new Module(this);
/*[ELSE]*/
			unnamedModule = SharedSecrets.getJavaLangReflectModuleAccess().defineUnnamedModule(this);
/*[ENDIF]*/
		}
	}
	this.classLoaderName = classLoaderName;
/*[ENDIF]*/	
	
	if (isAssertOptionFound) {
		initializeClassLoaderAssertStatus();
	}
}

/**
 * Constructs a new class from an array of bytes containing a 
 * class definition in class file format.
 *
 * @param 		classRep byte[]
 *					a memory image of a class file.
 * @param 		offset int
 *					the offset into the classRep.
 * @param 		length int
 *					the length of the class file.
 *
 * @return	the newly defined Class
 * 
 * @throws ClassFormatError when the bytes are invalid
 * 
 * @deprecated Use defineClass(String, byte[], int, int)
 */
/*[IF Sidecar19-SE]*/
@Deprecated(forRemoval=false, since="1.1")
/*[ELSE]*/
@Deprecated
/*[ENDIF]*/

protected final Class<?> defineClass (byte [] classRep, int offset, int length) throws ClassFormatError {
	return defineClass ((String) null, classRep, offset, length);
}

/**
 * Constructs a new class from an array of bytes containing a 
 * class definition in class file format.
 *
 * @param 		className java.lang.String
 *					the name of the new class
 * @param 		classRep byte[]
 *					a memory image of a class file
 * @param 		offset int
 *					the offset into the classRep
 * @param 		length int
 *					the length of the class file
 *
 * @return	the newly defined Class
 * 
 * @throws ClassFormatError when the bytes are invalid
 */
protected final Class<?> defineClass(String className, byte[] classRep, int offset, int length) throws ClassFormatError {
	return defineClass(className, classRep, offset, length, null);
}

private String checkClassName(String className) {
	int index;
	if((index = className.lastIndexOf('.')) >= 0) {
		String packageName = className.substring(0, index);
		/*[PR 94856]*/
		if (className.startsWith("java.")) { //$NON-NLS-1$
			/*[IF Sidecar19-SE]*/
			/*[PR RTC 115588: java.* classes can be loaded by the platform class loader]*/
			ClassLoader platformCL = ClassLoaders.platformClassLoader();
			if (!(this == platformCL || this.isAncestorOf(platformCL))) {
			/*[ENDIF]*/
				/*[MSG "K01d2", "{1} - protected system package '{0}'"]*/
				throw new SecurityException(com.ibm.oti.util.Msg.getString("K01d2", packageName, className)); //$NON-NLS-1$
			/*[IF Sidecar19-SE]*/
			}
			/*[ENDIF]*/
		}
		return packageName;
	}
	return ""; //$NON-NLS-1$
}

/**
 * Constructs a new class from an array of bytes containing a 
 * class definition in class file format and assigns the new
 * class to the specified protection domain.
 *
 * @param 		className java.lang.String
 *					the name of the new class.
 * @param 		classRep byte[]
 *					a memory image of a class file.
 * @param 		offset int
 *					the offset into the classRep.
 * @param 		length int
 *					the length of the class file.
 * @param 		protectionDomain ProtectionDomain
 *					the protection domain this class should
 *					belong to.
 *
 * @return	the newly defined Class
 * 
 * @throws ClassFormatError when the bytes are invalid
 */
protected final Class<?> defineClass (
		final String className, 
		final byte[] classRep, 
		final int offset, 
		final int length, 
		ProtectionDomain protectionDomain) 
		throws java.lang.ClassFormatError 
{
	Certificate[] certs = null; 
	if (protectionDomain != null) {
		final CodeSource cs = protectionDomain.getCodeSource();
		if (cs != null) certs = cs.getCertificates();
	}
	if (className != null) {
		/*[PR 95417]*/
		String packageName = checkClassName(className);
		/*[PR 93858]*/
		checkPackageSigners(packageName, className, certs);
	}

	/*[PR 123387] bogus parameters to defineClass() should produce ArrayIndexOutOfBoundsException */
	if (offset < 0 || length < 0 || offset > classRep.length || length > classRep.length - offset) {
		throw new ArrayIndexOutOfBoundsException();
	}

	if (protectionDomain == null)	{
		protectionDomain = getDefaultProtectionDomain();
	}
	
	final ProtectionDomain pd = protectionDomain;
	/*[PR CMVC 92062] disallow extending restricted packages */
	/*[PR CMVC 110183] checkPackageAccess() (accessClassInPackage permission) denied when granted access */
	/*[PR CVMC 124584] checkPackageAccess(), not defineClassImpl(), should use ProtectionDomain */
	Class<?> answer = defineClassImpl(className, classRep, offset, length, pd);

	if (certs != null) {
		setSigners(answer, certs);
	}
	
	boolean isVerbose = isVerboseImpl();
	URL url = null;
	if (isVerbose) {
		if (pd != null) {
			CodeSource cs = pd.getCodeSource();
			if (cs != null) {
				url = cs.getLocation();
			}
		}
	}
	/*[IF Sidecar19-SE]*/
	addPackageToList(answer);
	/*[ENDIF] Sidecar19-SE */

	/*[PR CMVC 89001] Verbose output when loading non-bootstrap classes */
	if (isVerbose) {
		/*[PR CMVC 99348] Do not log to System.err */
		String location = (url != null) ? url.toString() : "<unknown>"; //$NON-NLS-1$
		com.ibm.oti.vm.VM.dumpString("class load: " + answer.getName() + " from: " + location + "\n"); //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
	}
	return answer;
}

/*[IF Sidecar19-SE]*/
 /**
  * Add a class's package name to this classloader's list of packages, if not already present.
 * @param newClass
 */
void addPackageToList(Class<?> newClass) {
	synchronized(packages) {
		packages.computeIfAbsent(newClass.getPackageName(), pkgName->new NamedPackage(pkgName, newClass.getModule()));
	}
}
/*[ENDIF] Sidecar19-SE */

/*[PR CMVC 89001] Verbose output when loading non-bootstrap classes */ 
private native boolean isVerboseImpl();

private static native boolean foundJavaAssertOption();

private void checkPackageSigners(final String packageName, String className, final Certificate[] classCerts) {
	Certificate[] packageCerts = null;
	synchronized(lazyInitLock) {
		if (packageSigners != null) {
			packageCerts = packageSigners.get(packageName);
		} else {
			packageSigners = new Hashtable<>();
		}
	}
	if (packageCerts == null) {
		if (classCerts == null) {
			packageSigners.put(packageName, emptyCertificates);
		} else {
			packageSigners.put(packageName, classCerts);

		}
	} else {
		if ((classCerts == null && packageCerts.length == 0) || classCerts == packageCerts)
			return;
		if (classCerts != null && classCerts.length == packageCerts.length) {
			boolean foundMatch = true;
			test: for (int i=0; i<classCerts.length; i++) {
				if (classCerts[i] == packageCerts[i]) continue;
				if (classCerts[i].equals(packageCerts[i])) continue;
				for (int j=0; j<packageCerts.length; j++) {
					if (j == i) continue;
					if (classCerts[i] == packageCerts[j]) continue test;
					if (classCerts[i].equals(packageCerts[j])) continue test;
				}
				foundMatch = false;
				break;
			}
			if (foundMatch) return;
		}
/*[MSG "K01d1", "Signers of '{0}' do not match signers of other classes in package"]*/
		throw new SecurityException(com.ibm.oti.util.Msg.getString("K01d1", className)); //$NON-NLS-1$
	}	
} 

/**
 * Gets the current default protection domain. If there isn't
 * one, it attempts to construct one based on the currently
 * in place security policy. 
 * <p>
 * If the default protection domain can not be determined,
 * answers null.
 * <p>
 *
 * @return 		ProtectionDomain or null
 *					the default protection domain.
 */
private final ProtectionDomain getDefaultProtectionDomain () {
	if (isParallelCapable) {
		if (defaultProtectionDomain == null) {
			synchronized(lazyInitLock) {
				return	getDefaultProtectionDomainHelper();
			}
		}
		return defaultProtectionDomain;
	} else {
		// no need for synchronisation when not parallel capable
		return	getDefaultProtectionDomainHelper();
	}
}

private final ProtectionDomain getDefaultProtectionDomainHelper() {
	if (defaultProtectionDomain == null) {
		/*[PR 115587] Default 1.4 behavior is to create dynamic ProtectionDomains */
		defaultProtectionDomain = new ProtectionDomain(defaultCodeSource, null, this, null);
	}
	return defaultProtectionDomain;
}

/*
 * VM level support for constructing a new class. Should not
 * be called by subclasses.
 */
private final native Class<?> defineClassImpl(String className, byte [] classRep, int offset, int length, Object protectionDomain);


/**
 * Overridden by subclasses, by default throws ClassNotFoundException.
 * This method is called by loadClass() after the parent ClassLoader
 * has failed to find a loaded class of the same name.
 *
 * @return 		java.lang.Class
 *					the class or null.
 * @param 		className String
 *					the name of the class to search for.
 * @exception	ClassNotFoundException
 *					always, unless overridden.
 */
protected Class<?> findClass (String className) throws ClassNotFoundException {
    throw new ClassNotFoundException();
}

/*[IF Sidecar19-SE]*/
/**
 * Overridden by subclasses that support the loading from modules.
 * When the moduleName is null, the default implementation invokes findClass(String), 
 * attempts to find the class and return it, or returns null in case of ClassNotFoundException.
 * When the moduleName is not null, the default implementation returns null.
 * This method is called by Class.forName(Module module, String name).
 *
 * @return 		java.lang.Class
 *					the class or null.
 * @param		moduleName
 * 					the name of the module from which the class is to be loaded.
 * @param 		className String
 *					the name of the class to search for.

 */
protected Class<?> findClass(String moduleName, String className) {
	Class<?> classFound = null;
	if (moduleName == null) {
		try {
			classFound = findClass(className);
		} catch (ClassNotFoundException e) {
			// returns null if the class can't be found
		}
	}
	return classFound;
}
/*[ENDIF] Sidecar19-SE */

/**
 * Attempts to find and return a class which has already
 * been loaded by the virtual machine. Note that the class
 * may not have been linked and the caller should call
 * resolveClass() on the result if necessary.
 *
 * @return 		java.lang.Class
 *					the class or null.
 * @param 		className String
 *					the name of the class to search for.
 */
protected final Class<?> findLoadedClass (String className) {
	/*[PR CMVC 124675] allow findLoadedClass to find arrays if magic system property set */
	if (!allowArraySyntax) {
		if (className != null && className.length() > 0 && className.charAt(0) == '[') {
			return null;
		}
	}
	return findLoadedClassImpl(className);
}

private native Class<?> findLoadedClassImpl(String className);

/**
 * Attempts to load a class using the system class loader.
 * Note that the class has already been been linked.
 *
 * @return 		java.lang.Class
 *					the class which was loaded.
 * @param 		className String
 *					the name of the class to search for.
 * @exception	ClassNotFoundException
 *					if the class can not be found.
 */
protected final Class<?> findSystemClass (String className) throws ClassNotFoundException {
	return applicationClassLoader.loadClass(className);
}

/**
 * Returns the specified ClassLoader's parent.
 *
 * @return 		java.lang.ClassLoader
 *					the class or null.
 * @exception	SecurityException
 *					if a security manager exists and it does not
 *					allow the parent loader to be retrieved.
 */
@CallerSensitive
public final ClassLoader getParent() {
	SecurityManager security = System.getSecurityManager();
	if (security != null) {	
		ClassLoader callersClassLoader = callerClassLoader();
		/*[PR JAZZ103 76960] permission check is needed against the parent instead of this classloader */
		if (needsClassLoaderPermissionCheck(callersClassLoader, parent)) {
			security.checkPermission(com.ibm.oti.util.RuntimePermissions.permissionGetClassLoader);
		}
	}
	return parent;
}

/**
 * Answers an URL which can be used to access the resource
 * described by resName, using the class loader's resource lookup
 * algorithm. The default behavior is just to return null.
 *
 * @return		URL
 *					the location of the resource.
 * @param		resName String
 *					the name of the resource to find.
 *
 * @see			Class#getResource
 */
public URL getResource (String resName) {
	URL result = null;
	if (null == parent) {
/*[IF Sidecar19-SE]*/
		result =  BootLoader.findResource(resName);
/*[ELSE]
		result = bootstrapClassLoader.findResource(resName);
/*[ENDIF]*/
	} else {
		result = parent.getResource(resName);
	}
	if (null == result) {
		result = findResource(resName);
	}
	return result;
}

/*
 * A sequence of two or more enumerations.
 */
private static final class CompoundEnumeration<T> implements Enumeration<T> {

	private final Queue<Enumeration<T>> queue;

	CompoundEnumeration(Enumeration<T> first, Enumeration<T> second) {
		super();
		if (first instanceof CompoundEnumeration<?>) {
			queue = ((CompoundEnumeration<T>) first).queue;
		} else {
			queue = new LinkedList<>();
			queue.add(first);
		}
		queue.add(second);
	}
	
	void append(Enumeration<T> element) {
		queue.add(element);
	}

	@Override
	public boolean hasMoreElements() {
		for (;;) {
			Enumeration<T> head = queue.peek();

			if (head == null) {
				return false;
			}

			if (head.hasMoreElements()) {
				return true;
			}

			queue.remove();
		}
	}

	@Override
	public T nextElement() {
		for (;;) {
			Enumeration<T> head = queue.peek();

			if (head == null) {
				throw new NoSuchElementException();
			}

			if (head.hasMoreElements()) {
				return head.nextElement();
			}

			queue.remove();
		}
	}
}

/**
 * Answers an Enumeration of URL which can be used to access the resources
 * described by resName, using the class loader's resource lookup
 * algorithm.
 *
 * @param		resName String
 *					the name of the resource to find.

 * @return		Enumeration
 *					the locations of the resources.
 *
 * @throws IOException when an error occurs
 */
public Enumeration<URL> getResources(String resName) throws IOException {
	Enumeration<URL> resources = null;

	/*[PR CMVC 198371] ClassLoader.getResources doesn't get override or Overridden get */
	if (parent != null) {
		resources = parent.getResources(resName);
	} else if (this != bootstrapClassLoader) {
		resources = 
/*[IF Sidecar19-SE]*/
		BootLoader.findResources(resName);
/*[ELSE]
		bootstrapClassLoader.getResources(resName);
/*[ENDIF]*/
	}

	Enumeration<URL> localResources = findResources(resName);

	if (localResources != null && !localResources.hasMoreElements()) {
		localResources = null;
	}

	if (resources == null) {
		resources = localResources;
		if (resources == null) {
			resources = Collections.emptyEnumeration();
		}
	} else if (localResources != null) {
		if (resources instanceof CompoundEnumeration<?>) {
			((CompoundEnumeration<URL>) resources).append(localResources);
		} else {
			resources = new CompoundEnumeration<>(resources, localResources);
		}
	}

	return resources;
}

/**
 * Answers a stream on a resource found by looking up
 * resName using the class loader's resource lookup
 * algorithm. The default behavior is just to return null.
 *
 * @return		InputStream
 *					a stream on the resource or null.
 * @param		resName	String
 *					the name of the resource to find.
 *
 * @see			Class#getResourceAsStream
 */
public InputStream getResourceAsStream (String resName) {
	URL url = getResource(resName);
	try {
		if (url != null) return url.openStream();
	} catch (IOException e) {
		// ignore
	}
	return null;
}

static void completeInitialization() {
	/*[PR JAZZ 57622: Support -Dreflect.cache=boot option] -Dreflect.cache=boot causes deadlock (Details: CMVC 120695). Loading Void class explicitly will prevent possible deadlock during caching reflect classes/methods. */
	@SuppressWarnings("unused")
	Class<?> voidClass = Void.TYPE;

	/*[PR CMVC 193137] -Dreflect.cache=boot throws NPE. Caching reflect objects for bootstrap callers should be enabled late to avoid bootstrap problems. */
	/* Process reflect.cache=boot, other options are processed earlier in initializeClassLoaders() */
	String propValue = System.internalGetProperties().getProperty("reflect.cache"); //$NON-NLS-1$
	if (propValue != null) propValue = propValue.toLowerCase();
	/* Do not enable reflect cache if -Dreflect.cache=false is in commandline */
	boolean reflectCacheAppOnly = true;
	if (!"false".equals(propValue)) { //$NON-NLS-1$
		reflectCacheAppOnly = false;
		
		if (propValue != null) {
			/*[PR 125873] Improve reflection cache */
			int bootIndex = propValue.indexOf("boot"); //$NON-NLS-1$
			if (bootIndex >= 0) {
				reflectCacheAppOnly = false;
			} else {
				int appIndex = propValue.indexOf("app"); //$NON-NLS-1$
				if (appIndex >= 0) {
					reflectCacheAppOnly = true;
				}
			}
		}
	}
	Class.setReflectCacheAppOnly(reflectCacheAppOnly);
	
	initSystemClassLoader = true;
}

/*[IF Sidecar18-SE-OpenJ9|Sidecar19-SE]*/
//Returns incoming class's classloader without going through security checking
static ClassLoader getClassLoader(Class<?> clz) {
	if (null != clz) {
		return clz.getClassLoader0();
	} else {
		return null;
	}
}
/*[ENDIF]*/

/*[IF Sidecar19-SE]*/
@CallerSensitive
public static ClassLoader getPlatformClassLoader() {
	SecurityManager security = System.getSecurityManager();
	ClassLoader platformClassLoader = ClassLoaders.platformClassLoader();
	if (security != null) {
		ClassLoader callersClassLoader = callerClassLoader();
		if (needsClassLoaderPermissionCheck(callersClassLoader, platformClassLoader)) {
			security.checkPermission(com.ibm.oti.util.RuntimePermissions.permissionGetClassLoader);
		}
	}
	return platformClassLoader;
}

// Loads a class in a module defined to this classloader
// This method returns null if the class can't be found
// Not delegate to the parent classloader 
final Class<?> loadLocalClass(java.lang.String name) {
	Class<?> localClass = null;
	try {
		localClass = loadClassHelper(name, false, false, null);
	} catch (ClassNotFoundException e) {
		// returns null if the class can't be found
	}
	return localClass;
}

/**
 * Get the name given when creating the ClassLoader instance, or null if none was provided.
 * @return name of the class loader or null
 * @since 9
 */
public String getName() {
	return classLoaderName;
}
/*[ENDIF] Sidecar19-SE */

/**
 * Convenience operation to obtain a reference to the system class loader.
 * The system class loader is the parent of any new <code>ClassLoader</code>
 * objects created in the course of an application and will normally be the
 * same <code>ClassLoader</code> as that used to launch an application.
 * 
 * @return java.lang.ClassLoader the system classLoader.
 * @exception SecurityException
 *                if a security manager exists and it does not permit the
 *                caller to access the system class loader.
 */
@CallerSensitive
public static ClassLoader getSystemClassLoader () {
	/*[PR CMVC 99755] Implement -Djava.system.class.loader option */
	if (initSystemClassLoader) {
		Class<?> classLoaderClass = ClassLoader.class;
		synchronized(classLoaderClass) {
			if (initSystemClassLoader) {
				initSystemClassLoader = false;

					String userLoader = System.internalGetProperties().getProperty("java.system.class.loader"); //$NON-NLS-1$
					if (userLoader != null) {
						try {
							Class<?> loaderClass = Class.forName(userLoader, true, applicationClassLoader);
							Constructor<?> constructor = loaderClass.getConstructor(new Class<?>[]{classLoaderClass});
							applicationClassLoader = (ClassLoader)constructor.newInstance(new Object[]{applicationClassLoader});
							/*[PR JAZZ103 87642] Setting -Djava.system.class.loader on the command line doesn't update VMAccess.setExtClassLoader() */
							/* Find the extension class loader */
							ClassLoader tempLoader = applicationClassLoader;
							while (tempLoader.parent != null) {
								tempLoader = tempLoader.parent;
							}
							VMAccess.setExtClassLoader(tempLoader);
						} catch (Throwable e) {
							if (e instanceof InvocationTargetException) {
								throw new Error(e.getCause());
							} else {
								throw new Error(e);
							}
						}
					}
			}
		}
	}
	
	ClassLoader sysLoader = applicationClassLoader;
	SecurityManager security = System.getSecurityManager();
	if (security != null) {	
		ClassLoader callersClassLoader = callerClassLoader();
		if (needsClassLoaderPermissionCheck(callersClassLoader, sysLoader)) {
			security.checkPermission(com.ibm.oti.util.RuntimePermissions.permissionGetClassLoader);
		}
	}

	return sysLoader;
}

/**
 * Answers an URL specifying a resource which can be found by 
 * looking up resName using the system class loader's resource 
 * lookup algorithm. 
 *
 * @return		URL
 *					a URL specifying a system resource or null.
 * @param		resName String
 *					the name of the resource to find.
 *
 * @see			Class#getResource
 */
public static URL getSystemResource(String resName) {
	return getSystemClassLoader().getResource(resName);
}

/**
 * Answers an Enumeration of URL containing all resources which can be
 * found by looking up resName using the system class loader's resource 
 * lookup algorithm. 
 *
 * @param		resName String
 *					the name of the resource to find.
 *
 * @return		Enumeration
 *					an Enumeration of URL containing the system resources
 *
 * @throws IOException when an error occurs
 */
public static Enumeration<URL> getSystemResources(String resName) throws IOException {
	return getSystemClassLoader().getResources(resName);
}

/**
 * Answers a stream on a resource found by looking up
 * resName using the system class loader's resource lookup
 * algorithm. Basically, the contents of the java.class.path
 * are searched in order, looking for a path which matches
 * the specified resource.
 *
 * @return		a stream on the resource or null.
 * @param		resName		the name of the resource to find.
 *
 * @see			Class#getResourceAsStream
 */
public static InputStream getSystemResourceAsStream(String resName) {
	return getSystemClassLoader().getResourceAsStream(resName);
}

/*[IF Sidecar19-SE]*/
/**
 * Answers the unnamed Module of this class loader.
 * 
 * @return the unnamed Module of this class loader 
 */
public final Module getUnnamedModule()
{	
	return this.unnamedModule;
}
/*[ENDIF] Sidecar19-SE */

/**
 * Invoked by the Virtual Machine when resolving class references.
 * Equivalent to loadClass(className, false);
 *
 * @return 		java.lang.Class
 *					the Class object.
 * @param 		className String
 *					the name of the class to search for.
 * @exception	ClassNotFoundException
 *					If the class could not be found.
 */
public Class<?> loadClass (String className) throws ClassNotFoundException {
/*[IF Sidecar19-SE]*/
	if ((bootstrapClassLoader == null) || (this == bootstrapClassLoader)) {
		Class<?> cls = VMAccess.findClassOrNull(className, bootstrapClassLoader);
		if (cls == null) {
			throw new ClassNotFoundException(className);
		}
		return cls;
	}
/*[ENDIF] Sidecar19-SE */	
	return loadClass(className, false);
}

/**
 * Attempts to load the type <code>className</code> in the running VM, 
 * optionally linking the type after a successful load.
 *
 * @return 		java.lang.Class
 *					the Class object.
 * @param 		className String
 *					the name of the class to search for.
 * @param 		resolveClass boolean
 *					indicates if class should be resolved after loading.
 * @exception	ClassNotFoundException
 *					If the class could not be found.
 */
/*[PR CMVC 180958] SVT:HRT:deadlock following class library change to classloader */
protected Class<?> loadClass(final String className, boolean resolveClass) throws ClassNotFoundException {
	return loadClassHelper(className, resolveClass, true
/*[IF Sidecar19-SE]*/
		, null
/*[ENDIF]*/
		);
}

/*[IF Sidecar19-SE]*/
/**
 * Invoked by package access methods such as java.lang.Package.getPackageInfo()
 * resolveClass flag is false, delegateToParent flag is false as well.
 *
 * @param 		module Module
 *					the module to search for
 * @param 		className String
 *					the name of the class to search for
 * @exception	NullPointerException
 *					if the given module is null
 * @return 		java.lang.Class
 *					the Class object
 */
final Class<?> loadClass(Module module, String className) {
	Class<?> localClass = null;
	
	if ((bootstrapClassLoader == null) || (this == bootstrapClassLoader)) {
		localClass = VMAccess.findClassOrNull(className, bootstrapClassLoader);
	} else {
		try {
			localClass = loadClassHelper(className, false, false, module);
		} catch (ClassNotFoundException e) {
			// returns null if the class can't be found
		}
	}
	return localClass;
}
/*[ENDIF]*/
/**
 * Attempts to load the type <code>className</code> in the running VM, 
 * optionally linking the type after a successful load.
 *
 * @param 		className String
 *					the name of the class to search for.
 * @param 		resolveClass boolean
 *					indicates if class should be resolved after loading.
 * @param		delegateToParent boolean
 * 					indicate if the parent classloader should be delegated for class loading
 * @param		module
 * 					the module from which the class is to be loaded.
 * @exception	ClassNotFoundException
 *					If the class could not be found.
 * @return 		java.lang.Class
 *					the Class object.
 */
Class<?> loadClassHelper(final String className, boolean resolveClass, boolean delegateToParent
/*[IF Sidecar19-SE]*/
	, Module module
/*[ENDIF]*/
	) throws ClassNotFoundException {
	Object lock = isParallelCapable ? getClassLoadingLock(className) : this;

	synchronized (lock) {
		// Ask the VM to look in its cache.
		Class<?> loadedClass = findLoadedClass(className);
		// search in parent if not found
		if (loadedClass == null) {
			if (delegateToParent) {
				try {
					if (parent == null) {
						/*[PR 95894]*/
						if (isDelegatingCL) {
							loadedClass = bootstrapClassLoader.findLoadedClass(className);
						}
						if (loadedClass == null) {
							loadedClass = bootstrapClassLoader.loadClass(className);
						}
					} else {
						if (isDelegatingCL) {
							loadedClass = parent.findLoadedClass(className);
						}
						if (loadedClass == null) {
							loadedClass = parent.loadClass(className, resolveClass);
						}
					}
				} catch (ClassNotFoundException e) {
					// don't do anything.  Catching this exception is the normal protocol for
					// parent classloaders telling use they couldn't find a class.
				}
			}
			
			// not findLoadedClass or by parent.loadClass, try locally
			if (loadedClass == null) {
/*[IF Sidecar19-SE]*/
				if (module == null) {
/*[ENDIF]*/
					loadedClass = findClass(className);
/*[IF Sidecar19-SE]*/
				}
				else {
					loadedClass = findClass(module.getName(), className);
				}
/*[ENDIF]*/
			}
		}

/*[IF Sidecar19-SE]*/
		if (module != null && loadedClass != null) {
			Module	moduleLoadedClass = loadedClass.getModule();
			if (module != moduleLoadedClass) {
				return null;
			}
		}
/*[ENDIF]*/
		
		// resolve if required
		if (resolveClass) resolveClass(loadedClass);
		return loadedClass;
	}
}

/**
 * Attempts to register the  the ClassLoader as being capable of 
 * parallel class loading.  This requires that all superclasses must
 * also be parallel capable.
 *
 * @return		True if the ClassLoader successfully registers as 
 * 				parallel capable, false otherwise.
 *
 * @see			java.lang.ClassLoader
 */
@CallerSensitive
protected static boolean registerAsParallelCapable() {
	final Class<?> callerCls = System.getCallerClass();
	
	if (parallelCapableCollection.containsKey(callerCls)) {
		return true;
	}

	Class<?> superCls = callerCls.getSuperclass();

	if (superCls == ClassLoader.class || parallelCapableCollection.containsKey(superCls)) {
		parallelCapableCollection.put(callerCls, null);
		return true;
	}
	
	return false;
}

/**
 * Answers the lock object for class loading in parallel. 
 * If this ClassLoader object has been registered as parallel capable, 
 * a dedicated object associated with this specified class name is returned.
 * Otherwise, current ClassLoader object is returned. 
 * 
 * @param 		className String
 *					name of the to be loaded class
 *
 * @return		the lock for class loading operations
 * 
 * @exception	NullPointerException
 *					if registered as parallel capable and className is null
 *
 * @see			java.lang.ClassLoader
 *
 */
protected Object getClassLoadingLock(final String className) {
	Object lock = this;
	if (isParallelCapable)	{
		if (classNameBasedLock == null) {
			synchronized(lazyInitLock) {
				if (classNameBasedLock == null) {
					classNameBasedLock = new Hashtable<>();
				} 
			}
		}
		synchronized(classNameBasedLock) {
			// get() does null pointer check
			ClassNameLockRef wf = classNameBasedLock.get(className);
			lock = (null != wf) ? wf.get() : null;
			if (lock == null) {		
				lock = new ClassNameBasedLock();
				classNameBasedLock.put(className, new ClassNameLockRef(lock, className, classNameBasedLock));
			}
		}
	}
	return lock;
}


/**
 * Forces a class to be linked (initialized).  If the class has
 * already been linked this operation has no effect.
 *
 * @param		clazz Class
 *					the Class to link.
 * @exception	NullPointerException
 *					if clazz is null.
 *
 * @see			Class#getResource
 */
protected final void resolveClass(Class<?> clazz) {
	if (clazz == null) 
		throw new NullPointerException();
}

/**
 * Forces the parent of a classloader instance to be newParent
 *
 * @param		newParent ClassLoader
 *					the ClassLoader to make the parent.
 */
private void setParent(ClassLoader newParent) {
	parent = newParent;
}

/**
 * Answers true if the receiver is a system class loader.
 * <p>
 * Note that this method has package visibility only. It is
 * defined here to avoid the security manager check in
 * getSystemClassLoader, which would be required to implement
 * this method anywhere else.
 *
 * @return		boolean
 *					true if the receiver is a system class loader
 *
 * @see Class#getClassLoaderImpl()
 */
final boolean isASystemClassLoader() {
/*[IF]
	1FDVFL7: J9JCL:ALL - isASystemClassLoader should check starting at appClassLoader.
/*[ENDIF]*/
	if (this == bootstrapClassLoader) return true;
	ClassLoader cl = applicationClassLoader;
	while (cl != null) {
		if (this == cl) return true;
		cl = cl.parent;
	}
	return false;
}

/**
 * Answers true if the receiver is ancestor of another class loader.
 * <p>
 * Note that this method has package visibility only. It is
 * defined here to avoid the security manager check in
 * getParent, which would be required to implement
 * this method anywhere else.
 *
 * @param		child	ClassLoader, a child candidate
 *
 * @return		boolean
 *					true if the receiver is ancestor of the parameter
 */
final boolean isAncestorOf (ClassLoader child) {
	if (child == null) return false;
	if (this == bootstrapClassLoader) return true;
	ClassLoader cl = child.parent;
	while (cl != null) {
		if (this == cl) return true;
		cl = cl.parent;
	}
	return false;
}


/**
 * A class loader 'callerClassLoader' can access class loader 'requested' without permission check
 * if any of the following are true
 * (1) if class loader 'callerClassLoader' is same as class loader 'requested' or 
 * (2) if 'callerClassLoader' is an ancestor of 'requested'. 
 * (3) a 'callerClassLoader' in a system domain can access any class loader.
 *
 * @param callerClassLoader the calling ClassLoader
 * @param requested the ClassLoader being requested
 * @return boolean indicating if a security check is required
 */
static final boolean needsClassLoaderPermissionCheck(ClassLoader callerClassLoader, ClassLoader requested) {
	return callerClassLoader != null &&
		callerClassLoader != requested && !callerClassLoader.isAncestorOf(requested);
}

/**
 * Answers an URL which can be used to access the resource
 * described by resName, using the class loader's resource lookup
 * algorithm. The default behavior is just to return null.
 * This should be implemented by a ClassLoader.
 *
 * @return		URL
 *					the location of the resource.
 * @param		resName String
 *					the name of the resource to find.
 */
protected URL findResource (String resName) {
	return null;
}

/**
 * Answers an Enumeration of URL which can be used to access the resources
 * described by resName, using the class loader's resource lookup
 * algorithm. The default behavior is just to return an empty Enumeration.
 *
 * @param		resName String
 *					the name of the resource to find.
 * @return		Enumeration
 *					the locations of the resources.
 *
 * @throws IOException when an error occurs
 */
protected Enumeration<URL> findResources (String resName) throws IOException {
	return new Vector<URL>().elements();
}

/**
 * Answers the absolute path of the file containing the library
 * associated with the given name, or null. If null is answered,
 * the system searches the directories specified by the system
 * property "java.library.path".
 *
 * @return		String
 *					the library file name or null.
 * @param		libName	String
 *					the name of the library to find.
 */
protected String findLibrary(String libName) {
	return null;
}

/**
 * Answers a package of given name defined by this ClassLoader.
 *
 * @param		name		The name of the package to find
 * 
 * @return		The package requested
 */
/*[IF Sidecar19-SE]*/
public
/*[ENDIF]*/
final Package getDefinedPackage(String name) {
	/*[IF Sidecar19-SE]*/
	Package pkg = null;
	synchronized(packages) {
		NamedPackage	np = packages.get(name);
		if (null != np) {
			if (np instanceof Package) {
				pkg = (Package)np;
			} else {
				pkg = NamedPackage.toPackage(np.packageName(), np.module());
				packages.put(name, pkg);
			}
		}
	}
	return pkg;
	/*[ELSE]*/
	return packages.get(name);
	/*[ENDIF] Sidecar19-SE*/
}

/*[IF Sidecar19-SE]*/
/**
 * Answers all the packages defined by this classloader.
 * 
 * @return Array of Package objects or zero length array if no package is defined
 */
public final Package[] getDefinedPackages() {
	synchronized(packages) {
		if (packages.isEmpty()) {
			return EMPTY_PACKAGE_ARRAY;
		} else {
			return packages().toArray(Package[]::new);
		}
	}
}
/*[ENDIF] Sidecar19-SE*/
/**
 * Attempt to locate the requested package. If no package information
 * can be located, null is returned.
 *
 * @param		name		The name of the package to find
 * @return		The package requested, or null
 * 
 * @deprecated Use getDefinedPackage(String)
 */
/*[IF Sidecar19-SE]*/
@Deprecated(forRemoval=false, since="9")
/*[ENDIF]*/
protected Package getPackage(String name) {
	if (this == bootstrapClassLoader) { 
/*[IF Sidecar19-SE]*/		
		return BootLoader.getDefinedPackage(name);
/*[ELSE]*/
		return getDefinedPackage(name);
/*[ENDIF]*/		
	} else {
		Package p = getDefinedPackage(name);
		if (p == null) {
			ClassLoader parentLoader = this.parent;
			if (parentLoader == null) {
				parentLoader = bootstrapClassLoader;
			}
			p = parentLoader.getPackage(name);
		} 
		return p;
	}
}

private Package[] getPackagesHelper(
		/*[IF Sidecar19-SE]*/
		Hashtable<?, NamedPackage>
		/*[ELSE]
		Hashtable<?, Package>
		/*[ENDIF] Sidecar19-SE*/
		localPackages, Package[] ancestorsPackages) {
	int resultSize = localPackages.size();
	if (ancestorsPackages != null) {
		resultSize += ancestorsPackages.length;
	}
	Package[] result = new Package[resultSize];
	int i = 0;
	if (ancestorsPackages != null) {
		i = ancestorsPackages.length;
		System.arraycopy(ancestorsPackages, 0, result, 0, i);
	}

	/*[IF Sidecar19-SE]*/
	Package[] pkgs = packages().toArray(Package[]::new);
	System.arraycopy(pkgs, 0, result, i, pkgs.length);
	/*[ELSE]
	Enumeration<Package> myPkgs = localPackages.elements(); 
	while (myPkgs.hasMoreElements()) {
		result[i++] = myPkgs.nextElement();
	}
	/*[ENDIF] Sidecar19-SE*/

	return result;
}

/**
 * Answers all the packages known to this class loader.
 *
 * @return		All the packages known to this classloader
 */
protected Package[] getPackages() {
/*[IF Sidecar19-SE]*/			
	if (this == bootstrapClassLoader) {
		return BootLoader.packages().toArray(Package[]::new);
	}
/*[ENDIF]*/			
	
	Package[] ancestorsPackages = null;
	if (parent == null) {
/*[IF !Sidecar19-SE]*/
		if (this != bootstrapClassLoader) {
/*[ENDIF]*/
			ancestorsPackages = bootstrapClassLoader.getPackages();
/*[IF !Sidecar19-SE]*/			
		}
/*[ENDIF]*/	
	} else {
		ancestorsPackages = parent.getPackages();
	}

	/*[IF Sidecar19-SE]*/
	Hashtable<?, NamedPackage> localPackages = packages;
	/*[ELSE]
	Hashtable<?, Package> localPackages = packages;
	/*[ENDIF] Sidecar19-SE*/

	boolean rtExceptionThrown = false;
	do {
		try {
			Package[] result;
			/*[IF Sidecar19-SE]*/			
			if (rtExceptionThrown) {
				synchronized(packages) {
					result = getPackagesHelper(localPackages, ancestorsPackages);
				}
			} else {
			/*[ENDIF] Sidecar19-SE*/	
				result = getPackagesHelper(localPackages, ancestorsPackages);
			/*[IF Sidecar19-SE]*/	
			}
			/*[ENDIF] Sidecar19-SE*/
			return result;
		} catch(RuntimeException ex) {
			if (rtExceptionThrown) {
				throw ex;
			}
			rtExceptionThrown = true;
			/*[IF !Sidecar19-SE]*/
			localPackages = (Hashtable<?, Package>)packages.clone();
			/*[ENDIF] Sidecar19-SE*/
		}
	} while (true);
}

/**
 * Define a new Package using the specified information.
 *
 * @param		name		The name of the package
 * @param		specTitle	The title of the specification for the Package
 * @param		specVersion	The version of the specification for the Package
 * @param		specVendor	The vendor of the specification for the Package
 * @param		implTitle	The implementation title of the Package
 * @param		implVersion	The implementation version of the Package
 * @param		implVendor	The specification vendor of the Package
 * @param		sealBase	The URL used to seal the Package, if null the Package is not sealed
 * 
 * @return		The Package created
 *
 * @exception	IllegalArgumentException if the Package already exists
 */
protected Package definePackage(
	final String name, final String specTitle, 
	final String specVersion, final String specVendor,
	final String implTitle, final String implVersion,
	final String implVendor, final URL sealBase)
	throws IllegalArgumentException
{
	synchronized(packages) {
		/*[IF Sidecar19-SE]*/
		if (packages.containsKey(name)) {
		/*[ELSE]		
		if (null != getPackage(name)) {
		/*[ENDIF]*/			
			/*[MSG "K0053", "Package {0} already defined."]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0053", name)); //$NON-NLS-1$
		} else {
			Package newPackage = new Package(name, specTitle, specVersion, specVendor, implTitle, implVersion, implVendor, sealBase, this);
			packages.put(name, newPackage);
			return newPackage;
		}
	}
}

/**
 * Gets the signers of a class.
 *
 * @param		c		The Class object
 * @return		signers	The signers for the class
 */
final Object[] getSigners(Class<?> c) {
	Hashtable<Class<?>, Object[]> localSigners = classSigners;
	if (localSigners == null) {
		synchronized (lazyInitLock) {
			localSigners = classSigners;
			if (localSigners == null) {
				return null;
			}
		}
	}
	Object[] result = localSigners.get(c);
	if (result != null) {
		result = result.clone();
	}
	return result;
}

/**
 * Sets the signers of a class.
 *
 * @param		c		The Class object
 * @param		signers	The signers for the class
 */

protected final void setSigners(final Class<?> c, final Object[] signers) {
	/*[PR CMVC 93861] setSigners() throws NullPointerException */
	if (c.getClassLoaderImpl() == this) {
		if (signers == null) {
			if (classSigners == null) {
				synchronized(lazyInitLock) {
					if (classSigners == null) {
						return;
					}
				}
			}
			classSigners.remove(c);
		} else {
			if (classSigners == null) {
				synchronized(lazyInitLock) {
					if (classSigners == null) {
						classSigners = new Hashtable<>();
					}
				}
			}
			classSigners.put(c, signers);	
		}
	/*[PR 28064] Class.getSigner() method returns null */
	/*[PR CMVC 93861] setSigners() throws NullPointerException */
	} else c.getClassLoaderImpl().setSigners(c, signers);

}

@CallerSensitive
static ClassLoader getCallerClassLoader() {
	ClassLoader loader = getStackClassLoader(2);
	if (loader == bootstrapClassLoader) return null;
	return loader;
}

/**
 * Returns the ClassLoader of the method (including natives) at the
 * specified depth on the stack of the calling thread. Frames representing
 * the VM implementation of java.lang.reflect are not included in the list.
 *
 * Notes: <ul>
 * 	 <li> This method operates on the defining classes of methods on stack.
 *		NOT the classes of receivers. </li>
 *
 *	 <li> The item at depth zero is the caller of this method </li>
 * </ul>
 *
 * @param depth the stack depth of the requested ClassLoader 
 * @return the ClassLoader at the specified depth
 * 
 * @see com.ibm.oti.vm.VM#getStackClassLoader
 */
@CallerSensitive
static final native ClassLoader getStackClassLoader(int depth);

/**
 * Returns the ClassLoader of the method that called the caller.
 * i.e. A.x() calls B.y() calls callerClassLoader(),
 * A's ClassLoader will be returned. Returns null for the
 * bootstrap ClassLoader.
 *
 * @return 		a ClassLoader or null for the bootstrap ClassLoader
 */
@CallerSensitive
static ClassLoader callerClassLoader() {
	ClassLoader loader = getStackClassLoader(2);
	if (loader == bootstrapClassLoader) return null;
	return loader;
}

/**
 * Loads and links the library specified by the argument.
 *
 * @param		libName		the name of the library to load
 * @param		loader		the classloader in which to load the library
 *
 * @exception	UnsatisfiedLinkError
 *							if the library could not be loaded
 * @exception	SecurityException
 *							if the library was not allowed to be loaded
 */
static synchronized void loadLibraryWithClassLoader(String libName, ClassLoader loader) {
	SecurityManager smngr = System.getSecurityManager();
	if (smngr != null)
		smngr.checkLink(libName);
	if (loader != null) {
		String realLibName = loader.findLibrary(libName);
		
		if (realLibName != null) {
			loadLibraryWithPath(realLibName, loader, null);
			return;
		}
	}
	/*
	* [PR JAZZ 93728] Match behaviour of System.loadLibrary() in reference
	* implementation when system property java.library.path is set 
	*/

	if (null == loader) {
		loadLibraryWithPath(libName, loader, System.internalGetProperties().getProperty("com.ibm.oti.vm.bootstrap.library.path")); //$NON-NLS-1$
	} else {
		try {
			loadLibraryWithPath(libName, loader, System.internalGetProperties().getProperty("java.library.path")); //$NON-NLS-1$
		} catch (UnsatisfiedLinkError ule) {
			loadLibraryWithPath(libName, loader, System.internalGetProperties().getProperty("com.ibm.oti.vm.bootstrap.library.path")); //$NON-NLS-1$
		}
	}
}

/**
 * Loads and links the library specified by the argument.
 * No security check is done.
 *
 * @param		libName			the name of the library to load
 * @param		loader			the classloader in which to load the library
 * @param		libraryPath		the library path to search, or null
 *
 * @exception	UnsatisfiedLinkError
 *							if the library could not be loaded
 */
static void loadLibraryWithPath(String libName, ClassLoader loader, String libraryPath) {
	/*[PR 137143] - failure to load library, need to strip leading '/' on Windows platforms, if present */
	if (File.separatorChar == '\\'){
		if (libName.startsWith("/") && com.ibm.oti.util.Util.startsWithDriveLetter(libName.substring(1))){ //$NON-NLS-1$
			libName = libName.substring(1);
		}
	}
	byte[] message = ClassLoader.loadLibraryWithPath(com.ibm.oti.util.Util.getBytes(libName), loader, libraryPath == null ? null : com.ibm.oti.util.Util.getBytes(libraryPath));
	if (message != null) {
		String error;
		try {
			error = com.ibm.oti.util.Util.convertFromUTF8(message, 0, message.length);
		} catch (java.io.IOException e) {
			error = com.ibm.oti.util.Util.toString(message);
		}
		throw new UnsatisfiedLinkError(libName + " (" + error + ")"); //$NON-NLS-1$ //$NON-NLS-2$
	}
}

private static native byte[] loadLibraryWithPath(byte[] libName, ClassLoader loader, byte[] libraryPath);

static void loadLibrary(Class<?> caller, String name, boolean fullPath) {
	if (fullPath)
		loadLibraryWithPath(name, caller.getClassLoaderImpl(), null);
	else
		loadLibraryWithClassLoader(name, caller.getClassLoaderImpl());
}

/**
 * Sets the assertion status of a class.
 *
 * @param		cname		Class name
 * @param		enable		Enable or disable assertion
 * 
 * @since 1.4
 */
public void setClassAssertionStatus(String cname, boolean enable) {
	if (!isParallelCapable) {
		synchronized(this) {
			setClassAssertionStatusHelper(cname, enable);
		}
	} else {
		synchronized(assertionLock) {
			setClassAssertionStatusHelper(cname, enable);
		}
	}
}

private void setClassAssertionStatusHelper(final String cname, final boolean enable) {
	if (classAssertionStatus == null ) {
		classAssertionStatus = new HashMap<>();
	}
	classAssertionStatus.put(cname, Boolean.valueOf(enable));
}

/**
 * Sets the assertion status of a package.
 *
 * @param		pname		Package name
 * @param		enable		Enable or disable assertion
 * 
 * @since 1.4
 */
public void setPackageAssertionStatus(String pname, boolean enable) {
	if (!isParallelCapable) {
		synchronized(this) {
			setPackageAssertionStatusHelper(pname, enable);
		}
	} else {
		synchronized(assertionLock) {
			setPackageAssertionStatusHelper(pname, enable);
		}
	}
}


private void setPackageAssertionStatusHelper(final String pname, final boolean enable) {
	if (packageAssertionStatus == null ) {
		packageAssertionStatus = new HashMap<>();
	}
	packageAssertionStatus.put(pname, Boolean.valueOf(enable));
		
}

 /**
 * Sets the default assertion status of a classloader
 *
 * @param		enable		Enable or disable assertion
 * 
 * @since 1.4
 */
public void setDefaultAssertionStatus(boolean enable){
	if (!isParallelCapable) {
		synchronized(this) {
			defaultAssertionStatus = enable;
		}
	} else {
		synchronized(assertionLock) {
			defaultAssertionStatus = enable;
		}
	}
}

/**
 * Clears the default, package and class assertion status of a classloader
 *
 * @since 1.4
 */
public void clearAssertionStatus(){
	if (!isParallelCapable) {
		synchronized(this) {
			defaultAssertionStatus = false;
			classAssertionStatus = null;
			packageAssertionStatus = null;
		}
	} else {
		synchronized(assertionLock) {
			defaultAssertionStatus = false;
			classAssertionStatus = null;
			packageAssertionStatus = null;
		}
	}
}

/**
 * Answers the assertion status of the named class
 * 
 * Returns the assertion status of the class or nested class if it has
 * been set. Otherwise returns the assertion status of its package or
 * superpackage if that has been set. Otherwise returns the default assertion
 * status.
 * Returns 1 for enabled and 0 for disabled.
 *
 * @param		cname	String
 *					the name of class.
 *
 * @return		int
 *					the assertion status. 
 *
 * @since 1.4
 */
boolean getClassAssertionStatus(String cname) {
	if (!isParallelCapable) {
		synchronized(this) {
			return getClassAssertionStatusHelper(cname);
		}
	} else {
		synchronized(assertionLock) {
			return getClassAssertionStatusHelper(cname);
		}
	}
}
private boolean getClassAssertionStatusHelper(String cname) {
	int dlrIndex = -1;
	
	if (classAssertionStatus != null) {
		Boolean b = classAssertionStatus.get(cname);
		if (b != null) {
			return b.booleanValue();
		} else if ((dlrIndex = cname.indexOf('$'))>0) {
			b = classAssertionStatus.get(cname.substring(0, dlrIndex));
			if (b !=null)
				return b.booleanValue();
		}
	}
	/*[PR CMVC 80253] package assertion status not checked */
	if ((dlrIndex = cname.lastIndexOf('.'))>0) {
		return getPackageAssertionStatus(cname.substring(0, dlrIndex));
	}
	return getDefaultAssertionStatus();
}


/**
 * Answers the assertion status of the named package
 * 
 * Returns the assertion status of the named package or superpackage if
 * that has been set. Otherwise returns the default assertion status.
 * Returns 1 for enabled and 0 for disabled.
 *
 * @param		pname	String
 *					the name of package.
 *
 * @return		int
 *					the assertion status.
 *
 * @since 1.4
 */
boolean getPackageAssertionStatus(String pname) {
	if (!isParallelCapable) {
		synchronized(this) {
			return getPackageAssertionStatusHelper(pname);
		}
	} else {
		synchronized(assertionLock) {
			return getPackageAssertionStatusHelper(pname);
		}
	}
}
private boolean getPackageAssertionStatusHelper(String pname) {
	int prdIndex = -1;
	
	if (packageAssertionStatus != null) {
		Boolean b = packageAssertionStatus.get(pname);
		if (b != null) {
			return b.booleanValue();
		} else if ((prdIndex = pname.lastIndexOf('.'))>0) {
			return getPackageAssertionStatus(pname.substring(0, prdIndex));
		}
	}
	return getDefaultAssertionStatus();
}

/**
 * Answers the default assertion status
 *
 * @return		boolean
 *					the default assertion status.
 *
 * @since 1.4
 */
boolean getDefaultAssertionStatus() {
	if (!isParallelCapable) {
		synchronized(this) {
			return defaultAssertionStatus;
		}
	} else {
		synchronized(assertionLock) {
			return defaultAssertionStatus;
		}
	}
}

/**
 * This sets the assertion status based on the commandline args to VM
 *
 * @since 1.4
 */
private void initializeClassLoaderAssertStatus() {
	/*[PR CMVC 130382] Optimize checking ClassLoader assertion status */
	/*[IF Sidecar19-SE]*/
	boolean bootLoader = bootstrapClassLoader == this;
	/*[ELSE]
	boolean bootLoader = bootstrapClassLoader == null;
	/*[ENDIF]*/

	if (!bootLoader && !checkAssertionOptions) {
		// if the bootLoader didn't find any assertion options, other
		// classloaders can skip the check for options
		return;
	}

	boolean foundAssertionOptions = false;
	String [] vmargs = com.ibm.oti.vm.VM.getVMArgs();
	for (int i=0; i<vmargs.length; i++) {
		if (!vmargs[i].startsWith("-e") && !vmargs[i].startsWith("-d")) { //$NON-NLS-1$ //$NON-NLS-2$
			continue;
		}
		// splice around :
		int indexColon = vmargs[i].indexOf(':');
		String vmargOptions, vmargExtraInfo;
		if ( indexColon == -1 ) {
			vmargOptions = vmargs[i];
			vmargExtraInfo = null;
		} else {
			vmargOptions = vmargs[i].substring(0, indexColon);	
			vmargExtraInfo = vmargs[i].substring(indexColon+1);	
		}
		if ( vmargOptions.compareTo("-ea") == 0 //$NON-NLS-1$
			|| vmargOptions.compareTo("-enableassertions") == 0  //$NON-NLS-1$
			|| vmargOptions.compareTo("-da") == 0  //$NON-NLS-1$
			|| vmargOptions.compareTo("-disableassertions") == 0  //$NON-NLS-1$
			) {
				foundAssertionOptions = true;
				boolean def = vmargOptions.charAt(1) == 'e';
				if (vmargExtraInfo == null) {
					if (bootLoader) {
						continue;
					}
					setDefaultAssertionStatus(def);
				} else {
					String str = vmargExtraInfo;
					int len = str.length();
					if ( len > 3 && str.charAt(len-1) == '.'  && 
						str.charAt(len-2) == '.' && str.charAt(len-3) == '.') {
						str = str.substring(0,len-3);
						setPackageAssertionStatus(str, def);
					} else {
						setClassAssertionStatus(str, def);
					}
				}
		} else if ( vmargOptions.compareTo("-esa") == 0  //$NON-NLS-1$
					|| vmargOptions.compareTo("-enablesystemassertions") == 0 //$NON-NLS-1$ 
					|| vmargOptions.compareTo("-dsa") == 0  //$NON-NLS-1$
					|| vmargOptions.compareTo("-disablesystemassertions") == 0  //$NON-NLS-1$
		) {
			if (bootLoader) {
				boolean def = vmargOptions.charAt(1) == 'e';
				setDefaultAssertionStatus(def);
			}
		}

	}
	if (bootLoader && foundAssertionOptions) {
		// assertion options found, every classloader must check the options
		checkAssertionOptions = true;
	}
}

/**
 * Constructs a new class from an array of bytes containing a 
 * class definition in class file format and assigns the new
 * class to the specified protection domain.
 *
 * @param 		name java.lang.String
 *					the name of the new class.
 * @param 		buffer
 *					a memory image of a class file.
 * @param 		domain
 *					the protection domain this class should
 *					belong to.
 *
 * @return	the newly defined Class
 * 
 * @throws ClassFormatError when the bytes are invalid
 * 
 * @since 1.5
 */
protected final Class<?> defineClass(String name, java.nio.ByteBuffer buffer, ProtectionDomain domain) throws ClassFormatError {
	int position = buffer.position();
	int size = buffer.limit() - position;
	if (buffer.hasArray())
		return defineClass(name, buffer.array(), position, size, domain);

	byte[] bytes = new byte[size];
	buffer.get(bytes);
	return defineClass(name, bytes, 0, size, domain);
}

/**
 * Check if all the certs in one array are present in the other array
 * @param	pcerts	java.security.cert.Certificate[] 
 * @param	certs	java.security.cert.Certificate[]
 * @return	true when all the certs in one array are present in the other array
 * 			false otherwise
 */
private boolean compareCerts(java.security.cert.Certificate[] pcerts,
		java.security.cert.Certificate[] certs) {
	/*[PR CMVC 157478] compareCerts function missing in java.lang.ClassLoader class */
	if (pcerts == null) {
		pcerts = emptyCertificates;
	}
	if (certs == null) {
		certs = emptyCertificates;
	}
	if (pcerts == certs) {
		return true;
	} else if (pcerts.length != certs.length) {
		return false;
	} 
	
	boolean foundMatch = true;
	test: for(int i=0; i<pcerts.length; i++) {
		if (pcerts[i] == certs[i])	continue;
		if (pcerts[i].equals(certs[i]))	continue;
		for(int j=0; j<certs.length; j++) {
			if (j == i) continue;
			if (pcerts[i] == certs[j])	continue test;
			if (pcerts[i].equals(certs[j]))	continue test;
		}
		foundMatch = false;
		break;
	}
	return foundMatch;
}

//prevent subclasses from becoming Cloneable
@Override
protected Object clone() throws CloneNotSupportedException {
	throw new CloneNotSupportedException();
}

/**
 * Returns a {@code java.util.Map} from method descriptor string to the equivalent {@code MethodType} as generated by {@code MethodType.fromMethodDescriptorString}.
 * @return A {@code java.util.Map} from method descriptor string to the equivalent {@code MethodType}.
 */
Map<String, java.lang.invoke.MethodType> getMethodTypeCache() {
	if (null == methodTypeFromMethodDescriptorStringCache) {
		methodTypeFromMethodDescriptorStringCache = new java.util.concurrent.ConcurrentHashMap<>(16, 0.75f, 8);
	}
	return methodTypeFromMethodDescriptorStringCache;
}

/*[IF Sidecar19-SE]*/	
ServicesCatalog createOrGetServicesCatalog() {
	if (null == this.servicesCatalog) {
		this.servicesCatalog = ServicesCatalog.create();
	}
	return this.servicesCatalog;
}

ServicesCatalog getServicesCatalog() {
	return this.servicesCatalog;
}

/*[ENDIF] Sidecar19-SE*/

/*[IF Sidecar19-SE]*/
/**
 * Answers an URL which can be used to access the resource
 * described by resName, using the class loader's resource lookup
 * algorithm. By default, return null, unless moduleName is null,
 * in which case return findResource(resName).
 * This should be implemented by a ClassLoader.
 *
 * @return		URL
 *					the location of the resource.
 * @param		moduleName String
 * 					the module name
 * @param		resName String
 *					the name of the resource to find.
 */
protected URL findResource(String moduleName, String resName) throws IOException {
	URL result = null;
	if (null == moduleName) {
		/* Handle the default case for subclasses which do not implement this method */
		result = findResource(resName);
	}
	return result;
}

Package definePackage(String name, Module module) {
	Package	pkg = null;
	if (name.isEmpty() && module.isNamed()) {
		throw new InternalError("Unnamed package in " + module); //$NON-NLS-1$
	}
	synchronized(packages) {
		NamedPackage np = packages.get(name);
		if ((null != np) && (np instanceof Package)) {
			pkg = (Package)np;
		} else {
			pkg = NamedPackage.toPackage(name, module);
			packages.put(name, pkg);
		}
	}
	
	return pkg;
}
Stream<Package> packages() {
	return packages.values().stream().map(p->definePackage(p.packageName(), p.module()));
}
static ClassLoader getBuiltinAppClassLoader() {
	throw new Error("ClassLoader.getBuiltinAppClassLoader unimplemented"); //$NON-NLS-1$
}
static ClassLoader initSystemClassLoader() {
	throw new Error("ClassLoader.initSystemClassLoader unimplemented"); //$NON-NLS-1$
}
ConcurrentHashMap<?, ?> createOrGetClassLoaderValueMap() {
	if (null == classLoaderValueMap) {
		synchronized(lazyInitLock) {
			if (null == classLoaderValueMap) {
				classLoaderValueMap = new ConcurrentHashMap<>();
			}
		}
	}
	return classLoaderValueMap;
}

/**
 * Answers a stream of URL which can be used to access the resources
 * described by name, using the class loader's resource lookup algorithm.
 *
 * @param name - the name of the resource to find

 * @return a stream of the resources
 *
 * @throws NullPointerException when name is null
 */
public Stream<URL> resources(String name) {
	try {
		return StreamSupport.stream(Spliterators.spliteratorUnknownSize(getResources(name).asIterator(), Spliterator.NONNULL | Spliterator.IMMUTABLE), false);
	} catch (IOException e) {
		throw new UncheckedIOException(e);
	}
}

/**
 * Indicating if this class loader is registered as parallel capable or not
 *
 * @return true if this is registered as parallel capable, otherwise false is returned
 */
public final boolean isRegisteredAsParallelCapable() {
	return isParallelCapable;
}

/*[ENDIF] Sidecar19-SE*/
}

