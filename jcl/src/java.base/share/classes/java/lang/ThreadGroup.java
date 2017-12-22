/*[INCLUDE-IF Sidecar16]*/

package java.lang;

/*******************************************************************************
 * Copyright (c) 1998, 2017 IBM Corp. and others
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
 * ThreadGroups are containers of Threads and ThreadGroups, therefore providing
 * a tree-like structure to organize Threads. The root ThreadGroup name is "system"
 * and it has no parent ThreadGroup. All other ThreadGroups have exactly one parent
 * ThreadGroup. All Threads belong to exactly one ThreadGroup.
 *
 * @author		OTI
 * @version		initial
 *
 * @see			Thread
 * @see			SecurityManager
 */

public class ThreadGroup implements Thread.UncaughtExceptionHandler {

	private String name;					// Name of this ThreadGroup		
	private int maxPriority = Thread.MAX_PRIORITY;		// Maximum priority for Threads inside this ThreadGroup
	ThreadGroup parent;			// The ThreadGroup to which this ThreadGroup belongs			
	/*[PR 93952]*/
	int numThreads;
	private Thread[] childrenThreads = new Thread[5];		// The Threads this ThreadGroup contains
	int numGroups;					// The number of children groups
	private ThreadGroup[] childrenGroups = new ThreadGroup[3];		// The ThreadGroups this ThreadGroup contains

	/*[PR 106322] - Cannot synchronize on childrenThreads and childrenGroups arrays */
	/*[PR 122459] LIR646 - Remove use of generic object for synchronization */
	private static final class ChildrenGroupsLock {}
	// Locked when using the childrenGroups field
	private Object childrenGroupsLock = new ChildrenGroupsLock();
	/*[PR 122459] LIR646 - Remove use of generic object for synchronization */
	private static final class ChildrenThreadsLock {}
	// Locked when using the childrenThreads field
	private Object childrenThreadsLock = new ChildrenThreadsLock();

