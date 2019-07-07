/*
cp ../sample-post-data.txt bin
cp ~/docs/V4/xslt/rnumber/recurse-map.xml bin
g++ -std=c++14 -Wall -I ../../ -I ../../../json/single_include/nlohmann -o bin/parse-form ../005-parse-form.cpp -levent -lpthread -lpugixml
*/

#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <evhttp.h>
#include <regex>
#include "json.hpp"
#include <pugixml.hpp>

using json = nlohmann::json;

// An implementation of regex replace in c is here:
// https://gist.github.com/tanbro/4379692

// Reference for pugixml
// http://www.gerald-fahrnholz.eu/sw/DocGenerated/HowToUse/html/group___grp_pugi_xml_example_cpp_file.html
// https://source.openmpt.org/svn/openmpt/tags/1.22.07.00/include/pugixml/docs/manual/modify.html#manual.modify.clone

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

void removeNodes(pugi::xml_document& doc, const char* query) {
  pugi::xpath_node_set ns = doc.select_nodes(query);

  for (auto& n: ns)
    n.node().parent().remove_child(n.node());
}

std::regex slash_rgx("(%2F)");
std::regex colon_rgx("(%3A)");

auto main(int argc, char* argv[]) -> int {

  char* buf = readFile("sample-post-data.txt");
  struct evkeyvalq* headers = (struct evkeyvalq*) malloc(sizeof(struct evkeyvalq));

  pugi::xml_document doc, doc2;
  pugi::xml_parse_result result = doc.load_file("recurse-map.xml");
  pugi::xpath_node_set attrs = doc.select_nodes("//Attribute");
  std::vector<std::string> li;
  for (auto node : attrs) {
    // std::cout << node.node().attribute("path").value() << "\n";
    li.emplace_back(node.node().attribute("path").value());
  }

  // sort(v.begin(), v.end()); 

  evhttp_parse_query_str(buf, headers);

  std::vector<std::string> li_defined;
  std::set<std::string> li_defined_parent;
  std::set<int> li_depth;
  
  struct evkeyval* kv = headers->tqh_first;
  while (kv) {

    std::string key(kv->key);
    key = std::regex_replace(key, slash_rgx, "/");

    li_defined.emplace_back(key);

    auto tokens = split((char*)key.c_str(), "/");

    std::string parentPath;
    for (int i = 0; i < tokens.size() - 1; i++) {
      parentPath.append("/" + tokens.at(i));
    }
    li_defined_parent.emplace(parentPath);

    json val = kv->value[0] == '[' ? json::parse(kv->value) : kv->value;

    // Sets
    if (kv->value[0] == '[') {
      
      for (const auto o : val) {
        std::string code = o["code"].get<std::string>();
        if (code[0] == '{') {
          std::cout << key << "\n";
          // Get the WebService element.
          std::string q{"//Attribute[@path='" + key + "']/WebService"};
          std::cout << q << "\n";
          
          pugi::xpath_node_set ns = doc.select_nodes(q.c_str());

          pugi::xpath_node n = ns.first();
          ns.first().node().append_copy(doc2.document_element());

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
    }

    
    // std::cout << key << ": " << val.dump(2) << std::endl;
    kv = kv->next.tqe_next;
  }
  
  free(buf);
  free(headers);

  std::cout << li.size() << "\n";

  auto rend = std::remove_if(li.begin(), li.end(), [&li_defined](auto& s){
    return std::find(li_defined.begin(), li_defined.end(), s) != li_defined.end();
  });

  auto rend1 = std::remove_if(li.begin(), li.end(), [&li_defined_parent](auto& s){
    return std::find(li_defined_parent.begin(), li_defined_parent.end(), s) != li_defined_parent.end();
  });

  std::vector<std::string> li2;

  for(auto it = li.begin(); it != rend1; ++it) {
    li2.emplace_back(*it);
  }

  std::cout << li2.size() << "\n";
  
  for (auto& s : li2) {
    std::string p{"//Attribute[@path='" + s + "']"};
    removeNodes(doc, p.c_str());
  }

  removeNodes(doc, "//Attribute[@ReferenceType='SetType']/WebService");

  removeNodes(doc, "//Attribute/WebService[not(Attribute)]");
  
  // Reverse order
  pugi::xpath_node_set ns = doc.select_nodes("//Attribute/WebService");
  for (auto& n: ns)
    li_depth.emplace(std::stoi(n.node().attribute("nodeDepth").value()));
  
  if(!li_depth.empty()) {
    int depth = *li_depth.rbegin();

    while (depth > 0) {
      std::string q{"//Attribute/WebService[@nodeDepth='" + std::to_string(depth) + "']"};

      pugi::xpath_node_set ns = doc.select_nodes(q.c_str());
      for (auto& n: ns)
        std::cout << depth << ": " << n.node().attribute("TypeName").value() << "\n";

      depth--;
    }
  }
    

  doc.save_file("output-2.xml");
  doc2.save(std::cout);

  return 0;
}
