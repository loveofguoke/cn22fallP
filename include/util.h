#ifndef UTIL_H
#define UTIL_H
#define PROMPT "[PROMPT] "
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>

void errif(bool, const char*);
void printPrompt(std::string s);

#endif