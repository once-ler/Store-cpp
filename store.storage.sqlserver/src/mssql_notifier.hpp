#pragma once

#include <iostream>
#include <sstream>
#include <map>
#include <thread>
#include <mutex>
#include <chrono>
#include <regex>
#include "store.storage.sqlserver/src/mssql_dblib_base_client.hpp"
#include "store.common/src/spdlogger.hpp"

using namespace std;

namespace store::storage::mssql {
  using OnTableChangedFunc = function<void(string)>;

  class Notifier {
  public:
    const string version = "0.1.1";

    Notifier(shared_ptr<MsSqlDbLibBaseClient> sqlDbLibClient_, const string& databaseName_, const string& schemaName_, const string& tableName_): sqlDbLibClient(sqlDbLibClient_), databaseName(databaseName_), schemaName(schemaName_), tableName(tableName_) {
      // Remove brackets.
      databaseName = std::regex_replace(databaseName, std::regex("[\\[\\]]"), "");
      
      // Set Identity;
      stringstream ss;
      ss << std::hex << std::hash<std::string>{}(fmt::format("{}${}${}", databaseName, schemaName, tableName)); 
      identity = ss.str();

      compiledScripts[CompiledScript::INSTALL_LISTENER_SCRIPT] = installListenerScript();
      compiledScripts[CompiledScript::UNINSTALL_LISTENER_SCRIPT] = unstallListenerScript();      
      compiledScripts[CompiledScript::SQL_FORMAT_GET_DEPENDENCY_IDENTITIES] = sqlFormatGetDependencyIdentities();
      compiledScripts[CompiledScript::SQL_FORMAT_FORCED_DATABASE_CLEANING] = sqlFormatForcedDatabaseCleaning();
      compiledScripts[CompiledScript::MONITOR_CONVERSATIONS] = monitorConversations();
      compiledScripts[CompiledScript::SQL_FORMAT_CREATE_INSTALLATION_PROCEDURE] = sqlFormatCreateInstallationProcedure();
      compiledScripts[CompiledScript::SQL_FORMAT_CREATE_UNINSTALLATION_PROCEDURE] = sqlFormatCreateUninstallationProcedure();
      compiledScripts[CompiledScript::SQL_FORMAT_RECEIVE_EVENT] = sqlFormatReceiveEvent();
    }

    enum CompiledScript {
      INSTALL_LISTENER_SCRIPT,
      UNINSTALL_LISTENER_SCRIPT,
      SQL_FORMAT_GET_DEPENDENCY_IDENTITIES,
      SQL_FORMAT_FORCED_DATABASE_CLEANING,
      MONITOR_CONVERSATIONS,      
      SQL_FORMAT_CREATE_INSTALLATION_PROCEDURE,
      SQL_FORMAT_CREATE_UNINSTALLATION_PROCEDURE,      
      SQL_FORMAT_RECEIVE_EVENT    
    };

    void start(OnTableChangedFunc callback = [](string msg) { std::cout << msg << endl; }) {
      // Check if service is already installed.
      if (!checkServiceExists())
        installNotification();
      
      notificationLoop(callback);
    }

    void stop() {
      {
        std::unique_lock<std::mutex> lock(q_mutex);
        cancel = true;
      }
      this_thread::sleep_for(std::chrono::milliseconds(500));
      cout << "Thread " << notificationLoopThread.get_id() << " joined.\n";
      notificationLoopThread.join();
    }

    int uninstall() {
      return executeNonQuery(compiledScripts[CompiledScript::UNINSTALL_LISTENER_SCRIPT]);
    }

    vector<vector<string>> getDependencyDbIdentities() {
      string sqlstmt = compiledScripts[CompiledScript::SQL_FORMAT_GET_DEPENDENCY_IDENTITIES];
      vector<string> fieldNames;
      vector<vector<string>> fieldValues;
      sqlDbLibClient->quick(istringstream(sqlstmt), fieldNames, fieldValues);
      return fieldValues;
    }

    // Warning: remove ALL listeners in the database.
    int cleanDatabase() {
      cout << "Cleaning started." << endl;
      return executeNonQuery(compiledScripts[CompiledScript::SQL_FORMAT_FORCED_DATABASE_CLEANING]);
    }

