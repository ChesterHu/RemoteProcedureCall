// Binder 
#include<cstring>
#include<deque>
#include<iostream>
#include<limits.h>
#include<netdb.h>
#include<netinet/in.h>
#include<stdlib.h>
#include<string>
#include<sys/select.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<unistd.h>
#include<unordered_map>
#include<unordered_set>
#include<vector>


#include "globals.h"
#include "utilities.h"
using namespace std;


#define MAX_CLIENT_NUM INT_MAX
  // this provides the entire list of servers(fd) for a function signature
  // TODO need it because the binder needs to inform clients whether there is
  // a server providing a specific service
unordered_map<string, unordered_set<int> > serverMap;
  // servers that have registered
deque<int> servers;
  // mapping from file descriptor to server 
unordered_map<int, pair<string, int> > fdToServer;
 // master file descriptor list
static fd_set master; 
  // file descriptor to listen
static int listener;
  // function to get sockaddr, IPv4 or IPv6. Since a pointer to struct sockaddr can be cast
  // to a pointer to struct sockaddr_in (struct sockaddr_in/struct sockadrr_in6 or IPv4/IPv6)
  // the function will return a pointer to struct in_addr { uint32_t s_addr; } for IPv4, or
  // a pointer to struct in6_addr { unsigned char s6_addr[16]; } for IPv6.
void* get_in_addr(sockaddr* sa);


pair<string, int> roundRobin( string sign );

void binderRecv(int fd, int *msgType);

void handleClient(int fd, long long msgLength);
void handleServer(int fd, long long msgLength);
  // TODO test
void terminateAll();
void updateCache(int fd);

void sendLocFailMsg( int socket );


// Binder main routine
int main()
{
	  // server's address information
	addrinfo hints, *server_info;
	  // make sure the struct is empty fo latter use
	memset(&hints, 0, sizeof hints);  // sizeof is an operator
	hints.ai_family = AF_UNSPEC;      // don't care IPv4 or IPv 6
	hints.ai_socktype = SOCK_STREAM;  // TCP stream sockets
	hints.ai_flags = AI_PASSIVE;      // fill in my IP for me

	  // getaddrinfo will return non-zero if there is error.
	  // If no error occured, server_info will now points to linked list of 1 or more struct addrinfos
	int getinfo_status;
	if ((getinfo_status = getaddrinfo(nullptr, "0", &hints, &server_info)) != 0)
		exit(1);

	addrinfo *iter;  // use an iterator to get available addrinfo
	  // loop through all the results and bind to the first we can
	for (iter = server_info; iter != nullptr; iter = iter->ai_next)
	{
		  // if error occurred on current node, print error, and try next node on linked list
		if ((listener = socket(iter->ai_family, iter->ai_socktype, iter->ai_protocol)) == -1)
			continue;
		  // If a socket is binding to the port before and hogging the port, allow it reuse the port 
		  // setting options at socket level (SOL_SOCKET)
		  // enable the option_name (SO_REUSEADDR)
		int enable = 1;
		if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof enable) == -1)
		{
			exit(1);
		}

		  // try to bind the socket with port
		((sockaddr_in*)(iter->ai_addr))->sin_addr.s_addr = htonl(INADDR_ANY);
		if (bind(listener, iter->ai_addr, iter->ai_addrlen) == -1)
		{
			close(listener);
			continue;
		}
		  // only use the first available socket
		break;
	}

	freeaddrinfo(server_info);  // all done with this structure;
	
	if (iter == nullptr)
		exit(1);
	
	if (listen(listener, MAX_CLIENT_NUM) == -1)
		exit(1);
	  // add the listener to the master set
	FD_ZERO(&master);  // clear the master set
	FD_SET(listener, &master);  // add listener to master file descriptor set
	int fd_max = listener;
	
	
	  // get host name
	char *hostname = new char[HOST_NAME_MAX + 1];
	gethostname(hostname, HOST_NAME_MAX + 1);
	  // get port id
	sockaddr_in sin;
	socklen_t sin_len = sizeof(sin);
	getsockname(listener, (sockaddr*)&sin, &sin_len);

	fprintf(stdout, "BINDER_ADDRESS %s\n", hostname);
	fprintf(stdout, "BINDER_PORT %i\n", ntohs(sin.sin_port));
	
	delete [] hostname;
	  // client's address information
	sockaddr_storage client_addr;
	socklen_t sock_len;
    int new_fd;   //new connection on new filedecripter
	char s[INET6_ADDRSTRLEN];

	  // temp file descriptor set for select()
	fd_set read_fds;
	FD_ZERO(&read_fds);

	  // Binder begin listening
	while (1)
	{
		read_fds = master; // copy from master
		if (select(fd_max + 1, &read_fds, nullptr, nullptr, nullptr) == -1)
		{
			exit(1);
		}


		for(int i = 0; i < fd_max + 1; ++i)
		{
			if (!FD_ISSET(i, &read_fds)) continue;// get new socket

			  // handle new connection
			if (i == listener)
			{
				sock_len = sizeof client_addr;
				new_fd = accept(listener, (sockaddr*)&client_addr, &sock_len);
				if (new_fd == -1) {
					continue;
				}
				FD_SET(new_fd, &master);  // add new client file descriptor in master set
				fd_max = std::max(fd_max, new_fd);  // keep track of max

				  // server print request from client
				inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr*)&client_addr), s, sizeof s);
			}

			else  // Example of binder communicate with client
			{
				  // TODO encapsulate to a function
				char *name;
				int  *argTypes;
				int   msgType;
				try
				{
					binderRecv(i, &msgType);
				}
				catch (int)
				{
					fdToServer.erase(i);
					for (auto& it : serverMap)
						it.second.erase(i);

					for (auto it = servers.begin(); it != servers.end(); it++)
						if (*it == i)
						{
							servers.erase(it);
							break;
						}

					close(i);
					FD_CLR(i, &master);
				}
			}  // one file descriptor end
		}  // for loop for finding available file descriptor end
	
	} // listen end



	return 0;
}






  // Binder read message from client/server
