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

#include "control/CompilationRuntime.hpp"

#include <stdint.h>
#include "jni.h"
#include "j9.h"
#include "j9port.h"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CpuUtilization.hpp"

/*
 * Relevant port library API:
 * 
 * IDATA j9sysinfo_get_CPU_utilization(struct J9PortLibrary *portLibrary, struct J9SysinfoCPUTime *cpuTime)
 * 
 * typedef struct J9SysinfoCPUTime {
 *
 *    I_64 timestamp;    // time in nanoseconds from a fixed but arbitrary point in time
 *    I_64 cpuTime;      // cumulative CPU utilization (sum of system and user time in nanoseconds) of all CPUs on the system.
 *    I_32 numberOfCpus; // number of CPUs as reported by the operating system
 * 
 * } J9SysinfoCPUTime;
 *
 * I_64 j9thread_get_process_times(j9thread_process_time_t * processTime)
 * 
 * typedef struct j9thread_process_time_t {
 *
 *    // for consistency sake, times are stored as I_64s
 *    I_64 _systemTime; // system time used
 *    I_64 _userTime;   // non-system time used
 *
 * } j9thread_process_time_t;
 */

int32_t CpuUtilization::getCpuUtil(J9JITConfig *jitConfig, J9SysinfoCPUTime *machineCpuStats, j9thread_process_time_t *vmCpuStats)
   {  
   IDATA portLibraryStatusSys; // port lib call return value for system cpu usage
   IDATA portLibraryStatusVm;  // port lib call return value for vm cpu usage
   
   // get access to portlib
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);
   
   // get CPU stats for the machine
   portLibraryStatusSys = j9sysinfo_get_CPU_utilization(machineCpuStats);
   portLibraryStatusVm  = j9thread_get_process_times(vmCpuStats);
   
   // if either call failed, disable self
   if (portLibraryStatusSys < 0 || portLibraryStatusVm < 0)
      {
      disable();
      return (-1);
      }
   
   return 0;
   }

/*
 * Update the values stored inside the object. Only updates the values
 * when called more than _minIntervalLength milliseconds after the last
 * update.
 */
int CpuUtilization::updateCpuUtil(J9JITConfig *jitConfig)
   {
   // do nothing if turned off
   if (!_isFunctional)
      {
      return (-1);
      }
   
   // portlib data
   J9SysinfoCPUTime        machineCpuStats;      // stats for overall CPU usage on machine
   j9thread_process_time_t vmCpuStats;           // stats for VM's CPU usage
   
   if (getCpuUtil(jitConfig, &machineCpuStats, &vmCpuStats) == -1)
      return (-1);
   
   // calculate interval
   _prevIntervalLength = machineCpuStats.timestamp - _prevMachineUptime;
   
   // calculate usage and idle percentages
   if (_prevIntervalLength > 0)
      {
      int64_t prevTotalTimeUsedByVm = _prevVmSysTime + _prevVmUserTime;
      int64_t newTotalTimeUsedByVm = vmCpuStats._systemTime + vmCpuStats._userTime;
      
      _cpuUsage = (100 * (machineCpuStats.cpuTime - _prevMachineCpuTime)) / _prevIntervalLength;
      _vmCpuUsage = (100 * (newTotalTimeUsedByVm - prevTotalTimeUsedByVm)) / _prevIntervalLength;
      }
   
   if (machineCpuStats.numberOfCpus > 0)
      {
      _avgCpuUsage = _cpuUsage / machineCpuStats.numberOfCpus;
      }
   
   _avgCpuIdle = 100 - _avgCpuUsage;
   
   // remember values for next time
   _prevMachineUptime  = machineCpuStats.timestamp;
   _prevMachineCpuTime = machineCpuStats.cpuTime;
   _prevVmSysTime      = vmCpuStats._systemTime;
   _prevVmUserTime     = vmCpuStats._userTime;
   
   /*
   fprintf(stderr, "#CPUINFO: usage=%lld%%, vm-usage=%lld%%, avg-usage=%lld%%, avg-idle=%lld%%, interval=%lldms\n", 
      _cpuUsage, _vmCpuUsage, _avgCpuUsage, _avgCpuIdle, (_prevIntervalLength / 1000000ll));
   */
   
   return 0;
   } // updateCpuUtil

