/*
 * 程序名：tcpselect.cpp，此程序用于演示采用select模型的使用方法。
 * 作者：gmc
*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>

// 初始化服务端的监听端口。
int initserver(int port);

int main(int argc,char *argv[])
{
  if (argc != 2) { printf("usage: ./tcpselect port\n"); return -1; }

  // 初始化服务端用于监听的socket。
  int listensock = initserver(atoi(argv[1]));
  printf("listensock=%d\n",listensock);

  if (listensock < 0) { printf("initserver() failed.\n"); return -1; }

  fd_set readfds; // 读事件socket的集合，包括监听socket和客户端连接上来的socket
  FD_ZERO(&readfds); // 初始化读事件socket的集合
  FD_SET(listensock, &readfds); // 把listensock添加到读事件socket的集合中

  int maxfd = listensock; // 记录集合中socket的最大值
  while (true)
  {
    // 事件：1)新客户端的连接请求accept；2)客户端有报文到达recv，可以读；3)客户端连接已断开；
    //       4)可以向客户端发送报文send，可以写。
    // 可读事件  可写事件
    // select() 等待事件的发生(监视哪些socket发生了事件)。

  }
  return 0;
}



// 初始化服务端的监听端口。
int initserver(int port)
{
  int sock = socket(AF_INET,SOCK_STREAM,0);
  if (sock < 0)
  {
    perror("socket() failed"); return -1;
  }

  int opt = 1; unsigned int len = sizeof(opt);
  setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&opt,len);

  struct sockaddr_in servaddr;
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(port);

  if (bind(sock,(struct sockaddr *)&servaddr,sizeof(servaddr)) < 0 )
  {
    perror("bind() failed"); close(sock); return -1;
  }

  if (listen(sock,5) != 0 )
  {
    perror("listen() failed"); close(sock); return -1;
  }

  return sock;
}

