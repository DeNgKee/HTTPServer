
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <thread>
#include <thread>
#include <queue>
#include <unordered_map>
#include <condition_variable>
#include <sstream>
using std::string;
using std::unordered_map;
using std::cout;
using std::endl;
//using namespace std;
std::mutex mtx;
const int threadNum = 10;
bool threadAvail[threadNum] = {};
struct HTTPRequest {
    string method;
    string URL;
    string version;
    unordered_map<string, string> requestHead;
    string requestContext;
};
struct HTTPRespond {

};

class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount)
        : data_(std::make_shared<data>()) {
        for (size_t i = 0; i < threadCount; ++i) {
            std::thread([data = data_] {
                std::unique_lock<std::mutex> lk(data->mtx);
                for (;;) {
                    if (!data->tasks.empty()) {
                        auto current = std::move(data->tasks.front());
                        data->tasks.pop();
                        lk.unlock();
                        current();
                        lk.lock();
                    }
                    else if (data->isShutdown) {
                        break;
                    }
                    else {
                        std::cout<<"there is no task,"
                        " please wait a sec."<<std::endl;
                        data->cond.wait(lk);
                    }
                }
                }).detach();
        }
    }

    ThreadPool() = default;
    ThreadPool(ThreadPool&&) = default;

    ~ThreadPool() {
        if ((bool)data_) {
            {
                std::lock_guard<std::mutex> lk(data_->mtx);
                data_->isShutdown = true;
            }
            data_->cond.notify_all();
        }
    }

    template <class F>
    void execute(F&& task) {
        {
            std::lock_guard<std::mutex> lk(data_->mtx);
            if (data_->tasks.size() > 0) {
                std::cout<<"there is not enough thread for new client, please wait"<<std::endl;
            }
            data_->tasks.emplace(std::forward<F>(task));
        }
        data_->cond.notify_one();
    }

private:
    struct data {
        std::mutex mtx;
        std::condition_variable cond;
        bool isShutdown = false;
        std::queue<std::function<void()>> tasks;
    };
    std::shared_ptr<data> data_;
};

std::pair<bool, HTTPRequest> HttpMessageParser(string message)
{
    std::pair<bool, HTTPRequest> p;
    HTTPRequest req;
    p.first = false;
    std::stringstream ss(message);
    string line;
    if(std::getline(ss, line)) {
        cout<<line<<endl;
    }
    while (std::getline(ss, line)) {
        if (line == "\r\n"){
            break;
        }
        string key;
        string value;
        std::istringstream iss(line);
        if (!std::getline(iss, key, ':')) {
            cout<<"failed to get request head's key"<<endl;
        }
        if (!std::getline(iss, value, ':')) {
            cout<<"failed to get request head's value"<<endl;
        }
        p.second.requestHead.emplace(key, value);
    }
    if(std::getline(ss, line)) {
        cout<<line<<endl;
    }
    return p;
}
int main()
{
    int _sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (_sock == -1) {
        perror("scoket error!");
        return 1;
    }

    sockaddr_in _sin = {};
    _sin.sin_family = AF_INET;      //协议族IPV4
    _sin.sin_port = htons(4567);    //端口号 host_to_net
    //_sin.sin_addr.s_addr = inet_addr("127.0.0.1");    //服务器绑定的ip地址
    _sin.sin_addr.s_addr= INADDR_ANY; //不限定访问该服务器的ip
    if (bind(_sock, (sockaddr*)&_sin, sizeof(_sin)) == -1) {
        perror("bind error!");
        return 1;
    }

    if (listen(_sock, 5) == -1) { //套接字，最大允许连接数量
        perror("listen error!");
        close(_sock);
        return 1;
    }

    sockaddr_in clientAddr = {};
    socklen_t nAddrLen = sizeof(sockaddr_in);
    int _cSock = -1;
    char msgBuf[] = "Hello, I'm server!";
    char recvBuf[256];
    ThreadPool tp(threadNum);
    while (true) { //循环结构可用于重复接受新的客户端接入
        _cSock = accept(_sock, (sockaddr*)&clientAddr, &nAddrLen); //套接字，收到客户端socket地址，返回socket地址的大小
        if (_cSock == -1) {
            perror("client socket error!");
            close(_sock);
            return -1;
        }

        auto lambda = [=, &recvBuf](){
            printf("new client IP = %s \n", inet_ntoa(clientAddr.sin_addr));
            //send(_cSock, msgBuf, sizeof(msgBuf) + 1, 0); //长度+1，将结尾符一并发送过去
            int nlen = recv(_cSock, recvBuf, 256, 0); //返回接受数据的长度
            if (nlen > 0) {
                //printf("message: %s\n", recvBuf);
                HttpMessageParser(recvBuf);
            } else {
                printf("there is no data\n");
            }
        };
        tp.execute(lambda);
    }
    close(_sock);
    std::cout<<"end"<<std::endl;
    return 0;
}