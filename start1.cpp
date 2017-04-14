#include <stdlib.h>
#include <iostream>
#include <unistd.h>
using namespace std;
int main(int argc, char** argv)
{
	if(fork()==0)//1st child
	{
		cout << getpid() << "ch1" << endl;
		execl("./fbsd",argv[1]);
	}
	else //parent
	{
		if(fork()==0)//2nd child
		{
			cout << getpid() << "ch2" << endl;
			execl("./fbsd",argv[2]);
		}
		else //parent
		{
			cout << getpid() << "parent" << endl;
			execl("./fbsd",argv[3]);
		}
	}
}