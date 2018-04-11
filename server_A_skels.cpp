#include<iostream>

#include"server_A_skels.h"

using namespace std;


int sum_skel( int *argTypes, void **args ) {
	return sum( ( double ** )args );
}


