#include <iostream>
#include <vector>
#include <cstdlib>
#include <fstream>

#include "os-kernel.h"
#include "process.h"
#include "scheduler.h"

using namespace std;

int main(int argc, char* argv[]){ /* main function with command line arguments */
/* DRIVER*/
	vector<entry> pcb; //main pcb 

	fstream ifile(argv[1], ios::in); //opening file provided by user
	string buff;
	getline(ifile, buff); //reading first line for no reason
	while(getline(ifile, buff)){
		init_proc(pcb, buff); //reading lines while they exist and passing them to initialiser
	}

    sortVec(pcb);
	
	if(argv[3][0]=='f'){ //first come first serve 
		fcfs start(&pcb, stoi(argv[2]), argv[4]);
	}
	else if(argv[3][0]=='r'){ //round robin
		round_robin start(&pcb, stoi(argv[2]), stoi(argv[4]), argv[5]);
	}
	else if(argv[3][0]=='p'){ //preemptive priority
        priority start(&pcb, stoi(argv[2]), argv[4]);
	}
    else if(argv[3][0]=='s'){ //preemptive priority
        sjf start(&pcb, stoi(argv[2]), argv[4]);
	}
	
	return 0;
}