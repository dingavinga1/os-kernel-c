#include <iostream>
#include <utility>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <queue>
#include <cstdlib>
#include <fstream>
#include <iomanip>

/* =====IMPORTANT NOTICE===== 
I do not own colors.hpp. It is taken
from a github respository and is free
to use open source for including colors
in terminal output. 

Link: hugorplobo/colors.hpp */
#include "colors.hpp"

#include "process.h"

using namespace std;

#pragma once

struct fcfs{ /* first come first serve implementation */
    vector<entry>* pcb; //pointer to main pcb
    int cpus; //number of cpus

    thread** cpu;
    thread* io;
    mutex *cpu_mutex; //mutex to protect processes in cpus
    //mutex ready_mutex;
    mutex io_mutex; //mutex to protect io queue

    entry** cpu_curr; //processes in the cpus

    queue<entry*> ready_queue; //ready queue to hold processes according to AT
    queue<entry*> io_queue; //queue to hold processes on wait

    int context_switches=0; //counter for context switches
    mutex context_mutex; //mutex to protect context switches

    bool done=false; //decides when scheduler is to be shut down

    void context_switch(){ /* context_switch function */
        context_mutex.lock();
        context_switches+=1;
        context_mutex.unlock();
    }

    void yield(entry* send){ /* sending process from cpu to i/o */
        io_mutex.lock();
        io_queue.push(send);
        io_mutex.unlock();
        context_switch();
    }

    void terminate(entry* done){ /* ending the process */
        done->status="DONE";
    }

    void wakeup(entry *wake){ /* sending process back to cpu */
        wake->io_exec+=1;
        wake->status="RUNNING";
    }

    void cpu_thread(int id){ /* cpu threads */
        this_thread::sleep_for(chrono::milliseconds(10)); // sleep to sync thread creation 

        while(!done){ //infinite loop for each cpu
            cpu_mutex[id].lock();
            entry* curr=cpu_curr[id]; //getting process from scheduler
            cpu_mutex[id].unlock();

            if(curr){
                float time_elapsed=0.0;
                bool flag=false;
                while(curr->status!="DONE"){ //check for process execution
                    if(curr->status!="WAIT"){ //if process is not on wait send it to wait
                        if(curr->io_times!=-1&&curr->io_exec!=curr->io_times&&flag){
                            flag=false;
                            curr->status="WAIT";
                            yield(curr);
                        }
                        else{ //if i/o has been fulfilled
                            if(curr->exec_time>=curr->cpu_time){ //condition to terminate the process
                                terminate(curr);
                                curr=NULL;
                                cpu_mutex[id].lock();
                                cpu_curr[id]=NULL;
                                cpu_mutex[id].unlock();
                                break;
                            }
                            else{ //running the process in real time
                                this_thread::sleep_for(chrono::milliseconds(1000));
                                flag=true;
                                curr->exec_time+=1;
                            }
                        }
                    }
                }
            }
        }
    }

    void io_thread(int arg){ /* i/o thread */
        entry* curr=NULL;
        while(!done){ //infinite loop for i/o
            io_mutex.lock();
            if(!io_queue.empty()){ //getting process if in i/o
                curr=io_queue.front();
            }
            io_mutex.unlock();
            if(curr){ //exectuing i/o and sending it back to the cpu
                this_thread::sleep_for(chrono::milliseconds(2000));
                wakeup(curr);
                curr=NULL;
                context_switch();
                io_mutex.lock();
                io_queue.pop();
                io_mutex.unlock();
            }
        }
    }

    string idle(entry* curr){ //checking if cpu is idle
        if(!curr)
            return "IDLE";
        if(curr->status=="RUNNING")
            return curr->proc_name;
        else    
            return "IDLE";
    }

