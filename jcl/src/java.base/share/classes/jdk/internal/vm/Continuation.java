/*[INCLUDE-IF JAVA_SPEC_VERSION >= 19]*/
/*
 * Copyright IBM Corp. and others 2022
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
package jdk.internal.vm;

import java.util.EnumSet;
import java.util.Set;
import java.util.function.Supplier;
import jdk.internal.access.JavaLangAccess;
import jdk.internal.access.SharedSecrets;
import jdk.internal.misc.Unsafe;
import jdk.internal.vm.annotation.Hidden;
import jdk.internal.vm.annotation.JvmtiMountTransition;

/**
 * Continuation class performing the mount/unmount operation for VirtualThread
 */
public class Continuation {
	private long vmRef; /* J9VMContinuation */
	protected Thread vthread; /* Parent VirtualThread */

	/* The live thread's scopedValueCache is always kept in J9VMThread->scopedValueCache
	 * whereas the unmounted thread's scopedValueCache is stored in this field. This
	 * field is modified in ContinuationHelpers.hpp::swapFieldsWithContinuation. This
	 * assures a one to one mapping between a java.lang.Thread and scopedValueCache.
	 */
	private Object[] scopedValueCache;

	private final ContinuationScope scope;
	private final Runnable runnable;
	private Continuation parent;
	/* bit flag for finished state, matching native constant J9_GC_CONTINUATION_STATE_FINISHED */
	public static final long CONTINUATION_FINISHED = 2;
	/* it's a bit-wise struct of CarrierThread ID and continuation flags(includes started and finished flag)
	 * low 8 bits are reserved for flags and the rest are the carrier thread ID.
	 * the state should not be directly accessed from Java, instead using get methods(such as isDone()).
	 */
	private volatile long state;

	private static JavaLangAccess JLA = SharedSecrets.getJavaLangAccess();

	private volatile boolean isAccessible = true;
	private static Unsafe unsafe = Unsafe.getUnsafe();
	private static long isAccessibleOffset = unsafe.objectFieldOffset(Continuation.class, "isAccessible");

	/**
	 * Continuation's Pinned reasons
	 */
	public enum Pinned {
		/** In native code */
		NATIVE(1),
		/** Holding monitor(s) */
		MONITOR(2),
		/** In critical section */
		CRITICAL_SECTION(3);

		private final int errorCode;

		private Pinned(int errorCode) {
			this.errorCode = errorCode;
		}

		public int errorCode() {
			return this.errorCode;
		}
	}

	public enum PreemptStatus {
		/** Success */
		SUCCESS(null),
		/** Permanent fail */
		PERM_FAIL_UNSUPPORTED(null),
		/** Permanent fail due to yielding */
		PERM_FAIL_YIELDING(null),
		/** Permanent fail due to continuation unmounted */
		PERM_FAIL_NOT_MOUNTED(null),
		/** Transient fail due to continuation pinned {@link Pinned#CRITICAL_SECTION} */
		TRANSIENT_FAIL_PINNED_CRITICAL_SECTION(Pinned.CRITICAL_SECTION),
		/** Transient fail due to continuation pinned {@link Pinned#NATIVE} */
		TRANSIENT_FAIL_PINNED_NATIVE(Pinned.NATIVE),
		/** Transient fail due to continuation pinned {@link Pinned#MONITOR} */
		TRANSIENT_FAIL_PINNED_MONITOR(Pinned.MONITOR);

		final Pinned pinned;

		public Pinned pinned() {
			return pinned;
		}

		private PreemptStatus(Pinned reason) {
			this.pinned = reason;
		}
	}

	/**
	 * Public constructor for Continuation
	 * @param scope The scope of the Continuation
	 * @param target The target of the Continuation to execute
	 */
	public Continuation(ContinuationScope scope, Runnable target) {
		this.scope = scope;
		this.runnable = target;
		if (JLA == null) {
			JLA = SharedSecrets.getJavaLangAccess();
		}
	}

	public ContinuationScope getScope() {
		return scope;
	}

	public Continuation getParent() {
		return parent;
	}

	public static Continuation getCurrentContinuation(ContinuationScope scope) {
		throw new UnsupportedOperationException();
	}

	/**
	 * @return new StackWalker for this Continuation
	 */
	public StackWalker stackWalker() {
		return stackWalker(EnumSet.noneOf(StackWalker.Option.class));
	}

