//static char *id="@(#) $Id$";

/*
 *  Remote.cc: Socket utility functions for remote messaging. 
 *   NOTE: sendRequest() and receiveReply() should be rewritten to take some
 *   type of generic message.  They are currently hardwired to the remote
 *   GUI stuff.
 *
 *  Written by:
 *   Michelle Miller
 *   Department of Computer Science
 *   University of Utah
 *   May 1998
 *
 *  Copyright (C) 1998 SCI Group
 */

#ifndef _WIN32

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <SCICore/TclInterface/Remote.h>

using namespace std;
//#define DEBUG 1

// server-side function: returns the listening socket

namespace SCICore {
namespace TclInterface {

int setupConnect (int port)
{
    int in_socket = socket (SOCKET_DOMAIN, SOCKET_TYPE, PROTO);
    if (in_socket < 0) {
        perror ("Error opening incoming socket");
        exit (-1);
    }

#ifdef DEBUG
cerr << "setupConnect() opened listen socket = " << in_socket << endl;
#endif

    // name socket using wildcards to permit different protocols
    struct sockaddr_in master;
    master.sin_family = SOCKET_DOMAIN;
    master.sin_addr.s_addr = INADDR_ANY;
    master.sin_port = port;

    // bind to socket
#ifdef __linux
    if (bind (in_socket, (sockaddr*)&master, sizeof(master))) {
#else
    if (bind (in_socket, &master, sizeof(master))) {
#endif
        perror ("binding stream socket");
        exit (-1);
    }

    // listen on incoming socket for slave connect request
    if (listen (in_socket, MAX_Q_LENGTH) < 0) {
        perror ("Listen failed on in_socket");
        exit (-1);
    }

    return in_socket;
}

// server-side function: returns the connected socket

int acceptConnect (int in_socket)
{
    // accept slave connection & create communication channel
    int slave_socket = accept (in_socket, 0, 0);
    if (slave_socket == -1)
        perror ("Accept of slave_socket failed");
#ifdef DEBUG
    else
        cerr << "acceptConnect() accepted remote connection = "
	     << slave_socket << endl;
#endif

    return slave_socket;
}

// client-side function: requests a connection with a server

int requestConnect (int port, char* host)
{
    struct sockaddr_in master;
    struct hostent *hp;

#ifdef DEBUG
    cerr << "Entering requestConnect (port = " << port << ")" << endl;
#endif

    // create a socket
    int master_socket = socket (SOCKET_DOMAIN, SOCKET_TYPE, PROTO);
    if (master_socket < 0) {
        perror ("Error opening outgoing socket");
        return -1;
    }

    // connect to socket using name and port specified in command line
    hp = gethostbyname (host);
    if (hp == 0) {
        perror ("unknown host");
        return -1;
    }
    bzero ((char *)&master, sizeof(master));
    bcopy (hp->h_addr, &master.sin_addr, hp->h_length);
    master.sin_family = SOCKET_DOMAIN;
    master.sin_port = htons (port);

#ifdef __linux
    if (connect (master_socket, (sockaddr*)&master, sizeof(master)) < 0) {
#else
    if (connect (master_socket, &master, sizeof(master)) < 0) {
#endif
        perror ("connecting stream socket");
        return -1;
    }

#ifdef DEBUG
    cerr << "Return socket = " << master_socket << " from requestConnect ("
	 << port << ")" << endl;
#endif

    return master_socket;
}

int sendRequest (TCLMessage* msg, int skt)
{
    char buf[BUFSIZE];
    ssize_t byte_cnt = 0;    
    ssize_t msg_len = sizeof(*msg);
    bzero (buf, sizeof (buf));
    bcopy ((char *)msg, buf, sizeof (*msg));
    byte_cnt = write (skt, buf, sizeof(buf));
    if (byte_cnt == -1) {
	perror ("sendRequest() write failed");
	return -1;
    }

    while (byte_cnt < msg_len) {
	char *ptr = buf + byte_cnt;
	int i = write (skt, ptr, sizeof(ptr));
	byte_cnt += i;
    }
    return 1;
}

int receiveReply (TCLMessage* msg, int skt)
{
    char buf[BUFSIZE];
    bzero (buf, sizeof (buf));
    read (skt, buf, sizeof(buf));
    bcopy (buf, (char *) msg, sizeof (*msg));
    return 1;
}

} // End namespace TclInterface
} // End namespace SCICore

#endif // win32

//
// $Log$
// Revision 1.4.2.2  2000/10/26 10:04:44  moulding
// merge HEAD into FIELD_REDESIGN
//
// Revision 1.4.2.1  2000/09/28 03:11:44  mcole
// merge trunk into FIELD_REDESIGN branch
//
// Revision 1.6  2000/09/27 22:20:53  yarden
// move include on Remote.h after the system files includes
//
// Revision 1.5  2000/08/02 22:27:07  jas
// Added using namespace std;
//
// Revision 1.4  1999/10/07 02:08:03  sparker
// use standard iostreams and complex type
//
// Revision 1.3  1999/08/23 06:30:40  sparker
// Linux port
// Added X11 configuration options
// Removed many warnings
//
// Revision 1.2  1999/08/17 06:39:44  sparker
// Merged in modifications from PSECore to make this the new "blessed"
// version of SCIRun/Uintah.
//
// Revision 1.1  1999/07/27 16:57:15  mcq
// Initial commit
//
// Revision 1.1.1.1  1999/04/24 23:12:25  dav
// Import sources
//
//
