Zachary Ross rossx297@umn.edu                                                                            
George Thompsn thom7918@umn.edu        

# To use:

From the command line:

'''
make

make test
'''

Then input the directory 1 - 10.

## Division of Labor                                                                                          

Zachary Ross created the structure of the queue and initial logic around concurrency
while George Thompson debuged the mutex locks and conditional variables, simplifiing them
and removing the race conditions. We both worked on the image rotation aspect of 
the project.
                                                                                                         
We did not change the Makefile.                                                                         
                                                                                                         
Program design:
    
The concurrency strategy uses the following

two mutexes:
    One to protect access to the request queue
    One to protect access to the log file

two conditional variables:
    One for if the queue is not empty
    One for if the queue is not full

The queue is implemented using a linked list

Initialize global variables and structures for thread synchronization and logging

Function log_pretty_print:
    Log thread activity to a file and stdout with synchronization

Function signal_worker_threads_to_exit:
    Signal all worker threads to prepare for exit

Function processing:
    Traverse a directory and add image files to a shared queue
    Wait on queue is not full condition
    Use queue lock when adding image files
    Signal queue is not empty to a worker after adding
    unlock queue
    Signal worker threads when all files are queued
    Exit after signaling worker threads

Function worker:
    Continuously process image files from the shared queue
    
    Wait for the queue is not empty condition before accessing queue
    lock the queue when accessing it
    Signal that the queue is not full to the producer
    unlock queue

    Perform image rotation and save to output directory
    lock access to the log file
    Log each processed request
    unlock access to the log file
    Exit when signaled after all files are processed

Main Function:
    Parse command line arguments for input and output directories, number of worker threads, and rotation angle
    Initialize shared resources and structures for image processing
    Create and start one processing thread and multiple worker threads
    Wait for all threads to complete their tasks
    Clean up resources and exit program

End Program

We did not use AI for this project






    

    
    


                                                                                                    
                                                                                                         
