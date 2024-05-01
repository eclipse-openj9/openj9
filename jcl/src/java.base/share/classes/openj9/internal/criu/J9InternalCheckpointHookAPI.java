/*[INCLUDE-IF CRIU_SUPPORT]*/
/*
 * Copyright IBM Corp. and others 2023
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
package openj9.internal.criu;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import openj9.internal.criu.InternalCRIUSupport.HookMode;

final class J9InternalCheckpointHookAPI {

	private static List<J9InternalCheckpointHook> postRestoreHooksSingleThread = new ArrayList<>();
	private static List<J9InternalCheckpointHook> preCheckpointHooksSingleThread = new ArrayList<>();
	private static List<J9InternalCheckpointHook> postRestoreHooksConcurrentThread = new ArrayList<>();
	private static List<J9InternalCheckpointHook> preCheckpointHooksConcurrentThread = new ArrayList<>();

	/**
	 * This is an internal API
	 *
	 * Register post restore hook that runs after the JVM is resumed from
	 * checkpointed state.
	 *
	 * Do not spawn new threads or make use of threadpools in the hooks.
	 *
	 * @param priority the priority in which the hook will be run, higher the number
	 *                 higher the priority
	 * @param name     the name of the hook
	 * @param hook     the runnable hook
	 */
	static synchronized void registerPostRestoreHook(HookMode mode, int priority, String name, Runnable hook)
			throws UnsupportedOperationException {
		// The hook mode and priority have been verified  by the caller.
		J9InternalCheckpointHook internalHook = new J9InternalCheckpointHook(mode, priority, name, hook);
		if (mode == InternalCRIUSupport.HookMode.SINGLE_THREAD_MODE) {
			postRestoreHooksSingleThread.add(internalHook);
		} else if (mode == InternalCRIUSupport.HookMode.CONCURRENT_MODE) {
			postRestoreHooksConcurrentThread.add(internalHook);
		}
	}

	/**
	 * This is an internal API
	 *
	 * Register pre checkpoint hook that runs before the JVM state is checkpointed.
	 *
	 * Do not spawn new threads or make use of threadpools in the hooks.
	 *
	 * @param priority the priority in which the hook will be run, higher the number
	 *                 higher the priority
	 * @param name     the name of the hook
	 * @param hook     the runnable hook
	 */
	static synchronized void registerPreCheckpointHook(HookMode mode, int priority, String name, Runnable hook)
			throws UnsupportedOperationException {
		// The hook mode and priority have been verified  by the caller.
		J9InternalCheckpointHook internalHook = new J9InternalCheckpointHook(mode, priority, name, hook);
		if (InternalCRIUSupport.HookMode.SINGLE_THREAD_MODE == mode) {
			preCheckpointHooksSingleThread.add(internalHook);
		} else if (InternalCRIUSupport.HookMode.CONCURRENT_MODE == mode) {
			preCheckpointHooksConcurrentThread.add(internalHook);
		}
	}

	private static void runHooks(List<J9InternalCheckpointHook> hooks, boolean reverse) {
		boolean debug = System.getProperty("enable.j9internal.checkpoint.hook.api.debug") != null; //$NON-NLS-1$

		if (reverse) {
			Collections.sort(hooks, Collections.reverseOrder());
		} else {
			Collections.sort(hooks);
		}

		for (J9InternalCheckpointHook hookWrapper : hooks) {
			if (debug) {
				System.err.println(hookWrapper);
			}

			hookWrapper.runHook();
		}
	}

	/*
	 * Only called by the VM
	 */
	@SuppressWarnings("unused")
	private static void runPreCheckpointHooksSingleThread() {
		runHooks(preCheckpointHooksSingleThread, true);
	}

	/*
	 * Only called by the VM
	 */
	@SuppressWarnings("unused")
	private static void runPostRestoreHooksSingleThread() {
		runHooks(postRestoreHooksSingleThread, false);
	}

	static void runPreCheckpointHooksConcurrentThread() {
		runHooks(preCheckpointHooksConcurrentThread, true);
	}

	static void runPostRestoreHooksConcurrentThread() {
		runHooks(postRestoreHooksConcurrentThread, false);
	}

	final private static class J9InternalCheckpointHook implements Comparable<J9InternalCheckpointHook> {

		private final InternalCRIUSupport.HookMode hookMode;
		private final int priority;
		private final Runnable hook;
		private final String name;

		int getHookPriority() {
			return priority;
		}

		@Override
		public int compareTo(J9InternalCheckpointHook o) {
			return o.getHookPriority() - this.getHookPriority();
		}

		void runHook() {
			hook.run();
		}

		J9InternalCheckpointHook(InternalCRIUSupport.HookMode hookMode, int priority, String name, Runnable hook) {
			this.hookMode = hookMode;
			this.priority = priority;
			this.hook = hook;
			this.name = name;
		}

		@Override
		public String toString() {
			String hookModeStr = InternalCRIUSupport.HookMode.SINGLE_THREAD_MODE == hookMode ? "single-threaded" : "concurrent"; //$NON-NLS-1$ //$NON-NLS-2$
			return "[J9InternalCheckpointHook(" + hookModeStr + " mode): [" + name + "], priority:[" + priority //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
					+ "], runnable:[" + hook + "]]"; //$NON-NLS-1$ //$NON-NLS-2$
		}

		@Override
		public boolean equals(Object obj) {
			J9InternalCheckpointHook o = (J9InternalCheckpointHook) obj;
			if (this == o) {
				return true;
			} else if ((this.hookMode == o.hookMode) && (this.priority == o.priority) && (this.hook.equals(o.hook))
					&& (this.name.equals(o.name))) {
				return true;
			}

			return false;
		}

		@Override
		public int hashCode() {
			return super.hashCode();
		}
	}
}