    vector<vector<string>> monitor() {
      vector<string> fieldNames;
      vector<vector<string>> fieldValues;
      sqlDbLibClient->quick(istringstream(compiledScripts[CompiledScript::MONITOR_CONVERSATIONS]), fieldNames, fieldValues);
      return fieldValues;
    }

    int executeNonQuery(const string& input) {
      std::shared_ptr<MSSQLDbLibConnection> conn = nullptr;
      
      try {
        conn = sqlDbLibClient->pool->borrow();
        auto db = conn->sql_connection;
        db->sql(input);
        int rc = db->execute();

        if (rc) {
          sqlDbLibClient->pool->unborrow(conn);
          return rc;
        }
      } catch (active911::ConnectionUnavailable ex) {
        cerr << ex.what() << endl;
      } catch (std::exception e) {
        cerr << e.what() << endl;
      }
      if (conn)
        sqlDbLibClient->pool->unborrow(conn);
    }

  private:
    shared_ptr<MsSqlDbLibBaseClient> sqlDbLibClient = nullptr;
    std::mutex q_mutex;
    bool cancel = false;
    string databaseName;
    string schemaName;
    string tableName;
    string identity;
    int COMMAND_TIMEOUT = 60000;    
    std::map<CompiledScript, string> compiledScripts;
    std::thread notificationLoopThread;

    string conversationQueueName() { 
      return fmt::format("ListenerQueue_{0}", identity); 
    }

    string conversationServiceName() { 
      return fmt::format("ListenerService_{0}", identity); 
    }

    string conversationTriggerName() { 
      return fmt::format("tr_Listener_{0}", identity); 
    }

    string installListenerProcedureName() { 
      return fmt::format("sp_InstallListenerNotification_{0}", identity); 
    }

    string uninstallListenerProcedureName() { 
      return fmt::format("sp_UninstallListenerNotification_{0}", identity); 
    }

    string installListenerScript() {
      return fmt::format(
        SQL_FORMAT_EXECUTE_PROCEDURE, databaseName, installListenerProcedureName(), schemaName
      );
    }

    string unstallListenerScript() {
      return fmt::format(
        SQL_FORMAT_EXECUTE_PROCEDURE, databaseName, uninstallListenerProcedureName(), schemaName
      );
    }

    std::map<string, string> notificationTypes = { 
      { "ALL", "INSERT, UPDATE, DELETE" }, 
      { "UPDATE", "UPDATE" },
      { "INSERT", "INSERT" },
      { "DELETE", "DELETE" }
    };

    std::map<string, string> detailsIncluded = {
      { "YES", "" },
      { "NO", "NOT" }
    };
    
    int installNotification() {
      executeNonQuery(compiledScripts[CompiledScript::SQL_FORMAT_CREATE_INSTALLATION_PROCEDURE]);
      executeNonQuery(compiledScripts[CompiledScript::SQL_FORMAT_CREATE_UNINSTALLATION_PROCEDURE]);
      return executeNonQuery(compiledScripts[CompiledScript::INSTALL_LISTENER_SCRIPT]);
    }

    void notificationLoop(OnTableChangedFunc callback) {
      // Callback function must be copied to thread.
      auto th = thread([callback, this](){
        try {
          while (true) {
            {
              std::unique_lock<std::mutex> lock(q_mutex);
              if (cancel) {
                cout << "Cancel requested." << endl;
                return;
              }
            }

            vector<string> fieldNames;
            vector<vector<string>> fieldValues;
            sqlDbLibClient->quick(istringstream(compiledScripts[CompiledScript::SQL_FORMAT_RECEIVE_EVENT]), fieldNames, fieldValues);

            string message{};
            if (fieldValues.size() > 0)
              message = fieldValues.at(0).at(0);

            callback(message);

            this_thread::sleep_for(std::chrono::milliseconds(100));
          }
        } catch (...) {
          // NOOP
        }
      });
      
      // th.detach();
      notificationLoopThread = std::move(th);
      // notificationLoopThread.detach();
    }

    bool checkServiceExists() {
      vector<vector<string>> fieldValues = getDependencyDbIdentities();
      return fieldValues.size() > 0;
    }

