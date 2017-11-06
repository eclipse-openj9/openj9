/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#ifndef BCNAMES_H
#define BCNAMES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "j9comp.h"
extern J9_CDATA char * const JavaBCNames[];
extern J9_CDATA char * const sunJavaBCNames[];

#define JBnop 0
#define JBaconstnull 1
#define JBiconstm1 2
#define JBiconst0 3
#define JBiconst1 4
#define JBiconst2 5
#define JBiconst3 6
#define JBiconst4 7
#define JBiconst5 8
#define JBlconst0 9
#define JBlconst1 10
#define JBfconst0 11
#define JBfconst1 12
#define JBfconst2 13
#define JBdconst0 14
#define JBdconst1 15
#define JBbipush 16
#define JBsipush 17
#define JBldc 18
#define JBldcw 19
#define JBldc2lw 20
#define JBiload 21
#define JBlload 22
#define JBfload 23
#define JBdload 24
#define JBaload 25
#define JBiload0 26
#define JBiload1 27
#define JBiload2 28
#define JBiload3 29
#define JBlload0 30
#define JBlload1 31
#define JBlload2 32
#define JBlload3 33
#define JBfload0 34
#define JBfload1 35
#define JBfload2 36
#define JBfload3 37
#define JBdload0 38
#define JBdload1 39
#define JBdload2 40
#define JBdload3 41
#define JBaload0 42
#define JBaload1 43
#define JBaload2 44
#define JBaload3 45
#define JBiaload 46
#define JBlaload 47
#define JBfaload 48
#define JBdaload 49
#define JBaaload 50
#define JBbaload 51
#define JBcaload 52
#define JBsaload 53
#define JBistore 54
#define JBlstore 55
#define JBfstore 56
#define JBdstore 57
#define JBastore 58
#define JBistore0 59
#define JBistore1 60
#define JBistore2 61
#define JBistore3 62
#define JBlstore0 63
#define JBlstore1 64
#define JBlstore2 65
#define JBlstore3 66
#define JBfstore0 67
#define JBfstore1 68
#define JBfstore2 69
#define JBfstore3 70
#define JBdstore0 71
#define JBdstore1 72
#define JBdstore2 73
#define JBdstore3 74
#define JBastore0 75
#define JBastore1 76
#define JBastore2 77
#define JBastore3 78
#define JBiastore 79
#define JBlastore 80
#define JBfastore 81
#define JBdastore 82
#define JBaastore 83
#define JBbastore 84
#define JBcastore 85
#define JBsastore 86
#define JBpop 87
#define JBpop2 88
#define JBdup 89
#define JBdupx1 90
#define JBdupx2 91
#define JBdup2 92
#define JBdup2x1 93
#define JBdup2x2 94
#define JBswap 95
#define JBiadd 96
#define JBladd 97
#define JBfadd 98
#define JBdadd 99
#define JBisub 100
#define JBlsub 101
#define JBfsub 102
#define JBdsub 103
#define JBimul 104
#define JBlmul 105
#define JBfmul 106
#define JBdmul 107
#define JBidiv 108
#define JBldiv 109
#define JBfdiv 110
#define JBddiv 111
#define JBirem 112
#define JBlrem 113
#define JBfrem 114
#define JBdrem 115
#define JBineg 116
#define JBlneg 117
#define JBfneg 118
#define JBdneg 119
#define JBishl 120
#define JBlshl 121
#define JBishr 122
#define JBlshr 123
#define JBiushr 124
#define JBlushr 125
#define JBiand 126
#define JBland 127
#define JBior 128
#define JBlor 129
#define JBixor 130
#define JBlxor 131
#define JBiinc 132
#define JBi2l 133
#define JBi2f 134
#define JBi2d 135
#define JBl2i 136
#define JBl2f 137
#define JBl2d 138
#define JBf2i 139
#define JBf2l 140
#define JBf2d 141
#define JBd2i 142
#define JBd2l 143
#define JBd2f 144
#define JBi2b 145
#define JBi2c 146
#define JBi2s 147
#define JBlcmp 148
#define JBfcmpl 149
#define JBfcmpg 150
#define JBdcmpl 151
#define JBdcmpg 152
#define JBifeq 153
#define JBifne 154
#define JBiflt 155
#define JBifge 156
#define JBifgt 157
#define JBifle 158
#define JBificmpeq 159
#define JBificmpne 160
#define JBificmplt 161
#define JBificmpge 162
#define JBificmpgt 163
#define JBificmple 164
#define JBifacmpeq 165
#define JBifacmpne 166
#define JBgoto 167
#define JBtableswitch 170
#define JBlookupswitch 171
#define JBreturn0 172
#define JBreturn1 173
#define JBreturn2 174
#define JBsyncReturn0 175
#define JBsyncReturn1 176
#define JBsyncReturn2 177
#define JBgetstatic 178
#define JBputstatic 179
#define JBgetfield 180
#define JBputfield 181
#define JBinvokevirtual 182
#define JBinvokespecial 183
#define JBinvokestatic 184
#define JBinvokeinterface 185
#define JBinvokedynamic 186
#define JBnew 187
#define JBnewarray 188
#define JBanewarray 189
#define JBarraylength 190
#define JBathrow 191
#define JBcheckcast 192
#define JBinstanceof 193
#define JBmonitorenter 194
#define JBmonitorexit 195
#define JBmultianewarray 197
#define JBifnull 198
#define JBifnonnull 199
#define JBgotow 200
#define JBbreakpoint 202
#define JBiloadw 203
#define JBlloadw 204
#define JBfloadw 205
#define JBdloadw 206
#define JBaloadw 207
#define JBistorew 208
#define JBlstorew 209
#define JBfstorew 210
#define JBdstorew 211
#define JBastorew 212
#define JBiincw 213
#define JBaload0getfield 215
#define JBnewdup 216
#if defined(J9VM_OPT_VALHALLA_MVT)
#define JBvload 217
#define JBvstore 218
#define JBvreturn 219
#define JBvbox 220
#define JBvunbox 221
#define JBvaload 222
#define JBvastore 223
#define JBvdefault 224
#define JBvgetfield 225
#define JBvwithfield 226
#endif /* defined(J9VM_OPT_VALHALLA_MVT) */
#define JBreturnFromConstructor 228
#define JBgenericReturn 229
#define JBinvokeinterface2 231
#define JBinvokehandle 232
#define JBinvokehandlegeneric 233
#define JBinvokestaticsplit 234
#define JBinvokespecialsplit 235
#define JBretFromNative0 244
#define JBretFromNative1 245
#define JBretFromNativeF 246
#define JBretFromNativeD 247
#define JBretFromNativeJ 248
#define JBldc2dw 249
#define JBasyncCheck 250
#define JBreturnFromJ2I 251
#define JBimpdep1 254
#define JBimpdep2 255

#ifdef __cplusplus
}
#endif

#endif /* BCNAMES_H */
