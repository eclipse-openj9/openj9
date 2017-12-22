/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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

#if !defined(J9SC_STRING_TRANSACTION_HPP_INCLUDED)
#define J9SC_STRING_TRANSACTION_HPP_INCLUDED

#include "j9.h"
#include "j9generated.h"
#include "SCAbstractAPI.h"

class SCStringTransaction
{
public:
	/**
	 * Construct a transaction to allow the use of the shared string intern tree, with promotion & search enabled
	 */
	SCStringTransaction(J9VMThread* currentThread)
	{
		if (currentThread != NULL && currentThread->javaVM != NULL && currentThread->javaVM->sharedClassConfig != NULL) {
			sharedapi = (SCAbstractAPI *)(currentThread->javaVM->sharedClassConfig->sharedAPIObject);
			if (sharedapi != NULL) {
				sharedapi->stringTransaction_start((void *) &tobj, currentThread);
			}
		} else {
			sharedapi = NULL;
		}
		return;
	}

	/**
	 * Destruct the transaction
	 */
	~SCStringTransaction()
	{
		if ((sharedapi != NULL)) {
			sharedapi->stringTransaction_stop((void *) &tobj);
		}
		sharedapi = NULL;
	}

	/**
	 * Returns true if the state our the state of our transaction is ok. 
	 * Meaning we own the required locks and resources.
	 */
	bool isOK()
	{
		if ((sharedapi != NULL)) {
			if ( sharedapi->stringTransaction_IsOK((void *) &tobj) == TRUE ) {
				return true;
			}
		}
		return false;
	}

private:
	SCAbstractAPI * sharedapi;
	J9SharedStringTransaction tobj;

	/*
	 * We expect this class to always be allocated on the stack so new and delete are private
	 */
	void operator delete(void *)
	{
		return;
	}

	void *operator new(size_t size) throw ()
	{
		return NULL;
	}

};

#endif /* J9SC_STRING_TRANSACTION_HPP_INCLUDED */
