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



#include <Packages/rtrt/Core/Token.h>



// the one and only TokenManager instance
Token::TokenManager Token::token_manager_;
unsigned            Token::indent_ = 0;



bool
Token::ParseArgs(ifstream &str)
{
  unsigned loop=0;
  string curstring, literal;
  char curchar;
  DELIMITER delimiter;

  while (loop++<nargs_) {
    str >> curstring;
    delimiter = (DELIMITER)curstring[0];
    if (match(delimiter,(DELIMITER)curstring[curstring.size()-1])) {
      literal = curstring.substr(1,curstring.size());
      curstring = literal.substr(0,literal.size()-1);
    } else if (delimiter == '\"' || delimiter == '\'') {
      literal = curstring.substr(1,curstring.size());
      str.get(curchar);
      while (curchar != delimiter) {
	literal += curchar;
	str.get(curchar);
      }
      curstring = literal;
    }
    args_.push_back(curstring);
#if 0
    cout << "Token: " << moniker_ << " argument " 
	 << loop << " = \"" << curstring << "\"" << endl;
#endif
  }
  return true;
}



bool 
Token::ParseChildren(ifstream &str)
{
  string    curstring;
  DELIMITER delimiter = DEL_NONE;
  DELIMITER unknown_delimiter;
  unsigned  unknown_delimiter_stack = 0;

#if DEBUG
  cout << "Token: parsing a " << moniker_ << " token." << endl;
#endif

  if (!file_) {
    // parse the starting delimiter
    str >> curstring;
    delimiter = (DELIMITER)curstring[0];
    bool delimiter_found = true;
    switch (delimiter) {
	case DEL_LBRACE:
	case DEL_LBRACKET:
	case DEL_LPAREN:
	case DEL_LANGLE:
	case DEL_DOUBLEQUOTE:
	case DEL_SINGLEQUOTE:
	  break;
	default:
          delimiter_found = false;
          break;
    }
    if (!delimiter_found) {
      cerr << "Parse Error: Expected Delimiter:\n\t" 
           << "\"" << moniker_ << " ->" << curstring << "<-\"" << endl;
      return false;
    }
  }
  
  // parse all child tokens, and the closing delimiter
  str >> curstring;
  for(;;) {
    if (str.eof()) {
      cerr << "Token: EOF" << endl;
      return true;
    }
    if (valid_child_moniker(curstring)) {
      Token *newtoken = token_manager_.MakeToken(curstring);

      if (!newtoken) {
	cerr << "Token Manager: unknown token: " << curstring << endl;
	str >> curstring;
	continue;
      }

      newtoken->SetParent(this);
      children_.push_back(newtoken);
      if (!newtoken->Parse(str)) return false;
      str >> curstring;
    } else if (!file_ && match(delimiter,(DELIMITER)curstring[0])) {
      break;
    } 
#if IGNORE_UNKNOWN_TOKENS
    else {
#if DEBUG
      cerr << "Token: ignoring unknown token: " << curstring << endl;
      cerr << "\t parent is " << moniker_ << endl;
#endif
      // does the unknown token have a starting delimiter
      str >> curstring;
      unknown_delimiter = (DELIMITER)curstring[0];
      switch(unknown_delimiter) {
	  case DEL_LBRACE:
	  case DEL_LBRACKET:
	  case DEL_LPAREN:
	  case DEL_LANGLE:
//        case DEL_DOUBLEQUOTE:
//        case DEL_SINGLEQUOTE:
	    break;
	  default:
	    unknown_delimiter = (DELIMITER)'\0';
      }
      
      if (unknown_delimiter) {
	unknown_delimiter_stack++;
	str >> curstring;
        for(;;) {
	  if (unknown_delimiter == 
	      (DELIMITER)curstring[curstring.size()-1]) {
	    unknown_delimiter_stack++;
	  } else if (match(unknown_delimiter,
			   (DELIMITER)curstring[curstring.size()-1])) {
	    unknown_delimiter_stack--;
	    if (unknown_delimiter_stack==0)
	      break;
	  }
	  str >> curstring;
	}
	str >> curstring;
      }
    }
#else
    else {
      cerr << "Parse Error: unknown token:\n\t"
	   << "\"" << moniker_ << " { ... ->" << curstring << "<- ..." 
	   << endl;
      return false;
    }
#endif
  }
  
  return true;
}



// default token parser
bool
Token::Parse(ifstream &str)
{
  // empty token, has no args or children
  if (nargs_ == 0 && valid_child_monikers_.size() == 0) 
    return true;

  // parse args (if any)
  if (nargs_) {
    if (!ParseArgs(str))
      return false;
  }

  // parse children (if any)
  if (valid_child_monikers_.size()) {
    if (!ParseChildren(str))
      return false;
  }
    
  return true;
}



// default token writer
void
Token::Write(ofstream &str)
{
  size_t loop, length;

  if (!file_) {
    // write the token's moniker
    Indent(str);
    str << moniker_ << " ";
    
    // write the arguments (if any)
    if (args_.size()) {
      length = args_.size();
      for (loop=0; loop<length; ++loop) 
	str << args_[loop] << " ";
    }

    // write the delimiter and increase the indentation
    if (children_.size()) {
      str << "{" << endl;
      ++indent_;
    } else 
      str << endl;
      
  }

  // write the children (if any)
  if (!file_ && children_.size()) {
    length = children_.size();
    for (loop=0; loop<length; ++loop) {
      children_[loop]->Write(str);
    }
    
    // decrease the indentation and write the closing delimiter
    --indent_;
    Indent(str);
    str << "}" << endl;
  }
}



// write out tabs (for file indentation)
void
Token::Indent(ofstream &str)
{
  for (unsigned loop=0; loop<indent_; ++loop)
    str << "\t";
}



bool
Token::match(DELIMITER d1, DELIMITER d2)
{
  if (d1 == DEL_LPAREN && d2 == DEL_RPAREN) return true;
  else if (d1 == DEL_LANGLE && d2 == DEL_RANGLE) return true;
  else if (d1 == DEL_LBRACE && d2 == DEL_RBRACE) return true;
  else if (d1 == DEL_LBRACKET && d2 == DEL_RBRACKET) return true;
  else if (d1 == DEL_DOUBLEQUOTE && d2 == DEL_DOUBLEQUOTE) return true;
  else if (d1 == DEL_SINGLEQUOTE && d2 == DEL_SINGLEQUOTE) return true;
  return false;
}



