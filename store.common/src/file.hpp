#pragma once

#include <fstream>

using namespace std;

namespace store::common {

  string getFile(const string& infile) {
    std::ifstream t(infile);

    if (t.fail()) return "";

    std::string str;
    t.seekg(0, std::ios::end);
    str.reserve(t.tellg());
    t.seekg(0, std::ios::beg);

    str.assign((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

    return move(str);
  }
}