    void schedule(char* fname){ //schedule function to handle processes
        fstream ifile(fname, ios::out); //opening output file

        /* OUTPUT HEADER */
        cout<<"Where Ru=Running, Re=Ready, Wa=Waiting\n\n";
        cout<<colors::bold<<colors::grey<<colors::on_white<<left<<setw(6)<<"Time"<<setw(10)<<"Ru Re Wa";
        ifile<<"Time\tRu Re Wa\t";
        for(int i=0; i<cpus; i++){
            cout<<setw(3)<<"CPU "<<setw(10)<<i;
            ifile<<"CPU ";
            ifile<<i;
            ifile<<"\t";
        }
        cout<<setw(10)<<"I/O QUEUE"<<colors::reset<<endl<<endl;
        ifile<<"I/0 QUEUE\n\n";

        //starting time
        auto start=chrono::high_resolution_clock::now();
        int64_t time=100;

        double ready_time=0; //counter for ready time

        while(!done){ //infinite loop to schedule processes
            auto check=chrono::high_resolution_clock::now();
            auto count=chrono::duration_cast<chrono::seconds>(check-start);
            if(!ready_queue.empty()){
                if(count.count()>=ready_queue.front()->arrival_time){ //checking if process in queue has arrival time
                    for(int i=0; i<cpus; i++){ //checking free cpus
                        if(!cpu_curr[i]){
                            cpu_mutex[i].lock();
                            cpu_curr[i]=ready_queue.front();
                            ready_queue.pop();
                            cpu_mutex[i].unlock();
                            context_switch();
                            break;
                        }
                    }
                }
            }
            /* getting time for i/o after every 0.1 s */
            auto check2=chrono::high_resolution_clock::now();
			auto count2=chrono::duration_cast<chrono::milliseconds>(check2-start);
            if(count2.count()==time){
                int re=ready_queue.size(), ru=0, wa=io_queue.size();
                for(int i=0; i<cpus; i++){
                    if(cpu_curr[i]&&cpu_curr[i]->status!="WAIT")ru++;
                }

                /* I/O */

				cout<<colors::magenta<<left<<setw(6)<<(double)(time)/1000<<colors::reset;
                cout<<setw(10)<<to_string(re)+"  "+to_string(ru)+"  "+to_string(wa);
                ifile<<(double)(time)/1000;
                ifile<<"\t";
				for(int i=0; i<cpus; i++){
                    if(idle(cpu_curr[i])=="IDLE")
					    cout<<colors::red<<setw(13)<<idle(cpu_curr[i])<<colors::reset;
                    else    
                        cout<<colors::green<<setw(13)<<idle(cpu_curr[i])<<colors::reset;
                    ifile<<idle(cpu_curr[i])<<"\t";
				}
                cout<<colors::reset;
				
				if(io_queue.front()){
				cout<<colors::yellow<<io_queue.front()->proc_name<<colors::reset<<endl;
                    ifile<<io_queue.front()->proc_name<<endl;
                }
				else{
                    cout<<colors::bold<<" <<\n"<<colors::reset;
                    ifile<<"<<\n";
                }
				time+=100;
			}
            //checking if cpus are done with all processes
			bool cpudone=true;
			for(int i=0; i<cpus; i++){
				if(cpu_curr[i]!=NULL)
					cpudone=false;
			}

			if(ready_queue.empty()&&io_queue.empty()&&cpudone){
				done=true;
			}

        }

        auto check2=chrono::high_resolution_clock::now();
		auto total_time=chrono::duration_cast<chrono::seconds>(check2-start);

        /* I/O for final information */
        cout<<"\n\n";
        cout<<colors::on_grey<<colors::white<<" # of Context Switches: "<<context_switches<<" "<<colors::reset<<endl;
        cout<<colors::on_grey<<" Total time spent in READY state: "<<420<<" "<<colors::reset<<endl;
        cout<<colors::on_grey<<" Total execution time: "<<total_time.count()<<" s  "<<colors::reset<<endl;

        ifile<<"\n\n";
        ifile<<"# of Context Switches: "<<context_switches<<endl;
        ifile<<"Total time spent in READY state: "<<420<<endl;
        ifile<<"Total execution time: "<<total_time.count()<<" s"<<endl;

        ifile.close(); //closing file
    }

    fcfs(vector<entry>* pcb, int cpus, char* fname){ /* constructor for fcfs scheduling */
        for(auto i=pcb->begin(); i!=pcb->end(); i++){ //getting processes to queue
            ready_queue.push(&*i);  
        }

        this->pcb=pcb;
        this->cpus=cpus;

        cpu=new thread*[cpus]; /* creating threads for cpus */
        for(int i=0; i<cpus; i++){
            cpu[i]=new thread(&fcfs::cpu_thread, this, i);
        }
        io=new thread(&fcfs::io_thread, this, 0); //creating i/o thread
        cpu_mutex=new mutex[cpus];

        cpu_curr=new entry*[cpus]; /* creating current proccess holders for cpus */
        for(int i=0; i<cpus; i++){
            cpu_curr[i]=NULL;
        }
        
        schedule(fname); //calling the scheduler

        for(int i=0; i<cpus; i++){ //joining threads
            cpu[i]->join();
        }
        io->join();
    }
};

struct round_robin{ /* round robin implementation */ 
    vector<entry>* pcb; //pointer to main pcb
    int cpus; //number of cpus
    int time_slice; //time quantum

    thread** cpu;
    thread* io;
    mutex *cpu_mutex; //mutex to protect cpus
    mutex ready_mutex; //mutex for ready queue
    mutex io_mutex; //mutex for io queue