	/**
	 * @param options options for the StackWalker
	 * @return new StackWalker for this Continuation with set of options
	 */
	public StackWalker stackWalker(Set<StackWalker.Option> options) {
		return stackWalker(options, scope);
	}

	/**
	 * @param options options for the StackWalker
	 * @param scope scope of Continuations to include in the StackWalker
	 * @return new StackWalker for Continuations in the scope with set of options
	 */
	public StackWalker stackWalker(Set<StackWalker.Option> options, ContinuationScope scope) {
		return JLA.newStackWalkerInstance(options, scope, this);
	}

	/**
	 * @return The stacktrace of this Continuation
	 * @throws IllegalStateException if this Continuation is mounted
	 */
	public StackTraceElement[] getStackTrace() {
		StackWalker walker = stackWalker(EnumSet.of(StackWalker.Option.SHOW_REFLECT_FRAMES));
		return walker.walk(s -> s.map(StackWalker.StackFrame::toStackTraceElement).toArray(StackTraceElement[]::new));
	}

	public static <R> R wrapWalk(Continuation cont, ContinuationScope scope, Supplier<R> sup) {
		throw new UnsupportedOperationException();
	}

	public boolean trylockAccess() {
		return unsafe.compareAndSetBoolean(this, isAccessibleOffset, true, false);
	}

	public void unlockAccess() {
		isAccessible = true;
	}

	@Hidden
	private static void enter(Continuation cont) {
		try {
			cont.runnable.run();
		} finally {
			yieldImpl(true);
		}
	}

	public final void run() {
		if (!trylockAccess()) {
			throw new IllegalStateException("Continuation inaccessible: mounted or being inspected.");
		}
		if (isDone()) {
			throw new IllegalStateException("Continuation has already finished.");
		}

		Thread carrier = JLA.currentCarrierThread();
		Continuation currentContinuation = JLA.getContinuation(carrier);

		if ((null != parent) && (parent != currentContinuation)) {
			throw new IllegalStateException("Running on carrier with incorrect parent.");
		} else {
			parent = currentContinuation;
		}

		JLA.setContinuation(carrier, this);

		boolean result = true;
		try {
			result = enterImpl();
		} finally {
			if (result) {
				/* Continuation completed */
			} else {
				/* Continuation yielded */
			}
			JLA.setContinuation(carrier, parent);
			unlockAccess();
		}
	}

	/**
	 * Suspend Continuations in the given scope
	 * @param scope the scope to lookup/suspend
	 * @return {@link true} or {@link false} based on success/failure
	 */
	@Hidden
	public static boolean yield(ContinuationScope scope) {
		/* TODO find matching scope to yield */
		Thread carrierThread = JLA.currentCarrierThread();
		Continuation cont = JLA.getContinuation(carrierThread);

		return cont.yield0();
	}

	@Hidden
	private boolean yield0() {
		int rcPinned = isPinnedImpl();
		if (rcPinned != 0) {
			Pinned reason = null;
			if (rcPinned == Pinned.CRITICAL_SECTION.errorCode()) {
				reason = Pinned.CRITICAL_SECTION;
			} else if (rcPinned == Pinned.MONITOR.errorCode()) {
				reason = Pinned.MONITOR;
			} else if (rcPinned == Pinned.NATIVE.errorCode()) {
				reason = Pinned.NATIVE;
			} else {
				throw new AssertionError("Unknown pinned error code: " + rcPinned);
			}
			onPinned(reason);
		} else {
			yieldImpl(false);
			onContinue();
		}
		return (rcPinned == 0);
	}

	protected void onPinned(Pinned reason) {
		throw new IllegalStateException("Continuation is pinned: " + reason);
	}

	protected void onContinue() {
	}

	public boolean isDone() {
		return 0 != (state & CONTINUATION_FINISHED);
	}

	public boolean isPreempted() {
		throw new UnsupportedOperationException();
	}

	public static native void pin();

	public static native void unpin();

	private static native int isPinnedImpl();

	public static boolean isPinned(ContinuationScope scope) {
		return (isPinnedImpl() != 0);
	}

	public PreemptStatus tryPreempt(Thread t) {
		throw new UnsupportedOperationException();
	}

	/* Continuation Native APIs */
	@Hidden
	@JvmtiMountTransition
	private native boolean enterImpl();

	@Hidden
	@JvmtiMountTransition
	private static native boolean yieldImpl(boolean isFinished);
}
