#include <WinSock2.h>
#include <iostream>
#include "my_function.h"
#include "Class.h"

using std::endl;
using std::cout;

SOCKET BindListen(void)
{
	SOCKET hsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == hsock)
	{
		cout << "socket error : " << WSAGetLastError() << endl;
		return INVALID_SOCKET;
	}


	// 填充本地套接字地址
	struct sockaddr_in serv_addr = {0};	/*本地套接字地址*/
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(DEF_PORT);
	if(SOCKET_ERROR == bind(hsock, (const struct sockaddr *)&serv_addr, sizeof(struct sockaddr)))
	{
		cout << "bind error : " << WSAGetLastError() << endl;
		return INVALID_SOCKET;
	}

	// 开始监听(将其设置为监听状态)
	if (SOCKET_ERROR == listen(hsock, SOMAXCONN))
	{
		cout << "listen error : " << WSAGetLastError() << endl;
		closesocket(hsock);
		return INVALID_SOCKET;
	}

	return hsock;
}

SOCKET AcceptConnection(SOCKET hListenSocket)
{
	struct sockaddr_in clien_addr = {0};
	int addr_len = sizeof(struct sockaddr_in);

	SOCKET hsock  = accept(hListenSocket, (struct sockaddr *)&clien_addr, &addr_len);
	if (INVALID_SOCKET == hsock)
	{
		cout << "accept error : " << WSAGetLastError() << endl;
		return INVALID_SOCKET;
	}

	return hsock;
}

bool ShutdownConnection(SOCKET hClientSocket)
{
	char buff[BUFFER_SIZE];
	int result = 0;

	if (SOCKET_ERROR == shutdown(hClientSocket,SD_SEND))
	{
		cout << "shutdown error : " << WSAGetLastError() << endl;	
		return false;
	}

	do 
	{
		result = recv(hClientSocket, buff, sizeof(buff)/sizeof(char), 0);
		if (SOCKET_ERROR == result)
		{
			cout << "recv error : " << WSAGetLastError() << endl;
			return false;
		}
		else
		{
			cout << result << " unexpected bytes received." << endl;
		}
	} while (0 != result);

	/*关闭套接字*/
	if (SOCKET_ERROR == closesocket(hClientSocket))
	{
		cout << "closesocket error : " << WSAGetLastError() << endl;
		return false;
	}

	return true;
}

void DoWork(void)
{
	//获取监听套接字并进入监听状态
	SOCKET hListenSocket = BindListen();
	if (INVALID_ATOM == hListenSocket)
	{
		return;
	}
	
	//定义conns 用于保存当前所有客户端连接(不包括监听套接字,并且长度<FD_SETSIZE,之所以不能等于FD_SETSIZE是因为监听套接字要占一个位置)
	ConnectionList conns;

	//将套接字设置为非阻塞模式
	u_long nNoblock = 1;
	if (SOCKET_ERROR == ioctlsocket(hListenSocket, FIONBIO, &nNoblock))
	{
		cout << "ioctlsocket error : " << WSAGetLastError() << endl;
		return;
	}

	cout << "Server is running..." << endl;
	
	fd_set fdRead;
	fd_set fdWrite;
	fd_set fdExcept;

	//服务器循环
	while (true)
	{
		ResetFDSet(fdRead, fdWrite, fdExcept, hListenSocket, conns);
		int nRet = select(0, &fdRead, &fdWrite, &fdExcept, NULL);//循环会在此处暂停
		if (nRet < 0)
		{
			cout << "select error : " << WSAGetLastError() << endl;
			break;
		}
		
		//检查是否有新的客户端请求,若有新的连接请求(且conns未满),添加套conns中
		nRet = CheckAccept(fdRead, fdExcept, hListenSocket, conns);
		if (SOCKET_ERROR == nRet)
		{
			break;
		}
		//检查客户端连接,进行相应的操作
		CheckConn(fdRead, fdWrite, fdExcept, conns);
	}

	//释放资源
	for (ConnectionList::const_iterator iter = conns.begin(); iter != conns.end(); ++iter)
	{
		closesocket((*iter)->hSocket);
		delete *iter;
	}

	if(SOCKET_ERROR != closesocket(hListenSocket))
	{
		closesocket(hListenSocket);
	}

	return;
}