int32_t CpuUtilization::updateCpuUsageCircularBuffer(J9JITConfig *jitConfig)
   {
   // do nothing if turned off
   if (!_isFunctional || !_isCpuUsageCircularBufferFunctional)
      {
      return (-1);
      }
   
   // portlib data
   J9SysinfoCPUTime        machineCpuStats;      // stats for overall CPU usage on machine
   j9thread_process_time_t vmCpuStats;           // stats for VM's CPU usage
   
   if (getCpuUtil(jitConfig, &machineCpuStats, &vmCpuStats) == -1)
      return (-1);
    
   _cpuUsageCircularBuffer[_cpuUsageCircularBufferIndex]._timeStamp = machineCpuStats.timestamp;
   _cpuUsageCircularBuffer[_cpuUsageCircularBufferIndex]._sampleSystemCpu = machineCpuStats.cpuTime;
   _cpuUsageCircularBuffer[_cpuUsageCircularBufferIndex]._sampleJvmCpu = vmCpuStats._systemTime + vmCpuStats._userTime;
   
   _cpuUsageCircularBufferIndex = (_cpuUsageCircularBufferIndex + 1)%_cpuUsageCircularBufferSize;
   
   return 0;
   
   } // updateCpuUsageArray

CpuUtilization::CpuUtilization(J9JITConfig *jitConfig):

   // initialize usage to INITIAL_USAGE
   _cpuUsage    (INITIAL_USAGE),
   _vmCpuUsage  (INITIAL_USAGE),
   _avgCpuUsage (INITIAL_USAGE),
   _avgCpuIdle  (INITIAL_IDLE),
   
   _prevIntervalLength (0),
   
   _prevMachineUptime (0),
   _prevMachineCpuTime (0),
   _prevVmSysTime (0),
   _prevVmUserTime (0),
   
   _isFunctional (true),
   
   _cpuUsageCircularBufferIndex(0)
   
   {
   // If the circular buffer size is set to 0, disable the circular buffer
   if (TR::Options::_cpuUsageCircularBufferSize == 0)
      {
      _isCpuUsageCircularBufferFunctional = false;
      _cpuUsageCircularBuffer = NULL;
      return;
      }
   
   _isCpuUsageCircularBufferFunctional = true;
   
   // Enforce a minimum size
   if (TR::Options::_cpuUsageCircularBufferSize < CPU_UTIL_ARRAY_DEFAULT_SIZE)
      _cpuUsageCircularBufferSize = CPU_UTIL_ARRAY_DEFAULT_SIZE;
   else
      _cpuUsageCircularBufferSize = TR::Options::_cpuUsageCircularBufferSize;
   
   // Allocate the memory for the buffer
   _cpuUsageCircularBuffer = 
         (CpuUsageCircularBuffer *)TR_PersistentMemory::jitPersistentAlloc(sizeof(CpuUsageCircularBuffer)*_cpuUsageCircularBufferSize);   
   
   // Since this happens at bootstrap, if this fails, disable and return
   if (!_cpuUsageCircularBuffer)
      {
      _isCpuUsageCircularBufferFunctional = false;
      return;
      }
   
   // Initialize the buffer; no need to initialize the _sampleSystemCpu and _sampleJvmCpu
   // fields since we can just check if _timeStamp equals 0
   for (int32_t i = 0; i < _cpuUsageCircularBufferSize; i++)
      {
      _cpuUsageCircularBuffer[_cpuUsageCircularBufferIndex]._timeStamp = 0;
      }
   } // CpuUtilization


CpuSelfThreadUtilization::CpuSelfThreadUtilization(TR::PersistentInfo *persistentInfo, J9JITConfig *jitConfig, int64_t minPeriodNs, int32_t id)
      : _jitConfig(jitConfig),
        _persistentInfo(persistentInfo),
        _minMeasurementIntervalLength(minPeriodNs),
        _cpuTimeDuringLastInterval(-1), // Invalid value to avoid using it
        _lastIntervalLength(1), // 1 to avoid a division by 0
        _lastCpuUtil(-1), // Invalid value to avoid using it
        _cpuTimeDuringSecondLastInterval(-1), // Invalid value, so avoid using it
        _secondLastIntervalLength(1),
        _secondLastCpuUtil(-1), // Invalid value, so avoid using it
        _id(id),
        _isFunctional(true) // optimistic
   {
   _lowResolutionClockAtLastUpdate = _persistentInfo->getElapsedTime();
   _clockTimeAtLastUpdate = getCrtTimeNs(); 
   _cpuTimeAtLastUpdate   = 0; //getCpuTimeNow(); The constructor might not be called by the compilation thread
   } 

