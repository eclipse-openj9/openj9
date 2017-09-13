/*******************************************************************************
 * Copyright (c) 2003, 2016 IBM Corp. and others
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

/*****************/
/* Header files. */
/*****************/

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "jsigunix.h"

/**************************/
/* PRIVATE JSIG FUNCTIONS */
/**************************/

/*********************************************************************/
/* Function Name: call_os_sigaction()                                */
/*                                                                   */
/* Description:   Calls the real sigaction.                          */
/*                                                                   */
/* Parameters:    sig - Signal number                                */
/*                act - sigaction for signal handler to be installed */
/*                oact - sigaction for previously installed handler  */
/*                                                                   */
/* Returns:       return code from sigaction function                */
/*********************************************************************/
int real_sigaction(int sig, const struct sigaction *act,
                      struct sigaction *oact)
{
    static sigactionfunc_t real_sigaction = 0;     
    if (real_sigaction == 0) {
        #include <dlfcn.h>
        void *libhandle = 0;
        char *libname;    

        /* If environment variable IBM_JSIG_CRUNTIME is defined then use */
        /* that for the system library name, otherwise default to        */
        /* /usr/lib/libc.a(shr.o) (or shr_64.o)                ibm@57285 */

        libname = getenv("IBM_JSIG_CRUNTIME");
        if (libname == 0) {

#if defined(__64BIT__)                                      /* ibm@57285 */
            libname = "/usr/lib/libc.a(shr_64.o)";          /* ibm@57285 */
#else                                                       /* ibm@57285 */
            libname = "/usr/lib/libc.a(shr.o)";             /* ibm@57285 */
#endif                                                      /* ibm@57285 */
        }

        /* Open the system library, if this fails then abort */
        libhandle = dlopen (libname, RTLD_LAZY|RTLD_GLOBAL|RTLD_MEMBER);
        if (libhandle == NULL) {
            fprintf(stderr, "libjsig %s not found - error %s\n",libname, dlerror());
            abort();
        }

        /* Get the system sigaction function from the system library */
        /* If this fails then abort                                  */
        real_sigaction = (sigactionfunc_t)dlsym(libhandle, "sigaction");
        if (real_sigaction == 0) {
            fprintf(stderr, "libjsig unable to find sigaction - %s\n", dlerror());
            abort();
        }
    }

    /* Call the system sigaction function */
    return (*real_sigaction)(sig, act, oact);
}


/*********************************************************************/
/* Function Name: real_sigprocmask()                              */
/*                                                                   */
/* Description:   Calls the real sigprocmask. This just calls        */
/*                sigprocmask for all platforms except on AIX        */
/*                where sigthreadmask is called                      */
/*                                                                   */
/* Parameters:                                                       */
/*                                                                   */
/* Returns:       return code from sigprocmask function              */
/*********************************************************************/
int real_sigprocmask(int option, const sigset_t *new_set, sigset_t *old_set)
{
    return sigthreadmask(option, new_set, old_set);
}


/*************************/
/* PUBLIC JSIG FUNCTIONS */
/*************************/

/*********************************************************************/
/* Function Name: signal()                                           */
/*                                                                   */
/* Description:   Interposed signal system function.                 */
/*                Note this is implemented with sigaction            */
/*                                                                   */
/* Parameters:                                                       */
/*                                                                   */
/* Returns:                                                          */
/*********************************************************************/
sig_handler_t
signal(int sig, sig_handler_t disp)
{
    return sysv_signal(sig, disp);
}

/***************/
/* End of File */
/***************/

