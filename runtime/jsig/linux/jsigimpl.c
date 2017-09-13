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

#ifdef LINUX
#define _GNU_SOURCE 1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <dlfcn.h>

#include "jsigunix.h"

/**************************/
/* PRIVATE JSIG FUNCTIONS */
/**************************/

/*********************************************************************/
/* Function Name: real_sigaction()                                   */
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
    extern int jsig_sigaction_isdefault(void);
    static sigactionfunc_t real_sigaction_fn = 0; 
    if (real_sigaction_fn == 0) {
        if (jsig_sigaction_isdefault()) {
            real_sigaction_fn = (sigactionfunc_t)dlsym(RTLD_NEXT, "sigaction");
            if (real_sigaction_fn == 0) {
				real_sigaction_fn = (sigactionfunc_t)dlsym(RTLD_DEFAULT, "sigaction");
            }
        } else {
            real_sigaction_fn = (sigactionfunc_t)dlsym(RTLD_DEFAULT, "sigaction");
            if (real_sigaction_fn == 0) {
				real_sigaction_fn = (sigactionfunc_t)dlsym(RTLD_NEXT, "sigaction");
            }
        }
        if (real_sigaction_fn == 0) {
            fprintf(stderr, "libjsig unable to find sigaction - %s\n", dlerror());
            abort();
        }
    }
    return (*real_sigaction_fn)(sig, act, oact);
}

/*********************************************************************/
/* Function Name: real_sigprocmask()                                 */
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
    return pthread_sigmask(option, new_set, old_set);
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
    return bsd_signal(sig, disp);
}


/*********************************************************************/
/* Function Name: ssignal()                                          */
/*                                                                   */
/* Description:   Interposed signal system function.                 */
/*                Note this is implemented with sigaction            */
/*                                                                   */
/* Parameters:                                                       */
/*                                                                   */
/* Returns:                                                          */
/*********************************************************************/
sig_handler_t
ssignal(int sig, sig_handler_t disp)
{
    return bsd_signal(sig, disp);
}


/*********************************************************************/
/* Function Name: __sysv_signal()                                    */
/*                                                                   */
/* Description:   Interposed signal system function.                 */
/*                Note this is implemented with sigaction            */
/*                                                                   */
/* Parameters:                                                       */
/*                                                                   */
/* Returns:                                                          */
/*********************************************************************/
sig_handler_t
__sysv_signal(int sig, sig_handler_t disp)
{
    return sysv_signal(sig, disp);
}

/***************/
/* End of File */
/***************/

