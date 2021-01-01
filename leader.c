#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <arpa/inet.h>

#define OUT_DIR "output"
#define READY 0
#define WAITING 1
#define DONE 2
#define SENDING 3

#include "image_proc.h"
#include "socket_helper.h"

struct worker
{
    int id; // unused
    int socket;
};

int images_processed, images_to_process, count, in, out, workers_complete = 0;
int NUM_WORKERS = 3; // Number of workers running on other nodes
int BUFFER_SIZE = 10; // max items in bounded buffer
char *IN_DIR = "input"; // directory of input images
char *PORT = "4611";    // Port number to listen on
struct file_path **buffer;
pthread_cond_t fill, empty = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_t *threads; // Dynamic array for storing thread
struct worker *workers; // Dynamic array with sockets for connecting to worker nodes

/*
 * Purpose: place image in the buffer
 * Input: file path of the image to place in buffer
 * Return: nothing
 */
void put_img(struct file_path *r)
{
    buffer[in] = r;
    in = (in + 1) % BUFFER_SIZE;
    count++;
}

/*
 * Purpose: retrieve image from buffer
 * Input: nothing
 * Return: image retrieved form buffer
 */
struct file_path *get_img()
{
    struct file_path *r = buffer[out];
    out = (out + 1) % BUFFER_SIZE;
    count--;
    return r;
}

/*
 * Purpose: open, step through img directory and throw img file_paths onto the buffer
 * Input: directory name
 * Return: nothing
 */
int process_dir(char *dir_name)
{
    DIR *d;
    struct dirent *dir;
    int temp_images_to_process = 0;
    // First verify output directory exists
    d = opendir(OUT_DIR);
    assert(d);
    // Open input directory and iterate through JPG files
    d = opendir(dir_name);
    assert(d);
    images_to_process = INT_MAX;
    // tell all workers that leader is ready
    for (int i = 0; i < NUM_WORKERS; i++)
        send_int(workers[i].socket, READY);
    // step through directory and process all ".jpg" files
    while ((dir = readdir(d)) != NULL)
    {
        int len = (int) strlen(dir->d_name);
        if (len > 4)
        {
            if (strcmp(dir->d_name + len - 4, ".jpg") == 0)
            {
                struct file_path *r = malloc(sizeof(struct file_path));
                r->dir_name = malloc(strlen(dir_name) + 1);
                r->img_name = malloc(strlen(dir->d_name) + 1);
                strcpy(r->dir_name, dir_name);
                strcpy(r->img_name, dir->d_name);
                assert(count <= BUFFER_SIZE);
                pthread_mutex_lock(&lock);
                temp_images_to_process++;

                while (count == BUFFER_SIZE)
                    pthread_cond_wait(&empty, &lock);

                put_img(r);
                pthread_cond_signal(&fill);
                pthread_mutex_unlock(&lock);
            }
        }
    }

    closedir(d);
    images_to_process = temp_images_to_process; // avoid race cond
    return 0;
}

/*
 * Purpose: pull images off the buffer, coordinate/communicate with worker nodes, feed those images to them
 * Input: worker node data (socket and ID)
 * Return: nothing
 */
void *consumer(void *data)
{
    while (1)
    {
        struct worker *worker_data = data;
        int worker_socket = worker_data->socket;
        assert(count >= 0);
        printf("Leader Consumer started...\n");
        int worker_response = WAITING; // sentinel

        while (worker_response != READY) // wait until worker is ready to receive an image
            receive_int(worker_socket, &worker_response);

        if (worker_response == READY) // worker is ready
        {
            pthread_mutex_lock(&lock); // lock for both cases (DONE, SENDING)

            if (images_processed == images_to_process) // if we are done
            {
                int workers_done = WAITING; // sentinel
                send_int(worker_socket, DONE); // tell worker we're done

                while (workers_done == WAITING)
                {
                    receive_int(worker_socket, &workers_done);
                    if (workers_done == DONE)
                        workers_complete++;
                }

                if (workers_complete == NUM_WORKERS)
                    pthread_cond_signal(&empty);  // wake up main
                pthread_mutex_unlock(&lock);
            }

            else // there are more images
            {
                send_int(worker_socket, SENDING); // inform worker there are more incoming images

                while (count == 0)
                    pthread_cond_wait(&fill, &lock);

                struct file_path *r = get_img();
                pthread_cond_signal(&empty);
                pthread_mutex_unlock(&lock);
                // send directory and img name to worker
                send_string(worker_socket, r->dir_name);
                send_string(worker_socket, r->img_name);
                // free mem
                free(r->dir_name);
                free(r->img_name);
                free(r);
                // avoid race cond in inc
                pthread_mutex_lock(&lock);
                images_processed++;
                pthread_mutex_unlock(&lock);
            }
        }
    }
}

/*
 * Purpose: establish connection with worker nodes
 * Input: nothing
 * Return: success (0), failure (1)
 */
int accept_workers()
{
    int server_socket;
    int current_worker = 0;
    // Initialize the server socket
    server_socket = prepare_server_socket(PORT);
    // Wait for all clients to connect
    while (current_worker < NUM_WORKERS)
    {
        workers[current_worker].socket = accept_client(server_socket);
        workers[current_worker].id = current_worker;
        current_worker++;
        printf("Worker ID: %d, was accepted!\n", current_worker);
    }

    assert(current_worker == NUM_WORKERS);
    printf("all workers accepted\n");
    return 0;
}

/*
 * Purpose: malloc buffer, workers, threads; create workers; begin directory processing; free buffer, workers, threads
 * Input: nothing
 * Return: success (0), failure (1)
 */
int your_main()
{
    buffer = (struct file_path **) malloc(sizeof(struct file_path) * BUFFER_SIZE);
    workers = (struct worker *) malloc(sizeof(struct worker) * NUM_WORKERS);
    threads = (pthread_t *) malloc(NUM_WORKERS * sizeof(pthread_t));
    accept_workers();

    for (int i = 0; i < NUM_WORKERS; i++)
    {
        pthread_t thread;
        pthread_create(&thread, NULL, consumer, &workers[i]);
    }

    process_dir(IN_DIR);
    pthread_mutex_lock(&lock);

    while (workers_complete < NUM_WORKERS)
        pthread_cond_wait(&empty, &lock);

    pthread_mutex_unlock(&lock);
    printf("Distributed %d images\n", images_processed);
    free(workers);
    free(buffer);
    free(threads);
    return 0;
}

/* Do NOT modify this function! */
int main(int argc, char **argv)
{
    if (argc != 1 && argc != 4)
    {
        printf("ERROR: Run with no arguments or with 3 arguments: %s NUM_WORKERS PORT IN_DIR", argv[0]);
        return -1;
    }

    if (argc == 4)
    {
        NUM_WORKERS = atoi(argv[1]);
        PORT = argv[2];
        IN_DIR = argv[3];
    }

    printf("Waiting for %d workers on port %d, input directory \"%s\"\n", NUM_WORKERS, atoi(PORT), IN_DIR);
    your_main();
}
