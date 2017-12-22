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

#pragma optimize("", off)
#pragma inline_depth(0)

#include <windows.h>
#include <stdio.h>
#include <time.h>

#include "jni.h"
#include "j9port.h"
#include "exelib_api.h"

char *logpath = NULL;
char *jvmpath = NULL;
char *classpath = NULL;
char *classname = NULL;
char *stopmethod = NULL;
char *cachename = NULL;
char *tracepath = NULL;
char *install = NULL;
char *uninstall = NULL;
char *exepath = NULL;
char *srvdesc = NULL;
char *start = NULL;
char *stop = NULL;
char *noArgumentFor = NULL;
char *help = NULL;

HANDLE  stopEventHandle;
HANDLE  threadHandle = NULL;
LPTSTR  serviceName;
SERVICE_STATUS_HANDLE   servStatHandle;
FILE *newStdout;
FILE *newStderr;

JavaVM *jvm;

UDATA InstallService(void);
UDATA UninstallService(void);
UDATA StartTheService(int argc, char *argv[]);
UDATA StopTheService(void);
void WINAPI ServiceMainFunc(DWORD argc, LPTSTR *argv);
void WINAPI ServiceControlFunc(DWORD controlCode);
void RedirectStdoutStderr(char *filepath);
void WriteLog(LPTSTR logString);
void ProcessArgs(int argc, char *argv[]);
void ProcessMainArgs(int argc, char *argv[]);
char* OptionArg(int *i, int argc, char *argv[], char* argname);
void Usage(void);
void UpdateStatus(DWORD currentState, DWORD win32ExitCode, DWORD waitHint);
void ErrorHandler(LPTSTR errorFunc);
DWORD WINAPI JavaThread(LPVOID parm);
void StopApplProcessing(void);

#define PROCESSOPTION(o) optionRecognised = 1; \
    if (argRequired) { \
        o = OptionArg(&i, argc, argv, #o); \
    } else { \
        o = "No argument required"; \
    }
	

#define VARLOG(s) sprintf(logBuf, s); \
	WriteLog(logBuf);

/*
 * Main function.  
 * Redirects stdout and stderr to file and sets to no buffering
 * Calls Start Service Control Dispatcher to start the service
 * Tidy up and terminate on return
 */
UDATA 
signalProtectedMain(struct J9PortLibrary *portLibrary, void *arg)
{
/* void main(int argc, char *argv[]) { */

	struct j9cmdlineOptions * args = arg;
	int argc = args->argc;
	char **argv = args->argv;

	char logBuf[1024];

    SERVICE_TABLE_ENTRY serviceTableEntry[] =
      {{"MyService",(LPSERVICE_MAIN_FUNCTION)ServiceMainFunc}, {NULL, NULL}};

	// Initial redirect of stdout/stderr for testing
	// RedirectStdoutStderr("c:\\MyService\\log.txt");
	
	// Process the arguments to the main function (not the service)
	ProcessMainArgs(argc, argv);

    if (help) {
		Usage();
		return -1;
	}

	if (install) {
		WriteLog("About to install service ...\n");
		return InstallService();
	}
	
	if (uninstall) {
		WriteLog("About to uninstall service ...\n");
		return UninstallService();
	}

    if (start) {
		WriteLog("About to start service ...\n");
		return StartTheService(argc, argv);
	}

    if (stop) {
		WriteLog("About to stop service ...\n");
		return StopTheService();
	}

    WriteLog("About to call StartServiceCtrlDispatcher ...\n");

	if (!StartServiceCtrlDispatcher(serviceTableEntry)) {
        sprintf(logBuf, "Error returned from StartServiceCtrlDispatcher: %u.\n", GetLastError());
		WriteLog(logBuf);
        exit(1);
    } else {
        WriteLog("Returned from StartServiceCtrlDispatcher\n");
    }

	sprintf(logBuf, "End of service %s\n", serviceName);
	WriteLog(logBuf);

    fclose(newStdout);
    fclose(newStderr);

	return 0;
}

/*
 * Install the service
 */
UDATA InstallService(void) {
	char logBuf[1024];
	SC_HANDLE hSCM;
	SC_HANDLE hService;

	if (NULL == (hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE))) {
		sprintf(logBuf, "Error returned from OpenSCManager: %u.\n", GetLastError());
		WriteLog(logBuf);
		return 1;
	} 
	
	hService = CreateService(hSCM, 
								install, 
								srvdesc, 
								SERVICE_ALL_ACCESS,
								SERVICE_WIN32_OWN_PROCESS,
								SERVICE_DEMAND_START,
								SERVICE_ERROR_IGNORE,
								exepath,
								NULL,
								NULL,
								NULL,
								NULL,
								NULL);
	
	if (NULL == hService) {
		sprintf(logBuf, "Error returned from CreateService: %u.\n", GetLastError());
		WriteLog(logBuf);
		return 1;
	}
	
	CloseServiceHandle(hService);
	CloseServiceHandle(hSCM);

    sprintf(logBuf, "Service %s has been successfully installed.\n", install);
	WriteLog(logBuf);
	
	return 0;
}

