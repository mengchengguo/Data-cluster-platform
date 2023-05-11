/*
  程序名：syncupdate.cpp，本程序是数据中心的公共功能模块，采用刷新的方式同步MySQL数据库之间的表
  作者：吴惠明
*/
#include "_tools.h"

struct st_arg{
    char localconnstr[101];   // 本地数据库的连接参数
    char charset[51];         // 数据库的字符集
    char fedtname[31];        // Federate表名
    char localtname[31];      // 本地表名
    char remotecols[1001];    // 远程表的字段列表
    char localcols[1001];     // 本地表的字段列表
    char where[1001];         // 同步数据的条件
    int synctype;             // 同步方式：1-不分批同步，2-分批同步
    char remoteconnstr[101];  // 远程数据库的连接参数
    char remotetname[31];     // 远程表名
    char remotekeycol[31];    // 远程表的键值字段名
    char localkeycol[31];     // 本地表的键值字段名
    int maxcount;             // 每批执行一次同步操作的记录数
    int timeout;              // 本程序运行时的超时时间
    char pname[51];           // 本程序运行时的进程名
}starg;

// 显示程序的帮助
void _help(char* argv[]);

// 把xml解析到参数starg中
bool _xmltoarg(char* strxmlbuff);

CLogFile logfile;

connection connloc;  // 本地数据库的连接
connection connrem;  // 远程数据库的连接

// 业务处理主函数
bool _syncupdate();

void EXIT(int sig);

CPActive PActive;

int main(int argc, char* argv[]){
    if (argc!=3) { _help(argv); return -1; }
    // 关闭全部的信号和输入输出，处理程序退出的信号。
    //CloseIOAndSignal();
    signal(SIGINT,EXIT); signal(SIGTERM,EXIT);

    if (logfile.Open(argv[1],"a+")==false){
        printf("打开日志文件失败（%s）。\n",argv[1]); return -1;
    }

    // 把xml解析到参数starg结构中
    if (_xmltoarg(argv[2])==false) return -1;

    // PActive.AddPInfo(starg.timeout,starg.pname);
    // 注意，在调试程序的时候，可以启用类似以下的代码，防止超时。
    // PActive.AddPInfo(starg.timeout*100,starg.pname);

    if(connloc.connecttodb(starg.localconnstr, starg.charset) != 0){
        logfile.Write("connect database(%s) failed.\n%s\n",starg.localconnstr,connloc.m_cda.message); EXIT(-1);
    }
    logfile.Write("connect database(%s) ok.\n",starg.localconnstr);

    _syncupdate();
    return 0;
}

// 显示程序的帮助
void _help(char* argv[]){
    printf("Using:/project/tools1/bin/syncupdate logfilename xmlbuffer\n\n");

    printf("Sample:/project/tools1/bin/procctl 10 /project/tools1/bin/syncupdate /log/idc/syncupdate_ZHOBTCODE2.log \"<localconnstr>172.29.193.250,root,whmhhh1998818,mysql,3306</localconnstr><charset>utf8</charset><fedtname>LK_ZHOBTCODE1</fedtname><localtname>T_ZHOBTCODE2</localtname><remotecols>obtid,cityname,provname,lat,lon,height/10,upttime,keyid</remotecols><localcols>stid,cityname,provname,lat,lon,altitude,upttime,keyid</localcols><synctype>1</synctype><timeout>50</timeout><pname>syncupdate_ZHOBTCODE2</pname>\"\n\n");

    printf("本程序是数据中心的公共功能模块，采用刷新的方式同步MySQL数据库之间的表。\n");

    printf("logfilename  本程序运行的日志文件。\n");
    printf("xmlbuffer    本程序运行的参数。用xml表示，具体如下：\n");
    printf("localconnstr 本地数据库的连接参数，格式：ip, username, password, dbname, port。\n");
    printf("charset      数据库的字符串，这个参数要与远程数据库保持一致，否则会出现中文乱码的状况。\n");
    printf("fedtname     Federated表名。\n");
    printf("localtname   本地表名。\n");
    printf("remotecols   远程表的字段列表，用于填充在select和from之间，所以，remotecols可以是真实的字段，也可以是函数返回值和运算结果。如果本参数为空，就用localtime表的字段列表填充。\n");
    printf("localcols    本地表的字段列表，与remotecols不同，他必须是真实存在的字段。如果本参数为空，就用localtname表的字段列表填充。\n");
    printf("where        同步数据的条件，即select语句的where部分。\n");
    printf("synctype     同步方式：1-不分批同步；2-分批同步\n");
    printf("remoteconnstr远程数据库的连接参数，格式与localconnstr相同，当synctype == 2时有效。\n");
    printf("remotetname  远程表名，当synctype == 2时有效。\n");
    printf("remotekeycol 远程表的键值字段名，必须是唯一的，当synctype == 2时有效。\n");
    printf("localkeycol  本地表的键值字段名，必须是唯一的，当synctype == 2时有效。\n");

    printf("maxcount     每批执行一次同步操作的记录数，不能超过MAXPARAMS宏（在_mysql.h中定义），当synctype==2时有效。\n");

    printf("timeout      本程序的超时时间，单位：秒，视数据的大小而定，建议设置20以上。\n");
    printf("pname        进程名，尽可能采用易懂的、与其他进程不同的名称，方便故障排查。\n\n");
    printf("注意：1）remotekeycol和localkeycol的字段选取很重要，如果采用了mysql的自增字段，那么在远程表中数据生成后自增字段的值不可改变，否则同步会失败；\
    2）当远程表中存在delete操作时，无法分批同步，因为远程表的记录被delete后就找不到了，无法从本地表中执行delete操作。\n\n\n");
}

