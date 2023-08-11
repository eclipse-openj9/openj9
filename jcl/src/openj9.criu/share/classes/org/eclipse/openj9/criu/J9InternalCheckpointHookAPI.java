/*[INCLUDE-IF CRIU_SUPPORT]*/
/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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
 *******************************************************************************/
package org.eclipse.openj9.criu;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

final class J9InternalCheckpointHookAPI {

	private static List<J9InternalCheckpointHook> postRestoreHooks = new ArrayList<>();
	private static List<J9InternalCheckpointHook> preCheckpointHooks = new ArrayList<>();

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
	static synchronized void registerPostRestoreHook(int priority, String name, Runnable hook) {
		postRestoreHooks.add(new J9InternalCheckpointHook(priority, name, hook));
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
	static synchronized void registerPreCheckpointHook(int priority, String name, Runnable hook) {
		preCheckpointHooks.add(new J9InternalCheckpointHook(priority, name, hook));
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
	private static void runPreCheckpointHooks() {
		runHooks(preCheckpointHooks, true);
	}

	/*
	 * Only called by the VM
	 */
	@SuppressWarnings("unused")
	private static void runPostRestoreHooks() {
		runHooks(postRestoreHooks, false);
	}

	final private static class J9InternalCheckpointHook implements Comparable<J9InternalCheckpointHook> {

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

		J9InternalCheckpointHook(int priority, String name, Runnable hook) {
			this.priority = priority;
			this.hook = hook;
			this.name = name;
		}

		@Override
		public String toString() {
			return "[J9InternalCheckpointHook: [" + name + "], priority:[" + priority + "], runnable:[" + hook + "]]"; //$NON-NLS-1$//$NON-NLS-2$ //$NON-NLS-3$ //$NON-NLS-4$
		}

		@Override
		public boolean equals(Object obj) {
			J9InternalCheckpointHook o = (J9InternalCheckpointHook) obj;
			if (this == o) {
				return true;
			} else if ((this.priority == o.priority) && (this.hook.equals(o.hook)) && (this.name.equals(o.name))) {
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
