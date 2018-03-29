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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

//On zOS XLC linker can't handle files with same name at link time
//This workaround with pragma is needed. What this does is essentially
//give a different name to the codesection (csect) for this file. So it
//doesn't conflict with another file with same name.
#pragma csect(CODE,"J9Optimizer#C")
#pragma csect(STATIC,"J9Optimizer#S")
#pragma csect(TEST,"J9Optimizer#T")

#include "optimizer/Optimizer.hpp"

#include <stddef.h>                                       // for NULL
#include <stdint.h>                                       // for uint16_t
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"                             // for TR_Method
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "optimizer/AllocationSinking.hpp"
#include "optimizer/IdiomRecognition.hpp"
#include "optimizer/Inliner.hpp"                          // for TR_Inliner, etc
#include "optimizer/J9Inliner.hpp"                          // for TR_Inliner, etc
#include "optimizer/JitProfiler.hpp"
#include "optimizer/LiveVariablesForGC.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/OptimizationStrategies.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/PartialRedundancy.hpp"
#include "optimizer/ProfileGenerator.hpp"
#include "optimizer/SequentialStoreSimplifier.hpp"
#include "optimizer/SignExtendLoads.hpp"
#include "optimizer/StringBuilderTransformer.hpp"
#include "optimizer/SwitchAnalyzer.hpp"
#include "optimizer/DynamicLiteralPool.hpp"
#include "optimizer/EscapeAnalysis.hpp"
#include "optimizer/DataAccessAccelerator.hpp"
#include "optimizer/IsolatedStoreElimination.hpp"
#include "optimizer/RedundantBCDSignElimination.hpp"
#include "optimizer/LoopAliasRefiner.hpp"
#include "optimizer/MonitorElimination.hpp"
#include "optimizer/NewInitialization.hpp"
#include "optimizer/SinkStores.hpp"
#include "optimizer/SPMDParallelizer.hpp"
#include "optimizer/StringPeepholes.hpp"
#include "optimizer/StripMiner.hpp"
#include "optimizer/ValuePropagation.hpp"
#include "optimizer/TrivialDeadBlockRemover.hpp"
#include "optimizer/OSRGuardInsertion.hpp"
#include "optimizer/OSRGuardRemoval.hpp"
#include "optimizer/JProfilingBlock.hpp"
#include "optimizer/JProfilingValue.hpp"
#include "runtime/J9Profiler.hpp"
#include "optimizer/UnsafeFastPath.hpp"
#include "optimizer/VarHandleTransformer.hpp"


static const OptimizationStrategy J9EarlyGlobalOpts[] =
   {
   { OMR::stringBuilderTransformer             },
   { OMR::stringPeepholes                      }, // need stringpeepholes to catch bigdecimal patterns
   { OMR::methodHandleInvokeInliningGroup,  OMR::IfMethodHandleInvokes },
   { OMR::inlining                             },
   { OMR::osrGuardInsertion,                OMR::IfVoluntaryOSR       },
   { OMR::osrExceptionEdgeRemoval                       }, // most inlining is done by now
   { OMR::jProfilingBlock                      },
   { OMR::stringBuilderTransformer             },
   { OMR::stringPeepholes,                     },
   //{ basicBlockOrdering,          IfLoops }, // early ordering with no extension
   { OMR::treeSimplification,        OMR::IfEnabled },
   { OMR::compactNullChecks                    }, // cleans up after inlining; MUST be done before PRE
   { OMR::virtualGuardTailSplitter             }, // merge virtual guards
   { OMR::treeSimplification                   },
   { OMR::CFGSimplification                    },
   { OMR::endGroup                             }
   };

static const OptimizationStrategy J9EarlyLocalOpts[] =
   {
   { OMR::localValuePropagation                },
   //{ localValuePropagationGroup           },
   { OMR::localReordering                      },
   { OMR::switchAnalyzer                       },
   { OMR::treeSimplification,        OMR::IfEnabled }, // simplify any exprs created by LCP/LCSE
   { OMR::catchBlockRemoval                    }, // if all possible exceptions in a try were removed by inlining/LCP/LCSE
   { OMR::deadTreesElimination                 }, // remove any anchored dead loads
   { OMR::profiledNodeVersioning               },
   { OMR::endGroup                             }
   };

static const OptimizationStrategy signExtendLoadsOpts[] =
   {
   { OMR::signExtendLoads                      },
   { OMR::endGroup                             }
   };

// **************************************************************************
//
// Strategy that is used by full speed debug for methods that do share slots (the old FSD strategy before OSR)
//
// **************************************************************************
static const OptimizationStrategy fsdStrategyOptsForMethodsWithSlotSharing[] =
   {
   { OMR::trivialInlining,       OMR::IfNotFullInliningUnderOSRDebug   },         //added for fsd inlining
   { OMR::inlining,              OMR::IfFullInliningUnderOSRDebug      },         //added for fsd inlining
   { OMR::basicBlockExtension                           },
   { OMR::treeSimplification                            },         //added for fsd inlining
   { OMR::localCSE                                      },
   { OMR::treeSimplification                            },
   { OMR::cheapTacticalGlobalRegisterAllocatorGroup     },         // added for fsd gra
   { OMR::globalLiveVariablesForGC                      },
   { OMR::regDepCopyRemoval                             },
   { OMR::endOpts },
   };


