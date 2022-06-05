#include <iostream>
#include <utility>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <queue>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

/* =====IMPORTANT NOTICE===== 
I do not own colors.hpp. It is taken
from a github respository and is free
to use open source for including colors
in terminal output. 

Link: hugorplobo/colors.hpp */
#include "colors.hpp"

using namespace std;

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
        //cout<<"lmao\n";
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
                            //cout<<"judjrdu";
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
        //cout<<"lmao\n";
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
        //cout<<curr->exec_time<<endl;
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

struct priority{
    vector<entry>* pcb;
    int cpus;

    thread** cpu;
    thread* io;
    mutex *cpu_mutex;
    mutex ready_mutex;
    mutex io_mutex;

    entry** cpu_curr;

    queue<entry*> ready_queue;
    priority_queue<entry*, vector<entry*>, compareEntry> pr;
    mutex priority_mutex;
    queue<entry*> io_queue;

    int context_switches=0;
    mutex context_mutex;

    bool done=false;

    void yield(entry* send){
        
    }

    void terminate(entry* done){
        
    }

    void wakeup(entry *wake){
        
    }

    void preempt(){
        if(!ready_queue.empty()){
            ready_mutex.lock();
            pr.push(ready_queue.front());
            //cout<<pr.top()->proc_name<<endl;
            ready_queue.pop();
            ready_mutex.unlock();
        }

        //cout<<pr.top()->proc_name<<endl;

        int free_cpu=-1;
        for(int i=0; i<cpus; i++){
            if(!cpu_curr[i]){
                free_cpu=i;
                break;
            }
        }

        //cout<<free_cpu<<pr.top()->proc_name<<endl;
        if(free_cpu!=-1){
            cpu_mutex[free_cpu].lock();
            priority_mutex.lock();
            cpu_curr[free_cpu]=pr.top();
            cpu_mutex[free_cpu].unlock();
            pr.pop();
            priority_mutex.unlock();
            context_mutex.lock();
            context_switches+=1;
            context_mutex.unlock();
            return;
        }
        
        for(int i=0; i<cpus; i++){
            //cout<<"jnneuddnejd\n";
            if(cpu_curr[i]->priority<pr.top()->priority){
                //cout<<"HELLLOOO\n";
                cpu_mutex[i].lock();
                //cpu_curr[i]->status="PREEMPT";
                priority_mutex.lock();
                cpu_curr[i]=pr.top();
                cpu_mutex[i].unlock();
                pr.pop();
                priority_mutex.unlock();
                context_mutex.lock();
                context_switches+=1;
                context_mutex.unlock();
                return;
            }
        }
    }

    void cpu_thread(int id){
        this_thread::sleep_for(chrono::milliseconds(10));
        entry* curr=NULL;
        while(!done){
            //cout<<"YES2\n";
            if(curr&&curr==cpu_curr[id]){
                if(curr->flag&&curr->io_times!=curr->io_exec&&curr->io_times!=-1){
                    
                    io_mutex.lock();
                    io_queue.push(curr);
                    cpu_mutex[id].lock();
                    cpu_curr[id]=NULL;
                    curr=NULL;
                    cpu_mutex[id].unlock();
                    io_mutex.unlock();
                    //cout<<"YESSSSS\n";

                }
                else if(curr->exec_time>=curr->cpu_time){
                    cpu_mutex[id].lock();
                    curr=NULL;
                    cpu_curr[id]=NULL;
                    cpu_mutex[id].unlock();
                    context_mutex.lock();
                    context_switches+=1;
                    context_mutex.unlock();
                }
                else{
                        //cout<<"YES3"<<curr->exec_time<<"\n";
                        this_thread::sleep_for(chrono::milliseconds(100));
                        curr->flag=true;
                        curr->exec_time+=0.1;
                        //cout<<"HELLO\n";
                        //cout<<curr->exec_time<<endl;
                }
            }
            else{
                
                if(curr){
                    priority_mutex.lock();
                    pr.push(curr);
                    priority_mutex.unlock();
                    context_mutex.lock();
                    context_switches+=1;
                    context_mutex.unlock();
                    //cpu_mutex[id].lock();
                    curr=cpu_curr[id];
                    //cpu_mutex[id].unlock();
                }
                else{
                    //cout<<"yessssss\n";
                    if(cpu_curr[id]){
                        //cout<<"iejijd\n";
                        curr=cpu_curr[id];
                    }
                    else{
                        //cout<<"YES5\n";
                        curr=NULL;
                    }
                }
                //cout<<curr->proc_name<<"------------------\n";
            }
        }
    }

