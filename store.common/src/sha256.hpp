#pragma once

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <openssl/evp.h>

using namespace std;

/*
EVP_MD_CTX_new() in 1.1.0 has replaced EVP_MD_CTX_create() in 1.0.x.
EVP_MD_CTX_free() in 1.1.0 has replaced EVP_MD_CTX_destroy() in 1.0.x.
For 1.1, make sure to define link path (/usr/local/lib).
gcc -I /usr/local/ssl digest_example.c -Wl -L /usr/local/lib -lssl -lcrypto
*/

namespace store::common {

  string computeSha256Hash(const std::string& unhashed) {
    string hashed = "";

    EVP_MD_CTX* context = EVP_MD_CTX_new();

    if(context == NULL)
      return "";
    
    if(!EVP_DigestInit_ex(context, EVP_sha256(), NULL)) {
      EVP_MD_CTX_free(context);
      return "";
    }

    size_t len = unhashed.size(),
      len_read = 0,
      buf_size = 32768;

    #ifdef DEBUG
    cout << "Length: " << len << endl;
    #endif

    while (len > len_read) {
      size_t remainder = len - len_read;
      size_t len_end = remainder < buf_size ? remainder : buf_size;
      auto next = unhashed.substr(len_read, len_end);

      if (!EVP_DigestUpdate(context, next.c_str(), next.length()))
        break;

      len_read += len_end;
      #ifdef DEBUG
      cout << "Read: " << len_read << endl;
      #endif
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int lengthOfHash = 0;

    if(EVP_DigestFinal_ex(context, hash, &lengthOfHash)) {
        std::stringstream ss;
        for(unsigned int i = 0; i < lengthOfHash; ++i)
          ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];

        hashed = ss.str();
    }
    
    EVP_MD_CTX_free(context);

    return hashed;
  }

}
