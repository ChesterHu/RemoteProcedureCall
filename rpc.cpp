#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <iostream>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <queue>
#include <stack>
#include <stdlib.h>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>
#include "globals.h"
#include "rpc.h"
#include "utilities.h"
using namespace std;


struct threadArgs{
	int i;
	pthread_t *selfPtr;
};


pthread_mutex_t rpcLock;
bool serverTerminated = false;
stack<pthread_t*> threadStack;
static fd_set master;
static int numThreads;
// Sockets opened by a server for the binder and clients
int binderSock, clientSock;
// A local server hashmap to store skeletons 
static std::unordered_map<std::string, skeleton> functionMap;

static unordered_map<string, queue<pair<string, int> > > cache;  // function signature -> queue (server id, server port)

// RPC FUNCTION IMPLEMENTATION
//////////////////////////////////////////////////////////////////////////////////////////////////////
int rpcCall(char* name, int* argTypes, void** args)
{
	  // send a location request message to the binder to locate the server for the procedure
	  // if failure, return a negative integer, other otherwise it should return 0	
	addrinfo *binderinfo;
	int sockfd;
	  // connect to binder
	sockfd = connectTo(&binderinfo, nullptr, nullptr);
	  // count number of args
	int numArgs = 0;
	while (argTypes[numArgs] != 0) ++numArgs;
	  // make message for binder
	  //              message length             type      name           argTypes
	long long msgToBinderLength = sizeof(long long) + sizeof(int) + 65 + sizeof(int) * (numArgs + 1);
	int       msgType = LOC_REQUEST;
	char     *msgToBinder = new char[msgToBinderLength];
	
  	  // set message length and message type
	msgParse(msgToBinder, msgToBinderLength, msgType);
	  // set rpc name
	int offset = sizeof(long long) + sizeof(int);
	memcpy(msgToBinder + offset, name, strlen(name) + 1);
	offset += 65;
	  // set rpc argTypes
	memcpy(msgToBinder + offset, argTypes, sizeof(int) * (numArgs + 1));
	
	  // send message to binder
	send(sockfd, msgToBinder, msgToBinderLength, 0);
	  // TODO after get the location of server, send an excute-request message to the server
	delete [] msgToBinder;

	  // no sleep now

	char *serverId = new char[HOST_NAME_MAX + 1];
	int   portId;
	int   locStatus;

	if ((locStatus = clientRecvBinder(sockfd, serverId, &portId)) < 0)
	{
		close(sockfd);
		return locStatus;
	}
	  // done with binder
	close(sockfd);
	
	  // connect to server
	addrinfo *serverinfo;
	sockfd = connectTo(&serverinfo, serverId, &portId);
	msgType = EXECUTE;
	delete [] serverId;

	  // send message to server
	sendArgs(sockfd, msgType, name, argTypes, args);

	  // no sleep now
	int recvCode = recvServer(sockfd, name, argTypes, args);
	
	  // done with this server
	close(sockfd);

	return recvCode;
}


