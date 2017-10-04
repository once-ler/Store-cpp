#pragma once

#include <iostream>
#include <vector>
#include <functional>
#include "models.hpp"
#include "enumerations.hpp"

using namespace std;
using namespace store::models;
using namespace store::enumerations;

namespace store {
  namespace interfaces {
    /// <summary>
    /// Class that creates a new copy of a "database" from a master "database" or from an existing "database".
    /// "Database" is an notion, it is up to you to implement what a "database" is.
    /// Typically, a "database" is thought of as a container of one or more tables.
    /// </summary>
    struct IVersionControlManager {
    protected:
      /// <summary>
      /// Get a list of all VersionControl available in the store.
      /// </summary>
      /// <returns>List of VersionControl.  The master VersionControl should not be an option.</returns>
      virtual vector<VersionControl> getVersionControls() {}

      /// <summary>
      /// Create a new VersionControl with a friendly name provided by the user.
      /// This will in effect clone all collections from the master and thus may be time consuming.
      /// On success, a new VersionControl identifier will be provided.
      /// </summary>
      /// <param name="friendlyName">User defined name for the new VersionControl.</param>
      /// <returns>A new VersionControl object of the attempted VersionControl creation.</returns>
      /// Note: If there was an error in creating, Exception will be thrown.
      /// It is up to you to catch it.
      virtual VersionControl createNewVersionControl(string friendlyName) {};

      /// <summary>
      /// Create a new VersionControl from an existing one.  This essentially creates a fork-join model where the latest changes of an existing VersionControl becomes a new branch.
      /// </summary>
      /// <param name="existingVersionId">An existing VersionControl identifier.</param>
      /// <param name="friendlyName">User defined name for the new VersionControl.</param>
      /// <returns>A new VersionControl object of the attempted VersionControl creation.</returns>
      /// Note: If there was an error in creating, Exception will be thrown.
      /// It is up to you to catch it.
      virtual VersionControl createNewVersionControlFromExisting(string existingVersionId, string friendlyName) {};
    };
    
    /// <summary>
    /// All models are kept in stores.
    /// All stores must implement IStore.
    /// Each IStore must provide the same API to save/list/archive.
    /// Note:
    /// Although each model implements their own getVersionControls() and createNewVersionControl() functions for VersionControl,
    /// versions are shared amongst all models.
    /// For example, IStore{Membership} and IStore{Grants} share the same access to VersionControl.
    /// </summary>
    /// <typeparam name="T">A type that implements IModel.</typeparam>
    template<typename T>
    struct IStore {
      using ListFunc = function<vector<Record<T>(string, int, int, string, SortDirection)>>;
      
      /*
      using ListAnyFunc = function<vector<U(string, string, int, int, string, string)>>;
      using MakeRecordFunc = function<Record<T>(U)>;
      using MakeRecordFromJsonString = function<Record<T>(string)>;
      using MergeFunc = function<T(T, T)>;
      using OneFunc = function<T(string, string, string)>;
      using OneRecordFunc = function<U(string, string, string)>;
      using OneAnyFunc = function<U(string, string, string, string)>;
      using ReplaceFromHistoryFunc = function<Record<T>(string, string, string)>;
      using SaveFunc = function<T(string, T)>;
      using SaveRecordFunc = function<Record<T>(string, T)>;
      using SearchRecordFunc = function<vector<Record<T>>(string, string, string, int, int, string, SortDirection)>;
      using SearchAnyFunc = function<vector<U>(string, string, string, string, int, int, string, string) >;
      using CountFunc = function<long(string, string, string)>;
      using AssociateFunc = function<Record<Affiliation<U>>(string, string, string)>;
      using DisassociateFunc = function<Record<Affiliation<U>>(string, string, string)>;
      */
      // ListFunc listFunc;
      // ListAnyFunc listAnyFunc;
      //MakeRecordFunc makeRecordFunc;
      //MakeRecordFromJsonString makeRecordFromJsonString;
      //MergeFunc mergeFunc;
      //OneFunc oneFunc;
      //OneRecordFunc oneRecordFunc;
      //OneAnyFunc oneAnyFunc;
      //ReplaceFromHistoryFunc replaceFromHistoryFunc;
      //SaveFunc saveFunc;
      //SaveRecordFunc saveRecordFunc;
      //SearchRecordFunc searchRecordFunc;
      //SearchAnyFunc searchAnyFunc;
      //CountFunc countFunc;
      //AssociateFunc associateFunc;
      //DisassociateFunc disassociateFunc;
      