    // https://www.mssqltips.com/sqlservertip/1197/service-broker-troubleshooting/
    // Error messages in the queue: SELECT * FROM sys.transmission_queue;
    // Message Types: sys.service_message_types;
    // Contracts sys.service_contracts;
    string monitorConversations() {
      return fmt::format(R"__(
        SELECT conversation_handle, is_initiator, s.name as 'local service', 
        far_service, sc.name 'contract', state_desc, convert(varchar(30), security_timestamp, 126) ts
        FROM sys.conversation_endpoints ce
        JOIN sys.services s
        ON ce.service_id = s.service_id
        LEFT JOIN sys.service_contracts sc
        ON ce.service_contract_id = sc.service_contract_id
        WHERE s.name = '{}'
        ORDER BY security_timestamp
      )__", conversationServiceName());
    }

    string SQL_FORMAT_INSTALL_SEVICE_BROKER_NOTIFICATION() {
      return fmt::format(R"__(
        IF EXISTS (SELECT * FROM sys.databases 
            WHERE name = ''{0}'' AND (is_broker_enabled = 0 OR is_trustworthy_on = 0)) 
        BEGIN
            ALTER DATABASE [{0}] SET SINGLE_USER WITH ROLLBACK IMMEDIATE
            ALTER DATABASE [{0}] SET ENABLE_BROKER; 
            ALTER DATABASE [{0}] SET MULTI_USER WITH ROLLBACK IMMEDIATE
            -- FOR SQL Express
            ALTER AUTHORIZATION ON DATABASE::[{0}] TO [sa]
            ALTER DATABASE [{0}] SET TRUSTWORTHY ON;
        END
        -- Create a queue which will hold the tracked information 
        IF NOT EXISTS (SELECT * FROM sys.service_queues WHERE name = ''{1}'')
            CREATE QUEUE {3}.[{1}]
        -- Create a service on which tracked information will be sent 
        IF NOT EXISTS(SELECT * FROM sys.services WHERE name = ''{2}'')
            CREATE SERVICE [{2}] ON QUEUE {3}.[{1}] ([DEFAULT])
        -- Create a local address route.
        IF NOT EXISTS (SELECT * FROM sys.routes WHERE [address] = ''LOCAL'')
            CREATE ROUTE [AutoCreatedLocal] with address = N''LOCAL''    
      )__", databaseName, conversationQueueName(), conversationServiceName(), schemaName);
    }

    string SQL_FORMAT_CHECK_NOTIFICATION_TRIGGER() {
      return fmt::format(R"__(
        IF OBJECT_ID (''{1}.{0}'', ''TR'') IS NOT NULL
            RETURN;
      )__", conversationTriggerName(), schemaName);
    }