int rpcCacheCall(char *name, int *argTypes, void **args)
{
	  // get function signature and number of arguments
	const std::string funcSign = std::string(name) + signatureCreate(argTypes);
	  // try to find server in local cache
	if (cache.find(funcSign) != cache.end())
	{
		while (!cache[funcSign].empty())
		{
			  // try to connect server in the cache
			pair<string, int> server = cache[funcSign].front();
			cache[funcSign].pop();
			const char *serverId = server.first.c_str();
			int   portId   = server.second;

			int sockfd;
			addrinfo *serverinfo;
			if ((sockfd = connectTo(&serverinfo, serverId, &portId)) == -1)
				continue;
			
			  // try to send message to server
			sendArgs(sockfd, EXECUTE, name, argTypes, args);
			  // if execute success return 0 and push available server back to cache
			int rpcResult;
			if ((rpcResult = recvServer(sockfd, name, argTypes, args)) >= 0)
			{
				cache[funcSign].push(server);
				return rpcResult;
			}
			  // if execute fail, don't push back unavailable server
		}  // while end
	}  // if cache.find() end
	
	  // if no local cache, request cache update
	addrinfo *binderinfo;
	int sockfd;
	  // connect to binder
	sockfd = connectTo(&binderinfo, nullptr, nullptr);
	
	long long msgLength;
	int       msgType;
	int       offset;
	char     *buffer;

	  // set up message for cache update
	msgType = CACHE_REQUEST;
	msgLength = sizeof(long long) + sizeof(int);
	  // allocate buffer to send cache request to binder
	buffer = new char[msgLength];
	 
	  // write cache update request message	
	offset = 0;
	memcpy(buffer + offset, &msgLength, sizeof(long long));  offset += sizeof(long long);
	memcpy(buffer + offset, &msgType, sizeof(int));          offset += sizeof(int);

	  // send cache request message to binder
	send(sockfd, buffer, msgLength, 0);
	  // free buffer
	delete [] buffer;

	  // receive from binder
	
	  // get message length and message type
	msgLength = sizeof(long long) + sizeof(int);
	  // allocate buffer to receive binder message
	buffer = new char[msgLength];

	if(readToBuffer(sockfd, buffer, msgLength) < msgLength)  // fail to get message
	{
		close(sockfd);
		delete [] buffer;
		return -1;
	}

	  // read from buffer for message length and message type
	offset = 0;
	memcpy(&msgLength, buffer + offset, sizeof(long long));  offset += sizeof(long long);
	memcpy(&msgType, buffer + offset, sizeof(int));          offset += sizeof(int);
	
	  // done with this buffer
	delete [] buffer;
	  // update message length left
	msgLength = msgLength - sizeof(long long) - sizeof(int);

	if (msgLength < sizeof(int))  // minimum msgLength should left
	{
		close(sockfd);
		return -1;
	}

	  // if cache update fail
	if (msgType != CACHE_SUCCESS)
	{
		int reasonCode;
		char tempBuffer[4];
		if (readToBuffer(sockfd, tempBuffer, sizeof(int)) < sizeof(int))
			return -1;
		memcpy(&reasonCode, tempBuffer, sizeof(int));
		return reasonCode;
	}

	  // if success, read servers from binder
	buffer = new char[msgLength];
	if (readToBuffer(sockfd, buffer, msgLength) < msgLength)  // fail to receive full message
	{
		close(sockfd);
		delete [] buffer;
		return -1;
	}

	  // done with binder
	close(sockfd);

	  // read from buffer
	std::unordered_map<string, queue<pair<string, int> > > new_cache;
	offset = 0;
	do
	{
		  // read function signature
		int funcSignLength = 0;
		memcpy(&funcSignLength, buffer + offset, sizeof(int));   offset += sizeof(int);

		  // allocate buffer for funcSign
		char *funcSignBuffer = new char[funcSignLength];
		memcpy(funcSignBuffer, buffer + offset, funcSignLength); offset += funcSignLength;
		std::string newFuncSign(funcSignBuffer);

		  // done with this buffer
		delete [] funcSignBuffer;

		  // read server id and server port
		int numServer = 0;
		memcpy(&numServer, buffer + offset, sizeof(int));        offset += sizeof(int);

		while (numServer-- > 0)
		{
			char serverIdBuffer[65];
			int  portId;

			memcpy(serverIdBuffer, buffer + offset, 65);         offset += 65;
			memcpy(&portId, buffer + offset, sizeof(int));       offset += sizeof(int);

			std::string serverId(serverIdBuffer);
			new_cache[newFuncSign].push(std::make_pair(serverId, portId));
		}

	} while(offset < msgLength);

	cache.swap(new_cache);
	
	  // done with buffer
	delete [] buffer;

	if (cache.find(funcSign) == cache.end() || cache[funcSign].empty())
		return -1;
	
	  // make one call
	const char *serverId =  cache[funcSign].front().first.c_str();
	const int  *portId   = &cache[funcSign].front().second;
	
	addrinfo* serverinfo;
	sockfd = connectTo(&serverinfo, serverId, portId);
	sendArgs(sockfd, EXECUTE, name, argTypes, args);
	int result = recvServer(sockfd, name, argTypes, args);

	close(sockfd);
	
	return result;
}

