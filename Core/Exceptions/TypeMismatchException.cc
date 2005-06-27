
#include <Packages/Uintah/Core/Exceptions/TypeMismatchException.h>
#include <iostream>

using namespace Uintah;

TypeMismatchException::TypeMismatchException(const std::string& msg)
    : d_msg(msg)
{
#ifdef EXCEPTIONS_CRASH
  std::cout << d_msg << "\n";
#endif
}

TypeMismatchException::TypeMismatchException(const TypeMismatchException& copy)
    : d_msg(copy.d_msg)
{
}

TypeMismatchException::~TypeMismatchException()
{
}

const char* TypeMismatchException::message() const
{
    return d_msg.c_str();
}

const char* TypeMismatchException::type() const
{
    return "Uintah::Exceptions::TypeMismatchException";
}