// **************************************************************************
//
// Strategy that is used by full speed debug for methods that do not share slots
//
// **************************************************************************
static const OptimizationStrategy fsdStrategyOptsForMethodsWithoutSlotSharing[] =
   {
   { OMR::coldBlockOutlining },
   { OMR::trivialInlining,             OMR::IfNotFullInliningUnderOSRDebug   },         //added for fsd inlining
   { OMR::inlining,                    OMR::IfFullInliningUnderOSRDebug      },         //added for fsd inlining
   { OMR::virtualGuardTailSplitter                                              }, // merge virtual guards
   { OMR::treeSimplification                                                    },

   { OMR::CFGSimplification,           OMR::IfOptServer }, // for WAS trace folding
   { OMR::treeSimplification,          OMR::IfOptServer }, // for WAS trace folding
   { OMR::localCSE,                    OMR::IfEnabledAndOptServer }, // for WAS trace folding
   { OMR::treeSimplification,          OMR::IfEnabledAndOptServer }, // for WAS trace folding
   { OMR::globalValuePropagation,      },
   { OMR::treeSimplification,          OMR::IfEnabled },
   { OMR::cheapObjectAllocationGroup,  },
   { OMR::globalValuePropagation,      OMR::IfEnabled }, // if inlined a call or an object
   { OMR::treeSimplification,          OMR::IfEnabled },
   { OMR::catchBlockRemoval,           OMR::IfEnabled }, // if checks were removed
   { OMR::globalValuePropagation,      OMR::IfEnabledMarkLastRun}, // mark monitors requiring sync
   { OMR::virtualGuardTailSplitter,    OMR::IfEnabled }, // merge virtual guards
   { OMR::CFGSimplification            },
   { OMR::globalCopyPropagation,       },
   { OMR::lastLoopVersionerGroup,      OMR::IfLoops },
   { OMR::globalDeadStoreElimination,  OMR::IfLoops },
   { OMR::deadTreesElimination,        },
   { OMR::basicBlockOrdering,          OMR::IfLoops }, // required for loop reduction
   { OMR::treeSimplification           },
   { OMR::loopReduction                },
   { OMR::blockShuffling               }, // to stress idiom recognition
   { OMR::idiomRecognition,            OMR::IfLoops },
   { OMR::blockSplitter                },
   { OMR::treeSimplification           },
   { OMR::inductionVariableAnalysis,   OMR::IfLoopsAndNotProfiling },
   { OMR::generalLoopUnroller,         OMR::IfLoopsAndNotProfiling },
   { OMR::samplingJProfiling                   },
   { OMR::basicBlockExtension,         OMR::MarkLastRun                  }, // extend blocks; move trees around if reqd
   { OMR::treeSimplification           }, // revisit; not really required ?
   { OMR::localValuePropagationGroup,  },
   { OMR::arraycopyTransformation      },
   { OMR::treeSimplification,          OMR::IfEnabled },
   { OMR::localDeadStoreElimination,   }, // after latest copy propagation
   { OMR::deadTreesElimination,        }, // remove dead anchors created by check/store removal
   { OMR::treeSimplification,          OMR::IfEnabled },
   { OMR::localCSE                     },
   { OMR::treeSimplification,          OMR::MarkLastRun                },
#ifdef TR_HOST_S390
   { OMR::longRegAllocation                                                     },
#endif
   { OMR::andSimplification,           },  //clean up after versioner
   { OMR::compactNullChecks,           }, // cleanup at the end
   { OMR::deadTreesElimination,        OMR::IfEnabled }, // cleanup at the end
   { OMR::treesCleansing,              OMR::IfEnabled },
   { OMR::deadTreesElimination,        OMR::IfEnabled }, // cleanup at the end
   { OMR::localCSE,                    OMR::IfEnabled }, // common up expressions for sunk stores
   { OMR::treeSimplification,          OMR::IfEnabledMarkLastRun }, // cleanup the trees after sunk store and localCSE
   { OMR::dynamicLiteralPool,          },
   { OMR::localDeadStoreElimination,   OMR::IfEnabled }, //remove the astore if no literal pool is required
   { OMR::localCSE,                    OMR::IfEnabled },  //common up lit pool refs in the same block
   { OMR::deadTreesElimination,        OMR::IfEnabled }, // cleanup at the end
   { OMR::prefetchInsertionGroup,      OMR::IfLoops   }, // created IL should not be moved
   { OMR::treeSimplification,          OMR::IfEnabledMarkLastRun       }, // Simplify non-normalized address computations introduced by prefetch insertion
   { OMR::trivialDeadTreeRemoval,      OMR::IfEnabled }, // final cleanup before opcode expansion
   { OMR::globalDeadStoreElimination,            },
   { OMR::cheapTacticalGlobalRegisterAllocatorGroup, },
   { OMR::globalDeadStoreGroup,                  },
   { OMR::rematerialization,                     },
   { OMR::compactNullChecks,                     }, // cleanup at the end
   { OMR::deadTreesElimination,        OMR::IfEnabled }, // remove dead anchors created by check/store removal
   { OMR::deadTreesElimination,        OMR::IfEnabled }, // remove dead RegStores produced by previous deadTrees pass
   { OMR::globalLiveVariablesForGC                                              },
   { OMR::regDepCopyRemoval                                                     },
   { OMR::endOpts                                                               },
   };


static const OptimizationStrategy *fsdStrategies[] =
   {
   fsdStrategyOptsForMethodsWithSlotSharing,
   fsdStrategyOptsForMethodsWithoutSlotSharing
   };


// **********************************************************
//
// NO-OPT STRATEGY
//
// **********************************************************
static const OptimizationStrategy noOptStrategyOpts[] =
   {
   { OMR::trivialDeadTreeRemoval,  OMR::IfEnabled },
   { OMR::treeSimplification                      },
   { OMR::recompilationModifier,   OMR::IfEnabled },
   { OMR::globalLiveVariablesForGC, OMR::IfAggressiveLiveness },
   { OMR::endOpts                                 }
   };


// ***************************************************************************
//
// Strategy for cold methods. This is an early compile for methods known to have
// loops so it should have a light optimization load.
//
// ***************************************************************************

static const OptimizationStrategy coldStrategyOpts[] =
   {
   { OMR::trivialDeadTreeRemoval,                    OMR::IfEnabled                  },
   { OMR::coldBlockOutlining                                                    },
   { OMR::stringBuilderTransformer,                  OMR::IfNotQuickStart            },
   { OMR::stringPeepholes,                           OMR::IfNotQuickStart            }, // need stringpeepholes to catch bigdecimal patterns
   { OMR::trivialInlining                                                       },
   { OMR::jProfilingBlock                                                       },
   { OMR::virtualGuardTailSplitter                                              },
   { OMR::recompilationModifier,                     OMR::IfEnabled                  },
   { OMR::samplingJProfiling                                                    },
   { OMR::treeSimplification                                                    }, // cleanup before basicBlockExtension
   { OMR::basicBlockExtension                                                   },
   { OMR::localValuePropagationGroup                                            },
   { OMR::deadTreesElimination                                                  },
   { OMR::localCSE,                                  OMR::IfEnabled                  },
   { OMR::treeSimplification                                                    },
   { OMR::arraycopyTransformation                                               },
   { OMR::sequentialLoadAndStoreColdGroup,           OMR::IfEnabled                  }, // disabled by default, enabled by -Xjit:enableSequentialLoadStoreCold
   { OMR::localCSE,                                  OMR::IfEnabled                  },
   { OMR::treeSimplification,                                                   },
   { OMR::localDeadStoreElimination,                 OMR::IfEnabled                  },
   { OMR::deadTreesElimination,                      OMR::IfEnabled                  },
   { OMR::localCSE,                                  OMR::IfEnabled                  },
   { OMR::treeSimplification                                                    },
   { OMR::dynamicLiteralPool,                        OMR::IfNotProfiling             },
#ifdef TR_HOST_S390
   { OMR::longRegAllocation                                                     },
#endif
   { OMR::localCSE,                                  OMR::IfEnabled                  },
   { OMR::treeSimplification,                        OMR::MarkLastRun                },
   { OMR::rematerialization                                                     },
   { OMR::compactNullChecks,                         OMR::IfEnabled                  },
   { OMR::signExtendLoadsGroup,                      OMR::IfEnabled                  },
   { OMR::jProfilingValue,                           OMR::MustBeDone                 },
   { OMR::trivialDeadTreeRemoval,                                               },
   { OMR::cheapTacticalGlobalRegisterAllocatorGroup, OMR::IfAOTAndEnabled            },
   { OMR::globalLiveVariablesForGC,                  OMR::IfAggressiveLiveness  },
   { OMR::profilingGroup,                            OMR::IfProfiling                },
   { OMR::regDepCopyRemoval                                                     },
   { OMR::endOpts                                                               }
   };


