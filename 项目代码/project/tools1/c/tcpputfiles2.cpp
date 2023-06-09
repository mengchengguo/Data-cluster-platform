/*
 * 程序名：tcpputfiles.cpp，采用tcp协议，实现文件上传的客户端
 * 作者：吴惠明。
*/
#include "_public.h"

CTcpClient TcpClient;
CLogFile logfile;
CPActive PActive; // 进程心跳

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

char strrecvbuffer[1024]; // 发送报文的buffer
char strsendbuffer[1024]; // 接收报文的buffer

// 程序退出和信号2.5的处理函数
void EXIT(int sig);

void _help();

// 把xml解析到参数starg结构中
bool _xmltoarg(char *strxmlbuffer);

bool ActiveTest(); // 心跳

bool Login(const char* argv); // 登录业务

// 文件上传主函数，执行一次文件上传任务
bool _tcpputfiles();

int main(int argc,char *argv[])
{
  if (argc!=3){
    _help(); 
    return -1;
  }

  CloseIOAndSignal(); signal(SIGINT, EXIT); signal(SIGTERM, EXIT);

  // 打开日志文件
  if(logfile.Open(argv[1], "a+") == false) return -1;

  // 解析xml，得到程序运行的参数
  if(_xmltoarg(argv[2]) == false) return -1;

  // PActive.AddPInfo(st_arg.timeout, starg.pname); // 把进程的心跳信息写入共享内存。

  if(TcpClient.ConnectToServer(starg.ip, starg.port) == false){
    logfile.Write("TcpClient.ConnectToServer(%s, %s) failed\n", starg.ip, starg.port);
    EXIT(-1);
  }

  // 登录业务
  if(Login(argv[2]) == false){
    logfile.Write("Login(); failed.\n");
    EXIT(-1);
  }
  
  while(true)
  {
    // 调用文件上传的主函数，执行一次文件上传的任务
    if(_tcpputfiles() == false){ logfile.Write("_tcpputfiles() failed.\n" );  EXIT(-1); }

    sleep(starg.timetvl);

    // 心跳
    if(ActiveTest() == false) break;
  }

  EXIT(0);
}

// 心跳
bool ActiveTest(){
  memset(strsendbuffer, 0, sizeof(strsendbuffer));
  memset(strrecvbuffer, 0, sizeof(strrecvbuffer));

  SPRINTF(strsendbuffer, sizeof(strsendbuffer), "<activetest>ok</activetest>");
  logfile.Write("发送：%s\n", strsendbuffer);
  if(TcpClient.Write(strsendbuffer) == false) return false;

  if(TcpClient.Read(strrecvbuffer, 20) == false) return false;
  logfile.Write("接收：%s\n", strrecvbuffer);

  return true;
}

// 登录业务
bool Login(const char* argv){
  memset(strsendbuffer, 0, sizeof(strsendbuffer));
  memset(strrecvbuffer, 0, sizeof(strrecvbuffer));

  SPRINTF(strsendbuffer, sizeof(strsendbuffer), "%s<clienttype>1</clienttype>", argv);
  logfile.Write("发送：%s\n", strsendbuffer);
  if(TcpClient.Write(strsendbuffer) == false) return false;

  if(TcpClient.Read(strrecvbuffer, 20) == false) return false;
  logfile.Write("接收：%s\n", strrecvbuffer);

  logfile.Write("登录(%s, %d)成功。\n", starg.ip, starg.port);
  return true;
}

void EXIT(int sig){
  logfile.Write("程序退出，sig = %d\n\n", sig);
  exit(0);
}

void _help(){
  printf("\n");
  printf("Using:/project/tools1/bin/tcpputfiles logfilename xmlbuffer\n\n");

  printf("Sample:/project/tools1/bin/procctl 20 /root/project/tools1/bin/tcpputfiles2 /tmp/tcpputfiles_surfdata.log \"<ip>101.43.160.209</ip><port>5005</port><ptype>1</ptype><clientpath>/tmp/tcp/surfdata1</clientpath><clientpathbak>/tmp/tcp/surfdata1bak</clientpathbak><andchild>true</andchild><matchname>*.XML,*.CSV</matchname><srvpath>/tmp/tcp/surfdata2</srvpath><timetvl>10</timetvl><timeout>50</timeout><pname>tcpputfiles_surfdata</pname>\"\n\n\n");

  printf("本程序是数据中心的公共功能模块，采用tcp协议把文件发送给服务器。\n");
  printf("logfilename      本程序运行的日志文件\n");
  printf("xmlbuffer        本程序运行的参数，如下：\n");
  printf("ip               客户端的IP地址\n");
  printf("port             客户端的端口\n");
  printf("ptype            文件上传成功后文件的处理方式1-删除文件，2-移动到备份目录\n");
  printf("clientpath       本地文件存放的根目录\n");
  printf("clientpathbak    文件成功上传后，本地文件备份的根目录，当ptype==2时有效\n");
  printf("andchild         是否上传clientpath目录下各级子目录的文件，true-是；false-否\n");
  printf("matchname        待上传文件名的匹配方式，如\"*.txt,*.XML\",注意大写\n");
  printf("srvpath          服务端文件存放的根目录\n");
  printf("timetvl          扫描本地目录的时间间隔，单位秒\n");
  printf("timeout          进程心跳的超时时间\n");
  printf("pname            进程名，尽可能采用易懂的、与其他进程不同的名称方便故障排查。\n");
}