void binderRecv(int fd, int *msgType)
{
	long long msgLength = 0;
	int       bytesRead = 0;
	int       offset    = 0;
	char     *buffer;

	  // allocate buffer to receive
	buffer = new char[sizeof(long long) + sizeof(int)];
	do
	{
		int recvResult = recv(fd, buffer + bytesRead, sizeof(long long) + sizeof(int) - bytesRead, 0);
		if (recvResult <= 0)
		{
			delete [] buffer;
			throw 1;  // throw bad recv result, usually a server quit
		}
		bytesRead += recvResult;
	} while (bytesRead < sizeof(long long) + sizeof(int));

	  // read msgLength and msgType from buffer
	memcpy(&msgLength, buffer + offset, sizeof(long long)); offset += sizeof(long long);
	memcpy(msgType, buffer + offset, sizeof(int));          offset += sizeof(int);
	
	msgLength = msgLength - offset;
	  // done with this buffer
	delete [] buffer;

	switch(*msgType)
	{
		case LOC_REQUEST   : return handleClient(fd, msgLength);
		case CACHE_REQUEST : return updateCache(fd);
		case REGISTER      : return handleServer(fd, msgLength);
		case TERMINATE     : return terminateAll();
	}
}



  // binder receive from client
void handleClient(int fd, long long msgLength)
{
	char *buffer;
	char  name[65];
	int   msgType = LOC_SUCCESS;
	int   offset;
	string funcSign;
	pair<string, int> serverInfo;
	  
	  // allocate buffer to read client
	buffer = new char[msgLength];
	if (readToBuffer(fd, buffer, msgLength) < msgLength)
	{
		delete [] buffer;
		binderToClient(fd, LOC_FAILURE, name, &msgType);
		return;
	}

	  // read from buffer
	offset = 0;
	memcpy(name, buffer, 65); offset += 65;
	
	int *argTypes = reinterpret_cast<int*> (buffer + 65);
	  // find correct server
	funcSign = std::string(name) + signatureCreate(argTypes);
	
	  // done with this buffer
	delete [] buffer;

	if (serverMap.find(funcSign) == serverMap.end() || serverMap[funcSign].empty())
		msgType = LOC_FAILURE;
	else
		serverInfo = roundRobin(funcSign);
	/*
	  // send dummy server address for testing
	msgType = LOC_SUCCESS;
	serverInfo.first = "scspc136";
	serverInfo.second = 36305;
	*/
	  // send server information to client
	std::strcpy(name, serverInfo.first.c_str());
	binderToClient(fd, msgType, name, &serverInfo.second);

	  // done with this client
	close(fd);
	FD_CLR(fd, &master);
	return;
}