// ***************************************************************************
//
// Strategy for warm methods. An initial number of invocations of the method
// have already happened, but this is the first compile of the method.
//
// ***************************************************************************
//
static const OptimizationStrategy warmStrategyOpts[] =
   {
   { OMR::trivialDeadTreeRemoval,                                      OMR::IfEnabled},
   { OMR::coldBlockOutlining                                                    },
   { OMR::stringBuilderTransformer                                              },
   { OMR::stringPeepholes                                                       }, // need stringpeepholes to catch bigdecimal patterns
   { OMR::methodHandleInvokeInliningGroup,                OMR::IfMethodHandleInvokes },
   { OMR::inlining                                                              },
   { OMR::osrGuardInsertion,                         OMR::IfVoluntaryOSR       },
   { OMR::osrExceptionEdgeRemoval                       }, // most inlining is done by now
   { OMR::jProfilingBlock                                                       },
   { OMR::virtualGuardTailSplitter                                              }, // merge virtual guards
   { OMR::treeSimplification                                                    },
   { OMR::sequentialLoadAndStoreWarmGroup,           OMR::IfEnabled                  }, // disabled by default, enabled by -Xjit:enableSequentialLoadStoreWarm
   { OMR::cheapGlobalValuePropagationGroup                                      },
   { OMR::dataAccessAccelerator                                                 }, // globalValuePropagation and inlinging might expose opportunities for dataAccessAccelerator
   { OMR::globalCopyPropagation,                       OMR::IfVoluntaryOSR          },
   { OMR::lastLoopVersionerGroup,                      OMR::IfLoops                  },
   { OMR::globalDeadStoreElimination,                  OMR::IfEnabledAndLoops},
   { OMR::deadTreesElimination                                                  },
   { OMR::recompilationModifier,                     OMR::IfEnabledAndNotProfiling   },
   { OMR::localReordering,                           OMR::IfNoLoopsOREnabledAndLoops }, // if required or if not done earlier
   { OMR::basicBlockOrdering,                        OMR::IfLoops                    }, // required for loop reduction
   { OMR::treeSimplification                                                    },
   { OMR::loopReduction                                                         },
   { OMR::blockShuffling                                                        }, // to stress idiom recognition
   { OMR::idiomRecognition,                   OMR::IfLoopsAndNotProfiling            },
   { OMR::blockSplitter                                                         },
   { OMR::treeSimplification                                                    },
   { OMR::inductionVariableAnalysis,          OMR::IfLoopsAndNotProfiling            },
   { OMR::generalLoopUnroller,                OMR::IfLoopsAndNotProfiling            },
   { OMR::virtualGuardHeadMerger                                                },
   { OMR::basicBlockExtension,                     OMR::MarkLastRun                  }, // extend blocks; move trees around if reqd
   { OMR::treeSimplification                                                    }, // revisit; not really required ?
   { OMR::localValuePropagationGroup                                            },
   { OMR::arraycopyTransformation                                               },
   { OMR::treeSimplification,                        OMR::IfEnabled                  },
   { OMR::redundantAsyncCheckRemoval,                OMR::IfNotJitProfiling          },
   { OMR::localDeadStoreElimination                                             }, // after latest copy propagation
   { OMR::deadTreesElimination                                                  }, // remove dead anchors created by check/store removal
   { OMR::treeSimplification,                        OMR::IfEnabled                  },
   { OMR::localCSE                                                              },
   { OMR::treeSimplification,                        OMR::MarkLastRun                },
#ifdef TR_HOST_S390
   { OMR::longRegAllocation                                                     },
#endif
   { OMR::andSimplification,                         OMR::IfEnabled                  },  //clean up after versioner
   { OMR::compactNullChecks                                                     }, // cleanup at the end
   { OMR::deadTreesElimination,                      OMR::IfEnabled                  }, // cleanup at the end
   { OMR::globalCopyPropagation,                     OMR::IfMethodHandleInvokes      }, // Does a lot of good after methodHandleInvokeInliningGroup
   { OMR::generalStoreSinking                                                   },
   { OMR::treesCleansing,                            OMR::IfEnabled                  },
   { OMR::deadTreesElimination,                      OMR::IfEnabled                  }, // cleanup at the end
   { OMR::localCSE,                                  OMR::IfEnabled                  }, // common up expressions for sunk stores
   { OMR::treeSimplification,                        OMR::IfEnabledMarkLastRun       }, // cleanup the trees after sunk store and localCSE
   { OMR::dynamicLiteralPool,                        OMR::IfNotProfiling             },
   { OMR::samplingJProfiling                                                    },
   { OMR::trivialBlockExtension                                                 },
   { OMR::localDeadStoreElimination,                 OMR::IfEnabled                  }, //remove the astore if no literal pool is required
   { OMR::localCSE,                                  OMR::IfEnabled  },  //common up lit pool refs in the same block
   { OMR::deadTreesElimination,                      OMR::IfEnabled                  }, // cleanup at the end
   { OMR::signExtendLoadsGroup,                      OMR::IfEnabled                  }, // last opt before GRA
   { OMR::prefetchInsertionGroup,                    OMR::IfLoops                    }, // created IL should not be moved
   { OMR::treeSimplification,                        OMR::IfEnabledMarkLastRun       }, // Simplify non-normalized address computations introduced by prefetch insertion
   { OMR::trivialDeadTreeRemoval,                    OMR::IfEnabled                  }, // final cleanup before opcode expansion
   { OMR::globalDeadStoreElimination,                OMR::IfVoluntaryOSR            },
   { OMR::arraysetStoreElimination                                              },
   { OMR::checkcastAndProfiledGuardCoalescer                                    },
   { OMR::jProfilingValue,                           OMR::MustBeDone                 },
   { OMR::cheapTacticalGlobalRegisterAllocatorGroup, OMR::IfEnabled                  },
   { OMR::globalDeadStoreGroup,                                                 },
   { OMR::rematerialization                                                     },
   { OMR::compactNullChecks,                         OMR::IfEnabled                  }, // cleanup at the end
   { OMR::deadTreesElimination,                      OMR::IfEnabled                  }, // remove dead anchors created by check/store removal
   { OMR::deadTreesElimination,                      OMR::IfEnabled                  }, // remove dead RegStores produced by previous deadTrees pass
   { OMR::compactLocals,                             OMR::IfNotJitProfiling          }, // analysis results are invalidated by profilingGroup
   { OMR::globalLiveVariablesForGC                                              },
   { OMR::profilingGroup,                            OMR::IfProfiling                },
   { OMR::regDepCopyRemoval                                                     },
   { OMR::endOpts                                                               }
   };


