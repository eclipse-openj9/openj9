/*******************************************************************************
 * Copyright (c) 1998, 2019 IBM Corp. and others
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

/* _GNU_SOURCE forces GLIBC_2.0 sscanf/vsscanf/fscanf for RHEL5 compatibility */
#if defined(LINUX) && !defined(J9ZTPF)
#define _GNU_SOURCE
#endif /* defined(LINUX) && !defined(J9ZTPF) */

#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>

#include "ute_core.h"
#include "rastrace_internal.h"
#include "rastrace_external.h"
#include "omrutil.h"

static char *UT_MISSING_TRACE_FORMAT = "  Tracepoint format not in dat file";
#define MAX_QUALIFIED_NAME_LENGTH 16

omr_error_t
initializeComponentData(UtComponentData **componentDataPtr, UtModuleInfo *moduleInfo, const char *componentName)
{
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	UtComponentData *componentData = (UtComponentData *) j9mem_allocate_memory(sizeof(UtComponentData), OMRMEM_CATEGORY_TRACE );

	UT_DBGOUT(2, ("<UT> initializeComponentData: %s\n", componentName));
	if ( componentData == NULL){
		UT_DBGOUT(1, ("<UT> Unable to allocate componentData for %s\n", componentName));
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}
	
	initHeader( &componentData->header, UT_TRACE_COMPONENT_DATA, sizeof(UtComponentData) );
	componentData->componentName   =  (char *) j9mem_allocate_memory( strlen(componentName) +1, OMRMEM_CATEGORY_TRACE);
	if ( componentData->componentName == NULL){
		UT_DBGOUT(1, ("<UT> Unable to allocate componentData's name field for %s\n", componentName));
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}
	strcpy(componentData->componentName, componentName);

	/* Setup the fully qualified name here as when we come to print out trace counters the modules may have been
	 * freed already. */
	if( moduleInfo->traceVersionInfo->traceVersion >= 7 && moduleInfo->containerModule != NULL ) {
		char qualifiedName[MAX_QUALIFIED_NAME_LENGTH];
		j9str_printf(PORTLIB, qualifiedName, MAX_QUALIFIED_NAME_LENGTH,"%s(%s)", moduleInfo->name, moduleInfo->containerModule->name);
		componentData->qualifiedComponentName = (char *) j9mem_allocate_memory( strlen(qualifiedName) + 1, OMRMEM_CATEGORY_TRACE);
		if ( componentData->qualifiedComponentName == NULL){
				UT_DBGOUT(1, ("<UT> Unable to allocate componentData's name field for %s\n", componentName));
				return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		}
		strcpy(componentData->qualifiedComponentName, qualifiedName);
	} else {
		componentData->qualifiedComponentName = componentData->componentName;
	}

	if (moduleInfo->formatStringsFileName != NULL){
		componentData->formatStringsFileName = (char *) j9mem_allocate_memory( strlen(moduleInfo->formatStringsFileName) +1, OMRMEM_CATEGORY_TRACE );
		if ( componentData->formatStringsFileName == NULL){
			UT_DBGOUT(1, ("<UT> Unable to allocate componentData's format strings file name field for %s\n", componentName));
			return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		}
		strcpy(componentData->formatStringsFileName, moduleInfo->formatStringsFileName);
	} else {
		componentData->formatStringsFileName = NULL;
	}
	
	componentData->moduleInfo = moduleInfo;
	componentData->tracepointCount = moduleInfo->count;
	componentData->numFormats = 0;
	componentData->tracepointFormattingStrings = NULL;
	componentData->tracepointcounters = NULL;
	componentData->alreadyfailedtoloaddetails = 0;
	componentData->next = NULL;
	componentData->prev = NULL;
	
	*componentDataPtr = componentData;
	UT_DBGOUT(2, ("<UT> initializeComponentData complete: %s\n", componentName));

	return OMR_ERROR_NONE;
}

void
freeComponentData(UtComponentData *componentDataPtr){
	int numFormats, i;
	char *tempString;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	UT_DBGOUT(2, ("<UT> freeComponentData: %s\n", componentDataPtr->componentName));

	numFormats = componentDataPtr->numFormats;

	/* free any storage allocated for the format strings and names */
	if (componentDataPtr->tracepointFormattingStrings != NULL){
		for (i = 0; i < numFormats; i++) {
			tempString = componentDataPtr->tracepointFormattingStrings[i];
			if ( tempString != NULL && tempString != UT_MISSING_TRACE_FORMAT) {
				j9mem_free_memory( tempString);
			}
		}
		j9mem_free_memory( componentDataPtr->tracepointFormattingStrings);
	}

	if (componentDataPtr->tracepointcounters != NULL){
		j9mem_free_memory(componentDataPtr->tracepointcounters);
	}

	if (componentDataPtr->qualifiedComponentName != componentDataPtr->componentName && componentDataPtr->qualifiedComponentName != NULL){
		j9mem_free_memory(componentDataPtr->qualifiedComponentName);
	}

	if (componentDataPtr->componentName != NULL){
		j9mem_free_memory(componentDataPtr->componentName);
	}
	
	if (componentDataPtr->formatStringsFileName != NULL){
		j9mem_free_memory(componentDataPtr->formatStringsFileName);
	}
	
	j9mem_free_memory(componentDataPtr);

	UT_DBGOUT(2, ("<UT> freeComponentData completed\n"));
	return;
}

omr_error_t
initializeComponentList(UtComponentList **componentListPtr)
{
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	UtComponentList *componentList = (UtComponentList *)j9mem_allocate_memory( sizeof(UtComponentList), OMRMEM_CATEGORY_TRACE);
	UT_DBGOUT(2, ("<UT> initializeComponentList: %p\n", componentListPtr));
	if ( componentList == NULL){
		UT_DBGOUT(1, ("<UT> Unable to allocate component list\n" ));
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}
	initHeader( &componentList->header, UT_TRACE_COMPONENT_LIST, sizeof(UtComponentList) );		

	componentList->head = NULL;
	componentList->deferredConfigInfoHead = NULL;

	*componentListPtr = componentList;
	UT_DBGOUT(2, ("<UT> initializeComponentList: %p completed\n", componentListPtr));
	return OMR_ERROR_NONE;
}