void handleServer(int fd, long long msgLength)
{
	long long argTypesLength = msgLength - HOST_NAME_MAX - 1 - 4 - 65;

	char *serverId = new char[HOST_NAME_MAX + 1];
	char funcName[65];
	int  portId;
	int  *argTypes = new int[argTypesLength];

	recv(fd, serverId, HOST_NAME_MAX + 1, 0);
	recv(fd, &portId, 4, 0);
	recv(fd, funcName, 65, 0);
	recv(fd, argTypes, argTypesLength, 0);

	  // get function signature
	std::string funcSign = std::string(funcName) + signatureCreate(argTypes);
	  // get server name
	std::string serverName(serverId);

	int msgToServerType = REGISTER_SUCCESS;
    int msgToServerWarn;
	  // if server's function already registered before
	if (serverMap.find(funcSign) != serverMap.end() && serverMap[funcSign].find(fd) != serverMap[funcSign].end())
		msgToServerWarn = msgToServerType = REGISTER_FAILURE;
	
	  // register server
	serverMap[funcSign].insert(fd);	
	fdToServer[fd] = make_pair(serverName, portId);
	  // no duplicates in servers dequeue
	servers.push_back(fd);
	for (int i = 0; i < servers.size() - 1; ++i)
		if (servers[i] == fd)
		{
			servers.pop_back();
			break;
		}
	
	  // write message to server
	long long msgToServerLength = sizeof(long long) + sizeof(int) + sizeof(int);

	char msgToServer[msgToServerLength];
	int  offset = 0;
	
   	memcpy(msgToServer + offset, &msgToServerLength, sizeof(long long));  offset += sizeof(long long);
	memcpy(msgToServer + offset, &msgToServerType, sizeof(int));          offset += sizeof(int);
   	memcpy(msgToServer + offset, &msgToServerWarn, sizeof(int));          offset += sizeof(int);

	send(fd, msgToServer, offset, 0);
	delete [] serverId;
	delete [] argTypes;

	return; 
}

void terminateAll()
{
	  // binder sends teminate message to all servers
	long long msgLength = sizeof(long long) + sizeof(int);
	int       msgType   = TERMINATE;
	  // write terminate message for servers
	char *termMsg = new char[msgLength];
	memcpy(termMsg, &msgLength, sizeof(long long));
	memcpy(termMsg + sizeof(long long), &msgType, sizeof(int));
	
	while (!servers.empty())
	{
		int serverFD = servers.front();
		servers.pop_front();

		if (FD_ISSET(serverFD, &master) && serverFD != listener)
		{
			send(serverFD, termMsg, msgLength, 0);

			fdToServer.erase(serverFD);
			for (auto& it : serverMap)
				it.second.erase(serverFD);

			for (auto it = servers.begin(); it != servers.end(); it++)
				if (*it == serverFD)
				{
					servers.erase(it);
					break;
				}

		
			// TODO receive comfirmation from server
			close(serverFD);
			FD_CLR(serverFD, &master);
		}
	}
	exit(0);	
	delete [] termMsg;
}

  // responds cache update request from client
