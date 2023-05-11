/*
 * 程序名：fileserver.cpp，文件传输的服务端
 * 作者：gmc
*/
#include "_public.h"

CLogFile logfile; // 服务程序的运行日志
CTcpServer TcpServer; // 创建服务端对象

// 程序运行的参数结构体
struct st_arg{
  int clienttype; // 客户端类型，1-上传文件；2-下载文件。
  char ip[31]; // 客户端的IP地址
  int port; // 客户端的端口
  int ptype; // 文件上传成功后文件的处理方式1-删除文件，2-移动到备份目录
  char clientpath[301]; // 本地文件存放的根目录
  char clientpathbak[301]; // 文件成功上传后，本地文件备份的根目录，当ptype==2时有效
  bool andchild; // 是否上传clientpath目录下各级子目录的文件，true-是；false-否
  char matchname[301]; // 待上传文件名的匹配方式，如"*.txt,*.XML",注意大写
  char srvpath[301]; // 服务端文件存放的根目录
  int timetvl; // 扫描本地目录的时间间隔，单位秒
  int timeout; // 进程心跳的超时时间
  char pname[51]; // 进程名，用"tcpgetfiles_后缀"的方式
}starg;

void FathEXIT(int sig); // 父进程退出函数
void ChldEXIT(int sig); // 子进程退出函数

char strrecvbuffer[1024]; // 发送报文的buffer
char strsendbuffer[1024]; // 接收报文的buffer

bool srv000(const char* strrecvbuffer, char* strsendbuffer);// 心跳
// 登录业务处理函数
bool ClientLogin();

// 上传文件主函数
void RecvFilesMain();

// 把xml解析到参数starg结构中
bool _xmltoarg(char *strxmlbuffer);

int main(int argc,char *argv[])
{
  if (argc!=3)
  {
    printf("Using:./fileserver port logfile\nExample:./fileserver 5005 /log/idc/fileserver.log\n\n"); return -1;
  }

  CloseIOAndSignal();
  signal(SIGINT, FathEXIT); signal(SIGTERM, FathEXIT);
  
  if(logfile.Open(argv[2], "a+") == false){ printf("logfile.Open(%s) failed\n", argv[2]);  return -1; }

  // 服务端初始化
  if(TcpServer.InitServer(atoi(argv[1])) == false)
  {
    logfile.Write("TcpServer.InitServer(%s) failed.\n", argv[1]);
    return -1;
  }
  
  while(true){
    // 等待客户端的连接
    if(TcpServer.Accept() == false){
      logfile.Write("TcpServer.Accept() failed.\n");
      FathEXIT(-1);
      // return -1;
    }

    logfile.Write("客户端（%s）已连接。\n", TcpServer.GetIP());
//现在是开发调试阶段，所以我们把多进程的代码注释掉
// printf("listenfd = %d, connfd = %d\n", TcpServer.m_listenfd, TcpServer.m_connfd);

    // if(fork() > 0) { // 父进程继续回到Accept();
    //   TcpServer.CloseClient();
    //   continue;
    // }
    // // 子进程重新设置退出信号。
    // signal(SIGINT, ChldEXIT); signal(SIGTERM, ChldEXIT);

    // TcpServer.CloseListen();

    // 子进程与客户端进行通讯，处理业务
      // 处理登录客户端的登录报文，如果登陆失败，退出去
    if(ClientLogin() == false) ChldEXIT(-1);

      // 如果clienttype == 1 调用上传文件的主函数
    if(starg.clienttype == 1) RecvFilesMain();
      // 如果clienttype == 2 调用下载文件的主函数



      // 与客户端通讯，接收客户端发过来的报文后，回复ok。
    

    ChldEXIT(0);
    // return 0;
  }
}

void FathEXIT(int sig){// 父进程退出函数
// 以下代码是为了防止信号处理函数在执行的过程中被信号中断
  signal(SIGINT, SIG_IGN); signal(SIGTERM, SIG_IGN);
  logfile.Write("父进程退出。sig = %d.\n", sig);
  TcpServer.CloseListen(); // 关闭监听的socket
  kill(0, 15); // 通知全部的子进程退出
  exit(0);
}