omr_error_t
freeComponentList(UtComponentList *componentList)
{
	UtComponentData *compData = componentList->head;
	UtComponentData *tempCD;
	UtDeferredConfigInfo *configInfo = componentList->deferredConfigInfoHead;
	UtDeferredConfigInfo *tempCI;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	UT_DBGOUT(2, ("<UT> freeComponentList: %p\n", componentList));

	while(compData != NULL){
		tempCD = compData->next;
		UT_DBGOUT(2, ("<UT> freeComponentList: freeing CI [%p] from [%p]\n", compData, componentList));
		freeComponentData(compData);
		compData = tempCD;
	}

	while(configInfo != NULL){
		tempCI = configInfo->next;
		UT_DBGOUT(2, ("<UT> freeComponentList: freeing CI [%p] from [%p]\n", configInfo, componentList));
		if (configInfo->groupName != NULL){
			j9mem_free_memory(configInfo->groupName);
		}
		if (configInfo->componentName != NULL){
			j9mem_free_memory(configInfo->componentName);
		}
		j9mem_free_memory(configInfo);
		configInfo = tempCI;
	}

	j9mem_free_memory(componentList);

	UT_DBGOUT(2, ("<UT> freeComponentList: %p finished processing\n", componentList));

	return OMR_ERROR_NONE;
}

