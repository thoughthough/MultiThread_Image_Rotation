#include "../include/image_rotation.h"
#include <stdatomic.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#define STB_IMAGE_IMPLEMENTATION

int queue_length = 0;       //Global integer to indicate the length of the queue
int num_workers = 0;   //Global integer to indicate the number of worker threads
FILE* log_file = NULL;              //Global file pointer for writing to log file in worker
pthread_mutex_t log_file_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t thread_ids[MAX_THREADS];  //To track the ID's of your threads in a global array 100
pthread_cond_t queue_not_empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t queue_not_full = PTHREAD_COND_INITIALIZER;
pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
int exit_flag = false;

/*
    The Function takes:
    to_write: A file pointer of where to write the logs. 
    requestNumber: the request number that the thread just finished.
    file_name: the name of the file that just got processed. 

    The function output: 
    it should output the threadId, requestNumber, file_name into the logfile and stdout.
*/
void log_pretty_print(FILE* to_write, int threadId, int requestNumber, char * file_name) {

    pthread_mutex_lock(&log_file_mutex);  // Protecting the log file access
    // Write to the log file and stdout
    fprintf(to_write, "[%d][%d][%s]\n", threadId, requestNumber, file_name);
    fflush(to_write); // Ensure it's written immediately
    printf("[%d][%d][%s]\n", threadId, requestNumber, file_name);

    pthread_mutex_unlock(&log_file_mutex); // Release the mutex
}


// Function to signal all worker threads to exit
void signal_worker_threads_to_exit() {
    pthread_mutex_lock(&queue_lock);
    exit_flag = true;
    pthread_cond_broadcast(&queue_not_empty); // Broadcast to all waiting threads
    pthread_mutex_unlock(&queue_lock);
}

/*
    1: The processing function takes a void* argument called args. It is expected to be a pointer to a structure processing_args_t 
    that contains information necessary for processing.

    2: The processing thread need to traverse a given dictionary and add its files into the shared queue while maintaining synchronization using lock and unlock. 

    3: The processing thread should pthread_cond_signal/broadcast once it finish the traversing to wake the worker up from their wait.

    4: The processing thread will block(pthread_cond_wait) for a condition variable until the workers are done with the processing of the requests and the queue is empty.

    5: The processing thread will cross check if the condition from step 4 is met and it will signal to the worker to exit and it will exit.
*/
void *processing(void *args) {
    processing_args_t *processing_args = (processing_args_t *)args;
    request_queue *queue = processing_args->queue;
    int rotation_angle = processing_args->rotation_angle; // Get rotation angle from args
    const char* directory_path = processing_args->input_dir; // Use input_dir from args

    DIR *dir;
    struct dirent *entry;

    dir = opendir(directory_path);
    if (dir == NULL) {
        perror("opendir");
        return NULL;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            const char *ext = strrchr(entry->d_name, '.');
            if (ext && strcmp(ext, ".png") == 0) {
                char full_path[PATH_MAX];
                if (directory_path[strlen(directory_path) - 1] == '/') {
                // If the directory_path already ends with a slash
                    snprintf(full_path, PATH_MAX, "%s%s", directory_path, entry->d_name);
                } else {
                // If the directory_path does not end with a slash
                snprintf(full_path, PATH_MAX, "%s/%s", directory_path, entry->d_name);
                }
                pthread_mutex_lock(&queue_lock);

                while (queue->size == queue->capacity) {
                    pthread_cond_wait(&queue_not_full, &queue_lock);
                }

                // Enqueue the full file path
                strncpy(queue->requests[queue->tail].file_path, full_path, PATH_MAX);
                queue->requests[queue->tail].rotation = rotation_angle; 
                queue->tail = (queue->tail + 1) % queue->capacity;
                queue->size++;

                pthread_cond_signal(&queue_not_empty);
                pthread_mutex_unlock(&queue_lock);
            }
        }
    }

    closedir(dir);
    // Signal worker threads to exit
    signal_worker_threads_to_exit();

    pthread_exit(NULL);
}