void ChldEXIT(int sig){// 子进程退出函数
// 以下代码是为了防止信号处理函数在执行的过程中被信号中断
  signal(SIGINT, SIG_IGN); signal(SIGTERM, SIG_IGN);
  logfile.Write("子进程退出。sig = %d.\n", sig);
  TcpServer.CloseClient(); // 关闭客户端的socket
  exit(0);
}

// 登录
bool ClientLogin(){
  memset(strsendbuffer, 0, sizeof(strsendbuffer));
  memset(strrecvbuffer, 0, sizeof(strrecvbuffer));

  //接受客户端的报文，超时时间也设置为20秒
  if(TcpServer.Read(strrecvbuffer, 20) == false){
    logfile.Write("TcpServer.Read() failed.\n");
    return false;
  }
  //接受成功，把报文打印出来
  logfile.Write("strrecvbuffer = %s\n", strrecvbuffer);

  // 解析客户端登录报文
  _xmltoarg(strrecvbuffer);

  //判断客户端的合法性 如果不是上传或下载就是非法的报文
  if((starg.clienttype != 1) && (starg.clienttype != 2))
    strcpy(strsendbuffer, "failed");
  else
    strcpy(strsendbuffer, "ok");

  // 把报文发回给客户端
  if(TcpServer.Write(strsendbuffer) == false)
  {
    logfile.Write("TcpServer.Write() failed.\n");
    return false;
  }
  // 把登陆的结果记下来
  logfile.Write("%s login %s\n", TcpServer.GetIP(), strsendbuffer);
  return true;
}

//把xml解析到参数starg结构中 在服务端不需要做合法性判断，因为客户端已经判断过了
bool _xmltoarg(char* strxmlbuffer){
  memset(&starg, 0, sizeof(struct st_arg));

  GetXMLBuffer(strxmlbuffer, "clienttype", &starg.clienttype);
  GetXMLBuffer(strxmlbuffer, "ptype", &starg.ptype);
  GetXMLBuffer(strxmlbuffer, "clientpath", starg.clientpath);
  GetXMLBuffer(strxmlbuffer, "andchild", &starg.andchild);
  GetXMLBuffer(strxmlbuffer, "matchname", starg.matchname);
  GetXMLBuffer(strxmlbuffer, "srvpath", starg.srvpath);

  GetXMLBuffer(strxmlbuffer, "timetvl", &starg.timetvl);
  if(starg.timetvl > 30) starg.timetvl = 30; // 扫描本地文件的时间间隔，没有必要超过30秒

  GetXMLBuffer(strxmlbuffer, "timeout", &starg.timeout);
  if(starg.timeout < 50) starg.timeout = 50;// 进程心跳的超时时间，没有必要小于50秒

  GetXMLBuffer(strxmlbuffer, "pname", starg.pname);
  strcat(starg.pname, "_srv");

  return true;
}

// 上传文件主函数
void RecvFilesMain(){
  while(true){
    //初始化接收和发送的buffer
    memset(strsendbuffer, 0, sizeof(strsendbuffer));
    memset(strrecvbuffer, 0, sizeof(strrecvbuffer));
    // 接收客户端的报文，客户端发送的报文只有两种，一种是心跳报文，还有一种是上传文件的请求报文
    if(TcpServer.Read(strrecvbuffer, starg.timetvl + 10) == false){
      logfile.Write("TcpServer.Read() failed.\n");
      return;
    }
    logfile.Write("strrecvbuffer = %s\n", strrecvbuffer);

    // 处理心跳报文，如果是客户端的心跳报文(<activetest>ok</activetest>)就给客户端回复ok
    if(strcmp(strrecvbuffer, "<activetest>ok</activetest>") == 0){
      strcpy(strsendbuffer, "ok");
      // 记录日志，方便调试
      logfile.Write("strsendbuffer = %s\n", strsendbuffer);
      if(TcpServer.Write(strsendbuffer) == false){
        logfile.Write("TcpServer.Write() failed.\n");
        return;
      }
    }

    // 处理上传文件的报文
  }
}