void updateCache(int fd)
{
	// TODO send whole server data to client
	long long msgLength;
	int       msgType;
	int       reasonCode;
	int       offset;
	char     *buffer;

	if (servers.empty())  // no servers available
	{
		msgLength  = sizeof(long long) + sizeof(int) + sizeof(int);
		msgType    = CACHE_FAILURE;
		reasonCode = NO_SERVER_AVAILABLE;
		buffer = new char[msgLength];

		  // write buffer
		offset = 0;
		memcpy(buffer + offset, &msgLength, sizeof(long long));   offset += sizeof(long long);
		memcpy(buffer + offset, &msgType, sizeof(int));           offset += sizeof(int);
		memcpy(buffer + offset, &reasonCode, sizeof(int));        offset += sizeof(int);

		  // send failure to client
		send(fd, buffer, msgLength, 0);
		
		  // done with this buffer
		delete [] buffer;

		  // done with this client
		close(fd);
		FD_CLR(fd, &master);
		return;
	}

	  // send cache information to client
	msgType = CACHE_SUCCESS;
	  // compute message length
	msgLength = sizeof(long long) + sizeof(int);  // message length and message type
	for (auto it : serverMap)
	{
		msgLength += sizeof(int);          // function signature length
		msgLength += it.first.size() + 1;  // function signature, 1 for terminator
		msgLength += sizeof(int);          // number of severs for this function signature
		msgLength += it.second.size() * (65 + sizeof(int));  // number of servers * (server id + server port)
	}

	  // allocate message buffer for client
	buffer = new char[msgLength];
	
	  // write message to client for cache updating
	offset = 0;
	memcpy(buffer + offset, &msgLength, sizeof(long long));  offset += sizeof(long long);
	memcpy(buffer + offset, &msgType, sizeof(int));          offset += sizeof(int);
	
	for (auto it : serverMap)
	{
		int funcNameLength = it.first.size() + 1;
		int numServers     = it.second.size();

		memcpy(buffer + offset, &funcNameLength, sizeof(int));      offset += sizeof(int);
		memcpy(buffer + offset, it.first.c_str(), funcNameLength);  offset += funcNameLength;
		memcpy(buffer + offset, &numServers, sizeof(int));          offset += sizeof(int);
		
		  // write servers' id and ports
		for (int serverFd : it.second)
		{
			const char* serverId = fdToServer[serverFd].first.c_str();
			int portId = fdToServer[serverFd].second;

			memcpy(buffer + offset, serverId, std::strlen(serverId) + 1); offset += 65;
			memcpy(buffer + offset, &portId, sizeof(int));                 offset += sizeof(int);
		}
		  // finish write one function sinagture
	}

	  // send message to client for cache updating
	send(fd, buffer, msgLength, 0);

	  // done with this buffer
	delete [] buffer;

	  // done with client
	close(fd);
	FD_CLR(fd, &master);
}



  // find sign first
pair<string, int> roundRobin( string sign ) {
	unordered_set<int> avaiServers = serverMap[sign];
	pair<string, int> ret;
	for( int i = 0; i < servers.size(); i++ ) {
		unordered_set<int>::iterator it = avaiServers.find( servers[i] );
		if( it != avaiServers.end() ) {
			int putBack = servers[i];
			deque<int>::iterator toRemove = servers.begin() + i;
			servers.erase( toRemove );
			servers.push_back( putBack );
			ret = fdToServer[putBack];
		}
	}
	return ret;
}



void sendLocFailMsg( int socket ) {
	int err = 1;
	long long sendLen = sizeof( long long ) + 2 * sizeof( int );
	char *sendMsg = new char[sendLen];
	msgParse( sendMsg, sendLen, LOC_FAILURE );

	for( int z = sendLen - 1; z >= sizeof( long long ) + sizeof( int ); z-- ) {
		sendMsg[z] = err & 0xFF;
		err >>= 8;
	}

	send( socket, sendMsg, sizeof( long long ) + 2 * sizeof( int ), 0 );
	delete [] sendMsg;
}



inline
void* get_in_addr(sockaddr* sa)
{
	if (sa->sa_family == AF_INET)
		return &(((sockaddr_in*)sa)->sin_addr);  // cast to pointer to struct sockaddr_in for IPv4
	return &(((sockaddr_in6*)sa)->sin6_addr);  // cast to pointer to struct sockaddr_in6 fo IPv6
}
