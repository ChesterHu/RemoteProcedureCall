#include <iostream>
#include "server_A_func.h"

using namespace std;

int sum( double **args ) {
	args[0][0] = args[1][0] + args[2][0];
	cout << "FUNCTION EXECUTION RESULT: " << args[0][0] << endl;
	return 0;
}


