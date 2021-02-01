#include "utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

size_t str_hash(const char * s){
    size_t hash = 5381;
    // hash * 33 + c
    while(*s++)hash = ((hash << 5) + hash) + ((size_t)*s);
    return hash;
}

void err_exit(const char * fmt,...){
    va_list arg;
    va_start(arg,fmt);
    vfprintf(stderr,fmt,arg);
    va_end(arg);
    exit(1);
}
