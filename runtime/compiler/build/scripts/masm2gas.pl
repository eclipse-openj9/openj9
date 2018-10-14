#!/bin/perl

# Copyright (c) 2000, 2017 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception

# masm2gas.pl: translates Microsoft Assembler files to GNU Assembler format
# Options:
#   -Idir     Search for include files in "dir"
#   --64      Use 64-bit registers
#   -M        Generate a depend file
#   -o        Specify output directory for created files
#
# Output file names follow these rules (.x is any extension besides these two)
# foo.asm  -> GAS: (outdir)/foo.s           DEPEND: (outdir)/foo.d
# bar.ipp  -> GAS: (outdir)/bar.s           DEPEND: (outdir)/bar.d
# foobar.x -> GAS: (outdir)/foobar.x.s      DEPEND: (outdir)/foobar.x.d



use warnings;
use strict;
use bytes; # avoid using Unicode internally
use File::Spec::Functions "devnull";

die "usage: $0 [[-Idir1]* [-M]] file1.asm file2.asm ...\n" unless @ARGV;

$|=1;
my $null = &devnull;
my $gasVersion = (`as --version 2>$null`)[0];
if ($gasVersion)
   {
   chomp $gasVersion;
   $gasVersion =~ s/^GNU assembler ([\d\.]+).*$/$1/;
   print "using GNU assembler $gasVersion\n";
   }
else
   {
   $gasVersion = '';
   warn "cannot determine GNU assembler version\n";
   }

my %constants;       # keep a table of constants (.equ or .set) for hack value
my %registerMap;     # mapping of abstract register names to actual register names
my $offsetFlatExpr;  # "offset", or "offset\s+flat:"

my %asmKeywords =
   (
   ".486"     => { gasEquiv => ".arch i486",       args => 0 },
   ".486p"    => { gasEquiv => ".arch i486",       args => 0 },
   ".586"     => { gasEquiv => ".arch pentium",    args => 0 },
   ".586p"    => { gasEquiv => ".arch pentium",    args => 0 },
   ".686"     => { gasEquiv => ".arch pentiumpro", args => 0 },
   ".686p"    => { gasEquiv => ".arch pentiumpro", args => 0 },
   ".k3d"     => { gasEquiv => ".arch k6",         args => 0 },
   ".mmx"     => { gasEquiv => ".arch pentiumpro", args => 0 },
   ".xmm"     => { gasEquiv => ".arch pentium4",   args => 0 },
   "align"    => { gasEquiv => ".align",           args => 1 },
#   "endm"     => { gasEquiv => ".endm",            args => 0 },
   "db"       => { gasEquiv => ".byte",            args => -1 },
   "dd"       => { gasEquiv => ".int",             args => -1 }, # '.word' is architecture-dependent
   "dq"       => { gasEquiv => ".quad",            args => -1 },
   "dt"       => { gasEquiv => ".tfloat",          args => -1 },
   "dw"       => { gasEquiv => ".short",           args => -1 },
   "if"       => { gasEquiv => ".if",              args => -1 },
   "ifdef"    => { gasEquiv => ".ifdef",           args => 1 },
   "ifndef"   => { gasEquiv => ".ifndef",          args => 1 },
   "else"     => { gasEquiv => ".else",            args => 0 },
   "endif"    => { gasEquiv => ".endif",           args => 0 },
   "xmmmovsd" => { gasEquiv => "movsd",            args => -1},
   );

my @ignoredKeywords =
   (
   "assume",
   "end",
   "endp",
   "ends",
   "option",
   "textequ",
   );

my @labelMnemonics =
   (
   "ja",
   "jae",
   "jb",
   "jbe",
   "jc",
   "jcxz",
   "jecxz",
   "je",
   "jg",
   "jge",
   "jl",
   "jle",
   "jna",
   "jnae",
   "jnb",
   "jnbe",
   "jnc",
   "jne",
   "jng",
   "jnge",
   "jnl",
   "jnle",
   "jno",
   "jnp",
   "jns",
   "jnz",
   "jo",
   "jp",
   "jpe",
   "jpo",
   "js",
   "jz",
   "jmp",
   "org",
   );