int rpcInit() {
	int result;
	struct sockaddr_in addrBinder, addrClient, sinClient;
	struct in_addr **addr_list;
	struct hostent *binderHost;
	char hostname[HOST_NAME_MAX];
	pthread_mutex_init( &rpcLock, NULL );


	binderSock = socket( AF_INET, SOCK_STREAM, 0 );
	if( binderSock < 0 ) {	
		return SOCKET_CREATION;
	}
	clientSock = socket( AF_INET, SOCK_STREAM, 0 );
	if( clientSock < 0 ) {
		close( binderSock );
		return SOCKET_CREATION;
	}
	
	binderHost = gethostbyname( getenv( "BINDER_ADDRESS" ) );
	if( binderHost == NULL ) {
		close( binderSock );
		close( clientSock );
		return WRONG_BINDER_HOSTNAME;
	}
	addr_list = ( struct in_addr ** ) binderHost->h_addr_list;
	addrBinder.sin_family = AF_INET;
	addrBinder.sin_port = htons( atoi( getenv( "BINDER_PORT" ) ) );
	result = inet_pton( AF_INET, inet_ntoa( *addr_list[0] ), &( addrBinder.sin_addr ) );
	if( result < 0 ) {
		close( binderSock );
		close( clientSock );
		return ADDR_STRUCT_ERROR;
	}
	result = connect( binderSock, ( struct sockaddr * )&addrBinder, sizeof( addrBinder ) );
	if( result < 0 ) {
		close( binderSock );
		close( clientSock );
		return CONN_FAILURE;
	}
	
	addrClient.sin_family = AF_INET;
	addrClient.sin_port = htons( 0 );
	addrClient.sin_addr.s_addr = INADDR_ANY;
	result = bind( clientSock, ( struct sockaddr * )&addrClient, sizeof( addrClient ) );
	if( result < 0 ) {
		close( binderSock );
		close( clientSock );
		return BINDING_ERROR;
	}
	
	listen( clientSock, SOMAXCONN );
	return SUCCESS;
}
int rpcRegister( char *name, int *argTypes, skeleton f ) {
	int count = 0;
	for( int i = 0; argTypes[i] != 0; i++ ) {
		count++;
	}
	int argTypeSize = ( count + 1 ) * sizeof( int );
	long long msgLen = sizeof( long long ) + 2 * sizeof( int ) + HOST_NAME_MAX + 1 + 65 + argTypeSize;
	char msg[msgLen];
	int type = REGISTER;
	// send msg to binder
	memcpy( msg, &msgLen, sizeof( long long ) );
	int offset = sizeof( long long );
	
	memcpy( msg + offset, &type, sizeof( int ) );
	offset += sizeof( int );
	
	char hostname[HOST_NAME_MAX];
	struct sockaddr_in sin;
	socklen_t sinLen = sizeof( sin );
	getsockname( clientSock, ( struct sockaddr * )&sin, &sinLen );
	gethostname( hostname, HOST_NAME_MAX );
	int clientPort = ntohs( sin.sin_port );
	memcpy( msg + offset, &hostname, HOST_NAME_MAX);
	offset += HOST_NAME_MAX + 1;
	
	memcpy( msg + offset, &clientPort, sizeof( int ) );
	offset += sizeof( int );
	
	char nameDup[65];
	count = 0;
	int i = 0;
	while( name[i] != '\0' ) {
		count++;
		i++;
	}
	count++;
	memcpy( nameDup, name, count );
	memcpy( msg + offset, nameDup, 65 );
	offset += 65;
	
	int argTypesDup[argTypeSize / sizeof( int )];
	argTypesDup[argTypeSize / sizeof( int ) - 1] = 0;
	memcpy( argTypesDup, argTypes, argTypeSize );
	memcpy( msg + offset, argTypesDup, argTypeSize );
	
	int result = send( binderSock, msg, msgLen, 0 );
	// rec'v either success or failure
	// store the function only if binder registration is a success
	int recvMsgSize = sizeof( long long ) + 2 * sizeof( int );

	char *recvMsg = new char[recvMsgSize];
	if (readToBuffer(binderSock, recvMsg, recvMsgSize) < recvMsgSize)
	{
		delete [] recvMsg;
		return REGISTRATION_FAILURE;
	}

	
	long long recvLen;
	int recvType;
	int errorCode;

	memcpy(&recvLen, recvMsg, sizeof(long long));
	memcpy(&recvType, recvMsg + sizeof(long long), sizeof(int));
	memcpy(&errorCode, recvMsg + sizeof(long long) + sizeof(int), sizeof(int));

	delete [] recvMsg;

	string signature( name );
	signature += signatureCreate( argTypesDup );

	if( recvType == REGISTER_SUCCESS ) {
		functionMap[signature] = f;
		return SUCCESS;
	} else {
		if( errorCode == REGISTER_ALREADY ) {
			functionMap[signature] = f;
		} else {
			errorCode = REGISTRATION_FAILURE;
		}
		return errorCode;
	}
}
void sendExecuteError( int i, int errorType ) {
	int msgSize = sizeof( long long ) + 2 * sizeof( int );
	char failMsg[msgSize];
	int msgType = EXECUTE_FAILURE;
	memcpy( failMsg, &msgSize, sizeof( long long ) );
	memcpy( failMsg + sizeof( long long ), &msgType, sizeof( int ) );
	memcpy( failMsg + sizeof( long long ) + sizeof( int ), &errorType, sizeof( int ) );
	send( i, failMsg, msgSize, 0 );
}
inline
void* get_in_addr(sockaddr* sa)
{
	if (sa->sa_family == AF_INET)
		return &(((sockaddr_in*)sa)->sin_addr);  // cast to pointer to struct sockaddr_in for IPv4
	return &(((sockaddr_in6*)sa)->sin6_addr);  // cast to pointer to struct sockaddr_in6 fo IPv6
}


void *threadManager( void * ) {
	while( true ) {
		if( !threadStack.empty() ) {
			while( !threadStack.empty() ) {
				pthread_mutex_lock( &rpcLock );

				pthread_join( *threadStack.top(), NULL );
				numThreads--;
				threadStack.pop();

				pthread_mutex_unlock( &rpcLock );
			}
		} 
		else if( serverTerminated ) {
			break;
		}
	}
}


