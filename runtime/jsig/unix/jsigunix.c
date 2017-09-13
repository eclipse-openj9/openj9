/*******************************************************************************
 * Copyright (c) 2003, 2017 IBM Corp. and others
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
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#if defined(LINUX) && !defined(J9ZTPF)
#include <bits/signum.h>
#endif

#include "jsigunix.h"

/*********************/
/* Local Definitions */
/*********************/

/* signal handler function types */
typedef void (*sigact_handler_t)(int, siginfo_t *, void *);

/* saved non-primary signal handlers */
static struct sigaction *saved_sigaction = (struct sigaction *)0;

/* Array of bits indicating whether or not the a primnary or application signal handler is installed */
static sigset_t primary_sigs; 
static sigset_t appl_sigs; 
#define MAX_SIGNAL (sizeof(primary_sigs)*8 - 1)

#define SET_PRIMARY_HANDLER(sig) sigaddset(&primary_sigs, sig);
#define CLEAR_PRIMARY_HANDLER(sig) sigdelset(&primary_sigs, sig);
#define PRIMARY_HANDLER_INSTALLED(sig) ((sigismember(&primary_sigs, sig) == 1) ? 1 : 0) 
#define SET_APPL_HANDLER(sig) sigaddset(&appl_sigs, sig);
#define APPL_HANDLER_INSTALLED(sig) ((sigismember(&appl_sigs, sig) == 1) ? 1 : 0) 

/* Macro to add a sigset_t mask to another sigset_t mask */
#define SIGADDMASK(mask, addmask) \
    { int i; \
      for (i=0; i < sizeof(sigset_t)*8; i++) { \
          if (sigismember(&addmask, i)) sigaddset(&mask, i); \
      } \
    }

/* Macros for locking */
static pthread_mutex_t jsig_mutex = PTHREAD_MUTEX_INITIALIZER;

#define SIGACTION_LOCK pthread_mutex_lock(&jsig_mutex);
#define SIGACTION_UNLOCK pthread_mutex_unlock(&jsig_mutex);

/**************************/
/* PRIVATE JSIG FUNCTIONS */
/**************************/

/*********************************************************************/
/* Function Name: jsig_init()                                        */
/*                                                                   */
/* Description:   Initialises the jsig signal handler list. It       */
/*                assumes that it is called within SIGACTION_LOCK    */
/*                                                                   */
/* Parameters:    None                                               */
/*                                                                   */
/* Returns:       None                                               */
/*********************************************************************/
static void 
jsig_init(void)
{
	if (saved_sigaction == 0) {
		saved_sigaction = (struct sigaction *)malloc((MAX_SIGNAL+1) * (size_t)sizeof(struct sigaction));
		if (saved_sigaction == 0) { 
			fprintf(stderr, "libjsig unable to allocate memory\n");
			abort();
		}
		memset(saved_sigaction, 0, (MAX_SIGNAL+1) * (size_t)sizeof(struct sigaction));
	}

	sigemptyset(&primary_sigs);
}


static int jsig_sigaction_avail = 0;
#define INVALID_TEST_SIGNAL -2

/*********************************************************************/
/* Function Name: jsig_sigaction_isdefault()                         */
/*                                                                   */
/* Description:   Returns TRUE(1) if the jsig library sigaction 		 */
/*                function is the application's default, otherwise   */
/*                FALSE(0).                                          */
/*                Note that this is used by the Linux platform to    */
/*                determine whether or not the application has been  */
/*                linked such that jsig's sigaction is function is   */
/*                being picked up by default                         */
/*                                                                   */
/* Parameters:    None                                               */
/*                                                                   */
/* Returns:       None                                               */
/*********************************************************************/

int jsig_sigaction_isdefault()
{
#if defined(J9ZTPF)
    return 1;
#else /* defined(J9ZTPF) */
    sigaction(INVALID_TEST_SIGNAL, 0, 0);
    return jsig_sigaction_avail;
#endif /* !defined(J9ZTPF) */
}


/*************************/
/* PUBLIC JSIG FUNCTIONS */
/*************************/