	private boolean isDaemon;			// Whether this ThreadGroup is a daemon ThreadGroup or not	
	private boolean isDestroyed;			// Whether this ThreadGroup has already been destroyed or not	
	/*[PR CMVC 99507] Do not destroy daemon group if threads have been added but not started */
	private int addedNotStartedThreads;	// Threads that have been added but not yet started
	private long memorySpace;				// Memory space to associate all new threads with

/**
 * Used by the JVM to create the "system" ThreadGroup. Construct
 * a ThreadGroup instance, and assign the name "system".
 */
private ThreadGroup() {
	name = "system"; //$NON-NLS-1$
}

/**
 * Constructs a new ThreadGroup with the name provided.
 * The new ThreadGroup will be child of the ThreadGroup to which
 * the <code>Thread.currentThread()</code> belongs.
 *
 * @param		name		Name for the ThreadGroup being created
 *
 * @throws		SecurityException 	if <code>checkAccess()</code> for the parent group fails with a SecurityException
 *
 * @see			java.lang.Thread#currentThread
 */
 
public ThreadGroup(String name) {
	this(Thread.currentThread().getThreadGroup(), name);
}

/**
 * Constructs a new ThreadGroup with the name provided, as child of
 * the ThreadGroup <code>parent</code>
 *
 * @param		parent		Parent ThreadGroup
 * @param		name		Name for the ThreadGroup being created
 *
 * @throws		NullPointerException 			if <code>parent</code> is <code>null</code>
 * @throws		SecurityException 				if <code>checkAccess()</code> for the parent group fails with a SecurityException
 * @throws		IllegalThreadStateException 	if <code>parent</code> has been destroyed already
 */
public ThreadGroup(ThreadGroup parent, String name) {
	super();
	if (Thread.currentThread() != null) {
		// If parent is null we must throw NullPointerException, but that will be done "for free"
		// with the message send below
		parent.checkAccess();
	}

	this.name = name;
	this.setParent(parent);
	if (parent != null) {
		this.setMaxPriority(parent.getMaxPriority());
		if (parent.isDaemon()) this.setDaemon(true);
	}
}

/**
 * Initialize the "main" ThreadGroup
 */
/*[PR CMVC 71192] Initialize the "main" thread group without calling checkAccess() */
ThreadGroup(ThreadGroup parent) {
	this.name = "main"; //$NON-NLS-1$
	this.setParent(parent);
}

/**
 * Returns the number of Threads which are children of
 * the receiver, directly or indirectly.
 *
 * @return		Number of children Threads
 */

public int activeCount() {
	/*[PR 115667, CMVC 93001] should only count active threads */
	int count = 0;
	// Lock this subpart of the tree as we walk
	synchronized (childrenThreadsLock) {
		for (int i = numThreads; --i >= 0;) {
			if (childrenThreads[i].isAlive()) {
				count++;
			}
		}
		
	}
	synchronized ( this.childrenGroupsLock ) { // Lock this subpart of the tree as we walk
		for (int i = 0; i < numGroups; i++)
			count += this.childrenGroups[i].activeCount();
	}
	return count;	
}
/**
 * Returns the number of ThreadGroups which are children of
 * the receiver, directly or indirectly.
 *
 * @return		Number of children ThreadGroups
 */
 
public int activeGroupCount() {
	int count = 0;
	synchronized (this.childrenGroupsLock) { // Lock this subpart of the tree as we walk
		for (int i = 0 ; i < numGroups; i++)
			// One for this group & the subgroups
			count += 1 + this.childrenGroups[i].activeGroupCount();
	}
	return count;	
}

final void checkNewThread(Thread thread) throws IllegalThreadStateException {
	synchronized (this.childrenThreadsLock) {
		/*[PR 1FJC24P] testing state has to be done inside synchronized */
		if (isDestroyed) {
			throw new IllegalThreadStateException();
		}
		addedNotStartedThreads++;
	}
}

/**
 * Adds a Thread to the receiver. This should only be visible to class
 * java.lang.Thread, and should only be called when a new Thread is created
 * and initialized by the constructor.
 *
 * @param		thread		Thread to add to the receiver
 *
 * @throws		IllegalThreadStateException 	if the receiver has been destroyed already
 *
 * @see			#remove(java.lang.Thread)
 */
 
final void add(Thread thread) throws IllegalThreadStateException {
	synchronized (this.childrenThreadsLock) {
		/*[PR 1FJC24P] testing state has to be done inside synchronized */
		if (!isDestroyed) {
			if (childrenThreads.length == numThreads) {
					Thread[] newThreads = new Thread[childrenThreads.length * 2];
					System.arraycopy(childrenThreads, 0, newThreads, 0, numThreads);
					newThreads[numThreads++] = thread;
					childrenThreads = newThreads;
			} else childrenThreads[numThreads++] = thread;
			addedNotStartedThreads--;
		} else throw new IllegalThreadStateException();
	}
}

/**
 * Adds a ThreadGroup to the receiver.
 *
 * @param		g		ThreadGroup to add to the receiver
 *
 * @throws		IllegalThreadStateException 	if the receiver has been destroyed already
 *
 */
private void add(ThreadGroup g) throws IllegalThreadStateException {
	/*[PR 1FJC24P] testing state has to be done inside synchronized */
	/*[PR JAZZ 9154] ThreadGroup.isDestroyed is not properly synchronized */
	synchronized(this.childrenThreadsLock) {
		synchronized (this.childrenGroupsLock) {
			if (!isDestroyed()) {
				if (childrenGroups.length == numGroups) {
					ThreadGroup[] newGroups = new ThreadGroup[childrenGroups.length * 2];
					System.arraycopy(childrenGroups, 0, newGroups, 0, numGroups);
					childrenGroups = newGroups;
				}
				childrenGroups[numGroups++] = g;
			}
			else throw new IllegalThreadStateException();
		}
	}
}

/**
 * The definition of this method depends on the deprecated method <code>suspend()</code>.
 * The behavior of this call was never specified.
 *
 * @param		b		Used to control low memory implicit suspension
 * @return always returns true
 *
 * @deprecated 	Required deprecated method suspend().
 */
/*[IF Sidecar19-SE]*/
@Deprecated(forRemoval=false, since="1.2")
/*[ELSE]*/
@Deprecated
/*[ENDIF]*/
public boolean allowThreadSuspension(boolean b) {
	// Does not apply to this VM, no-op
	/*[PR 1PR4U1E]*/
	return true;
}
/**
 * If there is a SecurityManager installed, call <code>checkAccess</code>
 * in it passing the receiver as parameter, otherwise do nothing.
 */
public final void checkAccess() {
	// Forwards the message to the SecurityManager (if there's one)
	// passing the receiver as parameter

	SecurityManager currentManager = System.getSecurityManager();
	if (currentManager != null) currentManager.checkAccess(this);
}
/**
 * Destroys the receiver and recursively all its subgroups. It is only legal
 * to destroy a ThreadGroup that has no Threads.
 *  Any daemon ThreadGroup is destroyed automatically when it becomes empty
 * (no Threads and no ThreadGroups in it).
 *
 * @throws		IllegalThreadStateException 	if the receiver or any of its subgroups has been destroyed already
 * @throws		SecurityException 				if <code>this.checkAccess()</code> fails with a SecurityException
 */
public final void destroy(){
	destroyImpl();
	removeFromParent();
}

/**
 * Do the destroy logic but do not remove ourselves from the parent threadgroup
 * Callers must be sure to call removeFromParent() without holding locks to avoid deadlocks
 */
private void destroyImpl(){
	/*[PR CMVC 198585] Deadlock when removing self from parent */
	checkAccess();

	synchronized (this.childrenThreadsLock) { // Lock this subpart of the tree as we walk
		synchronized (this.childrenGroupsLock) {
			/*[PR 1FJC24P] testing state has to be done inside synchronized */
			if (isDestroyed)
/*[MSG "K0056", "Already destroyed"]*/
				throw new IllegalThreadStateException(com.ibm.oti.util.Msg.getString("K0056")); //$NON-NLS-1$
			if (numThreads > 0)
/*[MSG "K0057", "Has threads"]*/
				throw new IllegalThreadStateException(com.ibm.oti.util.Msg.getString("K0057")); //$NON-NLS-1$
		
			int toDestroy = numGroups;
			// Call recursively for subgroups
			for (int i = 0 ; i < toDestroy; i++) {
				// We always get the first element - remember, 
				// when the child dies it removes itself from our collection. See removeFromParent().
				this.childrenGroups[0].destroy();
			}
		/*[PR CMVC 137999] move enclosing braces to avoid deadlock (allow parent to be removed outside of locks) */
		}
		// Now that the ThreadGroup is really destroyed it can be tagged as so			
		/*[PR 1FJJ0N7] Comment and code have to be consistent */
		this.isDestroyed = true;
	}		

}
/**
 * Auxiliary method that destroys the receiver and recursively all its subgroups
 * if the receiver is a daemon ThreadGroup.
 *
 * @see			#destroy
 * @see			#setDaemon 
 * @see			#isDaemon 
 */
 
private void destroyIfEmptyDaemon() {
	boolean shouldRemoveFromParent = false;
	
	// Has to be non-destroyed daemon to make sense
	synchronized (this.childrenThreadsLock) {
		/*[PR 1FJC24P] testing state has to be done inside synchronized */
		if (isDaemon && !isDestroyed &&
				/*[PR CMVC 99507] Do not destroy daemon group if threads have been added but not started */
				addedNotStartedThreads == 0 &&
				numThreads == 0)
		{
			synchronized (this.childrenGroupsLock) {
				if (numGroups == 0) { 
					destroyImpl();
					shouldRemoveFromParent = true;
				}
			}
		}
	}
	
	if (shouldRemoveFromParent) {
		removeFromParent();
	}
}

/**
 * Does a nullcheck and removes this threadgroup from the parent.
 * Callers must not hold any locks when calling this function or a deadlock my occur
 */
private void removeFromParent() {
	/*[PR CMVC 198585] Deadlock when removing self from parent */
	/*[PR 97314] Cannot call getParent() */
	if (parent != null) {
		parent.remove(this);
	}
}
/**
 * Copies an array with all Threads which are children of
 * the receiver (directly or indirectly) into the array <code>threads</code>
 * passed as parameters. If the array passed as parameter is too small no
 * exception is thrown - the extra elements are simply not copied.
 *
 * @param		threads	Thread	array into which the Threads will be copied
 * @return		How many Threads were copied over
 *
 */
 
public int enumerate(Thread[] threads) {
	return enumerate(threads, true);
}
/**
 * Copies an array with all Threads which are children of
 * the receiver into the array <code>threads</code>
 * passed as parameter. Children Threads of subgroups are recursively copied
 * as well if parameter <code>recurse</code> is <code>true</code>.
 *
 * If the array passed as parameter is too small no
 * exception is thrown - the extra elements are simply not copied.
 *
 * @param		threads		array into which the Threads will be copied
 * @param		recurse		Indicates whether Threads in subgroups should be recursively copied as well or not
 * @return		How many Threads were copied over
 *
 */
 
public int enumerate(Thread[] threads, boolean recurse) {
	return enumerateGeneric(threads, recurse, 0, true);
}
/**
 * Copies an array with all ThreadGroups which are children of
 * the receiver (directly or indirectly) into the array <code>groups</code>
 * passed as parameters. If the array passed as parameter is too small no
 * exception is thrown - the extra elements are simply not copied.
 *
 * @param		groups 		array into which the ThreadGroups will be copied
 * @return		How many ThreadGroups were copied over
 *
 */
 
public int enumerate(ThreadGroup[] groups) {
	return enumerate(groups, true);
}
/**
 * Copies an array with all ThreadGroups which are children of
 * the receiver into the array <code>groups</code>
 * passed as parameter. Children ThreadGroups of subgroups are recursively copied
 * as well if parameter <code>recurse</code> is <code>true</code>.
 *
 * If the array passed as parameter is too small no
 * exception is thrown - the extra elements are simply not copied.
 *
 * @param		groups		array into which the ThreadGroups will be copied
 * @param		recurse		Indicates whether ThreadGroups in subgroups should be recursively copied as well or not
 * @return		How many ThreadGroups were copied over
 *
 */
 
public int enumerate(ThreadGroup[] groups, boolean recurse) {
	return enumerateGeneric(groups, recurse, 0, false);
}
/**
 * Copies into <param>enumeration</param> starting at </param>enumerationIndex</param>
 * all Threads or ThreadGroups in the receiver. If </param>recurse</param>
 * is true, recursively enumerate the elements in subgroups.
 *
 * If the array passed as parameter is too small no
 * exception is thrown - the extra elements are simply not copied.
 *
 * @param		enumeration				array into which the elements will be copied
 * @param		recurse					Indicates whether </param>recurseCollection</param> should be enumerated or not
 * @param		enumerationIndex		Indicates in which position of the enumeration array we are
 * @param		enumeratingThreads		Indicates whether we are enumerating Threads or ThreadGroups
 * @return		How many elements were enumerated/copied over
 */
 
private int enumerateGeneric(Object[] enumeration, boolean recurse, int enumerationIndex, boolean enumeratingThreads) {
	checkAccess();

	Object syncLock = enumeratingThreads ? childrenThreadsLock : childrenGroupsLock;

	synchronized (syncLock) { // Lock this subpart of the tree as we walk
		/*[PR CMVC 94112] ArrayIndexOutOfBoundsException when enumerating Threads.*/
		Object[] immediateCollection = enumeratingThreads ? (Object[])childrenThreads : (Object[])childrenGroups;
		
		for (int i = enumeratingThreads ? numThreads : numGroups; --i >= 0;) {
			if (!enumeratingThreads || ((Thread)immediateCollection[i]).isAlive()) {
				if (enumerationIndex >= enumeration.length) return enumerationIndex;
				enumeration[enumerationIndex++] = immediateCollection[i];
			}
		}
	}

	if (recurse) { // Lock this subpart of the tree as we walk
		synchronized (this.childrenGroupsLock) {
			for (int i = 0; i < numGroups; i++) {
				if (enumerationIndex >= enumeration.length) return enumerationIndex;
				enumerationIndex = childrenGroups[i].
						enumerateGeneric(enumeration, recurse, enumerationIndex, enumeratingThreads);
			}
		}
	}
	return enumerationIndex;
}

/**
 * Copies into <param>enumeration</param> starting at </param>enumerationIndex</param>
 * all Threads or ThreadGroups in the receiver. If </param>recurse</param>
 * is true, recursively enumerate the elements in subgroups.
 *
 * If the array passed as parameter is too small no
 * exception is thrown - the extra elements are simply not copied.
 *
 * @param		enumeration				array into which the elements will be copied
 * @param		recurse					Indicates whether </param>recurseCollection</param> should be enumerated or not
 * @param		enumerationIndex		Indicates in which position of the enumeration array we are
 * @param		enumeratingThreads		Indicates whether we are enumerating Threads or ThreadGroups
 * @return		How many elements were enumerated/copied over
 */
 
int enumerateDeadThreads(Object[] enumeration, int enumerationIndex) {
	boolean recurse = true;
	boolean enumeratingThreads = true;

	Object syncLock = enumeratingThreads ? childrenThreadsLock : childrenGroupsLock;

	synchronized (syncLock) { // Lock this subpart of the tree as we walk
		/*[PR CMVC 94112] ArrayIndexOutOfBoundsException when enumerating Threads.*/
		Object[] immediateCollection = enumeratingThreads ? (Object[])childrenThreads : (Object[])childrenGroups;
		
		for (int i = enumeratingThreads ? numThreads : numGroups; --i >= 0;) {
			if (!enumeratingThreads || !((Thread)immediateCollection[i]).isAlive()) {
				if (enumerationIndex >= enumeration.length) return enumerationIndex;
				enumeration[enumerationIndex++] = immediateCollection[i];
			}
		}
	}

	if (recurse) { // Lock this subpart of the tree as we walk
		synchronized (this.childrenGroupsLock) {
			for (int i = 0; i < numGroups; i++) {
				if (enumerationIndex >= enumeration.length) return enumerationIndex;
				enumerationIndex = childrenGroups[i].
				enumerateDeadThreads(enumeration, enumerationIndex);
			}
		}
	}
	return enumerationIndex;
}

/**
 * Answers the maximum allowed priority for a Thread in the receiver.
 *
 * @return		the maximum priority (an <code>int</code>)
 *
 * @see			#setMaxPriority
 */
 
public final int getMaxPriority() {
	return maxPriority;
}
/**
 * Answers the name of the receiver.
 *
 * @return		the receiver's name (a java.lang.String)
 */
 
public final String getName() {
	return name;
}
/**
 * Answers the receiver's parent ThreadGroup. It can be null if the receiver
 * is the the root ThreadGroup.
 *
 * @return		the parent ThreadGroup
 *
 */
 
