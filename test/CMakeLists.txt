# rm -rf build && mkdir build && cd build
# cmake .. && cd .. && make
cmake_minimum_required(VERSION 3.2)

project(testing VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_CXX_FLAGS "-fno-stack-protector")

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(OpenSSL)
find_package(Threads)
find_package(Boost 1.62.0 COMPONENTS thread date_time program_options filesystem system regex REQUIRED)

# Installl libpq.so for ubuntu xenial:
# sudo apt-get install libpq-dev (10.4)
# sudo apt-get install postgresql-server-dev-10
# May also need:
# sudo apt-get install postgresql-client
# sudo ln -s /usr/lib/libpq.so.5.4  /usr/lib/libpq.so  # adapt to the actual postgres version
# Configure libpq
find_package(PostgreSQL REQUIRED)
# include_directories(${PostgreSQL_INCLUDE_DIRS})
# link_directories(${PostgreSQL_LIBRARY_DIRS})

MESSAGE( STATUS "PROJECT_SOURCE_DIR: " ${PROJECT_SOURCE_DIR} )
MESSAGE( STATUS "CMAKE_CURRENT_SOURCE_DIR: " ${CMAKE_CURRENT_SOURCE_DIR} )
# MESSAGE( STATUS "PROJECT_SOURCE_DIR/src/index.cpp: " "${PROJECT_SOURCE_DIR}/../src/index.cpp" )

set(SIMPLE_WEB_SERVER ${PROJECT_SOURCE_DIR}/../../Simple-Web-Server)
set(SIMPLE_WEBSOCKET_SERVER ${PROJECT_SOURCE_DIR}/../../Simple-WebSocket-Server)
set(JSON ${PROJECT_SOURCE_DIR}/../../json/src)
# set(POSTGRESPP ${PROJECT_SOURCE_DIR}/../../postgrespp/postgrespp)

# Must use forked version for jsonb support
# https://github.com/once-ler/libpqmxx.git
set(LIBPQMXX ${PROJECT_SOURCE_DIR}/../../libpqmxx/include)

# https://github.com/eztier/tdspp.git
set(TDSPP ${PROJECT_SOURCE_DIR}/../../tdspp)


include_directories(
    /usr/local/include
    # /usr/include/postgresql
    ${PostgreSQL_INCLUDE_DIRS}
    /usr/local/include/bsoncxx/v_noabi
    /usr/local/include/mongocxx/v_noabi
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/../src"
    "${PROJECT_SOURCE_DIR}/../store.models/src"
    "${PROJECT_SOURCE_DIR}/../store.common/src"
    "${PROJECT_SOURCE_DIR}/../store.events/src"
    "${PROJECT_SOURCE_DIR}/../store.storage/src"
    "${PROJECT_SOURCE_DIR}/../store.storage.pgsql/src"
    "${PROJECT_SOURCE_DIR}/../store.storage.mongo/src"
    "${PROJECT_SOURCE_DIR}/../store.storage.sqlserver/src"
    "${PROJECT_SOURCE_DIR}/../test"
    "${JSON}"
    "${LIBPQMXX}"
    "${TDSPP}"
    )

# link_directories("${PROJECT_SOURCE_DIR}/../../postgrespp/lib")
link_directories(${PostgreSQL_LIBRARY_DIRS} "${PROJECT_SOURCE_DIR}/../../libpqmxx/build")
link_directories(${TDSPP_LIBRARY_DIRS} "${PROJECT_SOURCE_DIR}/../../tdspp")

# Probably not a good idea to glob.
# file(GLOB_RECURSE CPP_SOURCES "${PROJECT_SOURCE_DIR}/../index.cpp")
set(CPP_SOURCES ${PROJECT_SOURCE_DIR}/../test/index.cpp)
MESSAGE( STATUS "CPP_SOURCES: " ${CPP_SOURCES} )

add_executable(testing ${CPP_SOURCES})

# liblibpqmxx.so goes before ${PostgreSQL_LIBRARIES}
target_link_libraries(
    testing
    ${Boost_LIBRARIES}
    libmongocxx.so
    libbsoncxx.so
    liblibpqmxx.so
    libtds++.so
    libuuid.so
    ct
    ${PostgreSQL_LIBRARIES}
    ${TDSPP_LIBRARY_DIRS}   
)