/*********************************************************************/
/* Function Name: omrsig_handler()                                     */
/*                                                                   */
/* Description:   jsig's signal handler, responsible for chaining    */
/*                the installed signal handlers for the passed       */
/*                signal.                                            */
/*                Note that this can be called by the JVM            */
/*                to explicitly chain to the other application       */
/*                signal handlers                                    */
/*                Note also that for Linux when SA_NODEFER is set    */
/*                the sa_mask specified to sigaction is ignored.     */
/*                                                                   */
/* Parameters:                                                       */
/*                                                                   */
/* Returns:       Value indicating whether default OS action is      */
/*                required by application or whether it has handled  */
/*                (or ignored) the signal                            */
/*********************************************************************/
int 
omrsig_handler(int sig, void *siginfo, void *uc)
{
#ifdef J9ZTPF
    void (*sa_handler)();
#endif /* J9ZTPF */
    int rc = JSIG_RC_DEFAULT_ACTION_REQUIRED;
    sigset_t save_set;
    sigset_t mask;
    struct sigaction act;

    /* jsig not initialized, so do nothing */
    if ( 0 == saved_sigaction ) {return rc;}

    act = saved_sigaction[sig];
    
    /* Reset the signal handler if required */
    if (saved_sigaction[sig].sa_flags & SA_RESETHAND) {
         memset(&saved_sigaction[sig], 0, sizeof(struct sigaction));
    }
    
    /* Append signals to mask, according to SA_NODEFER and */
    /* specified in sa_mask for the handler                */
    real_sigprocmask(SIG_SETMASK, 0, &save_set);
    mask = save_set;
    if (act.sa_flags & SA_NODEFER) {
        sigdelset(&mask, sig);              /* Don't block signal by default */
    } else {
        sigaddset(&mask, sig);              /* Block the signal */
    }
    #ifdef LINUX
    if (!(act.sa_flags & SA_NODEFER))
    #endif
        SIGADDMASK(mask, act.sa_mask);          /* Add required set of signals to mask */
    real_sigprocmask(SIG_SETMASK, &mask, 0);

    /* Call the required signal handler */
    if (act.sa_flags & SA_SIGINFO) {
        if (act.sa_sigaction != 0) {
			rc = JSIG_RC_SIGNAL_HANDLED;
            act.sa_sigaction(sig, (siginfo_t*)siginfo, uc);
        }
    } else {
        if (act.sa_handler != 0 &&
            act.sa_handler != (sig_handler_t)SIG_DFL &&
            act.sa_handler != (sig_handler_t)SIG_IGN) {
			rc = JSIG_RC_SIGNAL_HANDLED;
            act.sa_handler(sig);
        } else {
			if (act.sa_handler != (sig_handler_t)SIG_DFL) {
				rc = JSIG_RC_SIGNAL_HANDLED;
            }
        }
    }

    /* Restore the signal mask */
    real_sigprocmask(SIG_SETMASK, &save_set, 0);

	/* Note: The return value is only really necessary and used on Linux */
    return rc;
}


/*********************************************************************/
/* Function Name: omrsig_primary_sigaction()                           */
/*                                                                   */
/* Description:   Installs the primary signal handler for the        */
/*                specified signal.                                  */
/*                                                                   */
/* Parameters:                                                       */
/*                                                                   */
/* Returns:       0 if successful, -1 if an error occurred           */
/*********************************************************************/
int 
omrsig_primary_sigaction(int sig, const struct sigaction *act,
                       struct sigaction *oact)
{
    struct sigaction oldact = {0};
    int status = 0;
    int primaryPreviouslyInstalled;
    int applPreviouslyInstalled;

    if (sig < 0 || sig > MAX_SIGNAL) {
        errno = EINVAL;
        return -1;
    }

    SIGACTION_LOCK

    /* Initialise jsig if this is the first call */
    if (saved_sigaction == 0) jsig_init();
  
    primaryPreviouslyInstalled = PRIMARY_HANDLER_INSTALLED(sig);
  
    applPreviouslyInstalled = APPL_HANDLER_INSTALLED(sig);

