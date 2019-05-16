/*[INCLUDE-IF Sidecar16]*/
package java.lang;

/*******************************************************************************
 * Copyright (c) 1998, 2019 IBM Corp. and others
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

import java.util.Map;
import java.security.AccessControlContext;
import java.security.AccessController;
import java.security.PrivilegedAction;
/*[IF Sidecar19-SE]
import jdk.internal.reflect.CallerSensitive;
/*[ELSE]*/
import sun.reflect.CallerSensitive;
/*[ENDIF]*/

/**
 *	A Thread is a unit of concurrent execution in Java. It has its own call stack
 * for methods being called and their parameters. Threads in the same VM interact and
 * synchronize by the use of shared Objects and monitors associated with these objects.
 * Synchronized methods and part of the API in Object also allow Threads to cooperate.
 *
 *	When a Java program starts executing there is an implicit Thread (called "main")
 * which is automatically created by the VM. This Thread belongs to a ThreadGroup
 * (also called "main") which is automatically created by the bootstrap sequence by
 * the VM as well.
 *
 * @see			java.lang.Object
 * @see			java.lang.ThreadGroup
 */
public class Thread implements Runnable {

/* Maintain thread shape in all configs */

	/**
	 * The maximum priority value for a Thread.
	 */
	public final static int MAX_PRIORITY = 10;		// Maximum allowed priority for a thread
	/**
	 * The minimum priority value for a Thread.
	 */
	public final static int MIN_PRIORITY = 1;			// Minimum allowed priority for a thread
	/**
	 * The default priority value for a Thread.
	 */
	public final static int NORM_PRIORITY = 5;		// Normal priority for a thread
	/*[PR 97331] Initial thread name should be Thread-0 */
	private static int createCount = -1;					// Used internally to compute Thread names that comply with the Java specification	
	/*[PR 122459] LIR646 - Remove use of generic object for synchronization */
	private static final class TidLock {
		TidLock() {}
	}
	private static Object tidLock = new TidLock();
	private static long tidCount = 1;
	private static final int NANOS_MAX = 999999;		// Max value for nanoseconds parameter to sleep and join
	private static final int INITIAL_LOCAL_STORAGE_CAPACITY = 5;	// Initial number of local storages when the Thread is created
	static final long NO_REF = 0;				// Symbolic constant, no threadRef assigned or already cleaned up

	// Instance variables
	private long threadRef;									// Used by the VM
	long stackSize = 0;
	private volatile boolean started;				// If !isAlive(), tells if Thread died already or hasn't even started
	private String name;						// The Thread's name
	private int priority = NORM_PRIORITY;			// The Thread's current priority
	private boolean isDaemon;				// Tells if the Thread is a daemon thread or not.

	ThreadGroup group;			// A Thread belongs to exactly one ThreadGroup
	private Runnable runnable;				// Target (optional) runnable object
	private boolean stopCalled = false;			// Used by the VM
/*[PR 1FENTZW]*/
	private ClassLoader contextClassLoader;	// Used to find classes and resources in this Thread
	ThreadLocal.ThreadLocalMap threadLocals;
	private java.security.AccessControlContext inheritedAccessControlContext;

	/*[PR 96127]*/
	/*[PR 122459] LIR646 - Remove use of generic object for synchronization */
	private static final class ThreadLock {}
	private Object lock = new ThreadLock();

	ThreadLocal.ThreadLocalMap inheritableThreadLocals;
	private volatile sun.nio.ch.Interruptible blockOn;
	
	int threadLocalsIndex;
	int inheritableThreadLocalsIndex;

	/*[PR 113602] Thread fields should be volatile */
	private volatile UncaughtExceptionHandler exceptionHandler;
	private long tid;

	volatile Object parkBlocker;

	private static ThreadGroup systemThreadGroup;		// Assigned by the vm
	private static ThreadGroup mainGroup;				// ThreadGroup where the "main" Thread starts
	
	/*[PR 113602] Thread fields should be volatile */
	private volatile static UncaughtExceptionHandler defaultExceptionHandler;
	
