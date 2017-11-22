/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#ifndef CPUUTILIZATION_HPP
#define CPUUTILIZATION_HPP

#include <stdint.h>         // for int64_t
#include "jni.h"
#include "j9.h"
#include "j9port.h"
#include "env/TRMemory.hpp" // for persistent alloc
#include "il/DataTypes.hpp"

namespace TR { class PersistentInfo; }

// TODO: maybe move this to a global config?
#define INITIAL_USAGE (77) // assume relatively high usage at start
#define INITIAL_IDLE (100 - INITIAL_USAGE)
#define CPU_UTIL_ARRAY_DEFAULT_SIZE 12

//------------------------- class CpuUtilization ----------------------------
// Class that maintains info about CPU utilization for a given period of time
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//                               WARNING
//
// The values obtained by this class have been observed during testing to be
// inaccurate within 4% in either direction. That is, a usage value of 10%
// could actually be anywhere from 6% to 14%.
//
// This is both because integer arithmetic is used in calculations, and
// because the information returned by the portlib is slightly inconsistent
// since it is obtained from two separate calls (and the time reference comes
// from only one of the calls).
//---------------------------------------------------------------------------
class CpuUtilization
   {

public:

   TR_PERSISTENT_ALLOC(TR_Memory::PersistentInfo);

   CpuUtilization(J9JITConfig *jitConfig);

   typedef struct CpuUsageCircularBuffer
      {
      int64_t _timeStamp;
      int64_t _sampleSystemCpu;
      int64_t _sampleJvmCpu;
      } CpuUsageCircularBuffer;

   bool    isFunctional() const { return _isFunctional; }
   int32_t getCpuUsage() const { return _cpuUsage; }
   int32_t getVmCpuUsage() const { return _vmCpuUsage; }
   int32_t getAvgCpuUsage() const { return _avgCpuUsage; }
   int32_t getCpuIdle() const { return _avgCpuIdle; }
   int64_t getUptime() const { return _prevMachineUptime; } // in nanoseconds
   int64_t getLastMeasurementInterval() const { return _prevIntervalLength; } // in nanoseconds
   int64_t getVmTotalCpuTime() const { return _prevVmSysTime + _prevVmUserTime; }
   int32_t getCpuUtil(J9JITConfig *jitConfig, J9SysinfoCPUTime *machineCpuStats, j9thread_process_time_t *vmCpuStats);
   int32_t updateCpuUtil(J9JITConfig *jitConfig);
   void    disable() { _isFunctional = false;
                       _cpuUsage = _vmCpuUsage = _avgCpuUsage = _avgCpuIdle = -1;
                       disableCpuUsageCircularBuffer(); }

   // Circular Buffer related functions
   CpuUsageCircularBuffer *getCpuUsageCircularBuffer() const { return _cpuUsageCircularBuffer; }
   void     disableCpuUsageCircularBuffer() { _isCpuUsageCircularBufferFunctional = false; }
   int32_t  getCircularBufferIndex() const { return _cpuUsageCircularBufferIndex; }
   int32_t  getCircularBufferSize() const { return _cpuUsageCircularBufferSize; }
   bool     isCpuUsageCircularBufferFunctional() const { return (_isFunctional && _isCpuUsageCircularBufferFunctional); }
   int32_t  updateCpuUsageCircularBuffer(J9JITConfig *jitConfig);

private:

   int32_t _cpuUsage;    // percentage of used CPU on the whole machine for the last update interval
   int32_t _vmCpuUsage;  // percentage of used CPU by the VM for the last update interval
   int32_t _avgCpuUsage; // percentage of time each processor is used on average
   int32_t _avgCpuIdle;  // percentage of time each processor is idle on average

   int64_t _prevIntervalLength; // the duration (in ns) of the last update interval

   // values recorded at start of this update interval
   int64_t _prevMachineUptime;  // absolute value (in ns) of machine uptime
   int64_t _prevMachineCpuTime; // absolute value (in ns) of used CPU time on the machine
   int64_t _prevVmSysTime;      // absolute value (in ns) of time VM spent in kernel space
   int64_t _prevVmUserTime;     // absolute value (in ns) of time VM spent in user space

   CpuUsageCircularBuffer *_cpuUsageCircularBuffer;      // Circular buffer containing timestsamp, system cpu usage sample, and jvm cpu use sample
   int32_t                 _cpuUsageCircularBufferIndex; // Current index of the buffer; contains the oldest data. Subtract 1 to get the most recent data
   int32_t                 _cpuUsageCircularBufferSize;  // Size of the circular buffer

   bool _isFunctional;
   bool _isCpuUsageCircularBufferFunctional;

   }; // class CpuUtilization

// How to use it:
// IMPORTANT: The update() and getCpuTimeNow() routines MUST be called
// only by the thread for which we want to compute CPU utilization
// You may call the update() function as often as you want, but the update actually happens only so often.
// The update() function will return false if no update took place (too soon to be recomputed)
// Use getCpuTime() to find CPU spent in thread from the beginning of the JVM till the last update
// Use getCpuTimeNow() to find CPU spent in thread till this moment;
//       - heavyweight because it invokes OS
//       - must be called only by the thread for which we want CPu time
// Use getThreadLastCpuUtil() to print CPU utilization of the thread during the last interval (should be <= 100)