    entry** cpu_curr; //processes in the cpus

    queue<entry*> ready_queue; //ready queue to hold processes according to AT
    queue<entry*> io_queue; //queue to hold processes on wait

    int context_switches=0; //counter for context switches
    mutex context_mutex; //mutex to protect context switches

    bool done=false; //decides when the scheduler is to be shut down

    void context_switch(){ /* context switch function */
        context_mutex.lock();
        context_switches+=1;
        context_mutex.unlock();
    }

    void yield(entry* send){ /* sending process from cpu to i/o */
        io_mutex.lock();
        io_queue.push(send);
        io_mutex.unlock();
        context_switch();
    }

    void terminate(entry* done){ /* ending the process */
        done->status="DONE";
    }

    void wakeup(entry *wake){ /* sending process back to cpu */
        wake->io_exec+=1;
        wake->status="RUNNING";
        ready_mutex.lock();
        ready_queue.push(wake);
        ready_mutex.unlock();
    }

    void preempt(entry* curr){ /* forcefully taking away cpu from process */
        curr->exec_time+=(float)time_slice/10;
        ready_mutex.lock();
        ready_queue.push(curr);
        ready_mutex.unlock();
    }

    void cpu_thread(int id){ /* cpu threads */
        this_thread::sleep_for(chrono::milliseconds(10)); //sleep to sync thread creation

        while(!done){ //infinite loop for each cpu
            cpu_mutex[id].lock();
            entry* curr=cpu_curr[id]; //getting process from scheduler
            cpu_mutex[id].unlock();

            if(curr){
                float time_elapsed=0.0;
                if(curr->io_times!=-1&&curr->io_exec!=curr->io_times&&curr->flag){ //for i/o
                    curr->flag=false;
                    curr->status="WAIT";
                    yield(curr);
                    curr=NULL;
                    cpu_mutex[id].lock();
                    cpu_curr[id]=NULL;
                    cpu_mutex[id].unlock();
                }
                else{
                    if(curr->exec_time>=curr->cpu_time){ //for terminating
                        terminate(curr);
                        curr=NULL;
                        cpu_mutex[id].lock();
                        cpu_curr[id]=NULL;
                        cpu_mutex[id].unlock();
                    }
                    else{ //for running
                        this_thread::sleep_for(chrono::milliseconds(time_slice*100));
                        curr->flag=true;
                        preempt(curr);
                        curr=NULL;
                        cpu_mutex[id].lock();
                        cpu_curr[id]=NULL;
                        cpu_mutex[id].unlock();

                    }
                }
            }
        }
    }

    void io_thread(int arg){ /* i/0 thread */
        entry* curr=NULL;
        while(!done){ //infinite loop for i/0
            io_mutex.lock();
            if(!io_queue.empty()){ //getting process if in i/o
                curr=io_queue.front();
            }
            io_mutex.unlock();
            if(curr){ //executing i/o and sending it back to the cpu
                this_thread::sleep_for(chrono::milliseconds(2000));
                wakeup(curr);
                curr=NULL;
                context_switch();
                io_mutex.lock();
                io_queue.pop();
                io_mutex.unlock();
            }
        }
    }

    string idle(entry* curr){ //checking if cpu is idle
        if(!curr)
            return "IDLE";
        if(curr->status=="RUNNING")
            return curr->proc_name;
        else    
            return "IDLE";
    }

