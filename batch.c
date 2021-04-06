#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h> 
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/wait.h>
#include "Job.h"

typedef unsigned int u_int;
#define DEBUG

//error code
#define EINVAL       1//invalid cmd
#define E2BIG        2//cmd too long

#define MAX_NUM_ARGS     8  //max number of arguments of commandline
#define JOB_QUEUE_SIZE   100//set up a large queue 
#define MAX_JOB_NAME_LEN 9  //max size of job name
#define arrival_rate     2//used in test benchmark

void *commandline(void *ptr); //process cmd 
void *executor(void *ptr);    //dispatch thread

//functions used in this project
void showmenu(const char *name, const char *x[]);
int cmd_helpmenu(int nargs, char **args);
int cmd_quit(int nargs, char **args);
int cmd_run(int nargs, char **args);
int cmd_2func(char *args);
int sort_jobs ();
void swap_jobs(int j);//used in re-order jobs in job queue

int cmd_list(int nargs, char **args);
int cmd_switch2sjf(int nargs, char **args);
int cmd_switch2fcfs(int nargs, char **args);
int cmd_switch2pri(int nargs, char **args);
int printpolicy();

int cmd_helpTest(int nargs, char **args);
int cmd_testBenchmark(int nargs, char **args);
//#define LOW_ARRIVAL_RATE /* Long arrivel-time interval */
#define LOW_SERVICE_RATE   /* Long service time */

pthread_mutex_t job_queue_lock;  
pthread_cond_t queue_not_empty; 
//Global variables needs to be locked
u_int buff_in = 0;//next available slot to store job
u_int buff_out = 0;//next available job for execv
u_int total_jobs = 0;//all jobs has been processed
u_int num_jobs_inQueue = 0;//number of jobs in waiting status
int policy = 0;// 0 for fcfs, 1 for SJF, 2 for priority
int total_waiting = 0;
int total_CPU = 0;

//using struct to represent job 
struct myjob{
    char job_name[MAX_JOB_NAME_LEN];
    int process_time;
    int priority;
	char arri_time[9];//use char array to save current time, format is 16:04:03
    char start_time[9];
    char end_time[9];
	char progress[9];//0 for just submitted, 1 for running
} jobs[JOB_QUEUE_SIZE] = {
};

static const char *helpmenu[] = {
    "run <job> <time> <pri>: Submit a job named <job>, execution time is <time>, priority is <pri>.",
    "list: Print Current jobs in queue.  ",   
    "fcfs: Change the scheduling policy to FCFS.",
    "sjf:  Change the scheduling policy to SJF.",
    "priority: Change the scheduling policy to Priority.",
    "test <benchmark> <policy> <num_of_jobs> <priority_levels> <min_CPU_time> <max_CPU_time>.",
    "quit: Exit AUbatch.                 ",
    NULL
};

void showmenu(const char *name, const char *x[]){
    int ct;
	int i; 
    printf("\n");
    printf("%s\n", name);
    for(i = ct = 0; x[i]; i++){
        ct++;
    }
    for(i = 0; i < ct; i++){
        printf("    %s\n", x[i]);
    }
    printf("\n");
}

int cmd_helpmenu(int n, char **args){
    (void)n;
    (void)args;
    showmenu("AUbatch help menu", helpmenu);
    return 0;
}

//build cmd dictionary
static struct{
    const char *name;
    int(*func)(int nargs, char **args);
} cmdtable[] ={
    {"help\n",   cmd_helpmenu}, 
    {"h\n",      cmd_helpmenu},
    {"?\n",      cmd_helpmenu},
    {"run",      cmd_run},
    {"r",        cmd_run},
    {"quit\n",   cmd_quit},
    {"q\n",      cmd_quit},
    {"list\n",   cmd_list},
    {"l\n",      cmd_list},
    {"fcfs\n",    cmd_switch2fcfs},
    {"f\n",       cmd_switch2fcfs},   
    {"sjf\n",     cmd_switch2sjf},
    {"s\n",       cmd_switch2sjf},
    {"priority\n",cmd_switch2pri},
    {"p\n",       cmd_switch2pri},
    {"help",      cmd_helpTest},
    {"h",         cmd_helpTest},
    {"?",         cmd_helpTest},  
	{"test",      cmd_testBenchmark},
    {"t",         cmd_testBenchmark},
    {NULL, NULL}
};

