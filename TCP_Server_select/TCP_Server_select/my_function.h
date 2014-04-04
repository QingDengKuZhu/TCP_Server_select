#ifndef MY_FUNCTION_H
#define MY_FUNCTION_H

#include <stdio.h>
#include <WinSock2.h>
#include "Class.h"

#define		BUFFER_SIZE		SO_MAX_MSG_SIZE
#define		DEF_PORT			10000

#pragma comment(lib, "ws2_32.lib")

// 创建一个监听套接字并把本地套接字地址(本地IP地址和本地端口)绑定到监听套接字.
// 若成功,返回监听套接字句柄;若失败,返回INVALID_SOCKET;
SOCKET BindListen(void);

// 服务器主题函数
void DoWork(void);

//分析conns中的客户连接,根据分析结构填充各个fd_set
void ResetFDSet(fd_set& fdRead, fd_set& fdWrite, fd_set& fdExcept, SOCKET hListenSocket, const ConnectionList& conns);

//检查是否有新的客户连接请求,若有新的连接请求(且conns未满),添加套conns中
//若监听套接字出现异常(在fdExcept中),返回SOCKET_ERROR,否者返回0(不论是否有新的连接,也不论conns是否已满).
int CheckAccept(const fd_set& fdRead, const fd_set& fdExcept, SOCKET hListenSocket, ConnectionList& conns);

//检查当前所有客户连接,看它们是否可读,可写, 还是发生I/O错误;根据检查结果,进行相应的操作.
void CheckConn(const fd_set& fdRead, const fd_set& fdWrite, const fd_set& fdExcept, ConnectionList& conns);

//若成功接收数据,返回true.
bool TryRead(Connection* pConn);

//若成功发送数据,返回true.
bool TryWrite(Connection* pConn);

//安全关闭连接
bool PassaveShutdown(SOCKET sd, const char* buff, int len);

#endif