// ***************************************************************************
// A (possibly temporary) strategy for partially optimizing W-Code
// ***************************************************************************
//
static const OptimizationStrategy reducedWarmStrategyOpts[] =
   {
   { OMR::inlining                                                              },
   { OMR::osrGuardInsertion,                         OMR::IfVoluntaryOSR       },
   { OMR::osrExceptionEdgeRemoval                                               }, // most inlining is done by now
   { OMR::jProfilingBlock                                                       },
   { OMR::dataAccessAccelerator                                                 }, // immediate does unconditional dataAccessAccelerator after inlining
   { OMR::treeSimplification                                                    },
   { OMR::deadTreesElimination                                                  },
   { OMR::treeSimplification                                                    },
   { OMR::basicBlockExtension                                                   }, // extend blocks; move trees around if reqd
   { OMR::treeSimplification                                                    }, // revisit; not really required ?
   { OMR::localCSE                                                              },
   { OMR::treeSimplification,                        OMR::MarkLastRun                 },
   { OMR::deadTreesElimination,                      OMR::IfEnabled                  }, // cleanup at the end
   { OMR::jProfilingValue,                           OMR::MustBeDone                 },
   { OMR::cheapTacticalGlobalRegisterAllocatorGroup, OMR::IfEnabled                  },
   { OMR::endOpts                                                               }
   };


// ***************************************************************************
//
// Strategy for hot methods. The method has been compiled before and sampling
// has discovered that it is hot.
//
// ***************************************************************************
const OptimizationStrategy hotStrategyOpts[] =
   {
   { OMR::trivialDeadTreeRemoval,                OMR::IfEnabled                },
   { OMR::coldBlockOutlining },
   { OMR::earlyGlobalGroup                                                },
   { OMR::earlyLocalGroup                                                 },
   { OMR::stripMiningGroup,                      OMR::IfLoops                  }, // strip mining in loops
   { OMR::loopReplicator,                        OMR::IfLoops                  }, // tail-duplication in loops
   { OMR::blockSplitter,                         OMR::IfNews                   }, // treeSimplification + blockSplitter + VP => opportunity for EA
   { OMR::expensiveGlobalValuePropagationGroup                            },
   { OMR::dataAccessAccelerator                                           },
   { OMR::osrGuardRemoval,                       OMR::IfEnabled           }, // run after calls/monents/asyncchecks have been removed
   { OMR::globalDeadStoreGroup,                                           },
   { OMR::idiomRecognition,                      OMR::IfLoopsAndNotProfiling   }, // Early pass of idiomRecognition - Loop Canonicalizer transformations break certain idioms (i.e. arrayTranslateAndTest)
   { OMR::globalCopyPropagation,                 OMR::IfNoLoops       },
   { OMR::loopCanonicalizationGroup,             OMR::IfLoops                  }, // canonicalize loops (improve fall throughs)
   { OMR::inductionVariableAnalysis,             OMR::IfLoops                  },
   { OMR::redundantInductionVarElimination,      OMR::IfLoops                  },
   { OMR::loopAliasRefinerGroup,                 OMR::IfLoops     },
   { OMR::recompilationModifier,                 OMR::IfEnabledAndNotProfiling },
   { OMR::sequentialStoreSimplificationGroup,                             }, // reduce sequential stores into an arrayset
   { OMR::prefetchInsertionGroup,                OMR::IfLoops                  }, // created IL should not be moved
   { OMR::partialRedundancyEliminationGroup                               },
   { OMR::globalDeadStoreElimination,            OMR::IfLoopsAndNotProfiling   },
   { OMR::inductionVariableAnalysis,             OMR::IfLoopsAndNotProfiling   },
   { OMR::loopSpecializerGroup,                  OMR::IfLoopsAndNotProfiling   },
   { OMR::inductionVariableAnalysis,             OMR::IfLoopsAndNotProfiling   },
   { OMR::generalLoopUnroller,                   OMR::IfLoopsAndNotProfiling   }, // unroll Loops
   { OMR::blockManipulationGroup                                          },
   { OMR::lateLocalGroup                                                  },
   { OMR::redundantAsyncCheckRemoval,            OMR::IfNotJitProfiling        }, // optimize async check placement
   { OMR::recompilationModifier,                 OMR::IfProfiling              }, // do before GRA to avoid commoning of longs afterwards
   { OMR::globalCopyPropagation,                 OMR::IfMoreThanOneBlock       }, // Can produce opportunities for store sinking
   { OMR::generalStoreSinking                                             },


   { OMR::localCSE,                              OMR::IfEnabled                }, //common up lit pool refs in the same block
   { OMR::treeSimplification,                    OMR::IfEnabled                }, // cleanup the trees after sunk store and localCSE
   { OMR::dynamicLiteralPool,                    OMR::IfNotProfiling           },
   { OMR::trivialBlockExtension                                           },
   { OMR::localDeadStoreElimination,             OMR::IfEnabled                }, //remove the astore if no literal pool is required
   { OMR::localCSE,                              OMR::IfEnabled                }, //common up lit pool refs in the same block
   { OMR::deadTreesElimination,                  OMR::IfEnabled                }, // cleanup at the end
   { OMR::signExtendLoadsGroup,                  OMR::IfEnabled                }, // last opt before GRA
   { OMR::trivialDeadTreeRemoval,                OMR::IfEnabled                }, // final cleanup before opcode expansion
   { OMR::arraysetStoreElimination                                              },
   { OMR::localValuePropagation,                 OMR::MarkLastRun              },
   { OMR::arraycopyTransformation      },
   { OMR::checkcastAndProfiledGuardCoalescer                              },
   { OMR::jProfilingValue,                           OMR::MustBeDone           },
   { OMR::tacticalGlobalRegisterAllocatorGroup,  OMR::IfEnabled                },
   { OMR::globalDeadStoreElimination,            OMR::IfMoreThanOneBlock       }, // global dead store removal
   { OMR::deadTreesElimination                                            }, // cleanup after dead store removal
   { OMR::compactNullChecks                                               }, // cleanup at the end
   { OMR::finalGlobalGroup                                                }, // done just before codegen
   { OMR::profilingGroup,                        OMR::IfProfiling              },
   { OMR::regDepCopyRemoval                                               },
   { OMR::endOpts                                                         }
   };

