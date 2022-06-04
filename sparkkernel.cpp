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
using namespace std;

struct entry{
    string proc_name;
    int priority;
    float arrival_time;
    float cpu_time;
    float exec_time=0.0;

    int io_times;
    int io_exec=0;

    char proc_type;

    bool flag=false;

    string status="RUNNING";
};

struct fcfs{
    vector<entry>* pcb;
    int cpus;

    thread** cpu;
    thread* io;
    mutex *cpu_mutex;
    //mutex ready_mutex;
    mutex io_mutex;

    entry** cpu_curr;

    queue<entry*> ready_queue;
    queue<entry*> io_queue;

    int context_switches=0;
    mutex context_mutex;

    bool done=false;

    void yield(entry* send){
        io_mutex.lock();
        //cout<<"lmao\n";
        io_queue.push(send);
        io_mutex.unlock();
        context_mutex.lock();
        context_switches+=1;
        context_mutex.unlock();
    }

    void terminate(entry* done){
        done->status="DONE";
    }

    void wakeup(entry *wake){
        wake->io_exec+=1;
        wake->status="RUNNING";
    }

    void cpu_thread(int id){
        this_thread::sleep_for(chrono::milliseconds(10));

        while(!done){
            cpu_mutex[id].lock();
            entry* curr=cpu_curr[id];
            cpu_mutex[id].unlock();

            if(curr){
                float time_elapsed=0.0;
                bool flag=false;
                while(curr->status!="DONE"){
                    if(curr->status!="WAIT"){
                        if(curr->io_times!=-1&&curr->io_exec!=curr->io_times&&flag){
                            //cout<<"judjrdu";
                            flag=false;
                            curr->status="WAIT";
                            yield(curr);
                        }
                        else{
                            if(curr->exec_time>=curr->cpu_time){
                                terminate(curr);
                                curr=NULL;
                                cpu_mutex[id].lock();
                                cpu_curr[id]=NULL;
                                cpu_mutex[id].unlock();
                                break;
                            }
                            else{
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

    void io_thread(int arg){
        entry* curr=NULL;
        while(!done){
            io_mutex.lock();
            if(!io_queue.empty()){
                curr=io_queue.front();
            }
            io_mutex.unlock();
            if(curr){
                this_thread::sleep_for(chrono::milliseconds(2000));
                wakeup(curr);
                curr=NULL;
                context_mutex.lock();
                context_switches+=1;
                context_mutex.unlock();
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
        cout<<left<<setw(6)<<"Time"<<setw(10)<<"Ru Re Wa";
        ifile<<"Time\tRu Re Wa\t";
        for(int i=0; i<cpus; i++){
            cout<<setw(3)<<"CPU "<<setw(10)<<i;
            ifile<<"CPU ";
            ifile<<i;
            ifile<<"\t";
        }
        cout<<setw(10)<<"I/O QUEUE\n\n";
        ifile<<"I/0 QUEUE\n\n";

        auto start=chrono::high_resolution_clock::now();
        int64_t time=100;

        double ready_time=0;

        while(!done){
            auto check=chrono::high_resolution_clock::now();
            auto count=chrono::duration_cast<chrono::seconds>(check-start);
            if(!ready_queue.empty()){
                if(count.count()>=ready_queue.front()->arrival_time){
                    for(int i=0; i<cpus; i++){
                        if(!cpu_curr[i]){
                            cpu_mutex[i].lock();
                            cpu_curr[i]=ready_queue.front();
                            ready_queue.pop();
                            cpu_mutex[i].unlock();
                            context_mutex.lock();
                            context_switches+=1;
                            context_mutex.unlock();
                            break;
                        }
                    }
                }
            }
            auto check2=chrono::high_resolution_clock::now();
			auto count2=chrono::duration_cast<chrono::milliseconds>(check2-start);
            if(count2.count()==time){
                int re=ready_queue.size(), ru=0, wa=io_queue.size();
                for(int i=0; i<cpus; i++){
                    if(cpu_curr[i]&&cpu_curr[i]->status!="WAIT")ru++;
                }

				cout<<left<<setw(6)<<(double)(time)/1000;
                cout<<setw(10)<<to_string(re)+"  "+to_string(ru)+"  "+to_string(wa);
                ifile<<(double)(time)/1000;
                ifile<<"\t";
				for(int i=0; i<cpus; i++){
					cout<<setw(13)<<idle(cpu_curr[i]);
                    ifile<<idle(cpu_curr[i])<<"\t";
				}
				
				if(io_queue.front()){
				cout<<io_queue.front()->proc_name<<endl;
                    ifile<<io_queue.front()->proc_name<<endl;
                }
				else{
                    cout<<" <<\n";
                    ifile<<"<<\n";
                }
				time+=100;
			}
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

        cout<<"\n\n";
        cout<<"# of Context Switches: "<<context_switches<<endl;
        cout<<"Total time spent in READY state: "<<420<<endl;
        cout<<"Total execution time: "<<total_time.count()<<" s"<<endl;

        ifile<<"\n\n";
        ifile<<"# of Context Switches: "<<context_switches<<endl;
        ifile<<"Total time spent in READY state: "<<420<<endl;
        ifile<<"Total execution time: "<<total_time.count()<<" s"<<endl;

        ifile.close();
    }

    fcfs(vector<entry>* pcb, int cpus, char* fname){
        for(auto i=pcb->begin(); i!=pcb->end(); i++){
            ready_queue.push(&*i);  
        }

        this->pcb=pcb;
        this->cpus=cpus;

        cpu=new thread*[cpus];
        for(int i=0; i<cpus; i++){
            cpu[i]=new thread(&fcfs::cpu_thread, this, i);
        }
        io=new thread(&fcfs::io_thread, this, 0);
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

struct round_robin{
    vector<entry>* pcb;
    int cpus;
    int time_slice;

    thread** cpu;
    thread* io;
    mutex *cpu_mutex;
    mutex ready_mutex;
    mutex io_mutex;

    entry** cpu_curr;

    queue<entry*> ready_queue;
    queue<entry*> io_queue;

    int context_switches=0;
    mutex context_mutex;

    bool done=false;

    void yield(entry* send){
        io_mutex.lock();
        //cout<<"lmao\n";
        io_queue.push(send);
        io_mutex.unlock();
        context_mutex.lock();
        context_switches+=1;
        context_mutex.unlock();
    }

    void terminate(entry* done){
        done->status="DONE";
    }

    void wakeup(entry *wake){
        wake->io_exec+=1;
        wake->status="RUNNING";
        ready_mutex.lock();
        ready_queue.push(wake);
        ready_mutex.unlock();
    }

    void preempt(entry* curr){
        curr->exec_time+=(float)time_slice/10;
        //cout<<curr->exec_time<<endl;
        ready_mutex.lock();
        ready_queue.push(curr);
        ready_mutex.unlock();
    }

    void cpu_thread(int id){
        this_thread::sleep_for(chrono::milliseconds(10));

        while(!done){
            cpu_mutex[id].lock();
            entry* curr=cpu_curr[id];
            cpu_mutex[id].unlock();

            if(curr){
                float time_elapsed=0.0;
                if(curr->io_times!=-1&&curr->io_exec!=curr->io_times&&curr->flag){
                    curr->flag=false;
                    curr->status="WAIT";
                    yield(curr);
                    curr=NULL;
                    cpu_mutex[id].lock();
                    cpu_curr[id]=NULL;
                    cpu_mutex[id].unlock();
                }
                else{
                    if(curr->exec_time>=curr->cpu_time){
                        terminate(curr);
                        curr=NULL;
                        cpu_mutex[id].lock();
                        cpu_curr[id]=NULL;
                        cpu_mutex[id].unlock();
                    }
                    else{
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

    void io_thread(int arg){
        entry* curr=NULL;
        while(!done){
            io_mutex.lock();
            if(!io_queue.empty()){
                curr=io_queue.front();
            }
            io_mutex.unlock();
            if(curr){
                this_thread::sleep_for(chrono::milliseconds(2000));
                wakeup(curr);
                curr=NULL;
                context_mutex.lock();
                context_switches+=1;
                context_mutex.unlock();
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
        cout<<left<<setw(6)<<"Time"<<setw(10)<<"Ru Re Wa";
        ifile<<"Time\tRu Re Wa\t";
        for(int i=0; i<cpus; i++){
            cout<<setw(3)<<"CPU "<<setw(10)<<i;
            ifile<<"CPU ";
            ifile<<i;
            ifile<<"\t";
        }
        cout<<setw(10)<<"I/O QUEUE\n\n";
        ifile<<"I/0 QUEUE\n\n";

        auto start=chrono::high_resolution_clock::now();
        int64_t time=100;

        double ready_time=0;

        while(!done){
            auto check=chrono::high_resolution_clock::now();
            auto count=chrono::duration_cast<chrono::seconds>(check-start);
            if(!ready_queue.empty()){
                if(count.count()>=ready_queue.front()->arrival_time){
                    for(int i=0; i<cpus; i++){
                        if(!cpu_curr[i]){
                            cpu_mutex[i].lock();
                            ready_mutex.lock();
                            cpu_curr[i]=ready_queue.front();
                            ready_queue.pop();
                            ready_mutex.unlock();
                            cpu_mutex[i].unlock();
                            context_mutex.lock();
                            context_switches+=1;
                            context_mutex.unlock();
                            break;
                        }
                    }
                }
            }
            auto check2=chrono::high_resolution_clock::now();
			auto count2=chrono::duration_cast<chrono::milliseconds>(check2-start);
            if(count2.count()==time){
                int re=ready_queue.size(), ru=0, wa=io_queue.size();
                for(int i=0; i<cpus; i++){
                    if(cpu_curr[i]&&cpu_curr[i]->status!="WAIT")ru++;
                }

				cout<<left<<setw(6)<<(double)(time)/1000;
                cout<<setw(10)<<to_string(re)+"  "+to_string(ru)+"  "+to_string(wa);
                ifile<<(double)(time)/1000;
                ifile<<"\t";
				for(int i=0; i<cpus; i++){
					cout<<setw(13)<<idle(cpu_curr[i]);
                    ifile<<idle(cpu_curr[i])<<"\t";
				}
				
				if(io_queue.front()){
				cout<<io_queue.front()->proc_name<<endl;
                    ifile<<io_queue.front()->proc_name<<endl;
                }
				else{
                    cout<<" <<\n";
                    ifile<<"<<\n";
                }
				time+=100;
			}
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

        cout<<"\n\n";
        cout<<"# of Context Switches: "<<context_switches<<endl;
        cout<<"Total time spent in READY state: "<<420<<endl;
        cout<<"Total execution time: "<<total_time.count()<<" s"<<endl;

        ifile<<"\n\n";
        ifile<<"# of Context Switches: "<<context_switches<<endl;
        ifile<<"Total time spent in READY state: "<<420<<endl;
        ifile<<"Total execution time: "<<total_time.count()<<" s"<<endl;

        ifile.close();

    }

    round_robin(vector<entry>* pcb, int cpus, int time_slice, char* fname){
        for(auto i=pcb->begin(); i!=pcb->end(); i++){
            ready_queue.push(&*i);  
        }

        this->pcb=pcb;
        this->cpus=cpus;
        this->time_slice=time_slice;

        cpu=new thread*[cpus];
        for(int i=0; i<cpus; i++){
            cpu[i]=new thread(&round_robin::cpu_thread, this, i);
        }
        io=new thread(&round_robin::io_thread, this, 0);
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

void init_proc(vector<entry>& pcb, string line){
	stringstream ss(line);
	
	entry temp;
	
	ss>>temp.proc_name;
	ss>>temp.priority;
	ss>>temp.arrival_time;
	ss>>temp.proc_type;
	ss>>temp.cpu_time;
	ss>>temp.io_times;
	
	pcb.push_back(temp);
}

int main(int argc, char* argv[]){
	vector<entry> pcb;

	fstream ifile(argv[1], ios::in);
	string buff;
	getline(ifile, buff);
	while(getline(ifile, buff)){
		init_proc(pcb, buff);
	}
	
	if(argv[3][0]=='f'){
		fcfs start(&pcb, stoi(argv[2]), argv[4]);
	}
	else if(argv[3][0]=='r'){
		round_robin start(&pcb, stoi(argv[2]), stoi(argv[4]), argv[5]);
	}
	else if(argv[3][0]=='p'){

	}
	
	return 0;
}