/*
 * Uninstall the service
 */
UDATA UninstallService(void) {
	char logBuf[1024];
	SC_HANDLE hSCM;
	SC_HANDLE hService;
	
	if (NULL == (hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS))) {
		sprintf(logBuf, "Error returned from OpenSCManager: %u.\n", GetLastError());
		WriteLog(logBuf);
		return 1;
	} 
	
	if (NULL == (hService = OpenService(hSCM, uninstall, SERVICE_ALL_ACCESS))) {
		sprintf(logBuf, "Error returned from OpenService: %u.\n", GetLastError());
		WriteLog(logBuf);
		return 1;
	}
	
	if ( !DeleteService(hService)) {
		sprintf(logBuf, "Error returned from DeleteService: %u.\n", GetLastError());
		WriteLog(logBuf);
		return 1;
	}
		
	CloseServiceHandle(hService);
	CloseServiceHandle(hSCM);

    sprintf(logBuf, "Service %s has been successfully uninstalled.\n", uninstall);
	WriteLog(logBuf);

	return 0;
}

/*
 * Start the service
 */
UDATA StartTheService(int argc, char *argv[]) {
	char logBuf[1024];
	SC_HANDLE hSCM;
	SC_HANDLE hService;
    BOOL rc;
    int serviceArgc = 0;
    char **serviceArgv = NULL;
    int i;

    /*
     * argc - 3 allows for the following initial args:
     *    argv[0]:  exe name
     *    argv[1]: -start
     *    argv[2]: service name
    */
    serviceArgc = argc - 3;
    if (serviceArgc > 0) {
        serviceArgv = (char **)malloc( serviceArgc * sizeof(char*) );
        for (i = 0; i < serviceArgc; i++) {
            serviceArgv[i] = argv[i + 3];
        }
    }

    WriteLog("About to start service with following parameters ...\n");
    for (i = 0; i < serviceArgc; i++) {
        sprintf(logBuf, "   serviceArgv[%d]: %s\n", i, serviceArgv[i]);
        WriteLog(logBuf);
    }
    
	if (NULL == (hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE))) {
		sprintf(logBuf, "Error returned from OpenSCManager: %u.\n", GetLastError());
		WriteLog(logBuf);
		return 1;
	} 

    if (NULL == (hService = OpenService(hSCM, start, SERVICE_ALL_ACCESS))) {
		sprintf(logBuf, "Error returned from OpenService: %u.\n", GetLastError());
		WriteLog(logBuf);
		return 1;
	}

	rc = StartService(hService, serviceArgc, serviceArgv);
	
	if (!rc) {
		sprintf(logBuf, "Error returned from StartService: %u.\n", GetLastError());
		WriteLog(logBuf);
		return 1;
	}
	
	CloseServiceHandle(hService);
	CloseServiceHandle(hSCM);

    sprintf(logBuf, "Service %s has been successfully started.\n", start);
	WriteLog(logBuf);
	
	return 0;
}

/*
 * Stop the service
 */