    if ( act != 0 ) {
        /* New handler details have been provided                  */
        if ( !(act->sa_flags & SA_SIGINFO) &&
             ( act->sa_handler == (sig_handler_t)SIG_DFL ||
               act->sa_handler == (sig_handler_t)SIG_IGN ) ) {
            /* Request to set to Default or Ignore.  Check further */
            /* to determine what needs to be done                  */
            if ( applPreviouslyInstalled ) {
                /* Application handler has been installed for this */
                /* signal.  If Primary was previously installed    */
                /* reset OS to application handler, otherwise do   */
                /* nothing as it is already installed for OS       */
                if ( primaryPreviouslyInstalled ) {
                    status = real_sigaction(sig, &saved_sigaction[sig], &oldact);
                    if (status == 0) {
                        memset(&saved_sigaction[sig], 0, sizeof(struct sigaction));
                    }
                }
            } else {
                /* No application handler has been installed, so   */
                /* if a jvm handler has been installed, reset the  */
                /* handler to its previous value.  If a jvm handler*/
                /* has not previously been installed, install the  */
                /* requested action as long as the current action  */
                /* is SIG_DFL (This is to support a calling        */
                /* application which has installed a handler, but  */
                /* hasn't linked with jsig                         */
                if ( primaryPreviouslyInstalled &&
                     ( (saved_sigaction[sig].sa_flags & SA_SIGINFO) ||
                        ( saved_sigaction[sig].sa_handler != (sig_handler_t)SIG_DFL && 
                          saved_sigaction[sig].sa_handler != (sig_handler_t)SIG_IGN ) ) ) {
                    status = real_sigaction(sig, &saved_sigaction[sig], &oldact);
                    if (status == 0) {
                        memset(&saved_sigaction[sig], 0, sizeof(struct sigaction));
                    }
                } else {
				    status = real_sigaction(sig, NULL, &oldact);
					if (!primaryPreviouslyInstalled && 
						status == 0 && 
						((oldact.sa_flags & SA_SIGINFO) ||
						oldact.sa_handler != (sig_handler_t)SIG_DFL )) {
							/* do nothing a handler has been installed without going through JSIG */
		    		} else {
						status = real_sigaction(sig, act, &oldact);
			    	}
                }
            }
            CLEAR_PRIMARY_HANDLER(sig);
        } else {
            /* Request to install handler, so install it */
            status = real_sigaction(sig, act, &oldact);
            if (status == 0 && !primaryPreviouslyInstalled) {
                saved_sigaction[sig] = oldact;
                SET_PRIMARY_HANDLER(sig);
            }
        }
    } else {
        status = real_sigaction(sig, NULL, &oldact);
    }
   
    /* If a primary handler was previously installed then return */
    /* as oact to caller otherwise return nothing                */
    if ( oact != 0 ) {
        if (primaryPreviouslyInstalled ||
               (!(oldact.sa_flags & SA_SIGINFO) &&
               (oldact.sa_handler == (sig_handler_t)SIG_DFL ||
                oldact.sa_handler == (sig_handler_t)SIG_IGN))) {
            *oact = oldact;
        } else {
            memset( oact, 0, sizeof( struct sigaction ) );
        }
    }
    
    SIGACTION_UNLOCK

    if ( oact != NULL && &oact->sa_handler != (void (**)(int))&oact->sa_sigaction ) {
        if ( oact->sa_flags & SA_SIGINFO ) {
            oact->sa_handler = (void (*)(int))oact->sa_sigaction;
        } else {
            oact->sa_sigaction = (void (*)(int, siginfo_t *, void *))oact->sa_handler;
        }
    }

    return status;
}


/*********************************************************************/
/* Function Name: omrsig_primary_signal()                              */
/*                                                                   */
/* Description:   Installs the primary signal handler for the        */
/*                specified signal. This function allows the         */
/*                use of a "signal" equivalent call in J9.           */
/*                Note this is implemented with sigaction.           */
/*                                                                   */
/* Parameters:                                                       */
/*                                                                   */
/* Returns:                                                          */
/*********************************************************************/
sig_handler_t 
omrsig_primary_signal(int sig, sig_handler_t disp)
{
    struct sigaction act;
    struct sigaction oact;

    act.sa_handler = disp;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;
    memset(&oact, 0, sizeof(struct sigaction));
    if (omrsig_primary_sigaction(sig, &act, &oact) != 0) {
        oact.sa_handler = SIG_ERR;
    }

    return oact.sa_handler;
}


/*********************************************************************/
/* Function Name: sigaction()                                        */
/*                                                                   */
/* Description:   Interposed sigaction system function. This only    */
/*                installs the required signal handler if there is   */
/*                no primary signal handler.                         */
/*                                                                   */
/* Parameters:                                                       */
/*                                                                   */
/* Returns:                                                          */
/*********************************************************************/
int 
sigaction(int sig, const struct sigaction *act, struct sigaction *oact)
{
    int status = 0;
    struct sigaction oldact;

    if (sig == INVALID_TEST_SIGNAL) {
        jsig_sigaction_avail = 1;
        errno = EINVAL;
        return -1;
    }
    
    if (sig < 0 || sig > MAX_SIGNAL) {
        errno = EINVAL;
        return -1;
    }

    SIGACTION_LOCK
 
    /* Initialise jsig if this is the first call */
    if (saved_sigaction == 0) jsig_init();
  
    /* If primary signal handler installed then just save the passed */
    /* signal info and return the previously saved signal info       */
    /* Otherwise just call the real sigaction function               */
    if (PRIMARY_HANDLER_INSTALLED(sig)) {
        if (oact != 0) *oact = saved_sigaction[sig];
        if (act != 0) saved_sigaction[sig] = *act;
    } else {
        status = real_sigaction(sig, act, oact);
    }

    /* NOTE: The application handler is a latch that can be set      */
    /*       and left if sigaction is called, even if the latest     */
    /*       action is SIG_DFL or SIG_IGN                            */
    SET_APPL_HANDLER(sig);

    SIGACTION_UNLOCK

    if ( oact != NULL && &oact->sa_handler != (void (**)(int))&oact->sa_sigaction ) {
        if ( oact->sa_flags & SA_SIGINFO ) {
            oact->sa_handler = (void (*)(int))oact->sa_sigaction;
        } else {
            oact->sa_sigaction = (void (*)(int, siginfo_t *, void *))oact->sa_handler;
        }
    }


    return status;
}