void ResetFDSet(fd_set& fdRead, fd_set& fdWrite, fd_set& fdExcept, SOCKET hListenSocket, const ConnectionList& conns)
{
	//首先清空各个fd_set
	FD_ZERO(&fdRead);
	FD_ZERO(&fdWrite);
	FD_ZERO(&fdExcept);

	//监听套接字每次都被放如fdRead,也就是说每次调用select都会检查是否有新的连接请求
	FD_SET(hListenSocket, &fdRead);
	FD_SET(hListenSocket, &fdExcept);

	for (ConnectionList::const_iterator iter=conns.begin(); iter!=conns.end(); ++iter)
	{
		Connection *pConn = *iter;

		//客户连接的缓冲区还有空间,可以继续接收数据,就需要把其对应的套接字句柄放入fdRead中
		if (pConn->nBytes < SO_MAX_MSG_SIZE)
		{
			FD_SET(pConn->hSocket, &fdRead);
		}
		
		//客户连接的缓冲区还有数据需要发送,就需要霸气对应的套接字句柄放入fdWrite中
		if (pConn->nBytes > 0)
		{
			FD_SET(pConn->hSocket, &fdWrite);
		}

		//每个连接诶套接字句柄都需要放到fdExcept中,以便select能够检测其I/O错误
		FD_SET(pConn->hSocket, &fdExcept);
	}
}

int CheckAccept(const fd_set& fdRead, const fd_set& fdExcept, SOCKET hListenSocket, ConnectionList& conns)
{
	int lastErr = 0;
	if (FD_ISSET(hListenSocket, &fdExcept) != 0) //当hListenSocket在描述符集fdExcept中返回非零值,否则,返回零.
	{
		int errlen = sizeof(lastErr);
		getsockopt(hListenSocket, SOL_SOCKET, SO_ERROR, (char*) &lastErr, &errlen);
		cout << "I/O error : " << lastErr << endl;

		return SOCKET_ERROR;
	}

	//检查fdRead中是否包含了监听套接字,如果是,则证明有新的连接请求, 可以调用accept
	if (FD_ISSET(hListenSocket, &fdRead) != 0 )
	{
		//由于fd_set有大小限制(最多为FD_SETSIZE), 所以当已有客户连接请求到达这个限制时,不再接收连接请求
		if (conns.size() >= FD_SETSIZE-1)	//监听套接字不再conns中
		{
			return 0;
		}
		//调用accept来接受客户端连接请求
		sockaddr_in saRemote;
		int nSize = (int)sizeof(sockaddr);
		SOCKET sd = accept(hListenSocket, (sockaddr *)&saRemote, &nSize);
		lastErr =  WSAGetLastError();
		if (sd == INVALID_SOCKET && lastErr != WSAEWOULDBLOCK)
		{
			cout << " accept error : " << lastErr << endl;
			return SOCKET_ERROR;
		}
		if (INVALID_SOCKET != sd)
		{
			//获取新的客户连接套接字句柄,需要把它设为非阻塞模式,以便对其使用select函数.
			u_long nNoBlock = 1;
			if (ioctlsocket(sd, FIONBIO, &nNoBlock))
			{
				cout << "ioctlsocket error : " << WSAGetLastError() << endl;
			}
			//为新的客户连接创建了一个Connection对象,并且加入conns中去
			conns.push_back(new Connection(sd));//动态生成
		}

	}

	return 0;
}

