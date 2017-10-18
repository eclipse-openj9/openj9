/*[INCLUDE-IF Sidecar17]*/
/*******************************************************************************
 * Copyright (c) 2011, 2017 IBM Corp. and others
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

/*[IF Sidecar19-SE]
import jdk.internal.ref.Cleaner;
/*[ELSE]*/
import sun.misc.Cleaner;
/*[ENDIF]*/

/**
 * A MutableCallSite acts as though its target MethodHandle were a normal variable.
 * <p>
 * Because it is an ordinary variable, other threads may not immediately observe the value of a
 * {@link #setTarget(MethodHandle)} unless external synchronization is used.  If the result of 
 * a {@link #setTarget(MethodHandle)} call must be observed by other threads, the {@link #syncAll(MutableCallSite[])}
 * method may be used to force it to be synchronized.
 * <p>
 * The {@link #syncAll(MutableCallSite[])} call is likely to be expensive and should be used sparingly.  Calls to 
 * {@link #syncAll(MutableCallSite[])} should be batched whenever possible.
 * 
 * @since 1.7
 */
public class MutableCallSite extends CallSite {
	private static native void registerNatives();
	static {
		registerNatives();
	}
	private MutableCallSiteDynamicInvokerHandle cachedDynamicInvoker; 
	
	final private GlobalRefCleaner globalRefCleaner;
	
	/* Field bypassBase is dependent on field targetFieldOffset */
	private static final long targetFieldOffset = initializeTargetFieldOffset();
	private static long initializeTargetFieldOffset(){
		try{
			return MethodHandle.UNSAFE.objectFieldOffset(MutableCallSite.class.getDeclaredField("target")); //$NON-NLS-1$
		} catch (Exception e) {
			InternalError ie = new InternalError();
			ie.initCause(e);
			throw ie;
		}
	}

	/*[IF ]*/
	//dynamicInvokerHandleDispatchTarget needs to be modified to remove the 'atomic readBarrier.' from
	// the mutable callsite case.
	/*[ENDIF]*/
	private volatile MethodHandle target;
	private volatile MethodHandle epoch;  // A previous target that was equivalent to target, in the sense of StructuralComparator.handlesAreEquivalent
	
	/**
	 * Create a MutableCallSite permanently set to the same type as the <i>mutableTarget</i> and using
	 * the mutableTarget</i> as the initial target value.
	 * 
	 * @param mutableTarget - the initial target of the CallSite
	 * @throws NullPointerException - if the <i>mutableTarget</i> is null.
	 */
	public MutableCallSite(MethodHandle mutableTarget) throws NullPointerException {
		// .type provides the NPE if volatileTarget null
		super(mutableTarget.type());
		target = epoch = mutableTarget;
		freeze();
		globalRefCleaner = new GlobalRefCleaner();
		Cleaner.create(this, globalRefCleaner);
	}
	
	/**
	 * Create a MutableCallSite with the MethodType <i>type</i> and an
	 * initial target that throws IllegalStateException.
	 * 
	 * @param type - the permanent type of this CallSite.
	 * @throws NullPointerException - if the type is null.
	 */
	public MutableCallSite(MethodType type) throws NullPointerException {
		super(type);
		// install a target that throws IllegalStateException
		target = CallSite.initialTarget(type);
		epoch = null; // Seems unlikely we really want the jit to commit itself to optimizing a throw
		freeze();
		globalRefCleaner = new GlobalRefCleaner();
		Cleaner.create(this, globalRefCleaner);
	}
	
	@Override
	public final MethodHandle dynamicInvoker() {
		if (null == cachedDynamicInvoker) {
			cachedDynamicInvoker = new MutableCallSiteDynamicInvokerHandle(this);
		}
		return cachedDynamicInvoker;
	}

	@Override
	public final MethodHandle getTarget() {
		return target;
	}

	// Allow jitted code to bypass the CallSite table in order to load the target.
	private static final Object bypassBase = initializeBypassBase();
	private static Object initializeBypassBase() {
		try{
			return MethodHandle.UNSAFE.staticFieldBase(MutableCallSite.class.getDeclaredField("targetFieldOffset")); //$NON-NLS-1$
		} catch (Exception e) {
			InternalError ie = new InternalError();
			ie.initCause(e);
			throw ie;
		}
	}

