#include <string>
#include "message.h"
#include <thread>
#include <signal.h>
#include "clientlist.h"

using std::cout;
using std::cin;
using std::endl;
using std::string;
#define SEVPORT 3936

bool connectState = false;
Message msg = Message();
std::thread receiver;

int connectSEV() {
  string ipAddress;
  uint16_t port;
  printPrompt("Please enter the IP of the server");
  cin >> ipAddress;
  printPrompt("Please enter the Port of the server");
  cin >> port;
  if(cin.bad()) {
    printPrompt("Incorrect input format");
    return -1;
  }

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  errif(sockfd == -1, "socket create error");

  struct sockaddr_in serv_addr;
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(ipAddress.c_str());
  serv_addr.sin_port = htons(port);
  bzero(&(serv_addr.sin_zero), sizeof(serv_addr.sin_zero));

  int isConnect = connect(sockfd, (sockaddr*)&serv_addr, sizeof(serv_addr));
  // errif(isConnect == -1, "socket connect error");
  if(isConnect == -1) {
    cout << "Socket connect error: Connection refused" << endl;
    close(sockfd);
    return -1;
  }

  return sockfd;
}

void printMenu() {
  cout << "Here is the guide menu" << endl;
  cout << "------------------------------------------------------" << endl;
  cout << "| Cmd |               Description                    |" << endl;
  cout << "------------------------------------------------------" << endl;
  cout << "|  c  |  Connect to the server                       |" << endl;
  cout << "|  d  |  Disconnect from the server                  |" << endl;
  cout << "|  t  |  Get the current time from the server        |" << endl;
  cout << "|  n  |  Get the machine name of the server          |" << endl;
  cout << "|  l  |  List all clients connected to the server    |" << endl;
  cout << "|  s  |  Send a message to another specified client  |" << endl;
  cout << "|  e  |  Disconnect and exit this client             |" << endl;
  cout << "|  m  |  Show this menu again                        |" << endl;
  cout << "------------------------------------------------------" << endl;
  cout << "Usage: Type one of the commands at a time" << endl;
}

void printClientsInfo(Message& msg) {
  uint32_t dataLen = msg.length - sizeof(Type) - 2*sizeof(uint32_t);
  uint32_t offset = 0;
  uint32_t clientId;
  char ipAddr[IPV4_LEN];
  int port;

  while(offset < dataLen) {
    // deserialize data
    memcpy(&(clientId), msg.data+offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(ipAddr, msg.data+offset, sizeof(ipAddr));
    offset += sizeof(ipAddr);
    memcpy(&(port), msg.data+offset, sizeof(int));
    offset += sizeof(int);
    // output
    cout << "[ClientId]" << clientId << " [IPAddress]" << ipAddr << " [Port]" << port << endl;
  }
}

void getTime(int sockfd) {
  msg.setMessage(Type::Time, 0, "");
  msg.sendMessage(sockfd);
}

void getName(int sockfd) {
  msg.setMessage(Type::Name, 0, "");
  msg.sendMessage(sockfd);
}

void getClientList(int sockfd) {
  msg.setMessage(Type::List, 0, "");
  msg.sendMessage(sockfd);
}

void sendMessage(int sockfd) {
  uint32_t clientId;
  string data;  
  printPrompt("Type the message you want to send in next lines, end with symbol #");
  getline(cin, data, '#');
  printPrompt("Type the ClientId you want to send to in the next line");
  cin >> clientId;
  msg.setMessage(Type::Mess, clientId, data);
  msg.sendMessage(sockfd);
}

void synDisconnect(int sockfd) {
  msg.setMessage(Type::Disconnect, 0, "");
  msg.sendMessage(sockfd);
}

void ackServerExit(int sockfd) {
  msg.setMessage(Type::ServerExit, 0, "");
  msg.sendMessage(sockfd);
}

bool isConnected() {
  if(!connectState) {
    printPrompt("Not connected yet");
    return false;
  }
  return true;
}

void rev(int sockfd) {
  while(true) {
    Message msg = Message();
    // errif(msg.revMessage(sockfd) == -1, "server socket disconnected!");
    if(msg.revMessage(sockfd) != Error::Normal) {
      raise(SIGQUIT);
      close(sockfd);
      connectState = false;
      printPrompt("Server disconnected, the connection is down");
      return;
    }

    if(msg.type == Type::Disconnect) break;
    else if(msg.type == Type::Time) {
      cout << "Time: " << msg.data;
    } else if(msg.type == Type::Name) {
      cout << "Server Machine Name: " << msg.data << endl;
    } else if(msg.type == Type::List) {
      cout << "Clients' Information: " << endl;
      printClientsInfo(msg);
    } else if(msg.type == Type::Mess) {
      cout << "Message from client with id[" << msg.clientId << "]: ";
      cout << msg.data << endl; 
    } else if(msg.type == Type::ReplySendStatus) {
      cout << msg.data << endl;
    } else if(msg.type == Type::ServerExit) {
      raise(SIGQUIT);
      ackServerExit(sockfd);
      close(sockfd);
      connectState = false;
      printPrompt("Server has exited, the connection is down");
      return;
    }
  }
}

void detachReceiver(int signum) {
  receiver.detach();
}

int main() {

  int sockfd;
  string cmd;
  printMenu();

  signal(SIGQUIT, detachReceiver);
    
  while(true) {
    cin >> cmd;
    if(cmd.empty()) continue;
    if(cmd == "c") {
      if(connectState) {
        printPrompt("Already connected");
        continue;
      }
      sockfd = connectSEV();
      if(sockfd == -1) continue;
      connectState = true;
      receiver = thread(rev, sockfd);
      printPrompt("Connect successfully");
    } else if(cmd == "d") {
      if(!connectState) {
        printPrompt("Not connected yet");
        continue;
      }
      synDisconnect(sockfd);
      receiver.join();
      close(sockfd);
      connectState = false;
      printPrompt("Disconnect successfully");
    } else if(cmd == "t") {
      if(isConnected()) {
        getTime(sockfd);
      }
    } else if(cmd == "n") {
      if(isConnected()) {
        getName(sockfd);
      }
    } else if(cmd == "l") {
      if(isConnected()) {
        getClientList(sockfd);
      }
    } else if(cmd == "s") {
      if(isConnected()) {
        sendMessage(sockfd);
      }
    } else if(cmd == "e") {
      if(connectState) {
        synDisconnect(sockfd);
        receiver.join();
        close(sockfd);
      }
      printPrompt("Exiting the client");
      return 0;
    } else if(cmd == "m") {
      printMenu();
    } else {
      printPrompt("Illegal command, please try again");
    }
  }
  return 0;
}
