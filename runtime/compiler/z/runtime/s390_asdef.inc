ZZ Copyright (c) 2000, 2018 IBM Corp. and others
ZZ
ZZ This program and the accompanying materials are made 
ZZ available under the terms of the Eclipse Public License 2.0 
ZZ which accompanies this distribution and is available at 
ZZ https://www.eclipse.org/legal/epl-2.0/ or the Apache License, 
ZZ Version 2.0 which accompanies this distribution and is available 
ZZ at https://www.apache.org/licenses/LICENSE-2.0.
ZZ
ZZ This Source Code may also be made available under the following
ZZ Secondary Licenses when the conditions for such availability set
ZZ forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
ZZ General Public License, version 2 with the GNU Classpath 
ZZ Exception [1] and GNU General Public License, version 2 with the
ZZ OpenJDK Assembly Exception [2].
ZZ
ZZ [1] https://www.gnu.org/software/classpath/license.html
ZZ [2] http://openjdk.java.net/legal/assembly-exception.html
ZZ
ZZ SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR
ZZ GPL-2.0 WITH Classpath-exception-2.0 OR
ZZ LicenseRef-GPL-2.0 WITH Assembly-exception

ifdef([TR_HOST_64BIT],[dnl

ZZ
ZZ ===================================================================
ZZ   64bit instructions
ZZ ===================================================================
ZZ

define([A_GPR],ag)
define([AHI_GPR],aghi)
define([AL_GPR],alg)
define([AL32_GPR],algf)
define([ALR_GPR],algr)
define([AR_GPR],agr)
define([AGFR_GPR],agfr)
define([AGF_GPR],agf)
define([BCTR_GPR],bctgr)
define([BRCT_GPR],brctg)
define([C_GPR],cg)
define([CHI_GPR],cghi)
define([CDS_GPR],cdsg)
define([CL_GPR],clg)
define([CLM_GPR],clmh)
define([CLR_GPR],clgr)
define([CR_GPR],cgr)
define([CS_GPR],csg)
define([ICMH_GPR],icmh)
define([LCR_GPR],lcgr)
define([L_GPR],lg)
define([LDT_GPR],ltgr)
define([LGF_GPR],lgf)
define([LGFR_GPR],lgfr)
define([LR_GPR],lgr)
define([LH_GPR],lgh)
define([LHI_GPR],lghi)
define([LM_GPR],lmg)
define([LNR_GPR],lngr)
define([LTR_GPR],ltgr)
define([MSGR_GPR],msgr)
define([N_GPR],ng)
define([NIHH_GPR],nihh)
define([NR_GPR],ngr)
define([O_GPR],og)
define([S_GPR],sg)
define([SL32_GPR],slgf)
define([SLA_GPR],[slag $1,$1,$2])
define([SLL_GPR],[sllg $1,$1,$2])
define([SLR_GPR],slgr)
define([SR_GPR],sgr)
define([SRA_GPR],[srag $1,$1,$2])
define([SRL_GPR],[srlg $1,$1,$2])
define([ST_GPR],stg)
define([STDG_GPR],stdg)
define([STM_GPR],stmg)
define([STCMH_GPR],stcmh)
define([XR_GPR],xgr)

ZZ
ZZ ===================================================================
ZZ   End of 64bit instructions
ZZ ===================================================================
ZZ

],[dnl

ZZ
ZZ ===================================================================
ZZ   31 bit instructions
ZZ ===================================================================
ZZ

define([A_GPR],a)
define([AHI_GPR],ahi)
define([AL_GPR],al)
define([AL32_GPR],al)
define([ALR_GPR],alr)
define([AR_GPR],ar)
define([AGFR_GPR],ar)
define([AGF_GPR],a)
define([BCTR_GPR],bctr)
define([BRCT_GPR],brct)
define([C_GPR],c)
define([CHI_GPR],chi)
define([CDS_GPR],cds)
define([CL_GPR],cl)
define([CLM_GPR],clm)
define([CLR_GPR],clr)
define([CR_GPR],cr)
define([CS_GPR],cs)
define([ICM_GPR],icm)
define([IC_GPR],ic)
define([L_GPR],l)
define([LCR_GPR],lcr)
define([LDT_GPR],ltr)
define([LGF_GPR],l)
define([LR_GPR],lr)
define([LGFR_GPR],lr)
define([LH_GPR],lh)
define([LHI_GPR],lhi)
define([LM_GPR],lm)
define([LMH_GPR],lmh)
define([LNR_GPR],lnr)
define([LTR_GPR],ltr)
define([N_GPR],n)
define([NR_GPR],nr)
define([O_GPR],o)
define([S_GPR],s)
define([SL32_GPR],sl)
define([SLA_GPR],[sla $1,$2])
define([SLL_GPR],[sll $1,$2])
define([SLR_GPR],slr)
define([SR_GPR],sr)
define([SRA_GPR],[sra $1,$2])
define([SRL_GPR],[srl $1,$2])
define([ST_GPR],st)
define([STM_GPR],stm)
define([STMH_GPR],stmh)
define([XR_GPR],xr)

ZZ
ZZ ===================================================================
ZZ   End of 31 bit instructions
ZZ ===================================================================
ZZ

])dnl
