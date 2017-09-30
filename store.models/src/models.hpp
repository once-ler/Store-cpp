#pragma once

#include <boost/any.hpp>
#include <iostream>
#include <vector>
#include "primitive.hpp"

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
      string server;
      int port;
      string database;
      string userId;
      string password;
      int commandTimeout;
    };

    /// <summary>
    /// All models must implement IModel.
    /// Models include Membership, Senior Leadership, Research Program, Disease Management Group, etc.
    /// </summary>
    struct IModel {
    protected:
    };

    /// <summary>
    /// The most basic type.  Nothing else.
    /// </summary>
    struct Model : public IModel {
      Model() = default;
      Model(const string& _id, const string& _name, const string& _ts) : id(_id), name(_name), ts(_ts) {}
      string id;
      string name;
      Primitive::dateTime ts;
    };

    /// <summary>
    /// History of an IModel is added to the "history" attribute collection of Record{T} each time the user updates a record.
    /// </summary>
    /// <typeparam name="T"></typeparam>
    template<typename T>
    struct History : public Model {
      History() = default;
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
      Record(string id, string name, Primitive::dateTime ts, T _current, vector<History<T>> _history) : Model(id, name, ts), current(_current), history(_history) {}
    };

    /// <summary>
    /// VersionControl identifies the entire set of IModel collections (i.e. Membership, Senior Leadership, Research Program, Disease Management Group, etc).  
    /// Think of VersionControl as a "database".
    /// There is one master VersionControl and users can create clones of the master.
    /// </summary>
    struct VersionControl : public Model {
      VersionControl() = default;
      VersionControl(string id, string name, Primitive::dateTime ts) : Model(id, name, ts) {}
    };

    /// <summary>
    /// Participant wraps another Model.  The purpose of Participant is to timestamp when member was created for an Affiliation.
    /// </summary>
    struct Participant : public Model {
      Participant() = default;
      Participant(string id, string name, Primitive::dateTime ts, boost::any _party) : Model{ id, name, ts }, party(_party) {};
      boost::any party;
    };

    /// <summary>
    /// Affiliation is what it says.
    /// </summary>
    template<typename T, T = Participant>
    struct Affiliation : public Model {
      Affiliation() = default;
      Affiliation(string id, string name, Primitive::dateTime ts, vector<T> _roster) : Model(id, name, ts), roster(_roster) {}
      vector<T> roster;
    };
  }  
}