UDATA StopTheService(void) {
	char logBuf[1024];
	SC_HANDLE hSCM;
	SC_HANDLE hService;
    SERVICE_STATUS serviceStatus;
    BOOL rc;
	
	if (NULL == (hSCM = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE))) {
		sprintf(logBuf, "Error returned from OpenSCManager: %u.\n", GetLastError());
		WriteLog(logBuf);
		return 1;
	} 

    if (NULL == (hService = OpenService(hSCM, stop, SERVICE_ALL_ACCESS))) {
		sprintf(logBuf, "Error returned from OpenService: %u.\n", GetLastError());
		WriteLog(logBuf);
		return 1;
	}

	rc = ControlService(hService, SERVICE_CONTROL_STOP, &serviceStatus);
	
	if (!rc) {
		sprintf(logBuf, "Error returned from ControlService: %u.\n", GetLastError());
		WriteLog(logBuf);
		return 1;
	}
	
	CloseServiceHandle(hService);
	CloseServiceHandle(hSCM);

    sprintf(logBuf, "Service %s has been successfully stopped.\n", stop);
	WriteLog(logBuf);
	
	return 0;
}

/*
 * Service main function
 * Register the service
 * Record service as start pending
 * Create thread to run the JVM
 * Record service as started
 */
void WINAPI ServiceMainFunc(DWORD argc, LPTSTR *argv) {

	char logBuf[1024];

   DWORD threadId;

   // Get service name.
   serviceName = argv[0];

   // Process arguments
	//WriteLog("Processing arguments ...\n");
	ProcessArgs(argc, argv);

	sprintf(logBuf, "Starting service %s\n", serviceName);
	WriteLog(logBuf);

   // Register the service ctrl handler.
   servStatHandle = RegisterServiceCtrlHandler(serviceName, (LPHANDLER_FUNCTION)ServiceControlFunc);

   // Tell the service control manager we are in the process of starting up
   UpdateStatus(SERVICE_START_PENDING, 0, 3000);
   WriteLog("Service Status, SERVICE_START_PENDING\n");

   // Create the stop event
   stopEventHandle = CreateEvent(NULL, TRUE, FALSE, NULL);
   if (stopEventHandle == NULL) {
      ErrorHandler("CreateEvent");
   }

   // Create the thread that will start the JVM
   threadHandle = CreateThread(NULL, 0, JavaThread, (LPVOID)1, 0, &threadId);
   if (threadHandle == INVALID_HANDLE_VALUE) {
       ErrorHandler("CreateThread");
   }

   // Now tell the service control manager we have started
   UpdateStatus(SERVICE_RUNNING, 0, 0);
   WriteLog("Service Status, SERVICE_RUNNING\n");


   // Wait for stop event to be signalled
   while(WaitForSingleObject(stopEventHandle, 1000) != WAIT_OBJECT_0)
   {
   }

   // Tell the Java Application to stop processing
   StopApplProcessing();

   // Wait for Java thread to complete
   WaitForSingleObject(threadHandle, 5000);

   // Close the event handle
   if (!CloseHandle(stopEventHandle)) {
      ErrorHandler("CloseHandle");
   }
   // Close the thread handle (heavy handed way of stopping the JVM)
   if (!CloseHandle(threadHandle)) {
      ErrorHandler("CloseHandle");
   }

   // Stop the service.
   WriteLog("Service Status, SERVICE_STOPPED\n");
   UpdateStatus(SERVICE_STOPPED, NO_ERROR, 0);
}

/*
 * Service control manager request handler
 */
void WINAPI ServiceControlFunc(DWORD controlCode) {

	char logBuf[1024];
	LPSTR stateDesc;
    DWORD state = SERVICE_RUNNING;

    switch(controlCode) {
        case SERVICE_CONTROL_STOP:
            state = SERVICE_STOP_PENDING;
			stateDesc = "SERVICE_CONTROL_STOP";
            break;

        case SERVICE_CONTROL_SHUTDOWN:
            state = SERVICE_STOP_PENDING;
			stateDesc = "SERVICE_CONTROL_SHUTDOWN";
            break;

        case SERVICE_CONTROL_INTERROGATE:
			stateDesc = "SERVICE_CONTROL_INTERROGATE";
            break;

        default:
			stateDesc = "Unknown";
            break;
   }

   // Tell the Service Control Manager of the update
   UpdateStatus(state, NO_ERROR, 0);
   sprintf(logBuf, "Service request %s processed\n", stateDesc);
   WriteLog(logBuf);

   // If the service control manager is telling us to stop, 
   // signal the stop event
   if ((controlCode == SERVICE_CONTROL_STOP) ||
	   (controlCode == SERVICE_CONTROL_SHUTDOWN)) {
      if (!SetEvent(stopEventHandle))
         ErrorHandler("SetEvent");
      else {
         WriteLog("Stop or Shutdown control request received, stopEventHandle signaled\n");
      }
   }
}