omr_error_t
addComponentToList(UtComponentData *componentData, UtComponentList *componentList)
{
	UtComponentData *compDataCursor;
	UtComponentData *endOfList;

	UT_DBGOUT(1, ("<UT> addComponentToList: component: %s list: %p\n",componentData->componentName, componentList));
	if (componentList == NULL){
		UT_DBGOUT(1, ("<UT> Not adding %s to NULL component list\n", componentData->componentName ));
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}
	if (componentData == NULL){
		UT_DBGOUT(1, ("<UT> Not adding NULL component to component list\n" ));
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	compDataCursor = componentList->head;
	endOfList = componentList->head;

	/* is it already in the list? */
	while (compDataCursor != NULL) {
		char *tempName = compDataCursor->componentName;
		if ( try_scan(&tempName, componentData->componentName) && *tempName == '\0' ){
			/* found the component in the component list */
			UT_DBGOUT(1, ("<UT> addComponentToList: component %s found\n",componentData->componentName));
			if (compDataCursor->moduleInfo != NULL && componentData->moduleInfo->traceVersionInfo->traceVersion >= 6){
				/* mirror existing active info into newly registering module */
				memcpy(componentData->moduleInfo->active, compDataCursor->moduleInfo->active, compDataCursor->moduleInfo->count);
				componentData->moduleInfo->next = compDataCursor->moduleInfo;				
			}
		} 
		/* need to walk to end of list in all cases */
		endOfList = compDataCursor;
		compDataCursor = compDataCursor->next;
	}

	UT_DBGOUT(1, ("<UT> addComponentToList: adding %s [%p] at ",componentData->componentName, componentData));
	if (endOfList == NULL){
		UT_DBGOUT(1, ("<UT> head\n"));
		/* this is the head of the list */
		componentList->head = componentData;        
		componentData->prev = NULL;
		componentData->next = NULL;
	} else {
		UT_DBGOUT(1, ("<UT> end\n"));
		/* add component to the end of the list */
		endOfList->next = componentData;
		componentData->prev = endOfList;
		componentData->next = NULL;
	}        
	return OMR_ERROR_NONE;
}

omr_error_t
processComponentDefferedConfig(UtComponentData *componentData, UtComponentList *componentList)
{
	omr_error_t rc = OMR_ERROR_NONE;
	UtDeferredConfigInfo *configInfo;

	if (componentList == NULL || componentData == NULL){
		UT_DBGOUT(1, ("<UT> Can't process config info for a NULL component [%p] or NULL component list [%p]\n", componentData, componentList ));
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	if (componentData->moduleInfo == NULL){
		UT_DBGOUT(1, ("<UT> Can't process defferred config info on a non live component: %s\n", componentData->componentName ));
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	/* all the pent up config needs to be released */
	if (componentList->deferredConfigInfoHead != NULL){
		UT_DBGOUT(2, ("<UT> processComponentDefferedConfig: component %s - applying global deferred config info\n",componentData->componentName));
		configInfo = componentList->deferredConfigInfoHead;
		while ( configInfo != NULL ){
			BOOLEAN configAppliesToAll = j9_cmdla_stricmp(configInfo->componentName, UT_ALL) == 0;
			
			if (configAppliesToAll || j9_cmdla_stricmp(configInfo->componentName, componentData->componentName) == 0){
				
				/* Applying "all" options to components may fail because of missing groups or other inappropriate settings.
				 * We suppress messages when applying "all" options to new components.
				 */
				BOOLEAN suppressMessages = configAppliesToAll;
				omr_error_t err = setTracePointsTo(componentData->componentName, componentList,
								  configInfo->all, configInfo->firstTracePoint, 
								  configInfo->lastTracePoint, configInfo->value,
								  configInfo->level, configInfo->groupName,
								  suppressMessages, configInfo->setActive);
				if (OMR_ERROR_NONE != err) {
					if (! suppressMessages ) {
						UT_DBGOUT(1, ("<UT> can't activate deferred trace opts on %s\n", componentData->componentName ));
						rc = err;
					}
				}
			}
			configInfo = configInfo->next;
		}
		UT_DBGOUT(2, ("<UT> processComponentDefferedConfig: component %s - apply global deferred config info complete\n",componentData->componentName));
	}

	UT_DBGOUT(2, ("<UT> addComponentToList: component %s processed deferred config info\n",componentData->componentName));
	return rc;
}

omr_error_t
removeModuleFromList(UtModuleInfo* module, UtComponentList *componentList)
{
	UtComponentData *compDataCursor = componentList->head;
	UtComponentData *prevCompData;
	UT_DBGOUT(2, ("<UT> removeModuleFromList: searching for module %s in componentList %p\n", module->name, componentList));

	/* for flight controller functionality, any unloaded components should be moved onto an unloaded list, so their
	   formatting strings and a memcpy of their moduleInfo is available */
	while ( compDataCursor != NULL ){
		if ( 0 == strcmp(compDataCursor->componentName, module->name) ) {

			UT_DBGOUT(2, ("<UT> removeModuleFromList: found component %s in componentList %p\n",module->name, componentList));

			/* remove this module from the linked list within the component */
			if (module->traceVersionInfo->traceVersion < 6) {
				/* pre version 6 modules can't be treated as a linked list */
				compDataCursor->moduleInfo = NULL;
			} else {
				UtModuleInfo** moduleCursor = &compDataCursor->moduleInfo;

				while (*moduleCursor != NULL) {
					if ( *moduleCursor == module ) {
						*moduleCursor = module->next;
						break;
					}
					moduleCursor = &(*moduleCursor)->next;
				}
			}

			/* if this component has no more modules, remove it from the list */
			if (NULL == compDataCursor->moduleInfo) {
				prevCompData = compDataCursor->prev;

				if (NULL == prevCompData) {
					/* this is the head of the list */
					componentList->head = compDataCursor->next;
					if (NULL != compDataCursor->next) {
						compDataCursor->next->prev = NULL;
					}                
				} else {
					/* this is in the middle of the list */
					prevCompData->next = compDataCursor->next;
					if ( compDataCursor->next != NULL ){
						compDataCursor->next->prev = prevCompData;
					}
				}
				if (componentList == UT_GLOBAL(componentList)){
					/* ensure no-one tries to continue using this as it will be going away */
					compDataCursor->moduleInfo = NULL;
					/* add to the unloaded modules list */
					addComponentToList(compDataCursor, UT_GLOBAL(unloadedComponentList));
				} else {
					/* otherwise we are unloading it from some other list so free it before we lose
					 * our reference to it */
					freeComponentData(compDataCursor);
				}
			}
			return OMR_ERROR_NONE;
		}
		compDataCursor = compDataCursor->next;
	}
	
	UT_DBGOUT(2, ("<UT> removeModuleFromList: didn't find component %s in componentList %p\n",module->name, componentList));
	return OMR_ERROR_INTERNAL;
}

static UtComponentData *
getComponentDataNext(const char *componentName, UtComponentList *componentList, UtComponentData* position)
{
	UtComponentData *compDataCursor;

	if(position == NULL ) {
		/* Beginning search from the first component */
		compDataCursor = componentList->head;
	} else {
		/* Resuming search from the last component found with this name. */
		compDataCursor = position->next;
	}
	UT_DBGOUT(4, ("<UT> getComponentData: searching for component %s in componentList %p\n", (componentName) ? componentName : "NULL", componentList));

	if (componentName == NULL){
		UT_DBGOUT(1, ("<UT> Can't get ComponentData for NULL componentName\n" ));
		return NULL;
	}

	while ( compDataCursor != NULL ){
		char *tempName = compDataCursor->componentName;
		if ( try_scan(&tempName, (char*)componentName) && *tempName == '\0' ){
			UT_DBGOUT(4, ("<UT> getComponentData: found component %s [%p] in componentList %p\n",componentName, compDataCursor, componentList));
			return compDataCursor;
		}
		compDataCursor = compDataCursor->next;
	}
	UT_DBGOUT(4, ("<UT> getComponentData: didn't find component %s in componentList %p\n",componentName, componentList));
	return NULL;
}

UtComponentData *
getComponentData(const char *componentName, UtComponentList *componentList) {
	return getComponentDataNext(componentName, componentList, NULL);
}

static UtComponentData *
getComponentDataForModule(UtModuleInfo *moduleInfo, UtComponentList *componentList)
{
	UtComponentData *compDataCursor;

	/* Beginning search from the first component */
	compDataCursor = componentList->head;

	UT_DBGOUT(4, ("<UT> getComponentData: searching for component for module %p in componentList %p\n", moduleInfo, componentList));

	while ( compDataCursor != NULL ){
		if ( compDataCursor->moduleInfo == moduleInfo){
			UT_DBGOUT(4, ("<UT> getComponentData: found component %s [%p] in componentList %p\n", compDataCursor->qualifiedComponentName, compDataCursor, componentList));
			return compDataCursor;
		}
		compDataCursor = compDataCursor->next;
	}
	UT_DBGOUT(4, ("<UT> getComponentData: didn't find component for module %p in componentList %p\n", moduleInfo, componentList));
	return NULL;
}

static void
updateActiveArray(UtComponentData *compData, int32_t first, int32_t last, unsigned char value, int32_t setActive)
{
	UtModuleInfo *moduleInfoCursor = compData->moduleInfo;
	int32_t i;

	while ( moduleInfoCursor != NULL ) {
		if ( value == 0 ) {
			for(i = first; i <= last; i++){
				moduleInfoCursor->active[i] = value;
			}
		} else {
			if (setActive) {
				for (i = first; i <= last; i++){
					moduleInfoCursor->active[i] |= value;
				}
			} else {
				for (i = first; i <= last; i++){
					moduleInfoCursor->active[i] &= ~value;
				}
			}
		}
		if (moduleInfoCursor->traceVersionInfo->traceVersion < 6){
			/* pre 6 modules can't be treated as linked list */
			moduleInfoCursor = NULL;
		} else {
			moduleInfoCursor = moduleInfoCursor->next;
		}
	}
}


/* Utility function for parsing command line options */
static char *
newSubString(const char *buffer, size_t ret)
{

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	char *temp = (char *)j9mem_allocate_memory(ret + 1, OMRMEM_CATEGORY_TRACE);
	UT_DBGOUT(2, ("<UT> newSubString: buffer %s size %d \n", buffer, ret));
	if (temp == NULL)
	{
		/*fprintf(stderr, "newSubString error\n");*/
		return NULL;
	}
	strncpy(temp, buffer, ret);
	temp[ret] = '\0';

	UT_DBGOUT(2, ("<UT> newSubString: returning buffer %p \n", temp));
	return temp;
}

static void freeSubString(char *buffer){
	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));
	UT_DBGOUT(2, ("<UT> freeSubString: buffer %p\n", buffer));
	j9mem_free_memory( buffer );
	return;
}

/* Utility function for parsing command line options */
static int32_t 
parseNumFromBuffer(const char *buffer, int ret)
{
	int32_t rc = 0;
	char *temp = newSubString(buffer, ret + 1);
	UT_DBGOUT(2, ("<UT> parseNumFromBuffer: buffer %s\n", buffer));
	if (temp == NULL)
	{
		/*fprintf(stderr, "parseNum error\n");*/
		return -1;
	}
	strncpy(temp, buffer, ret);
	temp[ret] = '\0';

	rc = atoi( temp );

	freeSubString(temp);
	UT_DBGOUT(2, ("<UT> parseNumFromBuffer: buffer %s found %d\n", buffer, rc));
	return rc;
}

static omr_error_t
addDeferredConfigToList(const char *componentName, int32_t all,
int32_t first, int32_t last, unsigned char value, int level, const char *groupName, UtDeferredConfigInfo **configList, int32_t setActive)
{
	UtDeferredConfigInfo *dconfiginfo;
	UtDeferredConfigInfo *temp;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	/* 1) add to deferred options for components that register */
	UT_DBGOUT(2, ("<UT> setTracePointsTo: component %s applying to all and adding to global deferred", componentName ));
	dconfiginfo = (UtDeferredConfigInfo *)j9mem_allocate_memory(sizeof(UtDeferredConfigInfo), OMRMEM_CATEGORY_TRACE);
	if ( dconfiginfo == NULL ){
		UT_DBGOUT(1, ("<UT> Unable to set tracepoints in %s - can't allocate config info\n", componentName));
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}
	
	dconfiginfo->componentName = (char *)j9mem_allocate_memory( strlen(componentName) + 1, OMRMEM_CATEGORY_TRACE );
	if (dconfiginfo->componentName == NULL){
		UT_DBGOUT(1, ("<UT> Unable to set tracepoints in %s - can't allocate config info componentName\n", componentName));
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}
	strcpy(dconfiginfo->componentName, componentName);
	
	dconfiginfo->all = all;
	dconfiginfo->firstTracePoint = first;
	dconfiginfo->lastTracePoint = last;
	dconfiginfo->value = value;
	dconfiginfo->level = level;
	dconfiginfo->setActive = setActive;

	if (groupName == NULL){
		dconfiginfo->groupName = NULL;
	} else {
		dconfiginfo->groupName = (char *)j9mem_allocate_memory(strlen( groupName ) + 1, OMRMEM_CATEGORY_TRACE);
		if ( dconfiginfo->groupName == NULL ){
			UT_DBGOUT(1, ("<UT> Unable to set tracepoints in %s - can't allocate config info groupName\n", componentName));
			return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		}
		strcpy(dconfiginfo->groupName, groupName);
	}
	dconfiginfo->next = NULL;

	if (*configList == NULL){
		*configList = dconfiginfo;
	} else {
		temp = *configList;
		while (temp->next != NULL){
			temp = temp->next;
		}
		temp->next = dconfiginfo;
	}
	
	return OMR_ERROR_NONE;
}

/* for this function, we assume that componentName has been parsed, and contains a single
 * component name only. */
static omr_error_t
setTracePointsForComponent(const char *componentName, UtComponentList *componentList, int32_t all, int32_t first, int32_t last,
unsigned char value, int level, const char *groupName, BOOLEAN suppressMessages, int32_t setActive)
{
	UtComponentData *compData;
	UtModuleInfo *modInfo = NULL;
	omr_error_t rc = OMR_ERROR_NONE;

	if (0 == j9_cmdla_strnicmp(componentName, UT_ALL, strlen(UT_ALL))) {
		rc = addDeferredConfigToList(componentName,
								all, first, last, value, level, groupName, 
								&componentList->deferredConfigInfoHead,
								setActive);
								
		compData = componentList->head;
		while (compData != NULL){ 
			if ( compData->moduleInfo != NULL){
				first = 0;
				last = compData->moduleInfo->count - 1;
				
				/* Don't configure auxiliary modules */
				if ( MODULE_IS_AUXILIARY(compData->moduleInfo) ) {
					compData = compData->next;
					continue;
				}

				if ( level != -1 ){
				 	setTracePointsByLevelTo(compData, level, value, setActive);
				} else if (groupName != NULL) {
				 	setTracePointGroupTo(groupName, compData, value, TRUE, setActive);
				} else {
					updateActiveArray(compData, first, last, value, setActive);
				}
			}
			compData = compData->next;
		}
		return rc; 
	} 
	
	compData = getComponentData(componentName, componentList);
	if ( compData == NULL ){
		/* add the information to the deferred configuration list */
		addDeferredConfigToList(componentName,
								all, first, last, value, level, groupName, 
								&componentList->deferredConfigInfoHead, setActive);
	} else while( compData != NULL ) { /* configuring live and registered module */
		int32_t maxTraceId = compData->moduleInfo->count - 1;

		UT_DBGOUT(2, ("<UT> setTracePointsTo: configuring registered component %s ",componentName));
		if (all) {
			first = 0;
			last = maxTraceId;
		}
		modInfo = compData->moduleInfo;
		if ( first > maxTraceId ){
			reportCommandLineError(suppressMessages, "Unable to set tracepoint %d in %s - tracepoint id out of range", first, componentName);
			return OMR_ERROR_INTERNAL;
		}
		if ( last > maxTraceId ){
			reportCommandLineError(suppressMessages, "Tracepoint %d not in range 0->%d %s", last, maxTraceId, componentName);
			last = maxTraceId;
			return OMR_ERROR_INTERNAL;
		}

		/*Auxiliary tracepoints can't be set*/
		if (MODULE_IS_AUXILIARY(modInfo)) {
			reportCommandLineError(suppressMessages, "Component %s is marked auxiliary and cannot be configured directly.", componentName);
			return OMR_ERROR_INTERNAL;
		}

		if ( groupName != NULL ){
			UT_DBGOUT(2, ("by group %s\n", groupName));
			rc = setTracePointGroupTo(groupName, compData, value, suppressMessages, setActive);
		} else if ( level != -1 ){
			UT_DBGOUT(2, ("by level %d\n", level));
			rc = setTracePointsByLevelTo(compData, level, value, setActive);
		} else {
			UT_DBGOUT(2, ("by range %d-%d\n", first, last));
			updateActiveArray(compData, first, last, value, setActive);
		}
		/* There may be more than one component with the same name, particularly in the case of
		 * libraries declared as submodules in tdf files. e.g. pool, j9util */
		compData =  getComponentDataNext(componentName, componentList, compData);
	}
	return rc;
}

/* valid ranges passed in in componentName are of one of the following forms:
 * tpnid{j9vm.10}
 * tpnid{j9vm.10-20}
 * j9vm.10
 * j9vm.10-20
 * and so on 
 */
static omr_error_t
parseAndSetTracePointsInRange(const char *componentName,
	unsigned char value, int32_t setActive, BOOLEAN suppressMessages)
{
	int length = 0;
	const char *p;
	omr_error_t rc = OMR_ERROR_INTERNAL;
	int ret = 0;
	int32_t firsttpid, lasttpid;
	char *compName;
	const char *temp;

	UT_DBGOUT(2, ("<UT> parseAndSetTracePointsInRange: %s\n", componentName));
	p = componentName;
	if (*p == '\0') {
		return OMR_ERROR_NONE;
	}

	/*
	 *  Tracepoint ID specified ?
	 */
	if (0 == j9_cmdla_strnicmp(p, UT_TPID, strlen(UT_TPID)) &&
		(p[strlen(UT_TPID)] == '(' || p[strlen(UT_TPID)] == '{') ) {
			reportCommandLineError(suppressMessages, "Invalid trace options, use: tpnid{componentName.[integer_offset]}");
			return OMR_ERROR_ILLEGAL_ARGUMENT;
	} else if (0 == j9_cmdla_strnicmp(p, UT_TPNID, strlen(UT_TPNID)) &&
						   p[strlen(UT_TPNID)] == '{') {
		length = 0;
		length += (int)strlen(UT_TPNID) + 1; /* the prefix and the open brace */
		
		/* Is there a close brace? */
		if ( strchr( p, '}') == NULL) {
			reportCommandLineError(suppressMessages, "Error: unclosed braces");
			return OMR_ERROR_ILLEGAL_ARGUMENT;
		}
	}

	if ( componentName[0] == '!' ){
		value = 0x00;
	}

	p += length;
	temp = p;
	while (*temp != '}' && *temp != '\0')
	{
		ret = 0;
		if (*temp == ',')
		{
			temp++, p++, length++;
		}

		while (*temp != '.')
		{
			if (*temp == '}' || *temp=='\0')
			{
				reportCommandLineError(suppressMessages, "Expecting tpnid{compname.offset} e.g. tpnid{j9trc.4}");
				return OMR_ERROR_ILLEGAL_ARGUMENT;
			}
			temp++;
			ret++;
		}

		compName = newSubString(p, ret);
		if (compName == NULL)
		{
			UT_DBGOUT(1, ("<UT> Can't allocate substring while parsing command line\n"));
			return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		}

		length += ret+1; 
		p+=ret+1;        /* plus one for the full-stop */

		ret = 0;
		temp = p;
		while (isdigit((int)*temp))
		{
			ret++;
			temp++;
		}
		length += ret;

		if (*temp == '-')
		{
			firsttpid = parseNumFromBuffer(p, ret);

			temp++;
			p += ret + 1;
			length++; /* plus one for the dash */
			ret = 0;
			if( !isdigit(*temp)) {
				reportCommandLineError(suppressMessages, "Expecting tracepoint range specified as tpnid{componentName.offset1-offset2} e.g. tpnid{j9trc.2-6}");
				return OMR_ERROR_ILLEGAL_ARGUMENT;
			}
			while (isdigit(*temp))
			{
				ret++;
				temp++;
			}
			lasttpid = parseNumFromBuffer(p, ret);
			/* Swap if specified largest first. */
			if( lasttpid < firsttpid ) {
				int32_t temptpid = firsttpid;
				firsttpid = lasttpid;
				lasttpid = temptpid;
			}
			rc = setTracePointsForComponent(compName, UT_GLOBAL(componentList), FALSE, firsttpid, lasttpid, value, -1, NULL, suppressMessages, setActive);
			length += ret;
			p += ret;
		} else	{
			firsttpid = lasttpid = parseNumFromBuffer(p, ret);
			rc = setTracePointsForComponent(compName, UT_GLOBAL(componentList), FALSE, firsttpid, lasttpid, value, -1, NULL, suppressMessages, setActive);
			p += ret;
		}
		freeSubString(compName);
	}
	return rc;
}

omr_error_t
setTracePointsTo(const char *componentName, UtComponentList *componentList, int32_t all, int32_t first, int32_t last,
unsigned char value, int level, const char *groupName, BOOLEAN suppressMessages, int32_t setActive)
{
	const char *tempstr = NULL;
	char *compNamesBuffer;
	int32_t stripbraces = FALSE;
	size_t namelength = 0;
	omr_error_t rc = OMR_ERROR_NONE;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	if (componentName == NULL){
		reportCommandLineError(suppressMessages, "Can't set tracepoints for NULL componentName" );
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	if (componentList == NULL){
		UT_DBGOUT(1, ("<UT> can't set tracepoints against NULL componentList\n"));
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	UT_DBGOUT(1, ("<UT> setTracePointsTo: component %s all= %s first=%d last=%d value=%d\n",componentName, (all?"TRUE":"FALSE"), first, last));

	/* check for multiple components */
	tempstr = strchr(componentName, ',');
	if (NULL != tempstr) {
		/* we have been passed multiple component names */
		UT_DBGOUT(2, ("<UT> setTracePointsTo found component list: %s\n", componentName ));

		if (componentName[0] == '{'){
			/* option is a list of components enclosed in braces */
			componentName++;
			stripbraces = TRUE;
		} else if ((0 == j9_cmdla_strnicmp(componentName, UT_TPNID, strlen(UT_TPNID))) && (tempstr < strchr(componentName, '}'))) {
			/* option is 'tpnid' followed by a list of components enclosed in braces */
			componentName += (int)strlen(UT_TPNID) + 1; /* skip over the prefix and the open brace */
			stripbraces = TRUE;
		}

		/* separate and process the first component name */
		namelength = tempstr - componentName;
		compNamesBuffer = (char *)j9mem_allocate_memory(strlen(componentName) + 1, OMRMEM_CATEGORY_TRACE);
		if ( compNamesBuffer == NULL ){
			UT_DBGOUT(1, ("<UT> Unable to set tracepoints in %s - can't allocate tempname info\n", componentName));
			return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		}
		strncpy(compNamesBuffer, componentName, namelength);
		compNamesBuffer[namelength] = '\0'; /* replace the comma and terminate the string */
		
		rc = setTracePointsToParsed(compNamesBuffer, componentList, all, first, last, value, level, groupName, suppressMessages,setActive);
		if (rc != OMR_ERROR_NONE) {
			j9mem_free_memory(compNamesBuffer);
			return rc;
		}
		componentName += namelength + 1;
		namelength = strlen(componentName);
		strcpy(compNamesBuffer, componentName);
		compNamesBuffer[namelength] = '\0';		
		if (stripbraces == TRUE){
			compNamesBuffer[namelength-1] = '\0'; /* blat the close brace out of the allocated copy */
		}
		/* recurse here in case the remainder is still a comma separated list */
		rc = setTracePointsTo(compNamesBuffer, componentList, all, first, last, value, level, groupName, suppressMessages,setActive);
		j9mem_free_memory(compNamesBuffer);
		return rc;
	}

	/* if we get here we have a single option, but it might still be enclosed by braces */
	if (componentName[0] == '{'){
		componentName++;	
		compNamesBuffer = (char *)j9mem_allocate_memory(strlen(componentName) + 1, OMRMEM_CATEGORY_TRACE);
		if (compNamesBuffer == NULL ){
			UT_DBGOUT(1, ("<UT> Unable to set tracepoints in %s - can't allocate tempname info\n", componentName));
			return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		}
		namelength = strlen(componentName);
		strcpy(compNamesBuffer, componentName);
		compNamesBuffer[namelength-1] = '\0'; /* blat the close brace out of the allocated copy */
		rc = setTracePointsToParsed(compNamesBuffer, componentList, all, first, last, value, level, groupName, suppressMessages, setActive);
		j9mem_free_memory(compNamesBuffer);
	} else {
		rc = setTracePointsToParsed(componentName, componentList, all, first, last, value, level, groupName, suppressMessages, setActive);
	}
	return rc;
}

omr_error_t
setTracePointsToParsed(const char *componentName, UtComponentList *componentList, int32_t all, int32_t first, int32_t last,
unsigned char value, int level, const char *groupName, BOOLEAN suppressMessages, int32_t setActive)
{
	const char *tempstr = NULL;
	char *newstr = NULL;
	char *newstr2 = NULL;
	size_t namelength = 0;
	size_t groupnamelength = 0;
	omr_error_t rc = OMR_ERROR_NONE;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	UT_DBGOUT(2, ("<UT> setTracePointsToParsed: %s\n", componentName));

	if (strchr(componentName, '.') != NULL){
		/* we have a range of tracepoints */
		rc = parseAndSetTracePointsInRange(componentName, value, setActive, suppressMessages);
		/* can't combine ranges and levels etc. */
		return rc;
	}

	if ( (tempstr = strchr( componentName, '{')) != NULL || (tempstr = strchr( componentName, '(')) != NULL ){
		char closingBrace;
		
		UT_DBGOUT(2, ("<UT> setTracePointsTo: has detected a suboption %s in %s\n", tempstr, componentName ));

		namelength = tempstr - componentName;
		
		if (*tempstr == '{') {
			closingBrace = '}';
		} else {
			closingBrace = ')';
		}
		
		/* Look for empty braces - this is always an error */
		if (*(tempstr + 1) == closingBrace) {
			reportCommandLineError(suppressMessages, "Error: found empty braces or parentheses");
			return OMR_ERROR_ILLEGAL_ARGUMENT;
		}
		
		/* Look for unclosed braces - another error */
		if ( strchr( tempstr, closingBrace) == NULL) {
			reportCommandLineError(suppressMessages, "Error: unclosed braces or parentheses");
			return OMR_ERROR_ILLEGAL_ARGUMENT;
		}
		
		tempstr++; /* points at first option char now */
		
		if (0 == j9_cmdla_strnicmp(tempstr, UT_LEVEL_KEYWORD, strlen(UT_LEVEL_KEYWORD))  ||
			  *tempstr == 'l' || *tempstr == 'L'){
			while ( !isdigit(*tempstr) ){
				if ( *tempstr == ',' || *tempstr == '}' || *tempstr == '\0'){
					reportCommandLineError(suppressMessages, "Trace level required without an integer level specifier");
					return OMR_ERROR_ILLEGAL_ARGUMENT;
				}
				tempstr++;
			}
			sscanf( tempstr, "%d", &level);
			newstr = (char *) j9mem_allocate_memory(namelength + 1, OMRMEM_CATEGORY_TRACE );
			if ( newstr == NULL ){
				UT_DBGOUT(1, ("<UT> Unable to set tracepoints in %s - can't allocate tempname info\n", componentName));
				return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
			}
			strncpy(newstr, componentName, namelength);
			newstr[namelength] = '\0';
			UT_DBGOUT(2, ("<UT> setTracePointsTo: Level detected %d in %s\n", level, newstr));
			componentName = newstr;
			/* actual configuration of the component will now fall through to below */
		} else {
			/* must be a group name note - types are now groups */
			UT_DBGOUT(2, ("<UT> setTracePointsTo: A Group detected \n"));
			newstr = (char *) j9mem_allocate_memory(namelength + 1, OMRMEM_CATEGORY_TRACE );
			if ( newstr == NULL ){
				UT_DBGOUT(1, ("<UT> Unable to set tracepoints in %s - can't allocate tempname info\n", componentName));
				return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
			}
			strncpy(newstr, componentName, namelength);
			newstr[namelength] = '\0';

			groupnamelength = strlen(componentName) - namelength;
			newstr2 = (char *) j9mem_allocate_memory(groupnamelength - 1, OMRMEM_CATEGORY_TRACE );
			if ( newstr2 == NULL ){
				UT_DBGOUT(1, ("<UT> Unable to set tracepoints in %s - can't allocate tempname info\n", componentName));
				return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
			}
			strncpy(newstr2, componentName + namelength + 1, groupnamelength - 2);
			newstr2[groupnamelength - 2] = '\0';

			groupName = newstr2;

			UT_DBGOUT(2, ("<UT> setTracePointsTo: Group %s detected in %s\n", groupName, newstr));
			componentName = newstr;
		} 
	}

	rc = setTracePointsForComponent(componentName, componentList, all, first, last,
				value, level, groupName,suppressMessages, setActive);

	/* tidy up */
	if (newstr != NULL){
		j9mem_free_memory( newstr);
	}
	if (newstr2 != NULL){
		j9mem_free_memory( newstr2);
	}
	return rc;
}


/*******************************************************************************
 * name        - openFileFromDirectorySearchList
 * description - Look for a file in a semicolon separated list of directories
 * parameters  - searchPath - a semicolon separated list of directories, null
 *                            terminated and non NULL.
 *               fileName - the name of the file to look for null terminated
 *               flags - flags to be passed into the file open call
 *               mode - flags to be passed into the file open call 
 * returns     - intptr_t >= 0 on success, < 0 on failure
 *                        success returns a valid file handle to be used in 
 *                        subsequent file operations.
 ******************************************************************************/
static intptr_t 
openFileFromDirectorySearchList(char *searchPath, char *fileName, int32_t flags, int32_t mode)
{
	intptr_t fileHandle = -1;
	char tempFileNamePath[MAX_IMAGE_PATH_LENGTH];
	char *nextPathEntry = searchPath;
	size_t searchPathLen;
	size_t offset = 0;
	if (searchPath == NULL || fileName == NULL){
		/* error */
		return fileHandle;			
	}
	
	searchPathLen = strlen(searchPath);
	while (offset < searchPathLen) {
		PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));
		size_t currentPathEntryEndsAt = strcspn(nextPathEntry, ";\0");
		strncpy(tempFileNamePath, nextPathEntry, currentPathEntryEndsAt);
		tempFileNamePath[currentPathEntryEndsAt] = '\0';
		strcat(tempFileNamePath, DIR_SEPARATOR_STR);
		strcat(tempFileNamePath, fileName);
		
		UT_DBGOUT(2, ("<UT> dat file loader looking for %s at %s\n", fileName, tempFileNamePath));
		
		fileHandle = j9file_open(tempFileNamePath, flags, mode);
		if (fileHandle != -1){
			UT_DBGOUT(2, ("<UT> dat file loader found for %s at %s\n", fileName, tempFileNamePath));
			return fileHandle;
		}
		
		offset += currentPathEntryEndsAt + 1;
		nextPathEntry += currentPathEntryEndsAt + 1;;
	}
	
	return fileHandle;
}

/**
 * read the first line of the dat file contents provided. Should contain a 
 * float denoting the file version.
 * 
 * Return the float version read, or 0.0F if an error reading the float occurred.
 */
static float
getDatFileVersion(char *formatFileContents)
{
	float version;
	/* read the length of the first line */
	if (formatFileContents == NULL){
		return 0.0F;
	}

	version = (float)atof(formatFileContents); 
	UT_DBGOUT(2, ("<UT> getDatFileVersion %f\n", version));
	return version;
}

static omr_error_t
loadFormatStringsForComponent(UtComponentData *componentData)
{
	omr_error_t rc = OMR_ERROR_NONE;
	intptr_t formatFileFD = -1;
	int numFormats = componentData->tracepointCount;
	char **formatStringsComponentArray = NULL;
	int formatStringsComponentArraySize = 0;
	char *tempPtr, *tempPtr2;
	int currenttp = 0;
	char compName[1024];
	char *fileContents = NULL;
	int64_t fileSize;
	float datFileVersion;
	const unsigned int componentNameLength = (const unsigned int)strlen(componentData->componentName);

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	UT_DBGOUT(2, ("<UT> loadFormatStringsForComponent %s\n", componentData->componentName));

	if (componentData->alreadyfailedtoloaddetails != 0){
		UT_DBGOUT(2, ("<UT> loadFormatStringsForComponent %s returning due to previous load failure\n", componentData->componentName));
		return OMR_ERROR_INTERNAL;
	}

	UT_DBGOUT(2, ("<UT> loadFormatStringsForComponent %s parsing filename = %s\n", componentData->componentName, componentData->formatStringsFileName));
	/* buffer format files into memory at some point */
	if (fileContents == NULL){
		/* look in jre/lib directory first */
		UT_DBGOUT(1, ("<UT> loadFormatStringsForComponent trying to load = %s\n", componentData->formatStringsFileName));
		formatFileFD = openFileFromDirectorySearchList(	UT_GLOBAL(traceFormatSpec),
														componentData->formatStringsFileName, 
														EsOpenText | EsOpenRead, 0);
		if ( formatFileFD == -1 ){
			UT_DBGOUT(1, ("<UT> loadFormatStringsForComponent can't load = %s, from current directory either - marking it unfindeable\n", componentData->formatStringsFileName));
			rc = OMR_ERROR_INTERNAL;
			goto epilogue;
		}
		fileSize = j9file_flength( formatFileFD );
		if ( fileSize < 0 ){
			UT_DBGOUT(1, ("<UT> error getting file size for file %s\n", componentData->formatStringsFileName));
			rc = OMR_ERROR_INTERNAL;
			goto epilogue;
		}
		fileContents = (char *) j9mem_allocate_memory( (uintptr_t)fileSize + 1, OMRMEM_CATEGORY_TRACE );
		if (fileContents == NULL){
			UT_DBGOUT(1, ("<UT> can't allocate buffer\n"));
			rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
			goto epilogue;
		}
		if (j9file_read(formatFileFD, fileContents, (int32_t)fileSize) != (int32_t) fileSize){
			UT_DBGOUT(1, ("<UT> can't read the file into the buffer\n"));
			rc = OMR_ERROR_INTERNAL;
			goto epilogue;
		}
		j9file_close(formatFileFD);
		formatFileFD = -1;

		/* if running on z/os convert to ascii */
		fileContents[fileSize] = '\0';
		twE2A(fileContents);
	}
	
	datFileVersion = getDatFileVersion(fileContents);
	if (datFileVersion == 0.0F){
		UT_DBGOUT(1, ("<UT> dat file version error.\n"));
		rc = OMR_ERROR_INTERNAL;
		goto epilogue;
	}
	
	formatStringsComponentArraySize = numFormats * sizeof(char *);
	formatStringsComponentArray = (char **) j9mem_allocate_memory( formatStringsComponentArraySize, OMRMEM_CATEGORY_TRACE );
	if ( formatStringsComponentArray == NULL ){
		UT_DBGOUT(1, ("<UT> can't allocate formatStrings array\n"));
		rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		goto epilogue;
	}

	/* parse the file */
	currenttp = 0;
	tempPtr = fileContents;
	while ( currenttp < numFormats ){
		if (*tempPtr == '\n'){
			unsigned int tempLen;
			tempPtr2 = ++tempPtr; /* points to start of next line */

			/* read to the next space */
			while(*tempPtr2 != ' '){
				tempPtr2++;
			}
			tempLen = (unsigned int)(tempPtr2 - tempPtr);
			strncpy(compName, tempPtr, tempLen);
			compName[tempLen] = '\0';
			
			if (datFileVersion >= 5.1F) {
				/* then the first word in each dat file line is of the form 
				 * componentName.integerID, which is all currently stored in comp name */
				char *period = strchr(compName, '.');
				if (period == NULL){
					UT_DBGOUT(1, ("<UT> error parsing 5.1 dat file component name: [%.*s].\n", tempLen, tempPtr));
				} else {
					*period = '\0'; /* replace the '.' with a '\0' to make compname coherent */
					tempLen = (unsigned int)(period - compName);
					++period;
				}
			}

			tempPtr = tempPtr2;
			if ( (tempLen == componentNameLength) &&  (0 == strncmp(compName, componentData->componentName, tempLen)) ){
				/* skip the next four fields: type number, overhead, level and explicit. e.g. " 0 1 1 N " */
				int field;

				for (field = 0; field < 4; field++) {
					while (*++tempPtr != ' ');
				}				
				tempPtr++;

				tempPtr2 = tempPtr;
				/* find the end of the trace point name */
				while (*tempPtr2 != ' ') {
					tempPtr2++;
				}

				tempLen = (unsigned int)(tempPtr2 - tempPtr);

				/* Skip the name as we don't use them. */

				tempPtr2 += 2; /* skip space and open quote */
				tempPtr = tempPtr2;
				/* find the end of the tracepoint's format string */
				while (*tempPtr2 != '\"'){
					tempPtr2++;
				}

				tempLen = (unsigned int)(tempPtr2 - tempPtr);
				formatStringsComponentArray[currenttp] = (char *) j9mem_allocate_memory( tempLen + 1, OMRMEM_CATEGORY_TRACE );
				if (formatStringsComponentArray[currenttp] == NULL){
					UT_DBGOUT(1, ("<UT> can't allocate a format string\n"));
					rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
					goto epilogue;
				} else {
					strncpy(formatStringsComponentArray[currenttp], tempPtr, tempLen);
					formatStringsComponentArray[currenttp][tempLen] = '\0';
				}
				tempPtr = tempPtr2; /* up to the end of line */
				currenttp++;
			} 
		}
		tempPtr++;
		if (tempPtr >= (fileContents + fileSize - 1)) {
			/* fill any remaining positions in the arrays so they can be dereferenced */
			for (; currenttp < numFormats; currenttp++) {
				formatStringsComponentArray[currenttp] = UT_MISSING_TRACE_FORMAT;
			}
			break;
		}
	}
	componentData->numFormats = numFormats;
	componentData->tracepointFormattingStrings = formatStringsComponentArray;
	
epilogue:
	if (OMR_ERROR_NONE != rc) {
	    int i = 0;
		componentData->alreadyfailedtoloaddetails = 1; /* don't try and load them again next time */
		
		if (formatStringsComponentArray != NULL) {
		    for (i = 0; i < formatStringsComponentArraySize; i++) {
		    	if (formatStringsComponentArray[i] != NULL) {
					j9mem_free_memory( formatStringsComponentArray[i]);
				} else {
					break;
				}
			}
			j9mem_free_memory( formatStringsComponentArray);
		}
		if (formatFileFD != -1) {
			j9file_close(formatFileFD);
		}
	}

	if (fileContents != NULL) {
		j9mem_free_memory( fileContents);
	}
	return rc;
}

char *
getFormatString(const char *componentName, int32_t tracepoint)
{
	UtComponentData *compData = getComponentData(componentName, UT_GLOBAL(componentList));

	UT_DBGOUT(2, ("<UT> getFormatString for: component %s tracepoint %d\n", componentName, tracepoint));
	if (compData == NULL) {
		/* search the unloaded components */
		compData = getComponentData(componentName, UT_GLOBAL(unloadedComponentList));
		if (compData == NULL){
			UT_DBGOUT(1, ("<UT> Unable to get formatString for %s.%d component not registered\n", componentName, tracepoint));
			return NULL;
		}
	}

	if (compData->alreadyfailedtoloaddetails != 0) {
		/* no error because we will have already reported it */
		return NULL;
	}
	
	if (compData->tracepointFormattingStrings == NULL ){
		omr_error_t rc = loadFormatStringsForComponent(compData);
		if (OMR_ERROR_NONE != rc) {
			UT_DBGOUT(1, ("<UT> Unable to load formatStrings for %s\n", componentName));
			return NULL;
		}
	}

	if ( tracepoint >= compData->tracepointCount ){
		UT_DBGOUT(1, ("<UT> Unable to get formatString for %s.%d - no such tracepoint - maximum allowable tracepoint for that component is %d\n", componentName, tracepoint, compData->moduleInfo->count));
		return NULL;
	}

	return compData->tracepointFormattingStrings[tracepoint];
}

omr_error_t
setTracePointGroupTo(const char *groupName, UtComponentData *componentData, unsigned char value, BOOLEAN suppressMessages, int32_t setActive)
{
	UtGroupDetails *groupDetails;
	char *tempgrpname;
	const char *tempstr;
	size_t gnamelength;
	omr_error_t rc = OMR_ERROR_NONE;
	int i = 0;
	int32_t tpid = 0;
	BOOLEAN groupFound = FALSE;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	if (componentData == NULL){
		UT_DBGOUT(1, ("<UT> setTracePointGroupTo called with invalid componentData\n"));
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	if (componentData->moduleInfo == NULL){
		UT_DBGOUT(1, ("<UT> setTracePointGroupTo called on unregistered component: %s\n", componentData->componentName));
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	if (componentData->moduleInfo->groupDetails == NULL){
		reportCommandLineError(suppressMessages, "Groups not supported in component %s", componentData->componentName);
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	UT_DBGOUT(2, ("<UT> setTraceGroupTo called: groupname %s compdata %p\n", groupName, componentData));

	if ( (tempstr = strchr(groupName, ';')) != NULL ){
		gnamelength = strlen(groupName);
		tempgrpname = (char *)j9mem_allocate_memory( gnamelength + 1, OMRMEM_CATEGORY_TRACE );
		if ( tempgrpname == NULL ){
			UT_DBGOUT(1, ("<UT> can't allocate temp group name\n"));
			return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		}
		strncpy(tempgrpname, groupName, (tempstr - groupName));
		tempgrpname[(tempstr - groupName)] = '\0';
		rc = setTracePointGroupTo(tempgrpname, componentData, value, suppressMessages, setActive);
		if (OMR_ERROR_NONE != rc) {
			j9mem_free_memory( tempgrpname);
			return rc;
		}
		strncpy(tempgrpname, tempstr + 1, gnamelength - (tempstr - groupName));
		tempgrpname[(gnamelength - (tempstr - groupName))] = '\0';
		rc = setTracePointGroupTo(tempgrpname, componentData, value, suppressMessages, setActive);

		j9mem_free_memory( tempgrpname);
		return rc;
	}

	UT_DBGOUT(2, ("<UT> setTraceGroupTo called: groupname %s component %s\n", groupName, componentData->componentName));

	groupDetails = componentData->moduleInfo->groupDetails;
	while ( groupDetails != NULL ) {
		if ( !j9_cmdla_strnicmp(groupName, groupDetails->groupName, strlen(groupDetails->groupName))  ){
			/* found the group, now enable its tracepoints */
			groupFound = TRUE;
			for ( i = 0 ; i < groupDetails->count ; i++ ){
				tpid = groupDetails->tpids[i];
				updateActiveArray(componentData, tpid, tpid, value, setActive);
			}
		}        
		groupDetails = groupDetails->next;
	}

	if (groupFound) {
		return OMR_ERROR_NONE;
	} else {
		reportCommandLineError(suppressMessages, "There is no group %s in component %s",groupName,componentData->moduleInfo->name);
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}
}

omr_error_t
setTracePointsByLevelTo(UtComponentData *componentData, int level, unsigned char value, int32_t setActive)
{
	omr_error_t rc = OMR_ERROR_NONE;
	UtModuleInfo *modInfo;
	int i;

	if (componentData == NULL){
		UT_DBGOUT(1, ("<UT> setTracepointsByLevelTo called with invalid componentData\n"));
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	if (componentData->moduleInfo == NULL){
		UT_DBGOUT(1, ("<UT> setTracepointsByLevelTo called on unregistered component: %s\n", componentData->componentName));
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}

	if (componentData->moduleInfo->levels == NULL){
		UT_DBGOUT(2, ("<UT> levels not supported in component %s\n", componentData->componentName));
		return OMR_ERROR_NONE;
	}

	modInfo = componentData->moduleInfo;
	for ( i = 0; i < modInfo->count; i++){         
		 if ( modInfo->levels[i] <= level ){
				updateActiveArray(componentData, i, i, value, setActive);
		 }
	}
	return rc;
}

uint64_t
incrementTraceCounter(UtModuleInfo *moduleInfo, UtComponentList *componentList, int32_t tracepoint)
{
	UtComponentData *compData;

	PORT_ACCESS_FROM_PORT(UT_GLOBAL(portLibrary));

	if( moduleInfo == NULL ) {
		/* this is an internal tracepoint */
		UT_DBGOUT(2, ("<UT> incrementTraceCounter short circuit returning due to NULL compName\n"));
		return 0;
	}
	compData = getComponentDataForModule( moduleInfo, componentList);
	if (compData == NULL){
		UT_DBGOUT(1, ("<UT> Unable to increment trace counter %s.%d - no component\n", moduleInfo->name, tracepoint));
		return 0;
	}
	if (compData->moduleInfo == NULL){
		UT_DBGOUT(1, ("<UT> Unable to increment trace counter %s.%d - no such loaded component\n", moduleInfo->name, tracepoint));
		return 0;
	}
	if (compData->tracepointcounters == NULL){
		/* first time anything in this component has been counted */
		compData->tracepointcounters = (uint64_t *) j9mem_allocate_memory(sizeof(uint64_t) * compData->moduleInfo->count, OMRMEM_CATEGORY_TRACE);
		if (compData->tracepointcounters == NULL){
			UT_DBGOUT(1, ("<UT> Unable to allocate trace counter buffers for %s\n", moduleInfo->name));
			return 0;
		}
		memset(compData->tracepointcounters, 0, sizeof(uint64_t) * compData->moduleInfo->count);
	}

	return ++compData->tracepointcounters[tracepoint];
}

