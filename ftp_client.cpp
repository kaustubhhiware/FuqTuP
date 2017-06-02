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
#define BUFFER_SIZE 1028

/*
* 14CS30011 : Hiware Kaustubh Narendra
* 14CS30017 : Surya Midatala
*/

/* FPS client
* run with
* ./client server_ip port
*/

int socketControl = 0, socketData = 0;
string server_ip, server_port;
extern int errno;

int createSocket(string port)
{
    addrinfo in, *servinfo, *i;
    int sock, sockfd;

    memset(&in, 0, sizeof in);
    in.ai_family = AF_UNSPEC;
    in.ai_socktype = SOCK_STREAM;

    sock = getaddrinfo(server_ip.c_str(), port.c_str(), &in, &servinfo);
    if(sock != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(sock) );
        exit(1);
    }

    for(i = servinfo; i != NULL; i = i->ai_next)
    {
        sockfd = socket(i->ai_family, i->ai_socktype, i->ai_protocol);
        if(sockfd == -1)
            perror("socket");
        int conn = connect(sockfd, i->ai_addr, i->ai_addrlen);
        if(conn == -1)
        {
            close(sockfd);
            perror("connect");
            continue;
        }

        break;
    }

    if(i == NULL)
    {
        fprintf(stderr, "Failed to create socket\n");
        exit(1);
    }

    freeaddrinfo(servinfo);
    return sockfd;

}


int recvMsg(FILE *fp)
{
    unsigned short nwrite, nread=0, length;
    char *message, *temp;
    char type, addl;
    int ret = 0;

    recv(socketData, &type, 1, 0);
    recv(socketData, &addl, 1, 0);
    recv( socketData, (unsigned short *) &nwrite, 2 , 0 );
    message = (char *)malloc(sizeof(char) * nwrite);
    temp = message;
    length = nwrite;
    while(nwrite > 0)
    {
        nread = recv( socketData, message, nwrite, 0 );
        nwrite -= nread;
        message += nread;
    }

    message = temp;
    if(type == 1 && fp != NULL)
        fwrite(message, 1, length, fp);
    else
    {
        fwrite(message, 1, length, stdout);
        if(type == 0)
            cout << endl;
    }

    free(message);

    if(type == 0)
        ret = 1;
    else if(addl == 0 )
        ret =  2;

    return ret;

}

// fget
void getFile(string file)
{

    FILE *fp;
    int msg, i = 0;
    string filename;

    i = file.rfind("/");
    filename = file;
    if(i != string::npos)
    {
        filename = file.substr(i + 1);
    }

    if(socketData < 0)
    {
        cout << "Bad file descriptor" << endl;
        return;
    }

    string command = "fget " + file + "\n";
    send(socketControl, command.c_str(), command.length() , 0);
    msg = recvMsg(NULL);
    if(msg == 1)
        return;

    fp = fopen(filename.c_str() , "wb");
    if(fp == NULL)
    {
        perror("fopen");
        return;
    }

    do
    {
        msg = recvMsg(fp);
    }while(msg == 0);

    fclose(fp);
}

