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

int images_processed, images_to_process, count, in, out = 0;
int NUM_WORKERS = 3; // Number of workers running on other nodes
int BUFFER_SIZE = 3; // max items in bounded buffer
char *IN_DIR = "input"; // directory of input images
char *PORT = "4611";    // Port number to listen on
char *SERVER_IP = "127.0.0.1"; // IP of leader node
struct file_path **buffer;
pthread_cond_t fill, empty = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_t *threads; /* Dynamic array for storing threads */


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
 * Purpose: coordinate/communicate with leader node, accept image data, place image data in buffer
 * Input: leader node's socket
 * Return: success (0), failure (1)
 */
int producer_loop(int socket_fd)
{
    char file_name[128];
    char dir_name[128];

    while (1)
    {
        int leader_response = WAITING;
        if (count < BUFFER_SIZE)
        {
            send_int(socket_fd, READY); // tell leader that worker is ready for another image
            receive_int(socket_fd, &leader_response); // check if there are anymore images
            printf("RECEIVED: %d\n", leader_response);

            if (leader_response == SENDING) // there are more images incoming
            {
                receive_string(socket_fd, dir_name, 128);
                receive_string(socket_fd, file_name, 128);
                printf("RX file: %s %s \n", dir_name, file_name);
                images_to_process++;
                struct file_path *r = malloc(sizeof(struct file_path));
                r->dir_name = malloc(strlen(dir_name) + 1);
                r->img_name = malloc(strlen(file_name) + 1);
                strcpy(r->dir_name, dir_name);
                strcpy(r->img_name, file_name);
                assert(count <= BUFFER_SIZE);
                pthread_mutex_lock(&lock);

                while (count == BUFFER_SIZE)
                    pthread_cond_wait(&empty, &lock);

                put_img(r);
                pthread_cond_signal(&fill);
                pthread_mutex_unlock(&lock);
            }

            else
            {
                send_int(socket_fd, DONE);
                return 0;
            }
        }

        else // buffer full; wait until count < BUFFER_SIZE
            send_int(socket_fd, WAITING);
    }
}

/*
 * Purpose: pull images off the buffer, resize those images, free thier memory
 * Input: nothing
 * Return: nothing
 */
void *consumer()
{
    printf("Worker Consumer started...\n");

    for (;;)
    {
        assert(count >= 0);
        pthread_mutex_lock(&lock);

        while (count == 0)
            pthread_cond_wait(&fill, &lock);

        struct file_path *r = get_img();
        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&lock);
        resize_image3x(r);
        free(r->dir_name);
        free(r->img_name);
        free(r);
        pthread_mutex_lock(&lock);
        images_processed++;

        if (images_processed == images_to_process) // check if done
        {
            pthread_cond_signal(&empty); // wake up main
            pthread_mutex_unlock(&lock);
        }

        pthread_mutex_unlock(&lock);
    }
}

/*
 * Purpose: malloc buffer, establish connection with leader node, create threads, begin producer reception, free buffer
 * Input: nothing
 * Return: success (0), failure (1)
 */
int your_main()
{
    buffer = (struct file_path **) malloc(BUFFER_SIZE * sizeof(struct file_path));
    int i, status;
    // Connect to Leader node
    int socket_fd = connect_to_server(SERVER_IP, PORT);
    printf("Creating Consumer threads\n");

    for (i = 0; i < NUM_WORKERS; i++)
    {
        pthread_t thread;
        pthread_create(&thread, NULL, consumer, NULL);
    }

    while (status != READY) // wait until leader node is ready to begin sending image data
        receive_int(socket_fd, &status);

    printf("Starting Producer loop\n");
    producer_loop(socket_fd);
    pthread_mutex_lock(&lock);

    while (images_processed < images_to_process)
        pthread_cond_wait(&empty, &lock);

    pthread_mutex_unlock(&lock);
    free(buffer);
    printf("Processed %d images\n", images_processed);
    return 0;
}


/* Do NOT modify this function! */
int main(int argc, char **argv)
{
    if (argc != 1 && argc != 4)
    {
        printf("ERROR: Run with no arguments or with 3 arguments: %s NUM_WORKERS PORT SERVER (name or ip)", argv[0]);
        return -1;
    }

    if (argc == 4)
    {
        NUM_WORKERS = atoi(argv[1]);
        PORT = argv[2];
        SERVER_IP = argv[3];
    }

    printf("Using %d workers, connecting to %s on port %d\n", NUM_WORKERS, SERVER_IP, atoi(PORT));
    your_main();
}