void CheckConn(const fd_set& fdRead, const fd_set& fdWrite, const fd_set& fdExcept, ConnectionList& conns)
{
	ConnectionList::iterator iter = conns.begin();
	while (iter != conns.end())
	{
		Connection *pConn = *iter;
		bool bOK = true;
		if (0 != FD_ISSET(pConn->hSocket, &fdExcept))
		{
			bOK = false;
			int lastErr;
			int errlen;
			getsockopt(pConn->hSocket, SOL_SOCKET, SO_ERROR, (char *)&lastErr, &errlen);
			cout << "I/O error : " << lastErr << endl;
		}
		else
		{
			//检查当前连接是否可读
			if (0 != FD_ISSET(pConn->hSocket, &fdRead))
			{
				bOK = TryRead(pConn);
			}
			else if (0 != FD_ISSET(pConn->hSocket, &fdWrite))
			{
				bOK = TryWrite(pConn);
			}
		}

		//发生了错误,关闭套接字并把其对应的对象从conns中移除
		if (false == bOK)
		{
			closesocket(pConn->hSocket);
			delete pConn;
			iter = conns.erase(iter);
		}
		else
		{
			++iter;
		}
	}
}

bool TryRead(Connection* pConn)
{
	// pConn->Buffer + pConn->nBytes 表示缓冲区Buffer可用空间的开始位置
	int nRent = recv(pConn->hSocket, pConn->Buffer+pConn->nBytes, (int)sizeof(pConn->Buffer)/sizeof(pConn->Buffer[0])-pConn->nBytes, 0);
	if (nRent > 0)
	{
		pConn->nBytes += nRent;
		
		return true;
	}
	else if (nRent == 0)//对方关闭连接(发送了SD_SEND分段)
 	{
		cout << "Connection closed by peer." << endl;
		PassaveShutdown(pConn->hSocket, pConn->Buffer, pConn->nBytes);
		
		return false;
	}
	else	//发生错误
	{
		int lastErr = WSAGetLastError();
		if (WSAEWOULDBLOCK == lastErr)
		{
			return true;
		}
		else
		{
			cout << "recv error : " << lastErr << endl;
			return false;
		}
	}
}

bool TryWrite(Connection* pConn)
{
	int nSend = send(pConn->hSocket, pConn->Buffer, pConn->nBytes, 0);
	if (nSend > 0)
	{
		pConn->nBytes -= nSend;//缓冲区中剩余数据数,若全部发送完,pCoon->nBytes == 0

		if (pConn->nBytes > 0)
		{	
			memmove(pConn->Buffer, pConn->Buffer+nSend, pConn->nBytes);//保证待发送数据(如果有)位于缓冲区最开始处
		}
		return true;
	}
	else if (0 == nSend) //对方连接被意外关闭
	{
		cout << "Connection closed by peer." << endl;
		PassaveShutdown(pConn->hSocket, pConn->Buffer, pConn->nBytes); //个人认为不需要
		return false;
	}
	else	//发生错误
	{
		int lastErr = WSAGetLastError();
		if (WSAEWOULDBLOCK == lastErr)
		{
			return true;
		}
		else
		{
			cout << "recv error : " << lastErr << endl;
			return false;
		}
	}

}

bool PassaveShutdown(SOCKET sd, const char* data, int datalen)
{
	if (NULL!=data && datalen>0)
	{
		//使用阻塞I/O模型把剩余数据发送出去
		u_long nNoBlock = 0;
		if (SOCKET_ERROR == ioctlsocket(sd, FIONBIO, &nNoBlock))
		{
			cout << "ioctlsocket error : " << WSAGetLastError() << endl;
			return false;
		}
	}

	//尝试将长度为datalen的数据全部发出
	int nSend = 0;
	while (nSend < datalen)
	{
		nSend = send(sd, data+nSend, datalen-nSend, 0);
		if (nSend > 0)
		{
			datalen -= nSend;
		}
		else if (nSend == 0)
		{
			cout << "Connection closed unexpectedly by peer." << endl;
			break;
		}
		else
		{
			cout << "send error : " << WSAGetLastError() << endl;
			return false;
		}
	}
	
	if (SOCKET_ERROR == shutdown(sd, SD_SEND))
	{
		cout << "shutdown error " << WSAGetLastError << endl;
		return false;
	}

	return true;
}