 public final ThreadGroup getParent() {
	/*[PR 97314]*/
	if (parent != null)
		parent.checkAccess();
	else {
		/*[IF] user created threadgroups can set the name to be system, however in 
		 * the test below the name hasn't been set yet if the parent is null, and a
		 * nullpointerexception was thrown during threadgroup creation. */  
		/*[ENDIF]*/		
		/*[MSG "K0550", "current thread cannot modify this thread group"]*/
		if (this.name == null || !this.name.equalsIgnoreCase("system")) //$NON-NLS-1$
			throw new SecurityException(com.ibm.oti.util.Msg.getString("K0550")); //$NON-NLS-1$
	}
	
	return parent;
}
/**
 * Interrupts every Thread in the receiver and recursively in all its subgroups.
 *
 * @throws		SecurityException 	if <code>this.checkAccess()</code> fails with a SecurityException
 *
 * @see			Thread#interrupt
 */

public final void interrupt() {
	checkAccess();
	synchronized (this.childrenThreadsLock) { // Lock this subpart of the tree as we walk
		for (int i = 0 ; i < numThreads; i++)
			this.childrenThreads[i].interrupt();
	}
	synchronized (this.childrenGroupsLock) { // Lock this subpart of the tree as we walk
		for (int i = 0 ; i < numGroups; i++)
			this.childrenGroups[i].interrupt();
	}
}
/**
 * Answers true if the receiver is a daemon ThreadGroup, false otherwise.
 *
 * @return		if the receiver is a daemon ThreadGroup
 *
 * @see			#setDaemon 
 * @see			#destroy
 */
 
public final boolean isDaemon() {
	return isDaemon;
}

/**
 * Answers true if the receiver has been destroyed already, false otherwise.
 *
 * @return		if the receiver has been destroyed already
 *
 * @see			#destroy
 */
public boolean isDestroyed(){
	// never call this when synchronized on childrenThreadsGroup or deadlock will occur
	/*[PR JAZZ 9154] ThreadGroup.isDestroyed is not properly synchronized */
	synchronized(childrenThreadsLock) {
		return isDestroyed;
	}
}
/**
 * Outputs to <code>System.out</code> a text representation of the hierarchy of
 * Threads and ThreadGroups in the receiver (and recursively). Proper indentation
 * is done to suggest the nesting of groups inside groups and threads inside groups.
 */
 
public void list() {
	System.out.println();	// We start in a fresh line
	list(0);
}
/**
 * Outputs to <code>System.out</code>a text representation of the hierarchy of
 * Threads and ThreadGroups in the receiver (and recursively). The indentation
 * will be four spaces per level of nesting.
 *
 * @param		levels		How many levels of nesting, so that proper indentation can be output.
 *
 */

private void list(int levels) {
	String spaces = "    "; // 4 spaces for each level //$NON-NLS-1$
	for (int i = 0; i < levels; i++)
		System.out.print(spaces);

	// Print the receiver
	System.out.println(this.toString());

	// Print the children threads, with 1 extra indentation
	synchronized (this.childrenThreadsLock) {
		for (int i = 0; i < numThreads; i++) {
			for (int j = 0; j <= levels; j++)
				System.out.print(spaces); // children get an extra indentation, 4 spaces for each level
			System.out.println(this.childrenThreads[i]);
		}
	}
	synchronized (this.childrenGroupsLock) {
		for (int i = 0; i < numGroups; i++)
			this.childrenGroups[i].list(levels + 1);
	}
}
/**
 * Answers true if the receiver is a direct or indirect parent group of
 * ThreadGroup <code>g</code>, false otherwise.
 *
 * @param		g			ThreadGroup to test
 *
 * @return		if the receiver is parent of the ThreadGroup passed as parameter
 *
 */
 