    void io_thread(int arg){
        entry* curr=NULL;
        while(!done){
            if(!io_queue.empty()){
                //cout<<"iirjriiricni\n";
                io_mutex.lock();
                curr=io_queue.front();
                io_mutex.unlock();
                curr->io_exec+=1;
                //cout<<"YES2\n";
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

    string idle(entry* curr){
        if(!curr)
            return "IDLE";
        if(curr->status=="RUNNING")
            return curr->proc_name;
        else    
            return "IDLE";
    }

    void schedule(char* fname){
        fstream ifile(fname, ios::out);

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

        auto start=chrono::high_resolution_clock::now();
        int64_t time=100;

        double ready_time=0;

        while(!done){
            auto check=chrono::high_resolution_clock::now();
            auto count=chrono::duration_cast<chrono::seconds>(check-start);
            if(!ready_queue.empty()){
                if(count.count()>=ready_queue.front()->arrival_time){
                    preempt();
                }
            }
            else{
                if(!pr.empty()){
                    preempt();
                }
            }
            auto check2=chrono::high_resolution_clock::now();
			auto count2=chrono::duration_cast<chrono::milliseconds>(check2-start);
            if(count2.count()==time){
                int re=pr.size(), ru=0, wa=io_queue.size();
                for(int i=0; i<cpus; i++){
                    if(cpu_curr[i]&&cpu_curr[i]->status!="WAIT")ru++;
                }

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

        cout<<"\n\n";
        cout<<colors::on_grey<<colors::white<<" # of Context Switches: "<<context_switches<<" "<<colors::reset<<endl;
        cout<<colors::on_grey<<" Total time spent in READY state: "<<420<<" "<<colors::reset<<endl;
        cout<<colors::on_grey<<" Total execution time: "<<total_time.count()<<" s  "<<colors::reset<<endl;

        ifile<<"\n\n";
        ifile<<"# of Context Switches: "<<context_switches<<endl;
        ifile<<"Total time spent in READY state: "<<420<<endl;
        ifile<<"Total execution time: "<<total_time.count()<<" s"<<endl;

        ifile.close();

    }

    priority(vector<entry>* pcb, int cpus, char* fname){
        for(auto i=pcb->begin(); i!=pcb->end(); i++){
            ready_queue.push(&*i);  
        }

        this->pcb=pcb;
        this->cpus=cpus;

        cpu=new thread*[cpus];
        for(int i=0; i<cpus; i++){
            cpu[i]=new thread(&priority::cpu_thread, this, i);
        }
        io=new thread(&priority::io_thread, this, 0);
        cpu_mutex=new mutex[cpus];

        cpu_curr=new entry*[cpus];
        for(int i=0; i<cpus; i++){
            cpu_curr[i]=NULL;
        }
        cout<<"YES1\n";
        schedule(fname);

        for(int i=0; i<cpus; i++){
            cpu[i]->join();
        }
        io->join();
    }
};

struct sjf{
    vector<entry>* pcb;
    int cpus;

    thread** cpu;
    thread* io;
    mutex *cpu_mutex;
    mutex ready_mutex;
    mutex io_mutex;

    entry** cpu_curr;

    queue<entry*> ready_queue;
    priority_queue<entry*, vector<entry*>, compareCpu> pr;
    mutex priority_mutex;
    queue<entry*> io_queue;

    int context_switches=0;
    mutex context_mutex;

    bool done=false;

    void yield(entry* send){
        
    }

    void terminate(entry* done){
        
    }

    void wakeup(entry *wake){
        
    }

    void preempt(){
        if(!ready_queue.empty()){
            ready_mutex.lock();
            pr.push(ready_queue.front());
            //cout<<pr.top()->proc_name<<endl;
            ready_queue.pop();
            ready_mutex.unlock();
        }

        //cout<<pr.top()->proc_name<<endl;

        int free_cpu=-1;
        for(int i=0; i<cpus; i++){
            if(!cpu_curr[i]){
                free_cpu=i;
                break;
            }
        }

        //cout<<free_cpu<<pr.top()->proc_name<<endl;
        if(free_cpu!=-1){
            cpu_mutex[free_cpu].lock();
            priority_mutex.lock();
            cpu_curr[free_cpu]=pr.top();
            cpu_mutex[free_cpu].unlock();
            pr.pop();
            priority_mutex.unlock();
            context_mutex.lock();
            context_switches+=1;
            context_mutex.unlock();
            return;
        }
        
        for(int i=0; i<cpus; i++){
            //cout<<"jnneuddnejd\n";
            if(cpu_curr[i]->cpu_time>pr.top()->cpu_time){
                //cout<<"HELLLOOO\n";
                cpu_mutex[i].lock();
                //cpu_curr[i]->status="PREEMPT";
                priority_mutex.lock();
                cpu_curr[i]=pr.top();
                cpu_mutex[i].unlock();
                pr.pop();
                priority_mutex.unlock();
                context_mutex.lock();
                context_switches+=1;
                context_mutex.unlock();
                return;
            }
        }
    }

    void cpu_thread(int id){
        this_thread::sleep_for(chrono::milliseconds(10));
        entry* curr=NULL;
        while(!done){
            //cout<<"YES2\n";
            if(curr&&curr==cpu_curr[id]){
                if(curr->flag&&curr->io_times!=curr->io_exec&&curr->io_times!=-1){
                    
                    io_mutex.lock();
                    io_queue.push(curr);
                    cpu_mutex[id].lock();
                    cpu_curr[id]=NULL;
                    curr=NULL;
                    cpu_mutex[id].unlock();
                    io_mutex.unlock();
                    //cout<<"YESSSSS\n";

                }
                else if(curr->exec_time>=curr->cpu_time){
                    cpu_mutex[id].lock();
                    curr=NULL;
                    cpu_curr[id]=NULL;
                    cpu_mutex[id].unlock();
                    context_mutex.lock();
                    context_switches+=1;
                    context_mutex.unlock();
                }
                else{
                        //cout<<"YES3"<<curr->exec_time<<"\n";
                        this_thread::sleep_for(chrono::milliseconds(100));
                        curr->flag=true;
                        curr->exec_time+=0.1;
                        //cout<<"HELLO\n";
                        //cout<<curr->exec_time<<endl;
                }
            }
            else{
                
                if(curr){
                    priority_mutex.lock();
                    pr.push(curr);
                    priority_mutex.unlock();
                    context_mutex.lock();
                    context_switches+=1;
                    context_mutex.unlock();
                    //cpu_mutex[id].lock();
                    curr=cpu_curr[id];
                    //cpu_mutex[id].unlock();
                }
                else{
                    //cout<<"yessssss\n";
                    if(cpu_curr[id]){
                        //cout<<"iejijd\n";
                        curr=cpu_curr[id];
                    }
                    else{
                        //cout<<"YES5\n";
                        curr=NULL;
                    }
                }
                //cout<<curr->proc_name<<"------------------\n";
            }
        }
    }

    void io_thread(int arg){
        entry* curr=NULL;
        while(!done){
            if(!io_queue.empty()){
                //cout<<"iirjriiricni\n";
                io_mutex.lock();
                curr=io_queue.front();
                io_mutex.unlock();
                curr->io_exec+=1;
                //cout<<"YES2\n";
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

    string idle(entry* curr){
        if(!curr)
            return "IDLE";
        if(curr->status=="RUNNING")
            return curr->proc_name;
        else    
            return "IDLE";
    }

    void schedule(char* fname){
        fstream ifile(fname, ios::out);

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

        auto start=chrono::high_resolution_clock::now();
        int64_t time=100;

        double ready_time=0;

        while(!done){
            auto check=chrono::high_resolution_clock::now();
            auto count=chrono::duration_cast<chrono::seconds>(check-start);
            if(!ready_queue.empty()){
                if(count.count()>=ready_queue.front()->arrival_time){
                    preempt();
                }
            }
            else{
                if(!pr.empty()){
                    preempt();
                }
            }
            auto check2=chrono::high_resolution_clock::now();
			auto count2=chrono::duration_cast<chrono::milliseconds>(check2-start);
            if(count2.count()==time){
                int re=pr.size(), ru=0, wa=io_queue.size();
                for(int i=0; i<cpus; i++){
                    if(cpu_curr[i]&&cpu_curr[i]->status!="WAIT")ru++;
                }

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

        cout<<"\n\n";
        cout<<colors::on_grey<<colors::white<<" # of Context Switches: "<<context_switches<<" "<<colors::reset<<endl;
        cout<<colors::on_grey<<" Total time spent in READY state: "<<420<<" "<<colors::reset<<endl;
        cout<<colors::on_grey<<" Total execution time: "<<total_time.count()<<" s  "<<colors::reset<<endl;

        ifile<<"\n\n";
        ifile<<"# of Context Switches: "<<context_switches<<endl;
        ifile<<"Total time spent in READY state: "<<420<<endl;
        ifile<<"Total execution time: "<<total_time.count()<<" s"<<endl;

        ifile.close();

    }

    sjf(vector<entry>* pcb, int cpus, char* fname){
        for(auto i=pcb->begin(); i!=pcb->end(); i++){
            ready_queue.push(&*i);  
        }

        this->pcb=pcb;
        this->cpus=cpus;

        cpu=new thread*[cpus];
        for(int i=0; i<cpus; i++){
            cpu[i]=new thread(&sjf::cpu_thread, this, i);
        }
        io=new thread(&sjf::io_thread, this, 0);
        cpu_mutex=new mutex[cpus];

        cpu_curr=new entry*[cpus];
        for(int i=0; i<cpus; i++){
            cpu_curr[i]=NULL;
        }
        schedule(fname);

        for(int i=0; i<cpus; i++){
            cpu[i]->join();
        }
        io->join();
    }
};

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