/*
 * Redirect stdout and stderr to a file and set no buffering
 */
void RedirectStdoutStderr(char *filepath) {

	char logBuf[1024];

    if (NULL == (newStdout = freopen(filepath, "a", stdout))) {
        sprintf(logBuf, "Cannot reopen stdout file: %u.\n", GetLastError());
		WriteLog(logBuf);
	    exit(1);
    } 

    if (NULL == (newStderr = freopen(filepath, "a", stderr))) {
        sprintf(logBuf, "Cannot reopen stdout file: %u.\n", GetLastError());
		WriteLog(logBuf);
		exit(1);
    }

    if (setvbuf(stdout, NULL, _IONBF, 0) != 0) {
        sprintf(logBuf, "Error from setvbuf for stdout %u\n", GetLastError());
		WriteLog(logBuf);
    }
    if (setvbuf(stderr, NULL, _IONBF, 0) != 0) {
        sprintf(logBuf, "Error from setvbuf for stderr %u\n", GetLastError());
		WriteLog(logBuf);
    }
    if (setvbuf(newStdout, NULL, _IONBF, 0) != 0) {
        sprintf(logBuf, "Error from setvbuf for newStdout %u\n", GetLastError());
		WriteLog(logBuf);
    }
    if (setvbuf(newStderr, NULL, _IONBF, 0) != 0) {
        sprintf(logBuf, "Error from setvbuf for newStderr %u\n", GetLastError());
		WriteLog(logBuf);
    }

}

/*
 * Write messages to error log
 */
void WriteLog(LPTSTR logString) {
	time_t timeNow;
	struct tm *tmTimeNow; 
	char timeStr[50];
	time(&timeNow);
	tmTimeNow = localtime(&timeNow);
	strftime(timeStr, 50, "%Y %b %d %H:%M:%S", tmTimeNow);
	printf("%s: %s", timeStr, logString);
}

/*
 * Process arguments to the service
 */