 public final boolean parentOf(ThreadGroup g) {
	while (g != null) {
		if (this == g) return true;
		/*[PR 97314] Cannot call getParent() */
		g = g.parent;
	}
	return false;
}

/**
 * Removes a Thread from the receiver. This should only be visible to class
 * java.lang.Thread, and should only be called when a Thread dies.
 *
 * @param		thread		Thread to remove from the receiver
 *
 * @see			#add(Thread)
 */
 final void remove(java.lang.Thread thread) {

	/*[PR JAZZ 86608] Hang because ThreadGroup.remove synchronizes childrenThreadsLock and then synchronizes ThreadGroup itself */
	boolean isThreadGroupEmpty = false;
	
	synchronized (this.childrenThreadsLock) {
		for (int i=0; i<numThreads; i++) {
			/*[PR CMVC 109438] Dead Threads not removed from ThreadGroups */
			if (childrenThreads[i] == thread) {
				numThreads--;
				if (numThreads == 0) {
					isThreadGroupEmpty = true;
				} else {
					System.arraycopy (childrenThreads, i + 1, childrenThreads, i, numThreads - i);
				}
				childrenThreads[numThreads] = null;
				break;
			}
		}
	}
    
	/*[PR CMVC 114880] ThreadGroup is not notified when all threads complete */
    if (isThreadGroupEmpty) {
		synchronized (this) {
			notifyAll();
		}
	}
	destroyIfEmptyDaemon();
}

/**
 * Removes an immediate subgroup from the receiver.
 *
 * @param		g			Threadgroup to remove from the receiver
 *
 * @see			#add(Thread)
 * @see			#add(ThreadGroup)
 */
private void remove(ThreadGroup g) {
	synchronized (this.childrenGroupsLock) {
		for (int i=0; i<numGroups; i++) {
			/*[PR CMVC 109438] Dead Threads not removed from ThreadGroups */
			if (childrenGroups[i] == g) {
				numGroups--;
				System.arraycopy (childrenGroups, i + 1, childrenGroups, i, numGroups - i);
				childrenGroups[numGroups] = null;
				break;
			}
		}
	}
	destroyIfEmptyDaemon();
}
/**
 * Resumes every Thread in the receiver and recursively in all its subgroups.
 *
 * @throws		SecurityException 	if <code>this.checkAccess()</code> fails with a SecurityException
 *
 * @see			Thread#resume
 * @see			#suspend
 *
 * @deprecated Requires deprecated method Thread.resume().
 */
/*[IF Sidecar19-SE]*/
@Deprecated(forRemoval=false, since="1.2")
/*[ELSE]*/
@Deprecated
/*[ENDIF]*/
public final void resume() {
	checkAccess();
	synchronized (this.childrenThreadsLock) { // Lock this subpart of the tree as we walk
		for (int i = 0 ; i < numThreads; i++)
			this.childrenThreads[i].resume();
	}
	synchronized (this.childrenGroupsLock) { // Lock this subpart of the tree as we walk
		for (int i = 0 ; i < numGroups; i++)
			this.childrenGroups[i].resume();
	}
}
/**
 * Configures the receiver to be a daemon ThreadGroup or not.
 * Daemon ThreadGroups are automatically destroyed when they become empty.
 *
 * @param		isDaemon	new value defining if receiver should be daemon or not
 *
 * @throws		SecurityException 	if <code>checkAccess()</code> for the parent group fails with a SecurityException
 *
 * @see			#isDaemon 
 * @see			#destroy
 */
 
public final void setDaemon(boolean isDaemon) {
	checkAccess();
	this.isDaemon = isDaemon;
}
/**
 * Configures the maximum allowed priority for a Thread in the receiver
 * and recursively in all its subgroups.
 *
 * One can never change the maximum priority of a ThreadGroup to be
 * higher than it was. Such an attempt will not result in an exception, it will
 * simply leave the ThreadGroup with its current maximum priority.
 * 
 * @param		newMax		the new maximum priority to be set
 *
 * @throws		SecurityException 			if <code>checkAccess()</code> fails with a SecurityException
 * @throws		IllegalArgumentException 	if the new priority is greater than Thread.MAX_PRIORITY or less than
 *											Thread.MIN_PRIORITY
 *
 * @see			#getMaxPriority
 */

public final void setMaxPriority(int newMax) {
	checkAccess();

	/*[PR 1FJ9S51] If new priority is greater than the current maximum, the maximum remains unchanged */
	/*[PR CMVC 177870] Java7:JCK:java_lang/ThreadGroup/setMaxPriority fails in all platform */
	if (Thread.MIN_PRIORITY <= newMax && newMax <= this.maxPriority) {
		/*[PR 97314] Cannot call getParent() */
		int parentPriority = parent == null ? newMax : parent.getMaxPriority();
		this.maxPriority = parentPriority <= newMax ? parentPriority : newMax;
		synchronized (this.childrenGroupsLock) { // Lock this subpart of the tree as we walk
			for (int i = 0 ; i < numGroups; i++)
				this.childrenGroups[i].setMaxPriority(newMax); 
		}
	}
}
/**
 * Sets the parent ThreadGroup of the receiver, and adds the receiver to the parent's
 * collection of immediate children (if <code>parent</code> is not <code>null</code>).
 *
 * @param		parent		The parent ThreadGroup, or null if the receiver is to be the root ThreadGroup
 *
 * @see			#getParent
 * @see			#parentOf
 */
 
private void setParent(ThreadGroup parent) {
	if (parent != null) parent.add(this);
	this.parent = parent;
}
/**
 * Stops every Thread in the receiver and recursively in all its subgroups.
 *
 * @throws		SecurityException 	if <code>this.checkAccess()</code> fails with a SecurityException
 *
 * @see			Thread#stop()
 * @see			Thread#stop(Throwable)
 * @see			ThreadDeath
 *
 * @deprecated Requires deprecated method Thread.stop().
 */
/*[IF Sidecar19-SE]*/
@Deprecated(forRemoval=false, since="1.2")
/*[ELSE]*/
@Deprecated
/*[ENDIF]*/
public final void stop() {
	/*[PR CMVC 73122] Stop the running thread last */
	if (stopHelper())
		Thread.currentThread().stop();
}

/**
* @deprecated Requires deprecated method Thread.suspend().
*/
@Deprecated
private final boolean stopHelper() {
	checkAccess();

	boolean stopCurrent = false;
	synchronized (this.childrenThreadsLock) { // Lock this subpart of the tree as we walk
		Thread current = Thread.currentThread();
		for (int i = 0 ; i < numThreads; i++)
			if (this.childrenThreads[i] == current) {
				stopCurrent = true;
			} else {
				this.childrenThreads[i].stop();
			}
	}
	synchronized (this.childrenGroupsLock) { // Lock this subpart of the tree as we walk
		for (int i = 0 ; i < numGroups; i++)
			stopCurrent |= this.childrenGroups[i].stopHelper();
	}
	return stopCurrent;
}
/**
 * Suspends every Thread in the receiver and recursively in all its subgroups.
 *
 * @throws		SecurityException 	if <code>this.checkAccess()</code> fails with a SecurityException
 *
 * @see			Thread#suspend
 * @see			#resume
 *
 * @deprecated Requires deprecated method Thread.suspend().
 */
/*[IF Sidecar19-SE]*/
@Deprecated(forRemoval=false, since="1.2")
/*[ELSE]*/
@Deprecated
/*[ENDIF]*/
public final void suspend() {
	if (suspendHelper())
		Thread.currentThread().suspend();
}

/**
* @deprecated Requires deprecated method Thread.suspend().
*/
@Deprecated
private final boolean suspendHelper() {
	checkAccess();

	boolean suspendCurrent = false;
	synchronized (this.childrenThreadsLock) { // Lock this subpart of the tree as we walk
		Thread current = Thread.currentThread();
		for (int i = 0 ; i < numThreads; i++)
			if (this.childrenThreads[i] == current) {
				suspendCurrent = true;
			} else {
				this.childrenThreads[i].suspend();
			}
	}
	synchronized (this.childrenGroupsLock) { // Lock this subpart of the tree as we walk
		for (int i = 0 ; i < numGroups; i++)
			suspendCurrent |= this.childrenGroups[i].suspendHelper();
	}
	return suspendCurrent;
}

/**
 * Answers a string containing a concise, human-readable
 * description of the receiver.
 *
 * @return		a printable representation for the receiver.
 */
 
/*[IF AnnotateOverride]*/
@Override
/*[ENDIF]*/
public String toString() {
	return getClass().getName() + "[name=" + this.getName() + ",maxpri=" + this.getMaxPriority() + "]" ;  //$NON-NLS-1$//$NON-NLS-2$ //$NON-NLS-3$
}


/**
 * Any uncaught exception in any Thread has to be forwarded (by the VM) to the Thread's ThreadGroup
 * by sending this message (uncaughtException). This allows users to define custom ThreadGroup classes
 * and custom behavior for when a Thread has an uncaughtException or when it does (ThreadDeath).
 *
 * @param		t		Thread with an uncaught exception
 * @param		e		The uncaught exception itself
 *
 * @see			Thread#stop()
 * @see			Thread#stop(Throwable)
 * @see			ThreadDeath
 */
public void uncaughtException(Thread t, Throwable e) {
	Thread.UncaughtExceptionHandler handler;
	/*[PR 95801]*/
	if (parent != null) {
		parent.uncaughtException(t,e);
	} else if ((handler = Thread.getDefaultUncaughtExceptionHandler()) != null) {
		handler.uncaughtException(t, e);
	} else if (!(e instanceof ThreadDeath)) {
		// No parent group, has to be 'system' Thread Group
		/*[MSG "K0319", "Exception in thread \"{0}\" "]*/
		System.err.print(com.ibm.oti.util.Msg.getString("K0319", t.getName())); //$NON-NLS-1$
		e.printStackTrace(System.err);
	}
}
}

