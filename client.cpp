#include <iostream>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> 
#include <netdb.h>
#include <unistd.h>
#include <string>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <vector>
#include <iterator>

using namespace std;
const int PORTNUMBER = 8080;

void Check(int num, const char* str){
    if (num < 0) perror(str);
}

class SocketAddress {
    struct sockaddr_in addr;
public:
    SocketAddress() {
        addr.sin_family = AF_INET;
        addr.sin_port = htons(PORTNUMBER); 
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }
    SocketAddress(const char* ip, short port){ 
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port); 
        addr.sin_addr.s_addr = inet_addr(ip);
    }
    SocketAddress(unsigned int ip, short port) {
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(ip);
    }
    struct sockaddr* GetAddr() const { return (sockaddr *)&addr;}
    int GetLen() const { return sizeof(addr);}
    ~SocketAddress() {}
};


class Socket{
protected:
    int sd;
    explicit Socket(int _sd) : sd(_sd) {}
public:
    Socket() {
        sd = socket(AF_INET, SOCK_STREAM, 0);
        Check(sd, "socket");
    }
    int GetSd() const { return sd;}
    void Shutdown() { shutdown(sd, 2);}
    ~Socket() { close(sd);}
};

class ConnectedSocket: public Socket {
public:
    ConnectedSocket() = default;
    explicit ConnectedSocket(int cd) : Socket(cd) {}
    void Write(const string& str) {
        cout << "Sending: " <<  str;
        send(sd, str.c_str(), str.length(), 0);
    }
  //  void перегрузка write
    void Write(const vector<uint8_t>& bytes) {
        send(sd, bytes.data(), bytes.size(), 0);
    }
    void Read(string& str) {
        int buflen = 8192;
        char buf[buflen];
        int req = recv(sd, buf, buflen, 0);
        Check(req, "read");
        str = buf;
    }
};


vector<string> SplitLines(string s) { // аналог сишного split'a 
    string meta = "\r\n", token;
    int start = 0, end;
    int len = meta.length();
    vector<string> res;
    while ((end = s.find(meta, start)) > 0) {
        token = s.substr(start, end - start);
        start = end + len;
        res.push_back(token);
    }
    res.push_back(s.substr(start));
    return res;
}

class ClientSocket: public ConnectedSocket {
public:
    void Connect(const SocketAddress& serverAddr) {
        Check(connect(sd, serverAddr.GetAddr(), serverAddr.GetLen()), "connect");
    }
};

class HttpHeader {
    string name;
    string value;
public:
    HttpHeader() = default;
    HttpHeader(const string& n, const string& v) : name(n), value(v) {}
    HttpHeader(const HttpHeader& copy) : name(copy.name), value(copy.value) {}
    string GetName()  const { return name;}
    string GetValue() const { return value;}
    string ToString() const { return (name + value);}
    static HttpHeader ParseHeader(const string& line){
        int i = 0;
        string name, value;
        if (!line.empty()){
            while (line[i] != ' ') {
                name += line[i];
                i++;
            }
            name += '\0';
            
            while (i < line.length()) {
                value += line[i];
                i++;
            }
            value += '\0';
    
        } else {
            name = " "; value = " ";
        }
        HttpHeader temp(name, value);
        return temp;
    }
};

class HttpRequest {
    vector<string> _lines;
public:
    HttpRequest() {
       // _lines.push_back("GET /qwer.txt HTTP/1.1\r\0"); // не хватало HTTP/1.1
       // _lines = {"GET /qwer.txt"};
       _lines.push_back("GET /cgi-bin/cgi?Name=R&Surname=I&TG=RI HTTP/1.1\r\0");
    }
    string ToString() const {
        string res;
        for (int i = 0; i < _lines.size(); i++) res += _lines[i];
        return res;
    };
};

class HttpResponse {
    HttpHeader response;
    HttpHeader* other;
    string body;
    int len;
public:
    HttpResponse(vector<string> lines) {
        response = HttpHeader::ParseHeader(lines[0]);
        other = new HttpHeader[lines.size() - 1];
        int i;
        for (i = 1; i < lines.size(); i++) {
            other[i - 1] = HttpHeader::ParseHeader(lines[i]);
            if ((lines[i]).empty()) {
                body = lines[i + 1];
                break;
            }
        }
        len = i;
    }
    void PrintResponse() const {
        cout << response.ToString() << endl;
        int j = 0;
        while (j < len) {
            cout << (other[j]).GetName() << (other[j]).GetValue();
            j++;
        }
        cout << body << endl;
    }
    ~HttpResponse() {
        delete[] other;
    }
};

void ClientConnection() {
    ClientSocket cs;
    SocketAddress saddr;
    cs.Connect(saddr);
    HttpRequest rq;
    string req = rq.ToString();
    cout << endl;
    cs.Write(req);
    cout << endl;
    vector<string> lines;
    string response, temp;
    for (int i = 0; i < 3; i++){
        cs.Read(response);
        temp += response;
    }
    lines = SplitLines(temp);
    HttpResponse resp(lines);
    resp.PrintResponse();
    cs.Shutdown();
}

int main(){
    ClientConnection();
    return 0;
}
