/*
 * The MIT License
 *
 * Copyright (c) 1997-2014 The University of Utah
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */



#include <Core/OS/Dir.h>

#include <Core/Exceptions/ErrnoException.h>
#include <Core/Exceptions/InternalError.h>
//#include <Core/Util/FileUtils.h>

#include <sci_defs/boost_defs.h>

#include <sys/types.h>
#include <sys/stat.h>
//#include <sys/syscall.h>

#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <iostream>

#include <unistd.h>
#include <dirent.h>

#include <boost/filesystem/operations.hpp>

#if defined( UINTAH_BOOST_FILESYSTEM_NAMESPACE_V3)
  namespace bfs = boost::filesystem3;
#else
  namespace bfs = boost::filesystem;
#endif
namespace bsys = boost::system;

using namespace std;
using namespace SCIRun;

Dir Dir::create(const string& name)
{
   int code = MKDIR(name.c_str(), 0777);
   if(code != 0)
      throw ErrnoException("Dir::create (mkdir call): " + name, errno, __FILE__, __LINE__);
   return Dir(name);
}

Dir::Dir()
{
}

Dir::Dir(const string& name)
   : name_(name)
{
}

Dir::Dir(const Dir& dir)
  : name_(dir.name_)
{
}

Dir::~Dir()
{
}

Dir& Dir::operator=(const Dir& copy)
{
   name_ = copy.name_;
   return *this;
}

void
Dir::remove(bool throwOnError)
{
  int code = rmdir(name_.c_str());
  if (code != 0) {
    ErrnoException exception("Dir::remove()::rmdir: " + name_, errno, __FILE__, __LINE__);
    if (throwOnError)
      throw exception;
    else
      cerr << "WARNING: " << exception.message() << "\n";
  }
  return;
}

// Removes a directory (and all its contents) using C++ calls.  Returns true if it succeeds.
bool
Dir::removeDir( const char * dirName )
{
  // Walk through the dir, delete files, find sub dirs, recurse, delete sub dirs, ...

  DIR *    dir = opendir( dirName );
  dirent * file = 0;

  if( dir == NULL ) {
    cout << "Error in Dir::removeDir():\n";
    cout << "  opendir failed for " << dirName << "\n";
    cout << "  - errno is " << errno << "\n";
    return false;
  }

  file = readdir(dir);
  while( file ) {
    if (strcmp(file->d_name, ".") != 0 && strcmp(file->d_name, "..") !=0) {
      string fullpath = string(dirName) + "/" + file->d_name;
      bool   isDir = false;
#if defined(_AIX)
      // Have to use 'stat' under AIX
      struct stat entryInfo;
      if( lstat( fullpath.c_str(), &entryInfo ) == 0 ) {
	if( S_ISDIR( entryInfo.st_mode ) ) {
	  isDir = true;
	}
      } else {
	printf( "Error statting %s: %s\n", fullpath, strerror( errno ) );
      }
#else
      if( file->d_type & DT_DIR ) {
	isDir = true;
      }
#endif
      if( isDir ) {
        removeDir( fullpath.c_str() );
      } else {
        int rc = ::remove( fullpath.c_str() );
        if (rc != 0) {
          cout << "WARNING: remove() failed for '" << fullpath.c_str() 
               << "'.  Return code is: " << rc << ", errno: " << errno << ": " << strerror(errno) << "\n";
          return false;
        }
      }
    }
    file = readdir(dir);
  }

  closedir(dir);
  int code = rmdir( dirName );

  if (code != 0) {
    cerr << "Error, rmdir failed for '" << dirName << "'\n";
    cerr << "  errno is " << errno << "\n";
    return false;
  }
  return true;
}

void
Dir::forceRemove(bool throwOnError)
{
   bfs::path p(name_);
   int code = bfs::remove_all(p);
   if (code == 0) {
     ErrnoException exception(string("Dir::forceRemove() failed to remove: ") + name_, errno, __FILE__, __LINE__);
     if (throwOnError)
       throw exception;
     else
       cerr << "WARNING: " << exception.message() << "\n";
   }
}

