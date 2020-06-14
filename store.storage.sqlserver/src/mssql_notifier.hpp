#pragma once

#include <iostream>
#include <sstream>
#include <map>
#include <thread>
#include "store.storage.sqlserver/src/mssql_dblib_base_client.hpp"
#include "store.common/src/spdlogger.hpp"

using namespace std;

namespace store::storage::mssql {
  using OnTableChangedFunc = function<void(string)>;

  class Notifier : public MsSqlDbLibBaseClient {
  public:
    Notifier(const string& databaseName_, const string& schemaName_, const string& tableName_): databaseName(databaseName_), schemaName(schemaName_), tableName(tableName_) {
      // Set Identity;
      stringstream ss;
      ss << std::hex << std::hash<std::string>{}(fmt::format("{}${}${}", databaseName, schemaName, tableName)); 
      identity = ss.str();
    }

    void* start(OnTableChangedFunc callback = [](string msg) { std::cout << msg << endl; }) {
      // Check if service is already installed.
      if (checkServiceExists())
        return NULL;
      
      installNotification();

      return &(std::thread([&callback, this](){ notificationLoop(callback); }));      
    }

    int stop() {
      string unstallListenerScript = fmt::format(
        SQL_FORMAT_EXECUTE_PROCEDURE, databaseName, uninstallListenerProcedureName(), schemaName
      );
      return executeNonQuery(unstallListenerScript);
    }

    vector<vector<string>> getDependencyDbIdentities() {
      string sqlstmt = fmt::format(SQL_FORMAT_GET_DEPENDENCY_IDENTITIES, databaseName);
      vector<string> fieldNames;
      vector<vector<string>> fieldValues;
      quick(istringstream(sqlstmt), fieldNames, fieldValues);
      return fieldValues;
    }

    int cleanDatabase() {
      string cleanDbScript = fmt::format(SQL_FORMAT_FORCED_DATABASE_CLEANING(), databaseName);
      return executeNonQuery(cleanDbScript);
    }

  private:
    shared_ptr<MsSqlDbLibBaseClient> client = nullptr;
    string databaseName;
    string schemaName;
    string tableName;
    string identity;
    int COMMAND_TIMEOUT = 60000;
    
    string conversationQueueName() { return fmt::format("ListenerQueue_{0}", identity); };
    string conversationServiceName() { return fmt::format("ListenerService_{0}", identity); };
    string conversationTriggerName() { return fmt::format("tr_Listener_{0}", identity); };
    string installListenerProcedureName() { return fmt::format("sp_InstallListenerNotification_{0}", identity); };
    string uninstallListenerProcedureName() { return fmt::format("sp_UninstallListenerNotification_{0}", identity); };
    
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
    
    int executeNonQuery(const string& input) {
      std::shared_ptr<MSSQLDbLibConnection> conn = nullptr;
      
      try {
        conn = pool->borrow();
        auto db = conn->sql_connection;
        db->sql(input);
        int rc = db->execute();

        if (rc) {
          pool->unborrow(conn);
          return rc;
        }
      } catch (active911::ConnectionUnavailable ex) {
        cerr << ex.what() << endl;
      } catch (std::exception e) {
        cerr << e.what() << endl;
      }
      if (conn)
        pool->unborrow(conn);
    }

    int installNotification() {
      executeNonQuery(SQL_FORMAT_CREATE_INSTALLATION_PROCEDURE());
      executeNonQuery(SQL_FORMAT_CREATE_UNINSTALLATION_PROCEDURE());

      string installListenerScript = fmt::format(
        SQL_FORMAT_EXECUTE_PROCEDURE, databaseName, installListenerProcedureName(), schemaName
      );
      return executeNonQuery(installListenerScript);
    }

    void notificationLoop(OnTableChangedFunc callback) {
      try {
        while (true) {
          vector<string> fieldNames;
          vector<vector<string>> fieldValues;
          quick(istringstream(SQL_FORMAT_RECEIVE_EVENT()), fieldNames, fieldValues);
          
          string message{};
          if (fieldValues.size() > 0)
            message = fieldValues.at(0).at(0);
          
          callback(message);
        }
      } catch (...) {
        // NOOP
      }
    }

    bool checkServiceExists() {
      string sqlstmt = fmt::format("select name from sys.services where name = '{}'", identity);
      vector<string> fieldNames;
      vector<vector<string>> fieldValues;
      quick(istringstream(sqlstmt), fieldNames, fieldValues);
      return fieldValues.size() > 0;
    }

    // SELECT is_broker_enabled FROM sys.databases WHERE Name = 'mydatabasename'
    // SELECT * FROM sys.service_queues WHERE name LIKE 'SqlQueryNotificationService-%' (ListenerQueue_{})
    // select * from sys.dm_qn_subscriptions

    // https://www.mssqltips.com/sqlservertip/1197/service-broker-troubleshooting/
    // Error messages in the queue: SELECT * FROM sys.transmission_queue;
    // Message Types: sys.service_message_types;
    // Contracts sys.service_contracts;
    string MONITOR_CONVERSATIONS() {
      return fmt::format(R"__(
        SELECT conversation_handle, is_initiator, s.name as 'local service', 
        far_service, sc.name 'contract', state_desc
        FROM sys.conversation_endpoints ce
        LEFT JOIN sys.services s
        ON ce.service_id = s.service_id
        LEFT JOIN sys.service_contracts sc
        ON ce.service_contract_id = sc.service_contract_id;
      )__");
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

    string SQL_FORMAT_CREATE_INSTALLATION_PROCEDURE() {
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
        installListenerProcedureName, 
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

    string SQL_FORMAT_CREATE_UNINSTALLATION_PROCEDURE() {
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
        uninstallListenerProcedureName, 
        SQL_FORMAT_UNINSTALL_SERVICE_BROKER_NOTIFICATION(),
        SQL_FORMAT_DELETE_NOTIFICATION_TRIGGER(),
        schemaName,
        installListenerProcedureName
      );
    }

    string SQL_FORMAT_FORCED_DATABASE_CLEANING() {
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

    string SQL_FORMAT_RECEIVE_EVENT() {
      return fmt::format(R"__(
        DECLARE @ConvHandle UNIQUEIDENTIFIER
        DECLARE @message VARBINARY(MAX)
        USE [{0}]
        WAITFOR (RECEIVE TOP(1) @ConvHandle=Conversation_Handle
            , @message=message_body FROM {3}.[{1}]), TIMEOUT {2};
                    
            BEGIN TRY END CONVERSATION @ConvHandle; END TRY BEGIN CATCH END CATCH
        SELECT CAST(@message AS NVARCHAR(MAX))
      )__", databaseName, conversationQueueName(), COMMAND_TIMEOUT, schemaName);
    }

    string SQL_FORMAT_EXECUTE_PROCEDURE = R"__(
      USE [{0}]
      IF OBJECT_ID ('{2}.{1}', 'P') IS NOT NULL
          EXEC {2}.{1}
    )__";

    string SQL_FORMAT_GET_DEPENDENCY_IDENTITIES = R"__(
      USE [{0}]
                
      SELECT REPLACE(name , 'ListenerService_' , '') 
      FROM sys.services 
      WHERE [name] like 'ListenerService_%';
    )__";

  };
}
