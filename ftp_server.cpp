#include <iostream>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <vector>
#include <fstream>

using namespace std;

#define DATA_SIZE 1024

/*
* written by @kingofools (Surya) and
* @kaustubhhiware as a part of OS Lab
*/

/* FPS server
* run with
* ./server port verbose
*/

string rootDir, controlPort, pwd;
int socketControl, socketData;
bool verbose = false;
extern int errno;
int cs = 0, commsock = 0, ds = 0, datacommsock = 0 ;
extern int optind;
extern char *optarg;

char path_string[1000];
char* root_dir = getcwd(path_string, sizeof(path_string));

int createSocket(string port)
{
    struct addrinfo in, *servinfo, *p;
    int n;
    int sockfd;

    memset(&in, 0 , sizeof in);
    in.ai_family = AF_UNSPEC;
    in.ai_socktype = SOCK_STREAM;
    in.ai_flags = AI_PASSIVE;

    n = getaddrinfo(NULL, port.c_str() ,&in, &servinfo);
    if( n != 0 )
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(n) );

    for( p=servinfo; p != NULL ; p = p->ai_next )
    {

        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if( sockfd == -1 )
            perror("socket");

        if( bind(sockfd, p->ai_addr, p->ai_addrlen) == -1 )
        {
            close(sockfd);
            perror("bind");
            continue;
        }

        break;
    }

    if( p == NULL )
    {
        fprintf(stderr, "Failed to create socket\n" );
        exit(1);
    }

    freeaddrinfo(servinfo);
    return sockfd;
}


