#include <iostream>
#include "my_function.h"

#pragma comment(lib, "ws2_32.lib")

using std::endl;
using std::cout;
using std::cin;

int main(int argc, char *argv[])
{
	WSADATA wsadata = {0};
	if (SOCKET_ERROR == WSAStartup(MAKEWORD(2, 2), &wsadata))
	{
		cout << "WSAStartup error : " << WSAGetLastError() << endl;
		return EXIT_FAILURE;
	}

	DoWork();

	if (SOCKET_ERROR == WSACleanup())
	{
		cout << "WSAClearup error : " << WSAGetLastError() << endl;
		return EXIT_FAILURE;
	}


	return EXIT_SUCCESS;


}