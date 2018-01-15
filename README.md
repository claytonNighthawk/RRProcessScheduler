# RRProcessScheduler
Round Robin Process Scheduler with variable quantum.

Program reads a workload file, forks new processes, and executes each command
Every quantum, the master process parses information from /proc and prints it to the terminal 

To test: type make or make main.