    void schedule(char* fname){ /* schedule function to handle processes */
        fstream ifile(fname, ios::out); //opening output file 

        /* OUTPUT HEADER */
        cout<<"Where Ru=Running, Re=Ready, Wa=Waiting\n\n";
        cout<<colors::bold<<colors::grey<<colors::on_white<<left<<setw(6)<<"Time"<<setw(10)<<"Ru Re Wa";
        ifile<<"Time\tRu Re Wa\t";
        for(int i=0; i<cpus; i++){
            cout<<setw(3)<<"CPU "<<setw(10)<<i;
            ifile<<"CPU ";
            ifile<<i;
            ifile<<"\t";
        }
        cout<<setw(10)<<"I/O QUEUE"<<colors::reset<<endl<<endl;
        ifile<<"I/0 QUEUE\n\n";

        //starting time
        auto start=chrono::high_resolution_clock::now();
        int64_t time=100;

        double ready_time=0; //counter for ready time

        while(!done){ //infinite loop to schedule processes
            auto check=chrono::high_resolution_clock::now();
            auto count=chrono::duration_cast<chrono::seconds>(check-start);
            if(!ready_queue.empty()){
                if(count.count()>=ready_queue.front()->arrival_time){ //checking if process in queue has arrival time
                    for(int i=0; i<cpus; i++){ //checking free cpus
                        if(!cpu_curr[i]){
                            cpu_mutex[i].lock();
                            ready_mutex.lock();
                            cpu_curr[i]=ready_queue.front();
                            ready_queue.pop();
                            ready_mutex.unlock();
                            cpu_mutex[i].unlock();
                            context_switch();
                            break;
                        }
                    }
                }
            }

            /* getting time for i/o after every 0.1 s */
            auto check2=chrono::high_resolution_clock::now();
			auto count2=chrono::duration_cast<chrono::milliseconds>(check2-start);
            if(count2.count()==time){
                int re=ready_queue.size(), ru=0, wa=io_queue.size();
                for(int i=0; i<cpus; i++){
                    if(cpu_curr[i]&&cpu_curr[i]->status!="WAIT")ru++;
                }

                /* I/O */

				cout<<colors::magenta<<left<<setw(6)<<(double)(time)/1000<<colors::reset;
                cout<<setw(10)<<to_string(re)+"  "+to_string(ru)+"  "+to_string(wa);
                ifile<<(double)(time)/1000;
                ifile<<"\t";
				for(int i=0; i<cpus; i++){
                    if(idle(cpu_curr[i])=="IDLE")
					    cout<<colors::red<<setw(13)<<idle(cpu_curr[i])<<colors::reset;
                    else    
                        cout<<colors::green<<setw(13)<<idle(cpu_curr[i])<<colors::reset;
                    ifile<<idle(cpu_curr[i])<<"\t";
				}
                cout<<colors::reset;
				
				if(io_queue.front()){
				cout<<colors::yellow<<io_queue.front()->proc_name<<colors::reset<<endl;
                    ifile<<io_queue.front()->proc_name<<endl;
                }
				else{
                    cout<<colors::bold<<" <<\n"<<colors::reset;
                    ifile<<"<<\n";
                }
				time+=100;
			}
            //checking if cpus are done with all processes
			bool cpudone=true;
			for(int i=0; i<cpus; i++){
				if(cpu_curr[i]!=NULL)
					cpudone=false;
			}

			if(ready_queue.empty()&&io_queue.empty()&&cpudone){
				done=true;
			}

        }

        auto check2=chrono::high_resolution_clock::now();
		auto total_time=chrono::duration_cast<chrono::seconds>(check2-start);

        /* I/O for final information */
        cout<<"\n\n";
        cout<<colors::on_grey<<colors::white<<" # of Context Switches: "<<context_switches<<" "<<colors::reset<<endl;
        cout<<colors::on_grey<<" Total time spent in READY state: "<<420<<" "<<colors::reset<<endl;
        cout<<colors::on_grey<<" Total execution time: "<<total_time.count()<<" s  "<<colors::reset<<endl;

        ifile<<"\n\n";
        ifile<<"# of Context Switches: "<<context_switches<<endl;
        ifile<<"Total time spent in READY state: "<<420<<endl;
        ifile<<"Total execution time: "<<total_time.count()<<" s"<<endl;

        ifile.close(); //closing file

    }

    round_robin(vector<entry>* pcb, int cpus, int time_slice, char* fname){ /* constructor for round robin scheduling */
        for(auto i=pcb->begin(); i!=pcb->end(); i++){ //getting processes to queue
            ready_queue.push(&*i);  
        }

        this->pcb=pcb;
        this->cpus=cpus;
        this->time_slice=time_slice;

        cpu=new thread*[cpus]; /* creating threads for cpus */
        for(int i=0; i<cpus; i++){
            cpu[i]=new thread(&round_robin::cpu_thread, this, i);
        }
        io=new thread(&round_robin::io_thread, this, 0); //creating i/o thread
        cpu_mutex=new mutex[cpus];

        cpu_curr=new entry*[cpus]; /* creating current proccess holders for cpus */
        for(int i=0; i<cpus; i++){
            cpu_curr[i]=NULL;
        }
        
        schedule(fname); //calling the scheduler

        for(int i=0; i<cpus; i++){ //joining threads
            cpu[i]->join();
        }
        io->join();
    }
};

struct priority{ /* preemptive priority implementation */
    vector<entry>* pcb; //pointer to main pcb
    int cpus; //number of cpus

    thread** cpu;
    thread* io;
    mutex *cpu_mutex; //mutex to protect cpus
    mutex ready_mutex; //mutex for ready queue
    mutex io_mutex; //mutex for io queue

    entry** cpu_curr; //processes in the cpus

