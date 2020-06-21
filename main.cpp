#include <iostream>
#include <thread>
#include "allocator.h"

using namespace std;

void thread_task() {
    char *tmp[5];
    for(auto & i : tmp)
        i = new char;
    for(auto & i : tmp)
        delete i;
}

int main() {

    int *pt = new int[10050];
    int *ppt = new int[10050];
    delete[] pt;
    delete[] ppt;

    char *tmp[5];
    for(auto & i : tmp)
        i = new char;
    for(auto & i : tmp)
        delete i;


    thread th[10];
    for(auto &i: th)
        i = thread(thread_task);
    for(auto &i: th)
        i.join();

    return 0;
}