// 把xml解析到参数starg结构中
bool _xmltoarg(char *strxmlbuffer){
  memset(&starg,0,sizeof(struct st_arg));

  // 本地数据库的连接参数，格式：ip,username,password,dbname,port。
  GetXMLBuffer(strxmlbuffer,"localconnstr",starg.localconnstr,100);
  if (strlen(starg.localconnstr)==0) { logfile.Write("localconnstr is null.\n"); return false; }

  // 数据库的字符集，这个参数要与远程数据库保持一致，否则会出现中文乱码的情况。
  GetXMLBuffer(strxmlbuffer,"charset",starg.charset,50);
  if (strlen(starg.charset)==0) { logfile.Write("charset is null.\n"); return false; }

  // Federated表名。
  GetXMLBuffer(strxmlbuffer,"fedtname",starg.fedtname,30);
  if (strlen(starg.fedtname)==0) { logfile.Write("fedtname is null.\n"); return false; }

  // 本地表名。
  GetXMLBuffer(strxmlbuffer,"localtname",starg.localtname,30);
  if (strlen(starg.localtname)==0) { logfile.Write("localtname is null.\n"); return false; }

  // 远程表的字段列表，用于填充在select和from之间，所以，remotecols可以是真实的字段，也可以是函数
  // 的返回值或者运算结果。如果本参数为空，就用localtname表的字段列表填充。\n");
  GetXMLBuffer(strxmlbuffer,"remotecols",starg.remotecols,1000);

  // 本地表的字段列表，与remotecols不同，它必须是真实存在的字段。如果本参数为空，就用localtname表的字段列表填充。
  GetXMLBuffer(strxmlbuffer,"localcols",starg.localcols,1000);

  // 同步数据的条件，即select语句的where部分。
  GetXMLBuffer(strxmlbuffer,"where",starg.where,1000);

  // 同步方式：1-不分批同步；2-分批同步。
  GetXMLBuffer(strxmlbuffer,"synctype",&starg.synctype);
  if ( (starg.synctype!=1) && (starg.synctype!=2) ) { logfile.Write("synctype is not in (1,2).\n"); return false; }

  if (starg.synctype==2)
  {
    // 远程数据库的连接参数，格式与localconnstr相同，当synctype==2时有效。
    GetXMLBuffer(strxmlbuffer,"remoteconnstr",starg.remoteconnstr,100);
    if (strlen(starg.remoteconnstr)==0) { logfile.Write("remoteconnstr is null.\n"); return false; }

    // 远程表名，当synctype==2时有效。
    GetXMLBuffer(strxmlbuffer,"remotetname",starg.remotetname,30);
    if (strlen(starg.remotetname)==0) { logfile.Write("remotetname is null.\n"); return false; }

    // 远程表的键值字段名，必须是唯一的，当synctype==2时有效。
    GetXMLBuffer(strxmlbuffer,"remotekeycol",starg.remotekeycol,30);
    if (strlen(starg.remotekeycol)==0) { logfile.Write("remotekeycol is null.\n"); return false; }

    // 本地表的键值字段名，必须是唯一的，当synctype==2时有效。
    GetXMLBuffer(strxmlbuffer,"localkeycol",starg.localkeycol,30);
    if (strlen(starg.localkeycol)==0) { logfile.Write("localkeycol is null.\n"); return false; }

    // 每批执行一次同步操作的记录数，不能超过MAXPARAMS宏（在_mysql.h中定义），当synctype==2时有效。
    GetXMLBuffer(strxmlbuffer,"maxcount",&starg.maxcount);
    if (starg.maxcount==0) { logfile.Write("maxcount is null.\n"); return false; }
    if (starg.maxcount>MAXPARAMS) starg.maxcount=MAXPARAMS;
  }

  // 本程序的超时时间，单位：秒，视数据量的大小而定，建议设置30以上。
  GetXMLBuffer(strxmlbuffer,"timeout",&starg.timeout);
  if (starg.timeout==0) { logfile.Write("timeout is null.\n"); return false; }

  // 本程序运行时的进程名，尽可能采用易懂的、与其它进程不同的名称，方便故障排查。
  GetXMLBuffer(strxmlbuffer,"pname",starg.pname,50);
  if (strlen(starg.pname)==0) { logfile.Write("pname is null.\n"); return false; }

  return true;
}

void EXIT(int sig){
  logfile.Write("程序退出，sig=%d\n\n",sig);

  connloc.disconnect();

  connrem.disconnect();

  exit(0);
}

// 业务处理主函数。
bool _syncupdate()
{





  return true;
}