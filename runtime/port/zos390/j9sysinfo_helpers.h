/*******************************************************************************
 * Copyright (c) 1991, 2013 IBM Corp. and others
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

#ifndef SYSINFO_HELPERS_H_
#define SYSINFO_HELPERS_H_

#include "j9port.h"
#include "omrsysinfo_helpers.h"

/* Is this a CAPPED LPAR? Returns boolean true, if yes; else false. */
#define J9LPAR_CAPPED(lpdatp)		(0x80 == lpdatp->flags)

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/**
 * Retrieve z/Architecture facility bits.
 *
 * @param [in]  lastDoubleWord   Size of the bits array in number of uint64_t, minus 1.
 * @param [out] bits             Caller-supplied uint64_t array that gets populated with the facility bits.
 *
 * @return The index of the last valid uint64_t in the bits array.
 */
extern int getstfle(int lastDoubleWord, uint64_t *bits);

typedef __packed struct J9LPDat {
	int32_t length; 					/**< 0:4 length of area */
	uint8_t version; 					/**< 4:1 version */
	uint8_t flags; 						/**< 5:1 flags */
	uint8_t reserved1[2]; 				/**< 6:2 reserved */
	int32_t cecCapacity; 				/**< 8:4 cec cpu capacity in MSUs per hour */
	uint8_t imgPartName[8]; 			/**< 12:8 logical partition name */
	int32_t imgCapacity; 				/**< 20:4 logical partition cpu capacity in MSUs per hour */
	int32_t phyCpuAdjFactor;			/**< 24:4 physical cpu adjustment factor */
	int32_t cumWeight;					/**< 28:4 cumulative weight of the image since IPL for the
									 * local partition
									 */
	int32_t weightAccumCounter; 		/**< 32:4 number of times the current weight is accumulated */
	int32_t avgImgService; 			/**< 36:4 long-term average cpu service used by this logical
									 * partition, in MSUs per hour
									 */
	uint64_t cumUncappedElapsedTime; 	/**< 40:8 Cumulative uncapped elapsed time, in microseconds */
	uint64_t cumCappedElapsedTime; 		/**< 48:8 Cumulative capped elapsed time, in microseconds */
	int32_t serviceTableEntryInterval; /**< 56:4 approximate time interval (in seconds) for each
									 * entry in the service table
									 */
	int32_t serviceTableOffset; 		/**< 60:4 offset to service table */
	int32_t serviceTableEntryLength; 	/**< 64:4 length of service table entries */
	int32_t serviceTableEntries; 		/**< 68:4 number of service entries in the service table */
	uint8_t capacityGroupName[8]; 		/**< 72:8 all partitions which have the same CapacityGroupName
									 * build the capacity group
									 */
	int32_t capacityGroupMSULimit; 	/**< 80:4 the group limit in million service units per hour */
	uint64_t groupJoinedTOD; 			/**< 84:8 timestamp when this lpar has joined its group */
	int32_t imgMSULimit; 				/**< 92:4 capacity in millions of service units per hour */
	uint8_t reserved2[8]; /* 96:8 reserved for future use */
} J9LPDat;

typedef __packed struct J9LPDatServiceTableEntry {
	int32_t serviceUncapped;			/**< 0:4 basic-mode service units accumulated while the partition
									 * was uncapped
									 */
	int32_t serviceUncappedTime;		/**< 4:4 elapsed time that the partition was uncapped, in
									 * 1.024 millisecond units
									 */
	int32_t serviceCapped;				/**< 8:4 basic-mode service units accumulated while the
									 * partition was capped
									 */
	int32_t serviceCappedTime;			/**< 12:4 elapsed time that the partition was capped, in
									 * 1.024 millisecond units
									 */
	int32_t serviceUnusedGroupCapacity;/**< 16:4 service units not consumed by members of the group */
} J9LPDatServiceTableEntry;

/**
 * Structure copied verbatim from the data set header "//'SYS1.SIEAHDR.H(IWMQVSH)'".
 * NB. This portion should be deleted from the current header once the compiler compiling
 * the sources is capable of including headers right out of data sets on Z/OS. For this
 * same reason, we are not replacing "C" data types with J9 data types (int32_t, uint64_t, etc).
 */
#pragma pack(packed)

