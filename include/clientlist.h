#ifndef CLIENTLIST_H
#define CLIENTLIST_H

#include <map>
#include <iostream>
#include <string.h>
#include <mutex>
#define IPV4_LEN 20
#define MAXDATASIZE 1024
using namespace std;
mutex mx;

class ClientInfo {
public:
    int sockfd;
    uint32_t clientId;
    char ipAddr[IPV4_LEN];
    int port;
};

class ClientList {
public:
    int getSockfd(uint32_t clientId) { // for transMessage between clients
        lock_guard<mutex> lg(mx);
        if(clientList.count(clientId)) {
            return clientList[clientId].sockfd;
        } 
        return -1; // client could not be found
    }

    void addClientInfo(ClientInfo& clientInfo) {
        lock_guard<mutex> lg(mx);
        if(clientList.count(clientInfo.clientId)) {
            cout << "Note: client[" << clientInfo.clientId << "] has existed" << endl;
        }
        clientList[clientInfo.clientId] = clientInfo;
    }

    void rmClientInfo(uint32_t clientId) {
        lock_guard<mutex> lg(mx);
        clientList.erase(clientId);
    }

    uint32_t serialClientInfos(char* data) { // for list all clients' infomation
        lock_guard<mutex> lg(mx);
        uint32_t offset = 0;
        if(sizeof(ClientInfo) * clientList.size() > MAXDATASIZE) {
            printPrompt("Clients' info out of range");
            return -1;
        }

        for(auto elem : clientList) {
            auto& clientInfo = elem.second;
            memcpy(data+offset, &(clientInfo.clientId), sizeof(uint32_t));
            offset += sizeof(uint32_t);
            memcpy(data+offset, clientInfo.ipAddr, sizeof(clientInfo.ipAddr));
            offset += sizeof(clientInfo.ipAddr);
            memcpy(data+offset, &(clientInfo.port), sizeof(int));
            offset += sizeof(int);
        }
        return offset;
    }

    map<uint32_t, ClientInfo> getClientList() {
        return clientList;
    }

private:
    map<uint32_t, ClientInfo> clientList;
};

#endif