void CpuSelfThreadUtilization::setAsUnfunctional() 
   { 
   _isFunctional = false;
   // set an invalid value which sometimes may obviate the need to test _isFunctional
   _cpuTimeDuringLastInterval = _cpuTimeDuringSecondLastInterval = _cpuTimeAtLastUpdate = -1;
   _lastCpuUtil = _secondLastCpuUtil = - 1;
   // TODO: place a VM style trace point
   } 

bool CpuSelfThreadUtilization::update() // return true if an update was performed
   {
   if (!_isFunctional)
      return false;
   // Refuse to update if not enough time has passed; 
   // use the lower resolution time to avoid overhead
   if ((_persistentInfo->getElapsedTime() - _lowResolutionClockAtLastUpdate)*1000000  < _minMeasurementIntervalLength)
      return false;
   int64_t currentThreadCpuTime = getCpuTimeNow();
   if (currentThreadCpuTime < 0)
      {
      setAsUnfunctional(); // once we got a wrong value there is no point to try again
      return false;
      }
   int64_t crtTime = getCrtTimeNs();
   if (crtTime <= 0) // not likely, but let's make sure
      {
      setAsUnfunctional(); // once we got a wrong value there is no point to try again
      return false;
      }
   // Propagate the old values
   _secondLastIntervalLength        = _lastIntervalLength;
   _cpuTimeDuringSecondLastInterval = _cpuTimeDuringLastInterval;
   _secondLastCpuUtil               = _lastCpuUtil;

   // Sanity checks regarding new values
   //
   int64_t elapsedTime = crtTime - _clockTimeAtLastUpdate;
   int64_t elapsedCPU  = currentThreadCpuTime - _cpuTimeAtLastUpdate;
   int32_t cpuUtil;
   // If time goes backwards, the CPU utilization will look negative (to avoid using it)
   if (elapsedTime <= 0) // the equality test will also avoid division by zero later on
      {
      cpuUtil = -1;
      }
   else
      {
      // We cannot spend more than 100% on a thread
      // However, we must allow for small imprecisions in time and CPU bookkeeping
      // Thus, if CPU exceeds elapsed time by more than 10% set a value of -1 for cpuUtil
      if (elapsedCPU > elapsedTime) // unlikely case
         cpuUtil = (elapsedCPU > (elapsedTime * 11 / 10)) ? -1 : 100;
      else
         cpuUtil = (int32_t)(100 * elapsedCPU / elapsedTime);
      }
   // Store the new readouts
   _lowResolutionClockAtLastUpdate = _persistentInfo->getElapsedTime();
   _cpuTimeDuringLastInterval      = elapsedCPU;
   _lastIntervalLength             = elapsedTime; // If time goes backwards, the CPU utilization will look negative
   _lastCpuUtil                    = cpuUtil;
   _cpuTimeAtLastUpdate            = currentThreadCpuTime;
   _clockTimeAtLastUpdate          = crtTime;
   return true;
   }