      /// <summary>
      /// Fetch a limited number of IModel records for a VersionControl.
      /// Typically, this array will be serialized as a JSON array and sent to a client in a web application.
      /// </summary>
      /// <param name="version">The VersionControl identifier for this IModel.</param>
      /// <param name="offset">The index number to start the list.</param>
      /// <param name="limit">The total number of records to list.</param>
      /// <param name="sortKey">The field name to use when performing a sort.</param>
      /// <param name="sortDirection">The sort direction of sort key used for the records to list.</param>
      /// <returns>List of IModel records.</returns>
      //vector<Record<T>> list(string version, int offset = 0, int limit = 10, string sortKey = "id", SortDirection sortDirection = SortDirection::Asc) {
      //  return listFunc(version, offset, limit, sortKey, sortDirection);
      //}

      template<typename F>
      vector<Record<T>> list(F func, string version, int offset = 0, int limit = 10, string sortKey = "id", SortDirection sortDirection = SortDirection::Asc) {
        return func(version, offset, limit, sortKey, sortDirection);
      }
      
      /// <summary>
      /// Fetch a limited number of IModel records for a VersionControl.
      /// Targeted use by GraphQL RootType Query.
      /// </summary>
      /// <param name="version">The VersionControl identifier for this IModel.</param>
      /// <param name="typeOfStore">Name of store model as a string.</param>
      /// <param name="offset">The index number to start the list.</param>
      /// <param name="limit">The total number of records to list.</param>
      /// <param name="sortKey">The field name to use when performing a sort.</param>
      /// <param name="sortDirection">The sort direction of sort key used for the records to list.</param>
      /// <returns>List of IModel records.  Typically should be boost::any.</returns>
      //vector<U> list(string version, string typeOfStore, int offset, int limit, string sortKey = "id", string sortDirection = "Asc") {
        //return listAnyFunc(version, typeOfStore, offset, limit, sortKey, sortDirection);
      //}

      /// <summary>
      /// Provided a run-time dynamic object, convert it to a statically typed Record of an IModel.
      /// Note:
      /// This is typically used internally by the IStore implementation when data is fetched from the store and must be deserialized into a statically typed object.
      /// </summary>
      /// <typeparam name="U">U should be a type that derives from Model.</typeparam>
      /// <param name="d">The dynamic object.</param>
      /// <returns>One record of IModel.</returns>
      template<typename F, typename U>      
      Record<T> makeRecord(F func, U d) {
        return func(d);
      };

      /// <summary>
      /// Provided a JSON object string, convert it to a statically typed Record of an IModel.
      /// Note:
      /// This is typically used by a web applcation when it recieves a POST request from a client.
      /// </summary>
      /// <typeparam name="U">U should be a type that derives from Model.</typeparam>
      /// <param name="jsonString">A valid JSON object string.</param>
      /// <returns>One record of IModel.</returns>
      Record<T> makeRecord(string jsonString) {
        //return makeRecordFromJsonString(jsonString);
      };

      // <summary>
      /// Create a new IModel by combining an existing IModel with another IModel.
      /// The source IModel may only contain a subset of attributes.
      /// If the attributes do not appear in the source, naturally the destination IModel will keep the existing values for those missing attributes.
      /// </summary>
      /// <param name="dest">The IModel to merge into.</param>
      /// <param name="source">The IModel that may contain only subset of attributes.</param>
      /// <returns>A new IModel with any updated attribute values from the source.</returns>
      T merge(T dest, T source) {
        //return mergeFunc(dest, source);
      }