namespace TR { class CompilationInfo; }
namespace TR {
class PersistentInfo;
}
extern "C" {
struct J9JITConfig;
}
class CpuSelfThreadUtilization
   {
public:
   TR_PERSISTENT_ALLOC(TR_Memory::PersistentInfo);
   CpuSelfThreadUtilization(TR::PersistentInfo * persistentInfo, J9JITConfig * jitConfig, int64_t minPeriodNs, int32_t id);
   bool update();
   int64_t getCpuTime() const { return _cpuTimeAtLastUpdate; }
   int64_t getCpuTimeNow() const {PORT_ACCESS_FROM_JITCONFIG(_jitConfig); return j9thread_get_self_cpu_time(j9thread_self()); } // expensive
   int64_t getTimeOfLastUpdate() const { return _clockTimeAtLastUpdate; } // ns
   int64_t getLowResolutionClockAtLastUpdate() const { return _lowResolutionClockAtLastUpdate; } // ms
   int64_t getLastMeasurementInterval() const { return _lastIntervalLength; } // ns
   int64_t getSecondLastMeasurementInterval() const { return _secondLastIntervalLength; } // ns
   int64_t getCpuTimeDuringLastInterval() const { return _cpuTimeDuringLastInterval; }
   int64_t getCpuTimeDuringSecondLastInterval() const { return _cpuTimeDuringSecondLastInterval; }
   // getThreadLastCpuUtil() returns CPU utilization of thread as a percentage. Values greater than 100 are errors.
   int32_t getThreadLastCpuUtil() const { return _lastCpuUtil; }
   int32_t getThreadPrevCpuUtil() const { return _secondLastCpuUtil; }
   int32_t computeThreadCpuUtilOverLastNns(int64_t validInterval) const; // this one looks at the last two intervals, but not too distant in the past
   int64_t getCrtTimeNs() const { PORT_ACCESS_FROM_JITCONFIG(_jitConfig); return j9time_current_time_millis() * 1000000; /* j9time_nano_time() */}
   int32_t getId() const { return _id; }
   bool isFunctional() const { return _isFunctional; }
   void setAsUnfunctional();
   void printInfoToVlog() const;

private:
   J9JITConfig *_jitConfig;
   TR::PersistentInfo *_persistentInfo;

   int64_t _minMeasurementIntervalLength; // update only if at least that much time has passed since the last update
   int64_t _clockTimeAtLastUpdate; // time when stats for last interval were computed (ns)
   uint64_t _lowResolutionClockAtLastUpdate; // this is based on the fast but inaccurate time maintained in persistentinfo (ms)
   int64_t _cpuTimeAtLastUpdate; // cpu time spent in thread at the time of last update (since the start of the process)
   int64_t _cpuTimeDuringLastInterval; // cpu time spent in thread during the last measurement interval
   int64_t _lastIntervalLength; // duration of the last measurement interval
   int32_t _lastCpuUtil; // CPU utilization of the thread during the last interval

   int64_t _cpuTimeDuringSecondLastInterval; // cpu time spent in thread during the second last measurement interval (precedes last interval)
   int64_t _secondLastIntervalLength; // duration of the second last measurement interval
   int32_t _secondLastCpuUtil; // CPU utilization of the thread during the second last interval

   int32_t _id; // some identifier; e.g. compilation thread id
   bool    _isFunctional;
   }; // CpuSelfThreadUtilization


// Note, an object of this type is embedded into TR::CompilationInfo which is
// zeroed out at construction time. Thus, TR_CpuEntitlement cannot have virtual
// functions. If virtual functions are added to TR_CpuEntitlement then we need
// to allocate it dynamically and store a pointer into TR::CompilationInfo
struct TR_CpuEntitlement
   {
public:
   // The constructor cannot set all fields correctly because we have to make
   // sure te portlib is up and running
   void init(J9JITConfig *jitConfig)
      {
      /* Couple of issues were discovered when the support for hypervisor was enabled.
       * When JVM ran on VMWare, then the port library loaded libvmGuestLib.so which
       * interferes with implementation of some of j.l.Math methods.
       * The library also causes JVM to core dump when an application is using libjsig.so
       * for signal chaining.
       * For these reasons, support for hypervisor is being disabled until the issues with
       * VMWare library are resolved.
       */
      _hypervisorPresent = TR_no;
      _jitConfig = jitConfig;
      computeAndCacheCpuEntitlement();
      }
   bool isHypervisorPresent();
   void computeAndCacheCpuEntitlement(); // used during bootstrap and periodically in samplerThreadProc
   uint32_t getNumTargetCPUs()     const { return _numTargetCpu; }  // num CPUs the JVM is pinned to. Guaranteed >= 1
   double getGuestCpuEntitlement() const { return _guestCpuEntitlement; } // as given by the hypervisor; 0 if error or no hypervisor
   double getJvmCpuEntitlement()   const { return _jvmCpuEntitlement; } // smallest of _numTargetCpu and _guestCpuEntitlement

private:
   double computeGuestCpuEntitlement() const; // this does not check for isHypervisorPresent, so don't call it directly

   TR_YesNoMaybe _hypervisorPresent;
   uint32_t      _numTargetCpu;
   double        _guestCpuEntitlement;
   double        _jvmCpuEntitlement;
   J9JITConfig * _jitConfig;
   };

#endif // CPUUTILIZATION_HPP
