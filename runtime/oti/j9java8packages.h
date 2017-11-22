/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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

/* ANSI-C code produced by gperf version 3.0.4 */
/* Command-line: gperf -CD -t --output-file=j9java8packages.h j9java8packages.gperf  */
/* Computed positions: -k'1,5,7,9,12,16,19-21,25-28,31,36,42,46,$' */

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
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif

#line 65 "j9java8packages.gperf"
struct AttributeType
{
	const char *name;
};

#define TOTAL_KEYWORDS 1103
#define MIN_WORD_LENGTH 3
#define MAX_WORD_LENGTH 73
#define MIN_HASH_VALUE 56
#define MAX_HASH_VALUE 12822
/* maximum key range = 12767, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
packageNameHash (register const char *str, register unsigned int len)
{
  static const unsigned short asso_values[] =
    {
      12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823,
      12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823,
      12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823,
      12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823,
      12823, 12823, 12823, 12823, 12823, 12823, 12823,    10,  1190,     5,
        155,     0, 12823,     0, 12823,    20,    10,    85, 12823, 12823,
      12823, 12823, 12823, 12823, 12823,     0,     0,    10,     0, 12823,
          0, 12823, 12823,    10, 12823, 12823,     0,     0,     5,     0,
          0, 12823,    40,     0, 12823, 12823, 12823, 12823,     0, 12823,
      12823, 12823, 12823, 12823, 12823,    25, 12823,    20,  1439,    35,
       1185,     5,  1810,  1575,  1866,    10,   215,  1945,   105,    15,
         10,   885,   585,    35,     0,     0,    25,  1320,   368,  3338,
        230,  1835,    20, 12823, 12823, 12823, 12823, 12823, 12823, 12823,
      12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823,
      12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823,
      12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823,
      12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823,
      12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823,
      12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823,
      12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823,
      12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823,
      12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823,
      12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823,
      12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823,
      12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823, 12823,
      12823, 12823, 12823, 12823, 12823, 12823, 12823
    };
  register int hval = len;

  switch (hval)
    {
      default:
        hval += asso_values[(unsigned char)str[45]];
      /*FALLTHROUGH*/
      case 45:
      case 44:
      case 43:
      case 42:
        hval += asso_values[(unsigned char)str[41]];
      /*FALLTHROUGH*/
      case 41:
      case 40:
      case 39:
      case 38:
      case 37:
      case 36:
        hval += asso_values[(unsigned char)str[35]];
      /*FALLTHROUGH*/
      case 35:
      case 34:
      case 33:
      case 32:
      case 31:
        hval += asso_values[(unsigned char)str[30]];
      /*FALLTHROUGH*/
      case 30:
      case 29:
      case 28:
        hval += asso_values[(unsigned char)str[27]];
      /*FALLTHROUGH*/
      case 27:
        hval += asso_values[(unsigned char)str[26]];
      /*FALLTHROUGH*/
      case 26:
        hval += asso_values[(unsigned char)str[25]];
      /*FALLTHROUGH*/
      case 25:
        hval += asso_values[(unsigned char)str[24]];
      /*FALLTHROUGH*/
      case 24:
      case 23:
      case 22:
      case 21:
        hval += asso_values[(unsigned char)str[20]];
      /*FALLTHROUGH*/
      case 20:
        hval += asso_values[(unsigned char)str[19]];
      /*FALLTHROUGH*/
      case 19:
        hval += asso_values[(unsigned char)str[18]];
      /*FALLTHROUGH*/
      case 18:
      case 17:
      case 16:
        hval += asso_values[(unsigned char)str[15]];
      /*FALLTHROUGH*/
      case 15:
      case 14:
      case 13:
      case 12:
        hval += asso_values[(unsigned char)str[11]+1];
      /*FALLTHROUGH*/
      case 11:
      case 10:
      case 9:
        hval += asso_values[(unsigned char)str[8]];
      /*FALLTHROUGH*/
      case 8:
      case 7:
        hval += asso_values[(unsigned char)str[6]];
      /*FALLTHROUGH*/
      case 6:
      case 5:
        hval += asso_values[(unsigned char)str[4]];
      /*FALLTHROUGH*/
      case 4:
      case 3:
      case 2:
      case 1:
        hval += asso_values[(unsigned char)str[0]];
        break;
    }
  return hval + asso_values[(unsigned char)str[len - 1]];
}