my @searchPath = grep  /^-I.*/, @ARGV; map { s/^-I// } @searchPath;
my @masmFiles  = grep !/^-I.*|^--[0-9]+|^-M|^-o.*/, @ARGV;
my @registerSize = grep /^--[0-9]+/, @ARGV;
my @makeDepend = grep /^-M/, @ARGV;
my @outFiles = grep /^-o.*/, @ARGV;
my $depends = undef;
my $outFile = "";
my $outDir = "";

if (@registerSize && pop @registerSize eq "--64")
   {
   %registerMap =
      (
      "_rax" => "rax",
      "_rbx" => "rbx",
      "_rcx" => "rcx",
      "_rdx" => "rdx",
      "_rsi" => "rsi",
      "_rdi" => "rdi",
      "_rsp" => "rsp",
      "_rbp" => "rbp",
      "_rip" => "rip +",
      );

   $offsetFlatExpr = 'offset';

   print "Mapping 64-bit registers.\n";
   }
else
   {
   %registerMap =
      (
      "_rax" => "eax",
      "_rbx" => "ebx",
      "_rcx" => "ecx",
      "_rdx" => "edx",
      "_rsi" => "esi",
      "_rdi" => "edi",
      "_rsp" => "esp",
      "_rbp" => "ebp",
      "_rip" => "",
      );

   $offsetFlatExpr = 'offset\s+flat:';

   print "Mapping 32-bit registers.\n";
   }

if (@makeDepend && pop @makeDepend eq "-M") {
    $depends = 1;
}

if (@outFiles) {
    $outFile =  join('', @outFiles);
    $outFile =~ s/^-o *//;
    if (-d $outFile)
       {
       $outDir = $outFile;
       }
}

for my $masmFile (@masmFiles)
   {
   if (open my $in, $masmFile)
      {
      my $baseFile = $masmFile;
      if ($outDir =~ /./) {
         $baseFile =~ s/.*\/([^\/]+)/$1/;
         $baseFile = $outDir . $baseFile;
      }

      my $gasFile = "";
      my $dependFile = "";
      
      if ($baseFile =~ /\.asm$/ || $baseFile =~ /\.spp$/)
         {
         $baseFile =~ s/\.[^.]*$//;
         $gasFile = $baseFile . ".s";
         $dependFile = $baseFile . ".d";
         }
      else
         {
         $gasFile = $baseFile . ".s";
         $dependFile = $baseFile . ".d";
         warn "unrecognized file extension in $masmFile; using $gasFile for output\n";
         }
         
      my $dep = undef;      
      open my $out, ">$gasFile" or die "could not write to $gasFile; aborting: $!\n";
      if ($depends) { open $dep, ">$dependFile" or die "could not write to $dependFile; aborting: $!\n"; }

      print $out ".intel_syntax noprefix\n";

      # reset per-file states
      map { delete $constants{$_} } keys %constants;

      &processFile($in, $out, $gasFile, $dep);

      close $dep if $dep;
      close $in;
      close $out;
      }
   else
      {
      warn "cannot open $masmFile: $!\n";
      }
   }

sub processFile
   {
   my ($in, $out, $gasFile, $dep) = @_;
   my ($line, $comment) = (undef, undef);
   my ($macroDepth) = 0;

   my @macroLocals;

   while (<$in>)
      {
      chomp;

      # strip comments from the line and save a copy
      $comment = s/\;+(.*)$// ? $1 : "";
      $line    = $_;

      # transform hex numbers
      $line =~ s/(\W)([0-9][[:xdigit:]]*)h\b/${1}0x$2/gi;

      if ($gasVersion lt '2.15.94')
         {
         # gas <2.15.94 uses "xword ptr" in lieu of "oword ptr" to refer to 16-byte storage
         $line =~ s/oword\s+ptr\s*\[/xword ptr \[/i;
	 }

      # masm macro parameters can be prefixed by &, suffixed by &, both, or
      # neither (not translated), while gas macro parameters must be prefixed by \.
      $line =~ s/\&(\w+)\&?/\\$1/g;
      $line =~ s/\b(\w+)\&/\\$1/g;                                    

      # parse "label keyword [arg1...argN]"
      if ($line =~ /^\s*(\S+)\s+(\S+)(.*)$/i)
         {
         my ($label, $keyword, $args) = ($1, lc $2, defined $3 ? $3 : "");

         # Map abstract register names to actual register names.
         {
         my $oldLine = $line;
         map { $line =~ s/\b$_\b/$registerMap{$_}/g } keys %registerMap;
         redo if $line ne $oldLine;
         }

         # skip masm keywords that have no gas-equivalents
         if (grep { $keyword eq $_ } @ignoredKeywords)
            {
            $line = "";
            next;
            }
         # translate segments into sections
         elsif ($keyword eq 'segment')
            {
            $line = "";
            if ($args =~ /['"](\w+)['"]\s*$/)
               {
               my $segmentClass = $1;
               $line = '.data' if $segmentClass =~ /DATA/i;
               $line = '.text' if $segmentClass =~ /CODE/i;
               }
            next;
            }
         # transform macro declarations
         elsif ($keyword eq "macro")
            { 
            $line = ".macro $label";
            for my $parm (split /,\s*/, $args)
               {
               if ($parm =~ /(\S+):=(\S+)/)
                  {
                  $line .= " $1=$2,"
                  }
               elsif ($parm =~ /([^\s\:]+)(:req)?/i)
                  {
                  $line .= " $1,";
                  }
               elsif ($parm !~ /$/)
                  {
                  # Don't warn on macros with no parameters.
                  #
                  warn "cannot parse '$parm' in macro $label\n";
                  }
               }
            $line =~ s/,$//;
            $macroDepth++;
            next;
            }
         # transform PROCs into labels, demangling at the same time
         elsif ($keyword eq "proc")
            {
            $line = "$label:";
            next;
            }
         # transform EQUs
         elsif ($keyword eq "equ")
            {
            $line = ".equ $label, $args";
            $constants{$label} = $args;
            next;
            }
         # transform '=' into '.set'
         elsif ($keyword eq "=")
            {
            $line = ".set $label, $args";
            $constants{$label} = $args;
            next;
            }
         # do not process TEXTEQUs
#         elsif ($keyword eq "textequ")
#            {
#            $line = "";
#            next;
#            }
         }


      # parse "keyword [arg1...argN]"
      if ($line =~ /^\s*(\S+)\s*(.*)$/)
         {
         my ($ucKeyword, $keyword, $args) = ($1, lc $1, defined $2 ? $2 : "");

         # trim surrounding spaces
         $args = "" if $args =~ /^\s*$/;
         $args =~ s/^\s*(\S.*\S)\s*$/$1/;

         # skip masm keywords that have no gas-equivalents
         if (grep { $keyword eq $_ } @ignoredKeywords)
            {
            $line = "";
            next;
            }
         # translate DUP() into .fill
         elsif ($args =~ /(\S+)\s+DUP\s*\((.*)\)/i)
            {
            my ($repeat, $value) = ($1, $2);
            my %sizes = (db=>1, dw=>2, dd=>4, dq=>8, dt=>10);
            die "error: DUP() is only supported in data declarations, e.g. DB, DD, etc.\n"
               unless exists $sizes{$keyword};
            $line = ".fill $repeat, $sizes{$keyword}, $value";
            }
         elsif (grep { $keyword eq $_ } @labelMnemonics)
            {
            if ($macroDepth > 0)
               {
               for (@{$macroLocals[$macroDepth]})
                  {
                  $line =~ s/($_)/$1\\\@/;
                  }
               }
            }
         # replace recognized masm keywords with their gas-equivalents
         elsif (exists $asmKeywords{$keyword})
            {
            $line = "$asmKeywords{$keyword}{gasEquiv}";

            if ($asmKeywords{$keyword}{args} < 0)
               {
               $line .= " " . $args;
               }
            elsif ($asmKeywords{$keyword}{args})
               {
               $line .= " " . join " ", (split /\s+/, $args)[0..$asmKeywords{$keyword}{args} - 1];
               }

            # the .arch directive was introduced in 2.11
            $line = "", next if $line =~ /\.arch/ and $gasVersion lt "2.11";

            # the .int, .long and .quad directives do not accept "offset flat:" arguments
            $line =~ s/$offsetFlatExpr\s*//gi if $line =~ /^\.(int|long|quad)/;
            

            # the .if directive needs &, |, ^ and !, instead of "and", "or", "xor" and "not"
            if ($line =~ /\.if\b/)
               {
               $line =~ s/\band\b/&/;
               $line =~ s/\bor\b/|/;
               $line =~ s/\bxor\b/^/;
               $line =~ s/\bnot\b/!/;
               }
            }
         elsif ($keyword eq "local")
            {
            my @locals  = split /\s*,\s*/, $args;

            for (@locals)
               {
               push @{$macroLocals[$macroDepth]}, $_;
               }

            $line = "";
            }
         elsif ($keyword eq "endm")
            {
            undef $macroLocals[$macroDepth];
            $macroDepth--;
            $line = ".endm";
            }
         elsif ($ucKeyword =~ /^\s*(\w+):/)
            {
            # Make the huge assumption that a label will appear on a distinct line.
            # Further assume that label case does not matter.  It does, but it is poor
            # programming practice.
            if ($macroDepth > 0)
               {
               my $label = $1;
               for (@{$macroLocals[$macroDepth]})
                  {
                  if ($_ eq $label)
                     {
                     $line = "$_\\\@:";
                     last;
                     }

                  }
               }
            }
         elsif ($keyword eq "include")
            {
            $args = &findIncludeFile($args);
            open my $includeFile, $args or die "cannot open $args: $!\n";
            print $dep "$gasFile: $args\n" if $dep;
            &processFile($includeFile, $out, $args);
            close $includeFile;
            $line = "";
            }
         elsif (grep { $keyword eq $_ } "public", "extern", "extrn", "externhelper")
            {
            # masm allows multiple symbols on one PUBLIC directive
            my @symbols  = split /\s*,\s*/, $args;
            my $gasEquiv = $keyword eq "public" ? ".global" : ".extern";

            # de-mangle each exported symbol and save a copy
            $line = "";
            for (@symbols)
               {
               next if /\\/; # a macro parameter is *not* a real exported symbol
               s/:.*$//; # masm allows :attributes to follow a symbol 
               }
            continue
               {
               $line .= "$gasEquiv $_\n";
               }

            chomp $line;
            }
         elsif ($keyword =~ /(fi?(ld|stp))/i)
            {
            # gas does not handle QWORD PTRs and TBYTE PTRs for FP loads/stores
            my $op = $1;
            if ($args =~ /qword\s+ptr\s*\[([^\[]*)\](.*)/i)
               {
               my ($memRef, $junk) = ($1, $2);
               my $size = $op =~ /^fi/ ? "q" : "l";
               die "cannot parse memory operand in $line"
                  unless $memRef = &morphMemoryOperand($memRef);
               $line = ".att_syntax\n${op}$size $memRef$junk\n.intel_syntax noprefix";
               }
            elsif ($args =~ /tbyte\s+ptr\s*\[([^\]]*)\](.*)/i)
               {
               my ($memRef, $junk) = ($1, $2);
               die "cannot parse memory operand in $line"
                  unless $memRef = &morphMemoryOperand($memRef);
               $line = ".att_syntax\n${op}t $memRef$junk\n.intel_syntax noprefix";
               }
            }
         }

      # Map abstract register names to actual register names.
#      {
#      my $oldLine = $line;
#      map { $line =~ s/\b$_\b/$registerMap{$_}/g } keys %registerMap;
#      redo if $line ne $oldLine;
#      }

      # HACK: substitute EQU'ed constants with their numeric equivalents
      #       (iteratively until convergence) to prevent old gas from
      #       treating them as memory references
      # HACK2: ...but don't do it in .if/.ifdef/.ifndef directives
      if ($gasVersion lt '2.16.91.0.5' && $line !~ /^\s*\.if/) # 2.16.91.0.5 is known to work - haven't tested others
         {
         my $oldLine;
         do
            {
            $oldLine = $line;
            map { $line =~ s/\b$_\b/$constants{$_}/g } keys %constants;
            }
         while ($line ne $oldLine);
         }

      # HACK: gas 2.17.50 no longer likes the xword ptr prefix in movdqa instructions.
      if ($gasVersion eq '2.17.50')
         {
         $line =~ s/xword ptr//i if $line =~ /movdqa/i;
         }

      # HACK: Pre-2.12.1 gas mixes up i386's CMPSD/MOVSD with SSE2's CMPSD/MOVSD; it also
      #       thinks that SSE2 instructions with QWORD PTR operands are reserved for x86-64
      # HACK: Unfortunately, post-2.15.90 gas on RHEL4 is broken again.
      if ($gasVersion lt '2.12.1' or $gasVersion gt '2.12.90')
         {
         # removing the modifier for SSE2 instructions seems to work
         $line =~ s/qword ptr//i
            if $line =~ /(add|andn?|cmp|u?comi|div|max|min|mov[ahlu]?|mul|x?or|shuf|sqrt|sub|unpck[hl])[ps]d|cvtt?[ps][diq]2[ps][diq]/i;

         # must use native AT&T syntax to avoid name collision
         if ($line =~ s/rep\s+movsd/rep movsl/i)
            {
            $line = ".att_syntax\n$line\n.intel_syntax noprefix";
            }
         }

      # HACK: gas 2.15.94 (FC4) gas does not know about word ptr in fstcw fldcw
      if ($gasVersion gt '2.15.92' and $gasVersion lt '2.15.95') {
	  $line =~ s/word ptr//i if $line =~ /(fstcw|fldcw)/i;
      } 

      # HACK: work around pre-2.11 gas bugs
      if ($gasVersion lt '2.11')
         {
         # memory references that add negative offsets crashed gas
         $line =~ s/(byte|[dqx]?word)\s+ptr\s*\[([^\]]*)(\s*\+\s*\-(\d))\]/$1 ptr\[$2 - $4\]/i;

         # SSE instructions which access 16 bytes of data caused problems in intel mode
         if ($line =~ /^\s*mov[au]ps\s+(\S.*\S)\s*,\s*(\S.*\S)\s*$/i)
            {
            my ($dest, $src) = ($1, $2);

            if ($src =~ /(byte|[dqx]?word)\s+ptr\s*\[([^\]]*)\]/i)
               {
               $src = &morphMemoryOperand($2);
               }
            elsif (&isRegister($src) == -1)
               {
               $src = "\%$src";
               }

            if ($dest =~ /(byte|[dqx]?word)\s+ptr\s*\[([^\]]*)\]/i)
               {
               $dest = &morphMemoryOperand($2);
               }
            elsif (&isRegister($dest) == -1)
               {
               $dest = "\%$dest";
               }

            $line = ".att_syntax\nmovaps $src, $dest\n.intel_syntax noprefix";
            }

         # the "offset flat:" notation was not recognized
         if ($line =~ s/$offsetFlatExpr\s*/\$/gi)
            {
            if ($line =~ /^\s*(\w+)\s+(\S.*\S)\s*,\s*(\S.*\S)\s*$/)
               {
               my ($op, $dest, $src, $size) = ($1, $2, $3);

               if ($src =~ /(byte|[dqx]?word)\s+ptr\s*\[([^\]]*)\]/i)
                  {
                  $src = &morphMemoryOperand($2); 
                  $size = $1;
                  }
               elsif ($src =~ /\$(\S+)\s*([\+\-])\s*\$(\S+)/)
                  {
                  $src = "\$($1 $2 $3)";
                  }
               elsif (&isRegister($dest) == -1)
                  {
                  $src = "\%$src";
                  }

               if ($dest =~ /(byte|[dqx]?word)\s+ptr\s*\[([^\]]*)\]/i)
                  {
                  $dest = &morphMemoryOperand($2); 
                  $size = $1;
                  }
               elsif ($dest =~ /\$(\S+)\s*([\+\-])\s*\$(\S+)/)
                  {
                  $dest = "\$($1 $2 $3)";
                  }
               elsif (&isRegister($dest) == -1)
                  {
                  $dest = "\%$dest";
                  }

               # FIXME: we need a fool-proof way to figure out the proper AT&T
               # FIXME: opcode suffix for a given instruction in MASM syntax
               if ($size)
                  {
                  $size = (split //, $size)[0];
                  $size = "l" if $size eq "d";
                  }
               else
                  {
                  $size = "l";
                  }

               die "Cannot parse $line" unless $src and $dest;
               $line = ".att_syntax\n$op$size $src, $dest\n.intel_syntax noprefix";
               }
            }
         }
      }
   continue
      {
      if ($line)
         {
         $comment ? print $out "$line\t/* $comment */\n" : print $out "$line\n";
         }
      else
         {
         $comment ? print $out "/* $comment */\n" : print $out "\n";
         }
      }
   }

sub findIncludeFile
   {
   my $filename = shift;
   my $directory = (grep { -e "$_/$filename" } @searchPath)[0];
   die "include file $filename not found\n" unless defined $directory;
   return "$directory/$filename";
   }

sub morphMemoryOperand
   {
   my $memRef = shift;
   $memRef =~ s/\s+//g;
   if ($memRef =~ /([er][abcdis][ipx]|r[189][0-5]?|\\\w+)\s*         # base or index register (or macro name)
                   (\+\s*([er][abcdis][ipx]|r[189][0-5]?|\\\w+))?\s* # index register (or macro name)
                   (\*\s*(\d))?\s*                                   # scale, assume base 10
                   ([\+\-]\s*[[:xdigit:]]+h?)?                       # displacement, assume base 10 or 16
                  /ix)
      {
      my ($base, $index, $scale, $disp) = ($1, $3, $5, $6);

      $index = $base, $base = undef if defined $base and defined $scale and not defined $index; 

      $scale =~ s/([[:xdigit:]]+)h/0x$1/i if defined $scale;

      $disp  =~ s/\s*([[:xdigit:]]+)h/0x$1/i if defined $disp;
      $disp  =~ s/^\+//                   if defined $disp;

      $memRef = "";
      $memRef .= "$disp"   if defined $disp;
      $memRef .= "(";
      $memRef .= "\%$base"  if defined $base;
      $memRef .= ",\%$index" if defined $index;
      $memRef .= ",$scale"   if defined $scale;
      $memRef .= ")";
      }
   else
      {
      return undef;
      }
   }

# Return 0 if the given text is not the name of an x86 register.
# Return 1 if the register name has the %-prefix required by AT&T syntax,
# or -1 if the register name is without a prefix, conforming to MASM syntax.
sub isRegister
   {
   my $text = shift;
   return 0 if $text !~ /^(\%)?([er]?([abcd]x|[is]p|[sd]i)|r([89]|1[0-5])[bwd]?|[abcd][lh]|xmm([0-9]|1[0-5])|mm[0-7])$/;
   return 1 if $1;
   return -1;
   }


