#pragma once

#include <stdio.h>  /* defines FILENAME_MAX */
#include <unistd.h> // getcwd
#include <fstream>
#include <boost/filesystem.hpp>

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

  void dumpFile(const string& outName, const string& content) {
    ofstream of;
    of.open(outName);
    of << content;
    of.close();
  }

  auto getAllFilesInDir = [](const string& dirName) -> vector<string> {
    namespace fs = boost::filesystem;
    vector<string> list;

    fs::path p(dirName);
    fs::recursive_directory_iterator end;

    for (fs::recursive_directory_iterator i(p); i != end; ++i) {
      const fs::path cp = (*i);
      if (boost::filesystem::is_regular_file(cp))
        list.push_back(cp.string());
    }
    return list;
  };

  auto getCurrentWorkingDir = []() -> string {
    char buff[FILENAME_MAX];
    getcwd( buff, FILENAME_MAX );
    string current_working_dir(buff);
    return current_working_dir;
  };

}
