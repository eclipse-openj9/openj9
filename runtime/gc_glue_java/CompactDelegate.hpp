
/*******************************************************************************
 * Licensed Materials - Property of IBM
 * "Restricted Materials of IBM"
 *
 * (c) Copyright IBM Corp. 2017 All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or disclosure
 * restricted by GSA ADP Schedule Contract with IBM Corp.
 *******************************************************************************/

#ifndef COMPACTDELEGATE_HPP_
#define COMPACTDELEGATE_HPP_

#include "omr.h"
#include "omrcfg.h" 
#include "omrgcconsts.h"

#include "EnvironmentStandard.hpp"

class MM_CompactScheme;
class MM_MarkMap;

#if defined(OMR_GC_MODRON_COMPACTION)

/**
 * Delegate class provides implementations for methods required for Collector Language Interface 
 */
class MM_CompactDelegate
{
	/*
	 * Data members  
	 */
private:
	MM_CompactScheme *_compactScheme;
	MM_MarkMap *_markMap; 
	OMR_VM *_omrVM;
 
protected:
 
public:

	/*
	 * Function members
	 */
private:
 
protected:

public:
	/**
	 * Initialize the delegate.
	 *
	 * @param omrVM 
	 * @return true if delegate initialized successfully
	 */
	void tearDown(MM_EnvironmentBase *env);

	bool initialize(MM_EnvironmentBase *env, OMR_VM *omrVM, MM_MarkMap *markMap, MM_CompactScheme *compactScheme);

	void verifyHeap(MM_EnvironmentBase *env, MM_MarkMap *markMap);

	void fixupRoots(MM_EnvironmentBase *env,  MM_CompactScheme *compactScheme);

	void workerCleanupAfterGC(MM_EnvironmentBase *env);

	void masterSetupForGC(MM_EnvironmentBase *env);

	MM_CompactDelegate()
		: _compactScheme(NULL)
		, _markMap(NULL)
		, _omrVM(NULL)
	{ }
};

#endif /* OMR_GC_MODRON_COMPACTION */
#endif /* COMPACTDELEGATE_HPP_ */