//---------------------------- computeThreadCpuUtilOverLastNns ---------------------------
// Returns thread CPU utilization for the last two interval measurement periods
// only if these measurement periods are included in [crtTime-validInterval, crtTime]
// May return -1 in case of error
//------------------------------------------------------------------------------------
int32_t CpuSelfThreadUtilization::computeThreadCpuUtilOverLastNns(int64_t validInterval) const
   {
   int32_t cpuUtil = 0;
   if (_lastCpuUtil < 0)
      return -1; // error case
   // If the last readout happened too much in the past, it should not be included
   int64_t crtTimeNs = _persistentInfo->getElapsedTime() * 1000000;
   int64_t lastValidTimeNs = crtTimeNs - validInterval;
   int64_t lastIntervalEndNs = _lowResolutionClockAtLastUpdate * 1000000;
   int64_t lastIntervalStartNs = lastIntervalEndNs - _lastIntervalLength;
   if (lastIntervalStartNs < lastValidTimeNs)
      {
      return 0; // the thread may have accumulated some CPU cycles since the last readout, so 0 is an underestimate
      }
   else // I can include at least the last interval
      {
      // Account for the last metered interval
      int64_t totalCPU = _cpuTimeDuringLastInterval;
      int64_t totalTime = _lastIntervalLength;

     
      // The interval between crtTimeNs and lastIntervalEndNs is not accounted for; if this interval
      // is larger than the measurement period the thread might have gone to sleep and not
      // had a chance to update its CPU utilization. Blindly assume a 0% duty cycle
      if (crtTimeNs - lastIntervalEndNs > _minMeasurementIntervalLength)
         {
         totalTime += crtTimeNs - lastIntervalEndNs;
         }

      // Can I include the second last interval?
      if (_secondLastCpuUtil >= 0) // yes
         {
         if (lastIntervalStartNs - _secondLastIntervalLength >= lastValidTimeNs)
            {
            totalCPU += _cpuTimeDuringSecondLastInterval;
            totalTime += _secondLastIntervalLength;
            }
         }
      return  (int32_t)(100 * totalCPU / totalTime);
      }
   }

void CpuSelfThreadUtilization::printInfoToVlog() const
   {
   TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "t=%6u CPUid=%d: lastCheckPoint=%u, lastCpuUtil=%3d (interval=%d ms) prevCpuUtil=%3d (interval=%d ms)",
      (uint32_t)_persistentInfo->getElapsedTime(),
      getId(),
      getLowResolutionClockAtLastUpdate(),
      getThreadLastCpuUtil(),
      (int32_t)(getLastMeasurementInterval()/1000000),
      getThreadPrevCpuUtil(),
      (int32_t)(getSecondLastMeasurementInterval()/1000000));
   }


bool TR_CpuEntitlement::isHypervisorPresent()
   {
   if (_hypervisorPresent == TR_maybe)
      {
      // Caching works because we don't expect the information to change
      // However, we must call this after portlib is up and running
      PORT_ACCESS_FROM_JITCONFIG(_jitConfig);
      if (j9hypervisor_hypervisor_present() > 0) // J9HYPERVISOR_NOT_PRESENT/J9HYPERVISOR_PRESENT/J9PORT_ERROR_HYPERVISOR_UNSUPPORTED
         {
         _hypervisorPresent = TR_yes;
         }
      else
         {
         _hypervisorPresent = TR_no;
         }
      }
   return (_hypervisorPresent == TR_yes);
   }

double TR_CpuEntitlement::computeGuestCpuEntitlement() const
   {
   // Caller must use isHypervisorPresent first to make sure we are running on a supported hypervisor
   PORT_ACCESS_FROM_JITCONFIG(_jitConfig);
   J9GuestProcessorUsage guestProcUsage;
   if (0 == j9hypervisor_get_guest_processor_usage(&guestProcUsage))
      return guestProcUsage.cpuEntitlement * 100;
   else // error case
      return 0.0;
   }

void TR_CpuEntitlement::computeAndCacheCpuEntitlement()
   {
   PORT_ACCESS_FROM_JITCONFIG(_jitConfig);
   _numTargetCpu = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_TARGET);
   if (_numTargetCpu == 0)
      _numTargetCpu = 1; // some correction in case we get it wrong
   uint32_t numTargetCpuEntitlement = _numTargetCpu * 100;
   if (isHypervisorPresent())
      {
      _guestCpuEntitlement = computeGuestCpuEntitlement();
      // If the number of target CPUs is smaller (bind the JVM to a subset of CPUs), use that value
      if (numTargetCpuEntitlement < _guestCpuEntitlement || _guestCpuEntitlement <= 0)
         _jvmCpuEntitlement = numTargetCpuEntitlement;
      else
         _jvmCpuEntitlement = _guestCpuEntitlement;
      }
   else
      {
      _jvmCpuEntitlement = numTargetCpuEntitlement;
      }
   }

