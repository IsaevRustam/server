#include <iostream>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> 
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cerrno>

#include <string>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <vector>
#include <iterator>

using namespace std;
const int PORTNUMB = 8080;
const int BACKLOG = 5;

#define REQUEST         "REQUEST="
#define SERVER_ADDR     "SERVER_ADDR=127.0.0.1"
#define CONTENT_TYPE    "CONTENT_TYPE=text/plain"
#define SERVER_PROTOCOL "SERVER_PROTOCOL=HTTP/1.0"
#define SCRIPT_NAME     "SCRIPT_NAME="
#define SERVER_PORT     "SERVER_PORT=8080"
#define QUERY_STRING    "QUERY_STRING="

void Check(int num, const char* str){
    if (num < 0) perror(str);
}

class Socket{
protected:
    int sd;
    explicit Socket(int _sd) : sd(_sd) {} // с консультации: предотвращает неявное преобразование типов при инициализации
public:
    Socket() {
        sd = socket(AF_INET, SOCK_STREAM, 0); // по методичке Столярова
        Check(sd, "socket");
    }
    int GetSd() const { return sd;}
    void Shutdown() { shutdown(sd, 2);}
    ~Socket() { close(sd);}
}; //++

class SocketAddress {
    struct sockaddr_in addr;
public:
    SocketAddress() {   // по методичке Столярова
        addr.sin_family = AF_INET;   
        addr.sin_port = htons(PORTNUMB); 
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }
    SocketAddress(const char* ip, short port){   // по методичке Столярова
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port); 
        addr.sin_addr.s_addr = inet_addr(ip);
    }
    SocketAddress(unsigned int ip, short port) {  // по методичке Столярова
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(ip);
    }
    struct sockaddr* GetAddr() const { return (sockaddr *)&addr;}
    int GetLen() const { return sizeof(addr);}
    ~SocketAddress() {}
}; //++

class ServerSocket: public Socket {
public:
    void Bind(const SocketAddress& sock) {  // сначала был написан переделанный бинд, но оказалось, что есть библиотечный
        Check(bind(sd, sock.GetAddr(), sock.GetLen()), "bind");
    }
    void Listen(int backlog){   // для ожидания сокетов
        Check(listen(sd, backlog), "listen");
    }
    int Accept(SocketAddress& sock) {  //  используется сервером для принятия связи на сокет, сокет должен быть уже слушающим на момент асепта
        size_t len = sizeof(sock);
        int acc = accept(sd, sock.GetAddr(), (socklen_t *)&len);
        Check(acc, "accept");
        return acc;
    }
}; //++

vector<uint8_t> FATV(int fd) {
    vector<uint8_t> a; char c;
    while(read(fd, &c, 1)) a.push_back(c);
    return a;
}

class ConnectedSocket: public Socket {
public:
    ConnectedSocket() : Socket() {}
    explicit ConnectedSocket(int cd) : Socket(cd) {} // почти под копирку с консультации 
    ConnectedSocket& operator<<(const string& str) {
        send(sd, str.c_str(), str.length(), 0);
        return *this;
    }
    ConnectedSocket& operator<<(const vector<uint8_t>& bytes) {
        send(sd, bytes.data(), bytes.size(), 0);
        return *this;
    }
    void operator>>(string& str) {
        int buflen = 1024;
        char buf[buflen];
        int req = recv(sd, buf, buflen, 0);
        Check(req, "read");
        str = buf;
    }
};

vector<string> SplitLines(string s) {
    string meta = "\r\n", token;
    int start = 0, end, len = meta.length();
    vector<string> res;
    while ((end = s.find(meta, start)) > 0) {
        token = s.substr(start, end - start);
        start = end + len;
        res.push_back(token);
    }
    res.push_back(s.substr(start));
    return res;
} //++

bool IsCgi(string str) { return !(str.find('?') == -1);}

string GetPath(string from) {
    int i = 4;
    string res = "index/";
    if (from[i] == ' '){
        i++;
    } 
    while (from[i] != ' '){
        res += from[i++];
    }
    return res;
}

string GetFileName(string path) {
    string temp;
    int i = 0;
    while(path[i] != '?') {
        temp += path[i];
        i++;
    }
    return temp;
}

string GetQuery(string path) {
    string temp;
    int i = GetFileName(path).length() + 1;
    while(i != path.length()) {
        temp += path[i];
        i++;
    }
    return temp;
}

