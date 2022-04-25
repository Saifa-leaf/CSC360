to compile: make
to run: ./mts *file*
	the file content must be in format:direction(E,e,W,w) loading_time(int>0) crossing_time(int>0)
	Ex: E 20 4
	    w 2 7

This program simulate train of an automated control system for the railway with 1 main track using pthread.
input 1 and 2 is example file to run the command on


1. How many threads are you going to use? Specify the work that you intend each thread to perform.
	The number of thread use is the same as the amount of trains that needed to be create + main thread. All threads except main will load a train, stored a train in global linked list, run a train on the main track. Main thread will wait to join other threads. 

2. Do the threads work independently? Or, is there an overall “controller” thread?
	The thread will work independently since each thread will load, stored, and run a train.

3. How many mutexes are you going to use? Specify the operation that each mutex will guard.
	3 mutex.
	3.1 mutex will guard adding train to global list function.
	3.2 mutex will guard the dispatcher
	3.3 mutex will wait until all thread reach the mutex to start loading
	    train at the same time

4. Will the main thread be idle? If not, what will it be doing?
	Main thread will wait to join other threads. 

5. How are you going to represent stations (which are collections of loaded trains ready to depart)? That is, whattype of data structure will you use?
	I will be using linked list struct to representing station.

6. How are you going to ensure that data structures in your program will not be modified concurrently?
	From answer 3, I will use mutex to add/remove 1 train at a time to global linked list

7. How many convars are you going to use? For each convar:
(a) Describe the condition that the convar will represent.
(b) Which mutex is associated with the convar? Why?
(c) What operation should be performed once pthread cond wait() has been unblocked and re-acquired the mutex?
	3 convars.
	7.1 cond_remove = signal dispatcher that add to list function is finish
	7.2 cond_add = signal add to list function that dispatcher is finish
		to start adding train to list
	7.3 cond_allload = signal to all thread that main finish create all thread
		to start loading
	
8. In 15 lines or less, briefly sketch the overall algorithm you will use. You may use sentences such as:
If train is loaded, get station mutex, put into queue, release station mutex.
The marker will not read beyond 15 lines.
	Open file, copy each line, close file
	Turn each line into individual struct class
	Pass to function to load,stored,run. Load train in all pthread.
	If finish load, get mutex, add to global list, release mutex.
	If finish add to list, get dispatch mutex, run dispatcher, release dispatcher mutex.
	After dispatch, that pthread join the main thread


