# RRProcessScheduler
Memory leak free round robin process scheduler with variable quantum. 

Program reads a workload file, forks new processes, and executes each command
Every quantum, the master process parses information from /proc and prints it to the terminal.  

Project was done in 4 parts that built upon each other, uspsv4.c is the final, feature complete version.
 
To test: type make or make main.