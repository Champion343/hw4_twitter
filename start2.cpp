#include <stdlib.h>
#include <iostream>
#include <unistd.h>
using namespace std;
int main()
{
	if(fork()==0)//1st child
	{
		cout << getpid() << " master pid" << endl;
		execl("./master","50032", "true");
	}
	else //parent
	{
		if(fork()==0)//2nd child
		{
			cout << getpid() << " replica pid" << endl;
			execl("./master","50034","0");
		}
		else //parent
		{
			cout << getpid() << " replica pid" << endl;
			execl("./master","50033","0");
			if(fork()==0)
				execl("./fbsd","50038");
		}
	}
}