int cmd_helpTest(int nargs, char **a){
    if(nargs != 2){
        printf("Usage: help -test\n");
        return EINVAL;
    }
    int i, j, ct, l, x;
    printf("\n");
    for(i = ct = 0; helpmenu[i]; i++){
        ct++;
    } 
    for(i = l = 0; a[1][i]; i++){
        l++;
    }
    for(i = 0; i < ct; i++){
        for(j = 1; j < l; j++){
            if(a[1][j] != helpmenu[i][j - 1])
                break;
            x = i;
        }
    }
    printf("%s\n\n", helpmenu[x]);
    return 0;
}

//swap jobs at j and j-1
void swap_jobs(int j){
    int i;
    char temp_job_name[MAX_JOB_NAME_LEN];
    int temp_process_time;
    int temp_priority;
    char temp_arri_time[9];
    char temp_start_time[9];
    char temp_end_time[9]; 
    char temp_progress[9];
    for(i = 0; i < MAX_JOB_NAME_LEN; i++){
        temp_job_name[i] = jobs[j].job_name[i];
        jobs[j].job_name[i] = jobs[j - 1].job_name[i];
        jobs[j - 1].job_name[i] = temp_job_name[i];
    }
    temp_process_time = jobs[j].process_time;
    jobs[j].process_time = jobs[j - 1].process_time;
    jobs[j - 1].process_time = temp_process_time;
    temp_priority = jobs[j].priority;
    jobs[j].priority = jobs[j - 1].priority;
    jobs[j - 1].priority = temp_priority;
    for(i = 0; i < 9; i++){
        temp_arri_time[i] = jobs[j].arri_time[i];
        jobs[j].arri_time[i] = jobs[j - 1].arri_time[i];
        jobs[j - 1].arri_time[i] = temp_arri_time[i];
    }
    for(i = 0; i < 9; i++){
        temp_start_time[i] = jobs[j].start_time[i];
        jobs[j].start_time[i] = jobs[j - 1].start_time[i];
        jobs[j - 1].start_time[i] = temp_start_time[i];
    }
    for(i = 0; i < 9; i++){
        temp_end_time[i] = jobs[j].end_time[i];
        jobs[j].end_time[i] = jobs[j - 1].end_time[i];
        jobs[j - 1].end_time[i] = temp_end_time[i];
    }
    for(i = 0; i < 9; i++){
        temp_progress[i] = jobs[j].progress[i];
        jobs[j].progress[i] = jobs[j - 1].progress[i];
        jobs[j - 1].progress[i] = temp_progress[i];
    }
}

//arrange job orders of waiting jobs
int sort_jobs(){
    if(policy == 1){
        for(int i = buff_out; i < buff_in; i++){
            for(int j = buff_in - 1; j > buff_out + 1; j--){
                if(jobs[j-1].process_time > jobs[j].process_time){
                    swap_jobs(j);
                }                     
            }
        }
        return 1;
    }
    else if(policy == 2){
        for(int i = buff_out; i < buff_in; i++){
            for(int j = buff_in - 1; j > buff_out + 1; j--){
                if(jobs[j-1].priority > jobs[j].priority){
                    swap_jobs(j);// Smallest priority first 
                }
            }
        }
        return 2;
    }
    else{
        for(int i = buff_out; i < buff_in; i++){
            for(int j = buff_in - 1; j > buff_out + 1; j--){
                int prev_hour = (jobs[j-1].arri_time[0] - '0') * 10 + (jobs[j-1].arri_time[1] - '0');
                int prev_min = (jobs[j-1].arri_time[3] - '0') * 10 + (jobs[j-1].arri_time[4] - '0');
                int prev_sec = (jobs[j-1].arri_time[6] - '0') * 10 + (jobs[j-1].arri_time[7] - '0');
                int prev_arrival = prev_hour * 3600 + prev_min * 60 + prev_sec * 1;
                int next_hour = (jobs[j].arri_time[0] - '0') * 10 + (jobs[j].arri_time[1] - '0');
                int next_min = (jobs[j].arri_time[3] - '0') * 10 + (jobs[j].arri_time[4] - '0');
                int next_sec = (jobs[j].arri_time[6] - '0') * 10 + (jobs[j].arri_time[7] - '0');
                int next_arrival = next_hour * 3600 + next_min * 60 + next_sec * 1;
                if(prev_arrival > next_arrival){
                    swap_jobs(j);
                }
            }           
        }
        return 0;
    }   
} 

