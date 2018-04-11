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
#include "server_A_skels.h"
using namespace std;

int main() {

	  // server init rpc with binder
	rpcInit();

	skeleton myFunc = &sum_skel;
	char funcArr[4];
	int  types[4];

	  // add double sum(double, double)
	funcArr[0] = 's';
	funcArr[1] = 'u';
	funcArr[2] = 'm';
	funcArr[3] = '\0';

	types[0] = (1 << ARG_OUTPUT) | (ARG_DOUBLE << 16);
	types[1] = (1 << ARG_INPUT)  | (ARG_DOUBLE << 16);
	types[2] = (1 << ARG_INPUT)  | (ARG_DOUBLE << 16);
	types[3] = 0;
	  // rpc regist
	rpcRegister( funcArr, types, myFunc );
	rpcRegister( funcArr, types, myFunc );


	rpcExecute();
}
