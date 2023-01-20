#include <mutex>
#include <signal.h>
#include <semaphore.h>
#include <thread>
#include <vector>
#include <setjmp.h>
#include "message.h"
#include "clientlist.h"

#define SEVPORT 3936
ClientList cList = ClientList();
bool serverExit = false;
std::vector<std::thread> childThreads;
int sevSockfd;
static jmp_buf jmpbuf;
using std::cout;

void sendTime(int sockfd) {
    Message msg = Message();
    time_t cur_time;
    cur_time = time(&cur_time);
    msg.setMessage(Type::Time, 0, string(ctime(&cur_time)));
    msg.sendMessage(sockfd);
}

void sendName(int sockfd) {
    Message msg = Message();
    char hostname[20];
    gethostname(hostname, 20);
    string s = string(hostname, strlen(hostname));    
    msg.setMessage(Type::Name, 0, s);
    msg.sendMessage(sockfd);
}

void sendList(int sockfd) {
    Message msg = Message();
    uint32_t dataLen = cList.serialClientInfos(msg.data);
    msg.type = Type::List;
    msg.clientId = 0;
    msg.length = dataLen + sizeof(Type) + 2*sizeof(uint32_t); // fix bug
    msg.sendMessage(sockfd);
}

int transMessage(uint32_t srcClientId, Message& revMsg) {
    uint32_t desClientId = revMsg.clientId;
    cout << PROMPT << "Receive message from client with id[" << srcClientId << "]" << endl; 
    cout << PROMPT << "Sending message to client with id[" << desClientId << "]" << endl;
    int sockfd = cList.getSockfd(desClientId);
    if(sockfd == -1) return -1;
    revMsg.clientId = srcClientId; // change to source clientId
    revMsg.sendMessage(sockfd); // send to destination clientId
    return 0;
}

void replySendStatus(int sockfd, string errInfo) {
    Message msg = Message();
    msg.setMessage(Type::ReplySendStatus, 0, errInfo);
    msg.sendMessage(sockfd);
}

void ackDisconnect(int sockfd) {
    Message msg = Message();
    msg.setMessage(Type::Disconnect, 0, "");
    msg.sendMessage(sockfd);
}

void synServerExit(int sockfd) {
    Message msg = Message();
    msg.setMessage(Type::ServerExit, 0, "");
    msg.sendMessage(sockfd);
}

void checkExit() {
    string s;
    printPrompt("Enter \"quit\" to exit");
    while(cin >> s) {
        if(s == "quit") {
            raise(SIGQUIT);
        }
    }
}

void handleServerExit(int signum) {
    map<uint32_t, ClientInfo> clientList = cList.getClientList();
    for(auto client : clientList) {
        int sockfd = cList.getSockfd(client.first);
        synServerExit(sockfd);
    }

    serverExit = true;
    auto it = childThreads.begin();
    for(; it != childThreads.end(); it++) {
        it->join();
    }
    printPrompt("All child threads exited");
    // exit(0);
    longjmp(jmpbuf, 0);
    // cout << "longjmp" << endl;
    // return;
}

void handleClient(ClientInfo cInfo) {
    int clnt_sockfd = cInfo.sockfd;
    Message revMsg = Message();

    while(!serverExit) {
        if(revMsg.revMessage(clnt_sockfd) != Error::Normal) {
            break;
        }

        if(revMsg.type == Type::Time) {
            sendTime(clnt_sockfd);
            cout << PROMPT << "Time has been sent to client with id[" << cInfo.clientId << "]" << endl; 
        } else if(revMsg.type == Type::Name) {
            sendName(clnt_sockfd);
            cout << PROMPT << "Machine name has been sent to client with id[" << cInfo.clientId << "]" << endl; 
        } else if(revMsg.type == Type::List) {
            sendList(clnt_sockfd);
            cout << PROMPT << "Client list has been sent to client with id[" << cInfo.clientId << "]" << endl; 
        } else if(revMsg.type == Type::Mess) {
            if(transMessage(cInfo.clientId, revMsg) == 0) {
                replySendStatus(clnt_sockfd, "Message sent successfully");
                printPrompt("Message sent successfully");
            } else {
                replySendStatus(clnt_sockfd, "Message failed to send, client could not be found");
                printPrompt("Message failed to send, desClient could not be found");
            }
        } else if(revMsg.type == Type::Disconnect) {
            ackDisconnect(clnt_sockfd);
            break;
        } else if(revMsg.type == Type::ServerExit) {
        } else {
            printPrompt("Unknown message type");
        }
    }

    close(clnt_sockfd);
    cList.rmClientInfo(cInfo.clientId);
    cout << PROMPT << "Client with id[" << cInfo.clientId << "] " << "disconnected" << endl;
    printPrompt("Child thread exited");
}

int main() {

    // Init the listening socket
    sevSockfd = socket(AF_INET, SOCK_STREAM, 0);
    errif(sevSockfd == -1, "socket create error");

    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(SEVPORT);

    errif(bind(sevSockfd, (sockaddr*)&serv_addr, sizeof(serv_addr)) == -1, "socket bind error");
    errif(listen(sevSockfd, SOMAXCONN) == -1, "socket listen error");

    // Create a child thread to checkExit
    thread exitServer(checkExit);
    exitServer.detach();

    uint32_t nextClientId = 0;

    signal(SIGQUIT, handleServerExit);

    while (!serverExit) {
        // accept new client
        printPrompt("Waiting for new client");
        struct sockaddr_in clnt_addr;
        socklen_t clnt_addr_len = sizeof(clnt_addr);
        bzero(&clnt_addr, sizeof(clnt_addr));
        int clnt_sockfd;

        setjmp(jmpbuf);
        if(serverExit) {
            close(sevSockfd);
            printPrompt("Server shutdown!");
            return 0;
        }
        clnt_sockfd = accept(sevSockfd, (sockaddr*)&clnt_addr, &clnt_addr_len);        
        // errif(clnt_sockfd == -1, "socket accept error");
        
        ClientInfo cInfo;
        cInfo.clientId = nextClientId++;
        bzero(cInfo.ipAddr, IPV4_LEN);
        strcpy(cInfo.ipAddr, inet_ntoa(clnt_addr.sin_addr));
        cInfo.port = ntohs(clnt_addr.sin_port);
        cInfo.sockfd = clnt_sockfd;
        cList.addClientInfo(cInfo); // need mutex

        cout << "New client: [ClientId]" << cInfo.clientId << " [IPAddress]" << cInfo.ipAddr << " [Port]" << cInfo.port << endl; 
        childThreads.push_back(thread(handleClient, cInfo));
    }

    close(sevSockfd);
    printPrompt("Server shutdown");
    return 0;
}