//arrange job orders of waiting jobs
int sort_Benchjobs(){
    if(policy == 1){
        for(int i = buff_out; i < buff_in; i++){
            for(int j = buff_in - 1; j > buff_out; j--){
                if(jobs[j-1].process_time > jobs[j].process_time){
                    swap_jobs(j);
                }                     
            }
        }
        return 1;
    }
    else if(policy == 2){
        for(int i = buff_out; i < buff_in; i++){
            for(int j = buff_in - 1; j > buff_out; j--){
                if(jobs[j-1].priority > jobs[j].priority){
                    swap_jobs(j);// Smallest priority first 
                }
            }
        }
        return 2;
    }
    else{
        for(int i = buff_out; i < buff_in; i++){
            for(int j = buff_in - 1; j > buff_out; j--){
                int prev_hour = (jobs[j-1].arri_time[0] - '0') * 10 + (jobs[j-1].arri_time[1] - '0');
                int prev_min = (jobs[j-1].arri_time[3] - '0') * 10 + (jobs[j-1].arri_time[4] - '0');
                int prev_sec = (jobs[j-1].arri_time[6] - '0') * 10 + (jobs[j-1].arri_time[7] - '0');
                int prev_arrival = prev_hour * 3600 + prev_min * 60 + prev_sec * 1;
                int next_hour = (jobs[j].arri_time[0] - '0') * 10 + (jobs[j].arri_time[1] - '0');
                int next_min = (jobs[j].arri_time[3] - '0') * 10 + (jobs[j].arri_time[4] - '0');
                int next_sec = (jobs[j].arri_time[6] - '0') * 10 + (jobs[j].arri_time[7] - '0');
                int next_arrival = next_hour * 3600 + next_min * 60 + next_sec * 1;
                if(prev_arrival > next_arrival){
                    swap_jobs(j);
                }
            }           
        }
        return 0;
    }   
} 

int cmd_quit(int nargs, char **args) {
    (void)nargs;
    (void)args;
    float avg_waiting = total_waiting / total_jobs;
    float avg_cpu = total_CPU / total_jobs;
    float avg_turnaround = avg_waiting + avg_cpu;
    printf("Total number of job submitted : %d\n", total_jobs);
    printf("Average turnaround time :  %10.2f seconds\n", avg_turnaround);
    printf("Average CPU time :\t  %10.2f seconds\n", avg_cpu);
    printf("Average waiting time :\t  %10.2f seconds\n", avg_waiting);
    printf("Throughput:\t\t %10.2f No. / second\n", 1 / avg_turnaround);
    exit(0);
}

//use run to submit a job
int cmd_run(int nargs, char **args){
    if(nargs < 3){
        printf("Usage: run <job> <time> <priority>\n");
        return EINVAL;
    }

    pthread_mutex_lock(&job_queue_lock);
    total_jobs++;

    //save job in queue
    char new_job_name[MAX_JOB_NAME_LEN];
    for(int i = 0; i < MAX_JOB_NAME_LEN; i++){
        new_job_name[i] = args[1][i];
        jobs[buff_in].job_name[i] = args[1][i];
    }
    jobs[buff_in].process_time = atoi(args[2]);
    if(nargs == 4){
        jobs[buff_in].priority = atoi(args[3]);
    }
    else{
        jobs[buff_in].priority = 69; 
        // give a default priority number if priority is missing
    }
    strcpy(jobs[buff_in].progress, "Submit");
    time_t t = time(NULL);
    struct tm *stm = localtime(&t);
    sprintf(jobs[buff_in].arri_time, "%02d:%02d:%02d", stm->tm_hour, stm->tm_min, stm->tm_sec);
    buff_in++;

    if(policy != 0){
        sort_jobs();// if policy is not fcfs, reschedule
    }

    pthread_cond_signal(&queue_not_empty);
    pthread_mutex_unlock(&job_queue_lock);

    //calculate expected waiting time
    int expected_waiting = 0;
    int index;//find the index of this new job
    for(int i = buff_out; i < buff_in; i++){
        if(strncmp(jobs[i].job_name, new_job_name, sizeof(new_job_name)) == 0){
            index = i;
        }
    }
	for(int i = buff_out; i < index; i++){
        expected_waiting += jobs[i].process_time;
    }

    num_jobs_inQueue = buff_in -buff_out;   
    printf("Job %s was submitted.\n", args[1]);
    printf("Total number of jobs in the queue: %d\n", num_jobs_inQueue);
    printf("Expected waiting time : %d seconds\n", expected_waiting);
    printf("Scheduling Policy : ");
    printpolicy();
    return 0; // if succeed
}

