# Pthread-based-batch-scheduling-system
The goal of this project is to design and implement a batch scheduling system called AUbatch using the C programming language and the Pthread library.  
AUbatch is comprised of two distinctive and collaborating threads, namely, the scheduling thread and the dispatching thread.   
The scheduling thread enforces scheduling policies, whereas the dispatching thread has submitted jobs executed by the execv() function.   
The synchronization of these two threads must be implemented by condition variables.  
In addition to condition variables, mutex must be adopted to solve the critical section problem in AUbatch.  
The three scheduling policies to be implemented in your AUbatch are FCFS, SJF, and Priority.   
After implementing three scheduling policies we need  to compare the performance of these three scheduling policies under various workload conditions.  

# system architecture
The scheduling module is in charge of the following two tasks:  
(1) accepting submitted jobs from users  
(2) enforcing a chosen scheduling policy.  
The dispatching module have two responsibilities listed below:  
(1) making use of the execv()function to run jobs sorted in the job queue and  
(2) measuring the execution time and response time (a.k.a., turn-around time) of each finished job.  
![image](https://user-images.githubusercontent.com/59773416/113738213-53e1de80-96c4-11eb-9676-702be8e36ba3.png)  

# user interface
The commonly used commands in AUbatch are (1) help, (2) run, (3) list, (4) change scheduling policies, (5) quit (6) test benchmark.    
These four commands will be parsed to scheduling module.    

# test benchmark
Test benchmark will generate a series of simulated jobs according to user defined parameters.  
Parameters include <policy> <num_of_jobs> <priority_levels> <min_CPU_time> <max_CPU_time>  
System will automatically run these jobs and generate performance metrics. 


# performance evaluation
The two suggested performance metrics are:  
• Average Response Time  
• Throughput  
• Average waiting time
