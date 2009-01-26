/*

The MIT License

Copyright (c) 1997-2009 Center for the Simulation of Accidental Fires and 
Explosions (CSAFE), and  Scientific Computing and Imaging Institute (SCI), 
University of Utah.

License for the specific language governing rights and limitations under
Permission is hereby granted, free of charge, to any person obtaining a 
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation 
the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the 
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included 
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL 
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
DEALINGS IN THE SOFTWARE.

*/


/* A Bison parser, made by GNU Bison 1.875.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum pdttokentype {
     IDENTIFIER = 258,
     OTHERLINE = 259,
     MAP = 260,
     ININTERFACE = 261,
     OUTINTERFACE = 262,
     CLOSEININTERFACE = 263,
     CLOSEOUTINTERFACE = 264,
     INOUTREMAP = 265,
     INOUTOMIT = 266,
     ARROW = 267,
     DBLANGLE = 268
   };
#endif
#define IDENTIFIER 258
#define OTHERLINE 259
#define MAP 260
#define ININTERFACE 261
#define OUTINTERFACE 262
#define CLOSEININTERFACE 263
#define CLOSEOUTINTERFACE 264
#define INOUTREMAP 265
#define INOUTOMIT 266
#define ARROW 267
#define DBLANGLE 268




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 115 "parser.y"
typedef union YYSTYPE {
  char* ident;
  char* oline;

  std::string* text;
  IrDefinition* irDef;
  IrDefList* irDef_list;
  IrMethodList* IrMethod_list;
  IrMethod* irMeth;
  IrArgumentList* irArg_list;
  IrArgument* irArg;
  IrNameMapList* irNM_list;
  IrNameMap* irNM; 
} YYSTYPE;
/* Line 1240 of yacc.c.  */
#line 77 "y.tab.h"
# define pdtstype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE pdtlval;



