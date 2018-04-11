#include<iostream>
#include "rpc.h"
#include "utilities.h"
#include "globals.h"

using namespace std;
int main() {
	char name[4];
	name[0] = 's';
	name[1] = 'u';
	name[2] = 'm';
	name[3] = '\0';
	int types[4];
	void *args[3];
	int result;
	types[0] = (1 << ARG_OUTPUT) | (ARG_DOUBLE << 16) | 3;
	types[1] = (1 << ARG_INPUT)  | (ARG_DOUBLE << 16) | 3;
	types[2] = (1 << ARG_INPUT)  | (ARG_DOUBLE << 16);
	types[3] = 0;
	double args1[3];
	args1[1] = 0.1;
	double args2[3];
	args2[1] = 1.1;
	double args3[1];
	args3[0] = 2.1;
	args[0] = ( void * )args1;
	args[1] = ( void * )args2;
	args[2] = ( void * )args3;

	result = rpcCacheCall( name, types, args );
	cout << "result is: " << args1[1] << endl;
	cout << "rpcCacheCall return: " << result << endl;

	result = rpcCacheCall( name, types, args );
	cout << "cache call result is: " << args1[1] << endl;
	cout << "rpcCacheCall return: " << result << endl;
	result = rpcCacheCall( name, types, args );
	result = rpcCacheCall( name, types, args );
	result = rpcCacheCall( name, types, args );
	result = rpcCacheCall( name, types, args );

	name[0] = 's';
	name[1] = 'u';
	name[2] = 'b';
	name[3] = '\0';

	types[0] = (1 << ARG_OUTPUT) | (1 << ARG_INPUT ) | (ARG_INT << 16) | 3;
	types[1] = (1 << ARG_INPUT)  | (ARG_INT << 16);
	types[2] = 0;


	int ar1[3];
	ar1[0] = 4;
	int ar2[1];
	ar2[0] = 9;

	args[0] = ( void * )ar1;
	args[1] = ( void * )ar2;

	result = rpcCacheCall( name, types, args );
	result = rpcCacheCall( name, types, args );
	result = rpcCacheCall( name, types, args );
	result = rpcCacheCall( name, types, args );
	cout << "result is: " << ar1[0] << endl;
	cout << "rpcCacheCall return: " << result << endl;
	
	cout << "terminate binder and servers" << endl;
	rpcTerminate();
}