void ProcessArgs(int argc, char *argv[]) {
	int i;
	char *parg;
	int optionRecognised;
    int argRequired;
	char logBuf[1024];
	char unrecOpts[1024] = "";

	//sprintf(logBuf, "ProcessArgs: argument count = %d\n", argc);
	//WriteLog(logBuf);

	// Start at arg 1, not 0, to skip service name
	for(i = 1; i < argc; i++) {
		parg = argv[i];
		optionRecognised = 0;
        argRequired = 1;
		//sprintf(logBuf, "Processing argument %s\n", parg);
		//WriteLog(logBuf);
		if (*parg == '-' || *parg == '/') {
			parg++;
			if (!strcmp("logpath", parg)) {
				PROCESSOPTION(logpath);
				if (NULL != logpath) {
					RedirectStdoutStderr(logpath);
				}
			}
			if (!strcmp("jvmpath", parg)) {
				PROCESSOPTION(jvmpath);
			}
			if (!strcmp("classpath", parg)) {
				PROCESSOPTION(classpath);
			}
			if (!strcmp("classname", parg)) {
				PROCESSOPTION(classname);
			}
			if (!strcmp("stopmethod", parg)) {
				PROCESSOPTION(stopmethod);
			}
			if (!strcmp("cachename", parg)) {
				PROCESSOPTION(cachename);
			}
			if (!strcmp("tracepath", parg)) {
				PROCESSOPTION(tracepath);
			}
			//if (!optionRecognised) {
			//	sprintf(logBuf, "Option %s not recognised\n", argv[i]);
			//	WriteLog(logBuf);
			//	//VARLOG("Option %s not recognised\n", argv[i]);
			//}
		}
		if (!optionRecognised) {
			if (strcmp(unrecOpts, "")) {
				strncat(unrecOpts, ", ", (sizeof(unrecOpts)-strlen(unrecOpts)-1));
			}
			strncat(unrecOpts, argv[i], (sizeof(unrecOpts)-strlen(unrecOpts)-1));
		}
	}

	if (NULL == logpath || NULL == jvmpath || NULL == classpath || NULL == classname) {
		Usage();
		exit(1);
	}

	if (logpath) {
		sprintf(logBuf, "Using logpath = %s\n", logpath);
		WriteLog(logBuf);
	}
	if (jvmpath) {
		sprintf(logBuf, "Using jvmpath = %s\n", jvmpath);
		WriteLog(logBuf);
	}
	if (classpath) {
		sprintf(logBuf, "Using classpath = %s\n", classpath);
		WriteLog(logBuf);
	}
	if (classname) {
		sprintf(logBuf, "Using classname = %s\n", classname);
		WriteLog(logBuf);
	}
	if (stopmethod) {
		sprintf(logBuf, "Using stopmethod = %s\n", stopmethod);
		WriteLog(logBuf);
	} else {
		WriteLog("Stopmethod not specified, JVM thread will be killed at end of service\n");
	}
	if (cachename) {
		sprintf(logBuf, "Using cachename = %s\n", cachename);
		WriteLog(logBuf);
	} else {
		WriteLog("Cachename not specified, shared classes will not be used by the service\n");
	}
	if (tracepath) {
		sprintf(logBuf, "Using tracepath = %s\n", tracepath);
		WriteLog(logBuf);
	}
	if (noArgumentFor) {
		sprintf(logBuf, "No value supplied for %s argument\n", noArgumentFor);
		WriteLog(logBuf);
	}
	if (strcmp(unrecOpts, "")) {
		strncat(unrecOpts, "\n", (sizeof(unrecOpts)-strlen(unrecOpts)-1));
		WriteLog("Unrecognised arguments:\n");
		WriteLog(unrecOpts);
	}
}

/*
 * Process arguments to the main function 
 * 
 * Note:
 *    This is NOT the same as args passed to the service
 *    main function.  Those are passed to the Service Main
 *    Function by the SCM.  The arguments processed here are
 *    the command line arguments to the main function.
 */
void ProcessMainArgs(int argc, char *argv[]) {
	int i;
	char *parg;
	int optionRecognised;
    int argRequired;
	char logBuf[1024];
	char unrecOpts[1024] = "";

	//sprintf(logBuf, "ProcessArgs: argument count = %d\n", argc);
	//WriteLog(logBuf);

	for(i = 1; i < argc; i++) {
		parg = argv[i];
		optionRecognised = 0;
        argRequired = 1;
		//sprintf(logBuf, "Processing argument %s\n", parg);
		//WriteLog(logBuf);
		if (*parg == '-' || *parg == '/') {
			parg++;
			if (!strcmp("help", parg)) {
                argRequired = 0;
				PROCESSOPTION(help);
			}
            if (!strcmp("install", parg)) {
				PROCESSOPTION(install);
			}
			if (!strcmp("exepath", parg)) {
				PROCESSOPTION(exepath);
			}
			if (!strcmp("srvdesc", parg)) {
				PROCESSOPTION(srvdesc);
			}
			if (!strcmp("uninstall", parg)) {
				PROCESSOPTION(uninstall);
			}
            if (!strcmp("start", parg)) {
				PROCESSOPTION(start);
			}
            if (!strcmp("stop", parg)) {
				PROCESSOPTION(stop);
			}
		}
		//if (!optionRecognised) {
		//	if (strcmp(unrecOpts, "")) {
		//		strncat(unrecOpts, ", ", (sizeof(unrecOpts)-strlen(unrecOpts)-1));
		//	}
		//	strncat(unrecOpts, argv[i], (sizeof(unrecOpts)-strlen(unrecOpts)-1));
		//}
	}

	if (NULL != install && (NULL == exepath || NULL == srvdesc)) {
		Usage();
		exit(1);
	}

    if (help) {
		WriteLog("Usage help requested\n");
	}
	if (install) {
		sprintf(logBuf, "Installing service = %s\n", install);
		WriteLog(logBuf);
	}
	if (exepath) {
		sprintf(logBuf, "Using executable path = %s\n", exepath);
		WriteLog(logBuf);
	}
	if (exepath) {
		sprintf(logBuf, "Using servicve description = %s\n", srvdesc);
		WriteLog(logBuf);
	}
	if (uninstall) {
		sprintf(logBuf, "Uninstalling service = %s\n", uninstall);
		WriteLog(logBuf);
	}
    if (start) {
		sprintf(logBuf, "Starting service = %s\n", start);
		WriteLog(logBuf);
	}
    if (stop) {
		sprintf(logBuf, "Stopping service = %s\n", stop);
		WriteLog(logBuf);
	}
	if (noArgumentFor) {
		sprintf(logBuf, "No value supplied for %s argument\n", noArgumentFor);
		WriteLog(logBuf);
	}
	if (strcmp(unrecOpts, "")) {
		strncat(unrecOpts, "\n", (sizeof(unrecOpts)-strlen(unrecOpts)-1));
		WriteLog("Unrecognised arguments:\n");
		WriteLog(unrecOpts);
	}
}