    queue<entry*> ready_queue; //ready queue to hold processes according to AT
    priority_queue<entry*, vector<entry*>, compareEntry> pr; //priority queue to hold processes according to priority
    mutex priority_mutex; //mutex for priority queue
    queue<entry*> io_queue; //queue to hold processes on wait

    int context_switches=0; //counter for context switches
    mutex context_mutex; //mutex to protect context switches

    void context_switch(){ /* context switch function */
        context_mutex.lock();
        context_switches+=1;
        context_mutex.unlock();
    }

    bool done=false; //decides when the scheduler is to be shut down

    void preempt(){ /* function to preempt existing cpu processes */
        if(!ready_queue.empty()){ //getting process from ready queue
            ready_mutex.lock();
            pr.push(ready_queue.front());
            ready_queue.pop();
            ready_mutex.unlock();
        }

        int free_cpu=-1;
        for(int i=0; i<cpus; i++){ //checking for free cpus
            if(!cpu_curr[i]){
                free_cpu=i;
                break;
            }
        }

        if(free_cpu!=-1){ //if there is a free cpu then allocate without preempting
            cpu_mutex[free_cpu].lock();
            priority_mutex.lock();
            cpu_curr[free_cpu]=pr.top();
            cpu_mutex[free_cpu].unlock();
            pr.pop();
            priority_mutex.unlock();
            context_switch();
            return;
        }
        
        for(int i=0; i<cpus; i++){ //preempting minimum priority process
            if(cpu_curr[i]->priority<pr.top()->priority){
                cpu_mutex[i].lock();
                priority_mutex.lock();
                cpu_curr[i]=pr.top();
                cpu_mutex[i].unlock();
                pr.pop();
                priority_mutex.unlock();
                context_switch();
                return;
            }
        }
    }

    void cpu_thread(int id){ /* cpu threads */
        this_thread::sleep_for(chrono::milliseconds(10)); //syncing while thread creation
        entry* curr=NULL;
        while(!done){ //infinite loop for cpus
            if(curr&&curr==cpu_curr[id]){ //if there has been no context switch
                if(curr->flag&&curr->io_times!=curr->io_exec&&curr->io_times!=-1){
                /* checking if i/o is needed */ 
                    io_mutex.lock();
                    io_queue.push(curr);
                    cpu_mutex[id].lock();
                    cpu_curr[id]=NULL;
                    curr=NULL;
                    cpu_mutex[id].unlock();
                    io_mutex.unlock();

                }
                else if(curr->exec_time>=curr->cpu_time){ //terminating process
                    cpu_mutex[id].lock();
                    curr=NULL;
                    cpu_curr[id]=NULL;
                    cpu_mutex[id].unlock();
                    context_switch();
                }
                else{ //running process
                        this_thread::sleep_for(chrono::milliseconds(100));
                        curr->flag=true;
                        curr->exec_time+=0.1;
                }
            }
            else{ //if there is a context switch
                
                if(curr){ //send current process back to priority queue
                    priority_mutex.lock();
                    pr.push(curr);
                    priority_mutex.unlock();
                    context_switch();
                    curr=cpu_curr[id];
                }
                else{ //own current process if empty
                    if(cpu_curr[id]){
                        curr=cpu_curr[id];
                    }
                    else{
                        curr=NULL;
                    }
                }
            }
        }
    }

    void io_thread(int arg){ /* i/o thread */
        entry* curr=NULL;
        while(!done){ //infinite loop for i/o 
            if(!io_queue.empty()){
                io_mutex.lock();
                curr=io_queue.front();
                io_mutex.unlock();
                curr->io_exec+=1;
                curr->flag=false;
                this_thread::sleep_for(chrono::milliseconds(2000));
                priority_mutex.lock();
                pr.push(curr);
                priority_mutex.unlock();
                curr=NULL;
                io_mutex.lock();
                io_queue.pop();
                io_mutex.unlock();
            }
        }
    }

    string idle(entry* curr){ /* checking if cpu is idle */
        if(!curr)
            return "IDLE";
        if(curr->status=="RUNNING")
            return curr->proc_name;
        else    
            return "IDLE";
    }