/*
    1: The worker threads takes an int ID as a parameter

    2: The Worker thread will block(pthread_cond_wait) for a condition variable that there is a requests in the queue. 

    3: The Worker threads will also block(pthread_cond_wait) once the queue is empty and wait for a signal to either exit or do work.

    4: The Worker thread will processes request from the queue while maintaining synchronization using lock and unlock. 

    5: The worker thread will write the data back to the given output dir as passed in main. 

    6:  The Worker thread will log the request from the queue while maintaining synchronization using lock and unlock.  

    8: Hint the worker thread should be in a While(1) loop since a worker thread can process multiple requests and It will have two while loops in total
        that is just a recommendation feel free to implement it your way :) 
    9: You may need different lock depending on the job.  

*/
void * worker(void *args)
{
    processing_args_t *worker_args = (processing_args_t *)args;
    request_queue *queue = worker_args->queue;
    int threadId = worker_args->thread_id; // Using thread_id from processing_args
    int requestCount = 0; // To keep track of the number of requests handled by this worker
    request current_request;

    char* output_dir = worker_args->output_dir;  // Output directory for processed images

     while(true){
            // lock the stand
            pthread_mutex_lock(&queue_lock);
            // Wait for atleast one pizza to be available on stand
            while(queue->size <= 0){
                // If the orders are completed, consumer should exit
                if(exit_flag){
                    // unlock the stand
                    pthread_mutex_unlock(&queue_lock);
                    pthread_exit(NULL);
                }

                pthread_cond_wait(&queue_not_empty, &queue_lock);
            }


        // Dequeue the file path
        char file_path[PATH_MAX];
        current_request = queue->requests[queue->head];
        strncpy(file_path, current_request.file_path, PATH_MAX);
        queue->head = (queue->head + 1) % queue->capacity;
        queue->size--;
        pthread_cond_signal(&queue_not_full);
        pthread_mutex_unlock(&queue_lock);


        /*
            Stbi_load takes:
                A file name, int pointer for width, height, and bpp

        */

        int width, height, bpp;
        uint8_t* image_result = stbi_load(current_request.file_path, &width, &height, &bpp, CHANNEL_NUM);
        

        uint8_t **result_matrix = (uint8_t **)malloc(sizeof(uint8_t*) * width);
        uint8_t** img_matrix = (uint8_t **)malloc(sizeof(uint8_t*) * width);
        for(int i = 0; i < width; i++){
            result_matrix[i] = (uint8_t *)malloc(sizeof(uint8_t) * height);
            img_matrix[i] = (uint8_t *)malloc(sizeof(uint8_t) * height);
        }

        /*
        linear_to_image takes: 
            The image_result matrix from stbi_load
            An image matrix
            Width and height that were passed into stbi_load
        */

        linear_to_image(image_result, img_matrix, width, height);
        
        if (current_request.rotation == 180) {
            // Call the function to flip the image left to right
            flip_left_to_right(img_matrix, result_matrix, width, height);
        } else // (rotation_angle == 270) 
        {
            // Call the function to flip the image upside down
            flip_upside_down(img_matrix, result_matrix, width, height);
        }

        uint8_t* img_array = (uint8_t*)malloc(width * height * CHANNEL_NUM * sizeof(uint8_t));
        flatten_mat(result_matrix, img_array, width, height);

        // Extracting the original filename
        char output_file_path[PATH_MAX] = "";
        const char* original_filename = strrchr(current_request.file_path, '/');

        // Construct the output file path
        snprintf(output_file_path, PATH_MAX, "%s%s", output_dir, original_filename);

        int stride = width * CHANNEL_NUM; // Calculate stride
        if (stbi_write_png(output_file_path, width, height, CHANNEL_NUM, img_array, stride) == 0) {
            fprintf(stderr, "Failed to write image to %s\n", output_file_path);
            free(image_result);
            for (int i = 0; i < width; i++) {
                free(result_matrix[i]);
                free(img_matrix[i]);
            }
            free(result_matrix);
            free(img_matrix);
            free(img_array);
            return NULL;
        }

        requestCount++;
        log_pretty_print(log_file, threadId, requestCount, file_path);
        // Free allocated memory
        free(image_result);
        for (int i = 0; i < width; i++) {
            free(result_matrix[i]);
            free(img_matrix[i]);
        }
        free(result_matrix);
        free(img_matrix);
        free(img_array);

    }
    return NULL;
}


/*
    Main:
        Get the data you need from the command line argument 
        Open the logfile
        Create the threads needed     
        Join on the created threads
        Clean any data if needed. 
The command line arguments for the main function are the following: 
input_dir                   argv[1]     Input_dir is used for the single process thread
output_dir                  argv[2]     output_dir is used for the rest of the worker threads
number of worker threads    argv[3]
Rotation angle              argv[4]
*/
int main(int argc, char* argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <input_dir> <output_dir> <num_workers> <rotation_angle>\n", argv[0]);
        return 1; // Exit if the argument count is incorrect
    }
    
    char *input_dir = argv[1];
    char *output_dir = argv[2];
    int num_workers = atoi(argv[3]);
    int rotation_angle = atoi(argv[4]);

    request_queue queue;
    queue.requests = malloc(sizeof(request) * num_workers); // Allocate memory for requests
    queue.capacity = num_workers;
    queue.size = 0;
    queue.head = 0;
    queue.tail = 0;

    if (!queue.requests) {
        perror("Failed to allocate memory for requests");
        return 1;
    }

    // Open the log file
    log_file = fopen(LOG_FILE_NAME, "w");
    if (log_file == NULL) {
        perror("Failed to open log file");
        return 1;
    }

    // Create processing and worker threads and pass unique IDs
    processing_args_t proc_args[num_workers + 1]; // Array of processing_args for each worker and one for the processing thread
    pthread_t workers[num_workers];
    pthread_t proc_thread;

    // Setup and create the processing thread
    proc_args[0].queue = &queue;
    proc_args[0].thread_id = -1; // or any unique identifier for the processing thread
    proc_args[0].rotation_angle = rotation_angle;
    proc_args[0].input_dir = input_dir;
    proc_args[0].output_dir = output_dir;
    if (pthread_create(&proc_thread, NULL, processing, &proc_args[0]) != 0) {
        perror("Failed to create processing thread");
        return 1; // Exit if thread creation fails
    }

    // Create worker threads
    for (int i = 0; i < num_workers; i++) {
        proc_args[i + 1].queue = &queue;
        proc_args[i + 1].thread_id = i; // Assign unique ID
        proc_args[i + 1].rotation_angle = rotation_angle;
        proc_args[i + 1].input_dir = input_dir;
        proc_args[i + 1].output_dir = output_dir;
        if (pthread_create(&workers[i], NULL, worker, &proc_args[i + 1]) != 0) {
            perror("Failed to create worker thread");
            return 1; // Exit if thread creation fails
        }
    }

    // Join on the processing thread
    pthread_join(proc_thread, NULL);

    // Join on the worker threads
    for (int i = 0; i < num_workers; i++) {
        pthread_join(workers[i], NULL);
    }

    free(queue.requests);
    pthread_mutex_destroy(&queue_lock);
    pthread_cond_destroy(&queue_not_empty);
    pthread_cond_destroy(&queue_not_full);
    fclose(log_file);

    return 0;
}
