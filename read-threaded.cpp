#include "boost/threadpool.hpp"
#include <stdio.h>

using namespace boost::threadpool;

// Some example tasks
void first_task() {
    printf("first\n");
}

void second_task() {
    printf("second\n");
}

void third_task() {
    printf("third\n");
}


int main(int argc, char **argv){

    // Create a thread pool.
    pool tp(2);

    // Add some tasks to the pool.
    tp.schedule(&first_task);
    tp.schedule(&second_task);
    tp.schedule(&third_task);

    tp.wait();

    return 0;
}