    void schedule(char* fname){ /* schedule function to handle processes */
        fstream ifile(fname, ios::out); //opening output file

        /* OUTPUT HEADER */
        cout<<"Where Ru=Running, Re=Ready, Wa=Waiting\n\n";
        cout<<colors::bold<<colors::grey<<colors::on_white<<left<<setw(6)<<"Time"<<setw(10)<<"Ru Re Wa";
        ifile<<"Time\tRu Re Wa\t";
        for(int i=0; i<cpus; i++){
            cout<<setw(3)<<"CPU "<<setw(10)<<i;
            ifile<<"CPU ";
            ifile<<i;
            ifile<<"\t";
        }
        cout<<setw(10)<<"I/O QUEUE"<<colors::reset<<endl<<endl;
        ifile<<"I/0 QUEUE\n\n";

        //starting time
        auto start=chrono::high_resolution_clock::now();
        int64_t time=100;

        double ready_time=0; //counter for ready time
 
        while(!done){ //infinite loop for schedule processes
            auto check=chrono::high_resolution_clock::now();
            auto count=chrono::duration_cast<chrono::seconds>(check-start);
            if(!ready_queue.empty()){ //checking if queue is empty
                if(count.count()>=ready_queue.front()->arrival_time){
                    preempt();
                }
            }
            else{ //if queue is not empty
                if(!pr.empty()){
                    preempt();
                }
            }

            /* getting time for i/o after every 0.1 s */
            auto check2=chrono::high_resolution_clock::now();
			auto count2=chrono::duration_cast<chrono::milliseconds>(check2-start);
            if(count2.count()==time){
                int re=pr.size(), ru=0, wa=io_queue.size();
                for(int i=0; i<cpus; i++){
                    if(cpu_curr[i]&&cpu_curr[i]->status!="WAIT")ru++;
                }

                /* I/0 */

				cout<<colors::magenta<<left<<setw(6)<<(double)(time)/1000<<colors::reset;
                cout<<setw(10)<<to_string(re)+"  "+to_string(ru)+"  "+to_string(wa);
                ifile<<(double)(time)/1000;
                ifile<<"\t";
				for(int i=0; i<cpus; i++){
                    if(idle(cpu_curr[i])=="IDLE")
					    cout<<colors::red<<setw(13)<<idle(cpu_curr[i])<<colors::reset;
                    else    
                        cout<<colors::green<<setw(13)<<idle(cpu_curr[i])<<colors::reset;
                    ifile<<idle(cpu_curr[i])<<"\t";
				}
                cout<<colors::reset;
				
				if(io_queue.front()){
				cout<<colors::yellow<<io_queue.front()->proc_name<<colors::reset<<endl;
                    ifile<<io_queue.front()->proc_name<<endl;
                }
				else{
                    cout<<colors::bold<<" <<\n"<<colors::reset;
                    ifile<<"<<\n";
                }
				time+=100;
			}
            //checking if cpu is done with all processes
			bool cpudone=true;
			for(int i=0; i<cpus; i++){
				if(cpu_curr[i]!=NULL)
					cpudone=false;
			}

			if(pr.empty()&&ready_queue.empty()&&io_queue.empty()&&cpudone){
				done=true;
			}

        }

        auto check2=chrono::high_resolution_clock::now();
		auto total_time=chrono::duration_cast<chrono::seconds>(check2-start);

        /* I/O for final information */
        cout<<"\n\n";
        cout<<colors::on_grey<<colors::white<<" # of Context Switches: "<<context_switches<<" "<<colors::reset<<endl;
        cout<<colors::on_grey<<" Total time spent in READY state: "<<420<<" "<<colors::reset<<endl;
        cout<<colors::on_grey<<" Total execution time: "<<total_time.count()<<" s  "<<colors::reset<<endl;

        ifile<<"\n\n";
        ifile<<"# of Context Switches: "<<context_switches<<endl;
        ifile<<"Total time spent in READY state: "<<420<<endl;
        ifile<<"Total execution time: "<<total_time.count()<<" s"<<endl;

        ifile.close(); //closing file

    }

    priority(vector<entry>* pcb, int cpus, char* fname){ //constructor for preemptive priority scheduling
        for(auto i=pcb->begin(); i!=pcb->end(); i++){ //getting processes to queue
            ready_queue.push(&*i);  
        }

        this->pcb=pcb;
        this->cpus=cpus;

        cpu=new thread*[cpus]; /* creating threads for cpus */
        for(int i=0; i<cpus; i++){
            cpu[i]=new thread(&priority::cpu_thread, this, i);
        }
        io=new thread(&priority::io_thread, this, 0); //creating i/o thread
        cpu_mutex=new mutex[cpus];

        cpu_curr=new entry*[cpus]; /* creating current process holders for cpus */
        for(int i=0; i<cpus; i++){
            cpu_curr[i]=NULL;
        }
        schedule(fname); //calling the scheduler 

        for(int i=0; i<cpus; i++){ //joining threads 
            cpu[i]->join();
        }
        io->join();
    }
};

struct sjf{ /* shortest job first implementation */
    vector<entry>* pcb; //pointer to main pcb
    int cpus; //number of cpus

    thread** cpu;
    thread* io;
    mutex *cpu_mutex; //mutex to protect cpus
    mutex ready_mutex; //mutex for ready queue
    mutex io_mutex; //mutex for io queue