bool _xmltoarg(char* strxmlbuffer){
  memset(&starg, 0, sizeof(struct st_arg));

  GetXMLBuffer(strxmlbuffer, "ip", starg.ip);
  if(strlen(starg.ip) == 0){logfile.Write("ip is null.\n"); return false;}

  GetXMLBuffer(strxmlbuffer, "port", &starg.port);
  if(starg.port == 0){logfile.Write("port is null.\n"); return false;}

  GetXMLBuffer(strxmlbuffer, "ptype", &starg.ptype);
  if(starg.ptype == 0){logfile.Write("ptype is null.\n"); return false;}

  GetXMLBuffer(strxmlbuffer, "clientpath", starg.clientpath);
  if(strlen(starg.clientpath) == 0){logfile.Write("clientpath is null.\n"); return false;}

  GetXMLBuffer(strxmlbuffer, "clientpathbak", starg.clientpathbak);
  if(strlen(starg.clientpathbak) == 0){logfile.Write("clientpathbak is null.\n"); return false;}

  GetXMLBuffer(strxmlbuffer, "andchild", &starg.andchild);

  GetXMLBuffer(strxmlbuffer, "matchname", starg.matchname);
  if(strlen(starg.matchname) == 0){logfile.Write("matchname is null.\n"); return false;}

  GetXMLBuffer(strxmlbuffer, "srvpath", starg.srvpath);
  if(strlen(starg.srvpath) == 0){logfile.Write("srvpath is null.\n"); return false;}

  GetXMLBuffer(strxmlbuffer, "timetvl", &starg.timetvl);
  if(starg.timetvl == 0){logfile.Write("timetvl is null.\n"); return false;}
  if(starg.timetvl > 30) starg.timetvl = 30; // 扫描本地文件的时间间隔，没有必要超过30秒

  GetXMLBuffer(strxmlbuffer, "timeout", &starg.timeout);
  if(starg.timeout == 0){logfile.Write("timeout is null.\n"); return false;}
  if(starg.timeout < 50) starg.timeout = 50;// 进程心跳的超时时间，没有必要小于50秒

  GetXMLBuffer(strxmlbuffer, "pname", starg.pname);
  if(strlen(starg.pname) == 0){logfile.Write("pname is null.\n"); return false;}

  return true;
}

// 文件上传主函数，执行一次文件上传任务
bool _tcpputfiles(){
  CDir Dir;
  // 调用OpenDir()打开starg.clientpath目录
  //bool OpenDir(const char *in_DirName,const char *in_MatchStr,const unsigned int in_MaxCount=10000,const bool bAndChild=false,bool bSort=false);
  if(Dir.OpenDir(starg.clientpath, starg.matchname, 10000, starg.andchild) == false){
    logfile.Write("Dir.OpenDir(%s) failed.\n", starg.clientpath);
  }

  while(true){
    memset(strsendbuffer, 0, sizeof(strsendbuffer));
    memset(strrecvbuffer, 0, sizeof(strrecvbuffer));

    // 遍历目录中的每个文件，调用ReadDir(获取文件名)
    if(Dir.ReadDir() == false) break;

    // 把文件名、修改时间、文件大小组成报文，发送给对端
    SNPRINTF(strsendbuffer, sizeof(strsendbuffer), 1000, "<filename>%s</filename><mtime>%s</mtime><size>%d</size>", Dir.m_FullFileName, Dir.m_ModifyTime, Dir.m_FileSize);

    logfile.Write("strsendbuffer = %s\n", strsendbuffer);
    if(TcpClient.Write(strsendbuffer) == false)
    {
      logfile.Write("strsendbuffer.Write() failed\n");
      return false;
    }

    // 把文件的内容发送给对端

    // 接收对端的确认报文
    if(TcpClient.Read(strrecvbuffer, 20) == false)
    {
      logfile.Write("TcpClient.Read() failed.\n");  return false;
    }
    logfile.Write("strrecvbuffer = %s\n", strrecvbuffer);

  }
  return true;
}