	@Override
	public void setTarget(MethodHandle newTarget) {
		// newTarget.type provides NPE if null
		if (type() != newTarget.type) {
			throw WrongMethodTypeException.newWrongMethodTypeException(type(), newTarget.type);
		}

		// no op if target and newTarget are the same
		MethodHandle oldTarget = target;
		if (oldTarget != newTarget) {
			/*[IF ]*/
			/* Determine if we can avoid doing a thaw() which will invalidate the JITTED code.
			 * We can avoid the thaw() if the new target is structurally identical to the original
			 * target.  We use equivalenceCounter and equivalenceInterval to limit how often we
			 * check structural equivalence.  If new targets are equivalent, then it is worthwhile
			 * to always do the check.  Once they start being different, than we start to back off
			 * on how frequently we check as the check itself must walk the two handle graphes and 
			 * this is expensive.
			 *
			 * It is important that every path in here sets this.target exactly once, or else we
			 * need to start worrying about race conditions.
			 * 
			 * PR 56302: Set target using CAS to avoid racing with thaw() and ending up with target
			 * and epoch that are not equivalent.  If the CAS fails, that means we're in a race;
			 * it is valid to behave as though we won the race, and then the other thread overwrote
			 * the target field afterward.  Hence, it's valid to take no action when the CAS fails. 
			 */
			/*[ENDIF]*/
			if (--equivalenceCounter <= 0) {
				if (StructuralComparator.get().handlesAreEquivalent(oldTarget, newTarget)) {
					// Equivalence check saved us a thaw, so it's worth doing them every time.
					equivalenceInterval = 1;
/*[IF Sidecar19-SE-B174]*/				
					MethodHandle.UNSAFE.compareAndSetObject(this, targetFieldOffset, oldTarget, newTarget);
/*[ELSE]
					MethodHandle.UNSAFE.compareAndSwapObject(this, targetFieldOffset, oldTarget, newTarget);
/*[ENDIF]*/					
				} else {
					thaw(oldTarget, newTarget);
					// Equivalence check was useless; wait longer before doing another one.
					equivalenceInterval = Math.min(1 + equivalenceInterval + (equivalenceInterval >> 2), 1000);
				}
				equivalenceCounter = equivalenceInterval;
			} else {
				// Equivalence check has failed recently; don't bother doing one this time.
				thaw(oldTarget, newTarget);
			}
			if (globalRefCleaner.bypassOffset != 0) {
				MethodHandle.UNSAFE.putObject(bypassBase, globalRefCleaner.bypassOffset, newTarget);
			}
		}
	}

	// Not thread safe - allow racing updates
	private int equivalenceInterval = 0;
	private int equivalenceCounter = 0;

