// file for server lib
#include<arpa/inet.h>
#include<cstdlib>
#include<cstring>
#include<iostream>
#include<limits.h>
#include<netdb.h>
#include<netinet/in.h>
#include<string>
#include<sys/types.h>
#include<unistd.h>
#include<unordered_map>
#include <stack>


#include "globals.h"
#include "rpc.h"
#include "utilities.h"
using namespace std;


int sum( int *argTypes, void **args ) {
	( ( double ** )args )[0][0] = ( ( double ** )args )[1][0] + ( ( double ** )args )[2][0];
	cout << "FUNCTION EXECUTION RESULT: " << ( ( double ** )args )[0][0] << endl;
	return 0;
}


int sub( int *argTypes, void **args ) {
	( ( int ** )args )[0][0] = ( ( int ** )args )[1][0] - ( ( int ** )args )[2][0];
	cout << "FUNCTION EXECUTION RESULT: " << ( ( int ** )args )[0][0] << endl;
	return 0;
}


int main() {

	  // server init rpc with binder
	rpcInit();

	skeleton myFunc = &sum;
	char funcArr[4];
	int  types[4];

	  // add double sum(double, double)
	funcArr[0] = 's';
	funcArr[1] = 'u';
	funcArr[2] = 'm';
	funcArr[3] = '\0';

	types[0] = (1 << ARG_OUTPUT) | (ARG_DOUBLE << 16) | 3;
	types[1] = (1 << ARG_INPUT)  | (ARG_DOUBLE << 16) | 3;
	types[2] = (1 << ARG_INPUT)  | (ARG_DOUBLE << 16);
	types[3] = 0;
	  // rpc regist
	rpcRegister( funcArr, types, myFunc );


	  //  add int sub(int, int)
	funcArr[0] = 's';
	funcArr[1] = 'u';
	funcArr[2] = 'b';
	types[0] = (1 << ARG_OUTPUT) | (1 << ARG_INPUT ) | (ARG_INT << 16) | 3;
	types[1] = (1 << ARG_INPUT)  | (ARG_INT << 16);
	types[2] = 0;

	myFunc = &sub;
	rpcRegister( funcArr, types, myFunc );

	rpcExecute();
}
