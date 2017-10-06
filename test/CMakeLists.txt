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

MESSAGE( STATUS "PROJECT_SOURCE_DIR: " ${PROJECT_SOURCE_DIR} )
MESSAGE( STATUS "CMAKE_CURRENT_SOURCE_DIR: " ${CMAKE_CURRENT_SOURCE_DIR} )
# MESSAGE( STATUS "PROJECT_SOURCE_DIR/src/index.cpp: " "${PROJECT_SOURCE_DIR}/../src/index.cpp" )

set(SIMPLE_WEB_SERVER ${PROJECT_SOURCE_DIR}/../../Simple-Web-Server)
set(SIMPLE_WEBSOCKET_SERVER ${PROJECT_SOURCE_DIR}/../../Simple-WebSocket-Server)
set(JSON ${PROJECT_SOURCE_DIR}/../../json/src)
set(POSTGRESPP ${PROJECT_SOURCE_DIR}/../../postgrespp/postgrespp)

include_directories(
    /usr/local/include
    /usr/include/postgresql
    /usr/local/include/bsoncxx/v_noabi
    /usr/local/include/mongocxx/v_noabi
    "${PROJECT_SOURCE_DIR}/../src"
    "${PROJECT_SOURCE_DIR}/../store.models/src"
    "${PROJECT_SOURCE_DIR}/../store.storage/src"
    "${PROJECT_SOURCE_DIR}/../store.storage.pgsql/src"
    "${PROJECT_SOURCE_DIR}/../test"
    "${JSON}"
    "${POSTGRESPP}"
    )

link_directories("${PROJECT_SOURCE_DIR}/../../postgrespp/lib")

file(GLOB_RECURSE CPP_SOURCES "${PROJECT_SOURCE_DIR}/../index.cpp")
MESSAGE( STATUS "CPP_SOURCES: " ${CPP_SOURCES} )

add_executable(testing ${CPP_SOURCES})

target_link_libraries(
    testing
    ${Boost_LIBRARIES}
    libmongocxx.so
    libbsoncxx.so
    libpostgrespp.so
)