int cmd_testBenchmark(int nargs, char **args){
    //check if args are in right format
    if(nargs != 7){
        printf("Usage: test <benchmark> <policy> <num_of_jobs> <priority_levels> <min_CPU_time> <max_CPU_time>\n");
		return EINVAL;
	}
    int min_cpu_time = atoi(args[5]);
    int max_cpu_time = atoi(args[6]);
    if(max_cpu_time < min_cpu_time){
        printf("Max CPU time should be larger than Min CPU time.\n");
		printf("Usage: test <benchmark> <policy> <num_of_jobs> <priority_levels> <min_CPU_time> <max_CPU_time>\n");
		return EINVAL;
	}
	//get args
    if(strlen(args[2]) == 3){
        policy = 1;//SJF
    }
    else if(strlen(args[2]) == 4){
        policy = 0;//FCFS
    }
    else{
        policy = 2;//priority
    }
    int num_jobs = atoi(args[3]);
    int pri_num = atoi(args[4]);	

    printf("Please wait for adding jobs to job queue\n");
    //adding jobs to job queue
	srand(time(NULL));//each time run the benchmark, it will generate different seed, since time is different
	pthread_mutex_lock(&job_queue_lock);
	for(buff_in; buff_in < num_jobs; buff_in++){ 
		//set job name all same as "testJob"
        strcpy(jobs[buff_in].job_name, "testJob");
        jobs[buff_in].process_time = rand() % (max_cpu_time + 1 - min_cpu_time) + min_cpu_time;
		jobs[buff_in].priority = rand() % (pri_num + 1);
        strcpy(jobs[buff_in].progress, "Submit");
		time_t tt = time(NULL);
		struct tm *stm = localtime(&tt);
		sprintf(jobs[buff_in].arri_time, "%02d:%02d:%02d", stm->tm_hour, stm->tm_min, stm->tm_sec);	

        //changing global variables
		total_jobs++;
        num_jobs_inQueue++;
		sleep(arrival_rate); //simulate an arrival rate of 0.5/s      
	}
    //reschedule jobs in waiting queue
    if(policy != 0){
        sort_Benchjobs();
    }
    pthread_cond_signal(&queue_not_empty);
    pthread_mutex_unlock(&job_queue_lock);
}

//transfer from command to function
int cmd_2func(char *cmd){
    char *args[MAX_NUM_ARGS];
    int nargs=0;
    char *word;
    char *context;
    int i;
    int result;
    for(word = strtok_r(cmd, " ", &context);
         word != NULL;
         word = strtok_r(NULL, " ", &context)) {
        if(nargs >= MAX_NUM_ARGS){
            printf("Too many arguments for cmd\n");
            return E2BIG;
        }
        args[nargs++] = word;
    }
    if(nargs==0){
        printf("No argument");
        return 0;
    }
    //transfer cmd to function
    for(i=0; cmdtable[i].name; i++){
        if(*cmdtable[i].name && !strcmp(args[0], cmdtable[i].name)){
            assert(cmdtable[i].func!=NULL);  
            /*Call function through the cmd_table; */
            result = cmdtable[i].func(nargs, args);
            return result;
        }
    }
    printf("%s: Command not found\n", args[0]);
    return EINVAL;
}

int cmd_list(int nargs, char **args){
    (void)nargs;
    (void)args;
    printf("Total number of jobs in the queue: %d\n", num_jobs_inQueue);
    printf("Scheduling Policy: ");
    printpolicy();
    printf("Name\t CPU_Time\t Pri\t Arrival_time\t Start_time\t End_time\t Progress\n");
    for(int i = 0; i < buff_in; i++){
        printf("%s\t", jobs[i].job_name);
        printf(" %d\t\t", jobs[i].process_time);
        printf(" %d\t", jobs[i].priority);
        printf(" %s\t", jobs[i].arri_time);
        printf(" %s\t", jobs[i].start_time);
        printf(" %s\t", jobs[i].end_time);
        printf(" %s\t", jobs[i].progress);
        printf("\n");
    }
    return 0;
}

int printpolicy(){
    switch (policy){
    case 0:
        printf("FCFS\n");
        return 0;
        break;
    case 1:
        printf("SJF\n");
        return 1;
        break;
    case 2:
        printf("Priority\n");
        return 2;
        break;
    };
}

int cmd_switch2sjf(int nargs, char **args){
    (void)nargs;
    (void)args;
    if(policy !=1){
        policy = 1; 
        sort_jobs();
        printf("Scheduling policy is switched to SJF. All the %d waiting jobs have been rescheduled.\n", num_jobs_inQueue);
    }
    else
        printf("Scheduling policy was already SJF.\n");
    return 0;
}

int cmd_switch2fcfs(int nargs, char **args){
    (void)nargs;
    (void)args;
    if(policy != 0){
        policy = 0; 
        sort_jobs();
        printf("Scheduling policy is switched to FCFS. All the %d waiting jobs have been rescheduled.\n", num_jobs_inQueue);
    }
    else
        printf("Scheduling policy was already FCFS.\n");
    return 0;
}

