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

struct SetMember {
  std::string key;
  std::map<std::string, std::string> vals;
};

struct Scalar {
  std::string key;
  std::string val;
};

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

/*
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
*/

std::vector<std::string> split(const std::string& s, char delim = '/') {
  std::vector<std::string> tokens;
  std::string tok;
  std::istringstream iss(s);
  while (std::getline(iss, tok, delim))
    tokens.push_back(tok);
  
  return tokens;
}

bool invalid_char(char c) {  
  return !(c>=0 && c <128);
}

std::string strip_unicode(std::string str) {
  str.erase(remove_if(str.begin(),str.end(), invalid_char), str.end());
  return move(str);
}

void removeNodes(pugi::xml_document& doc, const char* query) {
  pugi::xpath_node_set ns = doc.select_nodes(query);

  for (auto& n: ns)
    n.node().parent().remove_child(n.node());
}

void removeUnusedNodes(pugi::xml_document& doc, std::vector<std::string> li2) {
  // Remove unused attributes from the main type.
  std::set<std::string> rl;
  std::vector<std::string> rl2;
  // pugi::xpath_node_set nsr = doc.select_nodes("//Attribute[count(./WebService) > 0]");
  pugi::xpath_node_set nsr = doc.select_nodes("//Attribute");
  for (auto& n: nsr) {
    rl.emplace(n.node().attribute("path").value());
    // std::cout << n.node().attribute("path").value() << "\n";
  }
  for (auto& a : rl)
    rl2.emplace_back(a);

  std::sort(rl2.begin(), rl2.end(), [](const auto& a, const auto& b){ return split(a, '/').size() < split(b, '/').size(); });
  for (auto& a : rl2) {
    if (std::find(li2.begin(), li2.end(), a) != li2.end()) {
      // std::cout << "Removing " << a << "\n";
      std::string p{"//Attribute[@path='" + a + "']"};
      removeNodes(doc, p.c_str());
    }    
  }

  /*
  // Extremely slow.
  for (auto& s : li2) {
    std::string p{"//Attribute[@path='" + s + "']"};
    // std::cout << p << "\n";
    // removeNodes(doc, p.c_str());
  }
  */

  removeNodes(doc, "//Attribute[@ReferenceType='SetType']/WebService");

  removeNodes(doc, "//Attribute/WebService[not(Attribute)]");
}

std::regex slash_rgx("(%2F)");
std::regex colon_rgx("(%3A)");