// Send message to the server through the socketData
void sendMsg(const char * message, size_t l, int type , int addl)
{
    size_t i;

    if(socketData < 0)
    {
        cout << "Bad file descriptor" << endl;
        return;
    }

    char buff[l + 4];
    buff[0] = (char)type;
    buff[1] = (char)addl;
    *((unsigned short *)&buff[2]) = (unsigned short) l;
    for(i = 0; i < l; i++)
        buff[i+4] = message[i];

    send(socketData, buff, l+4, 0);
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

//Put the file in the server
void fput(string filename)
{

    FILE *fp;
    char temp[DATA_SIZE];
    size_t nread;
    string message;

    if(socketData < 0)
    {
        cout << "Bad file descriptor" << endl;
        return;
    }

    fp = fopen(filename.c_str() , "rb");
    if( fp == NULL )
    {
        perror("fopen");
        return;
    }

    int isValidDir = 1;
    struct stat dir;
    if ( stat( filename.c_str(), &dir ) != 0 )
        isValidDir = 0;

    isValidDir = ( (dir.st_mode & S_IFMT)  == S_IFDIR );
    if(isValidDir)
    {
        fclose(fp);
        cout << "Cannot put a complete directory" << endl;
        return;
    }

    string command = "fput " + filename + "\n" ;
    send(socketControl, command.c_str(), command.length(), 0);
    do
    {
        nread = fread(temp, 1, DATA_SIZE, fp);
        if(ferror(fp) != 0)
        {
            command = strError();
            sendMsg(command.c_str(), command.length() , 0, 0 );
            fclose(fp);
            return;
        }

        int ret = 1;
        if(nread < DATA_SIZE)
            ret = 0;

        sendMsg(temp, nread, 1,ret);
    }while(nread == DATA_SIZE);

    fclose(fp);

}

// List directories of server
void servls(string command)
{
    int x;

    if(socketData < 0)
    {
        cout << "Bad file descriptor" << endl;
        return;
    }

    command = "ls\n";
    send(socketControl,command.c_str() , command.length() , 0);
    do
    {
        x = recvMsg(NULL);
    }while(x == 0);

}


//Change directory of server
void servcd (string dirpath)
{
    int x;

    if(socketData < 0)
    {
        cout << "Bad file descriptor" <<endl;
        return;
    }

    int isValidDir = 1;
    struct stat dir;
    if ( stat( dirpath.c_str(), &dir ) != 0 )
        isValidDir = 0;

    isValidDir = ( (dir.st_mode & S_IFMT)  == S_IFDIR );
    if(!isValidDir)
    {
        cout << "No such directory" << endl;
        return;
    }

    string command = "servcd " + dirpath + "\n";
    send(socketControl, command.c_str(), command.length(),0 );
    do
    {
        x = recvMsg(NULL);
    }while(x == 0);

    if(x == 2)
        cout << endl;
}

//PWD of server
void servpwd()
{
    int x;

    if(socketData < 0)
    {
        cout << "Bad file descriptor" << endl;
        return;
    }

    send(socketControl, "servpwd\n", 8, 0);
    do
    {
        x = recvMsg(NULL);
    }while(x == 0);

    if(x == 2)
        cout << endl;
}

//List directories client-side
void clils(string command)
{

    char temp[1024];
    command = "ls 2>&1";

    FILE *fp = popen(command.c_str() , "r");
    if(fp == NULL)
    {
        perror("popen");
        return;
    }

    while(fgets(temp,1024,fp) != NULL)
    {
        printf("%s", temp);
    }

    fclose(fp);
}

// change directory functionality
void clicd(string path)
{
    int ret = chdir(path.c_str());
    if(ret!=0)
    {
        perror("+--- Error in clicd ");
    }

}

//PWD of client
void clipwd()
{
    char temp[1024];
    if( getcwd(temp, 1024) == NULL )
    {
        perror("+--- Error in getcwd() : ");
        return;
    }

    string s(temp);
    cout << temp << endl;

}

int main(int argc, char **argv)
{

    string hostname = "localhost", port = "22";
    if(argc < 3)
    {
        cout << "+--- Incorrect usage! Need to run ./client server_ip port\n";
        exit(0);
    }
    hostname = argv[1];
    port = argv[2];

    server_ip = hostname;
    server_port = port;

    int n, dataport, i;
    char temp[128], dataport_string[6];

    vector<string> tokens;
    socketControl = createSocket(server_port);

    recv(socketControl, (unsigned short *) &dataport, 2, 0);
    sprintf(dataport_string, "%hu", (unsigned short)dataport);
    string dpStr(dataport_string);

    socketData = createSocket(dpStr);
    cout << "Connected to dataport "<< endl;

    while(1)
    {
        cout << "ftp>";
        fgets(temp, 128, stdin);

        string input(temp);
        istringstream iss(input);
        tokens.clear();

        copy( istream_iterator<string>(iss), istream_iterator<string>(),
        back_inserter<vector<string> >(tokens) );

        n = tokens.size();
        if(n < 1) continue;

        string temp = "";
        if(tokens[0].compare("fput") == 0)
        {
            for(i=1; i < n; i++)
            {
                temp = temp + tokens[i] + " ";
            }
            temp = temp.substr(0, temp.length()-1 );
           fput(temp);
        }
        else if(tokens[0].compare("fget") == 0)
        {
            for(i=1; i < n; i++)
            {
                temp = temp + tokens[i] + " ";
            }
            temp = temp.substr(0, temp.length()-1 );
            getFile(temp);
        }
        else if(tokens[0].compare("servls") == 0)
        {
           servls(input);
        }
        else if(tokens[0].compare("servcd") == 0)
        {
            if(n == 2)
                servcd(tokens[1]);
            else cout << "Need to mention path!" << endl;
        }
        else if(tokens[0].compare("servpwd") == 0)
        {
           servpwd();
        }
        else if(tokens[0].compare("clils") == 0)
        {
           clils(input.substr(1));
        }
        else if(tokens[0].compare("clicd") == 0)
        {
            if(n == 2)
               clicd(tokens[1]);
        }
        else if(tokens[0].compare("clipwd") == 0)
        {
           clipwd();
        }
        else if(tokens[0].compare("quit") == 0 || tokens[0].compare("z") == 0)
        {
            cout << "Goodbye!" << endl;
            send(socketControl, "quit\n", 5, 0);
            close(socketControl);
            exit(1);
        }
        else if(tokens[0].compare("clear") == 0)
        {
            const char* blank = "\e[1;1H\e[2J";
            write(STDOUT_FILENO,blank,12);
        }
        else cout << "An invalid FTP Command" << endl;

    }
}
