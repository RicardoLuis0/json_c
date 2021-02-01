#include "json.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cerrno>
#include <cstring>

static std::string readfile(const std::string &filename){
    std::ostringstream ss;
    std::ifstream f(filename);
    if(!f)throw std::runtime_error(strerror(errno));
    ss<<f.rdbuf();
    return ss.str();
}

int main() {
    std::string file=readfile("test.json");
    JSON_Element * elem=json_parse_n(file.c_str(),file.size());
    FILE * f=fopen("test_out.json","w");
    if(!f){
        json_free_element(elem);
        throw std::runtime_error(strerror(errno));
    }
    json_write_element(f,elem,0);
    json_free_element(elem);
    fclose(f);
    return 0;
}
