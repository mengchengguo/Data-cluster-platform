/*
  程序名：dminingmysql.cpp，本程序是数据中心的公共功能模块，用于从mysql数据库源表抽取数据，生成xml文件
  作者：吴惠明
*/
#include"_public.h"
#include"_mysql.h"

struct st_arg{
  char connstr[101];        // 数据库连接参数
  char charset[51];         // 数据库字符集
  char selectsql[1024];     // 从数据源数据库抽取数据的SQL语句
  char fieldstr[501];       // 抽取数据的SQL语句输出结果集字段名，字段名之间用逗号隔开
  char fieldlen[501];       // 抽取数据的SQL语句输出结果集字段的长度，用逗号分割
  char bfilename[31];       // 输出xml文件的前缀
  char efilename[31];       // 输出xml文件的后缀
  char outpath[301];        // 输出xml文件存放的目录
  char starttime[51];       // 程序运行的时间区间
  char incfield[31];        // 递增字段名
  char incfilename[301];    // 已抽取数据的递增字段最大值存放的文件
  int timeout;              // 进程心跳的超时时间
  char pname[51];           // 进程名，建议使用"dminingmysql_后缀"的方式
}starg;

CLogFile logfile;

CPActive PActive; // 进程心跳

// 程序退出和信号2/5的处理函数
void EXIT(int sig);

void _help();

// 把xml解析到参数starg结构体中
bool _xmltoarg(char *strxmlbuff);

// 上传文件功能的主函数
bool _dminingmysql();

int main(int argc, char* argv[]){
  // 把服务器上某个目录的文件全部上传到本地目录（可以指定文件名匹配规则）。
  if(argc != 3){
    _help();
    return -1;
  }

  // 关闭全部的信号和输入输出
  // 设置信号，在shell状态下可用"kill + 进程号"正常终止进程
  // 但请不要用"kill -9 + 进程号"强行终止
  // CloseIOAndSignal();
  signal(SIGINT, EXIT);
  signal(SIGTERM, EXIT);

  // 打开日志文件
  if(logfile.Open(argv[1], "a+") == false){
    printf("打开日志文件失败（%s）\n", argv[1]);
    return -1;
  }

  // 解析xml，得到程序运行的参数
  if(_xmltoarg(argv[2]) == false) return -1;

  PActive.AddPInfo(starg.timeout, starg.pname); // 把进程的心跳信息写入共享内存

  // 上传文件功能的主函数
  _dminingmysql();
  return 0;
}

void EXIT(int sig){
  printf("进程退出，sig = %d\n\n", sig);

  exit(0);
}

