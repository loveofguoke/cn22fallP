#include "util.h"

void errif(bool condition, const char *errmsg){
    if(condition){
        perror(errmsg);
        exit(EXIT_FAILURE);
    }
}

void printPrompt(std::string s) {
  std::cout << PROMPT << s << std::endl;
}