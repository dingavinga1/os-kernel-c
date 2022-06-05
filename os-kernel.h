#include <iostream>
#include <utility>
#include <vector>
#include <cstdlib>
#include <sstream>
#include <algorithm>
#include "process.h"

using namespace std;

#pragma once

void init_proc(vector<entry>& pcb, string line){ /* parses read file to form process entries */
	stringstream ss(line);
	
	entry temp; //temporary entry to parse string
	
    /* PARSING STRING */
	ss>>temp.proc_name;
	ss>>temp.priority;
	ss>>temp.arrival_time;
	ss>>temp.proc_type;
	ss>>temp.cpu_time;
	ss>>temp.io_times;
	
	pcb.push_back(temp); //adding parsed process to pcb
}

void sortVec(vector<entry>& pcb){ /* sorts the given processes according to arrival time */
    std::sort(pcb.begin(), pcb.begin(), sortEntry());
}