/*
g++ -std=c++14 -I ../../ -I /usr/local/include -I ../../../Store-cpp -I ../../../json/single_include/nlohmann ../002-must-exist.cpp -o bin/testing -L /usr/lib/x86_64-linux-gnu -L /usr/local/lib -luuid
*/
#include <cassert>
#include <iostream>
#include "store.models/src/extensions.hpp"

using namespace std;
using namespace store::extensions;

string testMustExist(vector<string> mustExistKeys = {}) {

  vector<string> mustExistKeysQuoted;
  std::transform(mustExistKeys.begin(), mustExistKeys.end(), std::back_inserter(mustExistKeysQuoted), [](const string& s) { return wrapString(s); });
  string mustExistKeysQuotedString = join(mustExistKeysQuoted.begin(), mustExistKeysQuoted.end(), string(","));
  string matchMustExistKeys = mustExistKeysQuotedString.size() > 0 ? string_format(" and data ?& array[%s]", mustExistKeysQuotedString.c_str()) : "";

  return move(matchMustExistKeys);
}

int main(int argc, char *argv[]) {

  auto res = testMustExist();

  cout << "Should be empty: " << std::boolalpha << (res.size() == 0) << endl;
  assert(res == "");

  auto res1 = testMustExist({"a", "b", "c"});
  
  cout << "Should not be empty: " << std::boolalpha << (res1.size() > 0) << endl << res1 << endl;
  assert(" and data ?& array['a','b','c']" == res1);
}
