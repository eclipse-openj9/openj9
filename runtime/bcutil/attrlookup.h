/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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

/* ANSI-C code produced by gperf version 3.1 */
/* Command-line: gperf -CD -t --output-file=attrlookup.h attrlookup.gperf  */
/* Computed positions: -k'2' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gperf@gnu.org>."
#endif

#line 39 "attrlookup.gperf"
struct AttribType
{
	const char *name;
	U_8 attribCode;
	U_8 strippedAttribCode;
};

#define TOTAL_KEYWORDS 31
#define MIN_WORD_LENGTH 4
#define MAX_WORD_LENGTH 36
#define MIN_HASH_VALUE 4
#define MAX_HASH_VALUE 50
/* maximum key range = 47, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static size_t
attributeHash (register const char *str, register size_t len)
{
  static const unsigned char asso_values[] =
    {
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 31, 51, 51, 51, 15, 51, 51, 51,  5,
       0,  0, 51, 51, 51, 51, 15,  0, 51, 51,
      25,  0, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51
    };
  return len + asso_values[(unsigned char)str[1]];
}

#ifdef __GNUC__
__inline
#if defined __GNUC_STDC_INLINE__ || defined __GNUC_GNU_INLINE__
__attribute__ ((__gnu_inline__))
#endif
#endif
const struct AttribType *
lookupKnownAttribute (register const char *str, register size_t len)
{
  static const struct AttribType wordlist[] =
    {
#line 47 "attrlookup.gperf"
      {"Code", CFR_ATTRIBUTE_Code, CFR_ATTRIBUTE_Code},
#line 49 "attrlookup.gperf"
      {"Synthetic", CFR_ATTRIBUTE_Synthetic, CFR_ATTRIBUTE_Synthetic},
#line 53 "attrlookup.gperf"
      {"SourceFile", CFR_ATTRIBUTE_SourceFile, CFR_ATTRIBUTE_SourceFile},
#line 54 "attrlookup.gperf"
      {"InnerClasses", CFR_ATTRIBUTE_InnerClasses, CFR_ATTRIBUTE_InnerClasses},
#line 55 "attrlookup.gperf"
      {"ConstantValue", CFR_ATTRIBUTE_ConstantValue, CFR_ATTRIBUTE_ConstantValue},
#line 77 "attrlookup.gperf"
      {"NullRestricted", CFR_ATTRIBUTE_NullRestricted, CFR_ATTRIBUTE_NullRestricted},
#line 57 "attrlookup.gperf"
      {"EnclosingMethod", CFR_ATTRIBUTE_EnclosingMethod, CFR_ATTRIBUTE_EnclosingMethod},
#line 59 "attrlookup.gperf"
      {"BootstrapMethods", CFR_ATTRIBUTE_BootstrapMethods, CFR_ATTRIBUTE_BootstrapMethods},
#line 60 "attrlookup.gperf"
      {"AnnotationDefault", CFR_ATTRIBUTE_AnnotationDefault, CFR_ATTRIBUTE_AnnotationDefault},
#line 61 "attrlookup.gperf"
      {"LocalVariableTable", CFR_ATTRIBUTE_LocalVariableTable, CFR_ATTRIBUTE_StrippedLocalVariableTable},
#line 76 "attrlookup.gperf"
      {"LoadableDescriptors", CFR_ATTRIBUTE_LoadableDescriptors, CFR_ATTRIBUTE_LoadableDescriptors},
#line 62 "attrlookup.gperf"
      {"SourceDebugExtension", CFR_ATTRIBUTE_SourceDebugExtension, CFR_ATTRIBUTE_StrippedSourceDebugExtension},
#line 75 "attrlookup.gperf"
      {"ImplicitCreation", CFR_ATTRIBUTE_ImplicitCreation, CFR_ATTRIBUTE_ImplicitCreation},
#line 63 "attrlookup.gperf"
      {"LocalVariableTypeTable", CFR_ATTRIBUTE_LocalVariableTypeTable, CFR_ATTRIBUTE_StrippedLocalVariableTypeTable},
#line 48 "attrlookup.gperf"
      {"StackMap", CFR_ATTRIBUTE_StackMap, CFR_ATTRIBUTE_StackMap},
#line 50 "attrlookup.gperf"
      {"Signature", CFR_ATTRIBUTE_Signature, CFR_ATTRIBUTE_Signature},
#line 64 "attrlookup.gperf"
      {"RuntimeVisibleAnnotations", CFR_ATTRIBUTE_RuntimeVisibleAnnotations, CFR_ATTRIBUTE_RuntimeVisibleAnnotations},
#line 65 "attrlookup.gperf"
      {"RuntimeInvisibleAnnotations", CFR_ATTRIBUTE_RuntimeInvisibleAnnotations, CFR_ATTRIBUTE_RuntimeInvisibleAnnotations},
#line 56 "attrlookup.gperf"
      {"StackMapTable", CFR_ATTRIBUTE_StackMapTable, CFR_ATTRIBUTE_StackMapTable},
#line 66 "attrlookup.gperf"
      {"RuntimeVisibleTypeAnnotations", CFR_ATTRIBUTE_RuntimeVisibleTypeAnnotations, CFR_ATTRIBUTE_RuntimeVisibleTypeAnnotations},
#line 58 "attrlookup.gperf"
      {"LineNumberTable", CFR_ATTRIBUTE_LineNumberTable, CFR_ATTRIBUTE_StrippedLineNumberTable},
#line 67 "attrlookup.gperf"
      {"RuntimeInvisibleTypeAnnotations", CFR_ATTRIBUTE_RuntimeInvisibleTypeAnnotations, CFR_ATTRIBUTE_RuntimeInvisibleTypeAnnotations},
#line 68 "attrlookup.gperf"
      {"RuntimeVisibleParameterAnnotations", CFR_ATTRIBUTE_RuntimeVisibleParameterAnnotations, CFR_ATTRIBUTE_RuntimeVisibleParameterAnnotations},
#line 52 "attrlookup.gperf"
      {"Exceptions", CFR_ATTRIBUTE_Exceptions, CFR_ATTRIBUTE_Exceptions},
#line 69 "attrlookup.gperf"
      {"RuntimeInvisibleParameterAnnotations", CFR_ATTRIBUTE_RuntimeInvisibleParameterAnnotations, CFR_ATTRIBUTE_RuntimeInvisibleParameterAnnotations},
#line 73 "attrlookup.gperf"
      {"Record", CFR_ATTRIBUTE_Record, CFR_ATTRIBUTE_Record},
#line 72 "attrlookup.gperf"
      {"NestHost", CFR_ATTRIBUTE_NestHost, CFR_ATTRIBUTE_NestHost},
#line 51 "attrlookup.gperf"
      {"Deprecated", CFR_ATTRIBUTE_Deprecated, CFR_ATTRIBUTE_Deprecated},
#line 71 "attrlookup.gperf"
      {"NestMembers", CFR_ATTRIBUTE_NestMembers, CFR_ATTRIBUTE_NestMembers},
#line 70 "attrlookup.gperf"
      {"MethodParameters", CFR_ATTRIBUTE_MethodParameters, CFR_ATTRIBUTE_MethodParameters},
#line 74 "attrlookup.gperf"
      {"PermittedSubclasses", CFR_ATTRIBUTE_PermittedSubclasses, CFR_ATTRIBUTE_PermittedSubclasses}
    };

  static const signed char lookup[] =
    {
      -1, -1, -1, -1,  0, -1, -1, -1, -1,  1,  2, -1,  3,  4,
       5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, -1, 17,
      18, 19, 20, 21, -1, -1, 22, 23, 24, 25, -1, 26, -1, 27,
      28, -1, -1, -1, -1, 29, -1, -1, 30
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register size_t key = attributeHash (str, len);

      if (key <= MAX_HASH_VALUE)
        {
          register int index = lookup[key];

          if (index >= 0)
            {
              register const char *s = wordlist[index].name;

              if (*str == *s && !strcmp (str + 1, s + 1))
                return &wordlist[index];
            }
        }
    }
  return 0;
}