/*
 * Get next argument as value for current argument
 */
char* OptionArg(int *i, int argc, char *argv[], char *argname) {
	char *result = NULL;
	int index = *i;
	if ((index+1) < argc) {
		result = argv[++index];
		*i = index;
	} else {
		noArgumentFor = argname;
	}
	return result;
}

/*
 * Print usage details
 */
void Usage(void) {
    WriteLog("Usage:\n");
    WriteLog("       servicetest -install serviceName -exepath <path to servicetest.exe> -srvdesc \"service description\"\n");
    WriteLog("       servicetest -uninstall serviceName\n");
	WriteLog("       servicetest -start serviceName requiredServiceOptions [otherOptions]\n");
    WriteLog("       servicetest -stop serviceName\n");
    WriteLog("\n");
	WriteLog("requiredServiceOptions:\n");
	WriteLog("   -logpath <path to log file>\n");
	WriteLog("         if not specified, no output will be produced and\n");
	WriteLog("         the service will stop with Windows error message 1053\n");
	WriteLog("   -jvmpath <path to jvm.dll>\n");
	WriteLog("   -classpath <path to class to be run>\n");
	WriteLog("   -classname <class name to be run>\n");
	WriteLog("otherOptions:\n");
	WriteLog("   -cachename <shared classes cache name>\n");
	WriteLog("         required if shared classes are to be used\n");
	WriteLog("   -stopmethod <method to be called to stop processing>\n");
	WriteLog("         if specified this method will be called to request\n");
	WriteLog("         the class to stop processing, otherwise the JVM\n");
	WriteLog("         will simply be closed when the stop request is received\n");
	WriteLog("   -tracepath <path to trace file>\n");
	WriteLog("         turns on JVM trace if required\n");
}

/*
 * Update the service's status using SetServiceStatus
 */

void UpdateStatus(DWORD currentState, DWORD win32ExitCode, DWORD waitHint) {

    SERVICE_STATUS servStat;  // Current status of the service.

    // Don't accept control requests while service is starting
	if (currentState == SERVICE_START_PENDING) {
        servStat.dwControlsAccepted = 0;
	} else {
        servStat.dwControlsAccepted = SERVICE_ACCEPT_STOP|SERVICE_ACCEPT_SHUTDOWN;
	}

    // Set up servStat structure.
    servStat.dwServiceType             = SERVICE_WIN32_OWN_PROCESS;
    servStat.dwServiceSpecificExitCode = 0;
    servStat.dwCurrentState            = currentState;
    servStat.dwWin32ExitCode           = win32ExitCode;
    servStat.dwCheckPoint              = 0;
    servStat.dwWaitHint                = waitHint;

    // Update the status
	if (!SetServiceStatus(servStatHandle, &servStat)) {
        ErrorHandler("SetServiceStatus");
	}
}

