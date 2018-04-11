#ifndef GLOBALS_INCLUDED
#define GLOBALS_INCLUDED

// RPC ARGUMENT TYPE
//////////////////////////////////////////////////////////////////////////////////////////////////////
#define ARG_CHAR   1
#define ARG_SHORT  2
#define ARG_INT    3
#define ARG_LONG   4
#define ARG_DOUBLE 5
#define ARG_FLOAT  6

// input/output position
#define ARG_INPUT  31
#define ARG_OUTPUT 30

#ifndef HOST_NAME_MAX
	#define HOST_NAME_MAX 64
#endif

// RPC REQUEST TYPE
//////////////////////////////////////////////////////////////////////////////////////////////////////

#define REGISTER         1
#define LOC_REQUEST      2
#define LOC_SUCCESS      3
#define LOC_FAILURE      4
#define EXECUTE          5
#define EXECUTE_SUCCESS  6
#define EXECUTE_FAILURE	 7
#define TERMINATE        8
#define REGISTER_SUCCESS 9
#define REGISTER_FAILURE 10
#define CACHE_REQUEST    11
#define CACHE_SUCCESS    12
#define CACHE_FAILURE    13

#define SUCCESS	0

// SERVRE INITIALIZATION STATUS
#define SOCKET_CREATION		-1
#define WRONG_BINDER_HOSTNAME	-2
#define ADDR_STRUCT_ERROR	-3
#define CONN_FAILURE		-4
#define BINDING_ERROR		-5

// SERVER REGISTRATION WARNINGS
#define REGISTER_ALREADY	10

// SERVER REGISTRATION FAILURE
#define REGISTRATION_FAILURE	-6

// SERVER EXECUTION STATUS
#define EXECUTE_FUNC_NOT_FOUND	-7
#define EXECUTE_ARG_ERROR	-8

// LOCATE SERVER FAILURE
#define NO_SERVER_AVAILABLE	-9
#endif  // GLOBALS INCLUDED