    string SQL_FORMAT_CREATE_NOTIFICATION_TRIGGER() {
      return fmt::format(R"__(
        CREATE TRIGGER [{1}]
        ON {5}.[{0}]
        AFTER {2} 
        AS
        SET NOCOUNT ON;
        --Trigger {0} is rising...
        IF EXISTS (SELECT * FROM sys.services WHERE name = ''''{3}'''')
        BEGIN
            DECLARE @message NVARCHAR(MAX)
            SET @message = N''''<root/>''''
            
            IF ({4} EXISTS(SELECT 1))
            BEGIN
                DECLARE @retvalOUT NVARCHAR(MAX)
                %inserted_select_statement%
                IF (@retvalOUT IS NOT NULL)
                BEGIN SET @message = N''''<root>'''' + @retvalOUT END                        
                %deleted_select_statement%
                IF (@retvalOUT IS NOT NULL)
                BEGIN
                    IF (@message = N''''<root/>'''') BEGIN SET @message = N''''<root>'''' + @retvalOUT END
                    ELSE BEGIN SET @message = @message + @retvalOUT END
                END 
                IF (@message != N''''<root/>'''') BEGIN SET @message = @message + N''''</root>'''' END
            END
          --Beginning of dialog...
          DECLARE @ConvHandle UNIQUEIDENTIFIER
          --Determine the Initiator Service, Target Service and the Contract 
          BEGIN DIALOG @ConvHandle 
                FROM SERVICE [{3}] TO SERVICE ''''{3}'''' ON CONTRACT [DEFAULT] WITH ENCRYPTION=OFF, LIFETIME = 60; 
          --Send the Message
          SEND ON CONVERSATION @ConvHandle MESSAGE TYPE [DEFAULT] (@message);
          --End conversation
          END CONVERSATION @ConvHandle;
        END
      )__", tableName, conversationTriggerName(), notificationTypes["ALL"], conversationServiceName(), detailsIncluded["YES"], schemaName);
    }

    string sqlFormatCreateInstallationProcedure() {
      return fmt::format(R"__(
        USE [{0}]

        IF OBJECT_ID ('{6}.{1}', 'P') IS NULL
        BEGIN
            EXEC ('
            CREATE PROCEDURE {6}.{1}
            AS
            BEGIN
                -- Service Broker configuration statement.
                {2}
                
                -- Notification Trigger check statement.
                {4}
                
                -- Notification Trigger configuration statement.
                DECLARE @triggerStatement NVARCHAR(MAX)
                DECLARE @select NVARCHAR(MAX)
                DECLARE @sqlInserted NVARCHAR(MAX)
                DECLARE @sqlDeleted NVARCHAR(MAX)
                
                SET @triggerStatement = N''{3}''
                
                SET @select = STUFF((SELECT '','' + ''['' + COLUMN_NAME + '']''
                        FROM INFORMATION_SCHEMA.COLUMNS
                        WHERE DATA_TYPE NOT IN  (''text'',''ntext'',''image'',''geometry'',''geography'') AND TABLE_SCHEMA = ''{6}'' AND TABLE_NAME = ''{5}'' AND TABLE_CATALOG = ''{0}''
                        FOR XML PATH ('''')
                        ), 1, 1, '''')

                SET @sqlInserted = 
                    N''SET @retvalOUT = (SELECT '' + @select + N'' 
                                        FROM INSERTED 
                                        FOR XML PATH(''''row''''), ROOT (''''inserted''''))''

                SET @sqlDeleted = 
                    N''SET @retvalOUT = (SELECT '' + @select + N'' 
                                        FROM DELETED 
                                        FOR XML PATH(''''row''''), ROOT (''''deleted''''))''                            

                SET @triggerStatement = REPLACE(@triggerStatement
                                        , ''%inserted_select_statement%'', @sqlInserted)

                SET @triggerStatement = REPLACE(@triggerStatement
                                        , ''%deleted_select_statement%'', @sqlDeleted)

                EXEC sp_executesql @triggerStatement

            END
          ')
        END
      )__", 
        databaseName, 
        installListenerProcedureName(), 
        SQL_FORMAT_INSTALL_SEVICE_BROKER_NOTIFICATION(), 
        SQL_FORMAT_CREATE_NOTIFICATION_TRIGGER(),
        SQL_FORMAT_CHECK_NOTIFICATION_TRIGGER(),
        tableName,
        schemaName  
      );
    }

    string SQL_FORMAT_UNINSTALL_SERVICE_BROKER_NOTIFICATION() {
      return fmt::format(R"__(
        DECLARE @serviceId INT
        SELECT @serviceId = service_id FROM sys.services 
        WHERE sys.services.name = ''{1}''
        DECLARE @ConvHandle uniqueidentifier
        DECLARE Conv CURSOR FOR
        SELECT CEP.conversation_handle FROM sys.conversation_endpoints CEP
        WHERE CEP.service_id = @serviceId AND ([state] != ''CD'' OR [lifetime] > GETDATE() + 1)
        OPEN Conv;
        FETCH NEXT FROM Conv INTO @ConvHandle;
        WHILE (@@FETCH_STATUS = 0) BEGIN
          END CONVERSATION @ConvHandle WITH CLEANUP;
            FETCH NEXT FROM Conv INTO @ConvHandle;
        END
        CLOSE Conv;
        DEALLOCATE Conv;
        -- Droping service and queue.
        IF (@serviceId IS NOT NULL)
            DROP SERVICE [{1}];
        IF OBJECT_ID (''{2}.{0}'', ''SQ'') IS NOT NULL
          DROP QUEUE {2}.[{0}];
      )__", conversationQueueName(), conversationServiceName(), schemaName);
    }

    string SQL_FORMAT_DELETE_NOTIFICATION_TRIGGER() {
      return fmt::format(R"__(
        IF OBJECT_ID (''{1}.{0}'', ''TR'') IS NOT NULL
            DROP TRIGGER {1}.[{0}];  
      )__", conversationTriggerName(), schemaName);
    }

    string sqlFormatCreateUninstallationProcedure() {
      return fmt::format(R"__(
        USE [{0}]

        IF OBJECT_ID ('{4}.{1}', 'P') IS NULL
        BEGIN
            EXEC ('
                CREATE PROCEDURE {4}.{1}
                AS
                BEGIN
                    -- Notification Trigger drop statement.
                    {3}
                    -- Service Broker uninstall statement.
                    {2}
                    IF OBJECT_ID (''{4}.{5}'', ''P'') IS NOT NULL
                        DROP PROCEDURE {4}.{5}
                    
                    DROP PROCEDURE {4}.{1}
                END
                ')
        END
      )__", 
        databaseName, 
        uninstallListenerProcedureName(), 
        SQL_FORMAT_UNINSTALL_SERVICE_BROKER_NOTIFICATION(),
        SQL_FORMAT_DELETE_NOTIFICATION_TRIGGER(),
        schemaName,
        installListenerProcedureName()
      );
    }

    string sqlFormatForcedDatabaseCleaning() {
      return fmt::format(R"__(
        USE [{0}]
        DECLARE @db_name VARCHAR(MAX)
        SET @db_name = '{0}' -- provide your own db name                
        DECLARE @proc_name VARCHAR(MAX)
        DECLARE procedures CURSOR
        FOR
        SELECT   sys.schemas.name + '.' + sys.objects.name
        FROM    sys.objects 
        INNER JOIN sys.schemas ON sys.objects.schema_id = sys.schemas.schema_id
        WHERE sys.objects.[type] = 'P' AND sys.objects.[name] like 'sp_UninstallListenerNotification_%'
        OPEN procedures;
        FETCH NEXT FROM procedures INTO @proc_name
        WHILE (@@FETCH_STATUS = 0)
        BEGIN
        EXEC ('USE [' + @db_name + '] EXEC ' + @proc_name + ' IF (OBJECT_ID (''' 
                        + @proc_name + ''', ''P'') IS NOT NULL) DROP PROCEDURE '
                        + @proc_name)
        FETCH NEXT FROM procedures INTO @proc_name
        END
        CLOSE procedures;
        DEALLOCATE procedures;
        DECLARE procedures CURSOR
        FOR
        SELECT   sys.schemas.name + '.' + sys.objects.name
        FROM    sys.objects 
        INNER JOIN sys.schemas ON sys.objects.schema_id = sys.schemas.schema_id
        WHERE sys.objects.[type] = 'P' AND sys.objects.[name] like 'sp_InstallListenerNotification_%'
        OPEN procedures;
        FETCH NEXT FROM procedures INTO @proc_name
        WHILE (@@FETCH_STATUS = 0)
        BEGIN
        EXEC ('USE [' + @db_name + '] DROP PROCEDURE '
                        + @proc_name)
        FETCH NEXT FROM procedures INTO @proc_name
        END
        CLOSE procedures;
        DEALLOCATE procedures;
      )__", databaseName);
    }

    string sqlFormatReceiveEvent() {
      return fmt::format(R"__(
        DECLARE @ConvHandle UNIQUEIDENTIFIER
        DECLARE @message VARBINARY(MAX)
        USE [{0}]
        WAITFOR (RECEIVE TOP(1) @ConvHandle=Conversation_Handle
            , @message=message_body FROM {3}.[{1}]), TIMEOUT {2};
                    
            BEGIN TRY END CONVERSATION @ConvHandle; END TRY BEGIN CATCH END CATCH
        SELECT CAST(@message AS NVARCHAR(4000))
      )__", databaseName, conversationQueueName(), COMMAND_TIMEOUT, schemaName);
    }

    string sqlFormatGetDependencyIdentities() { 
      return fmt::format(R"__(
        USE [{}]
                  
        SELECT name FROM sys.services WHERE [name] = '{}';
      )__", databaseName, identity);
    }

    string SQL_FORMAT_EXECUTE_PROCEDURE = R"__(
      USE [{0}]
      IF OBJECT_ID ('{2}.{1}', 'P') IS NOT NULL
          EXEC {2}.{1}
    )__";

  };
}
