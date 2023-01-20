#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdint.h>
#include <iostream>
#include <string.h>
#include "util.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#define MAXDATASIZE 1024
#define BUFFERSIZE 1050 
// Define request/response type
using namespace std;
enum Type {
  Time,
  Name,
  List,
  Mess,
  Disconnect,
  ServerExit,
  ReplySendStatus,
};

enum Error {
  Normal,
  MsgOutRange,
  LenReadErr,
  Disconnected,
  SockReadErr,
  InCompleteMsg,
};

// Define message format
class Message {
public:
  uint32_t length; // the length of the whole message
  Type type;
  uint32_t clientId; // src or des
  char data[MAXDATASIZE];

  void printMessage() {
    cout << "length: " << length << endl;
    cout << "Type: " << type << endl;
    cout << "clientId: " << clientId << endl;
    cout << "data: " << data << endl;
  }

  int setMessage(Type type, uint32_t clientId, string data) {
    this->type = type;
    // this->length = length;
    this->clientId = clientId;
    // errif(data.size() > MAXDATASIZE, "Message data out of range");
    if(data.size() > MAXDATASIZE) {
      printPrompt("Message data out of range");
      return MsgOutRange;
    }
    memcpy(this->data, data.c_str(), data.size());
    this->length = data.size() + sizeof(Type) + 2*sizeof(uint32_t);
    return Normal;
  }

  int revMessage(int sockfd) { // receive message
    char buf[BUFFERSIZE];
    bzero(&buf, sizeof(buf));
    uint32_t length = 0;
    ssize_t read_bytes = read(sockfd, buf, sizeof(uint32_t));
    if(read_bytes != sizeof(uint32_t)) {
      // errif(true, "length read error");
      printPrompt("Length read error");
      return LenReadErr;
    }
    memcpy(&length, buf, sizeof(uint32_t));
    read_bytes += read(sockfd, buf+sizeof(uint32_t), length-sizeof(uint32_t));
    // cout << "read_bytes: " << read_bytes << endl;
    if(read_bytes > 0 && read_bytes == length) {
      this->length = length;
      this->deserialMessage(buf);
    } else if(read_bytes == 0) {
      printPrompt("Socket disconnected");
      return Disconnected;
    } else if(read_bytes == -1) {
      // errif(true, "socket read error");
      printPrompt("Socket read error");
      return SockReadErr;
    } else if(read_bytes < length) {
      // errif(true, "read incomplete message");
      printPrompt("Read incomplete message");
      return InCompleteMsg;
    }

    // printf("receive message:\n");
    // this->printMessage();
    return Normal;
  }

  int sendMessage(int sockfd) { // send message
    char buf[BUFFERSIZE];
    bzero(&buf, sizeof(buf));
    this->serialMessage(buf);
    ssize_t write_bytes = write(sockfd, buf, length);
    // cout << "write_bytes: " << write_bytes << endl;
    if(write_bytes == -1) {
      printPrompt("Socket already disconnected, can't write any more!");
      return Disconnected;
    }
    return Normal;
  }

private:
  void serialMessage(char* buf) { // serialize message

    memcpy(buf, this, this->length);
  } 

  void deserialMessage(char* buf) {
    bzero(this->data, sizeof(this->data));
    // uint32_t length;
    // memcpy(&length, buf, sizeof(uint32_t));
    memcpy(this, buf, this->length);
  }
};

#endif