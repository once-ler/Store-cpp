/*
cp ../sample-post-data.txt bin
g++ -std=c++14 -Wall -I ../../ -I ../../../json/single_include/nlohmann -o bin/parse-form ../005-parse-form.cpp -levent -lpthread
*/

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <evhttp.h>
#include <regex>
#include "json.hpp"

using json = nlohmann::json;

// An implementation of regex replace in c is here:
// https://gist.github.com/tanbro/4379692

char* readFile(const char* fname) {
  FILE* fdata = fopen(fname, "rb");

  if (fdata == NULL)
    return NULL;

  // Size?
  long sz;
  fseek (fdata , 0 , SEEK_END);
  sz = ftell (fdata);
  rewind (fdata);

  char* buf = (char*)malloc(sizeof(char)* sz);

  size_t ret = fread (buf, 1, sz, fdata);

  fclose(fdata);

  return buf;
}

auto main(int argc, char* argv[]) -> int {

  char* buf = readFile("sample-post-data.txt");
  struct evkeyvalq* headers = (struct evkeyvalq*) malloc(sizeof(struct evkeyvalq));

  std::regex rgx("(%2F)");

  evhttp_parse_query_str(buf, headers);

  struct evkeyval* kv = headers->tqh_first;
  while (kv) {

    std::string key(kv->key);
    key = std::regex_replace(key, rgx, "/");

    json val = kv->value[0] == '[' ? json::parse(kv->value) : kv->value;

    // Wrappers parsing.
    if (kv->value[0] == '[') {
      
      for (const auto o : val) {
        if (o["code"].get<std::string>()[0] == '{') {
          json code = json::parse(o.value("code", "{}"));
          for (auto& el : code.items()) {
            std::string v = el.value()["code"];
            if (v.size() > 0)
              std::cout << el.key() << " : " << el.value()["code"] << std::endl;
          }
        }
      }
    }

    // std::cout << key << ": " << val.dump(2) << std::endl;
    kv = kv->next.tqe_next;
  }
  
  free(buf);
  free(headers);

  return 0;
}
