CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(io-perf)

# Include the local directory
INCLUDE_DIRECTORIES(.)

ADD_EXECUTABLE(read read.c gethrxtime.c gettime.c)
TARGET_LINK_LIBRARIES(read rt) 

ADD_EXECUTABLE(read-threaded read-threaded.cpp gethrxtime.c gettime.c lz4/lz4.c lz4/lz4hc.c)
TARGET_LINK_LIBRARIES(read-threaded boost_thread ssl rt crypto)