      /// <summary>
      /// Fetch one Record{IModel} record with the provided key/value for a VersionControl.
      /// Typically, this statically typed object will be serialized as a JSON object and sent to a client in a web application. 
      /// </summary>
      /// <typeparam name="U">IModel or Record{IModel}</typeparam>
      /// <param name="version">The VersionControl identifier for this IModel.</param>
      /// <param name="field">The key field to search.  If nested, use dot notation.</param>
      /// <param name="value">The value for the key field to search.</param>
      /// <param name="typeOfParty">Provided a type, the "party" attribute of a Participant will be attempted to convert to that type.</param>
      /// <returns>One record of IModel or just IModel if exists.  If no IModel exists, error will be thrown.</returns>
      //U one(string version, string field, string value) {
        //return oneFunc(version, field, value);
      //}

      //Record<U> one_record(string version, string field, string value) {
        //return oneRecordFunc(version, field, value);
      //}

      /// <summary>
      /// 
      /// </summary>
      /// <param name="version"></param>
      /// <param name="typeOfStore"></param>
      /// <param name="field"></param>
      /// <param name="value"></param>
      /// <returns>U would be boost::any</returns>
      //U one(string version, string typeOfStore, string field, string value) {
        //return oneAnyFunc(version, typeOfStore, field, value);
      //}

      /// <summary>
      /// The user elects to replace the current record with one found in the history collection.
      /// This is possible because we save a copy of IModel each time a user saves changes.
      /// </summary>
      /// <param name="version">The VersionControl identifier for this IModel.</param>
      /// <param name="recordId">The record identifier of Record<IModel> that the user wants to update.</param>
      /// <param name="historyId">The identifier of a historical IModel that the user wants to use as the current one.</param>
      /// <returns>Record{IModel} of the attempted replacement of the current IModel with one an historical one.</returns>
      /// Note: If there was an error in creating, Exception will be thrown.
      /// It is up to you to catch it.
      Record<T> replaceFromHistory(string version, string recordId, string historyId) {
        //return replaceFromHistoryFunc(version, recordId, historyId);
      }

      /// <summary>
      /// Persist the IModel to the store for a VerionControl.
      /// This typically occurs when user saves on the client in a web application.
      /// The IStore will clone the current record and deque it in the history attribute.
      /// Therefore, there will always be a copy of all changes made.
      /// The user may elect to replace the current IModel with one found in the history collection at any time. 
      /// </summary>
      /// <typeparam name="U">IModel or Record{IModel}</typeparam>
      /// <param name="version">The VersionControl identifier for this IModel.</param>
      /// <param name="doc">Record{T} where T is type of Personnel, ResearchProgram, etc.</param>
      /// <returns>The same Record{IModel} if successful.  Exception will be thrown is failure.</returns>
      /// Note: If there was an error in creating, Exception will be thrown.
      /// It is up to you to catch it.
      T save(string version, T doc) {
        //return saveFunc(version, doc);
      }

      Record<T> save_record(string version, T doc) {
        //return saveRecordFunc(version, doc);
      }

      /// <summary>
      /// Fetch IModel records for a VersionControl that meets the search criteria.
      /// Note:
      /// 1. By default, only 10 IModel records will be returned.
      /// 2. By default, POSIX regex insensitive matching will be used.
      /// </summary>
      /// <param name="version">The VersionControl identifier for this IModel.</param>
      /// <param name="field">The key field to search.  If nested, use dot notation.</param>
      /// <param name="search">The value for the key field to search.</param>
      /// <param name="offset">The index from which the search results should start.</param>
      /// <param name="limit">The limit of records to return.  Ideally, the limit should be realistic.</param>
      /// <param name="sortKey">The field name to use when performing a sort.</param>
      /// <param name="sortDirection">The sort direction of sort key used for the records to search.</param>
      /// <returns>Collection of Record{IModel} if successful.  Empty collection if failure.</returns>
      vector<Record<T>> search_record(string version, string field, string search, int offset = 0, int limit = 10, string sortKey = "id", SortDirection sortDirection = SortDirection::Asc) {
        //return searchRecordFunc(version, field, search, offset, limit, sortKey, sortDirection);
      }