void WhatError(ConnectedSocket cs) {
    string str;
    switch (errno){
        case EACCES:
            str = "HTTP/1.1 403 Forbidden\n";
            break;
        case ENETRESET:
            str = "HTTP/1.1 503 Service Unavailable\n";
            break;
        default:
            str = "HTTP/1.1 404 Not Found\n";
            break;
    }
    cout << str;
    cs << str;
}


char** FillEnv(string filename, string query, string request) {
    char** env = new char*[8];
    env[0] = new char[request.size()];
    env[1] = new char[22];              //SERVER_ADDR
    env[2] = new char[24];              //CONTENT_TYPE
    env[3] = new char[25];              //SERVER_PROTOCOL
    env[4] = new char[13 + filename.size()]; //SCRIPT_NAME
    env[5] = new char[17];              //SERVER_PORT
    env[6] = new char[14 + query.size()]; //QUERY_STRING
    env[0] = (char *) request.c_str();
    env[1] = (char *) SERVER_ADDR;
    env[2] = (char *) CONTENT_TYPE;
    env[3] = (char *) SERVER_PROTOCOL;
    strcpy(env[4], SCRIPT_NAME);
    strcat(env[4], filename.c_str());
    env[5] = (char *) SERVER_PORT;
    strcpy(env[6], QUERY_STRING);
    strcat(env[6], query.c_str());
    env[7] = NULL;
    return env;
}


void CgiHandler(ConnectedSocket cs, string path, string request) {
    int fd;
    int status;
    pid_t pid = fork();
    Check(pid, "fork");
    if (pid > 0) {
        wait(&status);
        if(WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            fd = open("log.txt", O_RDONLY);
            vector<uint8_t> vect = FATV(fd);
            cout << "Server sends: HTTP/1.1 200 OK" << endl;
            cs << "HTTP/1.1 200 OK\0"; 
            string str;
            str += "\r\nVersion: HTTP/1.1\r\nContent-type: text/html\r\nContent-length: " + to_string(vect.size()) + "\r\n\r\n";
            cout << "Version: HTTP/1.1" << "\n";
            cout << "Content-length: " << to_string(vect.size()) << "\n";
            cs << str;
            cs << vect;
            cs.Shutdown();
            close(fd);
        } 
    } else if (pid == 0){
        fd = open("log.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        Check(fd, "file");
        dup2(fd, 1);
        close(fd);
        string filename = GetFileName(path);
        string query = GetQuery(path);
        char* argv[] = { (char*)filename.c_str(), NULL};
        char** env = FillEnv(filename, query, request);
        execve(filename.c_str(), argv, env);
        WhatError(cs);
        perror("exec");
        exit(1);
    }
}

void NotCgiHandler(ConnectedSocket cs, string path) {
    int fd;
    if (path == "index//") { fd = open("index/index.html", O_RDONLY);}
    else { fd = open(path.c_str(), O_RDONLY);}
    if (fd < 0) {
        cout << "Server sends: HTTP/1.1 404 Not Found" << endl;
        cs << "HTTP/1.1 404 Not Found\0";
        fd = open("index/404.html", O_RDONLY);
        Check(fd, "fd404");
    } else {
        cout << "Server sends: HTTP/1.1 200 OK" << endl;
        cs << "HTTP/1.1 200 OK\0";
    }
    vector<uint8_t> vect = FATV(fd);
    string str;
    str += "\r\nVersion: HTTP/1.1\r\nContent-length: " + to_string(vect.size()) + "\r\n\r\n";
    cout << "Version: HTTP/1.1" << endl;
    cout << "Content-length: " << to_string(vect.size())<< endl;
    cs << str;
    cs << vect;
    cs.Shutdown();
    close(fd);
}


void ProcessConnection(int cd, const SocketAddress& clAddr) {
    ConnectedSocket cs(cd);
    string request;
    cs >> request;
    vector<string> lines = SplitLines(request);
    cout << "Client sent:" << lines[0] << endl;
    string path = GetPath(lines[0]);
    cout << "Path: " << path << endl;
    if (IsCgi(path)) {
        CgiHandler(cs, path, request);
    } else NotCgiHandler(cs, path);  
}


void TURNON() {
    SocketAddress serverAddr;
    ServerSocket ss;
    ss.Bind(serverAddr);
    ss.Listen(BACKLOG);
    cout << "\nPlease connect to 127.0.0.1:8080\n" << endl;
    while(1) {
        SocketAddress clAddr;
        ProcessConnection(ss.Accept(clAddr), clAddr);
        cout << endl;
    }
}
int main(){
    TURNON();
    return 0;
}