// ***************************************************************************
//
// Strategy for very hot methods.  This is not currently used, same as hot.
//
// ***************************************************************************
const OptimizationStrategy veryHotStrategyOpts[] =
   {
   { OMR::hotStrategy },
   { OMR::endOpts     }
   };

// ***************************************************************************
//
// Strategy for scorching hot methods. This is the last time the method will
// be compiled, so throw everything (within reason) at it.
//
// ***************************************************************************
const OptimizationStrategy scorchingStrategyOpts[] =
   {
#if 0
   { OMR::hotStrategy                                        },
   { OMR::endOpts                                            }
#else
   { OMR::coldBlockOutlining },
   { OMR::earlyGlobalGroup                                   },
   { OMR::earlyLocalGroup                                    },
   { OMR::andSimplification                                  }, // needs commoning across blocks to work well; must be done after versioning
   { OMR::stripMiningGroup,                      OMR::IfLoops     }, // strip mining in loops
   { OMR::loopReplicator,                        OMR::IfLoops     }, // tail-duplication in loops
   { OMR::blockSplitter,                         OMR::IfNews      }, // treeSimplification + blockSplitter + VP => opportunity for EA
   { OMR::arrayPrivatizationGroup,               OMR::IfNews      }, // must preceed escape analysis
   { OMR::veryExpensiveGlobalValuePropagationGroup           },
   { OMR::dataAccessAccelerator                              }, //always run after GVP
   { OMR::osrGuardRemoval,                       OMR::IfEnabled }, // run after calls/monents/asyncchecks have been removed
   { OMR::globalDeadStoreGroup,                              },
   { OMR::idiomRecognition,                      OMR::IfLoopsAndNotProfiling   }, // Early pass of idiomRecognition - Loop Canonicalizer transformations break certain idioms (i.e. arrayTranslateAndTest)
   { OMR::globalCopyPropagation,                 OMR::IfNoLoops       },
   { OMR::loopCanonicalizationGroup,             OMR::IfLoops     }, // canonicalize loops (improve fall throughs)
   { OMR::inductionVariableAnalysis,             OMR::IfLoops     },
   { OMR::redundantInductionVarElimination,      OMR::IfLoops     },
   { OMR::loopAliasRefinerGroup,                 OMR::IfLoops     }, // version loops to improve aliasing (after versioned to reduce code growth)
   { OMR::expressionsSimplification,             OMR::IfLoops     },
   { OMR::recompilationModifier,                 OMR::IfEnabled   },

   { OMR::sequentialStoreSimplificationGroup                 }, // reduce sequential stores into an arrayset
   { OMR::prefetchInsertionGroup,                OMR::IfLoops     }, // created IL should not be moved
   { OMR::partialRedundancyEliminationGroup                  },
   { OMR::globalDeadStoreElimination,            OMR::IfLoops     },
   { OMR::inductionVariableAnalysis,             OMR::IfLoops     },
   { OMR::loopSpecializerGroup,                  OMR::IfLoops     },
   { OMR::inductionVariableAnalysis,             OMR::IfLoops     },
   { OMR::generalLoopUnroller,                   OMR::IfLoops     }, // unroll Loops
   { OMR::blockSplitter,                         OMR::MarkLastRun },
   { OMR::blockManipulationGroup                             },
   { OMR::lateLocalGroup                                     },
   { OMR::redundantAsyncCheckRemoval,            OMR::IfNotJitProfiling        }, // optimize async check placement
   { OMR::recompilationModifier,                 OMR::IfProfiling              }, // do before GRA to avoid commoning of longs afterwards
   { OMR::globalCopyPropagation,                 OMR::IfMoreThanOneBlock       }, // Can produce opportunities for store sinking
   { OMR::generalStoreSinking                                             },
   { OMR::localCSE,                              OMR::IfEnabled                }, //common up lit pool refs in the same block
   { OMR::treeSimplification,                    OMR::IfEnabled     }, // cleanup the trees after sunk store and localCSE
   { OMR::dynamicLiteralPool,                    OMR::IfNotProfiling             },
   { OMR::trivialBlockExtension                              },
   { OMR::localDeadStoreElimination,             OMR::IfEnabled                  }, //remove the astore if no literal pool is required
   { OMR::localCSE,                              OMR::IfEnabled  },  //common up lit pool refs in the same block
   { OMR::signExtendLoadsGroup,                  OMR::IfEnabled                }, // last opt before GRA
   { OMR::arraysetStoreElimination                                              },
   { OMR::localValuePropagation,                 OMR::MarkLastRun              },
   { OMR::arraycopyTransformation      },
   { OMR::checkcastAndProfiledGuardCoalescer      },
   { OMR::jProfilingValue,                           OMR::MustBeDone                 },
   { OMR::tacticalGlobalRegisterAllocatorGroup,  OMR::IfEnabled   },
   { OMR::globalDeadStoreElimination,            OMR::IfMoreThanOneBlock }, // global dead store removal
   { OMR::deadTreesElimination                               }, // cleanup after dead store removal
   { OMR::compactNullChecks                                  }, // cleanup at the end
   { OMR::finalGlobalGroup                                   }, // done just before codegen
   { OMR::profilingGroup,                        OMR::IfProfiling },
   { OMR::regDepCopyRemoval                                  },
   { OMR::endOpts                                            }
#endif
   };

const OptimizationStrategy sequentialLoadAndStoreColdOpts[] =
   {
   { OMR::localDeadStoreElimination                                             },
   { OMR::deadTreesElimination                                                  },
   { OMR::expensiveGlobalValuePropagationGroup                                  },
   { OMR::sequentialStoreSimplificationGroup                                    },
   { OMR::endGroup                                                              }
   };

const OptimizationStrategy sequentialLoadAndStoreWarmOpts[] =
   {
   { OMR::localValuePropagationGroup                                            },
   { OMR::localDeadStoreElimination                                             },
   { OMR::deadTreesElimination                                                  },
   { OMR::expensiveGlobalValuePropagationGroup                                  },
   { OMR::sequentialStoreSimplificationGroup                                    },
   { OMR::endGroup                                                              }
   };

