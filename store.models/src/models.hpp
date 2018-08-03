#pragma once

#include <boost/any.hpp>
#include <iostream>
#include <vector>
#include "store.models/src/primitive.hpp"

using namespace std;
namespace Primitive = store::primitive;

namespace store {
  namespace models {
    /// <summary>
    /// Should be a generalized endpoint to a persistant storage.
    /// Implementation details should not be made known.
    /// Examples:
    /// "MongoDB" -> new DBContext{ server = "127.0.0.1", port = 27017, database = "unicorns", userId = "foo", password = "bar", commandTimeout = 30000 }
    /// "PostgreSQL" -> new DBContext{ server = "127.0.0.1", port = 5432, database = "unicorns", userId = "foo", password = "bar", commandTimeout = 30000 }
    /// </summary>
    struct DBContext {
      DBContext() = default;
      ~DBContext() = default;
      DBContext(const string& _server, int _port, const string& _database)
        : server(_server), port(_port), database(_database) {
      }
      DBContext(const string& _applicationName, const string& _server, int _port, const string& _database, const string& _user, const string& _password, int _connectTimeout)
        : applicationName(_applicationName), server(_server), port(_port), database(_database), user(_user), password(_password), connectTimeout(_connectTimeout){}
      string applicationName = "store";
      string server = "127.0.0.1";
      int port = 5432;
      string database = "";
      string user = "";
      string password = "";
      int connectTimeout = 10;
    };

    /// <summary>
    /// All models must implement IModel.
    /// Models include Membership, Senior Leadership, Research Program, Disease Management Group, etc.
    /// </summary>
    struct IModel {
      IModel() = default;
      ~IModel() = default;
    protected:
    };

    /// <summary>
    /// The most basic type.  Nothing else.
    /// </summary>
    struct Model : public IModel {
      Model() = default;
      ~Model() = default;
      Model(const string& _id, const string& _name, const string& _ts, const string& _type = {}, const string& _related = {}) : id(_id), name(_name), ts(_ts), type(_type), related(_related) {}
      string id;
      string name;
      Primitive::dateTime ts;
      // In single hierarchical table, [type] and [related] should be used.
      string type;
      string related;
    };

    /// <summary>
    /// History of an IModel is added to the "history" attribute collection of Record{T} each time the user updates a record.
    /// </summary>
    /// <typeparam name="T"></typeparam>
    template<typename T>
    struct History : public Model {
      History() = default;
      ~History() = default;
      History(T _source) : source(_source) {};
      T source;
    };

    /// <summary>
    /// Record wraps the current IModel and its history, basically a collection of IModel. 
    /// </summary>
    /// <typeparam name="T">Typically, a Class that derives from Model.</typeparam>
    template<typename T>
    struct Record : public Model {
      T current;
      vector<History<T>> history;
      
      Record() = default;
      ~Record() = default;
      Record(string id, string name, Primitive::dateTime ts, T _current, vector<History<T>> _history) : Model(id, name, ts), current(_current), history(_history) {}
      Record(string id, string name, Primitive::dateTime ts, string type, string related, T _current, vector<History<T>> _history) : Model(id, name, ts, type, related), current(_current), history(_history) {}
    };

    /// <summary>
    /// VersionControl identifies the entire set of IModel collections (i.e. Membership, Senior Leadership, Research Program, Disease Management Group, etc).  
    /// Think of VersionControl as a "database".
    /// There is one master VersionControl and users can create clones of the master.
    /// </summary>
    struct VersionControl : public Model {
      VersionControl() = default;
      ~VersionControl() = default;
      VersionControl(string id, string name, Primitive::dateTime ts) : Model(id, name, ts) {}
    };

    /// <summary>
    /// Participant wraps another Model.  The purpose of Participant is to timestamp when member was created for an Affiliation.
    /// </summary>
    struct Participant : public Model {
      Participant() = default;
      ~Participant() = default;
      Participant(string id, string name, Primitive::dateTime ts, boost::any _party) : Model{ id, name, ts }, party(_party) {};
      boost::any party;
    };

    /// <summary>
    /// Affiliation is what it says.
    /// </summary>
    template<typename T>
    struct Affiliation : public Model {
      Affiliation() = default;
      ~Affiliation() = default;
      Affiliation(string id, string name, Primitive::dateTime ts, vector<T> _roster) : Model(id, name, ts), roster(_roster) {}
      vector<T> roster;
    };
  }  
}
