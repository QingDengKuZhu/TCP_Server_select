#ifndef CLASS_H
#define CLASS_H

#include <WinSock2.h>
#include "my_function.h"
#include <vector>

using std::vector;

class Connection
{
public:
	SOCKET hSocket;				//已连接套接字
	char	Buffer[SO_MAX_MSG_SIZE];	//缓冲区大小
	int nBytes;					//缓冲区中待发送数据大小
	Connection(SOCKET socket) : hSocket(socket), nBytes(0) {;}		//构造函数
};


//由于fd_set默认只能包含FD_SETSIZE(默认为64)个套接字句柄,所以程序最多能够同时服务63个客户连接(监听套接字要占据fd_set的一个空间)
//也就是当客户连接数已经达到63个的时候,服务器将不再接受新的客户连接,除非已有的客户连接断开

typedef vector<Connection *> ConnectionList;




#endif