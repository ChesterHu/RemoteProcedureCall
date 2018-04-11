#ifndef RPC_DEFINED
#define RPC_DEFINED
#include<unordered_map>
// Sockets opened by a server for the binder and clients

// Definition of skeleton
typedef int ( *skeleton )( int *, void ** );

// RPC FUNCTION DECLARATION
//////////////////////////////////////////////////////////////////////////////////////////////////////


// Returns an int as a result of a rpc call from client to server.
// equals to 0    : a successful execution
// greater than 0 : a warning
// less than 0    : a severe error (e.g. no available server)
// 
// Arguments:
// char* name    : the name of remote procedure to be executed
// int* argTypes : array specifies the types of the arguments, terminated by 0
// void** args   : array of pointers to different arguments. with max length pow(2, 16)
//
// Once the rpc excuted successfully, the result size and types will be store in the argTypes, and result
// will be stored in args. Note the result could have different size from input arguments.
int rpcCall(char* name, int* argTypes, void** args);

// Returns an int as the status code of server initiation
// equals to 0: successful initiation
// less than 0: initiation failures( e.g. sockets cannot be opened )
//
// Arguments:
// None
//
// It opens up two sockets for the binder and clients repectively.
int rpcInit( void );

// Returns an int as the status code of function registration
// equals to 0	: successful initiation
// equals to 10 : function has been registered already
//
// Arguments:
// char *name	: the name of the remote procedure to be exexuted
// int *argTypes: array specifying the types of the arguments, terminated by 0
// skeleton f	: function pointer to the local function corresponding to the remote procedure
//
// It sends a message to the binder to register a local function.
// Upon successful registration, the local database has a record of the local function.
int rpcRegister( char *name, int *argTypes, skeleton f );

// Returns an int as the status code of function execution
// equals to 0: successful execution
// less than 0: execution error( e.g. no registered procedure to serve )
//
// Arguments:
// None
//
// It consistently accepts client calls, performs procedures, and sends result back to clients.
// If binder sends termination message, it will gracefully close all file descriptors and terminate the server.
int rpcExecute( void );


// Returns an int as a result of a rpc call from client to server.
// equals to 0    : a successful execution
// greater than 0 : a warning
// less than 0    : a severe error (e.g. no available server)
// 
// Arguments:
// char* name    : the name of remote procedure to be executed
// int* argTypes : array specifies the types of the arguments, terminated by 0
// void** args   : array of pointers to different arguments. with max length pow(2, 16)
//
// Once the rpc excuted successfully, the result size and types will be store in the argTypes, and result
// will be stored in args. Note the result could have different size from input arguments.

int rpcCacheCall(char *name, int *argTypes, void **args);
// Returns an int as the status code of system termination.
// equals to 0	: successful termination
//
// Arguments:
// None
//
// It sends terminate message to binder, which will cause termination of servers.
int rpcTerminate( void );
#endif  // RPC_DEFINED 