const OptimizationStrategy sequentialStoreSimplificationOpts[] =
   {
   { OMR::treeSimplification },
   { OMR::sequentialStoreSimplification        },
   { OMR::treeSimplification                   }, // might fold expressions created by versioning/induction variables
   { OMR::endGroup                             }
   };


// **********************************************************
//
// AHEAD-OF-TIME-COMPILATION STRATEGY
//
// **********************************************************
static const OptimizationStrategy AOTStrategyOpts[] =
   {
   { OMR::earlyGlobalGroup                                 },
   { OMR::earlyLocalGroup                                  },
   { OMR::stripMiningGroup,                      OMR::IfLoops   }, // strip mining in loops
   { OMR::loopReplicator,                        OMR::IfLoops   }, // tail-duplication in loops
   { OMR::expensiveGlobalValuePropagationGroup             },
   { OMR::globalDeadStoreGroup,                            },
   { OMR::globalCopyPropagation,                 OMR::IfNoLoops },
   { OMR::loopCanonicalizationGroup,             OMR::IfLoops   }, // canonicalize loops (improve fall throughs) and versioning
   { OMR::partialRedundancyEliminationGroup                },
   { OMR::globalDeadStoreElimination,            OMR::IfLoops   },
   { OMR::sequentialStoreSimplificationGroup               }, // reduce sequential stores into an arrayset
   { OMR::generalLoopUnroller,                   OMR::IfLoops   }, // unroll Loops
   { OMR::blockManipulationGroup                           },
   { OMR::lateLocalGroup                                   },
   { OMR::redundantAsyncCheckRemoval,                OMR::IfNotJitProfiling          }, // optimize async check placement
   { OMR::dynamicLiteralPool,                        OMR::IfNotProfiling             },
   { OMR::localDeadStoreElimination,                 OMR::IfEnabled                  }, //remove the astore if no literal pool is required
   { OMR::localCSE,                              OMR::IfEnabled  },  //common up lit pool refs in the same block
   { OMR::signExtendLoadsGroup,                  OMR::IfEnabled }, // last opt before GRA
   { OMR::arraysetStoreElimination                                              },
   { OMR::tacticalGlobalRegisterAllocatorGroup,  OMR::IfEnabled },
   { OMR::globalCopyPropagation,                 OMR::IfMoreThanOneBlock}, // global copy propagation
   { OMR::globalDeadStoreElimination,            OMR::IfMoreThanOneBlock}, // global dead store removal
   { OMR::deadTreesElimination                             }, // cleanup after dead store removal
   { OMR::compactNullChecks                                }, // cleanup at the end
   { OMR::prefetchInsertionGroup,                OMR::IfLoops   }, // created IL should not be moved
   { OMR::finalGlobalGroup                                 }, // done just before codegen
   { OMR::regDepCopyRemoval                                },
   { OMR::endOpts                                          }
   };


static const OptimizationStrategy *j9CompilationStrategies[] =
   {
   noOptStrategyOpts,
   coldStrategyOpts,
   warmStrategyOpts,
   hotStrategyOpts,
   veryHotStrategyOpts,
   scorchingStrategyOpts,
   AOTStrategyOpts,
   reducedWarmStrategyOpts
   };


// ***************************************************************************
//
// Cheaper strategy for warm methods.
//
// ***************************************************************************
//
static const OptimizationStrategy cheapWarmStrategyOpts[] =
   {
   { OMR::trivialDeadTreeRemoval,                    OMR::IfEnabled                  },
   { OMR::coldBlockOutlining                                                    },
   { OMR::stringBuilderTransformer                                              },
   { OMR::stringPeepholes                                                       }, // need stringpeepholes to catch bigdecimal patterns
   { OMR::methodHandleInvokeInliningGroup,           OMR::IfMethodHandleInvokes      },
   { OMR::inlining                                                              },
   { OMR::osrGuardInsertion,                         OMR::IfVoluntaryOSR       },
   { OMR::osrExceptionEdgeRemoval                                               }, // most inlining is done by now
   { OMR::jProfilingBlock                                                       },
   { OMR::virtualGuardTailSplitter                                              }, // merge virtual guards
   { OMR::treeSimplification                                                    },
#ifdef TR_HOST_S390
   { OMR::sequentialLoadAndStoreWarmGroup,           OMR::IfEnabled                  },
#endif
   { OMR::cheapGlobalValuePropagationGroup                                      },
   { OMR::dataAccessAccelerator                                                 },
#ifdef TR_HOST_S390
   { OMR::globalCopyPropagation,                     OMR::IfVoluntaryOSR            },
#endif
   { OMR::lastLoopVersionerGroup,                    OMR::IfLoops                    },
#ifdef TR_HOST_S390
   { OMR::globalDeadStoreElimination,                OMR::IfEnabledAndLoops          },
   { OMR::deadTreesElimination                                                       },
   { OMR::recompilationModifier,                     OMR::IfEnabledAndNotProfiling   },
   { OMR::localReordering,                           OMR::IfNoLoopsOREnabledAndLoops },
   { OMR::basicBlockOrdering,                        OMR::IfLoops                    },
   { OMR::treeSimplification                                                         },
   { OMR::loopReduction                                                              },
   { OMR::blockShuffling                                                             },
#endif
   { OMR::localCSE,                                  OMR::IfLoopsAndNotProfiling     },
   { OMR::idiomRecognition,                          OMR::IfLoopsAndNotProfiling     },
   { OMR::blockSplitter                                                         },
   { OMR::treeSimplification                                                    }, // revisit; not really required ?
   { OMR::virtualGuardHeadMerger                                                },
   { OMR::basicBlockExtension,                       OMR::MarkLastRun                }, // extend blocks; move trees around if reqd
   { OMR::localValuePropagationGroup                                            },
   { OMR::arraycopyTransformation                                               },
   { OMR::treeSimplification,                        OMR::IfEnabled                  },
   { OMR::asyncCheckInsertion,                       OMR::IfNotJitProfiling          },
   { OMR::localCSE                                                              },
   { OMR::treeSimplification,                        OMR::MarkLastRun                },
#ifdef TR_HOST_S390
   { OMR::longRegAllocation                                                     },
#endif
   { OMR::andSimplification,                         OMR::IfEnabled                  },  //clean up after versioner
   { OMR::compactNullChecks                                                     }, // cleanup at the end
   { OMR::deadTreesElimination,                      OMR::IfEnabled                  }, // cleanup at the end
   { OMR::globalCopyPropagation,                     OMR::IfMethodHandleInvokes      }, // Does a lot of good after methodHandleInvokeInliningGroup
   { OMR::treesCleansing,                            OMR::IfEnabled                  },
   { OMR::deadTreesElimination,                      OMR::IfEnabled                  }, // cleanup at the end
   { OMR::localCSE,                                  OMR::IfEnabled                  }, // common up expressions for sunk stores
   { OMR::treeSimplification,                        OMR::IfEnabledMarkLastRun       }, // cleanup the trees after sunk store and localCSE
   
   /** \breif
    *      This optimization is performance critical on z Systems. On z Systems a literal pool register is blocked off
    *      by default at the start of the compilation since materializing this address could be expensive depending on
    *      the architecture level we are executing on. This optimization pass validates support for dynamically 
    *      materializing the literal pool address and frees up the literal pool register for register allocation.
   */
   { OMR::dynamicLiteralPool,                        OMR::IfNotProfiling             },
   { OMR::samplingJProfiling                                                         },
   { OMR::trivialBlockExtension                                                 },
   { OMR::localCSE,                                  OMR::IfEnabled                  },  //common up lit pool refs in the same block
   { OMR::deadTreesElimination,                      OMR::IfEnabled                  }, // cleanup at the end
   { OMR::treeSimplification,                        OMR::IfEnabledMarkLastRun       }, // Simplify non-normalized address computations introduced by prefetch insertion
   { OMR::trivialDeadTreeRemoval,                    OMR::IfEnabled                  }, // final cleanup before opcode expansion
   { OMR::jProfilingValue,                           OMR::MustBeDone                 },
   { OMR::cheapTacticalGlobalRegisterAllocatorGroup, OMR::IfEnabled                  },
   { OMR::globalDeadStoreGroup,                                                 },
   { OMR::compactNullChecks,                         OMR::IfEnabled                  }, // cleanup at the end
   { OMR::deadTreesElimination,                      OMR::IfEnabled                  }, // remove dead anchors created by check/store removal
   { OMR::deadTreesElimination,                      OMR::IfEnabled                  }, // remove dead RegStores produced by previous deadTrees pass
   { OMR::compactLocals,                             OMR::IfNotJitProfiling          }, // analysis results are invalidated by profilingGroup
   { OMR::globalLiveVariablesForGC                                              },
   { OMR::profilingGroup,                            OMR::IfProfiling                },
   { OMR::regDepCopyRemoval                                                     },
   { OMR::endOpts                                                               }
   };


