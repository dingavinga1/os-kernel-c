#include<bits/stdc++.h>
#include<unistd.h>
#include<pthread.h>
#include<sys/types.h>
#include<semaphore.h>
#include<queue>
using namespace std;

struct entry{
	string proc_name;
	int priority;
	float arrival_time;
	float cpu_time;
	float io_time;
	char proc_type;
	
	char status; //ready, running, wating, io, terminated
	int cpu; //which cpu is it assigned to
};

entry** alloc;
queue<entry*> io_queue;
static bool done=false;

pthread_mutex_t* alloc_mutex;
pthread_mutex_t io_mutex;

struct fcfs{
	list<entry>* pcb;
	float exec_time;
	float wait_time;
	int cpus;
	
	
	
	void init(list<entry>* pcb, int cpus){
		//cout<<cpus;
		this->pcb=pcb;
		this->cpus=cpus;
		alloc=new entry*[cpus]{NULL};
		alloc_mutex=new pthread_mutex_t[cpus];
		for(int i=0; i<cpus; i++)
			pthread_mutex_init(&alloc_mutex[i], NULL);
		pthread_mutex_init(&io_mutex, NULL);
	}
	
	static void* io(void* args){
		while(!done){
			pthread_mutex_lock(&io_mutex);
			entry* curr=io_queue.front();
			pthread_mutex_unlock(&io_mutex);
			
			if(curr){
				auto start=chrono::high_resolution_clock::now();
				bool end=false;
				while(!end){
					auto c=chrono::high_resolution_clock::now();
					auto count=chrono::duration_cast<chrono::seconds>(c-start);
					if(count.count()>=curr->io_time){
						end=true;
						break;
					}
				}
				curr->status='D';
				pthread_mutex_lock(&io_mutex);
				io_queue.pop();
				pthread_mutex_unlock(&io_mutex);
				curr=NULL;
			}
				
		}	
	}
	
	static void* cpu(void* args){
		int* id=(int*)args;
		//cout<<*id<<endl;
		
		while(!done){
			pthread_mutex_lock(&alloc_mutex[*id]);
			entry* curr=alloc[*id];
			pthread_mutex_unlock(&alloc_mutex[*id]);
			
			if(curr){
				auto start=chrono::high_resolution_clock::now();
				bool end=false;
				while(!end){
					auto c=chrono::high_resolution_clock::now();
					auto count=chrono::duration_cast<chrono::seconds>(c-start);
					//cout<<count.count()<<" "<<curr->cpu_time<<endl;
					if(count.count()>=curr->cpu_time){
						//cout<<"YES1"<<endl;
						end=true;
						break;
						//cout<<"YES1"<<endl;
					}
					//cout<<"YES2"<<endl;
					//cout<<count.count()<<endl;
					//cout<<"Going..."<<endl;
				}
				//cout<<"YES3"<<endl;
				pthread_mutex_lock(&alloc_mutex[*id]);
					curr->status='E'; //executed
					pthread_mutex_lock(&io_mutex);
					//cout<<"YES4"<<endl;
					if(curr->io_time!=-1){
						io_queue.push(curr);
					}
					pthread_mutex_unlock(&io_mutex);
					//cout<<"YES5"<<endl;
					curr=NULL;
					alloc[*id]=NULL;
					//cout<<alloc[*id]<<endl;
				pthread_mutex_unlock(&alloc_mutex[*id]);	
				//cout<<"YES6"<<endl;	
			}
		}
	}
	
	void schedule(){
		pthread_t id[cpus];	
	
		for(int i=0; i<cpus; i++){
			int* send=new int(i);
			pthread_create(&id[i], NULL, &cpu, (void*)send);
		}
		
		pthread_t ioid;
		pthread_create(&ioid, NULL, io, NULL);

		queue<entry*> dispatch;
		for(auto i=pcb->begin(); i!=pcb->end(); i++){
			//cout<<i->cpu_time<<endl;
			//cout<<i->proc_name<<endl;
			dispatch.push(&*i);
		}
		
		auto start=chrono::high_resolution_clock::now();
		int64_t time=100;
		auto iter=start;

		cout<<"time\tCPU1\tCPU2\tIO\n";

		while(!done){
			auto check=chrono::high_resolution_clock::now();
			auto count=chrono::duration_cast<chrono::seconds>(check-start);
			if(count.count()>=dispatch.front()->arrival_time){
				//cout<<count.count()<<endl;
				//cout<<"HELLO "<<count.count()<<endl;
				for(int i=0; i<cpus; i++){
					//cout<<"HELLO2"<<endl;
					//cout<<i<<endl;
					if(!alloc[i]){
						//cout<<"HELLO3"<<endl;
						pthread_mutex_lock(&alloc_mutex[i]);
						//cout<<"YES"<<endl;
						
						alloc[i]=dispatch.front();
						
						dispatch.pop();
						//cout<<"Done"<<endl;
						pthread_mutex_unlock(&alloc_mutex[i]);
						break;
					}
					
				}
			}
			auto check2=chrono::high_resolution_clock::now();
			auto count2=chrono::duration_cast<chrono::milliseconds>(check2-start);
			if(count2.count()==time){
				cout<<(double)(time)/1000<<"\t";
				if(alloc[0])
				cout<<alloc[0]->proc_name<<"\t";
				else
				cout<<"IDLE"<<"\t";
				if(alloc[1])
				cout<<alloc[1]->proc_name<<"\t";
				else
				cout<<"IDLE"<<"\t";
				if(io_queue.front())
				cout<<io_queue.front()->proc_name<<endl;
				else 
				cout<<"IDLE"<<endl;
				time+=100;
			}

			if(dispatch.empty()){
				done=true;
			}
		}

		for(int i=0; i<cpus; i++){
			pthread_join(id[i], NULL);
		}
		pthread_join(ioid, NULL);
		
	}
};

void init_proc(list<entry>& pcb, string line){
	stringstream ss(line);
	
	entry temp;
	
	ss>>temp.proc_name;
	ss>>temp.priority;
	ss>>temp.arrival_time;
	ss>>temp.proc_type;
	ss>>temp.cpu_time;
	ss>>temp.io_time;
	
	//cout<<temp.proc_name<<temp.arrival_time<<temp.cpu_time<<endl;

	temp.status='P';
	
	pcb.push_back(temp);
}

int main(int argc, char* argv[]){
	list<entry> pcb;

	fstream ifile(argv[1], ios::in);
	string buff;
	getline(ifile, buff);
	while(getline(ifile, buff)){
		init_proc(pcb, buff);
	}
	
	if(argv[3][0]=='f'){
		fcfs scheduler;
		scheduler.init(&pcb, stoi(argv[2]));
		scheduler.schedule();
	}
	else if(argv[3][0]=='r'){
		
	}
	else if(argv[3][0]=='p'){

	}
	
	return 0;
}