void *procExec( void *arg ) {
	int i = ( ( threadArgs * )arg )->i;
	skeleton func;
	int type;
	char funcName[65];
	int *argTypes;
	void **args;

	if (recvArgs( i, &type, funcName, &argTypes, &args ) < 0)
	{
		pthread_mutex_lock( &rpcLock );
		threadStack.push( ( ( threadArgs * )arg )->selfPtr );
		pthread_mutex_unlock( &rpcLock );
		
		clearArgs(argTypes, args);
		close(i);
		FD_CLR(i, &master);
		delete ( threadArgs * )arg;
		return arg;
	}
	// find function skeleton
	// get signature and find skeleton
	string signature = string( funcName ) + signatureCreate( argTypes );
	unordered_map<string, skeleton>::iterator iter = functionMap.find( signature );
	if( iter == functionMap.end() ) {
		// send failure msg

		sendExecuteError( i, EXECUTE_FUNC_NOT_FOUND );

	} else {
		func = functionMap[signature];
		
		// execute
		int result = ( *func )( argTypes, args );
		// send success msg
		if( result == 0 ) {
			sendArgs( i, EXECUTE_SUCCESS, funcName, argTypes, args );
		} else {
			// send failure msg
			sendExecuteError( i, EXECUTE_ARG_ERROR );
		}
		
	}
	
	clearArgs( argTypes, args );
	shutdown( i, SHUT_RDWR );
	close( i );
	  // push self to stack, wait for join
	pthread_mutex_lock( &rpcLock );
	threadStack.push( ( ( threadArgs * )arg )->selfPtr );
	pthread_mutex_unlock( &rpcLock );
	delete ( threadArgs * )arg;
}


int rpcExecute() {
	numThreads = 0;
	sockaddr_storage c;
	socklen_t sizeC = sizeof( c );
	int new_fd;
	char s[INET6_ADDRSTRLEN];
	FD_ZERO( &master );

	FD_SET( clientSock, &master );
	FD_SET( binderSock, &master );

	fd_set read_fds;
	FD_ZERO( &read_fds );
	pthread_t manager;
	pthread_create( &manager, NULL, threadManager, NULL );

	int fd_max = max( clientSock, binderSock );
	while( 1 ) 
	{
		read_fds = master;
			
		if( select( fd_max + 1, &read_fds, NULL, NULL, NULL ) == -1 ) {
			exit( 1 );
		}
		for( int i = 0; i < fd_max + 1; i++ ) 
		{
			if( !FD_ISSET( i, &read_fds ) ) {
				continue;
			}
			if ( i == clientSock ) {
				// handle new connection
				new_fd = accept( clientSock, ( sockaddr* )&c, &sizeC );
				if ( new_fd == -1 ) {
					continue;
				}
				FD_SET( new_fd, &master );  // add new client file descriptor in master set

				fd_max = max( fd_max, new_fd );  // keep track of max
				// server print request from client
				inet_ntop( c.ss_family, get_in_addr( ( struct sockaddr*)&c ), s, sizeof( s ) );
			} else if( i == binderSock ) {
				long long msgLength;
				int msgType;
				char *buffer;

				msgLength = sizeof(long long) + sizeof(int);
				buffer = new char[msgLength];
				if (readToBuffer(i, buffer, msgLength) < msgLength)  // fail to get message from binder
				{
					delete [] buffer;
					continue;
				}
				
				memcpy(&msgLength, buffer, sizeof(long long));
				memcpy(&msgType, buffer + sizeof(long long), sizeof(int));

				delete [] buffer;

				if( msgType == TERMINATE ) {
					serverTerminated = true;
					goto terminate;
				}
			} else {
				pthread_t t;
				threadArgs *arg = new threadArgs();
				arg->i = i;
				arg->selfPtr = &t;
				numThreads++;
				pthread_create( &t, NULL, procExec, ( void * )arg );
				FD_CLR(i, &master);
			}
		}
	}

	terminate:
		pthread_join( manager, NULL );
		pthread_mutex_destroy( &rpcLock );
		return 0;// Normal binder requested termination
}


int rpcTerminate( void )
{
	  // connect to binder
	addrinfo *binderinfo;
	int sockfd;
	sockfd = connectTo(&binderinfo, nullptr, nullptr);

	  // set up message for binder
	const long long msgLength = sizeof(long long) + sizeof(int);
	const int       msgType   = TERMINATE;
	  // initialize message
	int   offset = 0;
	char  msgToBinder[msgLength];
	  // write message for binder
	memcpy(msgToBinder, &msgLength, sizeof(long long));  offset += sizeof(long long);
	memcpy(msgToBinder + offset, &msgType, sizeof(int));
	  // send
	send(sockfd, msgToBinder, msgLength, 0);

	return 0;
}
