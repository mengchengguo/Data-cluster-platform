/*
 * 程序名：demo08.cpp，此程序用于演示socket通讯的服务端。
 * 作者：gmc
*/
#include "../_public.h"
 
int main(int argc,char *argv[])
{
  if (argc!=2)
  {
    printf("Using:./demo08 port\nExample:./demo08 5005\n\n"); return -1;
  }

  CTcpServer TcpServer;

  // 服务端初始化  端口要转换为整数
  if(TcpServer.InitServer(atoi(argv[1])) == false){
    printf("TcpServer.InitServer(%s) failed.\n", argv[1]);
    return -1;
  }

  // 等待客户端的连接
  if(TcpServer.Accept() == false){
    printf("TcpServer.Accept() failed.\n");
    return -1;
  }
// 获取客户端ip的地址
  printf("客户端（%s）已连接。\n", TcpServer.GetIP());

  char buffer[102400];

  // 与客户端通讯，接收客户端发过来的报文后，回复ok。
  while (1)
  {
    memset(buffer,0,sizeof(buffer));
    if ( (TcpServer.Read(buffer)) == false) break; // 接收客户端的请求报文。
    
    printf("接收：%s\n",buffer);

    strcpy(buffer,"ok");
    if ( (TcpServer.Write(buffer)) == false) break; // 向客户端发送响应结果。

    printf("发送：%s\n",buffer);
  }
}
