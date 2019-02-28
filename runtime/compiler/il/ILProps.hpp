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

#ifndef J9_ILPROPS_INCL
#define J9_ILPROPS_INCL

#include "il/OMRILProps.hpp"

namespace ILTypeProp
   {
   /**
    * Expands on the Type properties from the OMR layer. 
    */
   enum
      {
      DFP                               = LastOMRILTypeProp << 1,
      PackedDecimal                     = LastOMRILTypeProp << 2,
      UnicodeDecimal                    = LastOMRILTypeProp << 3,
      UnicodeDecimalSignLeading         = LastOMRILTypeProp << 4,
      UnicodeDecimalSignTrailing        = LastOMRILTypeProp << 5,
      ZonedDecimal                      = LastOMRILTypeProp << 6,
      ZonedDecimalSignLeadingEmbedded   = LastOMRILTypeProp << 7,
      ZonedDecimalSignLeadingSeparate   = LastOMRILTypeProp << 8,
      ZonedDecimalSignTrailingSeparate  = LastOMRILTypeProp << 9,
      };

   }

// Property flags for property word 4
namespace ILProp4
   {
   enum
      {
      // Available                     = 0x00000001,
      SetSign                          = 0x00000002,
      SetSignOnNode                    = 0x00000004, ///< the setSign value is tracked in a field on the node vs as a child (SetSign has the setSign value in a child)
      ModifyPrecision                  = 0x00000008,
      BinaryCodedDecimalOp             = 0x00000010,
      ConversionHasFraction            = 0x00000020,
      // Available                     = 0x00000040,
      PackedArithmeticOverflowMessage  = 0x00000080,
      DFPTestDataClass                 = 0x00000100,
      CanHaveStorageReferenceHint      = 0x00000200,
      CanHavePaddingAddress            = 0x00000400,
      CanUseStoreAsAnAccumulator       = 0x00000800,
      TrackLineNo                      = 0x00001000, ///< Indicates if the node's line number should be retained when the node is commoned
      // Available                     = 0x00002000,
      // Available                     = 0x00004000,
      // Available                     = 0x00008000,
      // Available                     = 0x00010000,
      // Available                     = 0x00020000,
      // Available                     = 0x00040000,
      // Available                     = 0x00080000,
      // Available                     = 0x00100000,
      // Available                     = 0x00200000,
      // Available                     = 0x00400000,
      // Available                     = 0x00800000,
      // Available                     = 0x01000000,
      // Available                     = 0x02000000,
      // Available                     = 0x04000000,
      // Available                     = 0x08000000,
      // Available                     = 0x10000000,
      // Available                     = 0x20000000,
      // Available                     = 0x40000000,
      // Available                     = 0x80000000,
      };
   }

#endif

