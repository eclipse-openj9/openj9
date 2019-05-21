/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifdef ENABLE_GPU

#include <stdio.h>
#include <stdarg.h>

#if (defined(LINUX) && defined(TR_TARGET_POWER) && !defined(__GNUC__))
#define __GNUC__ 4
#define XLC_GCC_HACK
#endif

#include "cuda.h"
#include "cuda_runtime.h"
#include "nvvm.h"
#include "nvml.h"

#if defined(XLC_GCC_HACK)
#undef __GNUC__
#undef XLC_GCC_HACK
#endif

#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#if (defined(LINUX) || !defined(TR_TARGET_X86))
#include <sys/time.h>
#include <dlfcn.h>
#include <dirent.h>
#endif

#include "vmaccess.h"
#include "codegen/CodeGenerator.hpp"
#include "env/VMJ9.h"
#include "infra/Monitor.hpp"
#include "runtime/MethodMetaData.h"

#define ERROR_CODE 1
#define NULL_DEVICE_PTR (CUdeviceptr)NULL
#define BAD_DEVICE_PTR (CUdeviceptr)(TR::CodeGenerator::GPUBadDevicePointer)

#define GPU_EXEC_SUCCESS 0
#define GPU_EXEC_FAIL_RECOVERABLE 1
#define GPU_EXEC_FAIL_NOT_RECOVERABLE 2

// returns time stamp in microseconds
// on Linux this is the number of microseconds since epoch
// on Windows this is the number of microseconds since the system booted
static double currentTime()
   {
#if (defined(LINUX) || !defined(TR_TARGET_X86))
   struct timeval now;
   time_t currentTime_s;
   suseconds_t currentTime_us;
   int rc;

   rc = gettimeofday(&now, NULL);
   if(rc != 0) TR_VerboseLog::writeLine(TR_Vlog_GPU, "Error start time");
   currentTime_s = now.tv_sec;
   currentTime_us = now.tv_usec;
   return ((double)currentTime_s)*1000000 + (double)currentTime_us;
#else
   LARGE_INTEGER ticksPerSec;
   LARGE_INTEGER ticksSinceSystemStart;

   QueryPerformanceFrequency(&ticksPerSec);

   if (ticksPerSec.QuadPart == 0)
      {
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "QueryPerformanceFrequency failed.");
      return 0; //Timing function does has failed
      }

   QueryPerformanceCounter(&ticksSinceSystemStart);

   return (double)(ticksSinceSystemStart.QuadPart * 1000000) / ticksPerSec.QuadPart;
#endif
   }


#define checkCudaError(result, error, tracing)  \
   { \
   if (result != cudaSuccess) \
      { \
      if (tracing > 0) TR_VerboseLog::writeLine(TR_Vlog_GPU, "[time.ms=%lld]: CUDA error %d", (int64_t)(currentTime()/1000), result); \
      return error; \
      } \
   }

#define checkNVVMError(result, tracing) \
   { \
   if (result != NVVM_SUCCESS) \
      { \
      if (tracing > 0) TR_VerboseLog::writeLine(TR_Vlog_GPU, "[time.ms=%lld]: NVVM error %d", (int64_t)(currentTime()/1000), result); \
      return NULL; \
      } \
   }

//stringMacro needs to be layered since sometimes it is used on a C macro.
//internalStringMacro by itself won't fully expand a C macro that is given as input.
#define stringMacro(x) internalStringMacro(x)
#define internalStringMacro(x) #x

//NVVM library functions
nvvmResult (*jitNvvmCreateProgram)(nvvmProgram*);
nvvmResult (*jitNvvmAddModuleToProgram)(nvvmProgram, const char*, size_t, const char*);
nvvmResult (*jitNvvmCompileProgram)(nvvmProgram, int, const char**);
nvvmResult (*jitNvvmGetProgramLogSize)(nvvmProgram, size_t*);
nvvmResult (*jitNvvmGetProgramLog)(nvvmProgram, char*);
nvvmResult (*jitNvvmGetCompiledResultSize)(nvvmProgram, size_t*);
nvvmResult (*jitNvvmGetCompiledResult)(nvvmProgram, char*);
nvvmResult (*jitNvvmDestroyProgram)(nvvmProgram*);
nvvmResult (*jitNvvmVersion)(int*, int*);

//CUDA Runtime library functions
cudaError_t (*jitCudaGetDeviceCount)(int*);
cudaError_t (*jitCudaSetDevice)(int);
cudaError_t (*jitCudaFree)(void*);
cudaError_t (*jitCudaGetDeviceProperties)(cudaDeviceProp*, int);
cudaError_t (*jitCudaDeviceSynchronize)();
cudaError_t (*jitCudaStreamCreate)(cudaStream_t*);
cudaError_t (*jitCudaStreamSynchronize)(cudaStream_t);
const char* (*jitCudaGetErrorString)(cudaError_t);


//CUDA Driver library functions
CUresult (*jitCuInit)(unsigned int);
CUresult (*jitCuLinkCreate)(unsigned int, CUjit_option*, void**, CUlinkState*);
CUresult (*jitCuLinkAddData)(CUlinkState, CUjitInputType, void*, size_t, const char*, unsigned int, CUjit_option*, void**);
CUresult (*jitCuLinkComplete)(CUlinkState, void**, size_t*);
CUresult (*jitCuModuleLoadData)(CUmodule*, const void*);
CUresult (*jitCuDeviceGet)(CUdevice*, int);
CUresult (*jitCuModuleLoadDataEx)(CUmodule*, const void*, unsigned int, CUjit_option*, void**);
CUresult (*jitCuModuleGetFunction)(CUfunction*, CUmodule, const char*);
CUresult (*jitCuLinkDestroy)(CUlinkState);
CUresult (*jitCuMemAlloc)(CUdeviceptr*, size_t);
CUresult (*jitCuMemcpyHtoD)(CUdeviceptr, const void*, size_t);
CUresult (*jitCuMemcpyHtoDAsync)(CUdeviceptr, const void*, size_t, CUstream);
CUresult (*jitCuLaunchKernel)(CUfunction, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, CUstream, void**, void**);
CUresult (*jitCuMemcpyDtoH)(void*, CUdeviceptr, size_t);
CUresult (*jitCuMemcpyDtoHAsync)(void*, CUdeviceptr, size_t, CUstream);
CUresult (*jitCuMemFree)(CUdeviceptr);
CUresult (*jitCuModuleUnload)(CUmodule);
CUresult (*jitCuMemHostRegister)(void*, size_t, unsigned int);
CUresult (*jitCuMemHostUnregister)(void*);
CUresult (*jitCuGetErrorString)(CUresult, const char**);

//NVML functions
nvmlReturn_t (*jitNvmlInit)();
nvmlReturn_t (*jitNvmlDeviceGetHandleByIndex)(unsigned int, nvmlDevice_t*);
nvmlReturn_t (*jitNvmlDeviceGetUtilizationRates)(nvmlDevice_t, nvmlUtilization_t*);
nvmlReturn_t (*jitNvmlDeviceGetMemoryInfo)(nvmlDevice_t, nvmlMemory_t*);
char* (*jitNvmlErrorString)(nvmlReturn_t);

