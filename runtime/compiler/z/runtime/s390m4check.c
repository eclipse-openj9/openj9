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

/* This program takes as input the file *.m4 and *.s and generates
 warnings if certain syntax errors are detected in the assembly */
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#define LINE_LENGTH 71        /* Maximum number of bytes allowed on the line */

int main(int argc, char **argv)
   {
   FILE *in;
   int i, j, ascii, m4, charencountered, insidebracket, linecnt, errors;
   /* Make data buffer 1K - definitely big enough to read in all characters on 1 line,      */
   /* since fgets continues reading characters from same line if end of line is not reached. */
   char data[1024];     

   /* datalength stores length of characters on the line */
   int datalength;      

   ascii = 0;
   m4 = 0;
   linecnt = 1;
   charencountered = 0;
   insidebracket = 0;
   j = 0;
   errors = 0;

   data[0]='A';                                                             /* determine ascii or ebcdic */
   if (data[0] == 65)
      ascii = 1;

   for (i=0; i<20; i++)                                                     /* determine m4 file or s file */
      {
      data[i]=*(argv[1]+i);
      if (i > 0 && data[i-1] == '.')
         {
         if (data[i] == 'm')
            m4 = 1;
         break;
         }
      }

   if ((in = fopen(argv[1], "r")) == NULL)                                  /* ensure file is opened */
      {
      printf("Unable to open the file to be checked.\n");
      return 1;
      }
   else
      {
      printf("Checking %s...\n", argv[1]);
      }

   while (!feof(in))
      {

      fgets(data, sizeof(data), in);                                        /* read next line */
      
      datalength = strlen(data); 
      
      /* fgets put a new line character at end of line if there is space, so we need to subtract 1 from */
      /* datalength to get actual numbers of characters before comparing. */
      if (datalength-1 > LINE_LENGTH)
         {
         printf("ERROR: Maximum line length reached on line %d\n", linecnt);
         errors = 1;
         }
  
      for (i=0; i < datalength; i++)                                      /* check for invalid characters */ 
         {
         if ((!((data[i] >= 64 && data[i] <= 249) || data[i] == 13 || data[i] == 21 || data[i] == 0) && ascii == 0) ||  /* EBCDIC */
             (!((data[i] >= 31 && data[i] <= 127) || data[i] == 13 || data[i] == 10 || data[i] == 0) && ascii == 1))    /* ASCII  */
            {
            printf("ERROR: Invalid character found on line %d\n", linecnt);
            printf(data);
            errors = 1;
            break;
            }
         }

      if ((data[0] == 'Z' && data[1] == 'Z' && m4 == 1) ||
          (m4 == 0 && ((data[0] == '*' && data[1]  == '*' && ascii == 0) || (data[0] == '#' && data[1]  == '#' && ascii == 1))))     /* filter out the comments */
         {
            linecnt++;
            continue;
         }

      while (charencountered == 0)                                          /* check for commenting errors with regard to '#' and 'ZZ' */
         {
         if (data[j] != ' ')
            {
            if ((data[j] == '#' && data[j+1] != '#' && j == 0) ||
                (data[j] == 'Z' && data[j+1] == 'Z' && j != 0) ||
                (data[j] == '*' && data[j+1] == '*' && j != 0 && ascii == 0) ||
                (data[j] == '#' && data[j+1] == '#' && j != 0 && ascii == 1))
               { 
               printf("ERROR: Incorrect commenting on line %d\n", linecnt);
               printf(data);
               errors = 1;
               break;
               }
               charencountered = 1;
            }
         j++;
         }

      for (i=0; i < datalength; i++)
         {
         if (data[i] == '#' || (data[i] == 'Z' && data[i-1] == 'Z') || data[i] == '\n' || (data[i] == '*' && data[i-1] == '*' && ascii == 0))
            {                                                            /* do not evaluate comments */
            break;
            }
         else
            {                                                            /* check all other requirements  */
            if (data[i] == ',' && (data[i-1] == ' ' || data[i+1] == ' ' || i == 0))
               {                                                         /* comma errors */
               printf("ERROR: Whitespace detected adjacent to comma on line %d\n", linecnt);
               printf(data);
               errors = 1;
               break;
               }

            if (data[i] == '(')                                       /* set inside bracket flag */
               insidebracket = 1;

            if (data[i] == ')')                                       /* unset inside bracket flag */
               insidebracket = 0;

            if (insidebracket == 1 && data[i] == ' ')                 /* check for whitespace inside brackets */
               {
               printf("ERROR: Whitespace detected inside brackets on line %d\n", linecnt);
               printf(data);
               errors = 1;
               break;
               }

            if (data[i] == '(' && data[i-1] == ' ' && data[i-2] == ',')      /* check for whitespace before brackets */
               {
               printf("ERROR: Whitespace detected adjacent to bracket on line %d\n", linecnt);
               printf(data);
               errors = 1;
               break;
               }
            }
         }

      j=0;                                                                  /* reset all variables for next line */
      linecnt++;
      insidebracket=0;
      charencountered=0;
      }
   fclose(in);                                                              /* close the file */
   return errors;
   }