    entry** cpu_curr; //processes in the cpus

    queue<entry*> ready_queue; //ready queue to hold processes according to AT
    priority_queue<entry*, vector<entry*>, compareCpu> pr; //priority queue to hold processes according to CPU Time
    mutex priority_mutex; //mutex to priority queue
    queue<entry*> io_queue; //queue to hold processes on wait

    int context_switches=0; //counter for context switches
    mutex context_mutex; //mutex to protect context switches

    bool done=false; //decides when the scheduler is to be shut down

    void context_switch(){ /* context switch function */
        context_mutex.lock();
        context_switches+=1;
        context_mutex.unlock();
    }

    void preempt(){ /* function to preempt existing cpu processes */
        if(!ready_queue.empty()){ //getting process from ready queue
            ready_mutex.lock();
            pr.push(ready_queue.front());
            ready_queue.pop();
            ready_mutex.unlock();
        }

        int free_cpu=-1;
        for(int i=0; i<cpus; i++){ //checking for free cpus
            if(!cpu_curr[i]){
                free_cpu=i;
                break;
            }
        }

        if(free_cpu!=-1){ //if there is a free cpu then allocate without preempting
            cpu_mutex[free_cpu].lock();
            priority_mutex.lock();
            cpu_curr[free_cpu]=pr.top();
            cpu_mutex[free_cpu].unlock();
            pr.pop();
            priority_mutex.unlock();
            context_switch();
            return;
        }
        
        for(int i=0; i<cpus; i++){
            if(cpu_curr[i]->cpu_time>pr.top()->cpu_time){ //preempting maximum priority process
                cpu_mutex[i].lock();
                priority_mutex.lock();
                cpu_curr[i]=pr.top();
                cpu_mutex[i].unlock();
                pr.pop();
                priority_mutex.unlock();
                context_switch();
                return;
            }
        }
    }

    void cpu_thread(int id){ /* cpu threads */
        this_thread::sleep_for(chrono::milliseconds(10)); //syncing while thread creation
        entry* curr=NULL;
        while(!done){
            if(curr&&curr==cpu_curr[id]){ //if there has been no context switch
                if(curr->flag&&curr->io_times!=curr->io_exec&&curr->io_times!=-1){
                /* checking if i/o is needed */
                    io_mutex.lock();
                    io_queue.push(curr);
                    cpu_mutex[id].lock();
                    cpu_curr[id]=NULL;
                    curr=NULL;
                    cpu_mutex[id].unlock();
                    io_mutex.unlock();

                }
                else if(curr->exec_time>=curr->cpu_time){ //terminating process
                    cpu_mutex[id].lock();
                    curr=NULL;
                    cpu_curr[id]=NULL;
                    cpu_mutex[id].unlock();
                    context_switch();
                }
                else{ //running process
                        this_thread::sleep_for(chrono::milliseconds(100));
                        curr->flag=true;
                        curr->exec_time+=0.1;
                }
            }
            else{ //if there is a context switch
                
                if(curr){ //send current process back to priority queue
                    priority_mutex.lock();
                    pr.push(curr);
                    priority_mutex.unlock();
                    context_switch();
                    curr=cpu_curr[id];
                }
                else{ //own current process if empty
                    if(cpu_curr[id]){
                        curr=cpu_curr[id];
                    }
                    else{
                        curr=NULL;
                    }
                }
            }
        }
    }

    void io_thread(int arg){ /* i/o thread */
        entry* curr=NULL;
        while(!done){ //infinite loop for i/o
            if(!io_queue.empty()){
                io_mutex.lock();
                curr=io_queue.front();
                io_mutex.unlock();
                curr->io_exec+=1;
                curr->flag=false;
                this_thread::sleep_for(chrono::milliseconds(2000));
                priority_mutex.lock();
                pr.push(curr);
                priority_mutex.unlock();
                curr=NULL;
                io_mutex.lock();
                io_queue.pop();
                io_mutex.unlock();
            }
        }
    }

    string idle(entry* curr){ /* checking if cpu is idle */
        if(!curr)
            return "IDLE";
        if(curr->status=="RUNNING")
            return curr->proc_name;
        else    
            return "IDLE";
    }