	/*[PR CMVC 196696] Build error:java.lang.Thread need extra fields */
	long threadLocalRandomSeed;
	int threadLocalRandomProbe;
	int threadLocalRandomSecondarySeed;

/**
 * 	Constructs a new Thread with no runnable object and a newly generated name.
 * The new Thread will belong to the same ThreadGroup as the Thread calling
 * this constructor.
 *
 * @see			java.lang.ThreadGroup
 */
public Thread() {
	this(null, null, newName(), null, true);
}

/**
 *
 * Private constructor to be used by the VM for the threads attached through JNI.
 * They already have a running thread with no associated Java Thread, so this is
 * where the binding is done.
 *
 * @param		vmName			Name for the Thread being created (or null to auto-generate a name)
 * @param		vmThreadGroup	ThreadGroup for the Thread being created (or null for main threadGroup)
 * @param		vmPriority		Priority for the Thread being created
 * @param		vmIsDaemon		Indicates whether or not the Thread being created is a daemon thread
 *
 * @see			java.lang.ThreadGroup
 */
private Thread(String vmName, Object vmThreadGroup, int vmPriority, boolean vmIsDaemon) {
	super();

	String threadName = (vmName == null) ? newName() : vmName;
	setNameImpl(threadRef, threadName);
	name = threadName;
	
	isDaemon = vmIsDaemon;
	priority = vmPriority;	// If we called setPriority(), it would have to be after setting the ThreadGroup (further down),
							// because of the checkAccess() call (which requires the ThreadGroup set). However, for the main
							// Thread or JNI-C attached Threads we just trust the value the VM is passing us, and just assign.

	ThreadGroup threadGroup = null;
	boolean booting = false;
	if (mainGroup == null) { // only occurs during bootstrap
		booting = true;
		/*[PR CMVC 71192] Initialize the "main" thread group without calling checkAccess() */
		mainGroup = new ThreadGroup(systemThreadGroup);
	}
	threadGroup = vmThreadGroup == null ? mainGroup : (ThreadGroup)vmThreadGroup;
	
	/*[PR 1FEVFSU] The rest of the configuration/initialization is shared between this constructor and the public one */
	initialize(booting, threadGroup, null, null, true);	// no parent Thread
	/*[PR 115667, CMVC 94448] In 1.5 and CDC/Foundation 1.1, thread is added to ThreadGroup when started */
 	this.group.add(this);
 	
	/*[PR 100718] Initialize System.in after the main thread */
	if (booting) {
		System.completeInitialization();
	}
}

/*
 * Called after everything else is initialized.
 */
void completeInitialization() {
	// Get the java.system.class.loader
	/*[PR CMVC 99755] Implement -Djava.system.class.loader option */
	contextClassLoader = ClassLoader.getSystemClassLoader();
	/*[IF Sidecar19-SE]*/
	jdk.internal.misc.VM.initLevel(4);
	/*[ELSE]*/ // Sidecar19-SE
	sun.misc.VM.booted();
	/*[ENDIF]*/ // Sidecar19-SE
	/*[IF Sidecar19-SE|Sidecar18-SE-OpenJ9]*/
	System.startSNMPAgent();
	/*[ENDIF]*/ // Sidecar19-SE|Sidecar18-SE-OpenJ9
}

/**
 * 	Constructs a new Thread with a runnable object and a newly generated name.
 * The new Thread will belong to the same ThreadGroup as the Thread calling
 * this constructor.
 *
 * @param		runnable		a java.lang.Runnable whose method <code>run</code> will be executed by the new Thread
 *
 * @see			java.lang.ThreadGroup
 * @see			java.lang.Runnable
 */
public Thread(Runnable runnable) {
	this(null, runnable, newName(), null, true);
}

/*
 * [PR CMVC 199693] Prevent a trusted method chain attack.
 */

/**
 * Constructs a new Thread with a runnable object and a newly generated name,
 * setting the specified AccessControlContext.
 * The new Thread will belong to the same ThreadGroup as the Thread calling
 * this constructor.
 *
 * @param		runnable		a java.lang.Runnable whose method <code>run</code> will be executed by the new Thread
 * @param		acc				the AccessControlContext to use for the Thread
 *
 * @see			java.lang.ThreadGroup
 * @see			java.lang.Runnable
 */
Thread(Runnable runnable, AccessControlContext acc) {
	this(null, runnable, newName(), acc, true);
}

/**
 * Constructs a new Thread with a runnable object and name provided.
 * The new Thread will belong to the same ThreadGroup as the Thread calling
 * this constructor.
 *
 * @param		runnable		a java.lang.Runnable whose method <code>run</code> will be executed by the new Thread
 * @param		threadName		Name for the Thread being created
 *
 * @see			java.lang.ThreadGroup
 * @see			java.lang.Runnable
 */
public Thread(Runnable runnable, String threadName) {
	this(null, runnable, threadName, null, true);
}


/**
 * 	Constructs a new Thread with no runnable object and the name provided.
 * The new Thread will belong to the same ThreadGroup as the Thread calling
 * this constructor.
 *
 * @param		threadName		Name for the Thread being created
 *
 * @see			java.lang.ThreadGroup
 * @see			java.lang.Runnable
 */
public Thread(String threadName) {
	this(null, null, threadName, null, true);
}

/**
 * 	Constructs a new Thread with a runnable object and a newly generated name.
 * The new Thread will belong to the ThreadGroup passed as parameter.
 *
 * @param		group			ThreadGroup to which the new Thread will belong
 * @param		runnable		a java.lang.Runnable whose method <code>run</code> will be executed by the new Thread
 *
 * @exception	SecurityException
 *					if <code>group.checkAccess()</code> fails with a SecurityException
 * @exception	IllegalThreadStateException
 *					if <code>group.destroy()</code> has already been done
 *
 * @see			java.lang.ThreadGroup
 * @see			java.lang.Runnable 
 * @see			java.lang.SecurityException
 * @see			java.lang.SecurityManager
 */
public Thread(ThreadGroup group, Runnable runnable) {
	this(group, runnable, newName(), null, true);
}

/**
 * 	Constructs a new Thread with a runnable object, the given name and
 * belonging to the ThreadGroup passed as parameter.
 *
 * @param		group			ThreadGroup to which the new Thread will belong
 * @param		runnable		a java.lang.Runnable whose method <code>run</code> will be executed by the new Thread
 * @param		threadName		Name for the Thread being created
 * @param		stack			Platform dependent stack size
 *
 * @exception	SecurityException
 *					if <code>group.checkAccess()</code> fails with a SecurityException
 * @exception	IllegalThreadStateException
 *					if <code>group.destroy()</code> has already been done
 *
 * @since 1.4
 *
 * @see			java.lang.ThreadGroup
 * @see			java.lang.Runnable 
 * @see			java.lang.SecurityException
 * @see			java.lang.SecurityManager
 */
public Thread(ThreadGroup group, Runnable runnable, String threadName, long stack) {
	this(group, runnable, threadName, null, true);
	this.stackSize = stack;
}

/*[IF Sidecar19-SE]*/
/**
 * Constructs a new Thread with a runnable object, the given name, the thread stack size,
 * the flag to inherit initial values for inheritable thread-local variables and
 * belonging to the ThreadGroup passed as parameter.
 *
 * @param		group					ThreadGroup to which the new Thread will belong
 * @param		runnable				A java.lang.Runnable whose method <code>run</code> will be executed by the new Thread
 * @param		threadName				Name for the Thread being created
 * @param		stack					Platform dependent stack size
 * @param       inheritThreadLocals		A boolean indicating whether to inherit initial values for inheritable thread-local variables.
 *
 * @exception	SecurityException
 *					if <code>group.checkAccess()</code> fails with a SecurityException
 * @exception	IllegalThreadStateException
 *					if <code>group.destroy()</code> has already been done
 *
 */
public Thread(ThreadGroup group, Runnable runnable, String threadName, long stack, boolean inheritThreadLocals)	{
	this(group, runnable, threadName, null, inheritThreadLocals);
	this.stackSize = stack;
}
/*[ENDIF]*/

/**
 * 	Constructs a new Thread with a runnable object, the given name and
 * belonging to the ThreadGroup passed as parameter.
 *
 * @param		group			ThreadGroup to which the new Thread will belong
 * @param		runnable		a java.lang.Runnable whose method <code>run</code> will be executed by the new Thread
 * @param		threadName		Name for the Thread being created
 *
 * @exception	SecurityException
 *					if <code>group.checkAccess()</code> fails with a SecurityException
 * @exception	IllegalThreadStateException
 *					if <code>group.destroy()</code> has already been done
 *
 * @see			java.lang.ThreadGroup
 * @see			java.lang.Runnable 
 * @see			java.lang.SecurityException
 * @see			java.lang.SecurityManager
 */
public Thread(ThreadGroup group, Runnable runnable, String threadName) {
	this(group, runnable, threadName, null, true);
}

Thread(Runnable runnable, String threadName, boolean isSystemThreadGroup, boolean inheritThreadLocals, boolean isDaemon, ClassLoader contextClassLoader) {
	this(isSystemThreadGroup ? systemThreadGroup : null, runnable, threadName, null, inheritThreadLocals);
	this.isDaemon = isDaemon;
	this.contextClassLoader = contextClassLoader;
}

private Thread(ThreadGroup group, Runnable runnable, String threadName, AccessControlContext acc, boolean inheritThreadLocals) {
	super();
	/*[PR 1FEVFSU] Re-arrange method so that common code to this constructor and the private one the VM calls can be put in a separate method */
	/*[PR 1FIGT59] name cannot be null*/
	if (threadName==null) throw new NullPointerException();
	this.name = threadName;		// We avoid the public API 'setName', since it does redundant work (checkAccess)
	this.runnable = runnable;	// No API available here, so just direct access to inst. var.
	Thread currentThread  = currentThread();
	
	this.isDaemon = currentThread.isDaemon(); // We avoid the public API 'setDaemon', since it does redundant work (checkAccess)
	
/*[PR 1FEO92F] (dup of 1FC0TRN) */
	if (group == null) {
		SecurityManager currentManager = System.getSecurityManager();
		 // if there is a security manager...
		if (currentManager != null)
			// Ask SecurityManager for ThreadGroup
			group = currentManager.getThreadGroup();
	}
	/*[PR 94235]*/
	if (group == null)
		// Same group as Thread that created us
		group = currentThread.getThreadGroup();
	

	/*[PR 1FEVFSU] The rest of the configuration/initialization is shared between this constructor and the private one */
	initialize(false, group, currentThread, acc, inheritThreadLocals);
	
	setPriority(currentThread.getPriority());	// In this case we can call the public API according to the spec - 20.20.10
}

/**
 * 	Initialize the thread according to its parent Thread and the ThreadGroup
 * where it should be added.
 *
 * @param		booting					Indicates if the JVM is booting up, i.e. if the main thread is being attached
 * @param		threadGroup	ThreadGroup		The ThreadGroup to which the receiver is being added.
 * @param		parentThread	Thread	The creator Thread from which to inherit some values like local storage, etc.
 *										If null, the receiver is either the main Thread or a JNI-C attached Thread.
 * @param		acc						The AccessControlContext. If null, use the current context
 * @param       inheritThreadLocals		A boolean indicating whether to inherit initial values for inheritable thread-local variables.
 */
private void initialize(boolean booting, ThreadGroup threadGroup, Thread parentThread, AccessControlContext acc, boolean inheritThreadLocals) {
	synchronized (tidLock) {
		tid = tidCount++;
	}

	/*[PR 96408]*/
	this.group = threadGroup;
	
	
	if (booting) {
		System.afterClinitInitialization();
	}
	
	
	// initialize the thread local storage before making other calls
	if (parentThread != null) { // Non-main thread
		if (inheritThreadLocals && (null != parentThread.inheritableThreadLocals)) {
			inheritableThreadLocals = ThreadLocal.createInheritedMap(parentThread.inheritableThreadLocals);
		}
		
		/*[PR CMVC 90230] enableContextClassLoaderOverride check added in 1.5 */
		final SecurityManager sm = System.getSecurityManager();
		final Class implClass = getClass();
		final Class thisClass = Thread.class;
		if (sm != null && implClass != thisClass) {
			boolean override = ((Boolean)AccessController.doPrivileged(new PrivilegedAction() {
				public Object run() {			
					try {
						java.lang.reflect.Method method = implClass.getMethod("getContextClassLoader", new Class[0]); //$NON-NLS-1$
						if (method.getDeclaringClass() != thisClass) {
							return Boolean.TRUE;
						}
					} catch (NoSuchMethodException e) {
					}
					try {
						java.lang.reflect.Method method = implClass.getDeclaredMethod("setContextClassLoader", new Class[]{ClassLoader.class}); //$NON-NLS-1$
						if (method.getDeclaringClass() != thisClass) {
							return Boolean.TRUE;
						}
					} catch (NoSuchMethodException e) {
					}
					return Boolean.FALSE;
				}
			})).booleanValue();
			if (override) {
				sm.checkPermission(com.ibm.oti.util.RuntimePermissions.permissionEnableContextClassLoaderOverride);
			}
		}
		// By default a Thread "inherits" the context ClassLoader from its creator
		/*[PR CMVC 90230] behavior change in 1.5, call getContextClassLoader() instead of accessing field */
		contextClassLoader = parentThread.getContextClassLoader();
	} else { // no parent: main thread, or one attached through JNI-C
		/*[PR 111189] Do not initialize ClassLoaders in a static initializer */
		if (booting) {
			// Preload and initialize the JITHelpers class
			try {
				Class.forName("com.ibm.jit.JITHelpers"); //$NON-NLS-1$
			} catch(ClassNotFoundException e) {
				// Continue silently if the class can't be loaded and initialized for some reason,
				// The JIT will tolerate this.
			}

			// Explicitly initialize ClassLoaders, so ClassLoader methods (such as
			// ClassLoader.callerClassLoader) can be used before System is initialized 
			ClassLoader.initializeClassLoaders();
		}
		// Just set the context class loader
		contextClassLoader = ClassLoader.getSystemClassLoader();
	}	

	threadGroup.checkAccess();
	/*[PR 115667, CMVC 94448] In 1.5 and CDC/Foundation 1.1, thread is added to ThreadGroup when started */
	threadGroup.checkNewThread(this);

	inheritedAccessControlContext = acc == null ? AccessController.getContext() : acc;
}

/**
 * 	Constructs a new Thread with no runnable object, the given name and
 * belonging to the ThreadGroup passed as parameter.
 *
 * @param		group			ThreadGroup to which the new Thread will belong
 * @param		threadName		Name for the Thread being created
 *
 * @exception	SecurityException
 *					if <code>group.checkAccess()</code> fails with a SecurityException
 * @exception	IllegalThreadStateException
 *					if <code>group.destroy()</code> has already been done
 *
 * @see			java.lang.ThreadGroup
 * @see			java.lang.SecurityException
 * @see			java.lang.SecurityManager
 */
public Thread(ThreadGroup group, String threadName) {
	this(group, null, threadName, null, true);
}

/**
 * Returns how many threads are active in the <code>ThreadGroup</code>
 * which the current thread belongs to.
 * 
 * @return Number of Threads
 */
public static int activeCount(){
	/*[PR CMVC 93001] changed in 1.5 to only count active threads */
	return currentThread().getThreadGroup().activeCount();	
}


/**
 * This method is used for operations that require approval from
 * a SecurityManager. If there's none installed, this method is a no-op.
 * If there's a SecurityManager installed , <code>checkAccess(Ljava.lang.Thread;)</code>
 * is called for that SecurityManager.
 *
 * @see			java.lang.SecurityException
 * @see			java.lang.SecurityManager
 */
public final void checkAccess() {
	SecurityManager currentManager = System.getSecurityManager();
	if (currentManager != null) currentManager.checkAccess(this);
}

/**
 * 	Returns the number of stack frames in this thread.
 *
 * @return		Number of stack frames
 *
 * @deprecated	The semantics of this method are poorly defined and it uses the deprecated suspend() method.
 */
/*[IF Sidecar19-SE]*/
@Deprecated(forRemoval=true, since="1.2")
/*[ELSE]*/
@Deprecated
/*[ENDIF]*/
public int countStackFrames() {
	return 0;
}

/**
 * Answers the instance of Thread that corresponds to the running Thread
 * which calls this method.
 *
 * @return		a java.lang.Thread corresponding to the code that called <code>currentThread()</code>
 */
public static native Thread currentThread();

/*[IF !Java11]*/
/**
 * 	Destroys the receiver without any monitor cleanup. Not implemented.
 * 
 * @deprecated May cause deadlocks.
 */
/*[IF Sidecar19-SE]*/
@Deprecated(forRemoval=true, since="1.5")
/*[ELSE]*/
@Deprecated
/*[ENDIF]*/
public void destroy() {
	/*[PR 121318] Should throw NoSuchMethodError */
	throw new NoSuchMethodError();
}
/*[ENDIF]*/


/**
 * 	Prints a text representation of the stack for this Thread.
 */
public static void dumpStack() {
	new Throwable().printStackTrace();
}

/**
 * 	Copies an array with all Threads which are in the same ThreadGroup as
 * the receiver - and subgroups - into the array <code>threads</code>
 * passed as parameter. If the array passed as parameter is too small no
 * exception is thrown - the extra elements are simply not copied.
 *
 * @param		threads	array into which the Threads will be copied
 *
 * @return		How many Threads were copied over
 *
 * @exception	SecurityException	
 *					if the installed SecurityManager fails <code>checkAccess(Ljava.lang.Thread;)</code>
 *
 * @see			java.lang.SecurityException
 * @see			java.lang.SecurityManager
 */
public static int enumerate(Thread[] threads) {
	return currentThread().getThreadGroup().enumerate(threads, true);
}


/**
 * 	Returns the context ClassLoader for the receiver.
 *
 * @return		ClassLoader		The context ClassLoader
 *
 * @see			java.lang.ClassLoader
 * @see			#getContextClassLoader()
 */
@CallerSensitive
public ClassLoader getContextClassLoader() {
/*[PR 1FCA807]*/
/*[PR 1FDTAMT] use callerClassLoader()*/
	SecurityManager currentManager = System.getSecurityManager();
	 // if there is a security manager...
	if (currentManager != null) {
		ClassLoader callerClassLoader = ClassLoader.callerClassLoader();
		if (ClassLoader.needsClassLoaderPermissionCheck(callerClassLoader, contextClassLoader)) {
			currentManager.checkPermission(com.ibm.oti.util.RuntimePermissions.permissionGetClassLoader);
		}	
	}	
	return contextClassLoader;
}

/**
 * Answers the name of the receiver.
 *
 * @return		the receiver's name (a java.lang.String)
 */
public final String getName() {
	/*[PR 1FIGT59] Return name as a String. If null, return "null" */
	return String.valueOf(name);
}

/**
 * Answers the priority of the receiver.
 *
 * @return		the receiver's priority (an <code>int</code>)
 *
 * @see			Thread#setPriority
 */
public final int getPriority() {
	return priority;
}

/**
 * Answers the ThreadGroup to which the receiver belongs
 *
 * @return		the receiver's ThreadGroup
 */
public final ThreadGroup getThreadGroup() {
	return group;
}


/**
 * Posts an interrupt request to the receiver
 *
 * @exception	SecurityException
 *					if <code>group.checkAccess()</code> fails with a SecurityException
 *
 * @see			java.lang.SecurityException
 * @see			java.lang.SecurityManager
 * @see			Thread#interrupted
 * @see			Thread#isInterrupted 
 */
public void interrupt() {
	SecurityManager currentManager = System.getSecurityManager();

	if (currentManager != null) {
		if (currentThread() != this) {
			currentManager.checkAccess(this);
		}
	}
	
	synchronized(lock) {
		interruptImpl();
		sun.nio.ch.Interruptible localBlockOn = blockOn;
		if (localBlockOn != null) {
			localBlockOn.interrupt(this);
		}
	}
}


/**
 * Answers a <code>boolean</code> indicating whether the current Thread
 * (<code>currentThread()</code>) has a pending interrupt request
 * (<code>true</code>) or not (<code>false</code>). It also has the
 * side-effect of clearing the flag.
 *
 * @return		a <code>boolean</code>
 *
 * @see			Thread#currentThread 
 * @see			Thread#interrupt 
 * @see			Thread#isInterrupted
 */
public static native boolean interrupted();

/**
 * Posts an interrupt request to the receiver
 *
 * @see			Thread#interrupted
 * @see			Thread#isInterrupted 
 */
private native void interruptImpl();

/**
 * Answers <code>true</code> if the receiver has
 * already been started and still runs code (hasn't died yet).
 * Answers <code>false</code> either if the receiver hasn't been
 * started yet or if it has already started and run to completion and died.
 *
 * @return		a <code>boolean</code>
 *
 * @see			Thread#start
 */
public final boolean isAlive() {
	synchronized(lock) {
		/*[PR CMVC 88976] the Thread is alive until cleanup() is called */
		return threadRef != NO_REF;
	}
}

/*[PR 1FJMO7Q] A Thread can be !isAlive() and still be in its ThreadGroup */
/**
 * Answers <code>true</code> if the receiver has
 * already died and been removed from the ThreadGroup
 * where it belonged.
 *
 * @return		a <code>boolean</code>
 *
 * @see			Thread#start
 * @see			Thread#isAlive
 */
private boolean isDead() {
	// Has already started, is not alive anymore, and has been removed from the ThreadGroup
	synchronized(lock) {
		return started && threadRef == NO_REF;
	}
}

/**
 * Answers a <code>boolean</code> indicating whether the receiver
 * is a daemon Thread (<code>true</code>) or not (<code>false</code>)
 *	A daemon Thread only runs as long as there are non-daemon Threads
 * running. When the last non-daemon Thread ends, the whole program ends
 * no matter if it had daemon Threads still running or not.
 *
 * @return		a <code>boolean</code>
 *
 * @see			Thread#setDaemon 
 */
public final boolean isDaemon() {
	return this.isDaemon;
}

/**
 * Answers a <code>boolean</code> indicating whether the receiver
 * has a pending interrupt request (<code>true</code>) or not (<code>false</code>)
 *
 * @return		a <code>boolean</code>
 *
 * @see			Thread#interrupt 
 * @see			Thread#interrupted
 */
public boolean isInterrupted() {
	synchronized(lock) {
		return isInterruptedImpl();
	}
}

private native boolean isInterruptedImpl();


/**
 * Blocks the current Thread (<code>Thread.currentThread()</code>) until the
 * receiver finishes its execution and dies.
 *
 * @exception	InterruptedException
 *					if <code>interrupt()</code> was called for the receiver while
 *					it was in the <code>join()</code> call
 * 
 * @see			Object#notifyAll
 * @see			java.lang.ThreadDeath
 */
public final synchronized void join() throws InterruptedException {
	if (started)
		while (!isDead())
			wait(0);
}

/**
 * Blocks the current Thread (<code>Thread.currentThread()</code>) until the
 * receiver finishes its execution and dies or the specified timeout expires, whatever
 * happens first.
 *
 * @param		timeoutInMilliseconds		The maximum time to wait (in milliseconds).
 *
 * @exception	InterruptedException
 *					if <code>interrupt()</code> was called for the receiver while
 *					it was in the <code>join()</code> call
 * 
 * @see			Object#notifyAll
 * @see			java.lang.ThreadDeath
 */
public final void join(long timeoutInMilliseconds) throws InterruptedException {
	join(timeoutInMilliseconds, 0);
}

/**
 * Blocks the current Thread (<code>Thread.currentThread()</code>) until the
 * receiver finishes its execution and dies or the specified timeout expires, whatever
 * happens first.
 *
 * @param		timeoutInMilliseconds	The maximum time to wait (in milliseconds).
 * @param		nanos					Extra nanosecond precision
 *
 * @exception	InterruptedException
 *					if <code>interrupt()</code> was called for the receiver while
 *					it was in the <code>join()</code> call
 * 
 * @see			Object#notifyAll
 * @see			java.lang.ThreadDeath
 */
public final synchronized void join(long timeoutInMilliseconds, int nanos) throws InterruptedException {
	if (timeoutInMilliseconds < 0 || nanos < 0 || nanos > NANOS_MAX)
		throw new IllegalArgumentException();

	if (!started || isDead()) return;

	// No nanosecond precision for now, we would need something like 'currentTimenanos'
	
	long totalWaited = 0;
	long toWait = timeoutInMilliseconds;
	boolean timedOut = false;

	/*[PR 1PQM757] Even though we do not have nano precision, we cannot wait(0) when any one of the parameters is not zero */
	if (timeoutInMilliseconds == 0 & nanos > 0) {
		// We either round up (1 millisecond) or down (no need to wait, just return)
		if (nanos < 500000)
			timedOut = true;
		else
			toWait = 1;
	}
	/*[PR 1FJMO7Q] A Thread can be !isAlive() and still be in its ThreadGroup. Use isDead() */
	while (!timedOut && !isDead()) {
		long start = System.currentTimeMillis();
		wait(toWait);
		long waited = System.currentTimeMillis() - start;
		totalWaited+= waited;
		toWait -= waited;
		// Anyone could do a synchronized/notify on this thread, so if we wait
		// less than the timeout, we must check if the thread really died
		timedOut = (totalWaited >= timeoutInMilliseconds);
	}
	
}

/**
 * Private method that generates Thread names that comply with the Java specification
 *
 * @version		initial
 *
 * @return		a java.lang.String representing a name for the next Thread being generated
 *
 * @see			Thread#createCount
 */
private synchronized static String newName() {
	/*[PR 97331] Initial thread name should be Thread-0 */
	if (createCount == -1) {
		createCount++;
		return "main"; //$NON-NLS-1$
	} else {
		return "Thread-" + createCount++; //$NON-NLS-1$
	}	
}

/**
 * This is a no-op if the receiver was never suspended, or suspended and already
 * resumed. If the receiver is suspended, however, makes it resume to the point
 * where it was when it was suspended.
 *
 * @exception	SecurityException
 *					if <code>checkAccess()</code> fails with a SecurityException
 *
 * @see			Thread#suspend()
 *
 * @deprecated	Used with deprecated method Thread.suspend().
 */
/*[IF Sidecar19-SE]*/
@Deprecated(forRemoval=false, since="1.2")
/*[ELSE]*/
@Deprecated
/*[ENDIF]*/
public final void resume() {
	checkAccess();
	synchronized(lock) {
		resumeImpl();
	}
}

/**
 * Private method for the VM to do the actual work of resuming the Thread
 *
 */
private native void resumeImpl();

/**
 * Calls the <code>run()</code> method of the Runnable object the receiver holds.
 * If no Runnable is set, does nothing.
 *
 * @see			Thread#start
 */
public void run() {
	if (runnable != null) {
		runnable.run();
	}
}

/**
 * 	Set the context ClassLoader for the receiver.
 *
 * @param		cl		The context ClassLoader
 *
 * @see			java.lang.ClassLoader
 * @see			#getContextClassLoader()
 */
public void setContextClassLoader(ClassLoader cl) {
/*[PR 1FCA807]*/	
	SecurityManager currentManager = System.getSecurityManager();
	 // if there is a security manager...
	if (currentManager != null) {
		// then check permission
		currentManager.checkPermission(com.ibm.oti.util.RuntimePermissions.permissionSetContextClassLoader);
	}	
	contextClassLoader = cl;
}

/**
 * Set if the receiver is a daemon Thread or not. This can only be done
 * before the Thread starts running.
 *
 * @param		isDaemon		A boolean indicating if the Thread should be daemon or not
 * 
 * @exception	SecurityException
 *					if <code>checkAccess()</code> fails with a SecurityException
 *
 * @see			Thread#isDaemon 
 */
public final void setDaemon(boolean isDaemon) {
	checkAccess();
	synchronized(lock) {
		if (!this.started) {
			this.isDaemon = isDaemon;
		} else {
			/*[PR CVMC 82531] Only throw IllegalThreadStateException if the thread is alive */
			if (isAlive()) {
				throw new IllegalThreadStateException();
			}
		}
	}
}

/**
 * Sets the name of the receiver.
 *
 * @param		threadName		new name for the Thread
 *
 * @exception	SecurityException
 *					if <code>checkAccess()</code> fails with a SecurityException
 *
 * @see			Thread#getName 
 */
public final void setName(String threadName) {
	checkAccess();
	/*[PR 1FIGT59]*/
	if (threadName == null) {
		throw new NullPointerException();
	}
	synchronized(lock) {
		if (started && threadRef != NO_REF) {
			setNameImpl(threadRef, threadName);
		}
		name = threadName;
	}
}

private native void setNameImpl(long threadRef, String threadName);

/**
 * Sets the priority of the receiver. Note that the final priority set may be less than the
 * requested value, as it is bounded by the maxPriority() of the receiver's ThreadGroup.
 *
 * @param		requestedPriority		new priority for the Thread
 *
 * @exception	SecurityException
 *					if <code>checkAccess()</code> fails with a SecurityException
 * @exception	IllegalArgumentException
 *					if the new priority is greater than Thread.MAX_PRIORITY or less than
 *					Thread.MIN_PRIORITY
 *
 * @see			Thread#getPriority
 */
public final void setPriority(int requestedPriority){
	checkAccess();
	if (MIN_PRIORITY <= requestedPriority && requestedPriority <= MAX_PRIORITY) {
		ThreadGroup myThreadGroup = getThreadGroup();
		if (null != myThreadGroup) { /* [PR 134167 Ignore dead threads. */
			int finalPriority = requestedPriority;
			int threadGroupMaxPriority = myThreadGroup.getMaxPriority();
			if (threadGroupMaxPriority < requestedPriority) {
				finalPriority = threadGroupMaxPriority;
			}
			priority = finalPriority;
			synchronized(lock) {
				/*[PR CMVC 113440] java/lang/Thread/setPriority() Blocks GC */
				if (started && (NO_REF != threadRef)) {
					setPriorityNoVMAccessImpl(threadRef, finalPriority);
				}
			}
		} 
		return;
	}
	throw new IllegalArgumentException();
}

/**
 * Private method to tell the VM that the priority for the receiver is changing.
 *
 * @param		priority		new priority for the Thread
 *
 * @see			Thread#setPriority
 */
private native void setPriorityNoVMAccessImpl(long threadRef, int priority);

/**
 * Causes the thread which sent this message to sleep an interval
 * of time (given in milliseconds). The precision is not guaranteed -
 * the Thread may sleep more or less than requested.
 *
 * @param		time		The time to sleep in milliseconds.
 *
 * @exception	InterruptedException
 *					if <code>interrupt()</code> was called for this Thread while it was sleeping
 *
 * @see			Thread#interrupt()
 */
 
public static void sleep(long time) throws InterruptedException {
	sleep(time, 0);
}

/**
 * Causes the thread which sent this message to sleep an interval
 * of time (given in milliseconds). The precision is not guaranteed -
 * the Thread may sleep more or less than requested.
 *
 * @param		time		The time to sleep in milliseconds.
 * @param		nanos		Extra nanosecond precision
 *
 * @exception	InterruptedException
 *					if <code>interrupt()</code> was called for this Thread while it was sleeping
 *
 * @see			Thread#interrupt()
 */
public static native void sleep(long time, int nanos) throws InterruptedException;

/*[IF Sidecar19-SE]*/
/**
 * Hints to the run-time that a spin loop is being performed 
 * which helps the thread in the spin loop not to use as much power.
 * 
 */
public static native void onSpinWait();
/*[ENDIF]*/

/**
 * Starts the new Thread of execution. The <code>run()</code> method of the receiver
 * will be called by the receiver Thread itself (and not the Thread calling <code>start()</code>).
 *
 * @exception	IllegalThreadStateException
 *					Unspecified in the Java language specification
 *
 * @see			Thread#run
 */
public synchronized void start() {
	boolean success = false;
	
	/*[PR CMVC 189553] Deadlock when a thread fails to start and another thread is ins */
	/*[IF]
	 * Deadlock happens when a thread is added to the ThreadGroup
	 * and its start method fails and calls its ThreadGroup's remove method for this thread to be removed while holding this thread's lock.
	 * If there is another call which is holding ThreadGroup's child lock and waiting for this thread's lock,
	 * then deadlock occurs since ThreadGroup's remove method is waiting for its child lock. 
	 *
	 * Release the lock before calling threadgroup's remove method for this thread.
	/*[ENDIF]*/
	 
	try {
		synchronized(lock) {
			if (started) {
				/*[MSG "K0341", "Thread is already started"]*/ 
				throw new IllegalThreadStateException(com.ibm.oti.util.Msg.getString("K0341")); //$NON-NLS-1$
			}
 		
			/*[PR 115667, CMVC 94448] In 1.5, thread is added to ThreadGroup when started */
			group.add(this);
		
			startImpl();
 		 	
			success = true;
		}
 	} finally {
 		if (!success && !started) {
 	 		group.remove(this);
 		}
 	}
}
 
private native void startImpl(); 

/**
 * Requests the receiver Thread to stop and throw ThreadDeath.
 * The Thread is resumed if it was suspended and awakened if it was
 * sleeping, so that it can proceed to throw ThreadDeath.
 *
 * @exception	SecurityException
 *					if <code>checkAccess()</code> fails with a SecurityException
 *
 * @deprecated
 */
/*[IF Sidecar19-SE]*/
@Deprecated(forRemoval=false, since="1.2")
/*[ELSE]*/
@Deprecated
/*[ENDIF]*/
public final void stop() {
	/*[PR CMVC 94728] AccessControlException on dead Thread */
	/* the only case we don't want to do the check is if the thread has been started but is now dead */
	if (!isDead()){
		stopWithThrowable(new ThreadDeath());
	}
}

/*[IF !Java11]*/
/**
 * Throws UnsupportedOperationException.
 *
 * @param		throwable		Throwable object to be thrown by the Thread
 *
 * @deprecated
 */
/*[IF Sidecar19-SE]*/
@Deprecated(forRemoval=true, since="1.2")
/*[ELSE]*/
@Deprecated
/*[ENDIF]*/
public final void stop(Throwable throwable) {
	throw new UnsupportedOperationException();
 }
/*[ENDIF]*/

private final synchronized void stopWithThrowable(Throwable throwable) {
	checkAccess();
	/*[PR 95390]*/
	if (currentThread() != this || !(throwable instanceof ThreadDeath)) {
		SecurityManager currentManager = System.getSecurityManager();
		if (currentManager != null)	{
			currentManager.checkPermission(com.ibm.oti.util.RuntimePermissions.permissionStopThread);
		}
	}

	synchronized(lock) {
		if (throwable != null){
			if (!started){
				/* [PR CMVC 179978] Java7:JCK:java_lang.Thread fails in all plat*/
				/* 
				 * if the thread has not yet been simply store the fact that stop has been called
				 * The JVM uses this to determine if stop has been called before start
				 */
				stopCalled = true;
			} else {
				/* thread was started so do the full stop */
				stopImpl(throwable);
			}
		}
		else throw new NullPointerException();
	}
}

/**
 * Private method for the VM to do the actual work of stopping the Thread
 *
 * @param		throwable		Throwable object to be thrown by the Thread
 */
private native void stopImpl(Throwable throwable);

/**
 * This is a no-op if the receiver is suspended. If the receiver <code>isAlive()</code>
 * however, suspended it until <code>resume()</code> is sent to it. Suspend requests
 * are not queued, which means that N requests are equivalent to just one - only one
 * resume request is needed in this case.
 *
 * @exception	SecurityException
 *					if <code>checkAccess()</code> fails with a SecurityException
 *
 * @see			Thread#resume()
 *
 * @deprecated May cause deadlocks.
 */
/*[IF Sidecar19-SE]*/
@Deprecated(forRemoval=false, since="1.2")
/*[ELSE]*/
@Deprecated
/*[ENDIF]*/
public final void suspend() { 
	checkAccess();
	/*[PR 106321]*/
	if (currentThread() == this) suspendImpl();
	else {
		synchronized( lock ) { 
			suspendImpl();
		}
	}
}

/**
 * Private method for the VM to do the actual work of suspending the Thread
 *
 * Conceptually this method can't be synchronized or a Thread that suspends itself will
 * do so with the Thread's lock and this will cause deadlock to valid Java programs.
 */
private native void suspendImpl();

/*[PR 97801]*/
/**
 * Answers a string containing a concise, human-readable
 * description of the receiver.
 *
 * @return		a printable representation for the receiver.
 */
public String toString() {
	/*[PR JAZZ 85499] : NullPointerException from Thread.toString() */
	ThreadGroup localGroup = getThreadGroup();
	
	return "Thread[" + this.getName() + "," + this.getPriority() + "," + //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
		/*[PR 97317] Handle a null thread group */
		(null == localGroup ? "" : localGroup.getName()) + "]" ; //$NON-NLS-1$ //$NON-NLS-2$
}

/**
 * Causes the thread which sent this message to yield execution to another Thread
 * that is ready to run. The actual scheduling is implementation-dependent.
 *
 * @version		initial
 */
public static native void yield(); 

/**
 * Returns whether the current thread has a monitor lock on the specified object.
 *
 * @param object the object to test for the monitor lock
 * @return true when the current thread has a monitor lock on the specified object
 * 
 * @since 1.4
 */
public static native boolean holdsLock(Object object);

/*[IF Java11]*/
static
/*[ENDIF]*/
void blockedOn(sun.nio.ch.Interruptible interruptible) {
	Thread currentThread;
	/*[IF Java11]*/
	currentThread = currentThread();
	/*[ELSE]
	currentThread = this;
	/*[ENDIF]*/
	synchronized(currentThread.lock) {
		currentThread.blockOn = interruptible;
	}
}


private native Throwable getStackTraceImpl();

/**
 * Returns an array of StackTraceElement, where each element of the array represents a frame
 * on the Java stack. 
 * 
 * @return an array of StackTraceElement
 * 
 * @throws SecurityException if the RuntimePermission "getStackTrace" is not allowed
 * 
 * @see java.lang.StackTraceElement
 */
public StackTraceElement[] getStackTrace() {
	if (Thread.currentThread() != this) {
		SecurityManager security = System.getSecurityManager();
		if (security != null)
			security.checkPermission(com.ibm.oti.util.RuntimePermissions.permissionGetStackTrace); //$NON-NLS-1$
	}
	Throwable t;

	synchronized(lock) {
		if (!isAlive()) {
			return new StackTraceElement[0];
		}
		t = getStackTraceImpl();
	}
	return J9VMInternals.getStackTrace(t, false);
}

/**
 * Returns a Map containing Thread keys, and values which are arrays of StackTraceElement. The Map contains
 * all Threads which were alive at the time this method was called.
 * 
 * @return an array of StackTraceElement
 * 
 * @throws SecurityException if the RuntimePermission "getStackTrace" is not allowed, or the
 * 		RuntimePermission "modifyThreadGroup" is not allowed
 * 
 * @see #getStackTrace()
 */
public static Map<Thread, StackTraceElement[]> getAllStackTraces() {
	SecurityManager security = System.getSecurityManager();
	if (security != null) {
		security.checkPermission(com.ibm.oti.util.RuntimePermissions.permissionGetStackTrace);
		security.checkPermission(com.ibm.oti.util.RuntimePermissions.permissionModifyThreadGroup);
	}
	// Allow room for more Threads to be created before calling enumerate()
	int count = systemThreadGroup.activeCount() + 20;
	Thread[] threads = new Thread[count];
	count = systemThreadGroup.enumerate(threads);
	java.util.Map result = new java.util.HashMap(count*4/3);
	for (int i=0; i<count; i++) {
		result.put(threads[i], threads[i].getStackTrace());
	}
	return result;
}

/**
 * Return a unique id for this Thread.
 * 
 * @return a positive unique id for this Thread.
 */
public long getId() {
	return tid;
}

/**
 * A handler which is invoked when an uncaught exception occurs in a Thread.
 */
@FunctionalInterface
public static interface UncaughtExceptionHandler {
	/**
	 * The method invoked when an uncaught exception occurs in a Thread.
	 * 
	 * @param thread the Thread where the uncaught exception occurred
	 * @param throwable the uncaught exception
	 */
	public void uncaughtException(Thread thread, Throwable throwable) ;
}

/**
 * Return the UncaughtExceptionHandler for this Thread.
 * 
 * @return the UncaughtExceptionHandler for this Thread
 * 
 * @see UncaughtExceptionHandler
 */
public UncaughtExceptionHandler getUncaughtExceptionHandler() {
	if (exceptionHandler == null)
		return getThreadGroup();
	return exceptionHandler;
}

/**
 * Set the UncaughtExceptionHandler for this Thread.
 * 
 * @param handler the UncaughtExceptionHandler to set
 * 
 * @see UncaughtExceptionHandler
 */
public void setUncaughtExceptionHandler(UncaughtExceptionHandler handler) {
	exceptionHandler = handler;
}

/**
 * Return the default UncaughtExceptionHandler used for new Threads.
 * 
 * @return the default UncaughtExceptionHandler for new Threads
 * 
 * @see UncaughtExceptionHandler
 */
public static UncaughtExceptionHandler getDefaultUncaughtExceptionHandler() {
	return defaultExceptionHandler;
}

/**
 * Set the UncaughtExceptionHandler used for new  Threads.
 * 
 * @param handler the UncaughtExceptionHandler to set
 * 
 * @see UncaughtExceptionHandler
 */
public static void setDefaultUncaughtExceptionHandler(UncaughtExceptionHandler handler) {
	SecurityManager security = System.getSecurityManager();
	if (security != null)
		security.checkPermission(com.ibm.oti.util.RuntimePermissions.permissionSetDefaultUncaughtExceptionHandler);
	defaultExceptionHandler = handler;
}

/**
 * The possible Thread states.
 */
// The order of the States is known by the getStateImpl() native 
public static enum State {
	/**
	 * A Thread which has not yet started.
	 */
	NEW,
	/**
	 * A Thread which is running or suspended.
	 */
	RUNNABLE,
	/**
	 * A Thread which is blocked on a monitor.
	 */
	BLOCKED, 
	/**
	 * A Thread which is waiting with no timeout.
	 */
	WAITING,
	/**
	 * A Thread which is waiting with a timeout.
	 */
	TIMED_WAITING, 
	/**
	 * A thread which is no longer alive.
	 */
	TERMINATED }

/**
 * Returns the current Thread state.
 * 
 * @return the current Thread state constant.
 * 
 * @see State
 */
public State getState() {
	synchronized(lock) {
		if (threadRef == NO_REF) {
			if (isDead()) {
				return State.TERMINATED;
			}
			return State.NEW;
		}
		return State.values()[getStateImpl(threadRef)];
	}
}

private native int getStateImpl(long threadRef);

/**
 * Any uncaught exception in any Thread has to be forwarded (by the VM) to the Thread's ThreadGroup
 * by sending this message (uncaughtException). This allows users to define custom ThreadGroup classes
 * and custom behavior for when a Thread has an uncaughtException or when it does (ThreadDeath).
 *
 * @version		initial
 *
 * @param		e		The uncaught exception itself
 *
 * @see			Thread#stop()
 * @see			Thread#stop(Throwable)
 * @see			ThreadDeath
 */
void uncaughtException(Throwable e) {
	UncaughtExceptionHandler handler = getUncaughtExceptionHandler();
	if (handler != null)
		handler.uncaughtException(this, e);
}

/**
 * Called by J9VMInternals.threadCleanup() so the Thread can release resources
 * and change its state.
 *
 * @see J9VMInternals#threadCleanup()
 */
void cleanup() {
	/*[PR 97317]*/
	group = null;

	/*[PR CVMC 118827] references are not removed in dead threads */
	runnable = null;
	inheritedAccessControlContext = null;

	threadLocals = null;
	inheritableThreadLocals = null;

	synchronized(lock) {
		threadRef = Thread.NO_REF;				// So that isAlive() can work
	}
}

// prevent subclasses from becoming Cloneable
protected Object clone() throws CloneNotSupportedException {
	throw new CloneNotSupportedException();
}

}