#if (defined(LINUX) || !defined(TR_TARGET_X86))
static void saveSignalHandlers(struct sigaction sigactionArray[], bool trace)
   {
   for (int i=1; i<=31; i++)
      {
      if (i==9 || i==19) //No need to save SIGKILL(9) or SIGSTOP(19) signal handlers
         continue;

      if (sigaction(i, NULL, &sigactionArray[i-1]) != 0) //Saving signal handlers to sigactionArray
         {
         if (trace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tsigaction error: Can't read sigaction for signal %d\n", i);
         }
      }
   }

static void restoreSignalHandlers(struct sigaction sigactionArray[], bool trace)
   {
   for (int i=1; i<=31; i++)
      {
      if (i==9 || i==19) //No need to restore SIGKILL(9) or SIGSTOP(19) signal handlers.
         continue;

      if (sigaction(i, &sigactionArray[i-1], NULL) != 0) //Restoring signal handlers from sigactionArray
         {
         if (trace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tsigaction error: Can't set sigaction for signal %d\n", i);
         }
      }
   }
#endif

#define     FILENAMELEN     255
#define     EXTENSIONLEN    16

static void writeDebugFile(const char *ptxSource, void *data, size_t dataSize, char *extension, char *fileoption) 
   {
   int fnpos = 0;
   char filename[FILENAMELEN+EXTENSIONLEN+1];
   if (EXTENSIONLEN < strlen(extension))
      return;

   filename[0] = '\0';
   for (int i = 0; i < strlen(ptxSource); i++)
      {
      if (strncmp(&ptxSource[i], ".entry", 6) == 0)
         {
         i += 6;
         int prevstatus = 0, status = 0; // 0:after .entry, 1:in symbol, 2:after symbol, -1:in /**/, -2:in //
         while (i < strlen(ptxSource))
            {
            if (FILENAMELEN < fnpos)
               {
               filename[fnpos] = '\0';
               i = strlen(ptxSource);
               break;
               }
            char c = ptxSource[i++];
            if (status == -2)
               {
               if (c == '\n') { status = prevstatus; }
               continue;
               }
            if (status == -1) 
               {
               if (strncmp(&ptxSource[i], "*/", 2) == 0) { status = prevstatus; i++; }
               continue;
               }
            if ((status == 0) &&
                ((('a' <= c) && (c <= 'z')) || (('A' <= c) && (c <= 'Z')) ||
                 (c == '_') || (c == '$') || (c == '%')))
               {
               status = 1;
               filename[fnpos++] = c;
               continue;
               }
            else
               {
               if (strncmp(&ptxSource[i], "/*", 2) == 0) { status = -1; i++; }
               if (strncmp(&ptxSource[i], "//", 2) == 0) { status = -2; i++; }
               }
            if (status == 1)
               {
               if ((('a' <= c) && (c <= 'z')) || (('A' <= c) && (c <= 'Z')) ||(('0' <= c) && (c <= '9')) ||
                   (c == '_') || (c == '$'))
                  {
                  filename[fnpos++] = c;
                  continue;
                  }
               else
                  {
                  filename[fnpos] = '\0';
                  i = strlen(ptxSource);
                  break;
                  }
               }
            }
         }
      }

   if (fnpos != 0)
      {
      strcpy(&filename[fnpos], extension);
      ::FILE *fp = fopen(filename, fileoption);
      if (fp != NULL)
         {
         fwrite(data, 1, dataSize, fp);
         fclose(fp);
         }
      }
   }  

//On Windows we need to check if the library or function pointer is null
//Function pointers can not be cast to a void* so currently the check is done outside of the function
static bool checkDlError(int tracing, bool isNull)
{
#if (defined(LINUX) || !defined(TR_TARGET_X86))
   char* errorMessage = dlerror();

   if (errorMessage)
      {
      if (tracing > 0) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tDynamic linking error: %s\n", errorMessage);
      return true;
      }
#endif

   if (isNull)
      {
      if (tracing > 0) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tDynamic linking error: null pointer while loading shared library\n");
      return true;
      }

   return false;
}

#if (defined(LINUX) || !defined(TR_TARGET_X86))
#define GET_FUNCTION dlsym
#else
#define GET_FUNCTION GetProcAddress
#endif

static bool loadNVVMlibrary(int tracing)
   {
#if (defined(LINUX) || !defined(TR_TARGET_X86))
   dlerror(); //clear preexisting error flags.
   void* libNvvmPointer = dlopen("libnvvm.so", RTLD_LAZY);
#else
   HINSTANCE libNvvmPointer = LoadLibrary("nvvm64_30_0.dll"); //TODO: change this to support future versions of NVVM

   if (!libNvvmPointer)
      libNvvmPointer = LoadLibrary("nvvm64_20_0.dll");

   if (tracing > 0 && !libNvvmPointer)
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tDynamic linking error: Unable to locate NVVM library nvvm64_30_0.dll or nvvm64_20_0.dll\n");
#endif

   if (checkDlError(tracing, !libNvvmPointer)) return false;
   
   jitNvvmCreateProgram = (nvvmResult (*)(nvvmProgram*))GET_FUNCTION(libNvvmPointer, stringMacro(nvvmCreateProgram));
   if (checkDlError(tracing, !jitNvvmCreateProgram)) return false;

   jitNvvmAddModuleToProgram = (nvvmResult (*)(nvvmProgram, const char*, size_t, const char*))GET_FUNCTION(libNvvmPointer, stringMacro(nvvmAddModuleToProgram));
   if (checkDlError(tracing, !jitNvvmAddModuleToProgram)) return false;

   jitNvvmCompileProgram = (nvvmResult (*)(nvvmProgram, int, const char**))GET_FUNCTION(libNvvmPointer, stringMacro(nvvmCompileProgram));
   if (checkDlError(tracing, !jitNvvmCompileProgram)) return false;

   jitNvvmGetProgramLogSize = (nvvmResult (*)(nvvmProgram, size_t*))GET_FUNCTION(libNvvmPointer, stringMacro(nvvmGetProgramLogSize));
   if (checkDlError(tracing, !jitNvvmGetProgramLogSize)) return false;

   jitNvvmGetProgramLog = (nvvmResult (*)(nvvmProgram, char*))GET_FUNCTION(libNvvmPointer, stringMacro(nvvmGetProgramLog));
   if (checkDlError(tracing, !jitNvvmGetProgramLog)) return false;

   jitNvvmGetCompiledResultSize = (nvvmResult (*)(nvvmProgram, size_t*))GET_FUNCTION(libNvvmPointer, stringMacro(nvvmGetCompiledResultSize));
   if (checkDlError(tracing, !jitNvvmGetCompiledResultSize)) return false;

   jitNvvmGetCompiledResult = (nvvmResult (*)(nvvmProgram, char*))GET_FUNCTION(libNvvmPointer, stringMacro(nvvmGetCompiledResult));
   if (checkDlError(tracing, !jitNvvmGetCompiledResult)) return false;

   jitNvvmDestroyProgram = (nvvmResult (*)(nvvmProgram*))GET_FUNCTION(libNvvmPointer, stringMacro(nvvmDestroyProgram));
   if (checkDlError(tracing, !jitNvvmDestroyProgram)) return false;

   jitNvvmVersion = (nvvmResult (*)(int*, int*))GET_FUNCTION(libNvvmPointer, stringMacro(nvvmVersion));
   if (checkDlError(tracing, !jitNvvmVersion)) return false;

   return true;
   }

static bool loadCudaRuntimeLibrary(int tracing)
   {
#if (defined(LINUX) || !defined(TR_TARGET_X86))
   dlerror(); //clear preexisting error flags.
   void* libCudartPointer = dlopen("libcudart.so", RTLD_LAZY);
#else
   HINSTANCE libCudartPointer = LoadLibrary("cudart64_75.dll"); //TODO: change this to support future versions of the runtime library

   if (!libCudartPointer)
      libCudartPointer = LoadLibrary("cudart64_70.dll");

   if (!libCudartPointer)
      libCudartPointer = LoadLibrary("cudart64_65.dll");

   if (!libCudartPointer)
      libCudartPointer = LoadLibrary("cudart64_60.dll");

   if (!libCudartPointer)
      libCudartPointer = LoadLibrary("cudart64_55.dll");

   if (tracing > 0 && !libCudartPointer)
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tDynamic linking error: Unable to locate Cuda Runtime library cudart64_75.dll\n");
#endif
   if (checkDlError(tracing, !libCudartPointer)) return false;

   jitCudaGetDeviceCount = (cudaError_t (*)(int*))GET_FUNCTION(libCudartPointer, stringMacro(cudaGetDeviceCount));
   if (checkDlError(tracing, !jitCudaGetDeviceCount)) return false;

   jitCudaSetDevice = (cudaError_t (*)(int))GET_FUNCTION(libCudartPointer, stringMacro(cudaSetDevice));
   if (checkDlError(tracing, !jitCudaSetDevice)) return false;

   jitCudaFree = (cudaError_t (*)(void*))GET_FUNCTION(libCudartPointer, stringMacro(cudaFree));
   if (checkDlError(tracing, !jitCudaFree)) return false;

   jitCudaGetDeviceProperties = (cudaError_t (*)(cudaDeviceProp*, int))GET_FUNCTION(libCudartPointer, stringMacro(cudaGetDeviceProperties));
   if (checkDlError(tracing, !jitCudaGetDeviceProperties)) return false;

   jitCudaDeviceSynchronize = (cudaError_t (*)())GET_FUNCTION(libCudartPointer, stringMacro(cudaDeviceSynchronize));
   if (checkDlError(tracing, !jitCudaDeviceSynchronize)) return false;

   jitCudaStreamCreate = (cudaError_t (*)(cudaStream_t*))GET_FUNCTION(libCudartPointer, stringMacro(cudaStreamCreate));
   if (checkDlError(tracing, !jitCudaStreamCreate)) return false;

   jitCudaStreamSynchronize = (cudaError_t (*)(cudaStream_t))GET_FUNCTION(libCudartPointer, stringMacro(cudaStreamSynchronize));
   if (checkDlError(tracing, !jitCudaStreamSynchronize)) return false;

   jitCudaGetErrorString = (const char* (*)(cudaError_t))GET_FUNCTION(libCudartPointer, stringMacro(cudaGetErrorString));
   if (checkDlError(tracing, !jitCudaGetErrorString)) return false;
   
   return true;
   }

static bool loadCudaLibrary(int tracing)
   {
#if (defined(LINUX) || !defined(TR_TARGET_X86))
   dlerror(); //clear preexisting error flags.
   void* libCudaPointer = dlopen("libcuda.so", RTLD_LAZY);
#else
   HINSTANCE libCudaPointer = LoadLibrary("nvcuda.dll");

   if (tracing > 0 && !libCudaPointer)
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tDynamic linking error: Unable to locate Cuda Driver library nvcuda.dll\n");
#endif
   if (checkDlError(tracing, !libCudaPointer)) return false;

   jitCuInit = (CUresult (*)(unsigned int))GET_FUNCTION(libCudaPointer, stringMacro(cuInit));
   if (checkDlError(tracing, !jitCuInit)) return false;

   jitCuLinkCreate = (CUresult (*)(unsigned int, CUjit_option*, void**, CUlinkState*))GET_FUNCTION(libCudaPointer, stringMacro(cuLinkCreate));
   if (checkDlError(tracing, !jitCuLinkCreate)) return false;

   jitCuLinkAddData = (CUresult (*)(CUlinkState, CUjitInputType, void*, size_t, const char*, unsigned int, CUjit_option*, void**))GET_FUNCTION(libCudaPointer, stringMacro(cuLinkAddData));
   if (checkDlError(tracing, !jitCuLinkAddData)) return false;

   jitCuLinkComplete = (CUresult (*)(CUlinkState, void**, size_t*))GET_FUNCTION(libCudaPointer, stringMacro(cuLinkComplete));
   if (checkDlError(tracing, !jitCuLinkComplete)) return false;

   jitCuModuleLoadData = (CUresult (*)(CUmodule*, const void*))GET_FUNCTION(libCudaPointer, stringMacro(cuModuleLoadData));
   if (checkDlError(tracing, !jitCuModuleLoadData)) return false;

   jitCuDeviceGet = (CUresult (*)(CUdevice*, int))GET_FUNCTION(libCudaPointer, stringMacro(cuDeviceGet));
   if (checkDlError(tracing, !jitCuDeviceGet)) return false;

   jitCuModuleLoadDataEx = (CUresult (*)(CUmodule*, const void*, unsigned int, CUjit_option*, void**))GET_FUNCTION(libCudaPointer, stringMacro(cuModuleLoadDataEx));
   if (checkDlError(tracing, !jitCuModuleLoadDataEx)) return false;

   jitCuModuleGetFunction = (CUresult (*)(CUfunction*, CUmodule, const char*))GET_FUNCTION(libCudaPointer, stringMacro(cuModuleGetFunction));
   if (checkDlError(tracing, !jitCuModuleGetFunction)) return false;

   jitCuLinkDestroy = (CUresult (*)(CUlinkState))GET_FUNCTION(libCudaPointer, stringMacro(cuLinkDestroy));
   if (checkDlError(tracing, !jitCuLinkDestroy)) return false;

   jitCuMemAlloc = (CUresult (*)(CUdeviceptr*, size_t))GET_FUNCTION(libCudaPointer, stringMacro(cuMemAlloc));
   if (checkDlError(tracing, !jitCuMemAlloc)) return false;

   jitCuMemcpyHtoD = (CUresult (*)(CUdeviceptr, const void*, size_t))GET_FUNCTION(libCudaPointer, stringMacro(cuMemcpyHtoD));
   if (checkDlError(tracing, !jitCuMemcpyHtoD)) return false;

   jitCuMemcpyHtoDAsync = (CUresult (*)(CUdeviceptr, const void*, size_t, CUstream))GET_FUNCTION(libCudaPointer, stringMacro(cuMemcpyHtoDAsync));
   if (checkDlError(tracing, !jitCuMemcpyHtoDAsync)) return false;

   jitCuLaunchKernel = (CUresult (*)(CUfunction, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, unsigned int, CUstream, void**, void**))GET_FUNCTION(libCudaPointer, stringMacro(cuLaunchKernel));
   if (checkDlError(tracing, !jitCuLaunchKernel)) return false;

   jitCuMemcpyDtoH = (CUresult (*)(void*, CUdeviceptr, size_t))GET_FUNCTION(libCudaPointer, stringMacro(cuMemcpyDtoH));
   if (checkDlError(tracing, !jitCuMemcpyDtoH)) return false;

   jitCuMemcpyDtoHAsync = (CUresult (*)(void*, CUdeviceptr, size_t, CUstream))GET_FUNCTION(libCudaPointer, stringMacro(cuMemcpyDtoHAsync));
   if (checkDlError(tracing, !jitCuMemcpyDtoHAsync)) return false;

   jitCuMemFree = (CUresult (*)(CUdeviceptr))GET_FUNCTION(libCudaPointer, stringMacro(cuMemFree));
   if (checkDlError(tracing, !jitCuMemFree)) return false;

   jitCuModuleUnload = (CUresult (*)(CUmodule))GET_FUNCTION(libCudaPointer, stringMacro(cuModuleUnload));
   if (checkDlError(tracing, !jitCuModuleUnload)) return false;

   jitCuMemHostRegister = (CUresult (*)(void*, size_t, unsigned int))GET_FUNCTION(libCudaPointer, stringMacro(cuMemHostRegister));
   if (checkDlError(tracing, !jitCuMemHostRegister)) return false;

   jitCuMemHostUnregister = (CUresult (*)(void*))GET_FUNCTION(libCudaPointer, stringMacro(cuMemHostUnregister));
   if (checkDlError(tracing, !jitCuMemHostUnregister)) return false;

   jitCuGetErrorString = (CUresult (*)(CUresult, const char**))GET_FUNCTION(libCudaPointer, stringMacro(cuGetErrorString));
   if (checkDlError(tracing, !jitCuGetErrorString)) return false;

   return true;
   }

static bool isNVMLavailable = false;
static bool enableMultiGPU = true;
#define DIRNAMES_LEN    5

static bool loadNVMLLibrary(int tracing)
   {
#if (defined(LINUX) || !defined(TR_TARGET_X86))
   dlerror(); //clear preexisting error flags.
   
   char path[PATH_MAX];
   int path_len;
   void* libNvmlPointer;
   bool found = false;
   char *dirnames[DIRNAMES_LEN] = {"",
                                   "/usr/lib64/", "/usr/lib64/nvidia/",
                                   "/usr/lib/", "/usr/lib/nvidia/"};
   for (int i = 0; i < DIRNAMES_LEN; i++)
      {
      path_len = snprintf(path, PATH_MAX, "%slibnvidia-ml.so", dirnames[i]);
      libNvmlPointer = dlopen(path, RTLD_LAZY);
      if (!checkDlError(tracing, !libNvmlPointer))
         {
         found = true;
         if (tracing > 1) TR_VerboseLog::writeLine(TR_Vlog_GPU, "NVML path to load %s", path);
         break;
         }
      }
   
   if (!found)
      {
      char *dirname = "/usr/lib";
      DIR *dir = opendir(dirname);
      if (dir != NULL)
         {
         struct dirent *ent = readdir(dir);
         while (ent)
            {
            path_len = snprintf(path, PATH_MAX, "%s/%s/libnvidia-ml.so", dirname, ent->d_name);
            libNvmlPointer = dlopen(path, RTLD_LAZY);
            if (!checkDlError(tracing, !libNvmlPointer))
               {
               found = true;
               if (tracing > 1) TR_VerboseLog::writeLine(TR_Vlog_GPU, "NVML path to load %s", path);
               break;
               }
            ent = readdir(dir);
            }
         }
      }


   if (!found)
      return false;
#else
   HINSTANCE libNvmlPointer = LoadLibrary("nvml.dll");

   if (tracing > 0 && !libNvmlPointer)
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tDynamic linking error: Unable to locate NVML library nvml.dll\n");

   checkDlError(tracing, !libNvmlPointer);
#endif
   if (!libNvmlPointer) return false;

   jitNvmlInit = (nvmlReturn_t (*)())GET_FUNCTION(libNvmlPointer, stringMacro(nvmlInit));
   if (checkDlError(tracing, !jitNvmlInit)) return false;
   
   jitNvmlDeviceGetHandleByIndex = (nvmlReturn_t (*)(unsigned int, nvmlDevice_t*))GET_FUNCTION(libNvmlPointer, stringMacro(nvmlDeviceGetHandleByIndex));
   if (checkDlError(tracing, !jitNvmlDeviceGetHandleByIndex)) return false;

   jitNvmlDeviceGetUtilizationRates = (nvmlReturn_t (*)(nvmlDevice_t, nvmlUtilization_t*))GET_FUNCTION(libNvmlPointer, stringMacro(nvmlDeviceGetUtilizationRates));
   if (checkDlError(tracing, !jitNvmlDeviceGetUtilizationRates)) return false;

   jitNvmlDeviceGetMemoryInfo = (nvmlReturn_t (*)(nvmlDevice_t, nvmlMemory_t*))GET_FUNCTION(libNvmlPointer, stringMacro(nvmlDeviceGetMemoryInfo));
   if (checkDlError(tracing, !jitNvmlDeviceGetMemoryInfo)) return false;

   jitNvmlErrorString = (char* (*)(nvmlReturn_t))GET_FUNCTION(libNvmlPointer, stringMacro(nvmlErrorString));
   if (checkDlError(tracing, !jitNvmlErrorString)) return false;

   nvmlReturn_t result = jitNvmlInit();
   if (result == NVML_SUCCESS)
      {
      isNVMLavailable = true;
      return true;
      }
   else
      {
      if (tracing > 0) TR_VerboseLog::writeLine(TR_Vlog_GPU, "Failed to initialize NVML: %s", jitNvmlErrorString(result));
      return false;
      }
   }

#define UNKNOWN_CC      256
#define UNKNOWN_CC_MAJORMINOR      ((UNKNOWN_CC << 16) | UNKNOWN_CC)
#define MAX_DEVICE_NUM  8        //Modify initialization of deviceCCs array if you change this value

//This only supports device ids in the range of [0,MAX_DEVICE_NUM-1]. Compilation fails for ids above MAX_DEVICE_NUM-1.
static int deviceCCs[MAX_DEVICE_NUM] = {
           UNKNOWN_CC_MAJORMINOR, UNKNOWN_CC_MAJORMINOR, UNKNOWN_CC_MAJORMINOR, UNKNOWN_CC_MAJORMINOR,
           UNKNOWN_CC_MAJORMINOR, UNKNOWN_CC_MAJORMINOR, UNKNOWN_CC_MAJORMINOR, UNKNOWN_CC_MAJORMINOR};

int getGpuDeviceCount(TR::PersistentInfo * persistentInfo, int tracing)
   {
   static int nDevices = 0;
   static bool initialized = false;

   //If everything has been initialized, we should already have the device count (or 0 for the failing case) so we just return it
   if (initialized || !persistentInfo)
      {
      return nDevices;
      }

   //get the lock
   persistentInfo->getGpuInitializationMonitor()->enter();

   //If everything has been initialized, we should already have the device count (or 0 for the failing case) so we just return it
   if (initialized)
      {
      persistentInfo->getGpuInitializationMonitor()->exit(); //release the lock
      return nDevices;
      }

   //Attempt to load the NVVM, CUDA Driver and CUDA Runtime libraries
   if (!loadNVVMlibrary(tracing) || !loadCudaRuntimeLibrary(tracing) || !loadCudaLibrary(tracing))
      {
      nDevices = 0;
      initialized = true;
      persistentInfo->getGpuInitializationMonitor()->exit(); //release the lock
      return 0; //failed
      }
   
   //Attempt to load and initialize the NVML library
   loadNVMLLibrary(tracing);

   //Attempt to initialize the CUDA driver API
   CUresult result = jitCuInit(0);
   if (result != CUDA_SUCCESS)
      {
      nDevices = 0;
      initialized = true;
      persistentInfo->getGpuInitializationMonitor()->exit(); //release the lock
      if (tracing > 0) TR_VerboseLog::writeLine(TR_Vlog_GPU, "FAILED with CUDA error : CUresult = %d", result);
      return 0; //failed
      }

   //Attempt to get the number of GPU devices
   cudaError_t cudaError = jitCudaGetDeviceCount(&nDevices);
   if (cudaError != cudaSuccess)
      {
      nDevices = 0;
      initialized = true;
      persistentInfo->getGpuInitializationMonitor()->exit(); //release the lock
      if (tracing > 0) TR_VerboseLog::writeLine(TR_Vlog_GPU, "FAILED with CUDA error : cudaError_t = %d", cudaError);
      return 0; //failed
      }

   int deviceLoopLimit = nDevices > MAX_DEVICE_NUM ? MAX_DEVICE_NUM : nDevices;
   for (int deviceId = 0; deviceId < deviceLoopLimit; deviceId++)
      {
      cudaDeviceProp prop[3];   // workaround to avoid memory collapsing since recent CUDA version requires large size for cudeDeviceProp (by Keith)

      cudaError = jitCudaGetDeviceProperties(&prop[0], deviceId);
      if (cudaError != cudaSuccess)
         {
         nDevices = 0;
         initialized = true;
         persistentInfo->getGpuInitializationMonitor()->exit(); //release the lock
         if (tracing > 0) TR_VerboseLog::writeLine(TR_Vlog_GPU, "FAILED with CUDA error : cudaError_t = %d", cudaError);
         return 0; //failed
         }

      short major = (short)prop[0].major;
      short minor = (short)prop[0].minor;
      if (tracing)
         {
         TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tDevice Number %2d: name=%s, ComputeCapability=%d.%d", deviceId, prop[0].name, major, minor);
         fflush(NULL);
         }
      deviceCCs[deviceId] = (major << 16) | minor;
      
      if (deviceCCs[deviceId] != deviceCCs[0])
         {
         if (tracing > 1) TR_VerboseLog::writeLine(TR_Vlog_GPU, "GPU compute capability mismatch detected. Disabling multi GPU use");
         enableMultiGPU = false; //TODO: multiple devices not currently supported if devices have varying compute capabilities
         }
      }

   //Everything was successful. Return the GPU device count.
   initialized = true;
   persistentInfo->getGpuInitializationMonitor()->exit(); //release the lock
   return nDevices;
   }

int getCUmoduleSize()
   {
   return sizeof(CUmodule);
   }

#define DELETED_HOSTREF      0xffffffff
#define ACCESS_PINNED        0x80000000
struct HashEntry
   {
   void** hostRef;
   uint64_t deviceArray;
   uint32_t accessMode;
   bool valid;

   // copy out data
   CUdeviceptr cpOutdevArray;
   int32_t     cpOutElementSize;
   int32_t     cpOutIsNoCopyDtoH;
   size_t      cpOutStartAddressOffset;
   size_t      cpOutEndAddressOffset;
   };

struct CudaInfo {
public:
   CudaInfo() : module(0), kernel(0) {}
   CUmodule   module;
   CUfunction kernel;

   uint32_t   hashSize;
   HashEntry  *hashTable;
   
   uint16_t    tracing;
   uint16_t    device;
   unsigned long long availableMemory;
   CUdeviceptr exceptionPointer;
   
   TR::CodeGenerator::GPUScopeType regionType;
   CUresult    ffdcCuError;
   cudaError_t ffdcCudaError;
   bool        sessionFailed; 
};

#define returnOnTrue(result,errorCode) \
   { \
   if (result) \
      return errorCode; \
   }

bool isGPUFailed(CudaInfo* cudaInfo) 
     {
     if (!cudaInfo) return true;

     TR_ASSERT(cudaInfo, "cudaInfo should not be NULL");

     return cudaInfo->sessionFailed; 
     }

bool captureCudaError(cudaError_t result, CudaInfo* cudaInfo, char* functionName)
    {
    if (isGPUFailed(cudaInfo))
       return true;

    TR_ASSERT(cudaInfo, "cudaInfo should not be NULL");

    if (!cudaInfo->sessionFailed && result != cudaSuccess)
      { // new error, capture it
      cudaInfo->ffdcCudaError = result;
      cudaInfo->sessionFailed = true;

      if (cudaInfo->tracing > 0) 
         TR_VerboseLog::writeLine(TR_Vlog_GPU, "CUDA Runtime error %d at %s (%p)", result, functionName, cudaInfo);

      }

    return cudaInfo->sessionFailed; 
    }

bool captureCuError(CUresult result, CudaInfo* cudaInfo, char* functionName, bool print = true)
    {
    if (isGPUFailed(cudaInfo))
       return true;

    TR_ASSERT(cudaInfo, "cudaInfo should not be NULL");

    if (!cudaInfo->sessionFailed && result != CUDA_SUCCESS)
      { // new error, capture it
      cudaInfo->ffdcCuError = result;
      cudaInfo->sessionFailed = true;

      if (cudaInfo->tracing > 0 && print) 
          TR_VerboseLog::writeLine(TR_Vlog_GPU, "CUDA Driver error %d at %s (%p)", result, functionName, cudaInfo);
      }

    return cudaInfo->sessionFailed; 
    }

bool captureJITError(char* errorText, CudaInfo* cudaInfo, bool print = true)
    {
    if (isGPUFailed(cudaInfo))
       return true;

    TR_ASSERT(cudaInfo, "cudaInfo should not be NULL");

    if (!cudaInfo->sessionFailed)
      { // new error, capture it
      cudaInfo->sessionFailed = true;

      if (cudaInfo->tracing > 0 && print)
          TR_VerboseLog::writeLine(TR_Vlog_GPU, "JIT GPU Error: %s (%p)", errorText, cudaInfo);
      }

    return true;
    }

static HashEntry* cudaInfoHashGet(CudaInfo *cudaInfo, void **hostref) 
   {
   for (int32_t i = 0; i < cudaInfo->hashSize; i++)
      {
      if (cudaInfo->hashTable[i].hostRef == 0)
         break;
      if (cudaInfo->hashTable[i].hostRef == (void **)DELETED_HOSTREF)
         continue;
      if (*cudaInfo->hashTable[i].hostRef == *hostref)
         {
         return &(cudaInfo->hashTable[i]);
         }
      }
   return NULL;
   }

static HashEntry* 
cudaInfoHashPut(CudaInfo *cudaInfo, void **hostref, int32_t elementSize, CUdeviceptr deviceArray, uint32_t accessMode) 
   {
   bool detailsTrace = (cudaInfo->tracing > 1);

   int32_t i = 0;
   for (; i < cudaInfo->hashSize; i++)
      {
      if (cudaInfo->hashTable[i].hostRef == 0)
         break;
      if (cudaInfo->hashTable[i].hostRef == (void **)DELETED_HOSTREF)
         continue;
      if (*cudaInfo->hashTable[i].hostRef == *hostref)
         return &(cudaInfo->hashTable[i]);
      }

   if (i == cudaInfo->hashSize)
      {
      HashEntry *oldHashTable = cudaInfo->hashTable;
      uint32_t oldHashSize = cudaInfo->hashSize;
      int32_t hashSize = oldHashSize * 3;
      HashEntry *hashTable = (HashEntry *)TR_Memory::jitPersistentAlloc(hashSize * sizeof(HashEntry), TR_Memory::GPUHelpers);

      if (!hashTable)
         {
         if (detailsTrace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "FAILED to extend persistent memory for cudaInfo hashTable to size %d bytes.", hashSize * sizeof(HashEntry));
         returnOnTrue(captureJITError("cudaInfoHashPut - out of JIT persistent memory", cudaInfo, detailsTrace), NULL);
         }

      cudaInfo->hashSize = hashSize;
      cudaInfo->hashTable = hashTable;

      memcpy(cudaInfo->hashTable, oldHashTable, oldHashSize * sizeof(HashEntry));

      TR_Memory::jitPersistentFree(oldHashTable);
       
      if (detailsTrace)
         TR_VerboseLog::writeLine(TR_Vlog_GPU, "EXTENDED hash table: %d", cudaInfo->hashSize);
      }

   if (i < cudaInfo->hashSize)
      {
      cudaInfo->hashTable[i].hostRef     = hostref;
      cudaInfo->hashTable[i].deviceArray = (uint64_t)deviceArray;
      cudaInfo->hashTable[i].accessMode  = accessMode;
      cudaInfo->hashTable[i].valid       = true;
      cudaInfo->hashTable[i].cpOutIsNoCopyDtoH = 1; //this will be cleared if the array needs to be copied back
      return &(cudaInfo->hashTable[i]);
      }
   

   return NULL;
   }

static CUdeviceptr
cudaInfoHashRemove(CudaInfo *cudaInfo, void **hostref, int32_t elementSize) 
   {
   for (int32_t i = 0; i < cudaInfo->hashSize; i++)
      {
      if (cudaInfo->hashTable[i].hostRef == 0)
         break;
      if (cudaInfo->hashTable[i].hostRef == (void **)DELETED_HOSTREF)
         continue;
      if (*cudaInfo->hashTable[i].hostRef == *hostref)
         {
         CUdeviceptr p = (CUdeviceptr)cudaInfo->hashTable[i].deviceArray;
         cudaInfo->hashTable[i].hostRef = (void **)DELETED_HOSTREF;
         return p;
         }
      }
   return NULL_DEVICE_PTR;
   }

#define OBJECT_HEADER_SIZE sizeof(J9IndexableObjectContiguous)
#define OBJECT_ARRAY_LENGTH_OFFSET (TMP_OFFSETOF_J9INDEXABLEOBJECTCONTIGUOUS_SIZE)
#define HEADER_PADDING  (TR::CodeGenerator::GPUAlignment - OBJECT_HEADER_SIZE)

#define     LOGSIZE     8192
#define     OPTIONSNUM  3

static int
cudaModuleLoadData(CUlinkState *lState, CudaInfo *cudaInfo, const char *ptxSource, bool writeCUBIN, bool writeLINKER)
   {
   CUjit_option optionCmds[OPTIONSNUM];
   void* optionVals[OPTIONSNUM];
   char info_log[LOGSIZE], error_log[LOGSIZE];
   size_t logSize = LOGSIZE;
   float wallTime;
   int tracing = cudaInfo->tracing;

   void* cubin;
   size_t cubinSize;

   // Setup linker options
   // Pass a buffer for info messages
   optionCmds[0] = CU_JIT_INFO_LOG_BUFFER;
   optionVals[0] = (void*) info_log;
   // Pass the size of the info buffer
   optionCmds[1] = CU_JIT_INFO_LOG_BUFFER_SIZE_BYTES;
   optionVals[1] = (void*) logSize;
   // Make the linker verbose
   optionCmds[2] = CU_JIT_LOG_VERBOSE;
   optionVals[2] = (void*) 1;

   // Create a pending linker invocation
   returnOnTrue(captureCuError(jitCuLinkCreate(OPTIONSNUM, optionCmds, optionVals, lState), cudaInfo, "cudaModuleLoadData - jitCuLinkCreate"), ERROR_CODE);
   returnOnTrue(captureCuError(jitCuLinkAddData(*lState, CU_JIT_INPUT_PTX, (void*)ptxSource, strlen(ptxSource)+1, 0, 0, 0, 0), cudaInfo, "cudaModuleLoadData - jitCuLinkAddData"), ERROR_CODE);
   returnOnTrue(captureCuError(jitCuLinkComplete(*lState, &cubin, &cubinSize), cudaInfo, "cudaModuleLoadData - jitCuLinkComplete"), ERROR_CODE);
 
   if (writeLINKER)
      writeDebugFile(ptxSource, info_log, strlen(info_log), ".linker", "wb");
   if (writeCUBIN)
      writeDebugFile(ptxSource, cubin, cubinSize, ".cubin", "wb");
   
   // Load resulting cuBin into module
   returnOnTrue(captureCuError(jitCuModuleLoadData(&(cudaInfo->module), cubin), cudaInfo, "cudaModuleLoadData - jitCuModuleLoadData"), ERROR_CODE);

   return 0;
}

struct ThreadGPUInfo
   {
public:
   ThreadGPUInfo() : nDevices(0) {}
   int32_t threadID;
   int32_t nDevices;
   cudaStream_t stream[MAX_DEVICE_NUM];
   };

#define getThreadGPUInfo(vmThread) ((ThreadGPUInfo *)((vmThread)->codertTOC))

#define GETSHIFTVAL(x)  (((x) == 0x00000001) ?  0 : ((x) == 0x00000002) ?  1 : ((x) == 0x00000004) ?  2 : ((x) == 0x00000008) ?  3 :\
                         ((x) == 0x00000010) ?  4 : ((x) == 0x00000020) ?  5 : ((x) == 0x00000040) ?  6 : ((x) == 0x00000080) ?  7 :\
                         ((x) == 0x00000100) ?  8 : ((x) == 0x00000200) ?  9 : ((x) == 0x00000400) ? 10 : ((x) == 0x00000800) ? 11 :\
                         ((x) == 0x00001000) ? 12 : ((x) == 0x00002000) ? 13 : ((x) == 0x00004000) ? 14 : ((x) == 0x00008000) ? 15 : 16);

void *initGPUThread(J9VMThread *vmThread, TR::PersistentInfo *persistentInfo, bool isForce)
   {
   ThreadGPUInfo *threadGPUInfo = getThreadGPUInfo(vmThread);
   if (threadGPUInfo != NULL)
      {
      return threadGPUInfo;
      }

   static bool disableGPU = false;
   if (!isForce && (disableGPU || !(TR::Options::getCmdLineOptions()->getEnableGPU(TR_EnableGPU))))
      {
      disableGPU = true;
      return NULL;
      }

   int tracing = 0;
   if (TR::Options::getCmdLineOptions()->getEnableGPU(TR_EnableGPUVerbose)) tracing = 1;
   if (TR::Options::getCmdLineOptions()->getEnableGPU(TR_EnableGPUDetails)) tracing = 2;

   static bool disableAsyncOps = feGetEnv("TR_disableGPUAsyncOps") ? true : false;
   static bool enableDefaultStream = disableAsyncOps || (feGetEnv("TR_enableGPUDefaultStream") ? true : false);

   threadGPUInfo = (ThreadGPUInfo *)TR_Memory::jitPersistentAlloc(sizeof(ThreadGPUInfo), TR_Memory::GPUHelpers);
   if (!threadGPUInfo)
      {
      if (tracing) TR_VerboseLog::writeLine(TR_Vlog_GPU, "FAILED to allocate persistent memory for threadGPUInfo object.");
      vmThread->codertTOC = NULL;
      return NULL;
      }

   int nDevices = getGpuDeviceCount(persistentInfo, tracing);
   if (nDevices == 0) //No devices found or Nvidia libs can not be found so the GPU can not be used
      {
      TR_Memory::jitPersistentFree(threadGPUInfo);
      return NULL;
      }
   
   threadGPUInfo->nDevices = nDevices;
   TR_ASSERT(nDevices > 0, "nDevices must be greater than 0 at initGPUThread()");
   
   uintptr_t shiftval = GETSHIFTVAL(J9VMTHREAD_ALIGNMENT);
   threadGPUInfo->threadID = (int)(((uintptr_t)vmThread >> shiftval) & 0x7FFFFFFF);

   if (!enableDefaultStream)
      {
      for (int device = 0; device < nDevices; device++)
         {
         checkCudaError(jitCudaSetDevice(device), NULL, tracing);
         checkCudaError(jitCudaStreamCreate(&(threadGPUInfo->stream[device])), NULL, tracing);
         }
      }
   else
      {
      for (int device = 0; device < nDevices; device++)
         {
         // set default stream (0 or NULL) for all devices
         threadGPUInfo->stream[device] = 0;
         }
      }

   // default GPU device is 0
   checkCudaError(jitCudaSetDevice(0), NULL, tracing);

   vmThread->codertTOC = (void *)threadGPUInfo;

   return threadGPUInfo;
   }

extern "C" void *initGPUThread(J9VMThread *vmThread, TR::PersistentInfo *persistentInfo)
   {
   return initGPUThread(vmThread, persistentInfo, false);
   }

extern "C" void *terminateGPUThread(J9VMThread *vmThread)
   {
   ThreadGPUInfo *threadGPUInfo = getThreadGPUInfo(vmThread);
   if (threadGPUInfo == NULL)
      return NULL;

   TR_Memory::jitPersistentFree(threadGPUInfo);
   return NULL;
   }

static CudaInfo *
initCUDA(J9VMThread *vmThread, int tracing, const char *ptxSource, int deviceId, int ptxSourceID, GpuMetaData* gpuMetaData)
   {
   bool trace = (tracing >= 2);

   if (trace)
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tInitializing CUDA");

   ThreadGPUInfo *threadGPUInfo = getThreadGPUInfo(vmThread);
   if (threadGPUInfo == NULL)
      {
      threadGPUInfo = (ThreadGPUInfo *)initGPUThread(vmThread, NULL, true);
      if (threadGPUInfo == NULL)
         {
         return 0;
         }
      }

   CudaInfo *cudaInfo = (CudaInfo *)TR_Memory::jitPersistentAlloc(sizeof(CudaInfo), TR_Memory::GPUHelpers);

   if (!cudaInfo)
      {
      if (trace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tFAILED to allocate persistent memory for CudaInfo object.");
      return 0;
      }
   
   cudaInfo->sessionFailed = false; 
   cudaInfo->ffdcCuError = CUDA_SUCCESS;
   cudaInfo->ffdcCudaError = cudaSuccess;
   cudaInfo->tracing = (uint16_t)tracing;
   cudaInfo->device = deviceId;
   cudaInfo->availableMemory = ULLONG_MAX;
   cudaInfo->exceptionPointer = NULL_DEVICE_PTR;
   cudaInfo->module = 0;
   cudaInfo->kernel = 0;
   cudaInfo->hashSize = 32;

   cudaInfo->hashTable = (HashEntry *)TR_Memory::jitPersistentAlloc(cudaInfo->hashSize * sizeof(HashEntry), TR_Memory::GPUHelpers);
   if (!cudaInfo->hashTable)
      {
      TR_Memory::jitPersistentFree(cudaInfo);
      if (trace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tFAILED to allocate persistent memory for cudaInfo hashTable.");
      return 0;
      }
   memset(cudaInfo->hashTable, 0, cudaInfo->hashSize * sizeof(HashEntry));

   double startTime, deviceSetTime, contextSetupTime;
   startTime = currentTime();

   returnOnTrue(captureCudaError(jitCudaSetDevice(deviceId), cudaInfo, "initCUDA - jitCudaSetDevice"), NULL);

   deviceSetTime = currentTime();

   returnOnTrue(captureCudaError(jitCudaFree(0), cudaInfo, "initCUDA - jitCudaFree"), NULL);

   contextSetupTime = currentTime();

   if (trace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tcudaSetDevice Time: %.3f msec.", (deviceSetTime - startTime)/1000);
   if (trace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tcontext setup Time: %.3f msec.", (contextSetupTime - deviceSetTime)/1000);

   return cudaInfo;
   }

static GpuMetaData* getGPUMetaData(uint8_t* startPC)
   {
      J9VMThread *vmThread = jitConfig->javaVM->internalVMFunctions->currentVMThread(jitConfig->javaVM);
      TR_MethodMetaData * metaData = jitConfig->jitGetExceptionTableFromPC(vmThread, (UDATA)startPC);
      return (GpuMetaData*)metaData->gpuCode;
   }

// load GPU kernel into the cudaInfo structure
static bool loadKernel(CudaInfo *cudaInfo, GpuMetaData* gpuMetaData, int kernelId, int deviceId)
   {
   CUlinkState lState = 0;

   char *ptxSource;
   ptxSource = (gpuMetaData->ptxArray)[kernelId];

   static bool writeCUBIN = feGetEnv("TR_writeCUBIN") ? true : false;
   static bool writeLINKER = feGetEnv("TR_writeLINKER") ? true : false;
   static bool disableModuleCaching = feGetEnv("TR_disableCUmoduleCaching") ? true : false;

   char functionName[16]; // 16 is max length of decimal string of ptxSourceID
   sprintf(functionName, "test%d", kernelId);

   int numPtxKernels = gpuMetaData->numPtxKernels;
   int maxNumCachedDevices = gpuMetaData->maxNumCachedDevices;
   CUmodule *storedModuleArray = (CUmodule*)(gpuMetaData->cuModuleArray);

   int cuModuleIndex = kernelId + numPtxKernels * deviceId;

   if (!disableModuleCaching && deviceId < maxNumCachedDevices)
      {
      cudaInfo->module = storedModuleArray[cuModuleIndex];
      }

   bool validStoredModule = cudaInfo->module ? true : false;

   if (!validStoredModule || disableModuleCaching)
      {
      if (writeCUBIN || writeLINKER)
         cudaModuleLoadData(&lState, cudaInfo, ptxSource, writeCUBIN, writeLINKER);
      else
        returnOnTrue(captureCuError(jitCuModuleLoadDataEx(&cudaInfo->module, ptxSource, 0, 0, 0), cudaInfo, "loadKernel - jitCuModuleLoadDataEx"), false);

      if (!disableModuleCaching && deviceId < maxNumCachedDevices)
         storedModuleArray[cuModuleIndex] = cudaInfo->module;
      }

   returnOnTrue(captureCuError(jitCuModuleGetFunction(&cudaInfo->kernel, cudaInfo->module, functionName), cudaInfo, "loadKernel - jitCuModuleGetFunction"), false);

   if ((!validStoredModule || disableModuleCaching) && (writeCUBIN || writeLINKER))
      {
      // Destroy the linker invocation
      if (lState)
         returnOnTrue(captureCuError(jitCuLinkDestroy(lState), cudaInfo, "loadKernel - jitCuLinkDestroy"), false); //TODO: need to free cudaInfo on error
      }

   return true;
   }

// libDevice
static bool addlibDeviceToProgram(nvvmProgram program, int computeArch, bool trace, TR_Memory* m)
   {
   const char *libNVVMHOME = "/usr/local/cuda/nvvm"; // FIXME
   const char *libdeviceByteCode = NULL;
   char *libDevicePath = NULL;

   if (!m)
   {
      if (trace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tTR_Memory is null. Can't allocate memory for libDevice");
      return false;
   }

   // try to get a path libNVVM.
   if (libNVVMHOME == NULL)
      {
      if (trace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tPath to libNVVM is UNKNOWN.");
      return false;
      }

   //Use libdevice for compute_20, if the target is not compute_30 or compute_35.
   switch (computeArch)
      {
      case 30:
         libdeviceByteCode = "/libdevice/libdevice.compute_30.10.bc";
         break;
      case 35:
         libdeviceByteCode = "/libdevice/libdevice.compute_35.10.bc";
         break;
      default:
         libdeviceByteCode = "/libdevice/libdevice.compute_20.10.bc";
         break;
      }

   // Concatenate libNVVMHOME and name.
   libDevicePath = (char *) m->allocateHeapMemory(strlen(libNVVMHOME) + strlen(libdeviceByteCode) + 1);
   if (libDevicePath == NULL)
      {
      if (trace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tFAILED to allocate memory for libDevice path.");
      return false;
      }
   libDevicePath = strcat(strcpy(libDevicePath, libNVVMHOME), libdeviceByteCode);

   if (trace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tAdding libDevice to NVVM program : %s", libDevicePath);

   // Open libDevice file
   char        *buffer;
   size_t       size;
   struct stat         fileStat;

   FILE *f = fopen(libDevicePath, "rb");
   if (f == NULL)
      {
      if (trace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tFAILED to open libDevice file.");
      return false;
      }

   // Allocate buffer for the libDevice.
   fstat(fileno(f), &fileStat);
   buffer = (char *) m->allocateHeapMemory(fileStat.st_size);
   if (buffer == NULL)
      {
      if (trace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tFAILED to allocate memory for libDevice file.");
      fclose(f);
      return false;
      }
   // Copy libDevice file to buffer.
   size = fread(buffer, 1, fileStat.st_size, f);
   if (ferror(f))
      {
      if (trace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tFAILED to allocate memory for a buffer for libDevice.");
      fclose(f);
      return false;
      }
   fclose(f);

   if (jitNvvmAddModuleToProgram(program, buffer, size, libDevicePath) != NVVM_SUCCESS)
      {
      if (trace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tFAILED to add the module %s to the compilation unit.", libDevicePath);
      return false;
      }

   return true;
   }

bool
calculateComputeCapability(int tracing, short * computeMajor, short * computeMinor, int deviceId)
   {
   bool detailsTrace = (tracing >= 2);

   short major, minor;

   major = (short)(deviceCCs[deviceId] >> 16);
   minor = (short)(deviceCCs[deviceId] & 0xFFFF);

   if (detailsTrace)
      {
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tDevice Number %2d: ComputeCapability=%d.%d", deviceId, major, minor);
      fflush(NULL);
      }

   if((major != UNKNOWN_CC) && (minor != UNKNOWN_CC))
      {
      if (major > 3) //Compute Capability range: 4.0+
         {
         *computeMajor = 3;
         *computeMinor = 5;
         }
      else if (major == 3 && minor >= 5) //Compute Capability range: 3.5-3.9
         {
         *computeMajor = 3;
         *computeMinor = 5;
         }
      else if (major == 3) //Compute Capability range: 3.0-3.4
         {
         *computeMajor = 3;
         *computeMinor = 0;
         }
      else if (major == 2) //Compute Capability range: 2.0-2.9
         {
         *computeMajor = 2;
         *computeMinor = 0;
         }
      else //Compute Capability range: 0.0-1.9
	 {
	 if (detailsTrace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tCompute Capability %d.%d is unsupported. Compute Capability must be 2.0 or higher", major, minor);
         return false;
         }
      }
   else //Compute Capability unknown
      {
      if (detailsTrace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tUnknown Compute Capability. Compute Capability must be 2.0 or higher", major, minor);
      return false;
      }

   return true;
   }

bool
getNvvmVersion(int tracing, int* majorVersion, int* minorVersion)
   {
   //checkNVVMError will return NULL if there's an error
   checkNVVMError(jitNvvmVersion(majorVersion, minorVersion), tracing);

   return true;
   }

char *
generatePTX(int tracing, const char *programSource, int deviceId, TR::PersistentInfo * persistentInfo, TR_Memory* m, bool enableMath)
   {
   nvvmProgram program;
   nvvmResult result;
   short computeMajor, computeMinor;
   clock_t start, end;
   J9VMThread *vmThread = jitConfig->javaVM->internalVMFunctions->currentVMThread(jitConfig->javaVM);
   ThreadGPUInfo *threadGPUInfo = getThreadGPUInfo(vmThread);
   bool detailsTrace = (tracing >= 2);

#if (defined(LINUX) || !defined(TR_TARGET_X86))
   struct sigaction sigactionArray[31];
#endif
   
   //Currently only device ids in the range of [0,MAX_DEVICE_NUM-1] are supported. Compilation fails for ids above MAX_DEVICE_NUM-1.
   if (deviceId >= MAX_DEVICE_NUM)
      {
      if (detailsTrace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tgeneratePTX failed. Device ID is %d. Device IDs greater than or equal to %d are not supported.", deviceId, MAX_DEVICE_NUM);
      return NULL;
      }

   if (threadGPUInfo == NULL)
      {
      threadGPUInfo = (ThreadGPUInfo *)initGPUThread(vmThread, persistentInfo, true);
      if (threadGPUInfo == NULL)
         {
         return NULL;
         }
      }

   if (deviceId >= threadGPUInfo->nDevices)
      {
      if (detailsTrace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tDevice ID is %d which is greater than or equal to the total number of devices: %d", deviceId, threadGPUInfo->nDevices);
      return NULL;
      }

   if (!calculateComputeCapability(tracing, &computeMajor, &computeMinor, deviceId))
      {
      if (detailsTrace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tgeneratePTX failed. calculateComputeCapability was unsuccessful.");
      return NULL;
      }

   if (detailsTrace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tCreating NVVM program");
   checkNVVMError(jitNvvmCreateProgram(&program), tracing);

   if (detailsTrace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tAdding NVVM module size=%d", strlen(programSource));
   checkNVVMError(jitNvvmAddModuleToProgram(program, programSource, strlen(programSource), "jittedcode"), tracing);
   if (detailsTrace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tAdded NVVM module size=%d", strlen(programSource));

#define OPTIONLENGTH    6

   char optionStr[OPTIONLENGTH][24] = {"-opt=0", "-ftz=1", "-prec-sqrt=1", "-prec-div=1", "-fma=0", "-arch=compute_MMMMmmmm"};
   char *options[OPTIONLENGTH];
   int optionLength = (computeMajor == 256 && computeMinor == 256) ? OPTIONLENGTH - 1 : OPTIONLENGTH;

   for (int i = 0; i < optionLength; i++)
      {
      options[i] = (char *)optionStr[i];
      }

   sprintf(options[OPTIONLENGTH-1], "-arch=compute_%d%d", computeMajor, computeMinor);

   //nvvm version 1.0 works with -opt=3
   //nvvm version 1.1 is untested. -opt=0 is used since it might have the same problem as version 1.2
   //nvvm version 1.2 sometimes segfaults in nvvmCompileProgram with -opt=3 so -opt=0 is used instead
   //1.2 is the latest version as of this comment. -opt=0 is default for future versions for safety
   int nvvmVersionMajor, nvvmVersionMinor; 
   checkNVVMError(jitNvvmVersion(&nvvmVersionMajor, &nvvmVersionMinor), tracing);
   if (detailsTrace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tNVVM Version: %d.%d", nvvmVersionMajor, nvvmVersionMinor);
   if ( (nvvmVersionMajor == 1) && (nvvmVersionMinor == 0) )
      sprintf(options[0], "-opt=3");

   if (enableMath)
      {
      // add libDevice to the program
      start = clock();
      if (!addlibDeviceToProgram(program, computeMajor*10+computeMinor, detailsTrace, m))
         {
         if (detailsTrace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tFAILED to add libDevice.");
         return 0;
         }
      end = clock();
      if (detailsTrace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tAdding libdevice : %6.3f msec", (double)(end-start)*1000/CLOCKS_PER_SEC);
      }

   if (detailsTrace)
      {
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tCompiling NVVM program with options:");
      for (int i = 0; i < optionLength; i++)
         {
         TR_VerboseLog::write(" %s", options[i]);
         }
      fflush(NULL);
      }

   //nvvmCompileProgram changes the signal handler for multiple signals which can cause errors when the wrong signal handler is called.
   //As a temporary solution, the current signal handlers are saved before the call to nvvmCompileProgram.
   //After the call returns, the signal handlers are restored from the saved versions.
   //This is a temporary solution since errors can still occur if a trap occurs between the call to nvvmCompileProgram and restoring
   //the signal handlers. Incorrect signal handler restoration is also possible if the signal handlers are saved during this point.
   //In the future the real problem of nvvmCompileProgram messing with the signal handlers will need to be fixed.
   //TODO: remove signal handler saving and restoring after we get a version of nvvmCompileProgram that doesn't change the signal handlers.
#if (defined(LINUX) || !defined(TR_TARGET_X86))
   saveSignalHandlers(sigactionArray, detailsTrace);
#endif

   start = clock();
   result = jitNvvmCompileProgram(program, optionLength, (const char **)options);

   end = clock();

#if (defined(LINUX) || !defined(TR_TARGET_X86))
   restoreSignalHandlers(sigactionArray, detailsTrace);
#endif

   if (detailsTrace)
      {
      TR_VerboseLog::write("  %6.3f msec", (double)(end-start)*1000/CLOCKS_PER_SEC);
      fflush(NULL);
      }

   if (result != NVVM_SUCCESS)
      {
      if (detailsTrace)
         {
         TR_VerboseLog::writeLine(TR_Vlog_GPU, "FAILED");
         fflush(NULL);
         }
      size_t logSize;
      jitNvvmGetProgramLogSize(program, &logSize);
      char *log;
      if (m)
         {
         log = (char*)m->allocateHeapMemory(logSize);
         }
      if (log)
         {
         jitNvvmGetProgramLog(program, log);
         if (detailsTrace) TR_VerboseLog::writeLine(TR_Vlog_GPU, "%s", log);
         }
      return 0;
     }

   size_t ptxSize;
   checkNVVMError(jitNvvmGetCompiledResultSize(program, &ptxSize), tracing);

   char *ptxString;
   if (m)
      {
      ptxString = (char*)m->allocateHeapMemory(ptxSize);
      }

   if (!ptxString)
      return 0;

   checkNVVMError(jitNvvmGetCompiledResult(program, ptxString), tracing);
   checkNVVMError(jitNvvmDestroyProgram(&program), tracing);

   return ptxString;
   }

static TR::CodeGenerator::GPUResult
launchKernel(int tracing, CudaInfo *cudaInfo, int gridDimX, int gridDimY, int gridDimZ, 
                                         int blockDimX, int blockDimY, int blockDimZ,
                                         int argCount, bool needExtraArg, void** args, int kernelID, int hasExceptionChecks)
   {
   TR::CodeGenerator::GPUResult ExceptionKind = TR::CodeGenerator::GPUSuccess;
   double time;
   void **_args = args;

   returnOnTrue(!cudaInfo,TR::CodeGenerator::GPUHelperError);

   J9VMThread *vmThread = jitConfig->javaVM->internalVMFunctions->currentVMThread(jitConfig->javaVM);
   ThreadGPUInfo *threadGPUInfo = getThreadGPUInfo(vmThread);
   TR_ASSERT(threadGPUInfo, "threadGPUInfo should not be NULL at launchKernel()");
   cudaStream_t stream = threadGPUInfo->stream[cudaInfo->device];

   static bool disableAsyncOps = feGetEnv("TR_disableGPUAsyncOps") ? true : false;

   if (hasExceptionChecks)
      {
      if (cudaInfo->exceptionPointer == NULL_DEVICE_PTR)
         {
         returnOnTrue(captureCuError(jitCuMemAlloc(&(cudaInfo->exceptionPointer), sizeof(int32_t)), cudaInfo, "launchKernel - jitCuMemAlloc"), TR::CodeGenerator::GPUHelperError);
         
         if (tracing > 1)
            TR_VerboseLog::writeLine(TR_Vlog_GPU,"\tallocated %d bytes at %p for GPU exceptions (%p)", sizeof(int32_t), cudaInfo->exceptionPointer, cudaInfo);
         }
   
      returnOnTrue(captureCuError(!disableAsyncOps ? jitCuMemcpyHtoDAsync(cudaInfo->exceptionPointer, &ExceptionKind, sizeof(int32_t), stream)
                                     : jitCuMemcpyHtoD(cudaInfo->exceptionPointer, &ExceptionKind, sizeof(int32_t)), cudaInfo, "launchKernel - jitCuMemcpyHtoD"),
                                       TR::CodeGenerator::GPUHelperError);
      
      if (tracing > 1) 
         TR_VerboseLog::writeLine(TR_Vlog_GPU,"\tperforming a copy to GPU %d bytes for GPU exceptions. Device: %p (%p)", sizeof(int32_t), cudaInfo->exceptionPointer, cudaInfo);
      }

   if (tracing > 1)
      {
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "Launching GPU Kernel (%p), kernelID=%d",cudaInfo, kernelID);
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "\targCount: %d", argCount - (needExtraArg ? 0:1));
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tgrid: Dim(%d %d %d), Block(%d %d %d)", gridDimX, gridDimY, gridDimZ, blockDimX, blockDimY, blockDimZ);
      }

   if (needExtraArg)
      {
      _args = (void **)malloc((argCount + 1) * sizeof(void**));
      memcpy(_args, args, argCount * sizeof(void**));
      argCount++;
      }
   _args[argCount-1] = &(cudaInfo->exceptionPointer);
   
   if (tracing > 1)
      {
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "Args dump:");
      for (int i = 0; i < argCount; i++) 
          {
          TR_VerboseLog::writeLine(TR_Vlog_GPU, "Arg %d: %p",i,*((long*)_args[i]));
          }
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "kernel: %p",cudaInfo->kernel);
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "stream: %p",stream);
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "_args: %p",_args);
      }
   time = currentTime();
   
   returnOnTrue(captureCuError(jitCuLaunchKernel(
                                  cudaInfo->kernel,
                                  gridDimX, gridDimY, gridDimZ,
                                  blockDimX, blockDimY, blockDimZ,
                                  0, stream, _args, NULL),
                                  cudaInfo, "launchKernel - jitCuLaunchKernel"),
                                  TR::CodeGenerator::GPUHelperError);  // TODO: used to be GPULaunchError

   if (tracing > 1)
      {
      if (stream == NULL)
         jitCudaDeviceSynchronize();
      else
         jitCudaStreamSynchronize(stream);
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tcuLaunchKernel: %.3f msec.", (currentTime() - time)/1000);
      }


   if (hasExceptionChecks)
      {
      returnOnTrue(captureCuError(!disableAsyncOps ? jitCuMemcpyDtoHAsync(&ExceptionKind, cudaInfo->exceptionPointer, sizeof(int32_t), stream)
                                    : jitCuMemcpyDtoH(&ExceptionKind, cudaInfo->exceptionPointer, sizeof(int32_t)),
                                       cudaInfo, "launchKernel - jitCuMemcpyDtoH"), TR::CodeGenerator::GPUHelperError);
      
      if (tracing > 1)
         TR_VerboseLog::writeLine(TR_Vlog_GPU,"\tCopy out %d bytes for exceptions. Device: %p (%p)", sizeof(int32_t), cudaInfo->exceptionPointer, cudaInfo);
      
      if (disableAsyncOps)
         {
         returnOnTrue(captureCudaError(jitCudaDeviceSynchronize(), cudaInfo, "launchKernel - jitCudaDeviceSynchronize"), TR::CodeGenerator::GPUHelperError);
         }
      else
         {
         returnOnTrue(captureCudaError(jitCudaStreamSynchronize(stream), cudaInfo, "launchKernel - jitCudaStreamSynchronize"), TR::CodeGenerator::GPUHelperError);
         }
      }

   if (needExtraArg)
      {
      free(_args);
      }

   return ExceptionKind;
   }


// GPU helpers (called from generated code)

int flushGPUDatatoCPU(CudaInfo *cudaInfo);
bool freeGPUScope(CudaInfo *cudaInfo);

extern "C" int
estimateGPU(CudaInfo *cudaInfo, int ptxSourceID, uint8_t *startPC, int lambdaCost, int dataSize, int startInclusive, int endExclusive)
   {
   returnOnTrue(!cudaInfo,0); // if cudaInfo is not defined, there is an error condition so pass through
                              // to enable the error handler to kick in 
   
   char *methodSignature;
   int lineNumber;
   int trace = cudaInfo->tracing;
   int dataCost = (cudaInfo->regionType == TR::CodeGenerator::naturalLoopScope) ? 0 : dataSize;

   if (trace > 0)
      {
      GpuMetaData* gpuMetaData = getGPUMetaData(startPC);

      methodSignature = gpuMetaData->methodSignature;
      lineNumber = (gpuMetaData->lineNumberArray)[ptxSourceID];
      }

   long range = (long)endExclusive - (long)startInclusive;

   if (trace > 1)
      {
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tforEach in %s at line %d has lambdaCost %d dataCost %d range %lld ratio %d", methodSignature, lineNumber, lambdaCost, dataCost, range, dataCost/range);
      }


   bool directToCPU = ((cudaInfo->availableMemory < (unsigned long long)dataSize) || lambdaCost == 0 || dataCost/range > 80 || range < 1024);

   if (directToCPU)
      {
      if (trace > 0)
         TR_VerboseLog::writeLine(TR_Vlog_GPU, "[time.ms=%lld]: Sent forEach in %s at line %d to CPU based on performance heuristics", (int64_t)(currentTime()/1000), methodSignature, lineNumber);

      switch(cudaInfo->regionType)
      {
      case TR::CodeGenerator::singleKernelScope:
         
         // no need to flush for single kernel scope
         // since the GPU doesn't contain any valid data
         freeGPUScope(cudaInfo);
         break;
      case TR::CodeGenerator::naturalLoopScope:
         flushGPUDatatoCPU(cudaInfo);
         break;
      }
            
      return 1;
      }

   return 0;
   }

static int selectGPUDevice(J9VMThread *vmThread, int tracing, unsigned long long *availableMemory, bool enforceGPU)
   {
   if (!isNVMLavailable)
      {
      return 0;
      }
   
   static bool disableGPUHighUtilRedirect = enforceGPU || feGetEnv("TR_disableGPUHighUtilRedirect") ? true : false;
   static int gpuUtilThreshold = feGetEnv("TR_GPUUtilThreshold") ? atoi(feGetEnv("TR_GPUUtilThreshold")) : 90;

   int deviceId = 0;
   ThreadGPUInfo *threadGPUInfo = getThreadGPUInfo(vmThread);
   
   if (threadGPUInfo == NULL)
      {
      threadGPUInfo = (ThreadGPUInfo *)initGPUThread(vmThread, NULL, true);
      if (threadGPUInfo == NULL)
         {
         if (tracing > 1) TR_VerboseLog::writeLine(TR_Vlog_GPU, "[%16p] GPUdevice is not selected due to null threadGPUInfo", vmThread);
         return 0;
         }
      }
   TR_ASSERT(threadGPUInfo, "threadGPUInfo should not be NULL");

   nvmlReturn_t result;
   uint32_t deviceCount = getGpuDeviceCount(0, tracing);
   if (tracing > 1) TR_VerboseLog::writeLine(TR_Vlog_GPU, "[%16p] #ofDevices=%d", vmThread, deviceCount);

   int gpuUtilization = INT_MAX;
   bool foundDevice = false;
   int localGPUdeviceNumber = threadGPUInfo->threadID;
   
   for (uint32_t i = 0; i < deviceCount; i++)
      {
      nvmlDevice_t device;
      nvmlMemory_t memory;
      nvmlUtilization_t utilization;
      uint32_t devId = (localGPUdeviceNumber + i) % deviceCount;

      result = jitNvmlDeviceGetHandleByIndex(devId, &device);
      if (NVML_SUCCESS != result)
         { 
         if (tracing > 1) TR_VerboseLog::writeLine(TR_Vlog_GPU, "Failed to get handle for device %d: %s", devId, jitNvmlErrorString(result));
         return 0;
         }
      result = jitNvmlDeviceGetUtilizationRates(device, &utilization);
      if (NVML_SUCCESS != result)
         { 
         if (tracing > 1) TR_VerboseLog::writeLine(TR_Vlog_GPU, "Failed to get GPU utilization for device %d: %s", devId, jitNvmlErrorString(result));
         return 0;
         }
      result = jitNvmlDeviceGetMemoryInfo(device, &memory);
      if (NVML_SUCCESS != result)
         { 
         if (tracing > 1) TR_VerboseLog::writeLine(TR_Vlog_GPU, "Failed to get memory information for device %d: %s", devId, jitNvmlErrorString(result));
         return 0;
         }
      if (tracing > 1) TR_VerboseLog::writeLine(TR_Vlog_GPU, "[%16p]   device%3d: GPUutilization=%4d, freemem=%lld", vmThread, devId, utilization.gpu, memory.free);
      
      if ((utilization.gpu <= gpuUtilThreshold || disableGPUHighUtilRedirect) && (utilization.gpu < gpuUtilization))
         {
         gpuUtilization = utilization.gpu;
         *availableMemory = memory.free;
         deviceId = devId;
         foundDevice = true;
         }
      }
   if (tracing > 1)
      {
      if (foundDevice)
         {
         TR_VerboseLog::writeLine(TR_Vlog_GPU, "[%16p] Selected GPUdevice=%3d, GPUutilization=%4d, freemem=%lld", vmThread, deviceId, gpuUtilization, *availableMemory);
         }
      else
         {
         TR_VerboseLog::writeLine(TR_Vlog_GPU, "[%16p] GPUdevice is not selected due to high utilization", vmThread);
         }
      }
   
   return foundDevice ? deviceId : -1; 
   }

static CudaInfo *
initGPUData(int trace, int ptxSourceID, uint8_t *startPC, bool enforceGPU)
   {
   J9VMThread *vmThread = jitConfig->javaVM->internalVMFunctions->currentVMThread(jitConfig->javaVM);
   GpuMetaData* gpuMetaData = getGPUMetaData(startPC);

   char *methodSignature = gpuMetaData->methodSignature;
   int lineNumber = (gpuMetaData->lineNumberArray)[ptxSourceID];
   
   int deviceId = 0;  //default device number if multiple GPU is disabled or unavailable
   unsigned long long availableMemory = 0;
   static bool disableMultipleGPU = (feGetEnv("TR_disableMultipleGPU") ? true : false) || !enableMultiGPU;
   if (!disableMultipleGPU)
      {
      deviceId = selectGPUDevice(vmThread, trace, &availableMemory, enforceGPU);
      if (deviceId < 0)
         {
         if (trace > 0)
            {
            TR_VerboseLog::writeLine(TR_Vlog_GPU, "[time.ms=%lld]: Redirected parallel forEach in %s at line %d to CPU due to high GPU utilization", (int64_t)(currentTime()/1000), methodSignature, lineNumber);
            }
         return NULL;
         }
      }

   char *ptxSource = (gpuMetaData->ptxArray)[ptxSourceID];
   CudaInfo * info = initCUDA(vmThread, trace, ptxSource, deviceId, ptxSourceID, gpuMetaData);

   if (trace > 0 && info == NULL)
      {
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "[time.ms=%lld]: Redirected parallel forEach in %s at line %d to CPU", (int64_t)(currentTime()/1000), methodSignature, lineNumber);
      }
   
   if (!disableMultipleGPU && (info != NULL))
      {
      info->availableMemory = availableMemory;
      }

   return info;
   }

extern "C" CudaInfo * 
regionEntryGPU(int trace, int ptxSourceID, uint8_t *startPC, TR::CodeGenerator::GPUScopeType regionType, bool enforceGPU)
   {
   if (trace > 1)
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tEntered regionEntryGPU method");

   CudaInfo * cudaInfo = initGPUData(trace, ptxSourceID, startPC, enforceGPU);

   if (cudaInfo)
      {
      cudaInfo->regionType = regionType;
      }

   if (trace > 1)
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tCreated GPU scope handle (%p)",cudaInfo);

   return cudaInfo;
   }

static int
copyGPUtoHost(CudaInfo *cudaInfo, void **hostRef, CUdeviceptr deviceArray, int32_t elementSize, int32_t isNoCopyDtoH,
              void *startAddress, void *endAddress, bool forceCopy);

int flushGPUDatatoCPU(CudaInfo *cudaInfo)
   {
   returnOnTrue(!cudaInfo,1);
   returnOnTrue(!(cudaInfo->hashTable),1);

   TR_ASSERT(cudaInfo, "cudaInfo should not be NULL");

   //////////////////////////////////////////////////////////////////////
   // Copy back from GPU all data that is CPU stale 
   //////////////////////////////////////////////////////////////////////
   
   for (int32_t i = 0; i < cudaInfo->hashSize; i++)
      {
      if (cudaInfo->hashTable[i].hostRef != 0 
          && cudaInfo->hashTable[i].hostRef != (void **)DELETED_HOSTREF 
          && cudaInfo->hashTable[i].deviceArray != NULL_DEVICE_PTR
          && cudaInfo->hashTable[i].deviceArray != BAD_DEVICE_PTR
          && cudaInfo->hashTable[i].valid)
         {
         cudaInfo->hashTable[i].valid = false; // invalidate GPU data
         
         void *startAddress = ((char*)*(cudaInfo->hashTable[i].hostRef)) + cudaInfo->hashTable[i].cpOutStartAddressOffset;
         void *endAddress   = ((char*)*(cudaInfo->hashTable[i].hostRef)) + cudaInfo->hashTable[i].cpOutEndAddressOffset;
         
         copyGPUtoHost(cudaInfo,
                       cudaInfo->hashTable[i].hostRef,
                       cudaInfo->hashTable[i].cpOutdevArray,
                       cudaInfo->hashTable[i].cpOutElementSize,
                       cudaInfo->hashTable[i].cpOutIsNoCopyDtoH,
                       startAddress,
                       endAddress,
                       true); // force copy
         }
      }

   return 0; 
   }

int resetAccessModeFlags(CudaInfo *cudaInfo)
   {
   returnOnTrue(!cudaInfo,1);

   TR_ASSERT(cudaInfo, "cudaInfo should not be NULL");

   //////////////////////////////////////////////////////////////////////
   //////////////////////////////////////////////////////////////////////
   
   for (int32_t i = 0; i < cudaInfo->hashSize; i++)
      {
      if (cudaInfo->hashTable[i].hostRef != 0 
          && cudaInfo->hashTable[i].hostRef != (void **)DELETED_HOSTREF 
          && cudaInfo->hashTable[i].deviceArray != NULL_DEVICE_PTR
          && cudaInfo->hashTable[i].deviceArray != BAD_DEVICE_PTR)
         {
         cudaInfo->hashTable[i].accessMode = TR::CodeGenerator::None;
         }
      }

   return 0; 
   }

bool freeGPUScope(CudaInfo *cudaInfo)
   {
   returnOnTrue(!cudaInfo,1);

   int tracing = cudaInfo->tracing;

   // free all GPU memory
   for (int32_t i = 0; i < cudaInfo->hashSize; i++)
      {
      if (cudaInfo->hashTable[i].hostRef != 0 
          && cudaInfo->hashTable[i].hostRef != (void **)DELETED_HOSTREF 
          && cudaInfo->hashTable[i].deviceArray != NULL_DEVICE_PTR
          && cudaInfo->hashTable[i].deviceArray != BAD_DEVICE_PTR)
         {
         if (tracing > 1)
            {
            TR_VerboseLog::writeLine(TR_Vlog_GPU,"\tFree GPU memory. Host: %p, Device: %p (%p)", *(cudaInfo->hashTable[i].hostRef), cudaInfo->hashTable[i].cpOutdevArray, cudaInfo);
            }

         // TODO: add error handling here
         CUresult res = jitCuMemFree(cudaInfo->hashTable[i].cpOutdevArray);
         
         if (res != CUDA_SUCCESS && tracing > 1)
            {
            TR_VerboseLog::writeLine(TR_Vlog_GPU,"\terror de-allocating GPU memory. Host: %p, Device: %p (%p)", *(cudaInfo->hashTable[i].hostRef), cudaInfo->hashTable[i].cpOutdevArray, cudaInfo);
            }
         }
      }
   
   // free GPU memory used for exception info
   if (cudaInfo->exceptionPointer != NULL_DEVICE_PTR)
      {
      if (tracing > 1)
         {
         TR_VerboseLog::writeLine(TR_Vlog_GPU,"\tFree GPU Exception memory %p (%p)", cudaInfo->exceptionPointer, cudaInfo);
         }
      
      CUresult res = jitCuMemFree(cudaInfo->exceptionPointer);

      if (res != CUDA_SUCCESS && tracing > 1)
         {
         TR_VerboseLog::writeLine(TR_Vlog_GPU,"\terror de-allocating GPU Exception memory %p (%p)", cudaInfo->exceptionPointer, cudaInfo);
         }
      }

   //////////////////////////////////////////////////////////////////////
   // Free hash table and cudaInfo handle
   //////////////////////////////////////////////////////////////////////

   bool failedGPUSession = isGPUFailed(cudaInfo);

   if (cudaInfo)
      {
      if (cudaInfo->hashTable)
         {
         TR_Memory::jitPersistentFree(cudaInfo->hashTable);
         }
      TR_Memory::jitPersistentFree(cudaInfo);
      }

   return failedGPUSession;
   }

extern "C" int
regionExitGPU(CudaInfo *cudaInfo, uint8_t *startPC, int ptxSourceID, void **liveRef)
   {
   returnOnTrue(!cudaInfo, 1);

   TR_ASSERT(cudaInfo, "cudaInfo should not be NULL");

   int tracing = cudaInfo->tracing;
   bool failedGPUSession = isGPUFailed(cudaInfo);

   if (tracing > 1)
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tEntered regionExitGPU method (%p) - %p", cudaInfo, liveRef);

   // if the GPU region did not have any errors, flush GPU data to the CPU
   if (!failedGPUSession)
      {
      flushGPUDatatoCPU(cudaInfo);
      }
   
   /*
    * In the naturalLoopScope case, regionExitGPU is the last call that uses cudaInfo.
    * cudaInfo and the allocated GPU device arrays can be freed at this point.
    */
   if (cudaInfo->regionType == TR::CodeGenerator::naturalLoopScope)
      {
      freeGPUScope(cudaInfo);
      }

   return (failedGPUSession ? 1 : 0);
   }

extern "C" int flushGPU(CudaInfo *cudaInfo, int blockId) 
   {
   returnOnTrue(!cudaInfo, 1);

   TR_ASSERT(cudaInfo, "cudaInfo should not be NULL");

   int tracing = cudaInfo->tracing;

   if (tracing > 1)
     TR_VerboseLog::writeLine(TR_Vlog_GPU, "Flushing GPU at block %d (%p)", blockId, cudaInfo);

   flushGPUDatatoCPU(cudaInfo);
   return 0;
   }

extern "C" CUdeviceptr
copyToGPU(CudaInfo *cudaInfo, void **hostRef, int32_t elementSize, int32_t accessMode, int32_t isNoCopyHtoD, int32_t isNoCopyDtoH, 
          void *startAddressHtoD, void *endAddressHtoD, void *startAddressDtoH, void *endAddressDtoH)
   {
   returnOnTrue(!cudaInfo,NULL_DEVICE_PTR);
   returnOnTrue(isGPUFailed(cudaInfo),NULL_DEVICE_PTR);

   TR_ASSERT(cudaInfo, "cudaInfo should not be NULL");

   int tracing = cudaInfo->tracing;

   if (hostRef == NULL) 
      {
      returnOnTrue(captureJITError("copyToGPU - host array pointer is NULL", cudaInfo, tracing > 1), NULL_DEVICE_PTR);
      }

   if ((*hostRef) == NULL)
      {
      returnOnTrue(captureJITError("copyToGPU - host array is NULL", cudaInfo, tracing > 1), NULL_DEVICE_PTR);
      }

   CUdeviceptr deviceArray = NULL_DEVICE_PTR;
   uint32_t hArrayAccessMode = TR::CodeGenerator::None;
   double time;

   int32_t arrayLength = *(int*)((char *)*hostRef + OBJECT_ARRAY_LENGTH_OFFSET);
   static bool isalign = feGetEnv("TR_disableGPUBufferAlign") ? false : true;
   static bool isPinned = feGetEnv("TR_enableGPUPinnedHeap") ? true : false;
   static bool disableAsyncOps = feGetEnv("TR_disableGPUAsyncOps") ? true : false;

   time = currentTime();
   HashEntry* parm_entry = cudaInfoHashGet(cudaInfo, hostRef);
   bool doAllocate = false;
   bool doCopy = false;

   if (parm_entry == NULL) 
      {
      doAllocate = true;
      doCopy = true;
      }
   else
      {
      if (parm_entry->accessMode == TR::CodeGenerator::None)
         parm_entry->accessMode = accessMode;

      hArrayAccessMode = parm_entry->accessMode;
      deviceArray = parm_entry->deviceArray;

      if (!parm_entry->valid) doCopy = true;
      }

   J9VMThread *vmThread = jitConfig->javaVM->internalVMFunctions->currentVMThread(jitConfig->javaVM);
   ThreadGPUInfo *threadGPUInfo = getThreadGPUInfo(vmThread);
   TR_ASSERT(threadGPUInfo, "threadGPUInfo should not be NULL at copyToGPU()");
   cudaStream_t stream = threadGPUInfo->stream[cudaInfo->device];

   size_t transNumBytes = 0, numPinnedBytes = 0, offset = 0;
   if (startAddressHtoD == 0)
      {
      transNumBytes = (isNoCopyHtoD == 0 ? (size_t)arrayLength*elementSize : 0) + OBJECT_HEADER_SIZE;
      numPinnedBytes = transNumBytes;
      offset = 0;
      }
   else
      {
      transNumBytes = ((char*)endAddressHtoD - (char*)startAddressHtoD) + OBJECT_HEADER_SIZE;
      numPinnedBytes = (size_t)arrayLength*elementSize + OBJECT_HEADER_SIZE; //TODO: change to partial array in future
      offset = ((char*)startAddressHtoD - (char*)*hostRef) - OBJECT_HEADER_SIZE;
      }

   if (doAllocate) 
      {
      size_t arrayNumBytes = (size_t)arrayLength*elementSize + OBJECT_HEADER_SIZE;
      size_t allocNumBytes = arrayNumBytes + HEADER_PADDING;

      //allocate memory on the device
      returnOnTrue(captureCuError(jitCuMemAlloc(&deviceArray, allocNumBytes), cudaInfo, "copyToGPU - jitCuMemAlloc"), BAD_DEVICE_PTR);

      if (tracing > 1)
         TR_VerboseLog::writeLine(TR_Vlog_GPU,"\tallocated %d bytes at %p (%p)",allocNumBytes,deviceArray,cudaInfo);

      if (isPinned)
         {
         CUresult result = jitCuMemHostRegister(*hostRef, numPinnedBytes, CU_MEMHOSTREGISTER_PORTABLE);
         if (result == CUDA_SUCCESS)
            accessMode |= ACCESS_PINNED;
         else
            {
            if (tracing > 0) TR_VerboseLog::writeLine(TR_Vlog_GPU, "FAILED with CUDA error : CUresult = %d (%p)", result, cudaInfo);
            jitCuMemFree(deviceArray);
            returnOnTrue(captureCuError(result, cudaInfo, "copyToGPU - jitCuMemHostRegister"), BAD_DEVICE_PTR);
            }
         }

      parm_entry = cudaInfoHashPut(cudaInfo, hostRef, elementSize, deviceArray, accessMode); //TODO: hostRef might be invalid here without VM access
      if (parm_entry == NULL)
         {
         if (!disableAsyncOps) jitCudaStreamSynchronize(stream);
         jitCuMemFree(deviceArray);

         if (tracing > 0)
            TR_VerboseLog::writeLine(TR_Vlog_GPU, "\thashing error");

         returnOnTrue(captureJITError("copyToGPU - Error inserting hash", cudaInfo, tracing > 1), BAD_DEVICE_PTR);
         }
      }

   parm_entry->cpOutdevArray      = deviceArray;
   parm_entry->cpOutElementSize   = elementSize;
   parm_entry->cpOutIsNoCopyDtoH &= isNoCopyDtoH; //clears the setting if the array needs to be copied back
   
   //NULL start/end address is used to indicate that the entire array should be copied. Offsets are set to
   //point to the start and end of the array data in this case
   if (!startAddressDtoH || !endAddressDtoH)
      {
      parm_entry->cpOutStartAddressOffset = OBJECT_HEADER_SIZE;
      parm_entry->cpOutEndAddressOffset   = OBJECT_HEADER_SIZE + arrayLength*elementSize;
      }
   else
      {
      parm_entry->cpOutStartAddressOffset = (size_t)((char *)startAddressDtoH - (char *)*hostRef);
      parm_entry->cpOutEndAddressOffset   = (size_t)((char *)endAddressDtoH - (char *)*hostRef);
      }

   if (doCopy)
      {
      if (tracing > 1) 
         TR_VerboseLog::writeLine(TR_Vlog_GPU,"\tperforming a copy to GPU %d bytes: %p -> %p (%p)",transNumBytes,*hostRef,deviceArray,cudaInfo);

      CUresult result;
      if (offset == 0)
         {
         // copy header + array body
         result = !disableAsyncOps ? jitCuMemcpyHtoDAsync(deviceArray+(isalign ? HEADER_PADDING : 0), (char *)*hostRef, transNumBytes, stream)
                                   : jitCuMemcpyHtoD(deviceArray+(isalign ? HEADER_PADDING : 0), (char *)*hostRef, transNumBytes);
         }
      else
         {
         // copy header
         result = !disableAsyncOps ? jitCuMemcpyHtoDAsync(deviceArray+(isalign ? HEADER_PADDING : 0), (char *)*hostRef, OBJECT_HEADER_SIZE, stream)
                                   : jitCuMemcpyHtoD(deviceArray+(isalign ? HEADER_PADDING : 0), (char *)*hostRef, OBJECT_HEADER_SIZE);
         // copy array body
         if (result == CUDA_SUCCESS)
            result = !disableAsyncOps ? jitCuMemcpyHtoDAsync(deviceArray+(isalign ? HEADER_PADDING : 0)+OBJECT_HEADER_SIZE+offset,
                                                             (char *)*hostRef + OBJECT_HEADER_SIZE + offset, transNumBytes - OBJECT_HEADER_SIZE, stream)
                                      : jitCuMemcpyHtoD(deviceArray+(isalign ? HEADER_PADDING : 0)+OBJECT_HEADER_SIZE+offset,
                                                        (char *)*hostRef + OBJECT_HEADER_SIZE + offset, transNumBytes - OBJECT_HEADER_SIZE);
         }
      if (result != CUDA_SUCCESS)
         {
         if (!disableAsyncOps) jitCudaStreamSynchronize(stream);
         jitCuMemFree(deviceArray);
         }

      returnOnTrue(captureCuError(result, cudaInfo, "copyToGPU - jitCuMemcpyHtoD"), BAD_DEVICE_PTR);
      parm_entry->valid = true;
      }
   else
      {
      if (tracing > 1)
         TR_VerboseLog::writeLine(TR_Vlog_GPU,"\tNo copy to GPU (%d bytes: %p -> %p (%p))",transNumBytes,*hostRef,deviceArray,cudaInfo);

      if (((accessMode & TR::CodeGenerator::WriteAccess) && (hArrayAccessMode == TR::CodeGenerator::ReadAccess)) ||
          ((hArrayAccessMode & TR::CodeGenerator::WriteAccess) && (accessMode == TR::CodeGenerator::ReadAccess)))
         {
         if (tracing > 0)
            TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tnon-trivial array access");

         returnOnTrue(captureJITError("copyToGPU - non-trivial array access", cudaInfo, tracing > 1), BAD_DEVICE_PTR);   // TODO: better handling helper error conditions
         }
      }

#if 0
    if (tracing > 1)
       {
       if (transNumBytes != 0)
          TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tallocated GPU device ptr %p", deviceArray);

       if (isNoCopyHtoD != 0)
          TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tno copy to GPU(except header) since Java array %p is not read, device ptr %p: %.3f msec.", *hostRef, deviceArray, (currentTime() - time)/1000);
       else if (transNumBytes == 0)
          TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tno copy to GPU since Java array %p is already copied, device ptr %p: %.3f msec.", *hostRef, deviceArray, (currentTime() - time)/1000);
       else
          TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tcopied %lld bytes to GPU from Java %sarray %p, device ptr %p: %.3f msec.", (int64_t)transNumBytes, (isPinned && (accessMode & ACCESS_PINNED)) ? "(Pinned) ":"", *hostRef, deviceArray, (currentTime() - time)/1000);
       }
#endif

   return deviceArray;
   }

static int
copyGPUtoHost(CudaInfo *cudaInfo, void **hostRef, CUdeviceptr deviceArray, int32_t elementSize, int32_t isNoCopyDtoH,
            void *startAddress, void *endAddress, bool forceCopy)
   {
   returnOnTrue(!cudaInfo, 0);

   // if forcing the copy, ignore error state
   if (!forceCopy)
      returnOnTrue(isGPUFailed(cudaInfo), 0);

   int tracing = cudaInfo->tracing;

   if (hostRef == NULL) 
      {
      if (tracing > 1)
         TR_VerboseLog::writeLine(TR_Vlog_GPU, "\thost array is NULL");

      returnOnTrue(captureJITError("copyGPUtoHost - host array pointer is NULL", cudaInfo, tracing > 1), 0);   // TODO: better handling helper error conditions
      }

   if ((*hostRef) == NULL) 
      {
      if (tracing > 1)
         TR_VerboseLog::writeLine(TR_Vlog_GPU, "\thost array is NULL");

      returnOnTrue(captureJITError("copyGPUtoHost - host array is NULL", cudaInfo, tracing > 1), 0);   // TODO: better handling helper error conditions
      }

   double time;
   size_t numBytes, offset;

   if (startAddress == 0)
      {
      int arrayLength = *(int*)((char *)*hostRef + OBJECT_ARRAY_LENGTH_OFFSET);
      numBytes = (size_t)arrayLength * (size_t)elementSize;
      offset = OBJECT_HEADER_SIZE;
      }
   else
      {
      numBytes = (char*)endAddress - (char*)startAddress;
      offset = (char*)startAddress - (char*)*hostRef;
      }

   static bool isalign = feGetEnv("TR_disableGPUBufferAlign") ? false : true;
   static bool disableAsyncOps = feGetEnv("TR_disableGPUAsyncOps") ? true : false;

   J9VMThread *vmThread = jitConfig->javaVM->internalVMFunctions->currentVMThread(jitConfig->javaVM);
   cudaStream_t stream = getThreadGPUInfo(vmThread)->stream[cudaInfo->device];

   time = currentTime();

   HashEntry* parm_entry = cudaInfoHashGet(cudaInfo, hostRef);

   TR_ASSERT(parm_entry, "parm_entry is NULL");

   if (parm_entry == NULL)
      {
      returnOnTrue(captureJITError("copyGPUtoHost - hashtable entry doesn't exist", cudaInfo, tracing > 1), 0);
      }

   CUdeviceptr deviceArrayInHash = parm_entry->deviceArray;
   bool isPinned = parm_entry->accessMode & ACCESS_PINNED;

   // do copy only if (entry not valid OR overridden by forceCopy) AND data is modified in the GPU
   bool doCopy = ((!parm_entry->valid || forceCopy) && isNoCopyDtoH == 0)                       
                 && (deviceArrayInHash != NULL_DEVICE_PTR) && (deviceArrayInHash != BAD_DEVICE_PTR); // only when pointers are valid

   if (doCopy) 
      {
      if (tracing > 1)
         TR_VerboseLog::writeLine(TR_Vlog_GPU,"\tCopy out %d bytes: %p -> %p",numBytes,deviceArray + offset + (isalign ? HEADER_PADDING : 0),(char*)*hostRef + offset);

      CUresult result = 
         !disableAsyncOps ? jitCuMemcpyDtoHAsync((char*)*hostRef + offset, deviceArray + offset + (isalign ? HEADER_PADDING : 0), numBytes, stream)
                               : jitCuMemcpyDtoH((char*)*hostRef + offset, deviceArray + offset + (isalign ? HEADER_PADDING : 0), numBytes);
      returnOnTrue(captureCuError(result, cudaInfo, "copyGPUtoHost - jitCuMemcpyDtoH"), TR::CodeGenerator::GPUHelperError);
      
      parm_entry->cpOutIsNoCopyDtoH = 1; //resets flag. array was copied back and does not need to be copied back again until this flag is cleared.

      if (isPinned)
         {
         CUresult result = jitCuMemHostUnregister(*hostRef);
         if (result != CUDA_SUCCESS)
            {
            if (tracing > 1) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tFailed to unpin memory : CUresult = %d", result);
            }
         }
      }

   if (tracing > 1)
      {
      if (!disableAsyncOps)
         jitCudaDeviceSynchronize();
      else
         jitCudaStreamSynchronize(stream);

      if (doCopy && !forceCopy)
         TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tcopied %lld bytes from GPU to Java %sarray %p, device ptr %p : %.3f msec.", (int64_t)numBytes, isPinned ? "(Pinned) ":"", *hostRef, deviceArray, (currentTime() - time)/1000);

      if (doCopy && forceCopy)
         TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tflushed %lld bytes from GPU to Java %sarray %p, device ptr %p : %.3f msec.", (int64_t)numBytes, isPinned ? "(Pinned) ":"", *hostRef, deviceArray, (currentTime() - time)/1000);

      if (!doCopy && isNoCopyDtoH != 0)
         TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tno copy from GPU since Java array %p is not updated, device ptr %p: %.3f msec.", *hostRef, deviceArray, (currentTime() - time)/1000);

      if (!doCopy && parm_entry->valid)
         TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tno copy from GPU since host Java array %p is valid, device ptr %p : %.3f msec.", *hostRef, deviceArray, (currentTime() - time)/1000);

      }

   return 0;
   }

extern "C" int
copyFromGPU(CudaInfo *cudaInfo, void **hostRef, CUdeviceptr deviceArray, int32_t elementSize, int32_t isNoCopyDtoH,
            void *startAddress, void *endAddress)
   {
   returnOnTrue(!cudaInfo, 0);
   returnOnTrue(isGPUFailed(cudaInfo), 0);

   return copyGPUtoHost(cudaInfo,hostRef,deviceArray,elementSize,isNoCopyDtoH,startAddress,endAddress,true);
   }

extern "C" CudaInfo *
invalidateGPU(CudaInfo *cudaInfo, void **hostRef) 
   {
   returnOnTrue(!cudaInfo,NULL);
   returnOnTrue(isGPUFailed(cudaInfo),NULL);

   TR_ASSERT(cudaInfo, "cudaInfo should not be NULL");

   int tracing = cudaInfo->tracing;

   if ((*hostRef) == NULL)
      {
      if (tracing > 1)
         TR_VerboseLog::writeLine(TR_Vlog_GPU, "\thost array is NULL");
      return NULL;
      }

   HashEntry* parm_entry = cudaInfoHashGet(cudaInfo, hostRef);

   // if entry exists, invalidate it       
   if (parm_entry != NULL)
      {
      parm_entry->valid = false;
      }
   
   return cudaInfo;
   }

extern "C" int
getStateGPU(CudaInfo *cudaInfo, uint8_t *startPC)
   {
   // TODO: pass tracing level outside cudaInfo so can print if required
   returnOnTrue(!cudaInfo,GPU_EXEC_FAIL_RECOVERABLE); 

   int tracing = cudaInfo->tracing;
   int returnValue = GPU_EXEC_SUCCESS;

   if (isGPUFailed(cudaInfo)) 
      {
      if (tracing > 1)
         TR_VerboseLog::writeLine(TR_Vlog_GPU,"\tgetStateGPU called: session failure-recoverable (%p)",cudaInfo);

      if (tracing > 0)
         {
         GpuMetaData* gpuMetaData = getGPUMetaData(startPC);
         char *methodSignature = gpuMetaData->methodSignature;

         if (cudaInfo->ffdcCudaError != cudaSuccess)
            {
            const char* cudaRuntimeErrorMsg = jitCudaGetErrorString(cudaInfo->ffdcCudaError);
            TR_VerboseLog::writeLine(TR_Vlog_GPU, "[time.ms=%lld]: Redirected parallel forEach in %s to CPU due to CUDA Runtime error %d: %s", (int64_t)(currentTime()/1000), methodSignature, cudaInfo->ffdcCudaError, cudaRuntimeErrorMsg);
            }
         else if (cudaInfo->ffdcCuError != CUDA_SUCCESS)
            {
            const char* cudaDriverErrorMsg = 0;
            if (jitCuGetErrorString(cudaInfo->ffdcCuError, &cudaDriverErrorMsg) == CUDA_SUCCESS)
               TR_VerboseLog::writeLine(TR_Vlog_GPU, "[time.ms=%lld]: Redirected parallel forEach in %s to CPU due to CUDA Driver error %d: %s", (int64_t)(currentTime()/1000), methodSignature, cudaInfo->ffdcCuError, cudaDriverErrorMsg);
            else
               TR_VerboseLog::writeLine(TR_Vlog_GPU, "[time.ms=%lld]: Redirected parallel forEach in %s to CPU due to CUDA Driver error %d: unrecognized error code", (int64_t)(currentTime()/1000), methodSignature, cudaInfo->ffdcCuError);
            }
         else //non-nvidia library failures like running out of persistent memory
            {
            TR_VerboseLog::writeLine(TR_Vlog_GPU, "[time.ms=%lld]: Redirected parallel forEach in %s to CPU due to JIT GPU error", (int64_t)(currentTime()/1000), methodSignature);
            }
         }

      returnValue = GPU_EXEC_FAIL_RECOVERABLE;
      }
   else
      {
      if (tracing > 1)
         TR_VerboseLog::writeLine(TR_Vlog_GPU,"\tgetStateGPU called: session successful (%p)",cudaInfo);
      
      // returnValue will keep the default of GPU_EXEC_SUCCESS in this case.
      }

   /*
    * In the singleKernelScope case, getStateGPU is the last call that uses cudaInfo.
    * cudaInfo and the allocated GPU device arrays can be freed at this point.
    */
   if (cudaInfo->regionType == TR::CodeGenerator::singleKernelScope)
      {
      freeGPUScope(cudaInfo);
      }

   return returnValue;
   }


// NUMEXTRAPARMS is set to 3. 2 extra parms for loop range and 1 extra parm is for exceptionKind in launchKernel.
#define NUMEXTRAPARMS    3

extern "C" void** allocateGPUKernelParms(int trace, int argCount)
   {
   //allocate space for the GPU parameters along with space for the extra parameters
   void **args = (void **)TR_Memory::jitPersistentAlloc(sizeof(void *)*(argCount + NUMEXTRAPARMS), TR_Memory::GPUHelpers);

   if (!args)
      {
      if (trace > 1) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tFAILED to allocate persistent memory for GPU args array.");
      return 0;
      }
   else
      {
      if (trace > 1) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tAllocated %d parms + %d parms for GPU args array",argCount,NUMEXTRAPARMS);
      }

   return args;
   }

extern "C" int launchGPUKernel(CudaInfo *cudaInfo, int startInclusive, int endExclusive, int argCount, void **args, int kernelId, uint8_t *startPC, int hasExceptionChecks)
   {
   if (!cudaInfo)
      {
      if (args) TR_Memory::jitPersistentFree(args);
      return TR::CodeGenerator::GPULaunchError;
      }
   
   returnOnTrue(isGPUFailed(cudaInfo), 0);

   double time;
   TR::CodeGenerator::GPUResult result;
   int tracing = cudaInfo->tracing;
   long range = (long)endExclusive - (long)startInclusive;

   if (!args)
      {
      if (tracing > 1) TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tFAILED to launch GPU kernel. GPU args array was not initialized.");
      returnOnTrue(captureJITError("launchGPUKernel - GPU args not initialized", cudaInfo, tracing > 1), TR::CodeGenerator::GPUHelperError);
      }

   if (tracing > 1)
      {
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tlaunchGPUKernel: start=%d end=%d range=%lld argCount=%d", startInclusive, endExclusive, range, argCount);
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tlaunchGPUKernel: kernelID=%d startPC=%p", kernelId, startPC); 
      }

   if (range <= 0)
      {
      TR_Memory::jitPersistentFree(args);
      return TR::CodeGenerator::GPUHelperError;
      }

   TR_ASSERT(range > 0, "expected positive range but got %lld\n", range);

   if (!loadKernel(cudaInfo, getGPUMetaData(startPC), kernelId, cudaInfo->device))
      {
      returnOnTrue(captureJITError("launchGPUKernel - loadKernel failed", cudaInfo, tracing > 1), TR::CodeGenerator::GPUHelperError);
      }

   if (tracing > 0)
      {
      GpuMetaData* gpuMetaData = getGPUMetaData(startPC);
      char *methodSignature = gpuMetaData->methodSignature;
      int lineNumber = (gpuMetaData->lineNumberArray)[kernelId];

      if (tracing > 1)
         TR_VerboseLog::writeLine(TR_Vlog_GPU, "[time.ms=%lld]: Launching parallel forEach in %s at line %d on GPU %d", (int64_t)(currentTime()/1000), methodSignature, lineNumber, cudaInfo->device);
      else
         TR_VerboseLog::writeLine(TR_Vlog_GPU, "[time.ms=%lld]: Launching parallel forEach in %s at line %d on GPU", (int64_t)(currentTime()/1000), methodSignature, lineNumber);
      }

   args[argCount] = &startInclusive;
   args[argCount+1] = &endExclusive;

   int blockDimX = (range > 1024) ? 1024 : ((range + 31) / 32) * 32;
   int blockDimY = 1;
   int blockDimZ = 1;
   int gridDimX = (range % blockDimX) == 0 ? (range / blockDimX) : (range / blockDimX) + 1; 
   int gridDimY = 1;
   int gridDimZ = 1;

   argCount += NUMEXTRAPARMS;

   time = currentTime();
   result = launchKernel(tracing, cudaInfo, gridDimX, gridDimY, gridDimZ,
                         blockDimX, blockDimY, blockDimZ, argCount, false, args, kernelId, hasExceptionChecks);

   if (tracing > 1)
      {
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tlaunched Kernel: %.3f msec.", (currentTime() - time)/1000);
      TR_VerboseLog::writeLine(TR_Vlog_GPU, "\tresult = (%d)", result);
      }

   TR_Memory::jitPersistentFree(args);

   if (result != TR::CodeGenerator::GPUSuccess)
      {
      captureJITError("launchGPUKernel - launchKernel did not return GPUSuccess", cudaInfo, tracing > 1);  // TODO: better handling helper error conditions
      }

   // reset accessMode for all HashTable entries since launchKernel is completed
   resetAccessModeFlags(cudaInfo);


   if (tracing > 0)
      {
      GpuMetaData* gpuMetaData = getGPUMetaData(startPC);
      char *methodSignature = gpuMetaData->methodSignature;
      int lineNumber = (gpuMetaData->lineNumberArray)[kernelId];

      if (tracing > 1)
         TR_VerboseLog::writeLine(TR_Vlog_GPU, "[time.ms=%lld]: Finished parallel forEach in %s at line %d on GPU %d", (int64_t)(currentTime()/1000), methodSignature, lineNumber, cudaInfo->device);
      else
         TR_VerboseLog::writeLine(TR_Vlog_GPU, "[time.ms=%lld]: Finished parallel forEach in %s at line %d on GPU", (int64_t)(currentTime()/1000), methodSignature, lineNumber);
      }

   return result;
   }


I_32 jitCallGPU(J9VMThread *vmThread, J9Method *method,
                       char * programSource, jobject invokeObject, int deviceId,
                       I_32 gridDimX, I_32 gridDimY, I_32 gridDimZ,
                       I_32 blockDimX, I_32 blockDimY, I_32 blockDimZ,
                       I_32 argCount, void **args)
   {
   return ERROR_CODE;
   }

extern "C"
int callGPU(void *vmThread, void *method, char * programSource, void * invokeObject, int deviceId,
		 int gridDimX, int gridDimY, int gridDimZ,
		 int blockDimX, int blockDimY, int blockDimZ,
         int argCount, void **args)
   {
   return 0;
   }

#else  // no GPU

extern "C"
int callGPU(void *vmThread, void *method, char * programSource, void * invokeObject, int deviceId,
		    int gridDimX, int gridDimY, int gridDimZ,
		    int blockDimX, int blockDimY, int blockDimZ,
            int argCount, void **args)
   {
   return 0;
   }

extern "C" int estimateGPU () { return 0; }
extern "C" int invalidateGPU () { return 0; }
extern "C" int flushGPU () { return 0; }
extern "C" int copyToGPU () { return 0; }
extern "C" int copyFromGPU () { return 0; }
extern "C" void** allocateGPUKernelParms () { return 0; }
extern "C" int launchGPUKernel () { return 0; }
extern "C" int regionEntryGPU () { return 0; }
extern "C" int regionExitGPU () { return 0; }
extern "C" int getStateGPU () { return 0; }
#endif

