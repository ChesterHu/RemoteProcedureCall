#ifndef UTILITIES_DEFINED
#define UTILITIES_DEFINED
// HELPER FUNCTIONS FOR RPC
//////////////////////////////////////////////////////////////////////////////////////////////////////

#include <limits.h>  
#include <cstring>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <iostream>  // for testing
#include <arpa/inet.h>
#include <string.h>

  // Parse message, get length and type of message
  // Arguments:
  // 	char      *msg    : array of valid message
  // 	long long *length : address of the variable to store the message's length
  // 	int       *type   : address of the variable to store the message's type
void parseMsg(char *msg, long long *length, int *type);



  // Store length and type into first 12 byte of msg
  // Arguments:
  // 	char      *msg    : array of chars to store length and type information
  // 	long long length : the message's length 
  // 	int       type   : the message's type
void msgParse(char *msg, long long length, int type);



  // Given an argType, get argument's information
  // Arguments:
  // 	int  *argTypes  : the argType to parse
  //	int  *typeSize  : the size of argument's type
  //	int  *argSize   : the size of arguemnt's array 
  // Return:
  // 	string argTypeName : the name of argType, e.g. "int" for scalar, "int*" for array
std::string parseArgType(const int* argType, int* typeSize, int* argSize);



  // Read argument of RPC to a format of protocal, return the msg's size.
  // Arguments: 
  // 	char   *msg       : the buffer to write msg
  // 	int    *argTypes  : the array of rpc argument types
  // 	void   **args     : the array of rpc arguments
size_t argsToBytes(char *msg, const int *argTypes, void **args);



  // Read argument types and arguments from message, return number of arguments array, will allocate memory for argTypes and args
  // Arguments:
  // 	char   *msg      : message to read
  // 	size_t  msg_size : message's size
  // 	int    *argTypes : argument's types
  // 	void   **args    : arguments 
int bytesToArgs(char *msg, int **argTypes, void ***args);
 


  // Connect to Binder, return the file descriptor id
  // this function will do:
  // 	step 1: get binder address, binder port number
  // 	step 2: get binder address information
  // 	step 3: find a file descriptor for future usage
  // 	return: the file descriptor id
  // Arguments:
  // 	addrinfo **binderinfo  : address of pointer to the binder infomation, should be allocated before this function being called
int connectTo(addrinfo** serverinfo, const char *server_addr, const int *portId);


  // Binder send message to rpc client, for the corresponding server identifier and port id
  // Arguments:
  // 	int   fd       : the given file descriptor id
  // 	int   msgType  : the returned message type can be LOC_SUCCESS/LOC_FAILURE
  //	char *serverId : pointer to the array of char (string), strlen(serverId) should be no more than HOST_NAME_MAX
  //	char *portId   : pointer to the array of char with size 2 bytes, port_id should be converted to 16-bits before call this function
void binderToClient(int fd, int msgType, char *serverId, int *portId);



  // Client recv message from binder
  // Arguments:
  // 	int   fd       : file descriptor to receive from binder
  //	char *serverId : buffer of size HOST_NAME_MAX, to store serverId
  //	char *portId   : bufer of size 2, to store serverId
  // Return:
  // 	int            : 0 if receive message is LOC_SUCCESS, -1 if receive message is LOC_FAILURE
int clientRecvBinder(int fd, char *serverId, int *portId);


  // Send args into file descriptor
  // Arguments:
  // 	int    fd      : file descriptor to send argTypes and args
  // 	int    msgType : the message type sender wants to send to receiver
  // 	char  *name    : the function name for args
  // 	int   *argTypes: the arguments' types
  // 	void **args    : the arguments 
void sendArgs(int fd, int msgType, const char *name, const int *argTypes, void **args);


  // allocate memory
int recvArgs(int fd, int *msgType, char *name, int **argTypes, void ***args);

  // clean up allcated memory for args
void clearArgs(int *argTypes, void **args);




std::string signatureCreate(const int *argTypes );


int recvServer(int fd, char *name, int *argTypes, void **args);


  // helper function for reading from file descriptor into buffer

int readToBuffer(int fd, char *buffer, long long msgLength);

// TODO : client to server
#endif  // UTILITIES DEFIEND
