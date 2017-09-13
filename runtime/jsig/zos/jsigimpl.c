/*******************************************************************************
 * Copyright (c) 2003, 2014 IBM Corp. and others
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

#include "jsigunix.h"

/*********************/
/* Local Definitions */
/*********************/

#pragma map (syssigaction, "\174\174SIGACT")
int syssigaction(int, const struct sigaction *, struct sigaction *);

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
int 
real_sigaction(int sig, const struct sigaction *act,
                             struct sigaction *oact)
{
    return syssigaction(sig, act, oact);
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
int 
real_sigprocmask(int option, const sigset_t *new_set, sigset_t *old_set)
{
    return sigprocmask(option, new_set, old_set);
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


/*********************************************************************/
/* Function Name: __sigactionset()                                   */
/*                                                                   */
/* Description:   Interposed __sigactionset system function.         */
/*                                                                   */
/* Parameters:                                                       */
/*                                                                   */
/* Returns:                                                          */
/*********************************************************************/
int 
__sigactionset(size_t newct, const __sigactionset_t newset[],
                   size_t *oldct, __sigactionset_t oldset[],
                   int options)
{
    #define NSIGS (sizeof(sigset_t)*8 - 1)
    int i;
    int sig;
    int status = 0;                                    
    int return_oldct = 0;
    struct sigaction act;
    struct sigaction oact[NSIGS];
    int oact_set[NSIGS];
    /* Get details for all signals */
    memset(&oact, 0, sizeof(struct sigaction)*NSIGS);
    memset(&oact_set, 0, sizeof(int)*NSIGS);
    for ( sig=1; sig < NSIGS && oldct && oldset; sig++ ) {
        if (sigaction(sig, 0, &oact[sig]) == 0) {
            oact_set[sig] = 1;
        }
    }

    /* Build the return old array */
    if ( oldct ) {                                                                    
      sig = 0;                                                                        
      for ( i=0; oldset && i < *oldct && sig < NSIGS; i++ ) {                            
        sigemptyset(&oldset[i].__sa_signals);
        for ( sig=i+1; sig < NSIGS; sig++ ) {                                         
            if (oact_set[sig]) {
                int s;
                return_oldct = i+1;
                sigaddset(&oldset[i].__sa_signals, sig);
                oact_set[sig] = 0;                                                
                oldset[i].__sa_handler = oact[sig].sa_handler;
                oldset[i].__sa_sigaction = oact[sig].sa_sigaction;
                memcpy(&oldset[i].__sa_mask, &oact[sig].sa_mask, sizeof(act.sa_mask));
                oldset[i].__sa_flags = oact[sig].sa_flags;
                for (s=sig+1; s < NSIGS; s++ ) {
                    int different = 0;
                    different |= (oact[sig].sa_handler != oact[s].sa_handler);
                    different |= (oact[sig].sa_sigaction != oact[s].sa_sigaction);
                    different |= memcmp(&oact[sig].sa_mask, &oact[s].sa_mask, sizeof(oact[s].sa_mask));
                    different |= memcmp(&oact[sig].sa_flags, &oact[s].sa_flags, sizeof(oact[s].sa_flags));
                    if (0 == different) {
                        sigaddset(&oldset[i].__sa_signals, s);
                        oact_set[s] = 0;
                    }
                }
                break;                                                                
            }
        }
      }
      *oldct = return_oldct;
    }    

    /* Install the required signal handlers */
    for ( i=0; newset && i < newct && status == 0; i++ ) {              
        act.sa_handler = newset[i].__sa_handler;                        
        act.sa_sigaction = newset[i].__sa_sigaction;
        memcpy(&act.sa_mask, &newset[i].__sa_mask, sizeof(act.sa_mask));
        act.sa_flags = newset[i].__sa_flags;                            
        for ( sig=1; sig < NSIGS && status == 0; sig++ ) {           
            if (sigismember(&newset[i].__sa_signals,sig) == 1) {
                status = sigaction(sig, &act, 0);
            }
        }
    }

    return status;
}


/***************/
/* End of File */
/***************/