	// Invalidate any assumptions based on the MCS.target in compiled code
	private synchronized void thaw(MethodHandle oldTarget, MethodHandle newTarget) {
		/*[IF ]*/
		/* The JIT performs aggressive optimizations relying on the assumption
		 * that this.target points at a handle that is equivalent to the value it
		 * found in this.epoch at the time of the compilation.  If this.target is
		 * ever changed to a non-equivalent handle, we must first compensate the
		 * assumptions to ensure that we never run unsuitable code on the new target.
		 *
		 * Ensuring this in a multithreaded system, where the compile thread is
		 * concurrent with the application threads, requires synchronization.  At
		 * the end of compilation, the JIT does the so-called "CHTable commit":
		 * the JIT acquires the CHTable (Class Hierarchy Table) lock, and then
		 * checks whether all the assumptions it made during compilation are
		 * still valid.  If they are valid, it registers the assumptions to be
		 * compensated later if the assumptions are ever violated; if the
		 * assumptions have already been violated, the JIT compensates the
		 * assumptions immediately.
		 *
		 * The invalidate() call acquires the same lock and performs the
		 * compensation.  Because it acquires the same lock, it will run either
		 * before or after any CHTable commit.  If it runs before, there will be
		 * no assumptions to compensate.  If it runs after, it will compensate
		 * precisely those assumptions that the commit process registered (and
		 * hence were not compensated by the JIT itself).
		 *
		 * For this to work with MCS target / epoch assumptions, it is CRUCIAL
		 * that there exists no window during which the CHTable commit can observe
		 * a value of this.epoch that is not equivalent to this.target.  If the
		 * CHTable commit ever observed such a state, it could incorrectly
		 * conclude that there is no need to compensate the assumptions.
		 *
		 * The protocol we have works as follows:
		 *   1. this.epoch  = null
		 *   2. this.target = newTarget
		 *   3. this.epoch  = newTarget
		 *
		 * With all these fields being volatile, the only possible way for epoch
		 * not to match target is if epoch is null.  This case is easy to check
		 * in the JIT code, and we deal with nulls appropriately there.  This
		 * is the only code anywhere that modifies the target or epoch fields,
		 * and this method is synchronized, so there is no race between different
		 * threads doing setTarget at the same time.
		 *
		 * The remaining question is, where is the appropriate place to call
		 * invalidate?  The core design of CHTable commit is that either it must
		 * do the assumption compensation itself, or it must register assumptions
		 * to be compensated later.  This design only works properly if the
		 * compensation will indeed be done later.  Hence, if we call invalidate
		 * before step #1, it would be too early, because there would be a window
		 * (right before step #1) where the CHTable commit would not yet see any
		 * reason to do the compensation immediately, and it would respond by
		 * registering an assumption to compensate later, yet the thread
		 * performing the setTarget would never compensate that assumption
		 * because it already did its check and found nothing to compensate.
		 *
		 * Hence, the call to invalidate must occur after the CHTable code is made aware
		 * that it needs to do the compensation, and that occurs in step #1.
		 *
		 * Furthermore, the design of invalidate is that it must occur before
		 * this.target actually changes.  Otherwise, the jitted code can continue
		 * to execute code that is incorrect when fed the new target handle.
		 * Hence, the invalidate call must occur before step #2.
		 *
		 * Therefore, the invalidate call can only go between steps #1 and #2.
		 *
		 */
		/*[ENDIF]*/
		epoch  = null;
		invalidate(new long[]{this.invalidationCookie});
		target = newTarget;
		epoch  = newTarget;
	}

	// Currently no-op.  May be implemented in the future.
	private void freeze() {
	}

	private static native void invalidate(long[] cookies);
	private long invalidationCookie;

	/**
	 * Forces the current target MethodHandle of each of the MutableCallSites in the <i>sites</i> array to be seen by all threads.
	 * Loads of the target from any of the CallSites that has already begun will continue to use the old value.
	 * <p>
	 * If any of the elements in the <i>sites</i> array is null, a NullPointerException will be raised.  It is undefined whether any
	 * of the sites may have been synchronized. 
	 * <p>
	 * Note: it is valid for an implementation to use a volatile variable for the target value of MutableCallSite.  In that case, 
	 * the {@link #syncAll(MutableCallSite[])} call becomes a no-op.
	 *  
	 * @param sites - the array of MutableCallSites to force to be synchronized.
	 * @throws NullPointerException - if sites or any of its elements are null.
	 */
	public static void syncAll(MutableCallSite[] sites) throws NullPointerException {
		for (int i = 0; i < sites.length; i++) {
			sites[i].freeze(); // Throws NPE if null
		}
	}
	
	/*
	 * Native that releases the globalRef allocated during JIT compilation.
	 */
	static native void freeGlobalRef(long offset);

}

/*
 * A Runnable used by the sun.misc.Cleaner that will free the JNI 
 * GlobalRef allocated during compilation if required.
 */
final class GlobalRefCleaner implements Runnable {
	// Will be updated during JIT compilation.  'bypassOffset' is treated 
	// as though it were returned from Unsafe.staticFieldOffset(bypassBase, ...).
	long bypassOffset = 0;
	
	public void run() {
		if (bypassOffset != 0) {
			MutableCallSite.freeGlobalRef(bypassOffset);
		}
	}
}
