/*
  The contents of this file are subject to the University of Utah Public
  License (the "License"); you may not use this file except in compliance
  with the License.
  
  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
  License for the specific language governing rights and limitations under
  the License.
  
  The Original Source Code is SCIRun, released March 12, 2001.
  
  The Original Source Code was developed by the University of Utah.
  Portions created by UNIVERSITY are Copyright (C) 2001, 1994 
  University of Utah. All Rights Reserved.

  Environment.h: Interface to setting environemnt variables and parsing .rc files

  Written by:
    McKay Davis
    Scientific Computing and Imaging Institute 
    University of Utah
    January 2004
    Copyright (C) 2004 SCI Institute

*/


#ifndef Core_Util_Environment_h
#define Core_Util_Environment_h 1

#include <string>

namespace SCIRun {
  using std::string;
  void create_sci_environment(char **environ);
  bool find_and_parse_scirunrc();
  bool parse_scirunrc( const string &filename );

  // Use the following functions to get/put environment variables.
  void sci_putenv( const string & key, const string & val );
  const char *sci_getenv( const string & key );

  // sci_getenv_p
  // will return a bool representing the value of environment variable 'key'
  // returns FALSE if and only if:
  //   the variable does not exist, is empty,
  //   is equal (Case insensitive) to 'false', 'no', 'off', or '0' 
  // returns TRUE:
  //   otherwise.
  bool sci_getenv_p( const string & key );
}


#endif // #ifndef Core_Util_Environment_h
