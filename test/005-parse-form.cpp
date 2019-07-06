/*
cp ../sample-post-data.txt bin
g++ -std=c++14 -Wall -I ../../ -I ../../../json/single_include/nlohmann -I ../../../rapidxml -o bin/parse-form ../005-parse-form.cpp -levent -lpthread
*/

#include <iostream>
#include <fstream>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <evhttp.h>
#include <regex>
#include "json.hpp"
// Modified versions of rapidxml for printing.
// https://stackoverflow.com/questions/14113923/rapidxml-print-header-has-undefined-methods
#include "rapidxml.hpp"
#include "rapidxml_print.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

using namespace rapidxml;
using namespace boost::property_tree;
using json = nlohmann::json;

// An implementation of regex replace in c is here:
// https://gist.github.com/tanbro/4379692

void dumpFile(const std::string& outName, const std::string& content) {
  std::ofstream of;
  of.open(outName);
  of << content;
  of.close();
}

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

xml_node<>* createRootNode(xml_document<>& doc) {
  // std::shared_ptr<xml_document<>> doc(new xml_document<>());

  // xml_document<> doc;
  // xml declaration
  xml_node<>* n = doc.allocate_node(node_declaration);
  n->append_attribute(doc.allocate_attribute("version", "1.0"));
  n->append_attribute(doc.allocate_attribute("encoding", "utf-8"));
  doc.append_node(n);

  // root node
  xml_node<>* o = doc.allocate_node(node_element, "object");
  doc.append_node(o);

  return o;
}

std::vector<std::string> split(char* line, char* tk = ":") {
  char* token = strtok(line, tk);
  std::vector<std::string> ret;

  while (token != NULL) { 
      // printf("%s\n", token);
      ret.push_back(token);
      token = strtok(NULL, tk); 
  }

  return ret;
}

std::regex slash_rgx("(%2F)");
std::regex colon_rgx("(%3A)");

auto main(int argc, char* argv[]) -> int {

  char* buf = readFile("sample-post-data.txt");
  struct evkeyvalq* headers = (struct evkeyvalq*) malloc(sizeof(struct evkeyvalq));

  xml_document<> doc;
  auto root = createRootNode(doc);

  ptree tree;

  evhttp_parse_query_str(buf, headers);

  struct evkeyval* kv = headers->tqh_first;
  while (kv) {

    std::string key(kv->key);
    key = std::regex_replace(key, slash_rgx, "/");
    key = std::regex_replace(key, colon_rgx, ":");
    
    auto tokens = split((char*)key.c_str(), ":");

    /*
    for (auto& a : tokens)
      std::cout << a << std::endl;
    */

    json val = kv->value[0] == '[' ? json::parse(kv->value) : kv->value;

    // Sets
    if (kv->value[0] == '[') {
      
      for (const auto o : val) {
        std::string code = o["code"].get<std::string>();
        if (code[0] == '{') {
          // Wrappers parsing.
          json codeVal = json::parse(o.value("code", "{}"));
          for (auto& el : codeVal.items()) {
            std::string v = el.value()["code"];
            // if (v.size() > 0)
            //   std::cout << el.key() << " : " << el.value()["code"] << std::endl;
          }
        } else {
          // Non wrapper
          // std::cout << key << ": " << code << std::endl;
        }
      }
    } else {
      // Scalar
      xml_node<>* field = doc.allocate_node(node_element, "field");
      field->append_attribute(doc.allocate_attribute("name", kv->key));
      field->append_attribute(doc.allocate_attribute("value", kv->value));
      root->append_node(field);

      auto spath = tokens[0];
      auto vpath = split((char*)spath.c_str(), "/");

      /*
      for (auto el : vpath) {
        ptree nextEl;
        nextEl.put("Attribute.<xmlattr>.Name", el);
        tree.put("WebServiceRoot.WebService.Attribute.<xmlattr>.Name", el);
      }

      if (tokens.size() > 2) {

      }
      */

      std::string npath = std::regex_replace(spath, std::regex("/"), ".");

      tree.put("WebServiceRoot.WebService" + npath + ".Attribute.<xmlattr>.Value", kv->value);
    }

    // std::cout << key << ": " << val.dump(2) << std::endl;
    kv = kv->next.tqe_next;
  }
  
  free(buf);
  free(headers);

  std::string outxml;
  rapidxml::print(std::back_inserter(outxml), doc);

  dumpFile("output.xml", outxml);

  // boost
  boost::property_tree::xml_writer_settings<std::string> xcfg(' ', 2);
  // write_xml(std::cout, tree, boost::property_tree::xml_writer_settings<std::string>(' ', 2));
  write_xml("output-2.xml", tree, std::locale(), xcfg);


  return 0;
}