/*********************************************************************/
/* Function Name: sysv_signal()                                      */
/*                                                                   */
/* Description:   sysv_signal system function, used to implement     */
/*                the interposed signal system function for          */
/*                System 5 Unix                                      */
/*                Note this is implemented with sigaction            */
/*                                                                   */
/* Parameters:                                                       */
/*                                                                   */
/* Returns:                                                          */
/*********************************************************************/
sig_handler_t 
sysv_signal(int sig, sig_handler_t disp)
{
    struct sigaction act;
    struct sigaction oact;

    act.sa_handler = disp;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESETHAND | SA_NODEFER;
    memset(&oact, 0, sizeof(struct sigaction));
    if (sigaction(sig, &act, &oact) != 0) {
        oact.sa_handler = SIG_ERR;
    }
    
    return oact.sa_handler;
}


/*********************************************************************/
/* Function Name: bsd_signal()                                       */
/*                                                                   */
/* Description:   bsd_signal system function, used to implement      */
/*                the interposed signal system function for          */
/*                BSD Unix                                           */
/*                Note this is implemented with sigaction            */
/*                                                                   */
/* Parameters:                                                       */
/*                                                                   */
/* Returns:                                                          */
/*********************************************************************/
sig_handler_t 
bsd_signal(int sig, sig_handler_t disp)
{
    struct sigaction act;
    struct sigaction oact;

    act.sa_handler = disp;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;
    memset(&oact, 0, sizeof(struct sigaction));
    if (sigaction(sig, &act, &oact) != 0) {
        oact.sa_handler = SIG_ERR;
    }

    return oact.sa_handler;
}


/*********************************************************************/
/* Function Name: sigset()                                           */
/*                                                                   */
/* Description:   Interposed signal system function.                 */
/*                Note this is implemented with sigaction            */
/*                Note also that for Linux only when disp is         */
/*                specified as SIG_HOLD, SIG_HOLD is always returned */
/*                                                                   */
/* Parameters:                                                       */
/*                                                                   */
/* Returns:                                                          */
/*********************************************************************/
sig_handler_t 
sigset(int sig, sig_handler_t disp)
{
    struct sigaction act;
    struct sigaction oact;
    
#if !defined(J9ZTPF)
    if (disp == SIG_HOLD) {
        sigset_t mask;
        real_sigprocmask(SIG_SETMASK, 0, &mask);
        if (sigismember(&mask, sig)) {
            oact.sa_handler = SIG_HOLD;
        } else {
            memset(&oact, 0, sizeof(struct sigaction));
            if (sigaction(sig, 0, &oact) != 0) {
                oact.sa_handler = SIG_ERR;
            }
            #ifdef LINUX
            else {
                oact.sa_handler = SIG_HOLD;
            }
            #endif
        }
        sigaddset(&mask, sig);
        real_sigprocmask(SIG_SETMASK, &mask, 0);
    } else
#endif /* !defined(J9ZTPF) */
    {
        act.sa_handler = disp;
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
        memset(&oact, 0, sizeof(struct sigaction));
        if (sigaction(sig, &act, &oact) != 0) {
            oact.sa_handler = SIG_ERR;
        }
    }

    return oact.sa_handler;
}


/*********************************************************************/
/* Function Name: sigignore()                                        */
/*                                                                   */
/* Description:   Interposed sigignore system function.              */
/*                Note this assumes standard ANSI rules              */
/*                (i.e. not BSD) and is implemented with sigaction   */
/*                                                                   */
/* Parameters:                                                       */
/*                                                                   */
/* Returns:                                                          */
/*********************************************************************/
int 
sigignore(int sig)
{
    return (signal(sig, SIG_IGN) != (sig_handler_t)-1) ? 0 : -1;
}


/***************/
/* End of File */
/***************/