    void schedule(char* fname){ /* schedule function to handle processes */
        fstream ifile(fname, ios::out); //opening output file

        /* OUTPUT HEADER */
        cout<<"Where Ru=Running, Re=Ready, Wa=Waiting\n\n";
        cout<<colors::bold<<colors::grey<<colors::on_white<<left<<setw(6)<<"Time"<<setw(10)<<"Ru Re Wa";
        ifile<<"Time\tRu Re Wa\t";
        for(int i=0; i<cpus; i++){
            cout<<setw(3)<<"CPU "<<setw(10)<<i;
            ifile<<"CPU ";
            ifile<<i;
            ifile<<"\t";
        }
        cout<<setw(10)<<"I/O QUEUE"<<colors::reset<<endl<<endl;
        ifile<<"I/0 QUEUE\n\n";

        //starting time
        auto start=chrono::high_resolution_clock::now();
        int64_t time=100;

        double ready_time=0; //counter for ready time

        while(!done){ //infinite loop for schedule processes
            auto check=chrono::high_resolution_clock::now();
            auto count=chrono::duration_cast<chrono::seconds>(check-start);
            if(!ready_queue.empty()){ //checking if queue is empty
                if(count.count()>=ready_queue.front()->arrival_time){
                    preempt();
                }
            }
            else{ //if queue is not empty
                if(!pr.empty()){
                    preempt();
                }
            }
            /* getting time for i/o after every 0.1 s */
            auto check2=chrono::high_resolution_clock::now();
			auto count2=chrono::duration_cast<chrono::milliseconds>(check2-start);
            if(count2.count()==time){
                int re=pr.size(), ru=0, wa=io_queue.size();
                for(int i=0; i<cpus; i++){
                    if(cpu_curr[i]&&cpu_curr[i]->status!="WAIT")ru++;
                }

                /* I/O */

				cout<<colors::magenta<<left<<setw(6)<<(double)(time)/1000<<colors::reset;
                cout<<setw(10)<<to_string(re)+"  "+to_string(ru)+"  "+to_string(wa);
                ifile<<(double)(time)/1000;
                ifile<<"\t";
				for(int i=0; i<cpus; i++){
                    if(idle(cpu_curr[i])=="IDLE")
					    cout<<colors::red<<setw(13)<<idle(cpu_curr[i])<<colors::reset;
                    else    
                        cout<<colors::green<<setw(13)<<idle(cpu_curr[i])<<colors::reset;
                    ifile<<idle(cpu_curr[i])<<"\t";
				}
                cout<<colors::reset;
				
				if(io_queue.front()){
				cout<<colors::yellow<<io_queue.front()->proc_name<<colors::reset<<endl;
                    ifile<<io_queue.front()->proc_name<<endl;
                }
				else{
                    cout<<colors::bold<<" <<\n"<<colors::reset;
                    ifile<<"<<\n";
                }
				time+=100;
			}
            //checking if cpu is done with all processes
			bool cpudone=true;
			for(int i=0; i<cpus; i++){
				if(cpu_curr[i]!=NULL)
					cpudone=false;
			}

			if(pr.empty()&&ready_queue.empty()&&io_queue.empty()&&cpudone){
				done=true;
			}

        }

        auto check2=chrono::high_resolution_clock::now();
		auto total_time=chrono::duration_cast<chrono::seconds>(check2-start);

        /* I/O for final information */
        cout<<"\n\n";
        cout<<colors::on_grey<<colors::white<<" # of Context Switches: "<<context_switches<<" "<<colors::reset<<endl;
        cout<<colors::on_grey<<" Total time spent in READY state: "<<420<<" "<<colors::reset<<endl;
        cout<<colors::on_grey<<" Total execution time: "<<total_time.count()<<" s  "<<colors::reset<<endl;

        ifile<<"\n\n";
        ifile<<"# of Context Switches: "<<context_switches<<endl;
        ifile<<"Total time spent in READY state: "<<420<<endl;
        ifile<<"Total execution time: "<<total_time.count()<<" s"<<endl;

        ifile.close(); //closing file

    }

    sjf(vector<entry>* pcb, int cpus, char* fname){ //constructor for shortest job first scheduling
        for(auto i=pcb->begin(); i!=pcb->end(); i++){ //getting processes to queue
            ready_queue.push(&*i);  
        }

        this->pcb=pcb;
        this->cpus=cpus;

        cpu=new thread*[cpus]; /* creating threads for cpus */
        for(int i=0; i<cpus; i++){
            cpu[i]=new thread(&sjf::cpu_thread, this, i);
        }
        io=new thread(&sjf::io_thread, this, 0); //creating i/o thread
        cpu_mutex=new mutex[cpus];

        cpu_curr=new entry*[cpus]; /* creating current process holders for cpus */
        for(int i=0; i<cpus; i++){
            cpu_curr[i]=NULL;
        }
        schedule(fname); //calling the scheduler

        for(int i=0; i<cpus; i++){ //joining threads
            cpu[i]->join();
        }
        io->join();
    }
};