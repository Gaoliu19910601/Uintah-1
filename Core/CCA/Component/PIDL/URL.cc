
/*
 *  URL.h: Abstraction for a URL
 *
 *  Written by:
 *   Steven G. Parker
 *   Department of Computer Science
 *   University of Utah
 *   July 1999
 *
 *  Copyright (C) 1999 SCI Group
 */

#include <Core/CCA/Component/PIDL/URL.h>
#include <Core/CCA/Component/PIDL/MalformedURL.h>
#include <sstream>

using PIDL::URL;

URL::URL(const std::string& protocol,
		       const std::string& hostname,
		       int portno, const std::string& spec)
    : d_protocol(protocol), d_hostname(hostname),
      d_portno(portno), d_spec(spec)
{
}   

URL::URL(const std::string& str)
{
    // This is pretty simple minded, but it works for now
    int s=str.find("://");
    if(s == -1)
	throw MalformedURL(str, "No ://");
    d_protocol=str.substr(0, s);
    std::string rest=str.substr(s+3);
    s=rest.find(":");
    if(s == -1){
	s=rest.find("/");
	if(s==-1){
	    d_hostname=rest;
	    d_portno=0;
	    d_spec="";
	} else {
	    d_hostname=rest.substr(0, s);
	    d_spec=rest.substr(s+1);
	}
    } else {
	d_hostname=rest.substr(0, s);
	rest=rest.substr(s+1);
	std::istringstream i(rest);
	i >> d_portno;
	if(!i)
	    throw MalformedURL(str, "Error parsing port number");
	s=rest.find("/");
	if(s==-1){
	    d_spec="";
	} else {
	    d_spec=rest.substr(s+1);
	}
    }
}

URL::~URL()
{
}

std::string URL::getString() const
{
    std::ostringstream o;
    o << d_protocol << "://" << d_hostname;
    if(d_portno > 0)
	o << ":" << d_portno;
    if(d_spec.length() > 0 && d_spec[0] != '/')
	o << '/';
    o << d_spec;
    return o.str();
}

std::string URL::getProtocol() const
{
    return d_protocol;
}

std::string URL::getHostname() const
{
    return d_hostname;
}

int URL::getPortNumber() const
{
    return d_portno;
}

std::string URL::getSpec() const
{
    return d_spec;
}