struct qvs {
	struct {
		int            _qvslen;
	} qvsin;
	struct {
		char           _qvsver;
		int            _qvscecvalid          : 1,
					   _qvsimgvalid          : 1,
					   _qvsvmvalid           : 1,
											 : 5;
		unsigned char  _filler1;
		unsigned char  _qvsceccapacitystatus;
		unsigned char  _qvscecmachinetype[4];
		unsigned char  _qvscecmodelid[16];
		unsigned char  _qvscecsequencecode[16];
		unsigned char  _qvscecmanufacturername[16];
		unsigned char  _qvscecplantofmanufacture[4];
		int            _qvsceccapacity;
		unsigned char  _qvsimglogicalpartitionname[8];
		short int      _qvsimglogicalpartitionid;
		short int      _filler2;
		int            _qvsimgcapacity;
		unsigned char  _qvsvmname[8];
		int            _qvsvmcapacity;
	} qvsout;
};                                                      /* @ME17399*/

#define qvslen                     qvsin._qvslen
#define qvsver                     qvsout._qvsver
#define qvscecvalid                qvsout._qvscecvalid
#define qvsimgvalid                qvsout._qvsimgvalid
#define qvsvmvalid                 qvsout._qvsvmvalid
#define qvscecmachinetype          qvsout._qvscecmachinetype
#define qvscecmodelid              qvsout._qvscecmodelid
#define qvscecsequencecode         qvsout._qvscecsequencecode
#define qvscecmanufacturername     qvsout._qvscecmanufacturername
#define qvscecplantofmanufacture   qvsout._qvscecplantofmanufacture
#define qvsceccapacity             qvsout._qvsceccapacity
#define qvsimglogicalpartitionname qvsout._qvsimglogicalpartitionname
#define qvsimglogicalpartitionid   qvsout._qvsimglogicalpartitionid
#define qvsimgcapacity             qvsout._qvsimgcapacity
#define qvsvmname                  qvsout._qvsvmname
#define qvsvmcapacity              qvsout._qvsvmcapacity
#define qvsceccapacitystatus       qvsout._qvsceccapacitystatus /* @ME17399 */

#pragma pack(reset)

#ifdef __cplusplus
	extern "OS" ??<
#else
	#pragma linkage(iwmqvs_calltype,OS)
#endif

typedef int iwmqvs_calltype( struct qvs * );

#ifdef _LP64                                             /* @PYN0133 */
#   pragma map(iwmqvs,"IWMQVS4")                         /* @WLMP64V */
#else                                                    /* @WLMP64V */
#   pragma map(iwmqvs,"IWMQVS")                          /* @WLMP64V */
#endif                                                   /* @WLMP64V */

extern iwmqvs_calltype iwmqvs;

#ifdef __cplusplus
	??>
#endif

/**
* End of portion extracted from the header "//'SYS1.SIEAHDR.H(IWMQVSH)'".
*/

/**
 * Returns memory usage statistics of the Guest VM as seen by the hypervisor
 * Uses hypfs to get the usage details
 *
 * @param [in]  portLibrary The port Library
 * @param [out] gmUsage     Caller supplied J9GuestMemoryUsage structure
 *                          that gets filled in with usage statistics
 *
 * @return 0 on success, a negative value on failure
 */
intptr_t
retrieveZGuestMemoryStats(struct J9PortLibrary *portLibrary, struct J9GuestMemoryUsage *gmUsage);

/**
 * Returns processor usage statistics of the Guest VM as seen by the hypervisor
 * Uses hypfs to get the usage details
 *
 * @param [in]  portLibrary The port Library
 * @param [out] gpUsage     Caller supplied J9GuestProcessorUsage structure
 *                          that gets filled in with usage statistics
 *
 * @return 0 on success, a negative value on failure
 */
intptr_t
retrieveZGuestProcessorStats(struct J9PortLibrary *portLibrary, struct J9GuestProcessorUsage *gpUsage);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#pragma linkage(omrreq_lpdatlen, OS_NOSTACK)
#pragma linkage(omrreq_lpdat, OS_NOSTACK)

extern int omrreq_lpdatlen(void);
extern int omrreq_lpdat(char* bufp);

#endif /* SYSINFO_HELPERS_H_ */