int cmd_switch2pri(int nargs, char **args) {
    (void)nargs;
    (void)args;
    if(policy != 2){
        policy = 2; 
        sort_jobs();
        printf("Scheduling policy is switched to Priority. All the %d waiting jobs have been rescheduled.\n", num_jobs_inQueue);
    }
    else
        printf("Scheduling policy was already Priority.\n");
    return 0;
}

void *commandline(void *ptr){
    char *buffer, *message;
    size_t bufsize = 64;
    //It’s a type which is used to represent the size of objects in bytes and is therefore used as the return type by the sizeof operator   
    buffer = (char*)malloc(bufsize * sizeof(char));
    if(buffer == NULL){
        perror("Unable to malloc buffer");
        exit(1);
    }
    message = (char *)ptr;
    printf("\n");
    printf("Welcome to Xuefei's batch job scheduler Version 1.0\nType 'help' to find more about AUbatch commands.\n");
    printf("\n");
    while(1){
        printf("> [? for menu]: ");
        getline(&buffer, &bufsize, stdin);
        cmd_2func(buffer);
    }
    return 0;
}

void *executor(void *ptr){
    char *message = (char *)ptr;
    time_t tt;//record the start time
	struct tm *stm;
    int temp_wait_time;//waiting time for running process
    pid_t pid;

    while(1){
        //lock the job queue
        pthread_mutex_lock(&job_queue_lock);
        while(num_jobs_inQueue == 0){
            pthread_cond_wait(&queue_not_empty, &job_queue_lock);
        }

        struct myjob temp_job;
        temp_job = jobs[buff_out];
        pthread_mutex_unlock(&job_queue_lock);
        //store status of current job
        total_CPU += temp_job.process_time;
        tt = time(NULL);
        stm = localtime(&tt);
        sprintf(jobs[buff_out].start_time, "%02d:%02d:%02d", stm->tm_hour, stm->tm_min, stm->tm_sec);
		int hour = stm->tm_hour - ((temp_job.arri_time[0] - '0') * 10 + (temp_job.arri_time[1] - '0'));
		int min = stm->tm_min - ((temp_job.arri_time[3] - '0') * 10 + (temp_job.arri_time[4] - '0'));
		int sec = stm->tm_sec - ((temp_job.arri_time[6] - '0') * 10 + (temp_job.arri_time[7] - '0'));
		temp_wait_time = hour * 3600 + min * 60 + sec * 1;
		total_waiting += temp_wait_time;
        strcpy(jobs[buff_out].progress, "Running");
        //run the benchmark job
        char benchmark_job[8] = "testJob"; 
        if(strcmp(temp_job.job_name, benchmark_job) == 0){
            int sleep_time;
            sleep_time = temp_job.process_time;
            sleep(sleep_time);//simulate a process
            time_t t = time(NULL);
            struct tm *stm = localtime(&t);
            sprintf(jobs[buff_out].end_time, "%02d:%02d:%02d", stm->tm_hour, stm->tm_min, stm->tm_sec);
            strcpy(jobs[buff_out].progress, "Finished");
            buff_out++;
            num_jobs_inQueue--;
        }
        //run actual job
        else{
            pid = fork();
            if(pid == 0){
                char process_name[MAX_JOB_NAME_LEN];
                strcpy(process_name, temp_job.job_name);
                char *argv_for_program[] = {process_name, NULL};
                execv(process_name, argv_for_program);
            }
            else{
                wait(NULL);//wait for child process terminate
                int sleep_time;
                sleep_time = temp_job.process_time;
                sleep(sleep_time);//simulate a process
                //store time to the job
                time_t t = time(NULL);
                struct tm *stm = localtime(&t);
                sprintf(jobs[buff_out].end_time, "%02d:%02d:%02d", stm->tm_hour, stm->tm_min, stm->tm_sec);
                strcpy(jobs[buff_out].progress, "Finished");
                buff_out++;
                num_jobs_inQueue--;
            }                                
        }
    } 
}

int main(){   
    pthread_t command_thread, executor_thread; 
    char *message1 = "Commandline Thread";
    char *message2 = "Executor Thread";
    int iret1;
    int iret2;

    num_jobs_inQueue = 0;
    buff_in = 0;
    buff_out = 0;

    iret1 = pthread_create(&command_thread, NULL, commandline, (void*)message1);
    iret2 = pthread_create(&executor_thread, NULL, executor, (void*)message2);

    //Initialize
    pthread_mutex_init(&job_queue_lock, NULL);
    pthread_cond_init(&queue_not_empty, NULL);

    pthread_join(command_thread, NULL);
    pthread_join(executor_thread, NULL);

    return 0;
}