/*
 * Error handling function
 */

void ErrorHandler(LPTSTR errorFunc)
{

    LPVOID errorMsg;
    char logBuf[1024];
    DWORD waitRes;
	DWORD lastErrorCode;

	lastErrorCode = GetLastError();

    sprintf(logBuf,"Error from function %s, error code = %d\n", errorFunc, lastErrorCode);
	WriteLog(logBuf);

	// Get the error message string
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, lastErrorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&errorMsg, 0, NULL);

    sprintf(logBuf,"Error message = %s\n", (char *)errorMsg);
    WriteLog(logBuf);

    // Free the buffer from FormatMessage
    LocalFree(errorMsg);

    // Signal stop event to stop JVM thread
    SetEvent(stopEventHandle);

    // Wait for the thread to stop.
    if ((waitRes = WaitForSingleObject(threadHandle,1000)) == WAIT_OBJECT_0) {
          // Drop through
	} else {
        if ((waitRes == WAIT_FAILED)|| (waitRes == WAIT_ABANDONED)) {
            // Wait failed, may want to add something here
        } else {
            UpdateStatus(SERVICE_STOP_PENDING, 0, 5000);
        }
	}

    // Record service as stopped
    UpdateStatus(SERVICE_STOPPED, lastErrorCode, 0);
}

/*
 * Thread procedure for java thread.
 */
DWORD WINAPI JavaThread(LPVOID parm)
{

    char logBuf[1024];

	HINSTANCE jvmDll;
	typedef jint (*JNICREATEJAVAVM)(JavaVM **, void **, void *);
	JNICREATEJAVAVM jniCreateJavaVM;
	DWORD lastError = 0;

    JNIEnv *env;
    jint rc;
    jclass classid;
    jmethodID methodid;
    jstring javaStr;
    jclass stringClass;
    jobjectArray args;
    JavaVMInitArgs vm_args;
    JavaVMOption options[10];

	// Note that in string length calculations below, extra space for null 
	// terminators is not included as the "%s" in the literals will not be
	// present in the final string and so cater for the terminators
	int noOpts = 0;
	char *classpathlit = "-Djava.class.path=%s";
	char *classpathopt;
	char *cachenamelit = "-Xshareclasses:name=%s";
	char *cachenameopt;
	char *tracepathlit = "-Xtrace:maximal=j9prt,maximal=j9shr,output=%s";
	char *tracepathopt;

	jvmDll = LoadLibrary(jvmpath);
	if (NULL == jvmDll) {
		lastError = GetLastError();
		sprintf(logBuf, "Error returned from LoadLibrary(\"%s\"), error code = %u\n", jvmpath, lastError);
		WriteLog(logBuf);
		return 1;
	}

	jniCreateJavaVM = (JNICREATEJAVAVM)GetProcAddress(jvmDll, "JNI_CreateJavaVM");
	if (NULL == jniCreateJavaVM) {
		lastError = GetLastError();
		sprintf(logBuf, "Error returned from GetProcAddress for \"JNI_CreateJavaVM\", error code = %u\n", lastError);
		WriteLog(logBuf);
		return 1;
	}

	// Build classpath option
	sprintf(logBuf, "Building classpathopt from %s and %s option %d\n", classpathlit, classpath, noOpts);
	WriteLog(logBuf);
	classpathopt = (char *)malloc(strlen(classpathlit) + strlen(classpath));
	sprintf(classpathopt, classpathlit, classpath);
    options[noOpts++].optionString = classpathopt;

	if (cachename) {
		sprintf(logBuf, "Building cachenameopt from %s and %s option %d\n", cachenamelit, cachename, noOpts);
		WriteLog(logBuf);
		cachenameopt = (char *)malloc(strlen(cachenamelit) + strlen(cachename));
		sprintf(cachenameopt, cachenamelit, cachename);
		options[noOpts++].optionString = cachenameopt;
	}

	if (tracepath) {
		sprintf(logBuf, "Building tracepathopt from %s and %s option %d\n", tracepathlit, tracepath, noOpts);
		WriteLog(logBuf);
		tracepathopt = (char *)malloc(strlen(tracepathlit) + strlen(tracepath));
		sprintf(tracepathopt, tracepathlit, tracepath);
		options[noOpts++].optionString = tracepathopt;
	}

	vm_args.nOptions = noOpts;
    vm_args.version = 0x00010002;
    vm_args.options = options;
    vm_args.ignoreUnrecognized = JNI_TRUE;
    
	// Create the Java VM 
    WriteLog("Starting Java ...\n");
    rc = (*jniCreateJavaVM)( &jvm, (void**)&env, &vm_args);
    WriteLog("Java Started ...\n");

    if (rc < 0) {
	    sprintf(logBuf, "Create JVM failed, rc = %d\n", rc);
        WriteLog(logBuf);
        return 1;
    }

    classid = (*env)->FindClass(env, classname);
    if (classid == NULL) {
        sprintf(logBuf, "Can't find class %s\n", classname);
		WriteLog(logBuf);
        goto destroy;
    }

    methodid = (*env)->GetStaticMethodID(env, classid, "main", "([Ljava/lang/String;)V");
    if (methodid == NULL) {
        sprintf(logBuf, "Can't find method %s.main\n", classname);
		WriteLog(logBuf);
        goto destroy;
    }

    javaStr = (*env)->NewStringUTF(env, "Argument");
    if (javaStr == NULL) {
        printf( "NewStringUTF failed\n");
        goto destroy;
    }

    stringClass = (*env)->FindClass(env, "java/lang/String");
    if (stringClass == NULL) {
        WriteLog("Find String class failed\n");
        goto destroy;
    }

    args = (*env)->NewObjectArray(env, 1, stringClass, javaStr);
    if (args == NULL) {
        WriteLog("Create NewObjectArray failed\n");
        goto destroy;
    }

    sprintf(logBuf, "Calling %s.main ...\n", classname);
	WriteLog(logBuf);
    (*env)->CallStaticVoidMethod(env, classid, methodid, args);
	WriteLog("Returned from main\n");

destroy:
    WriteLog("Closing Java ...\n");
    if ((*env)->ExceptionOccurred(env)) {
        (*env)->ExceptionDescribe(env);
    }
    (*jvm)->DestroyJavaVM(jvm);
	FreeLibrary(jvmDll);
    WriteLog("Java Ended\n");

	return 0;
}