void _help(){
    printf("\n");
    printf("Using:/project/tools1/bin/dminingmysql logfilename xmlbuffer\n\n");

    printf("Sample:/project/tools1/bin/procctl 3600 /project/tools1/bin/dminingmysql /log/idc/dminingmysql_ZHOBTCODE.log \
    \"<connstr>127.0.0.1,root,whmhhh1998818,mysql,3306</connstr><charset>utf8</charset><selectsql>Select obtid,cityname,provname,lat,lon,height from T_ZHOBTCODE</selectsql>\
    <fieldstr>obtid,cityname,provname,lat,lon,height</fieldstr><fieldlen>10,30,30,10,10,10</fieldlen><bfilename>ZHOBTCODE</bfilename>\
    <efilename>HYCZ</efilename><outpath>/idcdata/dmindata</outpath>\"\n\n");

    printf("     /project/tools1/bin/procctl 30 /project/tools1/bin/dminingmysql /log/idc/dminingmysql_ZHOBTCODE.log \
    \"<connstr>127.0.0.1,root,whmhhh1998818,mysql,3306</connstr><charset>utf8</charset>\
    <selectsql>Select obtid,date_format(ddatetime,'%%%%Y-%%%%m-%%%%d %%%%H:%%%%m:%%%%s'),t,p,u,wd,wf,r,vis,keyid from t_zhobtmind where keyid>:1 and ddatetime>timestampaddd(minute,-120,now())</selectsql>\")\
    <fieldstr>obtid,ddatetime,t,p,u,wd,wf,r,vis,keyid</fieldstr><fieldlen>10,19,8,8,8,8,8,8,8,15</fieldlen><bfilename>ZHOBTMIND</bfilename>\
    <efilename>HYCZ</efilename><outpath>/idcdata/dmindata</outpath><starttime></starttime><incfield>keyid</incfield><incfilename>/idcdata/dmining/dminingmysql_ZHOBTMIND_HYCZ.list</incfilename>\"\n\n");

    printf("本程序是数据中心的公共功能模块，用于从mysql数据库源表抽取数据，生成xml文件\n");
    printf("logfilename     是本程序的运行的日志文件。\n");
    printf("xmlbuffer       为文件上传的参数，如下：\n");
    printf("connstr         数据库连接参数,格式：ip,username,password,dbname,port。\n");
    printf("charset         数据库的字符集，这个参数要与数据源数据库保持一致，否则会出现中文乱码的现象\n");
    printf("selectsql       从数据源数据库抽取数据的SQL语句，注意：1.时间函数的百分号%需要四个，显示出来才有两个，被prepare后将剩一个\n");
    printf("fieldstr        抽取数据的SQL语句输出结果集字段名，字段名之间用逗号隔开，作为xml文件的字段名\n");
    printf("fieldlen        抽取数据的SQL语句输出结果集字段的长度，中间用逗号隔开。fieldstr与fieldlen的字段必须意义对应\n");
    printf("bfilename       输出xml文件的前缀。\n");
    printf("efilename       输出xml文件的后缀。\n");
    printf("outpath         输出xml文件存放的目录。\n");
    printf("starttime       程序运行的时间区间，例如02,13表示：如果程序启动时，02时和13时则运行，其他时间不运行"\
    "如果starttime为空，那么starttime参数将失效，只要本程序启动就会执行数据抽取，为了减少数据源的压力，从数据库抽取的时候，一般在对方数据库最闲的时候进行。\n");
    printf("incfilename     已抽取数据的递增字段放最大值存放的文件，如果该文件丢失，将重挖全部的数据\n");
    printf("timeout         本程序的超时时间，单位：秒。\n");
    printf("pname           进程名，尽可能采用易懂的、与其他进程不同的名称，方便故障排查。\n\n\n");

}

bool _xmltoarg(char *strxmlbuff){
  memset(&starg, 0, sizeof(struct st_arg));

  GetXMLBuffer(strxmlbuff, "connstr", starg.connstr, 100);
  if(strlen(starg.connstr) == 0){
    logfile.Write("connstr is null.\n");
    return false;
  }

  GetXMLBuffer(strxmlbuff, "charset", starg.charset, 50);
  if(strlen(starg.charset) == 0){
    logfile.Write("charset is null.\n");
    return false;
  }

  GetXMLBuffer(strxmlbuff, "selectsql", starg.selectsql, 1000);
  if(strlen(starg.selectsql) == 0){
    logfile.Write("selectsql is null.\n");
    return false;
  }

  GetXMLBuffer(strxmlbuff, "fieldstr", starg.fieldstr, 500);
  if(strlen(starg.fieldstr) == 0){
    logfile.Write("fieldstr is null.\n");
    return false;
  }

  GetXMLBuffer(strxmlbuff, "fieldlen", starg.fieldlen, 500);
  if(strlen(starg.fieldlen) == 0){
    logfile.Write("fieldlen is null.\n");
    return false;
  }

  GetXMLBuffer(strxmlbuff, "bfilename", starg.bfilename, 30);
  if(strlen(starg.bfilename) == 0){
    logfile.Write("bfilename is null.\n");
    return false;
  }

  GetXMLBuffer(strxmlbuff, "efilename", starg.efilename, 30);
  if(strlen(starg.efilename) == 0){
    logfile.Write("efilename is null.\n");
    return false;
  }

  GetXMLBuffer(strxmlbuff, "outpath", starg.outpath, 300);
  if(strlen(starg.outpath) == 0){
    logfile.Write("outpath is null.\n");
    return false;
  }

  GetXMLBuffer(strxmlbuff, "starttime", starg.starttime, 50); // 可选参数

  GetXMLBuffer(strxmlbuff, "incfield", starg.incfield, 30); // 可选参数

  GetXMLBuffer(strxmlbuff, "incfilename", starg.incfilename, 300); // 可选参数

  GetXMLBuffer(strxmlbuff, "timeout", &starg.timeout);
  if(starg.timeout == 0){
    logfile.Write("timeout is null.\n");
    return false;
  }

  GetXMLBuffer(strxmlbuff, "pname", starg.pname, 50);
  if(strlen(starg.pname) == 0){
    logfile.Write("pname is null.\n");
    return false;
  }

  return true;
}

// 上传文件功能的主函数
bool  _dminingmysql(){
  
}