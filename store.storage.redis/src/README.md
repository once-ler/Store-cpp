# Installation
```https://github.com/Cylix/cpp_redis/wiki/Mac-&-Linux-Install```

# Clone the project
git clone https://github.com/Cylix/cpp_redis.git
# Go inside the project directory
cd cpp_redis
# Get tacopie submodule
git submodule init && git submodule update
# Create a build directory and move into it
mkdir build && cd build
# Generate the Makefile using CMake
cmake .. -DCMAKE_BUILD_TYPE=Release
# Build the library
make
# Install the library
make install