      /// <summary>
      /// Fetch IModel records for a VersionControl that meets the search criteria.
      /// Note:
      /// 1. By default, only 10 IModel records will be returned.
      /// 2. By default, POSIX regex insensitive matching will be used.
      /// </summary>
      /// <param name="version">The VersionControl identifier for this IModel.</param>
      /// <param name="typeOfStore">Name of store model as a string.</param>
      /// <param name="field">The key field to search.  If nested, use dot notation.</param>
      /// <param name="search">The value for the key field to search.</param>
      /// <param name="offset">The index from which the search results should start.</param>
      /// <param name="limit">The limit of records to return.  Ideally, the limit should be realistic.</param>
      /// <param name="sortKey">The field name to use when performing a sort.</param>
      /// <param name="sortDirection">The sort direction of sort key used for the records to search.</param>
      /// <returns>Collection of Record{IModel} if successful.  Empty collection if failure.</returns>
      //vector<U> search(string version, string typeOfStore, string field, string search, int offset = 0, int limit = 10, string sortKey = "id", string sortDirection = "Asc") {
        //return searchAnyFunc(version, typeOfStore, field, search, offset, limit, sortKey, sortDirection);
      //}

      /// <summary>
      /// Count IModel records for a VersionControl that meets the search criteria.
      /// </summary>
      /// <param name="version">The VersionControl identifier for this IModel.</param>
      /// <param name="field">The key field to search.  If nested, use dot notation.</param>
      /// <param name="search">The value for the key field to search.</param>
      /// <returns>Count of records matching search criteria.</returns>
      long count(string version, string field = "", string search = "") {
        //return countFunc(version, field, search);
      }

      /// <summary>
      /// Associates a Model to an Affiliation object.
      /// The method does not add the Model directly.  The Model must be wrapped as the "party" attribute of a Participant type.
      /// The Participant type is then added to the "roster" attribute of the Affiliation.
      /// The Participant type of the Affiliation may be derived so that one can create additional attributes like "isLeader", and so forth.
      /// </summary>
      /// <typeparam name="U">Participant or derived Participant with additional fields.</typeparam>
      /// <typeparam name="M">Represents the type of the "party" attribute.</typeparam>
      /// <param name="version">The VersionControl identifier for this IModel.</param>
      /// <param name="recordId">The id of the Record type for an Affiliation.  An Affiliation can also be derived.  i.e. RebelAlliance, Empire, TradeFederation, Resistance, etc.</param>
      /// <param name="partyId">The id that uniquely identifies the Model referenced in the "party" attribute of the Participant.</param>
      /// <returns></returns>
      /// Affiliation<Participant>
      //Record<Affiliation<U>> associate(string version, string recordId, string partyId) {
        //return associateFunc(version, recordId, partyId);
      //}

      /// <summary>
      /// Disassociates a Model from an Affiliation object.
      /// </summary>
      /// <typeparam name="U">Participant or derived Participant with additional fields.</typeparam>
      /// <param name="version">The VersionControl identifier for this IModel.</param>
      /// <param name="recordId">The id of the Record type for an Affiliation.  An Affiliation can also be derived.  i.e. RebelAlliance, Empire, TradeFederation, Resistance, etc.</param>
      /// <param name="partyId">The id that uniquely identifies the Model referenced in the "party" attribute of the Participant.</param>
      /// <returns></returns>
      /// Affiliation<Participant>
      //Record<Affiliation<U>> disassociate(string version, string recordId, string partyId) {
        //return disassociate(version, recordId, partyId);
      //}
    };
  }  
}
