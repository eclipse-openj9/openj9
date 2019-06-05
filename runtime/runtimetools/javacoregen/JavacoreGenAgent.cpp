/*******************************************************************************
 * Copyright (c) 2011, 2019 IBM Corp. and others
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
#include "JavacoreGenAgent.hpp"

/* used to store reference to the agent */
static JavacoreGenAgent* javacoreGenAgent;

/*********************************************************
 * Methods exported for agent
 */
#ifdef __cplusplus
extern "C" {
#endif

void JNICALL
Agent_OnUnload(JavaVM * vm)
{
	JavacoreGenAgent* agent = JavacoreGenAgent::getAgent(vm, &javacoreGenAgent);
	agent->shutdown();
	agent->kill();
}

jint JNICALL
Agent_OnLoad(JavaVM * vm, char * options, void * reserved)
{
	JavacoreGenAgent* agent = JavacoreGenAgent::getAgent(vm, &javacoreGenAgent);
	return agent->setup(options);
}

jint JNICALL
Agent_OnAttach(JavaVM * vm, char * options, void * reserved)
{
	JavacoreGenAgent* agent = JavacoreGenAgent::getAgent(vm, &javacoreGenAgent);
	return  agent->setup(options);
	/* need to add code here that will get jvmti_env and jni env and then call startGenerationThread */
}

#ifdef __cplusplus
}
#endif

/************************************************************************
 * Start of class methods
 *
 */

/**
 * This method is used to initialize an instance
 * @returns true if initialization was successful
 */
bool JavacoreGenAgent::init()
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());

	if (!RuntimeToolsIntervalAgent::init()){
		return false;
	}

	_cls = NULL;
	_method = NULL;

	return true;

}

/**
 * This method should be called to destroy the object and free the associated memory
 */
void JavacoreGenAgent::kill()
{
	PORT_ACCESS_FROM_JAVAVM(getJavaVM());

	j9mem_free_memory(this);
}


/**
 * This method is used to create an instance
 *
 * @param vm java vm that can be used by this manager
 * @returns an instance of the class
 */
JavacoreGenAgent* JavacoreGenAgent::newInstance(JavaVM * vm)
{
	J9JavaVM * javaVM = ((J9InvocationJavaVM *)vm)->j9vm;
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	JavacoreGenAgent* obj = (JavacoreGenAgent*) j9mem_allocate_memory(sizeof(JavacoreGenAgent), OMRMEM_CATEGORY_VM);
	if (obj){
		new (obj) JavacoreGenAgent(vm);
		if (!obj->init()){
			obj->kill();
			obj = NULL;
		}
	}
	return obj;
}

/**
 * method that generates the calls to runAction at the appropriate intervals *.
 */
void JavacoreGenAgent::runAction()
{
	/* if this is the first time lookup the class/method id we need to generate the javacore */
	if (_method == NULL){
		_cls = getEnv()->FindClass("com/ibm/jvm/Dump");
		if (!getEnv()->ExceptionCheck()) {
			_method = getEnv()->GetStaticMethodID(_cls, "JavaDump", "()V");
			if (getEnv()->ExceptionCheck()) {
				_method = NULL;
				error("failed to lookup JavaDump method\n");
			}
		} else {
			error("failed to lookup Dump class\n");
		}
	}

	/* generate a javacore */
	if (_method != NULL){
		getEnv()->CallStaticVoidMethod(_cls, _method);
	}
}