#ifdef __GNUC__
__inline
#if defined __GNUC_STDC_INLINE__ || defined __GNUC_GNU_INLINE__
__attribute__ ((__gnu_inline__))
#endif
#endif
const struct AttributeType *
lookupJava8Package (register const char *str, register unsigned int len)
{
  static const struct AttributeType wordlist[] =
    {
#line 1045 "j9java8packages.gperf"
      {"sun/net/spi"},
#line 1036 "j9java8packages.gperf"
      {"sun/misc"},
#line 988 "j9java8packages.gperf"
      {"sun/awt/X11"},
#line 1042 "j9java8packages.gperf"
      {"sun/net/idn"},
#line 1037 "j9java8packages.gperf"
      {"sun/net"},
#line 987 "j9java8packages.gperf"
      {"sun/awt"},
#line 993 "j9java8packages.gperf"
      {"sun/awt/im"},
#line 176 "j9java8packages.gperf"
      {"com/ibm/rmi"},
#line 71 "j9java8packages.gperf"
      {"com/ibm"},
#line 999 "j9java8packages.gperf"
      {"sun/corba"},
#line 72 "j9java8packages.gperf"
      {"com/ibm/CORBA"},
#line 417 "j9java8packages.gperf"
      {"com/sun/source/tree"},
#line 224 "j9java8packages.gperf"
      {"com/sun/awt"},
#line 161 "j9java8packages.gperf"
      {"com/ibm/net"},
#line 1095 "j9java8packages.gperf"
      {"sun/security/pkcs"},
#line 411 "j9java8packages.gperf"
      {"com/sun/rowset"},
#line 1094 "j9java8packages.gperf"
      {"sun/security/jca"},
#line 79 "j9java8packages.gperf"
      {"com/ibm/CORBA/ras"},
#line 1091 "j9java8packages.gperf"
      {"sun/security/action"},
#line 451 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws"},
#line 996 "j9java8packages.gperf"
      {"sun/awt/shell"},
#line 1103 "j9java8packages.gperf"
      {"sun/security/util"},
#line 507 "j9java8packages.gperf"
      {"com/sun/tools/javac"},
#line 513 "j9java8packages.gperf"
      {"com/sun/tools/javac/main"},
#line 508 "j9java8packages.gperf"
      {"com/sun/tools/javac/api"},
#line 518 "j9java8packages.gperf"
      {"com/sun/tools/javac/tree"},
#line 509 "j9java8packages.gperf"
      {"com/sun/tools/javac/code"},
#line 412 "j9java8packages.gperf"
      {"com/sun/rowset/internal"},
#line 734 "j9java8packages.gperf"
      {"java/beans"},
#line 767 "j9java8packages.gperf"
      {"java/time"},
#line 754 "j9java8packages.gperf"
      {"java/rmi"},
#line 745 "j9java8packages.gperf"
      {"java/net"},
#line 758 "j9java8packages.gperf"
      {"java/rmi/server"},
#line 751 "j9java8packages.gperf"
      {"java/nio/file"},
#line 326 "j9java8packages.gperf"
      {"com/sun/jdi"},
#line 938 "j9java8packages.gperf"
      {"jdk/net"},
#line 1113 "j9java8packages.gperf"
      {"sun/text"},
#line 765 "j9java8packages.gperf"
      {"java/text"},
#line 1114 "j9java8packages.gperf"
      {"sun/text/bidi"},
#line 1115 "j9java8packages.gperf"
      {"sun/text/normalizer"},
#line 137 "j9java8packages.gperf"
      {"com/ibm/jvm"},
#line 270 "j9java8packages.gperf"
      {"com/sun/corba/se/internal/POA"},
#line 314 "j9java8packages.gperf"
      {"com/sun/jarsigner"},
#line 132 "j9java8packages.gperf"
      {"com/ibm/jit"},
#line 766 "j9java8packages.gperf"
      {"java/text/spi"},
#line 1093 "j9java8packages.gperf"
      {"sun/security/internal/spec"},
#line 1116 "j9java8packages.gperf"
      {"sun/text/resources"},
#line 1138 "j9java8packages.gperf"
      {"sun/text/resources/sr"},
#line 1124 "j9java8packages.gperf"
      {"sun/text/resources/es"},
#line 779 "j9java8packages.gperf"
      {"java/util/prefs"},
#line 764 "j9java8packages.gperf"
      {"java/sql"},
#line 223 "j9java8packages.gperf"
      {"com/sun/activation/registries"},
#line 1165 "j9java8packages.gperf"
      {"com/ibm/Compiler/Internal"},
#line 1123 "j9java8packages.gperf"
      {"sun/text/resources/en"},
#line 1119 "j9java8packages.gperf"
      {"sun/text/resources/cs"},
#line 514 "j9java8packages.gperf"
      {"com/sun/tools/javac/model"},
#line 512 "j9java8packages.gperf"
      {"com/sun/tools/javac/jvm"},
#line 1125 "j9java8packages.gperf"
      {"sun/text/resources/et"},
#line 781 "j9java8packages.gperf"
      {"java/util/spi"},
#line 1129 "j9java8packages.gperf"
      {"sun/text/resources/it"},
#line 1132 "j9java8packages.gperf"
      {"sun/text/resources/mt"},
#line 358 "j9java8packages.gperf"
      {"com/sun/naming/internal"},
#line 1117 "j9java8packages.gperf"
      {"sun/text/resources/ca"},
#line 478 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc"},
#line 485 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/api"},
#line 504 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/runtime"},
#line 445 "j9java8packages.gperf"
      {"com/sun/tools/internal/jxc"},
#line 447 "j9java8packages.gperf"
      {"com/sun/tools/internal/jxc/api"},
#line 772 "j9java8packages.gperf"
      {"java/util"},
#line 826 "j9java8packages.gperf"
      {"javax/rmi/CORBA"},
#line 825 "j9java8packages.gperf"
      {"javax/rmi"},
#line 448 "j9java8packages.gperf"
      {"com/sun/tools/internal/jxc/api/impl/j2s"},
#line 339 "j9java8packages.gperf"
      {"com/sun/jndi/dns"},
#line 1118 "j9java8packages.gperf"
      {"sun/text/resources/cldr"},
#line 1130 "j9java8packages.gperf"
      {"sun/text/resources/lt"},
#line 819 "j9java8packages.gperf"
      {"javax/net"},
#line 491 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/model"},
#line 1137 "j9java8packages.gperf"
      {"sun/text/resources/sl"},
#line 1122 "j9java8packages.gperf"
      {"sun/text/resources/el"},
#line 1133 "j9java8packages.gperf"
      {"sun/text/resources/nl"},
#line 827 "j9java8packages.gperf"
      {"javax/rmi/ssl"},
#line 350 "j9java8packages.gperf"
      {"com/sun/jndi/url/dns"},
#line 735 "j9java8packages.gperf"
      {"java/beans/beancontext"},
#line 820 "j9java8packages.gperf"
      {"javax/net/ssl"},
#line 1043 "j9java8packages.gperf"
      {"sun/net/sdp"},
#line 355 "j9java8packages.gperf"
      {"com/sun/jndi/url/rmi"},
#line 343 "j9java8packages.gperf"
      {"com/sun/jndi/ldap/sasl"},
#line 985 "j9java8packages.gperf"
      {"sun/applet"},
#line 995 "j9java8packages.gperf"
      {"sun/awt/resources"},
#line 1068 "j9java8packages.gperf"
      {"sun/print"},
#line 841 "j9java8packages.gperf"
      {"javax/sql"},
#line 1044 "j9java8packages.gperf"
      {"sun/net/smtp"},
#line 799 "j9java8packages.gperf"
      {"javax/jws"},
#line 486 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/api/impl/s2j"},
#line 174 "j9java8packages.gperf"
      {"com/ibm/pkcs11"},
#line 76 "j9java8packages.gperf"
      {"com/ibm/CORBA/iiop"},
#line 175 "j9java8packages.gperf"
      {"com/ibm/pkcs11/nat"},
#line 1098 "j9java8packages.gperf"
      {"sun/security/timestamp"},
#line 285 "j9java8packages.gperf"
      {"com/sun/corba/se/spi/ior"},
#line 910 "j9java8packages.gperf"
      {"jdk/internal/util/xml"},
#line 465 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/spi"},
#line 454 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/processor"},
#line 776 "j9java8packages.gperf"
      {"java/util/function"},
#line 515 "j9java8packages.gperf"
      {"com/sun/tools/javac/parser"},
#line 291 "j9java8packages.gperf"
      {"com/sun/corba/se/spi/oa"},
#line 1012 "j9java8packages.gperf"
      {"sun/java2d/loops"},
#line 452 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/api"},
#line 460 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/processor/modeler"},
#line 798 "j9java8packages.gperf"
      {"javax/imageio/stream"},
#line 284 "j9java8packages.gperf"
      {"com/sun/corba/se/spi/extension"},
#line 1014 "j9java8packages.gperf"
      {"sun/java2d/pipe"},
#line 510 "j9java8packages.gperf"
      {"com/sun/tools/javac/comp"},
#line 1016 "j9java8packages.gperf"
      {"sun/java2d/pisces"},
#line 801 "j9java8packages.gperf"
      {"javax/lang/model"},
#line 835 "j9java8packages.gperf"
      {"javax/security/cert"},
#line 455 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/processor/generator"},
#line 341 "j9java8packages.gperf"
      {"com/sun/jndi/ldap/ext"},
#line 461 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/processor/modeler/annotation"},
#line 892 "j9java8packages.gperf"
      {"javax/xml/ws"},
#line 458 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/processor/model/java"},
#line 297 "j9java8packages.gperf"
      {"com/sun/corba/se/spi/presentation/rmi"},
#line 893 "j9java8packages.gperf"
      {"javax/xml/ws/handler"},
#line 897 "j9java8packages.gperf"
      {"javax/xml/ws/spi"},
#line 771 "j9java8packages.gperf"
      {"java/time/zone"},
#line 802 "j9java8packages.gperf"
      {"javax/lang/model/element"},
#line 242 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/ior"},
#line 252 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/oa"},
#line 753 "j9java8packages.gperf"
      {"java/nio/file/spi"},
#line 463 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/processor/util"},
#line 456 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/processor/model"},
#line 769 "j9java8packages.gperf"
      {"java/time/format"},
#line 462 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/processor/modeler/wsdl"},
#line 866 "j9java8packages.gperf"
      {"javax/xml"},
#line 272 "j9java8packages.gperf"
      {"com/sun/corba/se/internal/iiop"},
#line 262 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/presentation/rmi"},
#line 836 "j9java8packages.gperf"
      {"javax/security/sasl"},
#line 1151 "j9java8packages.gperf"
      {"sun/tools/tree"},
#line 1065 "j9java8packages.gperf"
      {"sun/nio/cs"},
#line 804 "j9java8packages.gperf"
      {"javax/lang/model/util"},
#line 240 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/interceptors"},
#line 719 "j9java8packages.gperf"
      {"java/applet"},
#line 1099 "j9java8packages.gperf"
      {"sun/security/tools"},
#line 1088 "j9java8packages.gperf"
      {"sun/rmi/transport"},
#line 1140 "j9java8packages.gperf"
      {"sun/tools/asm"},
#line 167 "j9java8packages.gperf"
      {"com/ibm/nio"},
#line 940 "j9java8packages.gperf"
      {"org/ietf/jgss"},
#line 991 "j9java8packages.gperf"
      {"sun/awt/event"},
#line 1149 "j9java8packages.gperf"
      {"sun/tools/jstat"},
#line 1135 "j9java8packages.gperf"
      {"sun/text/resources/pt"},
#line 77 "j9java8packages.gperf"
      {"com/ibm/CORBA/nio"},
#line 78 "j9java8packages.gperf"
      {"com/ibm/CORBA/poa"},
#line 773 "j9java8packages.gperf"
      {"java/util/concurrent"},
#line 430 "j9java8packages.gperf"
      {"com/sun/tools/doclets"},
#line 83 "j9java8packages.gperf"
      {"com/ibm/amino/refq"},
#line 446 "j9java8packages.gperf"
      {"com/sun/tools/internal/jxc/ap"},
#line 441 "j9java8packages.gperf"
      {"com/sun/tools/doclint"},
#line 267 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/util"},
#line 821 "j9java8packages.gperf"
      {"javax/print"},
#line 531 "j9java8packages.gperf"
      {"com/sun/tools/script/shell"},
#line 1139 "j9java8packages.gperf"
      {"sun/text/resources/sv"},
#line 340 "j9java8packages.gperf"
      {"com/sun/jndi/ldap"},
#line 774 "j9java8packages.gperf"
      {"java/util/concurrent/atomic"},
#line 793 "j9java8packages.gperf"
      {"javax/imageio/event"},
#line 746 "j9java8packages.gperf"
      {"java/nio"},
#line 526 "j9java8packages.gperf"
      {"com/sun/tools/jconsole"},
#line 1134 "j9java8packages.gperf"
      {"sun/text/resources/pl"},
#line 783 "j9java8packages.gperf"
      {"java/util/zip"},
#line 492 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/model/nav"},
#line 450 "j9java8packages.gperf"
      {"com/sun/tools/internal/jxc/model/nav"},
#line 1131 "j9java8packages.gperf"
      {"sun/text/resources/lv"},
#line 278 "j9java8packages.gperf"
      {"com/sun/corba/se/spi/activation"},
#line 842 "j9java8packages.gperf"
      {"javax/sql/rowset"},
#line 1002 "j9java8packages.gperf"
      {"sun/dc/pr"},
#line 300 "j9java8packages.gperf"
      {"com/sun/corba/se/spi/servicecontext"},
#line 269 "j9java8packages.gperf"
      {"com/sun/corba/se/internal/Interceptors"},
#line 1000 "j9java8packages.gperf"
      {"sun/dc"},
#line 1038 "j9java8packages.gperf"
      {"sun/net/dns"},
#line 1082 "j9java8packages.gperf"
      {"sun/rmi/rmic"},
#line 279 "j9java8packages.gperf"
      {"com/sun/corba/se/spi/activation/InitialNameServicePackage"},
#line 188 "j9java8packages.gperf"
      {"com/ibm/rmi/ras"},
#line 271 "j9java8packages.gperf"
      {"com/sun/corba/se/internal/corba"},
#line 280 "j9java8packages.gperf"
      {"com/sun/corba/se/spi/activation/LocatorPackage"},
#line 363 "j9java8packages.gperf"
      {"com/sun/nio/file"},
#line 186 "j9java8packages.gperf"
      {"com/ibm/rmi/pi"},
#line 187 "j9java8packages.gperf"
      {"com/ibm/rmi/poa"},
#line 416 "j9java8packages.gperf"
      {"com/sun/source/doctree"},
#line 527 "j9java8packages.gperf"
      {"com/sun/tools/jdeps"},
#line 524 "j9java8packages.gperf"
      {"com/sun/tools/javap"},
#line 529 "j9java8packages.gperf"
      {"com/sun/tools/jdi"},
#line 797 "j9java8packages.gperf"
      {"javax/imageio/spi"},
#line 843 "j9java8packages.gperf"
      {"javax/sql/rowset/serial"},
#line 160 "j9java8packages.gperf"
      {"com/ibm/misc"},
#line 184 "j9java8packages.gperf"
      {"com/ibm/rmi/javax/rmi"},
#line 530 "j9java8packages.gperf"
      {"com/sun/tools/jdi/resources"},
#line 482 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/addon/episode"},
#line 1104 "j9java8packages.gperf"
      {"sun/security/x509"},
#line 1086 "j9java8packages.gperf"
      {"sun/rmi/runtime"},
#line 1155 "j9java8packages.gperf"
      {"sun/util/cldr"},
#line 185 "j9java8packages.gperf"
      {"com/ibm/rmi/javax/rmi/CORBA"},
#line 483 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/addon/locator"},
#line 863 "j9java8packages.gperf"
      {"javax/tools"},
#line 479 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/addon/accessors"},
#line 484 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/addon/sync"},
#line 1154 "j9java8packages.gperf"
      {"sun/util/calendar"},
#line 493 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/outline"},
#line 1160 "j9java8packages.gperf"
      {"sun/util/resources"},
#line 896 "j9java8packages.gperf"
      {"javax/xml/ws/soap"},
#line 347 "j9java8packages.gperf"
      {"com/sun/jndi/toolkit/dir"},
#line 277 "j9java8packages.gperf"
      {"com/sun/corba/se/pept/transport"},
#line 895 "j9java8packages.gperf"
      {"javax/xml/ws/http"},
#line 192 "j9java8packages.gperf"
      {"com/ibm/rmi/util/list"},
#line 1153 "j9java8packages.gperf"
      {"sun/util"},
#line 911 "j9java8packages.gperf"
      {"jdk/internal/util/xml/impl"},
#line 982 "j9java8packages.gperf"
      {"org/xml/sax"},
#line 243 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/ior/iiop"},
#line 1028 "j9java8packages.gperf"
      {"sun/launcher"},
#line 1019 "j9java8packages.gperf"
      {"sun/jvmstat/monitor"},
#line 190 "j9java8packages.gperf"
      {"com/ibm/rmi/util"},
#line 756 "j9java8packages.gperf"
      {"java/rmi/dgc"},
#line 1048 "j9java8packages.gperf"
      {"sun/net/util"},
#line 177 "j9java8packages.gperf"
      {"com/ibm/rmi/channel"},
#line 464 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/resources"},
#line 997 "j9java8packages.gperf"
      {"sun/awt/util"},
#line 245 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/javax/rmi/CORBA"},
#line 329 "j9java8packages.gperf"
      {"com/sun/jdi/event"},
#line 244 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/javax/rmi"},
#line 331 "j9java8packages.gperf"
      {"com/sun/jmx/defaults"},
#line 216 "j9java8packages.gperf"
      {"com/oracle/webservices/internal/api"},
#line 266 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/transport"},
#line 1029 "j9java8packages.gperf"
      {"sun/launcher/resources"},
#line 1021 "j9java8packages.gperf"
      {"sun/jvmstat/monitor/remote"},
#line 328 "j9java8packages.gperf"
      {"com/sun/jdi/connect/spi"},
#line 327 "j9java8packages.gperf"
      {"com/sun/jdi/connect"},
#line 361 "j9java8packages.gperf"
      {"com/sun/net/ssl/internal/ssl"},
#line 348 "j9java8packages.gperf"
      {"com/sun/jndi/toolkit/url"},
#line 517 "j9java8packages.gperf"
      {"com/sun/tools/javac/resources"},
#line 1020 "j9java8packages.gperf"
      {"sun/jvmstat/monitor/event"},
#line 356 "j9java8packages.gperf"
      {"com/sun/management"},
#line 1121 "j9java8packages.gperf"
      {"sun/text/resources/de"},
#line 1064 "j9java8packages.gperf"
      {"sun/nio/ch/sctp"},
#line 85 "j9java8packages.gperf"
      {"com/ibm/cuda"},
#line 1161 "j9java8packages.gperf"
      {"sun/util/resources/cldr"},
#line 1162 "j9java8packages.gperf"
      {"sun/util/spi"},
#line 1120 "j9java8packages.gperf"
      {"sun/text/resources/da"},
#line 82 "j9java8packages.gperf"
      {"com/ibm/amino/map"},
#line 792 "j9java8packages.gperf"
      {"javax/imageio"},
#line 418 "j9java8packages.gperf"
      {"com/sun/source/util"},
#line 80 "j9java8packages.gperf"
      {"com/ibm/CORBA/transport"},
#line 519 "j9java8packages.gperf"
      {"com/sun/tools/javac/util"},
#line 494 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/reader"},
#line 466 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/util"},
#line 614 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws"},
#line 467 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/util/xml"},
#line 755 "j9java8packages.gperf"
      {"java/rmi/activation"},
#line 1046 "j9java8packages.gperf"
      {"sun/net/spi/nameservice"},
#line 310 "j9java8packages.gperf"
      {"com/sun/istack/internal"},
#line 992 "j9java8packages.gperf"
      {"sun/awt/geom"},
#line 346 "j9java8packages.gperf"
      {"com/sun/jndi/toolkit/ctx"},
#line 710 "j9java8packages.gperf"
      {"com/sun/xml/internal/xsom"},
#line 500 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/reader/xmlschema"},
#line 666 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/message"},
#line 503 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/reader/xmlschema/parser"},
#line 861 "j9java8packages.gperf"
      {"javax/swing/tree"},
#line 81 "j9java8packages.gperf"
      {"com/ibm/CosNaming"},
#line 669 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/message/source"},
#line 162 "j9java8packages.gperf"
      {"com/ibm/net/rdma/jverbs"},
#line 457 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/processor/model/exporter"},
#line 606 "j9java8packages.gperf"
      {"com/sun/xml/internal/stream"},
#line 670 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/message/stream"},
#line 610 "j9java8packages.gperf"
      {"com/sun/xml/internal/stream/events"},
#line 502 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/reader/xmlschema/ct"},
#line 848 "j9java8packages.gperf"
      {"javax/swing/event"},
#line 1080 "j9java8packages.gperf"
      {"sun/rmi/log"},
#line 712 "j9java8packages.gperf"
      {"com/sun/xml/internal/xsom/impl/parser"},
#line 857 "j9java8packages.gperf"
      {"javax/swing/text"},
#line 782 "j9java8packages.gperf"
      {"java/util/stream"},
#line 498 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/reader/internalizer"},
#line 241 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/io"},
#line 713 "j9java8packages.gperf"
      {"com/sun/xml/internal/xsom/impl/parser/state"},
#line 163 "j9java8packages.gperf"
      {"com/ibm/net/rdma/jverbs/cm"},
#line 325 "j9java8packages.gperf"
      {"com/sun/javadoc"},
#line 749 "j9java8packages.gperf"
      {"java/nio/charset"},
#line 643 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/assembler"},
#line 856 "j9java8packages.gperf"
      {"javax/swing/table"},
#line 844 "j9java8packages.gperf"
      {"javax/sql/rowset/spi"},
#line 1062 "j9java8packages.gperf"
      {"sun/nio"},
#line 711 "j9java8packages.gperf"
      {"com/sun/xml/internal/xsom/impl"},
#line 645 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/assembler/jaxws"},
#line 273 "j9java8packages.gperf"
      {"com/sun/corba/se/org/omg/CORBA"},
#line 828 "j9java8packages.gperf"
      {"javax/script"},
#line 715 "j9java8packages.gperf"
      {"com/sun/xml/internal/xsom/impl/util"},
#line 747 "j9java8packages.gperf"
      {"java/nio/channels"},
#line 777 "j9java8packages.gperf"
      {"java/util/jar"},
#line 254 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/oa/toa"},
#line 649 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/client/sei"},
#line 647 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/client"},
#line 1168 "j9java8packages.gperf"
      {"com/ibm/cuda/internal"},
#line 800 "j9java8packages.gperf"
      {"javax/jws/soap"},
#line 859 "j9java8packages.gperf"
      {"javax/swing/text/html/parser"},
#line 1070 "j9java8packages.gperf"
      {"sun/reflect"},
#line 1003 "j9java8packages.gperf"
      {"sun/font"},
#line 1004 "j9java8packages.gperf"
      {"sun/instrument"},
#line 333 "j9java8packages.gperf"
      {"com/sun/jmx/mbeanserver"},
#line 612 "j9java8packages.gperf"
      {"com/sun/xml/internal/txw2/annotation"},
#line 1007 "j9java8packages.gperf"
      {"sun/java2d"},
#line 994 "j9java8packages.gperf"
      {"sun/awt/image"},
#line 611 "j9java8packages.gperf"
      {"com/sun/xml/internal/txw2"},
#line 858 "j9java8packages.gperf"
      {"javax/swing/text/html"},
#line 505 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/util"},
#line 787 "j9java8packages.gperf"
      {"javax/annotation"},
#line 1008 "j9java8packages.gperf"
      {"sun/java2d/cmm"},
#line 986 "j9java8packages.gperf"
      {"sun/applet/resources"},
#line 159 "j9java8packages.gperf"
      {"com/ibm/math"},
#line 882 "j9java8packages.gperf"
      {"javax/xml/soap"},
#line 785 "j9java8packages.gperf"
      {"javax/activation"},
#line 794 "j9java8packages.gperf"
      {"javax/imageio/metadata"},
#line 308 "j9java8packages.gperf"
      {"com/sun/imageio/spi"},
#line 511 "j9java8packages.gperf"
      {"com/sun/tools/javac/file"},
#line 868 "j9java8packages.gperf"
      {"javax/xml/bind/annotation"},
#line 309 "j9java8packages.gperf"
      {"com/sun/imageio/stream"},
#line 1164 "j9java8packages.gperf"
      {"com/ibm/java/lang/management/internal"},
#line 736 "j9java8packages.gperf"
      {"java/io"},
#line 688 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/server"},
#line 795 "j9java8packages.gperf"
      {"javax/imageio/plugins/bmp"},
#line 488 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/generator/bean"},
#line 894 "j9java8packages.gperf"
      {"javax/xml/ws/handler/soap"},
#line 690 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/server/sei"},
#line 768 "j9java8packages.gperf"
      {"java/time/chrono"},
#line 689 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/server/provider"},
#line 424 "j9java8packages.gperf"
      {"com/sun/tools/classfile"},
#line 845 "j9java8packages.gperf"
      {"javax/swing"},
#line 1010 "j9java8packages.gperf"
      {"sun/java2d/cmm/lcms"},
#line 423 "j9java8packages.gperf"
      {"com/sun/tools/attach/spi"},
#line 898 "j9java8packages.gperf"
      {"javax/xml/ws/spi/http"},
#line 718 "j9java8packages.gperf"
      {"com/sun/xml/internal/xsom/visitor"},
#line 525 "j9java8packages.gperf"
      {"com/sun/tools/javap/resources"},
#line 1092 "j9java8packages.gperf"
      {"sun/security/internal/interfaces"},
#line 332 "j9java8packages.gperf"
      {"com/sun/jmx/interceptor"},
#line 352 "j9java8packages.gperf"
      {"com/sun/jndi/url/iiopname"},
#line 668 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/message/saaj"},
#line 338 "j9java8packages.gperf"
      {"com/sun/jndi/cosnaming"},
#line 490 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/generator/util"},
#line 156 "j9java8packages.gperf"
      {"com/ibm/lang/management"},
#line 299 "j9java8packages.gperf"
      {"com/sun/corba/se/spi/resolver"},
#line 644 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/assembler/dev"},
#line 883 "j9java8packages.gperf"
      {"javax/xml/stream"},
#line 183 "j9java8packages.gperf"
      {"com/ibm/rmi/io"},
#line 1096 "j9java8packages.gperf"
      {"sun/security/provider"},
#line 901 "j9java8packages.gperf"
      {"jdk"},
#line 1170 "j9java8packages.gperf"
      {"com/ibm/oti/vm"},
#line 1127 "j9java8packages.gperf"
      {"sun/text/resources/fr"},
#line 1090 "j9java8packages.gperf"
      {"sun/rmi/transport/tcp"},
#line 770 "j9java8packages.gperf"
      {"java/time/temporal"},
#line 235 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/activation"},
#line 1126 "j9java8packages.gperf"
      {"sun/text/resources/fi"},
#line 292 "j9java8packages.gperf"
      {"com/sun/corba/se/spi/orb"},
#line 405 "j9java8packages.gperf"
      {"com/sun/org/omg/CORBA"},
#line 73 "j9java8packages.gperf"
      {"com/ibm/CORBA/channel"},
#line 867 "j9java8packages.gperf"
      {"javax/xml/bind"},
#line 231 "j9java8packages.gperf"
      {"com/sun/codemodel/internal"},
#line 169 "j9java8packages.gperf"
      {"com/ibm/org/omg/CORBA"},
#line 265 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/resolver"},
#line 189 "j9java8packages.gperf"
      {"com/ibm/rmi/transport"},
#line 908 "j9java8packages.gperf"
      {"jdk/internal/org/xml/sax"},
#line 359 "j9java8packages.gperf"
      {"com/sun/net/httpserver"},
#line 294 "j9java8packages.gperf"
      {"com/sun/corba/se/spi/orbutil/fsm"},
#line 881 "j9java8packages.gperf"
      {"javax/xml/parsers"},
#line 691 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/spi"},
#line 557 "j9java8packages.gperf"
      {"com/sun/xml/internal/dtdparser"},
#line 413 "j9java8packages.gperf"
      {"com/sun/rowset/providers"},
#line 286 "j9java8packages.gperf"
      {"com/sun/corba/se/spi/ior/iiop"},
#line 619 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/api"},
#line 301 "j9java8packages.gperf"
      {"com/sun/corba/se/spi/transport"},
#line 639 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/api/server"},
#line 520 "j9java8packages.gperf"
      {"com/sun/tools/javadoc"},
#line 628 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/api/message"},
#line 716 "j9java8packages.gperf"
      {"com/sun/xml/internal/xsom/parser"},
#line 1166 "j9java8packages.gperf"
      {"com/ibm/lang/management/internal"},
#line 627 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/api/handler"},
#line 626 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/api/ha"},
#line 824 "j9java8packages.gperf"
      {"javax/print/event"},
#line 621 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/api/client"},
#line 1173 "j9java8packages.gperf"
      {"com/ibm/oti/reflect"},
#line 630 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/api/message/stream"},
#line 584 "j9java8packages.gperf"
      {"com/sun/xml/internal/org/jvnet/fastinfoset"},
#line 1041 "j9java8packages.gperf"
      {"sun/net/httpserver"},
#line 205 "j9java8packages.gperf"
      {"com/ibm/security/smime"},
#line 237 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/corba"},
#line 750 "j9java8packages.gperf"
      {"java/nio/charset/spi"},
#line 293 "j9java8packages.gperf"
      {"com/sun/corba/se/spi/orbutil/closure"},
#line 1005 "j9java8packages.gperf"
      {"sun/invoke"},
#line 194 "j9java8packages.gperf"
      {"com/ibm/security/cert"},
#line 622 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/api/config/management"},
#line 805 "j9java8packages.gperf"
      {"javax/management"},
#line 1152 "j9java8packages.gperf"
      {"sun/tools/util"},
#line 195 "j9java8packages.gperf"
      {"com/ibm/security/ec"},
#line 516 "j9java8packages.gperf"
      {"com/sun/tools/javac/processing"},
#line 813 "j9java8packages.gperf"
      {"javax/management/timer"},
#line 1172 "j9java8packages.gperf"
      {"com/ibm/oti/util"},
#line 1142 "j9java8packages.gperf"
      {"sun/tools/jar"},
#line 152 "j9java8packages.gperf"
      {"com/ibm/jvm/io"},
#line 426 "j9java8packages.gperf"
      {"com/sun/tools/corba/se/idl/constExpr"},
#line 818 "j9java8packages.gperf"
      {"javax/naming/spi"},
#line 253 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/oa/poa"},
#line 885 "j9java8packages.gperf"
      {"javax/xml/stream/util"},
#line 225 "j9java8packages.gperf"
      {"com/sun/beans"},
#line 1144 "j9java8packages.gperf"
      {"sun/tools/java"},
#line 816 "j9java8packages.gperf"
      {"javax/naming/event"},
#line 428 "j9java8packages.gperf"
      {"com/sun/tools/corba/se/idl/som/idlemit"},
#line 429 "j9java8packages.gperf"
      {"com/sun/tools/corba/se/idl/toJavaPortable"},
#line 354 "j9java8packages.gperf"
      {"com/sun/jndi/url/ldaps"},
#line 229 "j9java8packages.gperf"
      {"com/sun/beans/infos"},
#line 1145 "j9java8packages.gperf"
      {"sun/tools/javac"},
#line 228 "j9java8packages.gperf"
      {"com/sun/beans/finder"},
#line 780 "j9java8packages.gperf"
      {"java/util/regex"},
#line 748 "j9java8packages.gperf"
      {"java/nio/channels/spi"},
#line 157 "j9java8packages.gperf"
      {"openj9/lang/management"},
#line 425 "j9java8packages.gperf"
      {"com/sun/tools/corba/se/idl"},
#line 937 "j9java8packages.gperf"
      {"jdk/nashorn/tools"},
#line 990 "j9java8packages.gperf"
      {"sun/awt/dnd"},
#line 638 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/api/policy/subject"},
#line 415 "j9java8packages.gperf"
      {"com/sun/security/ntlm"},
#line 364 "j9java8packages.gperf"
      {"com/sun/nio/sctp"},
#line 182 "j9java8packages.gperf"
      {"com/ibm/rmi/iiop"},
#line 1039 "j9java8packages.gperf"
      {"sun/net/ftp"},
#line 1069 "j9java8packages.gperf"
      {"sun/print/resources"},
#line 983 "j9java8packages.gperf"
      {"org/xml/sax/ext"},
#line 916 "j9java8packages.gperf"
      {"jdk/nashorn/internal/ir"},
#line 923 "j9java8packages.gperf"
      {"jdk/nashorn/internal/parser"},
#line 810 "j9java8packages.gperf"
      {"javax/management/relation"},
#line 589 "j9java8packages.gperf"
      {"com/sun/xml/internal/org/jvnet/staxex"},
#line 629 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/api/message/saaj"},
#line 230 "j9java8packages.gperf"
      {"com/sun/beans/util"},
#line 884 "j9java8packages.gperf"
      {"javax/xml/stream/events"},
#line 481 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/addon/code_injector"},
#line 924 "j9java8packages.gperf"
      {"jdk/nashorn/internal/runtime"},
#line 207 "j9java8packages.gperf"
      {"com/ibm/security/util"},
#line 925 "j9java8packages.gperf"
      {"jdk/nashorn/internal/runtime/arrays"},
#line 921 "j9java8packages.gperf"
      {"jdk/nashorn/internal/objects"},
#line 586 "j9java8packages.gperf"
      {"com/sun/xml/internal/org/jvnet/fastinfoset/sax/helpers"},
#line 927 "j9java8packages.gperf"
      {"jdk/nashorn/internal/runtime/linker"},
#line 778 "j9java8packages.gperf"
      {"java/util/logging"},
#line 872 "j9java8packages.gperf"
      {"javax/xml/bind/util"},
#line 1156 "j9java8packages.gperf"
      {"sun/util/locale"},
#line 501 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/reader/xmlschema/bindinfo"},
#line 587 "j9java8packages.gperf"
      {"com/sun/xml/internal/org/jvnet/fastinfoset/stax"},
#line 786 "j9java8packages.gperf"
      {"javax/activity"},
#line 913 "j9java8packages.gperf"
      {"jdk/nashorn/internal"},
#line 931 "j9java8packages.gperf"
      {"jdk/nashorn/internal/runtime/regexp/joni"},
#line 686 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/resources"},
#line 480 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/addon/at_generated"},
#line 873 "j9java8packages.gperf"
      {"javax/xml/crypto"},
#line 922 "j9java8packages.gperf"
      {"jdk/nashorn/internal/objects/annotations"},
#line 1018 "j9java8packages.gperf"
      {"sun/java2d/xr"},
#line 598 "j9java8packages.gperf"
      {"com/sun/xml/internal/rngom/nc"},
#line 1017 "j9java8packages.gperf"
      {"sun/java2d/x11"},
#line 591 "j9java8packages.gperf"
      {"com/sun/xml/internal/rngom/ast/om"},
#line 932 "j9java8packages.gperf"
      {"jdk/nashorn/internal/runtime/regexp/joni/ast"},
#line 874 "j9java8packages.gperf"
      {"javax/xml/crypto/dom"},
#line 694 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/transport"},
#line 697 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/transport/http/server"},
#line 659 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/encoding/fastinfoset"},
#line 933 "j9java8packages.gperf"
      {"jdk/nashorn/internal/runtime/regexp/joni/constants"},
#line 833 "j9java8packages.gperf"
      {"javax/security/auth/spi"},
#line 334 "j9java8packages.gperf"
      {"com/sun/jmx/remote/internal"},
#line 886 "j9java8packages.gperf"
      {"javax/xml/transform"},
#line 311 "j9java8packages.gperf"
      {"com/sun/istack/internal/localization"},
#line 164 "j9java8packages.gperf"
      {"com/ibm/net/rdma/jverbs/common"},
#line 696 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/transport/http/client"},
#line 1167 "j9java8packages.gperf"
      {"openj9/lang/management/internal"},
#line 135 "j9java8packages.gperf"
      {"com/ibm/jtc/orb/map"},
#line 351 "j9java8packages.gperf"
      {"com/sun/jndi/url/iiop"},
#line 528 "j9java8packages.gperf"
      {"com/sun/tools/jdeps/resources"},
#line 1083 "j9java8packages.gperf"
      {"sun/rmi/rmic/iiop"},
#line 592 "j9java8packages.gperf"
      {"com/sun/xml/internal/rngom/ast/util"},
#line 298 "j9java8packages.gperf"
      {"com/sun/corba/se/spi/protocol"},
#line 890 "j9java8packages.gperf"
      {"javax/xml/transform/stream"},
#line 287 "j9java8packages.gperf"
      {"com/sun/corba/se/spi/legacy/connection"},
#line 900 "j9java8packages.gperf"
      {"javax/xml/xpath"},
#line 1011 "j9java8packages.gperf"
      {"sun/java2d/jules"},
#line 276 "j9java8packages.gperf"
      {"com/sun/corba/se/pept/protocol"},
#line 989 "j9java8packages.gperf"
      {"sun/awt/datatransfer"},
#line 86 "j9java8packages.gperf"
      {"com/ibm/dataaccess"},
#line 209 "j9java8packages.gperf"
      {"com/ibm/security/util/text"},
#line 1067 "j9java8packages.gperf"
      {"sun/nio/fs"},
#line 180 "j9java8packages.gperf"
      {"com/ibm/rmi/corba"},
#line 740 "j9java8packages.gperf"
      {"java/lang/invoke"},
#line 832 "j9java8packages.gperf"
      {"javax/security/auth/login"},
#line 369 "j9java8packages.gperf"
      {"com/sun/org/apache/xerces/internal/jaxp/validation"},
#line 365 "j9java8packages.gperf"
      {"com/sun/org/apache/xalan/internal/xsltc/trax"},
#line 238 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/dynamicany"},
#line 585 "j9java8packages.gperf"
      {"com/sun/xml/internal/org/jvnet/fastinfoset/sax"},
#line 1066 "j9java8packages.gperf"
      {"sun/nio/cs/ext"},
#line 263 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/protocol"},
#line 1106 "j9java8packages.gperf"
      {"sun/swing/icon"},
#line 663 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/encoding/xml"},
#line 268 "j9java8packages.gperf"
      {"com/sun/corba/se/internal/CosNaming"},
#line 1100 "j9java8packages.gperf"
      {"sun/security/tools/jarsigner"},
#line 1063 "j9java8packages.gperf"
      {"sun/nio/ch"},
#line 1006 "j9java8packages.gperf"
      {"sun/invoke/util"},
#line 495 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/reader/dtd"},
#line 1047 "j9java8packages.gperf"
      {"sun/net/spi/nameservice/dns"},
#line 880 "j9java8packages.gperf"
      {"javax/xml/namespace"},
#line 360 "j9java8packages.gperf"
      {"com/sun/net/httpserver/spi"},
#line 330 "j9java8packages.gperf"
      {"com/sun/jdi/request"},
#line 864 "j9java8packages.gperf"
      {"javax/transaction"},
#line 1150 "j9java8packages.gperf"
      {"sun/tools/serialver"},
#line 846 "j9java8packages.gperf"
      {"javax/swing/border"},
#line 635 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/api/pipe"},
#line 919 "j9java8packages.gperf"
      {"jdk/nashorn/internal/ir/visitor"},
#line 888 "j9java8packages.gperf"
      {"javax/xml/transform/sax"},
#line 889 "j9java8packages.gperf"
      {"javax/xml/transform/stax"},
#line 714 "j9java8packages.gperf"
      {"com/sun/xml/internal/xsom/impl/scd"},
#line 605 "j9java8packages.gperf"
      {"com/sun/xml/internal/rngom/xml/util"},
#line 926 "j9java8packages.gperf"
      {"jdk/nashorn/internal/runtime/events"},
#line 345 "j9java8packages.gperf"
      {"com/sun/jndi/toolkit/corba"},
#line 533 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind/annotation"},
#line 761 "j9java8packages.gperf"
      {"java/security/cert"},
#line 588 "j9java8packages.gperf"
      {"com/sun/xml/internal/org/jvnet/mimepull"},
#line 1169 "j9java8packages.gperf"
      {"com/ibm/jit/crypto"},
#line 349 "j9java8packages.gperf"
      {"com/sun/jndi/url/corbaname"},
#line 775 "j9java8packages.gperf"
      {"java/util/concurrent/locks"},
#line 791 "j9java8packages.gperf"
      {"javax/crypto/spec"},
#line 817 "j9java8packages.gperf"
      {"javax/naming/ldap"},
#line 636 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/api/pipe/helper"},
#line 487 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/generator/annotation/spec"},
#line 127 "j9java8packages.gperf"
      {"com/ibm/gpu"},
#line 419 "j9java8packages.gperf"
      {"com/sun/swing/internal/plaf/basic/resources"},
#line 809 "j9java8packages.gperf"
      {"javax/management/openmbean"},
#line 353 "j9java8packages.gperf"
      {"com/sun/jndi/url/ldap"},
#line 342 "j9java8packages.gperf"
      {"com/sun/jndi/ldap/pool"},
#line 421 "j9java8packages.gperf"
      {"com/sun/swing/internal/plaf/synth/resources"},
#line 193 "j9java8packages.gperf"
      {"com/ibm/rmi/util/ranges"},
#line 837 "j9java8packages.gperf"
      {"javax/sound/midi"},
#line 420 "j9java8packages.gperf"
      {"com/sun/swing/internal/plaf/metal/resources"},
#line 1147 "j9java8packages.gperf"
      {"sun/tools/jconsole"},
#line 433 "j9java8packages.gperf"
      {"com/sun/tools/doclets/formats/html/resources"},
#line 870 "j9java8packages.gperf"
      {"javax/xml/bind/attachment"},
#line 984 "j9java8packages.gperf"
      {"org/xml/sax/helpers"},
#line 604 "j9java8packages.gperf"
      {"com/sun/xml/internal/rngom/xml/sax"},
#line 1013 "j9java8packages.gperf"
      {"sun/java2d/opengl"},
#line 760 "j9java8packages.gperf"
      {"java/security/acl"},
#line 1110 "j9java8packages.gperf"
      {"sun/swing/table"},
#line 435 "j9java8packages.gperf"
      {"com/sun/tools/doclets/internal/toolkit/builders"},
#line 370 "j9java8packages.gperf"
      {"com/sun/org/apache/xerces/internal/parsers"},
#line 434 "j9java8packages.gperf"
      {"com/sun/tools/doclets/internal/toolkit"},
#line 439 "j9java8packages.gperf"
      {"com/sun/tools/doclets/internal/toolkit/util/links"},
#line 431 "j9java8packages.gperf"
      {"com/sun/tools/doclets/formats/html"},
#line 436 "j9java8packages.gperf"
      {"com/sun/tools/doclets/internal/toolkit/resources"},
#line 752 "j9java8packages.gperf"
      {"java/nio/file/attribute"},
#line 1073 "j9java8packages.gperf"
      {"sun/reflect/generics/parser"},
#line 936 "j9java8packages.gperf"
      {"jdk/nashorn/internal/scripts"},
#line 1077 "j9java8packages.gperf"
      {"sun/reflect/generics/tree"},
#line 1079 "j9java8packages.gperf"
      {"sun/reflect/misc"},
#line 1001 "j9java8packages.gperf"
      {"sun/dc/path"},
#line 865 "j9java8packages.gperf"
      {"javax/transaction/xa"},
#line 218 "j9java8packages.gperf"
      {"com/oracle/webservices/internal/api/message"},
#line 717 "j9java8packages.gperf"
      {"com/sun/xml/internal/xsom/util"},
#line 700 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/util/pipe"},
#line 929 "j9java8packages.gperf"
      {"jdk/nashorn/internal/runtime/options"},
#line 930 "j9java8packages.gperf"
      {"jdk/nashorn/internal/runtime/regexp"},
#line 803 "j9java8packages.gperf"
      {"javax/lang/model/type"},
#line 217 "j9java8packages.gperf"
      {"com/oracle/webservices/internal/api/databinding"},
#line 599 "j9java8packages.gperf"
      {"com/sun/xml/internal/rngom/parse"},
#line 168 "j9java8packages.gperf"
      {"com/ibm/nio/ch"},
#line 438 "j9java8packages.gperf"
      {"com/sun/tools/doclets/internal/toolkit/util"},
#line 935 "j9java8packages.gperf"
      {"jdk/nashorn/internal/runtime/regexp/joni/exception"},
#line 601 "j9java8packages.gperf"
      {"com/sun/xml/internal/rngom/parse/host"},
#line 891 "j9java8packages.gperf"
      {"javax/xml/validation"},
#line 1105 "j9java8packages.gperf"
      {"sun/swing"},
#line 625 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/api/fastinfoset"},
#line 600 "j9java8packages.gperf"
      {"com/sun/xml/internal/rngom/parse/compact"},
#line 499 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/reader/relaxng"},
#line 661 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/encoding/soap"},
#line 695 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/transport/http"},
#line 698 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/util"},
#line 220 "j9java8packages.gperf"
      {"com/oracle/webservices/internal/impl/internalspi/encoding"},
#line 701 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/util/xml"},
#line 125 "j9java8packages.gperf"
      {"com/ibm/dtfj/utils"},
#line 869 "j9java8packages.gperf"
      {"javax/xml/bind/annotation/adapters"},
#line 95 "j9java8packages.gperf"
      {"com/ibm/dtfj/image"},
#line 808 "j9java8packages.gperf"
      {"javax/management/monitor"},
#line 676 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/policy/jaxws"},
#line 811 "j9java8packages.gperf"
      {"javax/management/remote"},
#line 682 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/policy/spi"},
#line 124 "j9java8packages.gperf"
      {"com/ibm/dtfj/runtime"},
#line 677 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/policy/jaxws/spi"},
#line 442 "j9java8packages.gperf"
      {"com/sun/tools/doclint/resources"},
#line 693 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/streaming"},
#line 1040 "j9java8packages.gperf"
      {"sun/net/ftp/impl"},
#line 136 "j9java8packages.gperf"
      {"com/ibm/jtc/orb/nio"},
#line 683 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/policy/subject"},
#line 367 "j9java8packages.gperf"
      {"com/sun/org/apache/xerces/internal/jaxp"},
#line 812 "j9java8packages.gperf"
      {"javax/management/remote/rmi"},
#line 288 "j9java8packages.gperf"
      {"com/sun/corba/se/spi/legacy/interceptor"},
#line 579 "j9java8packages.gperf"
      {"com/sun/xml/internal/messaging/saaj/soap/name"},
#line 1146 "j9java8packages.gperf"
      {"sun/tools/jcmd"},
#line 316 "j9java8packages.gperf"
      {"com/sun/java/swing"},
#line 1074 "j9java8packages.gperf"
      {"sun/reflect/generics/reflectiveObjects"},
#line 876 "j9java8packages.gperf"
      {"javax/xml/crypto/dsig/dom"},
#line 211 "j9java8packages.gperf"
      {"com/ibm/security/validator"},
#line 489 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/generator/bean/field"},
#line 583 "j9java8packages.gperf"
      {"com/sun/xml/internal/messaging/saaj/util/transform"},
#line 173 "j9java8packages.gperf"
      {"com/ibm/oti/shared"},
#line 741 "j9java8packages.gperf"
      {"java/lang/management"},
#line 602 "j9java8packages.gperf"
      {"com/sun/xml/internal/rngom/parse/xml"},
#line 878 "j9java8packages.gperf"
      {"javax/xml/crypto/dsig/spec"},
#line 699 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/util/exception"},
#line 1087 "j9java8packages.gperf"
      {"sun/rmi/server"},
#line 957 "j9java8packages.gperf"
      {"org/omg/IOP"},
#line 523 "j9java8packages.gperf"
      {"com/sun/tools/javah/resources"},
#line 248 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/monitoring"},
#line 582 "j9java8packages.gperf"
      {"com/sun/xml/internal/messaging/saaj/util"},
#line 943 "j9java8packages.gperf"
      {"org/omg/CORBA"},
#line 153 "j9java8packages.gperf"
      {"com/ibm/jvm/j9/dump/extract"},
#line 320 "j9java8packages.gperf"
      {"com/sun/java/swing/plaf/motif/resources"},
#line 578 "j9java8packages.gperf"
      {"com/sun/xml/internal/messaging/saaj/soap/impl"},
#line 998 "j9java8packages.gperf"
      {"sun/awt/windows"},
#line 737 "j9java8packages.gperf"
      {"java/lang"},
#line 948 "j9java8packages.gperf"
      {"org/omg/CORBA_2_3"},
#line 1107 "j9java8packages.gperf"
      {"sun/swing/plaf"},
#line 917 "j9java8packages.gperf"
      {"jdk/nashorn/internal/ir/annotations"},
#line 862 "j9java8packages.gperf"
      {"javax/swing/undo"},
#line 1111 "j9java8packages.gperf"
      {"sun/swing/text"},
#line 849 "j9java8packages.gperf"
      {"javax/swing/filechooser"},
#line 534 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind/api"},
#line 313 "j9java8packages.gperf"
      {"com/sun/istack/internal/tools"},
#line 1030 "j9java8packages.gperf"
      {"sun/management"},
#line 440 "j9java8packages.gperf"
      {"com/sun/tools/doclets/standard"},
#line 641 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/api/wsdl/parser"},
#line 642 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/api/wsdl/writer"},
#line 571 "j9java8packages.gperf"
      {"com/sun/xml/internal/messaging/saaj"},
#line 678 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/policy/privateutil"},
#line 871 "j9java8packages.gperf"
      {"javax/xml/bind/helpers"},
#line 165 "j9java8packages.gperf"
      {"com/ibm/net/rdma/jverbs/endpoints"},
#line 556 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind/v2/util"},
#line 679 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/policy/sourcemodel"},
#line 166 "j9java8packages.gperf"
      {"com/ibm/net/rdma/jverbs/verbs"},
#line 607 "j9java8packages.gperf"
      {"com/sun/xml/internal/stream/buffer"},
#line 539 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind/v2"},
#line 1031 "j9java8packages.gperf"
      {"sun/management/counter"},
#line 763 "j9java8packages.gperf"
      {"java/security/spec"},
#line 558 "j9java8packages.gperf"
      {"com/sun/xml/internal/fastinfoset"},
#line 477 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/wsdl/parser"},
#line 468 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/wscompile"},
#line 561 "j9java8packages.gperf"
      {"com/sun/xml/internal/fastinfoset/dom"},
#line 565 "j9java8packages.gperf"
      {"com/sun/xml/internal/fastinfoset/stax/events"},
#line 851 "j9java8packages.gperf"
      {"javax/swing/plaf/basic"},
#line 357 "j9java8packages.gperf"
      {"com/sun/media/sound"},
#line 134 "j9java8packages.gperf"
      {"com/ibm/jtc/orb/iiop"},
#line 631 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/api/model"},
#line 535 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind/api/impl"},
#line 473 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/wsdl/document/mime"},
#line 633 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/api/model/wsdl"},
#line 178 "j9java8packages.gperf"
      {"com/ibm/rmi/channel/giop"},
#line 470 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/wsdl/document"},
#line 472 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/wsdl/document/jaxws"},
#line 432 "j9java8packages.gperf"
      {"com/sun/tools/doclets/formats/html/markup"},
#line 634 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/api/model/wsdl/editable"},
#line 226 "j9java8packages.gperf"
      {"com/sun/beans/decoder"},
#line 210 "j9java8packages.gperf"
      {"com/ibm/security/util/text/resources"},
#line 838 "j9java8packages.gperf"
      {"javax/sound/midi/spi"},
#line 99 "j9java8packages.gperf"
      {"com/ibm/dtfj/java"},
#line 720 "j9java8packages.gperf"
      {"java/awt"},
#line 274 "j9java8packages.gperf"
      {"com/sun/corba/se/pept/broker"},
#line 728 "j9java8packages.gperf"
      {"java/awt/im"},
#line 724 "j9java8packages.gperf"
      {"java/awt/dnd/peer"},
#line 721 "j9java8packages.gperf"
      {"java/awt/color"},
#line 822 "j9java8packages.gperf"
      {"javax/print/attribute"},
#line 449 "j9java8packages.gperf"
      {"com/sun/tools/internal/jxc/gen/config"},
#line 569 "j9java8packages.gperf"
      {"com/sun/xml/internal/fastinfoset/util"},
#line 155 "j9java8packages.gperf"
      {"com/ibm/lang"},
#line 474 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/wsdl/document/schema"},
#line 410 "j9java8packages.gperf"
      {"com/sun/rmi/rmid"},
#line 96 "j9java8packages.gperf"
      {"com/ibm/dtfj/image/j9"},
#line 852 "j9java8packages.gperf"
      {"javax/swing/plaf/metal"},
#line 233 "j9java8packages.gperf"
      {"com/sun/codemodel/internal/util"},
#line 212 "j9java8packages.gperf"
      {"com/ibm/security/x509"},
#line 738 "j9java8packages.gperf"
      {"java/lang/annotation"},
#line 969 "j9java8packages.gperf"
      {"org/omg/SendingContext"},
#line 366 "j9java8packages.gperf"
      {"com/sun/org/apache/xerces/internal/dom"},
#line 459 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/processor/model/jaxb"},
#line 1034 "j9java8packages.gperf"
      {"sun/management/jmxremote"},
#line 580 "j9java8packages.gperf"
      {"com/sun/xml/internal/messaging/saaj/soap/ver1_1"},
#line 1076 "j9java8packages.gperf"
      {"sun/reflect/generics/scope"},
#line 692 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/spi/db"},
#line 395 "j9java8packages.gperf"
      {"com/sun/org/apache/xpath/internal/jaxp"},
#line 407 "j9java8packages.gperf"
      {"com/sun/org/omg/CORBA/portable"},
#line 214 "j9java8packages.gperf"
      {"com/ibm/tools/attach/target"},
#line 648 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/client/dispatch"},
#line 257 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/orbutil/closure"},
#line 743 "j9java8packages.gperf"
      {"java/lang/reflect"},
#line 258 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/orbutil/concurrent"},
#line 179 "j9java8packages.gperf"
      {"com/ibm/rmi/channel/orb"},
#line 255 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/orb"},
#line 1143 "j9java8packages.gperf"
      {"sun/tools/jar/resources"},
#line 608 "j9java8packages.gperf"
      {"com/sun/xml/internal/stream/buffer/sax"},
#line 609 "j9java8packages.gperf"
      {"com/sun/xml/internal/stream/buffer/stax"},
#line 567 "j9java8packages.gperf"
      {"com/sun/xml/internal/fastinfoset/stax/util"},
#line 208 "j9java8packages.gperf"
      {"com/ibm/security/util/calendar"},
#line 564 "j9java8packages.gperf"
      {"com/sun/xml/internal/fastinfoset/stax"},
#line 256 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/orbutil"},
#line 596 "j9java8packages.gperf"
      {"com/sun/xml/internal/rngom/dt"},
#line 590 "j9java8packages.gperf"
      {"com/sun/xml/internal/rngom/ast/builder"},
#line 506 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/writer"},
#line 656 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/developer"},
#line 789 "j9java8packages.gperf"
      {"javax/crypto"},
#line 970 "j9java8packages.gperf"
      {"org/omg/stub/java/rmi"},
#line 337 "j9java8packages.gperf"
      {"com/sun/jmx/remote/util"},
#line 1157 "j9java8packages.gperf"
      {"sun/util/locale/provider"},
#line 581 "j9java8packages.gperf"
      {"com/sun/xml/internal/messaging/saaj/soap/ver1_2"},
#line 733 "j9java8packages.gperf"
      {"java/awt/print"},
#line 671 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/model"},
#line 887 "j9java8packages.gperf"
      {"javax/xml/transform/dom"},
#line 640 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/api/streaming"},
#line 576 "j9java8packages.gperf"
      {"com/sun/xml/internal/messaging/saaj/soap"},
#line 522 "j9java8packages.gperf"
      {"com/sun/tools/javah"},
#line 1009 "j9java8packages.gperf"
      {"sun/java2d/cmm/kcms"},
#line 422 "j9java8packages.gperf"
      {"com/sun/tools/attach"},
#line 264 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/protocol/giopmsgheaders"},
#line 1081 "j9java8packages.gperf"
      {"sun/rmi/registry"},
#line 321 "j9java8packages.gperf"
      {"com/sun/java/swing/plaf/nimbus"},
#line 250 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/naming/namingutil"},
#line 814 "j9java8packages.gperf"
      {"javax/naming"},
#line 232 "j9java8packages.gperf"
      {"com/sun/codemodel/internal/fmt"},
#line 372 "j9java8packages.gperf"
      {"com/sun/org/apache/xml/internal/security/algorithms"},
#line 387 "j9java8packages.gperf"
      {"com/sun/org/apache/xml/internal/security/signature"},
#line 389 "j9java8packages.gperf"
      {"com/sun/org/apache/xml/internal/security/transforms"},
#line 391 "j9java8packages.gperf"
      {"com/sun/org/apache/xml/internal/security/transforms/params"},
#line 388 "j9java8packages.gperf"
      {"com/sun/org/apache/xml/internal/security/signature/reference"},
#line 563 "j9java8packages.gperf"
      {"com/sun/xml/internal/fastinfoset/sax"},
#line 373 "j9java8packages.gperf"
      {"com/sun/org/apache/xml/internal/security/algorithms/implementations"},
#line 100 "j9java8packages.gperf"
      {"com/ibm/dtfj/java/j9"},
#line 374 "j9java8packages.gperf"
      {"com/sun/org/apache/xml/internal/security/c14n"},
#line 390 "j9java8packages.gperf"
      {"com/sun/org/apache/xml/internal/security/transforms/implementations"},
#line 757 "j9java8packages.gperf"
      {"java/rmi/registry"},
#line 375 "j9java8packages.gperf"
      {"com/sun/org/apache/xml/internal/security/c14n/helper"},
#line 376 "j9java8packages.gperf"
      {"com/sun/org/apache/xml/internal/security/c14n/implementations"},
#line 1148 "j9java8packages.gperf"
      {"sun/tools/jconsole/inspector"},
#line 744 "j9java8packages.gperf"
      {"java/math"},
#line 909 "j9java8packages.gperf"
      {"jdk/internal/org/xml/sax/helpers"},
#line 213 "j9java8packages.gperf"
      {"com/ibm/tools/attach/attacher"},
#line 796 "j9java8packages.gperf"
      {"javax/imageio/plugins/jpeg"},
#line 532 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind"},
#line 1078 "j9java8packages.gperf"
      {"sun/reflect/generics/visitor"},
#line 899 "j9java8packages.gperf"
      {"javax/xml/ws/wsaddressing"},
#line 335 "j9java8packages.gperf"
      {"com/sun/jmx/remote/protocol/rmi"},
#line 650 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/commons/xmlutil"},
#line 1101 "j9java8packages.gperf"
      {"sun/security/tools/keytool"},
#line 594 "j9java8packages.gperf"
      {"com/sun/xml/internal/rngom/binary/visitor"},
#line 834 "j9java8packages.gperf"
      {"javax/security/auth/x500"},
#line 128 "j9java8packages.gperf"
      {"com/ibm/java/diagnostics/utils"},
#line 939 "j9java8packages.gperf"
      {"netscape/javascript"},
#line 295 "j9java8packages.gperf"
      {"com/sun/corba/se/spi/orbutil/proxy"},
#line 130 "j9java8packages.gperf"
      {"com/ibm/java/diagnostics/utils/plugins"},
#line 914 "j9java8packages.gperf"
      {"jdk/nashorn/internal/codegen"},
#line 603 "j9java8packages.gperf"
      {"com/sun/xml/internal/rngom/util"},
#line 879 "j9java8packages.gperf"
      {"javax/xml/datatype"},
#line 129 "j9java8packages.gperf"
      {"com/ibm/java/diagnostics/utils/commands"},
#line 400 "j9java8packages.gperf"
      {"com/sun/org/glassfish/external/statistics"},
#line 251 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/naming/pcosnaming"},
#line 739 "j9java8packages.gperf"
      {"java/lang/instrument"},
#line 398 "j9java8packages.gperf"
      {"com/sun/org/glassfish/external/probe/provider"},
#line 397 "j9java8packages.gperf"
      {"com/sun/org/glassfish/external/arc"},
#line 560 "j9java8packages.gperf"
      {"com/sun/xml/internal/fastinfoset/alphabet"},
#line 399 "j9java8packages.gperf"
      {"com/sun/org/glassfish/external/probe/provider/annotations"},
#line 204 "j9java8packages.gperf"
      {"com/ibm/security/rfc5915"},
#line 1015 "j9java8packages.gperf"
      {"sun/java2d/pipe/hw"},
#line 74 "j9java8packages.gperf"
      {"com/ibm/CORBA/channel/giop"},
#line 471 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/wsdl/document/http"},
#line 427 "j9java8packages.gperf"
      {"com/sun/tools/corba/se/idl/som/cff"},
#line 616 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/addressing/model"},
#line 658 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/encoding"},
#line 206 "j9java8packages.gperf"
      {"com/ibm/security/tools"},
#line 623 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/api/config/management/policy"},
#line 727 "j9java8packages.gperf"
      {"java/awt/geom"},
#line 101 "j9java8packages.gperf"
      {"com/ibm/dtfj/java/javacore"},
#line 637 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/api/policy"},
#line 662 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/encoding/soap/streaming"},
#line 453 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/api/wsdl"},
#line 1033 "j9java8packages.gperf"
      {"sun/management/jdp"},
#line 685 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/protocol/xml"},
#line 497 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/reader/gbind"},
#line 1136 "j9java8packages.gperf"
      {"sun/text/resources/sk"},
#line 1089 "j9java8packages.gperf"
      {"sun/rmi/transport/proxy"},
#line 1022 "j9java8packages.gperf"
      {"sun/jvmstat/perfdata/monitor"},
#line 545 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind/v2/model/runtime"},
#line 199 "j9java8packages.gperf"
      {"com/ibm/security/pkcs5"},
#line 196 "j9java8packages.gperf"
      {"com/ibm/security/pkcs1"},
#line 537 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind/unmarshaller"},
#line 201 "j9java8packages.gperf"
      {"com/ibm/security/pkcs8"},
#line 538 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind/util"},
#line 200 "j9java8packages.gperf"
      {"com/ibm/security/pkcs7"},
#line 542 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind/v2/model/core"},
#line 541 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind/v2/model/annotation"},
#line 396 "j9java8packages.gperf"
      {"com/sun/org/glassfish/external/amx"},
#line 672 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/model/soap"},
#line 408 "j9java8packages.gperf"
      {"com/sun/org/omg/SendingContext"},
#line 784 "j9java8packages.gperf"
      {"javax/accessibility"},
#line 402 "j9java8packages.gperf"
      {"com/sun/org/glassfish/external/statistics/impl"},
#line 171 "j9java8packages.gperf"
      {"com/ibm/org/omg/SendingContext"},
#line 202 "j9java8packages.gperf"
      {"com/ibm/security/pkcs9"},
#line 674 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/org/objectweb/asm"},
#line 409 "j9java8packages.gperf"
      {"com/sun/org/omg/SendingContext/CodeBasePackage"},
#line 543 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind/v2/model/impl"},
#line 215 "j9java8packages.gperf"
      {"com/ibm/virtualization/management"},
#line 172 "j9java8packages.gperf"
      {"com/ibm/org/omg/SendingContext/CodeBasePackage"},
#line 98 "j9java8packages.gperf"
      {"com/ibm/dtfj/image/javacore"},
#line 219 "j9java8packages.gperf"
      {"com/oracle/webservices/internal/impl/encoding"},
#line 568 "j9java8packages.gperf"
      {"com/sun/xml/internal/fastinfoset/tools"},
#line 87 "j9java8packages.gperf"
      {"com/ibm/dtfj/addressspace"},
#line 122 "j9java8packages.gperf"
      {"com/ibm/dtfj/phd/parser"},
#line 283 "j9java8packages.gperf"
      {"com/sun/corba/se/spi/encoding"},
#line 198 "j9java8packages.gperf"
      {"com/ibm/security/pkcs12"},
#line 952 "j9java8packages.gperf"
      {"org/omg/CosNaming/NamingContextPackage"},
#line 89 "j9java8packages.gperf"
      {"com/ibm/dtfj/corereaders"},
#line 618 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/addressing/v200408"},
#line 951 "j9java8packages.gperf"
      {"org/omg/CosNaming/NamingContextExtPackage"},
#line 521 "j9java8packages.gperf"
      {"com/sun/tools/javadoc/resources"},
#line 807 "j9java8packages.gperf"
      {"javax/management/modelmbean"},
#line 275 "j9java8packages.gperf"
      {"com/sun/corba/se/pept/encoding"},
#line 227 "j9java8packages.gperf"
      {"com/sun/beans/editors"},
#line 829 "j9java8packages.gperf"
      {"javax/security/auth"},
#line 572 "j9java8packages.gperf"
      {"com/sun/xml/internal/messaging/saaj/client/p2p"},
#line 203 "j9java8packages.gperf"
      {"com/ibm/security/pkcsutil"},
#line 726 "j9java8packages.gperf"
      {"java/awt/font"},
#line 577 "j9java8packages.gperf"
      {"com/sun/xml/internal/messaging/saaj/soap/dynamic"},
#line 378 "j9java8packages.gperf"
      {"com/sun/org/apache/xml/internal/security/exceptions"},
#line 660 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/encoding/policy"},
#line 667 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/message/jaxb"},
#line 239 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/encoding"},
#line 368 "j9java8packages.gperf"
      {"com/sun/org/apache/xerces/internal/jaxp/datatype"},
#line 1163 "j9java8packages.gperf"
      {"com/ibm/virtualization/management/internal"},
#line 496 "j9java8packages.gperf"
      {"com/sun/tools/internal/xjc/reader/dtd/bindinfo"},
#line 437 "j9java8packages.gperf"
      {"com/sun/tools/doclets/internal/toolkit/taglets"},
#line 120 "j9java8packages.gperf"
      {"com/ibm/dtfj/javacore/parser/j9/section/title"},
#line 544 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind/v2/model/nav"},
#line 684 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/protocol/soap"},
#line 1084 "j9java8packages.gperf"
      {"sun/rmi/rmic/newrmic"},
#line 123 "j9java8packages.gperf"
      {"com/ibm/dtfj/phd/util"},
#line 109 "j9java8packages.gperf"
      {"com/ibm/dtfj/javacore/parser/j9"},
#line 1097 "j9java8packages.gperf"
      {"sun/security/provider/certpath"},
#line 547 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind/v2/runtime"},
#line 1050 "j9java8packages.gperf"
      {"sun/net/www/content/text"},
#line 665 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/handler"},
#line 759 "j9java8packages.gperf"
      {"java/security"},
#line 550 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind/v2/runtime/reflect"},
#line 762 "j9java8packages.gperf"
      {"java/security/interfaces"},
#line 536 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind/marshaller"},
#line 131 "j9java8packages.gperf"
      {"com/ibm/java/diagnostics/utils/plugins/impl"},
#line 840 "j9java8packages.gperf"
      {"javax/sound/sampled/spi"},
#line 469 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/wscompile/plugin/at_generated"},
#line 723 "j9java8packages.gperf"
      {"java/awt/dnd"},
#line 296 "j9java8packages.gperf"
      {"com/sun/corba/se/spi/orbutil/threadpool"},
#line 729 "j9java8packages.gperf"
      {"java/awt/im/spi"},
#line 552 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind/v2/runtime/unmarshaller"},
#line 790 "j9java8packages.gperf"
      {"javax/crypto/interfaces"},
#line 90 "j9java8packages.gperf"
      {"com/ibm/dtfj/corereaders/j9"},
#line 111 "j9java8packages.gperf"
      {"com/ibm/dtfj/javacore/parser/j9/section/classloader"},
#line 290 "j9java8packages.gperf"
      {"com/sun/corba/se/spi/monitoring"},
#line 953 "j9java8packages.gperf"
      {"org/omg/Dynamic"},
#line 1071 "j9java8packages.gperf"
      {"sun/reflect/annotation"},
#line 875 "j9java8packages.gperf"
      {"javax/xml/crypto/dsig"},
#line 1128 "j9java8packages.gperf"
      {"sun/text/resources/hu"},
#line 573 "j9java8packages.gperf"
      {"com/sun/xml/internal/messaging/saaj/packaging/mime"},
#line 574 "j9java8packages.gperf"
      {"com/sun/xml/internal/messaging/saaj/packaging/mime/internet"},
#line 664 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/fault"},
#line 632 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/api/model/soap"},
#line 595 "j9java8packages.gperf"
      {"com/sun/xml/internal/rngom/digested"},
#line 853 "j9java8packages.gperf"
      {"javax/swing/plaf/multi"},
#line 854 "j9java8packages.gperf"
      {"javax/swing/plaf/nimbus"},
#line 249 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/naming/cosnaming"},
#line 941 "j9java8packages.gperf"
      {"org/jcp/xml/dsig/internal"},
#line 950 "j9java8packages.gperf"
      {"org/omg/CosNaming"},
#line 75 "j9java8packages.gperf"
      {"com/ibm/CORBA/channel/orb"},
#line 960 "j9java8packages.gperf"
      {"org/omg/Messaging"},
#line 261 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/orbutil/threadpool"},
#line 575 "j9java8packages.gperf"
      {"com/sun/xml/internal/messaging/saaj/packaging/mime/util"},
#line 570 "j9java8packages.gperf"
      {"com/sun/xml/internal/fastinfoset/vocab"},
#line 553 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind/v2/schemagen"},
#line 554 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind/v2/schemagen/episode"},
#line 949 "j9java8packages.gperf"
      {"org/omg/CORBA_2_3/portable"},
#line 401 "j9java8packages.gperf"
      {"com/sun/org/glassfish/external/statistics/annotations"},
#line 475 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/wsdl/document/soap"},
#line 902 "j9java8packages.gperf"
      {"jdk/internal/org/objectweb/asm"},
#line 905 "j9java8packages.gperf"
      {"jdk/internal/org/objectweb/asm/tree"},
#line 730 "j9java8packages.gperf"
      {"java/awt/image"},
#line 906 "j9java8packages.gperf"
      {"jdk/internal/org/objectweb/asm/tree/analysis"},
#line 620 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/api/addressing"},
#line 904 "j9java8packages.gperf"
      {"jdk/internal/org/objectweb/asm/signature"},
#line 624 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/api/databinding"},
#line 675 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/policy"},
#line 126 "j9java8packages.gperf"
      {"com/ibm/dtfj/utils/file"},
#line 963 "j9java8packages.gperf"
      {"org/omg/PortableServer"},
#line 88 "j9java8packages.gperf"
      {"com/ibm/dtfj/binaryreaders"},
#line 148 "j9java8packages.gperf"
      {"com/ibm/jvm/dtfjview/spi"},
#line 815 "j9java8packages.gperf"
      {"javax/naming/directory"},
#line 555 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind/v2/schemagen/xmlschema"},
#line 559 "j9java8packages.gperf"
      {"com/sun/xml/internal/fastinfoset/algorithm"},
#line 966 "j9java8packages.gperf"
      {"org/omg/PortableServer/POAPackage"},
#line 907 "j9java8packages.gperf"
      {"jdk/internal/org/objectweb/asm/util"},
#line 149 "j9java8packages.gperf"
      {"com/ibm/jvm/dtfjview/tools"},
#line 1025 "j9java8packages.gperf"
      {"sun/jvmstat/perfdata/monitor/protocol/rmi"},
#line 1023 "j9java8packages.gperf"
      {"sun/jvmstat/perfdata/monitor/protocol/file"},
#line 282 "j9java8packages.gperf"
      {"com/sun/corba/se/spi/copyobject"},
#line 319 "j9java8packages.gperf"
      {"com/sun/java/swing/plaf/motif"},
#line 597 "j9java8packages.gperf"
      {"com/sun/xml/internal/rngom/dt/builtin"},
#line 406 "j9java8packages.gperf"
      {"com/sun/org/omg/CORBA/ValueDefPackage"},
#line 170 "j9java8packages.gperf"
      {"com/ibm/org/omg/CORBA/ValueDefPackage"},
#line 392 "j9java8packages.gperf"
      {"com/sun/org/apache/xml/internal/security/utils"},
#line 133 "j9java8packages.gperf"
      {"com/ibm/jtc/orb/asynch"},
#line 246 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/legacy/connection"},
#line 393 "j9java8packages.gperf"
      {"com/sun/org/apache/xml/internal/security/utils/resolver"},
#line 394 "j9java8packages.gperf"
      {"com/sun/org/apache/xml/internal/security/utils/resolver/implementations"},
#line 680 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/policy/sourcemodel/attach"},
#line 850 "j9java8packages.gperf"
      {"javax/swing/plaf"},
#line 1024 "j9java8packages.gperf"
      {"sun/jvmstat/perfdata/monitor/protocol/local"},
#line 1171 "j9java8packages.gperf"
      {"com/ibm/oti/lang"},
#line 657 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/dump"},
#line 934 "j9java8packages.gperf"
      {"jdk/nashorn/internal/runtime/regexp/joni/encoding"},
#line 860 "j9java8packages.gperf"
      {"javax/swing/text/rtf"},
#line 613 "j9java8packages.gperf"
      {"com/sun/xml/internal/txw2/output"},
#line 92 "j9java8packages.gperf"
      {"com/ibm/dtfj/corereaders/zos/le"},
#line 1108 "j9java8packages.gperf"
      {"sun/swing/plaf/synth"},
#line 106 "j9java8packages.gperf"
      {"com/ibm/dtfj/javacore/parser/framework/parser"},
#line 653 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/config/metro/util"},
#line 566 "j9java8packages.gperf"
      {"com/sun/xml/internal/fastinfoset/stax/factory"},
#line 732 "j9java8packages.gperf"
      {"java/awt/peer"},
#line 150 "j9java8packages.gperf"
      {"com/ibm/jvm/dtfjview/tools/impl"},
#line 107 "j9java8packages.gperf"
      {"com/ibm/dtfj/javacore/parser/framework/scanner"},
#line 725 "j9java8packages.gperf"
      {"java/awt/event"},
#line 945 "j9java8packages.gperf"
      {"org/omg/CORBA/ORBPackage"},
#line 154 "j9java8packages.gperf"
      {"com/ibm/jvm/j9/dump/indexsupport"},
#line 742 "j9java8packages.gperf"
      {"java/lang/ref"},
#line 831 "j9java8packages.gperf"
      {"javax/security/auth/kerberos"},
#line 197 "j9java8packages.gperf"
      {"com/ibm/security/pkcs10"},
#line 1026 "j9java8packages.gperf"
      {"sun/jvmstat/perfdata/monitor/v1_0"},
#line 971 "j9java8packages.gperf"
      {"org/w3c/dom"},
#line 234 "j9java8packages.gperf"
      {"com/sun/codemodel/internal/writer"},
#line 476 "j9java8packages.gperf"
      {"com/sun/tools/internal/ws/wsdl/framework"},
#line 259 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/orbutil/fsm"},
#line 94 "j9java8packages.gperf"
      {"com/ibm/dtfj/corereaders/zos/util"},
#line 97 "j9java8packages.gperf"
      {"com/ibm/dtfj/image/j9/corrupt"},
#line 1112 "j9java8packages.gperf"
      {"sun/swing/text/html"},
#line 289 "j9java8packages.gperf"
      {"com/sun/corba/se/spi/logging"},
#line 112 "j9java8packages.gperf"
      {"com/ibm/dtfj/javacore/parser/j9/section/common"},
#line 113 "j9java8packages.gperf"
      {"com/ibm/dtfj/javacore/parser/j9/section/environment"},
#line 121 "j9java8packages.gperf"
      {"com/ibm/dtfj/phd"},
#line 247 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/logging"},
#line 615 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/addressing"},
#line 1027 "j9java8packages.gperf"
      {"sun/jvmstat/perfdata/monitor/v2_0"},
#line 551 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind/v2/runtime/reflect/opt"},
#line 1059 "j9java8packages.gperf"
      {"sun/net/www/protocol/jar"},
#line 117 "j9java8packages.gperf"
      {"com/ibm/dtfj/javacore/parser/j9/section/platform"},
#line 1052 "j9java8packages.gperf"
      {"sun/net/www/protocol/file"},
#line 961 "j9java8packages.gperf"
      {"org/omg/PortableInterceptor"},
#line 1035 "j9java8packages.gperf"
      {"sun/management/resources"},
#line 652 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/config/metro/dev"},
#line 236 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/copyobject"},
#line 546 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind/v2/model/util"},
#line 403 "j9java8packages.gperf"
      {"com/sun/org/glassfish/gmbal"},
#line 654 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/db"},
#line 1159 "j9java8packages.gperf"
      {"sun/util/logging/resources"},
#line 93 "j9java8packages.gperf"
      {"com/ibm/dtfj/corereaders/zos/mvs"},
#line 404 "j9java8packages.gperf"
      {"com/sun/org/glassfish/gmbal/util"},
#line 962 "j9java8packages.gperf"
      {"org/omg/PortableInterceptor/ORBInitInfoPackage"},
#line 1051 "j9java8packages.gperf"
      {"sun/net/www/http"},
#line 371 "j9java8packages.gperf"
      {"com/sun/org/apache/xml/internal/security"},
#line 344 "j9java8packages.gperf"
      {"com/sun/jndi/rmi/registry"},
#line 377 "j9java8packages.gperf"
      {"com/sun/org/apache/xml/internal/security/encryption"},
#line 102 "j9java8packages.gperf"
      {"com/ibm/dtfj/javacore/builder"},
#line 1085 "j9java8packages.gperf"
      {"sun/rmi/rmic/newrmic/jrmp"},
#line 681 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/policy/sourcemodel/wspolicy"},
#line 303 "j9java8packages.gperf"
      {"com/sun/imageio/plugins/common"},
#line 947 "j9java8packages.gperf"
      {"org/omg/CORBA/portable"},
#line 593 "j9java8packages.gperf"
      {"com/sun/xml/internal/rngom/binary"},
#line 379 "j9java8packages.gperf"
      {"com/sun/org/apache/xml/internal/security/keys"},
#line 281 "j9java8packages.gperf"
      {"com/sun/corba/se/spi/activation/RepositoryPackage"},
#line 383 "j9java8packages.gperf"
      {"com/sun/org/apache/xml/internal/security/keys/keyresolver"},
#line 385 "j9java8packages.gperf"
      {"com/sun/org/apache/xml/internal/security/keys/storage"},
#line 381 "j9java8packages.gperf"
      {"com/sun/org/apache/xml/internal/security/keys/content/keyvalues"},
#line 324 "j9java8packages.gperf"
      {"com/sun/java/util/jar/pack"},
#line 386 "j9java8packages.gperf"
      {"com/sun/org/apache/xml/internal/security/keys/storage/implementations"},
#line 384 "j9java8packages.gperf"
      {"com/sun/org/apache/xml/internal/security/keys/keyresolver/implementations"},
#line 915 "j9java8packages.gperf"
      {"jdk/nashorn/internal/codegen/types"},
#line 903 "j9java8packages.gperf"
      {"jdk/internal/org/objectweb/asm/commons"},
#line 380 "j9java8packages.gperf"
      {"com/sun/org/apache/xml/internal/security/keys/content"},
#line 839 "j9java8packages.gperf"
      {"javax/sound/sampled"},
#line 84 "j9java8packages.gperf"
      {"com/ibm/bidiTools/bdlayout"},
#line 443 "j9java8packages.gperf"
      {"com/sun/tools/example/debug/expr"},
#line 382 "j9java8packages.gperf"
      {"com/sun/org/apache/xml/internal/security/keys/content/x509"},
#line 1141 "j9java8packages.gperf"
      {"sun/tools/attach"},
#line 105 "j9java8packages.gperf"
      {"com/ibm/dtfj/javacore/parser/framework/input"},
#line 806 "j9java8packages.gperf"
      {"javax/management/loading"},
#line 318 "j9java8packages.gperf"
      {"com/sun/java/swing/plaf/gtk/resources"},
#line 968 "j9java8packages.gperf"
      {"org/omg/PortableServer/portable"},
#line 823 "j9java8packages.gperf"
      {"javax/print/attribute/standard"},
#line 548 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind/v2/runtime/output"},
#line 788 "j9java8packages.gperf"
      {"javax/annotation/processing"},
#line 912 "j9java8packages.gperf"
      {"jdk/nashorn/api/scripting"},
#line 336 "j9java8packages.gperf"
      {"com/sun/jmx/remote/security"},
#line 1102 "j9java8packages.gperf"
      {"sun/security/tools/policytool"},
#line 847 "j9java8packages.gperf"
      {"javax/swing/colorchooser"},
#line 1053 "j9java8packages.gperf"
      {"sun/net/www/protocol/ftp"},
#line 1058 "j9java8packages.gperf"
      {"sun/net/www/protocol/https"},
#line 731 "j9java8packages.gperf"
      {"java/awt/image/renderable"},
#line 1056 "j9java8packages.gperf"
      {"sun/net/www/protocol/http/ntlm"},
#line 704 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/wsdl/writer"},
#line 703 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/wsdl/parser"},
#line 139 "j9java8packages.gperf"
      {"com/ibm/jvm/dtfjview/commands"},
#line 705 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/wsdl/writer/document"},
#line 191 "j9java8packages.gperf"
      {"com/ibm/rmi/util/buffer"},
#line 142 "j9java8packages.gperf"
      {"com/ibm/jvm/dtfjview/commands/setcommands"},
#line 702 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/wsdl"},
#line 1158 "j9java8packages.gperf"
      {"sun/util/logging"},
#line 115 "j9java8packages.gperf"
      {"com/ibm/dtfj/javacore/parser/j9/section/monitor"},
#line 540 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind/v2/bytecode"},
#line 687 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/runtime/config"},
#line 967 "j9java8packages.gperf"
      {"org/omg/PortableServer/ServantLocatorPackage"},
#line 708 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/wsdl/writer/document/soap12"},
#line 617 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/addressing/policy"},
#line 116 "j9java8packages.gperf"
      {"com/ibm/dtfj/javacore/parser/j9/section/nativememory"},
#line 549 "j9java8packages.gperf"
      {"com/sun/xml/internal/bind/v2/runtime/property"},
#line 144 "j9java8packages.gperf"
      {"com/ibm/jvm/dtfjview/commands/xcommands"},
#line 928 "j9java8packages.gperf"
      {"jdk/nashorn/internal/runtime/logging"},
#line 118 "j9java8packages.gperf"
      {"com/ibm/dtfj/javacore/parser/j9/section/stack"},
#line 959 "j9java8packages.gperf"
      {"org/omg/IOP/CodecPackage"},
#line 222 "j9java8packages.gperf"
      {"com/sun/accessibility/internal/resources"},
#line 151 "j9java8packages.gperf"
      {"com/ibm/jvm/dtfjview/tools/utils"},
#line 315 "j9java8packages.gperf"
      {"com/sun/java/accessibility/util"},
#line 954 "j9java8packages.gperf"
      {"org/omg/DynamicAny"},
#line 976 "j9java8packages.gperf"
      {"org/w3c/dom/ls"},
#line 973 "j9java8packages.gperf"
      {"org/w3c/dom/css"},
#line 974 "j9java8packages.gperf"
      {"org/w3c/dom/events"},
#line 958 "j9java8packages.gperf"
      {"org/omg/IOP/CodecFactoryPackage"},
#line 91 "j9java8packages.gperf"
      {"com/ibm/dtfj/corereaders/zos/dumpreader"},
#line 965 "j9java8packages.gperf"
      {"org/omg/PortableServer/POAManagerPackage"},
#line 1049 "j9java8packages.gperf"
      {"sun/net/www"},
#line 722 "j9java8packages.gperf"
      {"java/awt/datatransfer"},
#line 1054 "j9java8packages.gperf"
      {"sun/net/www/protocol/http"},
#line 975 "j9java8packages.gperf"
      {"org/w3c/dom/html"},
#line 707 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/wsdl/writer/document/soap"},
#line 103 "j9java8packages.gperf"
      {"com/ibm/dtfj/javacore/builder/javacore"},
#line 920 "j9java8packages.gperf"
      {"jdk/nashorn/internal/lookup"},
#line 942 "j9java8packages.gperf"
      {"org/jcp/xml/dsig/internal/dom"},
#line 110 "j9java8packages.gperf"
      {"com/ibm/dtfj/javacore/parser/j9/registered"},
#line 322 "j9java8packages.gperf"
      {"com/sun/java/swing/plaf/windows"},
#line 562 "j9java8packages.gperf"
      {"com/sun/xml/internal/fastinfoset/org/apache/xerces/util"},
#line 260 "j9java8packages.gperf"
      {"com/sun/corba/se/impl/orbutil/graph"},
#line 1032 "j9java8packages.gperf"
      {"sun/management/counter/perf"},
#line 673 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/model/wsdl"},
#line 143 "j9java8packages.gperf"
      {"com/ibm/jvm/dtfjview/commands/showcommands"},
#line 141 "j9java8packages.gperf"
      {"com/ibm/jvm/dtfjview/commands/infocommands"},
#line 855 "j9java8packages.gperf"
      {"javax/swing/plaf/synth"},
#line 979 "j9java8packages.gperf"
      {"org/w3c/dom/traversal"},
#line 104 "j9java8packages.gperf"
      {"com/ibm/dtfj/javacore/parser/framework"},
#line 158 "j9java8packages.gperf"
      {"com/ibm/le/conditionhandling"},
#line 646 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/binding"},
#line 302 "j9java8packages.gperf"
      {"com/sun/imageio/plugins/bmp"},
#line 1060 "j9java8packages.gperf"
      {"sun/net/www/protocol/mailto"},
#line 1072 "j9java8packages.gperf"
      {"sun/reflect/generics/factory"},
#line 1075 "j9java8packages.gperf"
      {"sun/reflect/generics/repository"},
#line 709 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/wsdl/writer/document/xsd"},
#line 1061 "j9java8packages.gperf"
      {"sun/net/www/protocol/netdoc"},
#line 877 "j9java8packages.gperf"
      {"javax/xml/crypto/dsig/keyinfo"},
#line 651 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/config/management/policy"},
#line 972 "j9java8packages.gperf"
      {"org/w3c/dom/bootstrap"},
#line 323 "j9java8packages.gperf"
      {"com/sun/java/swing/plaf/windows/resources"},
#line 317 "j9java8packages.gperf"
      {"com/sun/java/swing/plaf/gtk"},
#line 964 "j9java8packages.gperf"
      {"org/omg/PortableServer/CurrentPackage"},
#line 918 "j9java8packages.gperf"
      {"jdk/nashorn/internal/ir/debug"},
#line 830 "j9java8packages.gperf"
      {"javax/security/auth/callback"},
#line 140 "j9java8packages.gperf"
      {"com/ibm/jvm/dtfjview/commands/helpers"},
#line 977 "j9java8packages.gperf"
      {"org/w3c/dom/ranges"},
#line 146 "j9java8packages.gperf"
      {"com/ibm/jvm/dtfjview/heapdump/classic"},
#line 181 "j9java8packages.gperf"
      {"com/ibm/rmi/corba/DynamicAny"},
#line 114 "j9java8packages.gperf"
      {"com/ibm/dtfj/javacore/parser/j9/section/memory"},
#line 138 "j9java8packages.gperf"
      {"com/ibm/jvm/dtfjview"},
#line 108 "j9java8packages.gperf"
      {"com/ibm/dtfj/javacore/parser/framework/tag"},
#line 981 "j9java8packages.gperf"
      {"org/w3c/dom/xpath"},
#line 306 "j9java8packages.gperf"
      {"com/sun/imageio/plugins/png"},
#line 978 "j9java8packages.gperf"
      {"org/w3c/dom/stylesheets"},
#line 944 "j9java8packages.gperf"
      {"org/omg/CORBA/DynAnyPackage"},
#line 706 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/wsdl/writer/document/http"},
#line 145 "j9java8packages.gperf"
      {"com/ibm/jvm/dtfjview/heapdump"},
#line 1055 "j9java8packages.gperf"
      {"sun/net/www/protocol/http/logging"},
#line 305 "j9java8packages.gperf"
      {"com/sun/imageio/plugins/jpeg"},
#line 312 "j9java8packages.gperf"
      {"com/sun/istack/internal/logging"},
#line 119 "j9java8packages.gperf"
      {"com/ibm/dtfj/javacore/parser/j9/section/thread"},
#line 444 "j9java8packages.gperf"
      {"com/sun/tools/example/debug/tty"},
#line 414 "j9java8packages.gperf"
      {"com/sun/security/auth/callback"},
#line 1057 "j9java8packages.gperf"
      {"sun/net/www/protocol/http/spnego"},
#line 655 "j9java8packages.gperf"
      {"com/sun/xml/internal/ws/db/glassfish"},
#line 980 "j9java8packages.gperf"
      {"org/w3c/dom/views"},
#line 304 "j9java8packages.gperf"
      {"com/sun/imageio/plugins/gif"},
#line 956 "j9java8packages.gperf"
      {"org/omg/DynamicAny/DynAnyPackage"},
#line 147 "j9java8packages.gperf"
      {"com/ibm/jvm/dtfjview/heapdump/portable"},
#line 1109 "j9java8packages.gperf"
      {"sun/swing/plaf/windows"},
#line 955 "j9java8packages.gperf"
      {"org/omg/DynamicAny/DynAnyFactoryPackage"},
#line 307 "j9java8packages.gperf"
      {"com/sun/imageio/plugins/wbmp"},
#line 946 "j9java8packages.gperf"
      {"org/omg/CORBA/TypeCodePackage"},
#line 221 "j9java8packages.gperf"
      {"com/oracle/xmlns/internal/webservices/jaxws_databinding"},
#line 362 "j9java8packages.gperf"
      {"com/sun/net/ssl/internal/www/protocol/https"}
    };

  static const short lookup[] =
    {
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
         0,   -1,    1,   -1,   -1,    2,   -1,   -1,
        -1,   -1,    3,    4,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,    5,   -1,   -1,
         6,    7,    8,   -1,    9,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   10,   -1,   -1,
        -1,   -1,   -1,   11,   -1,   12,   -1,   -1,
        -1,   -1,   13,   14,   -1,   15,   -1,   16,
        -1,   -1,   -1,   -1,   -1,   17,   -1,   18,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   19,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   20,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   21,   -1,   -1,   -1,   -1,
        -1,   -1,   22,   -1,   -1,   -1,   -1,   23,
        -1,   -1,   -1,   24,   25,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   26,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   27,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   28,   -1,   -1,   -1,   29,   -1,
        -1,   -1,   30,   -1,   -1,   -1,   -1,   31,
        -1,   32,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   33,   -1,
        -1,   34,   35,   -1,   -1,   -1,   -1,   -1,
        36,   37,   -1,   -1,   -1,   38,   39,   -1,
        -1,   -1,   -1,   -1,   -1,   40,   -1,   -1,
        41,   -1,   -1,   42,   -1,   -1,   -1,   43,
        -1,   44,   -1,   -1,   45,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   46,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   47,   -1,   -1,   -1,
        -1,   48,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   49,   -1,   -1,   50,   51,   -1,
        -1,   -1,   -1,   -1,   52,   53,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   54,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        55,   -1,   -1,   56,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   57,   -1,   58,   -1,   -1,
        59,   -1,   -1,   -1,   -1,   60,   -1,   61,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   62,
        -1,   -1,   -1,   -1,   63,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   64,   -1,   -1,
        -1,   65,   -1,   66,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   67,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   68,   69,   -1,
        -1,   -1,   70,   -1,   -1,   -1,   -1,   71,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   72,   -1,   -1,   -1,   -1,
        -1,   -1,   73,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   74,
        -1,   -1,   75,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   76,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   77,   -1,
        -1,   -1,   -1,   78,   -1,   -1,   -1,   -1,
        79,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   80,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   81,   -1,   -1,   -1,   -1,   -1,
        -1,   82,   -1,   -1,   -1,   -1,   -1,   83,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   84,
        -1,   -1,   -1,   85,   -1,   -1,   -1,   -1,
        -1,   -1,   86,   -1,   -1,   87,   -1,   -1,
        -1,   -1,   -1,   -1,   88,   -1,   89,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        90,   -1,   -1,   91,   -1,   92,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   93,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   94,   -1,   -1,   -1,   95,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   96,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   97,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   98,   -1,   99,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  100,
       101,  102,   -1,   -1,   -1,   -1,  103,   -1,
       104,  105,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  106,   -1,   -1,   -1,  107,
        -1,  108,   -1,   -1,   -1,   -1,  109,   -1,
        -1,  110,  111,  112,  113,   -1,   -1,  114,
       115,  116,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  117,   -1,
        -1,  118,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  119,
        -1,   -1,   -1,   -1,   -1,  120,   -1,   -1,
       121,  122,   -1,   -1,  123,   -1,   -1,   -1,
        -1,  124,  125,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  126,   -1,   -1,
       127,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       128,  129,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  130,   -1,  131,  132,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  133,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  134,   -1,
        -1,   -1,   -1,   -1,  135,   -1,   -1,   -1,
        -1,  136,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  137,  138,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  139,   -1,  140,
        -1,   -1,   -1,   -1,   -1,   -1,  141,   -1,
        -1,   -1,  142,  143,   -1,   -1,  144,   -1,
       145,   -1,   -1,   -1,   -1,  146,   -1,  147,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  148,
       149,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  150,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  151,  152,   -1,  153,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  154,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  155,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  156,
        -1,   -1,   -1,   -1,  157,   -1,   -1,   -1,
        -1,  158,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  159,   -1,   -1,   -1,
        -1,  160,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  161,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  162,
       163,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  164,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  165,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       166,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  167,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       168,   -1,   -1,   -1,   -1,  169,   -1,  170,
        -1,  171,   -1,   -1,  172,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  173,   -1,   -1,
        -1,   -1,  174,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  175,   -1,   -1,   -1,   -1,  176,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  177,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  178,   -1,   -1,   -1,
        -1,  179,  180,   -1,   -1,  181,   -1,  182,
        -1,   -1,  183,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  184,   -1,  185,
        -1,  186,   -1,   -1,   -1,   -1,  187,   -1,
        -1,  188,   -1,   -1,   -1,   -1,  189,  190,
        -1,   -1,   -1,  191,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  192,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  193,   -1,
        -1,  194,   -1,  195,   -1,   -1,  196,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  197,   -1,
        -1,   -1,  198,   -1,   -1,  199,  200,  201,
        -1,   -1,   -1,   -1,  202,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       203,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  204,   -1,
        -1,   -1,  205,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  206,   -1,  207,   -1,  208,
       209,   -1,   -1,   -1,  210,   -1,   -1,   -1,
        -1,   -1,   -1,  211,   -1,   -1,  212,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  213,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  214,   -1,
       215,   -1,  216,   -1,  217,   -1,   -1,   -1,
        -1,   -1,  218,   -1,   -1,   -1,   -1,  219,
        -1,  220,   -1,   -1,   -1,   -1,   -1,  221,
        -1,  222,   -1,   -1,  223,   -1,  224,   -1,
       225,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  226,   -1,   -1,   -1,   -1,
       227,  228,  229,   -1,   -1,   -1,  230,   -1,
       231,  232,   -1,   -1,   -1,  233,  234,   -1,
        -1,   -1,   -1,  235,   -1,   -1,   -1,   -1,
        -1,  236,   -1,  237,   -1,   -1,   -1,  238,
        -1,   -1,   -1,  239,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  240,   -1,
        -1,   -1,   -1,   -1,   -1,  241,   -1,   -1,
        -1,  242,   -1,   -1,   -1,  243,   -1,   -1,
        -1,   -1,   -1,  244,  245,   -1,   -1,   -1,
        -1,   -1,  246,   -1,   -1,   -1,  247,  248,
        -1,   -1,   -1,  249,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  250,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  251,  252,   -1,   -1,
        -1,   -1,  253,   -1,  254,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  255,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  256,   -1,  257,  258,   -1,   -1,  259,
        -1,   -1,  260,   -1,   -1,   -1,  261,  262,
       263,  264,   -1,   -1,  265,   -1,   -1,   -1,
       266,   -1,   -1,   -1,   -1,   -1,   -1,  267,
       268,   -1,   -1,   -1,   -1,   -1,  269,   -1,
       270,  271,   -1,   -1,   -1,  272,   -1,   -1,
        -1,   -1,   -1,  273,   -1,   -1,   -1,  274,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  275,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  276,   -1,   -1,  277,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  278,
       279,   -1,   -1,   -1,   -1,  280,  281,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       282,   -1,   -1,   -1,  283,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  284,   -1,  285,   -1,   -1,  286,   -1,
        -1,   -1,  287,  288,   -1,   -1,   -1,   -1,
        -1,   -1,  289,   -1,   -1,  290,  291,  292,
        -1,   -1,   -1,   -1,   -1,  293,  294,  295,
        -1,   -1,   -1,   -1,  296,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  297,   -1,   -1,   -1,
       298,   -1,   -1,  299,   -1,  300,   -1,   -1,
        -1,   -1,   -1,  301,   -1,   -1,   -1,   -1,
        -1,   -1,  302,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  303,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  304,   -1,   -1,
        -1,   -1,   -1,  305,  306,  307,   -1,   -1,
        -1,   -1,  308,   -1,   -1,   -1,  309,   -1,
       310,   -1,   -1,  311,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  312,  313,  314,   -1,  315,
        -1,   -1,  316,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  317,  318,   -1,
        -1,  319,   -1,   -1,  320,   -1,   -1,   -1,
        -1,   -1,  321,   -1,   -1,   -1,   -1,   -1,
       322,   -1,  323,   -1,   -1,   -1,   -1,   -1,
       324,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  325,   -1,   -1,   -1,   -1,   -1,   -1,
       326,   -1,   -1,   -1,  327,  328,  329,   -1,
        -1,  330,   -1,   -1,   -1,   -1,   -1,  331,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  332,
       333,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  334,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  335,   -1,   -1,  336,  337,   -1,  338,
       339,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  340,   -1,   -1,   -1,  341,
        -1,   -1,  342,   -1,   -1,  343,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  344,  345,   -1,   -1,  346,   -1,  347,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  348,  349,   -1,  350,   -1,
        -1,   -1,   -1,  351,   -1,  352,   -1,  353,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  354,   -1,  355,   -1,   -1,  356,
       357,   -1,  358,   -1,  359,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  360,   -1,
        -1,   -1,   -1,   -1,   -1,  361,   -1,   -1,
       362,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       363,  364,   -1,   -1,   -1,   -1,   -1,   -1,
       365,   -1,   -1,   -1,  366,  367,   -1,   -1,
       368,   -1,  369,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  370,   -1,
        -1,  371,   -1,   -1,  372,   -1,   -1,   -1,
       373,   -1,  374,   -1,   -1,   -1,  375,   -1,
       376,  377,   -1,  378,   -1,   -1,   -1,   -1,
       379,   -1,  380,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  381,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  382,   -1,   -1,
       383,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       384,   -1,  385,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  386,  387,   -1,   -1,  388,   -1,
       389,   -1,  390,  391,  392,   -1,   -1,   -1,
        -1,   -1,  393,   -1,   -1,  394,   -1,   -1,
       395,   -1,   -1,  396,   -1,  397,   -1,   -1,
        -1,  398,  399,   -1,  400,   -1,   -1,   -1,
        -1,   -1,   -1,  401,  402,  403,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  404,  405,  406,
       407,   -1,   -1,   -1,  408,   -1,   -1,  409,
       410,   -1,   -1,   -1,   -1,   -1,  411,  412,
        -1,   -1,   -1,   -1,  413,   -1,   -1,   -1,
        -1,   -1,  414,   -1,   -1,   -1,   -1,  415,
        -1,  416,   -1,   -1,   -1,  417,  418,   -1,
        -1,   -1,  419,  420,   -1,  421,   -1,  422,
        -1,   -1,   -1,   -1,  423,   -1,   -1,   -1,
        -1,  424,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  425,  426,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  427,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       428,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       429,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  430,   -1,   -1,   -1,   -1,  431,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  432,   -1,   -1,
        -1,  433,   -1,   -1,   -1,   -1,  434,   -1,
       435,  436,  437,   -1,   -1,  438,   -1,   -1,
        -1,  439,   -1,   -1,  440,   -1,  441,   -1,
       442,  443,   -1,  444,   -1,   -1,  445,   -1,
        -1,   -1,  446,  447,   -1,   -1,   -1,   -1,
       448,   -1,   -1,  449,   -1,  450,  451,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  452,   -1,   -1,   -1,
        -1,   -1,  453,   -1,   -1,  454,   -1,   -1,
       455,  456,   -1,   -1,   -1,   -1,   -1,  457,
       458,   -1,   -1,  459,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  460,
        -1,   -1,   -1,  461,  462,   -1,   -1,   -1,
       463,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       464,   -1,   -1,   -1,   -1,   -1,   -1,  465,
       466,   -1,   -1,  467,   -1,   -1,   -1,   -1,
        -1,   -1,  468,  469,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       470,   -1,  471,   -1,   -1,  472,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  473,   -1,   -1,  474,   -1,   -1,
        -1,  475,   -1,  476,   -1,  477,   -1,   -1,
       478,   -1,   -1,  479,  480,   -1,   -1,   -1,
       481,   -1,   -1,  482,   -1,  483,   -1,   -1,
        -1,   -1,   -1,  484,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  485,   -1,
        -1,   -1,   -1,  486,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  487,   -1,   -1,
        -1,  488,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  489,   -1,   -1,  490,   -1,  491,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  492,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  493,   -1,
        -1,   -1,  494,   -1,  495,   -1,   -1,   -1,
       496,   -1,   -1,  497,   -1,   -1,  498,   -1,
        -1,   -1,   -1,  499,   -1,   -1,   -1,   -1,
        -1,  500,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  501,   -1,
        -1,  502,  503,   -1,   -1,   -1,   -1,  504,
        -1,  505,   -1,   -1,  506,   -1,   -1,   -1,
       507,   -1,   -1,   -1,  508,   -1,   -1,   -1,
        -1,   -1,   -1,  509,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  510,  511,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  512,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  513,
        -1,   -1,  514,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  515,  516,
        -1,   -1,   -1,   -1,   -1,   -1,  517,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  518,   -1,   -1,   -1,   -1,
        -1,  519,   -1,  520,  521,  522,  523,  524,
        -1,   -1,  525,   -1,   -1,   -1,  526,   -1,
       527,   -1,   -1,  528,   -1,   -1,   -1,   -1,
       529,  530,  531,   -1,   -1,   -1,   -1,  532,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  533,
        -1,  534,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  535,   -1,  536,   -1,   -1,
        -1,   -1,   -1,  537,   -1,   -1,  538,   -1,
        -1,   -1,   -1,  539,  540,   -1,   -1,   -1,
        -1,   -1,   -1,  541,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  542,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  543,   -1,   -1,   -1,
        -1,  544,  545,   -1,   -1,   -1,   -1,   -1,
       546,  547,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  548,   -1,
        -1,   -1,  549,   -1,   -1,   -1,   -1,  550,
       551,   -1,  552,  553,  554,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       555,   -1,   -1,   -1,  556,   -1,  557,   -1,
        -1,  558,   -1,   -1,  559,   -1,   -1,   -1,
       560,   -1,   -1,   -1,   -1,   -1,  561,  562,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  563,   -1,   -1,  564,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  565,   -1,   -1,   -1,   -1,   -1,   -1,
       566,   -1,  567,   -1,  568,   -1,   -1,   -1,
        -1,  569,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  570,  571,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  572,
       573,  574,   -1,   -1,   -1,   -1,  575,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  576,   -1,   -1,   -1,
        -1,  577,  578,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  579,   -1,   -1,
        -1,  580,  581,   -1,  582,   -1,  583,   -1,
        -1,   -1,   -1,   -1,  584,  585,   -1,   -1,
        -1,   -1,  586,  587,   -1,  588,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  589,   -1,   -1,
       590,   -1,   -1,   -1,  591,  592,   -1,   -1,
       593,   -1,  594,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  595,   -1,   -1,   -1,  596,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  597,  598,
        -1,  599,   -1,   -1,   -1,  600,   -1,   -1,
       601,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       602,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  603,   -1,   -1,   -1,   -1,   -1,
       604,   -1,  605,   -1,   -1,   -1,   -1,  606,
        -1,  607,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  608,   -1,   -1,   -1,  609,   -1,
        -1,   -1,  610,   -1,  611,   -1,   -1,  612,
        -1,   -1,  613,   -1,   -1,   -1,  614,  615,
        -1,   -1,   -1,   -1,   -1,   -1,  616,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  617,  618,   -1,   -1,  619,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       620,  621,   -1,   -1,   -1,   -1,   -1,  622,
        -1,   -1,  623,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  624,  625,   -1,   -1,   -1,
       626,  627,  628,   -1,   -1,   -1,  629,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       630,   -1,   -1,   -1,   -1,   -1,  631,   -1,
       632,  633,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  634,   -1,   -1,   -1,   -1,  635,  636,
        -1,   -1,   -1,   -1,   -1,   -1,  637,   -1,
        -1,   -1,   -1,  638,   -1,   -1,   -1,   -1,
        -1,   -1,  639,  640,  641,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  642,   -1,   -1,  643,   -1,   -1,
       644,   -1,   -1,  645,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  646,   -1,   -1,  647,   -1,
        -1,   -1,   -1,   -1,   -1,  648,  649,   -1,
        -1,  650,  651,   -1,   -1,  652,  653,  654,
        -1,   -1,  655,  656,   -1,   -1,   -1,  657,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  658,   -1,
        -1,  659,   -1,   -1,   -1,  660,  661,   -1,
        -1,   -1,   -1,  662,  663,   -1,  664,  665,
       666,   -1,  667,   -1,   -1,   -1,  668,  669,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  670,  671,  672,   -1,  673,   -1,   -1,
        -1,   -1,  674,   -1,   -1,   -1,   -1,   -1,
       675,   -1,   -1,   -1,  676,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  677,   -1,   -1,   -1,
        -1,   -1,  678,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  679,  680,  681,   -1,   -1,
        -1,   -1,  682,  683,  684,   -1,   -1,  685,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       686,  687,   -1,   -1,   -1,  688,   -1,  689,
       690,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  691,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  692,
        -1,   -1,  693,   -1,   -1,   -1,   -1,   -1,
       694,   -1,   -1,   -1,  695,  696,  697,   -1,
        -1,  698,  699,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       700,  701,  702,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       703,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  704,   -1,   -1,
        -1,   -1,  705,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  706,   -1,   -1,
        -1,   -1,   -1,   -1,  707,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  708,   -1,   -1,   -1,  709,   -1,
        -1,  710,   -1,  711,   -1,  712,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  713,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  714,   -1,   -1,  715,
       716,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  717,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  718,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  719,  720,
        -1,  721,   -1,   -1,   -1,   -1,   -1,  722,
        -1,   -1,   -1,  723,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  724,   -1,   -1,   -1,
        -1,   -1,  725,   -1,   -1,   -1,  726,  727,
        -1,   -1,   -1,   -1,   -1,   -1,  728,   -1,
       729,  730,  731,  732,   -1,  733,   -1,  734,
       735,   -1,   -1,   -1,  736,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  737,   -1,  738,
        -1,   -1,  739,   -1,  740,   -1,   -1,  741,
        -1,   -1,   -1,   -1,  742,   -1,   -1,   -1,
       743,   -1,   -1,  744,   -1,  745,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       746,   -1,   -1,  747,   -1,  748,   -1,   -1,
        -1,  749,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  750,   -1,   -1,   -1,  751,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  752,
        -1,   -1,   -1,   -1,   -1,   -1,  753,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  754,   -1,   -1,   -1,   -1,
        -1,  755,   -1,   -1,  756,  757,  758,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  759,   -1,  760,   -1,   -1,   -1,   -1,
        -1,  761,  762,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  763,  764,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       765,   -1,   -1,   -1,  766,   -1,   -1,   -1,
        -1,   -1,   -1,  767,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  768,   -1,   -1,   -1,   -1,
       769,   -1,  770,  771,   -1,  772,   -1,   -1,
       773,   -1,   -1,   -1,   -1,   -1,   -1,  774,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  775,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  776,   -1,   -1,
        -1,   -1,   -1,   -1,  777,   -1,   -1,  778,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  779,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  780,   -1,   -1,  781,
        -1,  782,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  783,   -1,
       784,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  785,   -1,  786,   -1,   -1,   -1,
        -1,  787,   -1,   -1,   -1,   -1,  788,   -1,
        -1,  789,   -1,  790,   -1,   -1,   -1,   -1,
       791,   -1,   -1,   -1,   -1,  792,   -1,  793,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  794,   -1,  795,   -1,
        -1,   -1,  796,  797,   -1,   -1,   -1,  798,
        -1,   -1,  799,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  800,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  801,  802,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  803,  804,   -1,   -1,   -1,  805,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       806,  807,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  808,   -1,   -1,  809,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  810,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  811,  812,   -1,  813,   -1,  814,
       815,  816,  817,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  818,   -1,
        -1,   -1,   -1,   -1,  819,   -1,   -1,  820,
        -1,   -1,   -1,   -1,  821,   -1,   -1,   -1,
        -1,   -1,  822,   -1,   -1,   -1,   -1,  823,
        -1,   -1,   -1,   -1,   -1,   -1,  824,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       825,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  826,   -1,   -1,  827,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  828,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       829,  830,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  831,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  832,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  833,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       834,   -1,   -1,   -1,   -1,   -1,   -1,  835,
       836,   -1,   -1,  837,   -1,   -1,   -1,   -1,
        -1,  838,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  839,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  840,   -1,
       841,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  842,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  843,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  844,  845,   -1,
        -1,   -1,  846,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  847,   -1,
        -1,  848,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  849,   -1,   -1,   -1,   -1,   -1,
       850,   -1,   -1,   -1,   -1,  851,   -1,  852,
        -1,   -1,   -1,   -1,  853,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  854,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  855,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  856,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  857,   -1,
       858,   -1,  859,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  860,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  861,   -1,   -1,   -1,  862,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  863,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  864,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  865,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  866,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  867,   -1,  868,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  869,   -1,   -1,   -1,   -1,  870,  871,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  872,   -1,  873,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  874,   -1,   -1,  875,   -1,  876,  877,
        -1,  878,   -1,   -1,   -1,  879,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  880,
        -1,  881,   -1,   -1,  882,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  883,   -1,   -1,   -1,
        -1,   -1,  884,   -1,   -1,   -1,   -1,  885,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  886,
        -1,   -1,   -1,   -1,   -1,  887,  888,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       889,   -1,   -1,   -1,   -1,   -1,  890,  891,
        -1,   -1,  892,   -1,   -1,   -1,   -1,   -1,
        -1,  893,  894,  895,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  896,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  897,
       898,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  899,   -1,  900,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  901,   -1,  902,  903,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  904,   -1,   -1,  905,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  906,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  907,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       908,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  909,   -1,  910,   -1,   -1,   -1,   -1,
        -1,  911,  912,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  913,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       914,  915,   -1,  916,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  917,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  918,  919,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  920,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       921,   -1,   -1,   -1,  922,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  923,   -1,   -1,   -1,   -1,   -1,  924,
        -1,   -1,  925,   -1,   -1,   -1,  926,   -1,
        -1,   -1,   -1,   -1,  927,   -1,   -1,  928,
        -1,   -1,   -1,   -1,  929,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  930,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  931,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  932,
        -1,   -1,   -1,   -1,   -1,  933,  934,   -1,
       935,   -1,  936,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  937,   -1,  938,
        -1,  939,   -1,   -1,   -1,  940,  941,  942,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  943,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  944,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  945,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  946,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       947,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  948,
        -1,   -1,   -1,   -1,  949,   -1,   -1,   -1,
       950,   -1,   -1,  951,  952,   -1,   -1,   -1,
       953,   -1,   -1,   -1,   -1,   -1,   -1,  954,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  955,   -1,  956,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       957,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  958,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       959,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,  960,   -1,  961,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  962,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  963,   -1,  964,   -1,   -1,
        -1,  965,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  966,  967,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  968,   -1,   -1,   -1,   -1,   -1,
       969,   -1,   -1,   -1,   -1,   -1,  970,  971,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  972,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,  973,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  974,  975,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  976,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,  977,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,  978,
       979,   -1,   -1,   -1,   -1,  980,   -1,  981,
        -1,   -1,   -1,  982,   -1,   -1,   -1,  983,
       984,   -1,  985,   -1,  986,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,  987,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       988,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,  989,   -1,  990,   -1,   -1,
        -1,   -1,  991,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       992,   -1,  993,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  994,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  995,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,  996,   -1,
        -1,   -1,   -1,   -1,  997,   -1,   -1,   -1,
        -1,  998,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
       999,   -1, 1000,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
      1001,   -1,   -1,   -1,   -1,   -1, 1002,   -1,
        -1,   -1,   -1,   -1,   -1, 1003,   -1, 1004,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1, 1005,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1, 1006,
        -1,   -1,   -1,   -1, 1007,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
      1008,   -1,   -1,   -1, 1009,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1, 1010, 1011, 1012,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1, 1013,   -1,
        -1,   -1,   -1,   -1, 1014,   -1,   -1,   -1,
        -1,   -1,   -1,   -1, 1015,   -1,   -1,   -1,
        -1, 1016,   -1,   -1, 1017,   -1, 1018,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1, 1019,   -1,
      1020,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1, 1021,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1, 1022,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
      1023,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1, 1024,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1, 1025, 1026,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1, 1027,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1, 1028,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1, 1029,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1, 1030,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1, 1031,
      1032,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1, 1033,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1, 1034,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1, 1035,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1, 1036,   -1,
        -1,   -1, 1037,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1, 1038,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1, 1039,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1, 1040,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1, 1041,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
      1042,   -1,   -1,   -1,   -1,   -1, 1043,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1, 1044,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
      1045,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1, 1046,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1, 1047,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1, 1048,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1, 1049,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1, 1050,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1, 1051,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
      1052,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1, 1053,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1, 1054,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1, 1055,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
      1056,   -1,   -1,   -1,   -1,   -1, 1057,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1, 1058,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1, 1059,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1, 1060,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1, 1061,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1, 1062,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1, 1063,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1, 1064,   -1,   -1,
        -1, 1065,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1, 1066,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1, 1067,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1, 1068,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1, 1069,   -1,   -1,   -1,   -1,   -1,   -1,
        -1, 1070,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1, 1071,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1, 1072,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1, 1073,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1, 1074,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1, 1075,   -1,   -1,
        -1,   -1,   -1,   -1, 1076,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1, 1077,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1, 1078,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1, 1079,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
      1080,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1, 1081,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1, 1082,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1, 1083,   -1,   -1,   -1,
      1084,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1, 1085,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1, 1086,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
      1087,   -1,   -1,   -1,   -1, 1088,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1, 1089,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1, 1090,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1, 1091,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1, 1092,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1, 1093,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1, 1094,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1, 1095,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1, 1096,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
      1097,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1, 1098,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1, 1099,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1, 1100,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
      1101,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
        -1,   -1,   -1,   -1,   -1,   -1, 1102
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = packageNameHash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
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
