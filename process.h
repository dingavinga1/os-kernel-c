#include <iostream>

using namespace std;

#pragma once

struct entry{ /* Holds all information of a process in the pcb */
    string proc_name;
    int priority;
    float arrival_time;
    float cpu_time;
    float exec_time=0.0; //current executed time

    int io_times; //number of times i/o has to be executed
    int io_exec=0; //number of times i/o has been executed

    char proc_type;

    bool flag=false; //flag for delaying process i/o

    string status="RUNNING";
};

struct compareEntry{ /* overloading operator for comparison related to priority queue */
    bool operator()(entry* one, entry* two){
        return one->priority<two->priority; //maximum is top priority
    }
};

struct compareCpu{ /* overloading operator for comparison related to priority queue */
    bool operator()(entry* one, entry* two){
        return one->cpu_time>two->cpu_time; //minimum is top priority
    }
};

struct sortEntry{ /* overloading operator for comparison for sorting */
    inline bool operator()(const entry& one, const entry& two){
        return one.arrival_time<two.arrival_time;
    }
};