void
Dir::remove(const string& filename, bool throwOnError)
{
   string filepath = name_ + "/" + filename;
   bfs::path p(filepath);
   bool code = bfs::remove(p);
   if (code == false) {
     ErrnoException exception(string("Dir::remove failed to remove: ") + filepath, errno, __FILE__, __LINE__);
     if (throwOnError)
       throw exception;
     else
       cerr << "WARNING: " << exception.message() << "\n";
   }
}

Dir
Dir::createSubdir(const string& sub)
{
   return create(name_+"/"+sub);
}

Dir
Dir::getSubdir(const string& sub)
{
   // This should probably do more
   return Dir(name_+"/"+sub);
}

void
Dir::copy(Dir& destDir)
{
   bfs::path from(name_);  
   bfs::path to(destDir.name_);
   bfs::path new_to(to);
  
   new_to /= from.filename();
      
   bsys::error_code ec;
   bfs::create_directories(new_to,ec);
   
   if (ec != 0) {
     cout << "error code: " << ec << endl;
     cout << "error message: " << ec.message() << endl;
     throw InternalError(string("Dir::copy failed to copy: ") + name_, __FILE__, __LINE__);
   }

   // Create iterators for iterating all entries in the directory
   bfs::recursive_directory_iterator it(from);     
   bfs::recursive_directory_iterator end;   

   // Iterate over all entries in the directory and sub directories
   while (it!=end) {
     std::string it_string = it->path().c_str();
     unsigned int pos = it_string.find(from.filename().c_str());
     std::string to_string = it_string.substr (pos);

     bfs::path new_entry = to/to_string;
     
     // only copy if it does not  exist.
     if ( !bfs::exists(new_entry) ){
     
       bfs::copy(*it, new_entry, ec);

       if (ec != 0) {
         cout << "  ERROR: from: " << it_string << " to " << new_entry.c_str() << endl;
         cout << "  error code: " << ec << endl;
         cout << "  error message: " << ec.message() << endl;
         throw InternalError(string("Dir::copy failed to copy: ") + name_, __FILE__, __LINE__);
       }
       // When a symbolic link, don't iterate it. Can cause infinite loop.
       if (bfs::is_symlink(it->status())) it.no_push();
     
     }
     // Next directory entry
     it++;
   }

   return;
}

void
Dir::move(Dir& destDir)
{
   bfs::path from(name_);  
   bfs::path to(destDir.name_);
   bsys::error_code ec;
   
   bfs::rename(from,to,ec);
   if (ec != 0)
      throw InternalError(string("Dir::move failed to move: ") + name_, __FILE__, __LINE__);
   return;
}

void
Dir::copy(const std::string& filename, Dir& destDir)
{
   string filepath = name_ + "/" + filename;
   bfs::path from(filepath);  
   bfs::path to(destDir.name_ + "/" + filename);


   bsys::error_code ec;
   if (bfs::exists(to))
     bfs::remove(to);
   bfs::copy(from,to,ec);

   if (ec != 0)
      throw InternalError(string("Dir::copy failed to copy: ") + filepath + " to " + destDir.name_, __FILE__, __LINE__);
   return;
}

void
Dir::move(const std::string& filename, Dir& destDir)
{
   string filepath = name_ + "/" + filename;
   bfs::path from(filepath);  
   bfs::path to(destDir.name_);
   bsys::error_code ec;
   bfs::rename(from,to,ec);
   if (ec != 0)
      throw InternalError(string("Dir::move failed to move: ") + filepath, __FILE__, __LINE__);
   return;
}

void
Dir::getFilenamesBySuffix( const std::string& suffix,
                           vector<string>& filenames )
{
  DIR* dir = opendir(name_.c_str());
  if (!dir){ 
    return;
  }
    
  const char* ext = suffix.c_str();
  for(dirent* file = readdir(dir); file != 0; file = readdir(dir)){
    if ((strlen(file->d_name)>=strlen(ext)) && 
	(strcmp(file->d_name+strlen(file->d_name)-strlen(ext),ext)==0)) {
      filenames.push_back(file->d_name);
      //cout << "  Found " << file->d_name << "\n";
    }
  }
  closedir(dir);
}

bool
Dir::exists()
{
  struct stat buf;
  if(LSTAT(name_.c_str(),&buf) != 0)
    return false;
  if (S_ISDIR(buf.st_mode))
    return true;
  return false;
}