/*******************************************************************************
 * Copyright IBM Corp. and others 2025
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef J9_REPEATRETAINEDMETHODSANALYSIS_INCL
#define J9_REPEATRETAINEDMETHODSANALYSIS_INCL

#if !defined(J9VM_OPT_JITSERVER)
#error "JITServer-only header"
#endif

// On the server, to save round trips, J9::RetainedMethodSet does not scan the
// loader graph (outlivingLoaders). Instead it just represents the loaders of
// the methods that it started with (and their defining classes if anonymous).
// This is allowed because a RetainedMethodSet is not required to be complete,
// and the resulting sets of keepalives and bonds that the server builds are
// correct, though potentially more conservative than they would have been if
// the loader graph had been scanned.
//
// Near the end of the compilation, the client will repeat the analysis of the
// inlining table in terms of RetainedMethodSet, and it *will* scan the loader
// graph, resulting in the actual sets of keepalives and bonds to use. The
// results will comport with those of the server in the following sense: If a
// site generates a keepalive (or bond) on the client, then it also generated a
// keepalive (or bond, resp.) on the server. This holds because when considering
// a call site or call target, the contents of the RetainedMethodSet are based
// purely on the inlined call path leading to it, and at every point the
// server's set is a subset of the client's.
//
// This file contains declarations related to this repeat analysis.

#include "env/ResolvedInlinedCallSite.hpp"
#include <vector>

struct J9JITExceptionTable;
namespace TR { class Compilation; }
class TR_ResolvedMethod;

namespace J9 {
namespace RepeatRetainedMethodsAnalysis {

/**
 * \brief Attributes of a call site relevant to the repeat analysis.
 */
struct InlinedSiteInfo
   {
   uint32_t _inlinedSiteIndex; ///< The index of the site.
   TR_ResolvedMethod *_refinedMethod; ///< The call site refined method, if any.
   bool _generatedKeepalive; ///< True if the server generated a keepalive.
   bool _generatedBond; ///< True if the server generated a bond.
   };

/**
 * \brief Populate data needed for the client to repeat the analysis.
 * \param comp the compilation object
 * \param[out] inlinedSiteInfo vector of InlinedSiteInfo, sorted by site index
 * \param[out] keepaliveMethods the remote mirrors of the server's keepalive methods
 * \param[out] bondMethods the remote mirrors of the server's bond methods
 */
void getDataForClient(
   TR::Compilation *comp,
   std::vector<InlinedSiteInfo> &inlinedSiteInfo,
   std::vector<TR_ResolvedMethod*> &keepaliveMethods,
   std::vector<TR_ResolvedMethod*> &bondMethods);

/**
 * \brief Do the repeat analysis. This must only be called on the client.
 * \param comp the compilation object
 * \param inliningTable the inlining table
 * \param inlinedSiteInfo vector of InlinedSiteInfo, sorted by site index
 * \param serverKeepaliveMethods the remote mirrors of the server's keepalive methods
 * \param serverBondMethods the remote mirrors of the server's bond methods
 * \return the root retained method set resulting from analysis
 */
OMR::RetainedMethodSet *analyzeOnClient(
   TR::Compilation *comp,
   const TR::vector<J9::ResolvedInlinedCallSite, TR::Region&> &inliningTable,
   const std::vector<InlinedSiteInfo> &inlinedSiteInfo,
   const std::vector<TR_ResolvedMethod*> &serverKeepaliveMethods,
   const std::vector<TR_ResolvedMethod*> &serverBondMethods);

/**
 * \brief Do the repeat analysis. This must only be called on the client.
 * \param comp the compilation object
 * \param metadata the JIT body metadata
 * \param inlinedSiteInfo vector of InlinedSiteInfo, sorted by site index
 * \param serverKeepaliveMethods the remote mirrors of the server's keepalive methods
 * \param serverBondMethods the remote mirrors of the server's bond methods
 * \return the root retained method set resulting from analysis
 */
OMR::RetainedMethodSet *analyzeOnClient(
   TR::Compilation *comp,
   J9JITExceptionTable *metadata,
   const std::vector<InlinedSiteInfo> &inlinedSiteInfo,
   const std::vector<TR_ResolvedMethod*> &serverKeepaliveMethods,
   const std::vector<TR_ResolvedMethod*> &serverBondMethods);

} // namespace RepeatRetainedMethodsAnalysis
} // namespace J9

#endif // J9_REPEATRETAINEDMETHODSANALYSIS_INCL
