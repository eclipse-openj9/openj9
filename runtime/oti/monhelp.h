/*******************************************************************************
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
 *******************************************************************************/
#ifndef J9_MONHELP_H_
#define J9_MONHELP_H_

/* @ddr_namespace: default */
/*
 * Calculate the flatlock recursion count for an owned or reserved flat lock.
 * NOTE: this will return an incorrect value if the flat lock is unowned or for an inflated lock
 * The location of the RC field is slightly different under the Learning state so a check is done to deteremine the shift value.
 * Flat state has a different internal representation of the same nested locking depth compared to Reserved and Learning. Under Flat, 1 needs to be added before returning the value.
 */
#if defined(J9VM_THR_LOCK_RESERVATION)
#define J9_FLATLOCK_COUNT(lock) \
	((((lock) & OBJECT_HEADER_LOCK_BITS_MASK) >> \
		(((lock) & OBJECT_HEADER_LOCK_LEARNING) \
			? OBJECT_HEADER_LOCK_LEARNING_RECURSION_OFFSET \
			: OBJECT_HEADER_LOCK_V2_RECURSION_OFFSET)) \
	+ (((lock) & (OBJECT_HEADER_LOCK_RESERVED | OBJECT_HEADER_LOCK_LEARNING)) ? 0 : 1))
#else
#define J9_FLATLOCK_COUNT(lock)  ((((lock) & OBJECT_HEADER_LOCK_BITS_MASK) >> OBJECT_HEADER_LOCK_V2_RECURSION_OFFSET) + 1)
#endif

#define J9_FLATLOCK_OWNER(lockWord) ((J9VMThread *)((UDATA)(lockWord) & (~(UDATA)OBJECT_HEADER_LOCK_BITS_MASK)))

#define J9_LOCK_IS_INFLATED(lockWord) ((lockWord) & OBJECT_HEADER_LOCK_INFLATED)
#define J9_INFLLOCK_OBJECT_MONITOR(lockWord) ((J9ObjectMonitor *)((UDATA)(lockWord) & (~(UDATA)OBJECT_HEADER_LOCK_INFLATED)))
#define J9_INFLLOCK_MONITOR(lockWord) (J9_INFLLOCK_OBJECT_MONITOR(lockWord)->monitor)

#define J9_INFLLOCK_ABSTRACT_MONITOR(lockWord) ((J9ThreadAbstractMonitor*)J9_INFLLOCK_MONITOR(lockWord))

#define J9_LOCK_IS_FLATLOCKED(lockWord) \
		(!J9_LOCK_IS_INFLATED(lockWord) \
				&& (NULL != J9_FLATLOCK_OWNER(lockWord)) \
				&& (J9_FLATLOCK_COUNT(lockWord) > 0))

#endif /*J9_MONHELP_H_*/