static const OptimizationStrategy profilingOpts[] =
   {
   { OMR::profileGenerator,          OMR::MustBeDone },
   { OMR::deadTreesElimination,      OMR::IfEnabled  },
   { OMR::endGroup                              }
   };

static const OptimizationStrategy cheapTacticalGlobalRegisterAllocatorOpts[] =
   {
   { OMR::redundantGotoElimination,        OMR::IfNotJitProfiling }, // need to be run before global register allocator
   { OMR::tacticalGlobalRegisterAllocator, OMR::IfEnabled },
   { OMR::endGroup                        }
   };


J9::Optimizer::Optimizer(TR::Compilation *comp, TR::ResolvedMethodSymbol *methodSymbol, bool isIlGen,
      const OptimizationStrategy *strategy, uint16_t VNType)
   : OMR::Optimizer(comp, methodSymbol, isIlGen, strategy, VNType)
   {
   // initialize additional J9 optimizations

   _opts[OMR::inlining] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_Inliner::create, OMR::inlining);
   _opts[OMR::targetedInlining] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_Inliner::create, OMR::targetedInlining);
   _opts[OMR::targetedInlining]->setOptPolicy(new (comp->allocator()) TR_J9JSR292InlinerPolicy(comp));

   _opts[OMR::trivialInlining] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_TrivialInliner::create, OMR::trivialInlining);

   _opts[OMR::dynamicLiteralPool] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_DynamicLiteralPool::create, OMR::dynamicLiteralPool);
   _opts[OMR::arraycopyTransformation] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR::ArraycopyTransformation::create, OMR::arraycopyTransformation);
   _opts[OMR::signExtendLoads] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_SignExtendLoads::create, OMR::signExtendLoads);
   _opts[OMR::sequentialStoreSimplification] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_SequentialStoreSimplifier::create, OMR::sequentialStoreSimplification);
   _opts[OMR::explicitNewInitialization] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_LocalNewInitialization::create, OMR::explicitNewInitialization);
   _opts[OMR::redundantMonitorElimination] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR::MonitorElimination::create, OMR::redundantMonitorElimination);
   _opts[OMR::escapeAnalysis] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_EscapeAnalysis::create, OMR::escapeAnalysis);
   _opts[OMR::isolatedStoreElimination] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_IsolatedStoreElimination::create, OMR::isolatedStoreElimination);
   _opts[OMR::localLiveVariablesForGC] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_LocalLiveVariablesForGC::create, OMR::localLiveVariablesForGC);
   _opts[OMR::globalLiveVariablesForGC] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_GlobalLiveVariablesForGC::create, OMR::globalLiveVariablesForGC);
   _opts[OMR::recompilationModifier] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_RecompilationModifier::create, OMR::recompilationModifier);
   _opts[OMR::profileGenerator] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_ProfileGenerator::create, OMR::profileGenerator);
   _opts[OMR::dataAccessAccelerator] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_DataAccessAccelerator::create, OMR::dataAccessAccelerator);
   _opts[OMR::stringBuilderTransformer] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_StringBuilderTransformer::create, OMR::stringBuilderTransformer);
   _opts[OMR::stringPeepholes] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_StringPeepholes::create, OMR::stringPeepholes);
   _opts[OMR::switchAnalyzer] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR::SwitchAnalyzer::create, OMR::switchAnalyzer);
   _opts[OMR::varHandleTransformer] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_VarHandleTransformer::create, OMR::varHandleTransformer);
   _opts[OMR::unsafeFastPath] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_UnsafeFastPath::create, OMR::unsafeFastPath);
   _opts[OMR::trivialStoreSinking] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_TrivialSinkStores::create, OMR::trivialStoreSinking);
   _opts[OMR::idiomRecognition] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_CISCTransformer::create, OMR::idiomRecognition);
   _opts[OMR::loopAliasRefiner] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_LoopAliasRefiner::create, OMR::loopAliasRefiner);
   _opts[OMR::allocationSinking] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_AllocationSinking::create, OMR::allocationSinking);
   _opts[OMR::samplingJProfiling] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_JitProfiler::create, OMR::samplingJProfiling);
   _opts[OMR::redundantBCDSignElimination] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_RedundantBCDSignElimination::create, OMR::redundantBCDSignElimination);
   _opts[OMR::SPMDKernelParallelization] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_SPMDKernelParallelizer::create, OMR::SPMDKernelParallelization);
   _opts[OMR::trivialDeadBlockRemover] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_TrivialDeadBlockRemover::create, OMR::trivialDeadBlockRemover);
   _opts[OMR::osrGuardInsertion] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_OSRGuardInsertion::create, OMR::osrGuardInsertion);
   _opts[OMR::osrGuardRemoval] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_OSRGuardRemoval::create, OMR::osrGuardRemoval);
   _opts[OMR::jProfilingBlock] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_JProfilingBlock::create, OMR::jProfilingBlock);
   _opts[OMR::jProfilingValue] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_JProfilingValue::create, OMR::jProfilingValue);
   // NOTE: Please add new J9 optimizations here!

   // initialize additional J9 optimization groups

   _opts[OMR::loopAliasRefinerGroup] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::loopAliasRefinerGroup, loopAliasRefinerOpts);
   _opts[OMR::cheapObjectAllocationGroup] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::cheapObjectAllocationGroup, cheapObjectAllocationOpts);
   _opts[OMR::expensiveObjectAllocationGroup] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::expensiveObjectAllocationGroup, expensiveObjectAllocationOpts);
   _opts[OMR::eachEscapeAnalysisPassGroup] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::eachEscapeAnalysisPassGroup, eachEscapeAnalysisPassOpts);
   _opts[OMR::cheapGlobalValuePropagationGroup] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::cheapGlobalValuePropagationGroup, cheapGlobalValuePropagationOpts);
   _opts[OMR::expensiveGlobalValuePropagationGroup] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::expensiveGlobalValuePropagationGroup, expensiveGlobalValuePropagationOpts);
   _opts[OMR::earlyGlobalGroup] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::earlyGlobalGroup, J9EarlyGlobalOpts);
   _opts[OMR::earlyLocalGroup] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::earlyLocalGroup, J9EarlyLocalOpts);
   _opts[OMR::isolatedStoreGroup] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::isolatedStoreGroup, isolatedStoreOpts);
   _opts[OMR::cheapTacticalGlobalRegisterAllocatorGroup] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::cheapTacticalGlobalRegisterAllocatorGroup, cheapTacticalGlobalRegisterAllocatorOpts);
   _opts[OMR::sequentialStoreSimplificationGroup] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::sequentialStoreSimplificationGroup, sequentialStoreSimplificationOpts);
   _opts[OMR::signExtendLoadsGroup] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::signExtendLoadsGroup, signExtendLoadsOpts);
   _opts[OMR::loopSpecializerGroup] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::loopSpecializerGroup, loopSpecializerOpts);
   _opts[OMR::profilingGroup] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::profilingGroup, profilingOpts);
   _opts[OMR::sequentialLoadAndStoreColdGroup] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::sequentialLoadAndStoreColdGroup, sequentialLoadAndStoreColdOpts);
   _opts[OMR::sequentialLoadAndStoreWarmGroup] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::sequentialLoadAndStoreWarmGroup, sequentialLoadAndStoreWarmOpts);

   _opts[OMR::noOptStrategy] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::noOptStrategy, noOptStrategyOpts);
   _opts[OMR::coldStrategy] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::coldStrategy, coldStrategyOpts);
   _opts[OMR::warmStrategy] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::warmStrategy, warmStrategyOpts);
   _opts[OMR::hotStrategy] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::hotStrategy, hotStrategyOpts);
   _opts[OMR::veryHotStrategy] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::veryHotStrategy, veryHotStrategyOpts);
   _opts[OMR::scorchingStrategy] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::scorchingStrategy, scorchingStrategyOpts);

   // NOTE: Please add new J9 optimization groups here!

   // turn requested on for optimizations/groups
   self()->setRequestOptimization(OMR::eachExpensiveGlobalValuePropagationGroup, true);

   self()->setRequestOptimization(OMR::cheapTacticalGlobalRegisterAllocatorGroup, true);
   self()->setRequestOptimization(OMR::tacticalGlobalRegisterAllocatorGroup, true);

   self()->setRequestOptimization(OMR::tacticalGlobalRegisterAllocator, true);

   if (shouldEnableSEL(comp))
     self()->setRequestOptimization(OMR::signExtendLoadsGroup, true);
   if (comp->getOption(TR_EnableSequentialLoadStoreWarm))
      self()->setRequestOptimization(OMR::sequentialLoadAndStoreWarmGroup, true);
   if (comp->getOption(TR_EnableSequentialLoadStoreCold))
      self()->setRequestOptimization(OMR::sequentialLoadAndStoreColdGroup, true);
   }