void sig_child( int s )
{
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// handle Ctrl+C input
void close_and_exit(int s)
{
    close(cs);
    close(commsock);
    close(ds);
    close(datacommsock);
    exit(200);
}

// need to declare as separate function since converting socket type
string getClientIP(struct sockaddr_storage *addr)
{
    char ip[20];
    string ipstr;

    struct sockaddr_in * sp = (struct sockaddr_in *)addr;
    inet_ntop(AF_INET, &(sp->sin_addr), ip, sizeof ip );
    ipstr = ip;

    return ipstr;
}


//Get port number from the sockaddr_storage to sockaddr_in
unsigned short getPort(struct sockaddr_storage *addr)
{
    unsigned short port;
    struct sockaddr_in * sp = (struct sockaddr_in *) addr;
    port = ntohs(sp->sin_port);

    return port;
}


int recvMsg( int fd, FILE *fp)
{
    unsigned short nwrite, nread=0, length;
    char type, addl;
    char *message, *temp;
    int ret = 0;

    if(fd < 0)
    {
        cout << "Bad file descriptor" << endl;
        return 1;
    }

    recv(fd, &type, 1, 0);
    recv(fd, &addl, 1, 0);
    recv( fd, (unsigned short *) &nwrite, 2 , 0 );
    message = (char *) malloc(sizeof(char) * nwrite);
    length = nwrite;
    temp = message;
    while(nwrite > 0)
    {
        nread = recv( fd, message, nwrite, 0 );
        nwrite -= nread;
        message += nread;
    }
    message = temp;
    if(type == 1 && fp != NULL)
        fwrite(message, 1, length, fp);
    else {
        fwrite(message, 1, length, stdout);
        if(type == 0)
            cout << endl;
    }

    free(message);

    if(verbose){
        cout << "Received " << length << " bytes of data" << endl;
    }

    if(type == 0)
        ret = 1;
    else if(addl == 0 )
        ret =  2;

    return ret;

}


void sendMsg(const char *message, size_t length, int type, int addl, int fd)
{
    size_t i;

    if(fd < 0)
    {
        cout << "Bad file descriptor" << endl;
        return;
    }

    char buff[length + 4];
    buff[0] = (char)type;
    buff[1] = (char)addl;
    *((unsigned short *)&buff[2]) = (unsigned short) length;

    for(i = 0; i < length; i++)
        buff[4+i] = message[i];

    send(fd, buff, length+4, 0);
    if(verbose){
        cout << "sent " << length << " bytes of data" << endl;
    }
}



void fput(string filename, int fd)
{

    FILE *fp;
    int n;
    int index = 0;

    index = filename.rfind("/");
    if(index != string::npos)
    {
        filename = filename.substr(index+1);
    }

    cout << filename << endl;

    if(fd < 0){
        cout << "Bad file descriptor" << endl;
        return;
    }

    fp = fopen(filename.c_str() , "wb");
    if(fp == NULL)
    {
        perror(filename.c_str());
        return;
    }

    do
    {
        n = recvMsg(fd, fp);
    }while(n == 0);

    fclose(fp);
    cout << "Put File Completed" << endl;
}

string strError()
{
    string errMsg = "";
    switch(errno)
    {
        case EBADF:     errMsg = "Bad file descriptor"; break;
        case EINTR:     errMsg = "Operation interrupted"; break;
        case ENOSPC:    errMsg = "No space available on disk"; break;
        case EISDIR:    errMsg = "Is a directory"; break;
        case ENOENT:    errMsg = "No such file"; break;
        default:        errMsg = "Error Occured";
    }
    return errMsg;
}


void fget(string filename, int fd)
{

    char buffer[DATA_SIZE];
    FILE *fp;
    size_t nread=0;
    string errorMsg;

    if( fd < 0)
    {
        cout << "Bad file descriptor" << endl;
        return;
    }

    if(filename[0] == '/')
    {
        filename.replace(0,1, rootDir);
    }

    fp = fopen(filename.c_str() , "rb");
    if( fp == NULL )
    {
        errorMsg = strError();
        errorMsg = filename + ": " + errorMsg;
        cout << errorMsg << endl;
        sendMsg(errorMsg.c_str(), errorMsg.length(), 0, 0, fd);
        return;
    }

    int isValidDir = 1;
    struct stat dir;
    if ( stat( filename.c_str(), &dir ) != 0 )
        isValidDir = 0;

    isValidDir = ( (dir.st_mode & S_IFMT)  == S_IFDIR );
    if(isValidDir)
    {
        errorMsg = "Cannot send a directory";
        cout << errorMsg << endl;
        sendMsg(errorMsg.c_str(), errorMsg.length(), 0, 0, fd);
        fclose(fp);
        return ;
    }

    sendMsg("File downloaded\n", 17, 1, 0, fd);
    do
    {
        nread = fread( buffer, 1, DATA_SIZE, fp );
        if(ferror(fp) != 0)
        {
            errorMsg = strError();
            cout << errorMsg << endl;
            sendMsg(errorMsg.c_str(), errorMsg.length(), 0, 0, fd);
            fclose(fp);
            return;
        }
        if(nread < DATA_SIZE) // implies all data has been read
            sendMsg(buffer, nread, 1, 0, fd);
        else sendMsg(buffer, nread, 1, 1, fd);
    }while(nread == DATA_SIZE);

    fclose(fp);

    cout << "File Sent" << endl;
}


void servls(string command,int fd)
{
    int len;
    string errorMsg;
    FILE *fp;
    char temp[DATA_SIZE];

    if(fd < 0)
    {
        cout << "Bad file descriptor" << endl;
        return;
    }

    cout << "Listing all files and folders" << endl;

    command += " 2>&1";

    fp = popen(command.c_str() , "r");
    if( fp == NULL )
    {
        errorMsg = strError();
        cout << errorMsg << endl;
        sendMsg(errorMsg.c_str() , errorMsg.length(), 0, 0, fd );
        return;
    }

    do
    {
        len = fread(temp, 1, DATA_SIZE, fp );
        if(ferror(fp) != 0){
            errorMsg = strError();
            cout << errorMsg << endl;
            sendMsg(errorMsg.c_str() , errorMsg.length(), 0, 0, fd );
            pclose(fp);
            return;
        }
        if(len < DATA_SIZE) // completed transmission
        {
            sendMsg(temp, len, 1, 0, fd);
        }
        else sendMsg(temp, len, 1, 1, fd);
    }while(len == DATA_SIZE );

    pclose(fp);
}


void servcd(string dirpath, int fd)
{

    string temp = dirpath;
    char t[1024];
    size_t index = 0;
    string errorMsg;

    if(fd < 0)
    {
        cout << "Bad file descriptor" << endl;
        return;
    }

    int ret = chdir(dirpath.c_str());
    if(ret != 0)
    {
        errorMsg = strError();
        cout << errorMsg << endl;
        sendMsg(errorMsg.c_str(), errorMsg.length() , 0, 0, fd );
        return;
    }

    errorMsg = "Changed directory successfully";
    sendMsg(errorMsg.c_str() , errorMsg.length() , 1, 0, fd );
}


void servpwd(int fd)
{
    if( fd < 0 )
    {
        cout << "Bad file descriptor" << endl;
        return;
    }

    root_dir = getcwd(path_string, sizeof(path_string));
    string t(root_dir);
    pwd = t;
    sendMsg(pwd.c_str() , pwd.length(), 1, 0,  fd);
    cout << "servpwd successful" << endl;
}


int main(int argc, char **argv)
{
    string rootdir = root_dir;
    string port = "22";

    if(argc < 2)
    {
        cout << "Need to mention port ! Usage - ./server port";
    }
    else if (argc > 2)
    {
        verbose = true;
        cout << "verbose \n";
    }
    port = argv[1];

    rootDir = rootdir;
    controlPort = port;
    pwd = "";
    socketControl = socketData = 0;

    struct sockaddr_storage otherAddr, otherAddr2;
    socklen_t otherAddrLen, otherAddrLen2;
    int n, i;
    int newfd, newfd2;
    unsigned short dataPort;
    struct sigaction sa;
    char buffer[DATA_SIZE];
    vector<string> tokens;
    string errMsg = "An invalid FTP Command";

    cs = socketControl = createSocket( controlPort);
    ds = socketData = createSocket("0");

    struct sockaddr_storage addr;
    socklen_t len = sizeof addr;
    getsockname(socketData, (struct sockaddr *) &addr, &len );

    struct sockaddr_in *sp = (struct sockaddr_in *) &addr;
    dataPort = ntohs(sp->sin_port);

    if ( listen (socketControl , 10) != 0)
    {
        perror("listen");
        close(socketControl);
        exit(1);
    }

    if( listen(socketData , 10) != 0 )
    {
        perror("listen");
        close(socketData);
        exit(1);
    }

    sa.sa_handler = sig_child;        //read all dead processes
    sigemptyset( &sa.sa_mask );
    sa.sa_flags = SA_RESTART;
    if( sigaction( SIGCHLD, &sa , NULL ) == -1 )
    {
        perror("sigaction");
        exit(1);
    }

    //Handle user interrupts
    signal(SIGINT, close_and_exit);
    cout << "Started server" << endl;

    while(1)
    {
        otherAddrLen = sizeof ( otherAddr) ;
        newfd = accept ( socketControl , (struct sockaddr *) &otherAddr , &otherAddrLen );
        cout << "Connection from " << getClientIP(&otherAddr) << ":" << getPort(&otherAddr) << endl;
        commsock = newfd;

        send(newfd, (unsigned short *)&dataPort, 2, 0);

        otherAddrLen2 = sizeof otherAddr2;
        newfd2 = accept(socketData, (struct sockaddr *) &otherAddr2, &otherAddrLen2 );
        datacommsock = newfd2;
        cout << "Connection to data socket from client port " << getPort(&otherAddr2) << endl;

        if(fork() == 0)
        {
            close(socketControl);
            close(socketData);

            if(verbose)
            {
                cout << "Closing listener sockets" << endl;
                cout << "Child process created for communication with client" << endl;
            }

            if (dup2( newfd , 0 ) != 0 )
            {
                perror("dup2");
                exit(1);
            }

            do
            {
                if(fgets( buffer, DATA_SIZE, stdin ) == NULL)
                {
                    close(newfd);
                    exit(1);
                }

                for(n = 0; n < DATA_SIZE ; n++)
                {
                    if(buffer[n] == '\r' || buffer[n] == '\n')
                    {
                        buffer[n] = 0;
                        break;
                    }
                }

                string s(buffer);
                if(verbose)
                    cout << "Received command " + s << endl;

                istringstream iss(s);

                //copy the tokens into the string vector
                copy( istream_iterator<string>(iss),
                 istream_iterator<string>() ,
                 back_inserter<vector<string> >(tokens) );

                //Copied the tokens into tokens
                n = tokens.size();
                if (n < 1) continue;

                string temp = "";
                if(tokens[0].compare("fput") == 0)
                {
                    for(i = 1; i < n; i++)
                    {
                        temp = temp + tokens[i] + " ";
                    }
                    temp = temp.substr(0, temp.length()-1 );
                    fput(temp, newfd2);
                }
                else if(tokens[0].compare("fget") == 0)
                {
                    for(i = 1; i < n; i++)
                    {
                        temp = temp + tokens[i] + " ";
                    }
                    temp = temp.substr(0, temp.length()-1 );
                    fget(temp, newfd2);
                }
                else if(tokens[0].compare("ls") == 0) // servls
                {
                    servls(s, newfd2);
                }
                else if(tokens[0].compare("servcd") == 0)
                {
                    if(n == 2)
                        servcd(tokens[1], newfd2);
                    else cout << "Need to mention path!" << endl;
                }
                else if( tokens[0].compare("servpwd") == 0 )
                {
                    servpwd(newfd2);
                }
                else if(tokens[0].compare("quit") == 0)
                {
                    break;
                }
                else{
                    sendMsg( errMsg.c_str() , errMsg.length() , 0, 0, newfd2 );
                }

                tokens.clear();

            }while (1);

            close(newfd);
            close(newfd2);
            cout << "Connection closed" << endl;
            exit(1);
        }
        close(newfd);
    }

    close(socketControl);
}