/*
 * Attach to JVM and call method to stop application processing
 */
void StopApplProcessing(void) {

	char logBuf[1024];

	JNIEnv *env;
	jint rc;
	jclass classid;
	jmethodID methodid;

	if (NULL == jvm) {
		WriteLog("StopApplProcessing: jvm is NULL, returning\n");
		return;
	}

	if (NULL == stopmethod) {
		WriteLog("StopApplProcessing: stopmethod not specified closing jvm thread and returning\n");
		return;
	}

	// Attach to the JVM
	rc = (*jvm)->AttachCurrentThread(jvm, (void**)&env, NULL);
	if (rc != 0) {
		sprintf(logBuf, "Error from AttachCurrentThread, rc = %d\n", rc);
		WriteLog(logBuf);
		return;
	}

	// Find the class
	classid = (*env)->FindClass(env, classname);
    if (classid == NULL) {
        sprintf(logBuf, "Can't find class %s\n", classname);
        WriteLog(logBuf);
        goto cleanup;
    }

	// Find the method
    methodid = (*env)->GetStaticMethodID(env, classid, stopmethod, "()V");
    if (methodid == NULL) {
		sprintf(logBuf, "Can't find method %s in class %s\n", stopmethod, classname);
        WriteLog(logBuf);
        goto cleanup;
    }

	// Call the method
	sprintf(logBuf, "Calling %s.%s() ...\n", classname, stopmethod);
	WriteLog(logBuf);

	(*env)->CallStaticVoidMethod(env, classid, methodid, NULL);

	WriteLog("Returned from call\n");

cleanup:
	// Detach from JVM
	WriteLog("Detaching from JVM\n");

	(*jvm)->DetachCurrentThread(jvm);

	WriteLog("Returned from DetachCurrentThread()\n");

	return;
}


