#include <signal.h>
#include <thread>
#include <setjmp.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "util.h"
#include "threadpool.h"

#define BUFFERSIZE 1024
#define SEVPORT 3936
#define IPV4_LEN 20
#define POOLSIZE 10

#define WHITE "\033[37m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"

bool serverExit = false;
int sevSockfd;
static jmp_buf jmpbuf;
using std::cin;
using std::cout;
using std::endl;
using std::thread;
using std::string;

int readLine(int sockfd, char* buf, int size) {
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size) && (c != '\n')) {
        n = recv(sockfd, &c, 1, MSG_PEEK);
        if (n > 0) {
            n = recv(sockfd, &c, 1, 0);
            if (c == '\r') {
                n = recv(sockfd, &c, 1, MSG_PEEK);
                if ((n > 0) && (c == '\n')) {
                    recv(sockfd, &c, 1, 0);
                } else break;
            }
            buf[i++] = c;
        }
        else break;
    }

    buf[i] = '\0';
    return i;
}

void discardHeader(int sockfd, char* line = nullptr, const char* attribute = "") {
    int num = 1;
    bool getAttr = (line != nullptr);
    char buf[BUFFERSIZE];
    // discard header
    while ((num > 0) && strcmp("\n", buf)) {  
        num = readLine(sockfd, buf, sizeof(buf));
        cout << buf;
        // need to get specified attribute
        if(getAttr) {
            if(strstr(buf, attribute)) {
                strcpy(line, buf);
                getAttr = false;
            }
        }
    }
}

// Response without file
void response(int sockfd, const char* sCode, const char* rPhrase, const char* content) {

    char buf[BUFFERSIZE];
    char body[BUFFERSIZE];

    cout << YELLOW << "Response:" << endl;
    sprintf(body, "<HTML><TITLE>%s</TITLE><BODY><H1>%s</H1></BODY></HTML>", rPhrase, content);

    sprintf(buf, "HTTP/1.1 %s %s\r\n", sCode, rPhrase);
    send(sockfd, buf, strlen(buf), 0);
    cout << buf;
    sprintf(buf, "Server: yey/1.0\r\n");
    send(sockfd, buf, strlen(buf), 0);
    cout << buf;
    sprintf(buf, "Content-Type: text/html\r\n");
    send(sockfd, buf, strlen(buf), 0);
    cout << buf;
    sprintf(buf, "Content-Length: %d\r\n\r\n", (int)strlen(body)); // length is important
    send(sockfd, buf, strlen(buf), 0);
    cout << buf;
    strcpy(buf, body);
    send(sockfd, buf, strlen(buf), 0);
    cout << buf << endl << endl << WHITE;
}

// Response with file
void response(int sockfd, const char *filepath, int filesize, char* filetype) {

    char buf[BUFFERSIZE];
    cout << YELLOW << "Response:" << endl;

    // HEADER
    strcpy(buf, "HTTP/1.1 200 OK\r\n");
    send(sockfd, buf, strlen(buf), 0);
    cout << buf;
    strcpy(buf, "Server: yey/1.0\r\n");
    send(sockfd, buf, strlen(buf), 0);
    cout << buf;
    sprintf(buf, "Content-Type: %s\r\n", filetype);
    send(sockfd, buf, strlen(buf), 0);
    cout << buf;
    sprintf(buf, "Content-Length: %d\r\n\r\n", filesize);
    send(sockfd, buf, strlen(buf), 0);
    cout << buf << WHITE;

    // BODY
    int resfd = open(filepath, O_RDONLY, 0);                       
    char* res = (char*)mmap(0, filesize, PROT_READ, MAP_PRIVATE, resfd, 0); 
    close(resfd);                                               
    send(sockfd, res, filesize, 0);                             
    munmap(res, filesize);  
}

void url2Path(char* url, char* path, char* filetype) {

    if(url[strlen(url) - 1] == '/') {
        sprintf(path, "docs/html/test.html");
        strcpy(filetype, "text/html");
        return;
    }

    if (strstr(url, ".html")) {
        sprintf(path, "docs/html%s", url);
        strcpy(filetype, "text/html");
    }  
    else if (strstr(url, ".png")) {
        sprintf(path, "docs/img%s", url);
        strcpy(filetype, "image/png");
    }
    else if (strstr(url, ".jpg")) {
        sprintf(path, "docs/img%s", url);
        strcpy(filetype, "image/jpeg");
    }
    else if (strstr(url, ".txt")) {
        sprintf(path, "docs/txt%s", url);
        strcpy(filetype, "text/plain");
    } else {
        sprintf(path, "docs%s", url);
    }
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

    serverExit = true;
    longjmp(jmpbuf, 0);
}

