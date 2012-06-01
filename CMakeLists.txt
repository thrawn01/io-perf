CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(perf-tests)

#ADD_EXECUTABLE(read-throughput read-throughput.c gethrxtime.c human.c argmatch.c)
ADD_EXECUTABLE(read-throughput read-throughput.c gethrxtime.c gettime.c)
TARGET_LINK_LIBRARIES(read-throughput rt ) 
