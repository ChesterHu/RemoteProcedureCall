#include <iostream>
#include <assert.h>
#include <cstring>
#include <bitset>

#include "utilities.h"
#include "rpc.h"
#include "globals.h"

using namespace std;

int main()
{

	  // testing helper function msgParse and parseMsg
	{
		long long l_in, l_out;
		int t_in, t_out;
		char msg[100];

		l_in = ((long long)12345654321); t_in = 3;
		msgParse(msg, l_in, t_in);
		parseMsg(msg, &l_out, &t_out);
		assert(l_in == l_out && t_in == t_out);

		l_in = (long long)1234321454; t_in = 3;
		msgParse(msg, l_in, t_in);
		parseMsg(msg, &l_out, &t_out);
		assert(l_in == l_out && t_in == t_out);

	}

	  // test cases for matching argsToBytes and bytesToArgs
	{
		const int PARAMETER_COUNT = 2;
		const int LENGTH = 32;
		const size_t MSG_SIZE = 100000;	
		typedef double argType1;
		typedef long long argType2;
		
		  // two arguments
		argType1 result = 234;
		argType2 vector[LENGTH] = {1};
		  // two arguments's types 
		int argTypes[PARAMETER_COUNT + 1];
		argTypes[0] = (1 << ARG_OUTPUT) | (ARG_DOUBLE << 16);            // result
		argTypes[1] = (1 << ARG_INPUT)  | (ARG_LONG << 16) | LENGTH;   // vector input
		argTypes[2] = 0;                                              // terminator
		  // set up arguments	
		void **args = new void* [2];
		args[0] = (void *) &result;
		args[1] = (void *) vector;
	
		char msg[MSG_SIZE];
		int  *argTypes_bk;
		void **args_bk;
		  // test msg parsing
		size_t numBytesWrite = argsToBytes(msg, argTypes, args);
		int    numArgs = bytesToArgs(msg, &argTypes_bk, &args_bk);
		
		argType1 *result_bk = reinterpret_cast<argType1*> (args_bk[0]);
		argType2 *vector_bk = reinterpret_cast<argType2*> (args_bk[1]);

		 // assert result
		assert(numArgs == 2);
		assert(numBytesWrite == sizeof(argType1) + sizeof(argType2) * LENGTH + sizeof(int) * (numArgs + 1));
		assert(argTypes_bk[0] == argTypes[0]);
		assert(argTypes_bk[1] == argTypes[1]);
		assert(argTypes_bk[2] == 0);
		assert(*result_bk == result);
		for (int i = 0; i < LENGTH; ++i)
			assert(vector_bk[i] == vector[i]);

		clearArgs(argTypes_bk, args_bk);
	}

	  // first call double sum(double, double)
	{
		const int PARAMETER_COUNT = 3;

		typedef double    argType1;
		typedef long long argType2;
		typedef int       argType3; 
		char rpcName[] = "sum";	
		
		  // two arguments
		double result;
		double input_1 = 12;
		double input_2 = 21;

		  // two arguments's types 
		int argTypes[PARAMETER_COUNT + 1];

		argTypes[0] = (1 << ARG_OUTPUT) | (ARG_DOUBLE << 16);            // result
		argTypes[1] = (1 << ARG_INPUT)  | (ARG_DOUBLE << 16);            // result2
		argTypes[2] = (1 << ARG_INPUT)  | (ARG_DOUBLE << 16);            // vector input
		argTypes[3] = 0;                                                 // terminator

		  // set up arguments	
		void **args = new void* [PARAMETER_COUNT];
		args[0] = (void *) &result;
		args[1] = (void *) &input_1;
		args[2] = (void *) &input_2;
		
		rpcCall(rpcName, argTypes, args);
		cout << "result of rpcCall double sum(double 12, double 21): " << result << endl;
	}

	  // second rpcCall int sub(int, int)
	{
		const int PARAMETER_COUNT = 3;
		const int LENGTH = 32;
		char rpcName[] = "sub";	
		
		  // two arguments
		int result;
		int input_1 = 132;
		int input_2 = 123;

		  // two arguments's types 
		int argTypes[PARAMETER_COUNT + 1];

		argTypes[0] = (1 << ARG_OUTPUT) | (ARG_INT << 16);            // result
		argTypes[1] = (1 << ARG_INPUT)  | (ARG_INT << 16);            // input1
		argTypes[2] = (1 << ARG_INPUT)  | (ARG_INT << 16);            // input2
		argTypes[3] = 0;                                              // terminator

		  // set up arguments	
		void **args = new void* [PARAMETER_COUNT];
		args[0] = (void *) &result;
		args[1] = (void *) &input_1;
		args[2] = (void *) &input_2;
		
		  // first call double sum(double, double)
		rpcCall(rpcName, argTypes, args);
		cout << "result of rpcCall int sub(int 132, int 123): " << result << endl;
	}


	{
		const int PARAMETER_COUNT = 3;

		typedef double    argType1;
		typedef long long argType2;
		typedef int       argType3; 
		char rpcName[] = "sum";	
		
		  // two arguments
		double result;
		double input_1 = 12;
		double input_2 = 21;

		  // two arguments's types 
		int argTypes[PARAMETER_COUNT + 1];

		argTypes[0] = (1 << ARG_OUTPUT) | (ARG_DOUBLE << 16);            // result
		argTypes[1] = (1 << ARG_INPUT)  | (ARG_DOUBLE << 16);            // result2
		argTypes[2] = (1 << ARG_INPUT)  | (ARG_DOUBLE << 16);            // vector input
		argTypes[3] = 0;                                                 // terminator

		  // set up arguments	
		void **args = new void* [PARAMETER_COUNT];
		args[0] = (void *) &result;
		args[1] = (void *) &input_1;
		args[2] = (void *) &input_2;
		
		rpcCacheCall(rpcName, argTypes, args);
		cout << "result of rpcCall double sum(double 12, double 21): " << result << endl;
	}


	/*
	 // terminate binder
	{
		addrinfo *binderinfo;
		int sockfd;

		  // connect to binder
		sockfd = connectToBinder(&binderinfo);

		  //              message length             type   
		long long msgToBinderLength = sizeof(long long) + sizeof(int) ;
		int       msgType = TERMINATE;
		char     *msgToBinder = new char[msgToBinderLength];
		
		  // set message length and message type
		msgParse(msgToBinder, msgToBinderLength, msgType);
		  // send message to binder
		send(sockfd, msgToBinder, msgToBinderLength, 0);
		  // TODO after get the location of server, send an excute-request message to the server
		delete [] msgToBinder;
	}
	*/
	printf("test passed\n");
}