auto main(int argc, char* argv[]) -> int {

  char* buf = readFile("sample-post-data.txt");
  std::string cleaned_str = strip_unicode(std::string{buf});
  
  struct evkeyvalq* headers = (struct evkeyvalq*) malloc(sizeof(struct evkeyvalq));

  pugi::xml_document doc, doc_c, doc2, doc3, doc5, doc6;
  // Add root to temp doc.
  doc2.append_child("WebServiceRoot");
  doc3.append_child("WebServiceRoot");
  doc5.append_child("WebServiceRoot");
  doc6.append_child("WebServiceRoot");
  
  pugi::xml_parse_result result = doc.load_file("recurse-map.xml");
  pugi::xml_parse_result result2 = doc_c.load_file("recurse-map.xml");
  
  pugi::xpath_node_set attrs = doc.select_nodes("//Attribute");
  std::vector<std::string> li;
  for (auto node : attrs) {
    // std::cout << node.node().attribute("path").value() << "\n";
    li.emplace_back(node.node().attribute("path").value());
  }

  int rc = evhttp_parse_query_str(cleaned_str.c_str(), headers);

  std::cout << rc << "\n";

  std::set<std::string> li_defined;
  std::set<std::string> li_defined_parent;
  std::set<int> li_depth;
  
  // Hold set parsed values.
  std::vector<SetMember> members;
  std::vector<Scalar> scalars;


  struct evkeyval* kv = headers->tqh_first;
  while (kv) {

    std::string key(kv->key);
    key = std::regex_replace(key, slash_rgx, "/");

    li_defined.emplace(key);

    auto tokens = split(key, '/');

    std::string parentPath;
    for (int i = 0; i < tokens.size() - 1; i++) {
      auto tok = tokens.at(i);
      if (tok.size() > 0)
        parentPath.append("/" + tok);
    }
    li_defined_parent.emplace(parentPath);

    json val = kv->value[0] == '[' ? json::parse(kv->value) : kv->value;

    // Sets
    if (kv->value[0] == '[') {
      
      for (const auto o : val) {
        // Wrappers parsing.
        SetMember mem;
        mem.key = key;

        std::string code = o["code"].get<std::string>();
        if (code[0] == '{') {
          json codeVal = json::parse(o.value("code", "{}"));
          for (auto& el : codeVal.items()) {
            std::string v = el.value()["code"];
            if (v.size() > 0) {
              // std::cout << el.key() << " : " << el.value()["code"] << std::endl;
              mem.vals.insert({el.key(), el.value()["code"]});
            }
          }
        } else {
          // Non wrapper
          // std::cout << key << ": " << code << std::endl;
          mem.vals.insert({"code", code});
        }
        members.emplace_back(mem);
      }
    } else {
      // Scalar
      Scalar sca{key, val};
      scalars.emplace_back(sca);
    }

    
    // std::cout << key << ": " << val.dump(2) << std::endl;
    kv = kv->next.tqe_next;
  }

  free(buf);
  free(headers);


  // Add parent to defined before copying.  
  int counter = 0;
  for (auto& path : li_defined_parent) {
    std::cout << counter << ": " << path << "\n";
    if (path.size() > 0)
      li_defined.emplace(path);
    /*
    // EntityType
    std::string q2{"//Attribute[@ReferenceType='EntityType' and @path='" + path + "']"};
    pugi::xpath_node_set ns2 = doc.select_nodes(q2.c_str());
    for (auto& n: ns2) {
      doc3.document_element().append_copy(n.node());
    }
    */
    counter++;
  }
  

  std::cout << "Sets =================================================\n";
  for (auto& a : members) {
    std::cout << a.key << "\n";

    for (auto& b : a.vals) {
      std::cout << b.first << ": " << b.second << "\n";

      if (b.first == "code") {
        // Non wrapper
        std::string q{"//Attribute[@path='" + a.key + "']"};
        pugi::xpath_node_set ns = doc.select_nodes(q.c_str());
        
        auto container = ns.first().node();

        std::string q1{"AttributeMember[@Value='" + b.second + "']"};
        pugi::xpath_node mSearch = container.select_node(q1.c_str());
        
        // Add if not found
        if (!mSearch) {
          pugi::xml_node m = container.append_child("AttributeMember");
          m.append_attribute("Value") = b.second.c_str();
        }        
      } else {
        // Wrapper

        // Make a copy of the element
        std::string q1{"AttributeMember[@Value='" + b.second + "']"};
        pugi::xpath_node mSearch = doc_c.select_node(q1.c_str());
        


      }

    }
  }

  std::cout << "Scalars =================================================\n";
  for (auto& a : scalars) {
    std::cout << "k: " << a.key << " v: " << a.val << "\n";
    //
    std::string q{"//Attribute[@path='" + a.key + "']"};
    pugi::xpath_node_set ns = doc.select_nodes(q.c_str());
    ns.first().node().attribute("Value").set_value(a.val.c_str());
  }

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

  for (auto& a : li_defined) {
    std::cout << a << "\n"; 
  }

  // Remove unused nodes from main type.
  removeUnusedNodes(doc, li2);

  for (auto& path : li_defined) {
    // SetType
    std::string q{"//Attribute[@ReferenceType='SetType' and @path='" + path + "']"};
    pugi::xpath_node_set ns1 = doc_c.select_nodes(q.c_str());
    for (auto& n: ns1) {
      doc2.document_element().append_copy(n.node());
    }
    
    // EntityType
    std::string q2{"//Attribute[@ReferenceType='EntityType' and @path='" + path + "']"};
    pugi::xpath_node_set ns2 = doc.select_nodes(q2.c_str());
    for (auto& n: ns2) {
      doc3.document_element().append_copy(n.node());
    }

    /*
    // Scalar
    std::string q3{"//Attribute[@ReferenceType='' and @path='" + path + "']"};
    pugi::xpath_node_set ns3 = doc.select_nodes(q3.c_str());
    for (auto& n: ns3) {
      doc4.document_element().append_copy(n.node());
    }
    */
  }
  //
  
  std::cout << li2.size() << "\n";
 
  // Reverse order from doc3.  Write to doc5
  pugi::xpath_node_set ns = doc3.select_nodes("//Attribute/WebService");
  for (auto& n: ns)
    li_depth.emplace(std::stoi(n.node().attribute("nodeDepth").value()));
  
  if(!li_depth.empty()) {
    int depth = *li_depth.rbegin();

    while ((depth + 1) > 0) {
      std::string q{"//Attribute[./WebService[@nodeDepth='" + std::to_string(depth) + "']]"};

      pugi::xpath_node_set ns = doc3.select_nodes(q.c_str());
      for (auto& n: ns)
        doc5.document_element().append_copy(n.node());
        
        // std::cout << depth << ": " << n.node().attribute("TypeName").value() << "\n";

      depth--;
    }
  }
    
  removeUnusedNodes(doc5, li2);

  
  doc.save_file("output.xml");
  // doc2.save(std::cout);
  doc2.save_file("output-2.xml");

  doc5.save_file("output-5.xml");
  
  return 0;
}
