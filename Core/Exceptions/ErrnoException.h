/*
 *  ErrnoException.h: Generic exception for internal errors
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   May 2000
 *
 *  Copyright (C) 2000 SCI Group
 */

#ifndef SCICore_Exceptions_ErrnoException_h
#define SCICore_Exceptions_ErrnoException_h

#include <SCICore/Exceptions/Exception.h>
#include <string>

namespace SCICore {
namespace Exceptions {

class ErrnoException : public Exception {
public:
  ErrnoException(const std::string&, int err);
  ErrnoException(const ErrnoException&);
  virtual ~ErrnoException();
  virtual const char* message() const;
  virtual const char* type() const;
	 
  int getErrno() const;
protected:
private:
  std::string d_message;
  int d_errno;

  ErrnoException& operator=(const ErrnoException&);
};

}
}

#endif