inline
TR::Optimizer *J9::Optimizer::self()
   {
   return (static_cast<TR::Optimizer *>(this));
   }

OMR_InlinerPolicy *J9::Optimizer::getInlinerPolicy()
   {
   return new (comp()->allocator()) TR_J9InlinerPolicy(comp());
   }


OMR_InlinerUtil *J9::Optimizer::getInlinerUtil()
   {
   return new (comp()->allocator()) TR_J9InlinerUtil(comp());
   }

bool
J9::Optimizer::switchToProfiling(uint32_t f, uint32_t c)
   {
   TR::Recompilation *recomp = comp()->getRecompilationInfo();
   if (!recomp) return false;
   if (!recomp->shouldBeCompiledAgain()) return false; // do not profile if do not intend to compile again
   if (!recomp->switchToProfiling(f, c)) return false;
   setRequestOptimization(OMR::recompilationModifier, true);
   setRequestOptimization(OMR::profileGenerator,      true);
   return true;
   }


bool
J9::Optimizer::switchToProfiling()
   {
   return self()->switchToProfiling(DEFAULT_PROFILING_FREQUENCY, DEFAULT_PROFILING_COUNT);
   }


const OptimizationStrategy *
J9::Optimizer::optimizationStrategy(TR::Compilation *c)
   {
   if (c->getOption(TR_MimicInterpreterFrameShape))
      {
      if (c->getJittedMethodSymbol()->sharesStackSlots(c))
         return fsdStrategies[0];       //0 is fsdStrategyOptsForMethodsWithSlotSharing
      else
         return fsdStrategies[1];       // 1 is fsdStrategyOptsForMethodsWithoutSlotSharing
      }

   TR_Hotness strategy = c->getMethodHotness();
   if (strategy == warm && !c->getOption(TR_DisableCheapWarmOpts))
      {
      return cheapWarmStrategyOpts;
      }
   else
      {
      return j9CompilationStrategies[strategy];
      }
   }


ValueNumberInfoBuildType
J9::Optimizer::valueNumberInfoBuildType()
   {
   return PrePartitionVN;
   }