void handleClient(int clnt_sockfd) {

    char buf[BUFFERSIZE];
    char method[BUFFERSIZE];
    char url[BUFFERSIZE];
    char path[BUFFERSIZE];
    char filetype[20] = {0};
    struct stat st;

    // get request
    cout << GREEN << "Request:" << endl;
    readLine(clnt_sockfd, buf, sizeof(buf));
    cout << buf;

    // parse method, url (GET /test.html HTTP/1.1)
    sscanf(buf, "%s %s", method, url);

    // method = GET
    if (strcasecmp(method, "GET") == 0) {
        discardHeader(clnt_sockfd);
        cout << WHITE;
        // convert url to actual file path && get file type
        url2Path(url, path, filetype);
        // cout << endl << url << endl;
        // cout << path << endl;

        if (stat(path, &st) == -1) {
            response(clnt_sockfd, "404", "NOT FOUND", "The requested resource not found");
        } else if (!(S_IRUSR & st.st_mode)) {
            response(clnt_sockfd, "403", "FORBIDDEN", "The requested resource inaccessible");
        } else {
            if ((st.st_mode & S_IFMT) == S_IFDIR) {
                strcat(path, "/index.html");
                strcpy(filetype, "text/html");
            }
            
            response(clnt_sockfd, path, st.st_size, filetype);
        }
    }
    // method = POST
    else if (strcasecmp(method, "POST") == 0) {
        // the url is /dopost
        if (strcmp(url, "/dopost") == 0) {
        
            char line[BUFFERSIZE];
            int len;
            // discard the header and get Content-Length
            discardHeader(clnt_sockfd, line, "Content-Length");
            sscanf(line, "Content-Length: %d", &len);
            
            // read the body
            readLine(clnt_sockfd,buf, len); 
            cout << buf << endl << endl << WHITE;
            // check username and password
            if(strcmp(buf,"login=3200103936&pass=3936") == 0) {
                response(clnt_sockfd, "200", "OK", "Login successfully");
            } else {
                response(clnt_sockfd, "200", "OK", "Login failed");
            }
        }
        else {
            response(clnt_sockfd, "404", "NOT FOUND", "Not dopost");
        }
    } 
    else {
        response(clnt_sockfd, "501", "METHOD NOT IMPLEMENTED", "Request method not supported");
    }
    
    close(clnt_sockfd);
    cout << PROMPT << "Client with sockfd[" << clnt_sockfd << "] " << "disconnected" << endl;
    return;
}

int main() {

    ThreadPool pool(POOLSIZE);
    // Init the listening socket
    sevSockfd = socket(AF_INET, SOCK_STREAM, 0);
    errif(sevSockfd == -1, "socket create error");

    int on = 1;
    setsockopt(sevSockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(SEVPORT);

    errif(bind(sevSockfd, (sockaddr*)&serv_addr, sizeof(serv_addr)) == -1, "socket bind error");
    errif(listen(sevSockfd, SOMAXCONN) == -1, "socket listen error");

    cout << PROMPT << "Server running on port " << SEVPORT << endl; 

    // Create a child thread to checkExit
    thread exitServer(checkExit);
    exitServer.detach();

    signal(SIGQUIT, handleServerExit);

    while (!serverExit) {
        // accept new client
        printPrompt("Waiting for new client...");
        cout << endl;
        struct sockaddr_in clnt_addr;
        socklen_t clnt_addr_len = sizeof(clnt_addr);
        bzero(&clnt_addr, sizeof(clnt_addr));
        int clnt_sockfd;

        setjmp(jmpbuf);
        if(serverExit) {
            break;
        }
        clnt_sockfd = accept(sevSockfd, (sockaddr*)&clnt_addr, &clnt_addr_len);        
        errif(clnt_sockfd == -1, "socket accept error");
        
        char ipAddr[IPV4_LEN];
        uint16_t port;
        bzero(ipAddr, IPV4_LEN);
        strcpy(ipAddr, inet_ntoa(clnt_addr.sin_addr));
        port = ntohs(clnt_addr.sin_port);
        cout << PROMPT << "New client: [IPAddress]" << ipAddr << " [Port]" << port << endl; 

        // Create a child thread to handle this client
        pool.enqueue(handleClient, clnt_sockfd);
    }

    close(sevSockfd);
    return 0;
}