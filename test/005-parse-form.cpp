/*
cp ../sample-post-data.txt bin
g++ -std=c++14 -Wall -I ../../ -o bin/parse-form ../005-parse-form.cpp -levent -lpthread
*/

#include <stdio.h>
#include <stdlib.h>
#include <evhttp.h>

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

  evhttp_parse_query_str(buf, headers);

  struct evkeyval* kv = headers->tqh_first;
  while (kv) {
    printf("%s: %s\n", kv->key, kv->value);
    kv = kv->next.tqe_next;
  }
  
  free(buf);
  free(headers);

  return 0;
}
