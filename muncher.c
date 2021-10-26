/*
Andy Chamberlain
2021-04-17
CS 475: Operating Systems with Adam A. Smith
Text processing with coordinated multithreading

Each thread has a spinlock where it waits for the right conditions to be met,
signaling the prevous thread in the spinlock, and signaling the following
thread once it has completed one round of its own work
*/


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include <limits.h>

void* reader(void* arguments);
void* measurer(void* arguments);
void* numberer(void* arguments);
void* printer(void* arguments);

typedef struct muncherArgs {
    char *filepath;
    int buffer_size;
    char **buffer;
} muncherArgs;

typedef struct muncherPositions {
    int reader, measurer, numberer, printer;
} muncherPositions;

// global struct that contains how many lines each thread has munched
// or -1 if the thread is finished
muncherPositions *mPoses;

pthread_mutex_t readerMutex;
pthread_mutex_t measurerMutex;
pthread_mutex_t numbererMutex;
pthread_mutex_t printerMutex;

pthread_cond_t readerCond;
pthread_cond_t measurerCond;
pthread_cond_t numbererCond;
pthread_cond_t printerCond;

int total_lines_in_file = INT_MAX; // gets set to the actual value once the reader is done

int main(int argc, char * argv[]){
    // input scrubbing
    if (argc != 3){
        printf("Incorrect number of arguments.\n");
        return 0;
    }
    int buff_length = strlen(argv[2]);
    if (buff_length > 10){
        printf("string buffer size too large.\n");
        return 0;
    }
    for (int i = 0; i < buff_length; i++){
        if (!isdigit(argv[2][i])){
            printf("string buffer must be a positive integer.\n");
            return 0;
        }
    }
    if (atol(argv[2]) > 0x7FFFFFFF){
        printf("string buffer size too large.\n");
        return 0;
    }
    if (atol(argv[2]) < 1){
        printf("string buffer must be a positive integer.\n");
        return 0;
    }

    // allocate memory for the muncher Positions global struct
    mPoses = (muncherPositions*)malloc(sizeof(muncherPositions));

    pthread_cond_init(&readerCond, NULL);
    pthread_cond_init(&measurerCond, NULL);
    pthread_cond_init(&numbererCond, NULL);
    pthread_cond_init(&printerCond, NULL);

    pthread_mutex_init(&readerMutex, NULL);
    pthread_mutex_init(&measurerMutex, NULL);
    pthread_mutex_init(&numbererMutex, NULL);
    pthread_mutex_init(&printerMutex, NULL);

    int buffer_size = atoi(argv[2]);

    // allocate the buffer
    char **buffer = (char**)malloc(sizeof(char**) * buffer_size);

    muncherArgs *args = (muncherArgs*)malloc(sizeof(muncherArgs));

    args->filepath = argv[1];
    args->buffer_size = buffer_size;
    args->buffer = buffer;

    // initialize muncher Positions
    mPoses->reader = 0;
    mPoses->measurer = 0;
    mPoses->numberer = 0;
    mPoses->printer = 0;

    // spin off and join the threads, returning a unique error value
    // in each failure case
    pthread_t p1, p2, p3, p4;
    if(pthread_create(&p1, NULL, &reader, args)) return 1;
    if(pthread_create(&p2, NULL, &measurer, args)) return 2;
    if(pthread_create(&p3, NULL, &numberer, args)) return 3;
    if(pthread_create(&p4, NULL, &printer, args)) return 4;

    if(pthread_join(p1, NULL)) return 5;
    if(pthread_join(p2, NULL)) return 6;
    if(pthread_join(p3, NULL)) return 7;
    if(pthread_join(p4, NULL)) return 8;

    // free the buffer and the strings in it
    for (int i = 0; i < buffer_size && i < total_lines_in_file; i++){
        free(buffer[i]);
    }
    free(buffer);

    pthread_cond_destroy(&readerCond);
    pthread_cond_destroy(&measurerCond);
    pthread_cond_destroy(&numbererCond);
    pthread_cond_destroy(&printerCond);

    pthread_mutex_destroy(&readerMutex);
    pthread_mutex_destroy(&measurerMutex);
    pthread_mutex_destroy(&numbererMutex);
    pthread_mutex_destroy(&printerMutex);

    return 0;
}


void* reader(void* arguments){
    // reads each line of the input file into the top-level buffer

    FILE *fp;
    muncherArgs *args = (muncherArgs*)arguments;
    fp = fopen(args->filepath, "r");
    if(!fp){
        printf("Could not open the specified path as a file.\n");
        exit(1);
    }

    char *line_buf_local = NULL;
    size_t line_buf_local_size = 0;
    ssize_t line_size = 0;

    // getline() returns -1 when there are no more lines,
    // and line_size gets getline()
    while (line_size >= 0){
        // reader spinlock
        pthread_mutex_lock(&readerMutex);
        while(!(mPoses->reader - args->buffer_size < mPoses->printer)){
            pthread_cond_signal(&printerCond); // ask the printer if it can go
            pthread_cond_wait(&readerCond, &readerMutex); // wait to get signaled
        }
        pthread_mutex_unlock(&readerMutex);

        line_size = getline(&line_buf_local, &line_buf_local_size, fp); // read line
        // allocate memory for the new line in the top-level buffer
        args->buffer[mPoses->reader % args->buffer_size] = (char*)malloc(sizeof(char) * (line_size + 20));
        line_buf_local[line_size - 1] = '\0'; // get rid of newline character
        // copy over the local buffer into the appropriate slot in the top level buffer
        strcpy(args->buffer[mPoses->reader % args->buffer_size], line_buf_local);
        mPoses->reader++; // since we have now read another line

        pthread_cond_signal(&measurerCond); // ask the measurer thread if it can go
    }

    total_lines_in_file = mPoses->reader - 1;

    mPoses->reader = -1; // reader is finished
    pthread_cond_signal(&measurerCond);

    free(line_buf_local);
    fclose(fp);
    
    return NULL;
}

void* measurer(void* arguments){
    // appends the length of each string in the buffer to the buffer, in parens

    muncherArgs *args = (muncherArgs*)arguments;

    while (mPoses->measurer < total_lines_in_file){
        // measurer spinlock
        pthread_mutex_lock(&measurerMutex);
        while(!(mPoses->measurer < mPoses->reader || mPoses->reader < 0)){
            pthread_cond_signal(&readerCond);
            pthread_cond_wait(&measurerCond, &measurerMutex);
        }
        pthread_mutex_unlock(&measurerMutex);

        // we need the length of the string so we can append to it
        int length = strlen(args->buffer[mPoses->measurer % args->buffer_size]);
        char len_str[11];
        if(length > 9999999) length = 9999999; // largest number that fits into len_str as allocated
        sprintf(len_str, " (%d)", length); // create string to append
        strcat(args->buffer[mPoses->measurer % args->buffer_size], len_str); // append!

        mPoses->measurer++;

        pthread_cond_signal(&numbererCond);
    }

    mPoses->measurer = -1; // measurer is finished
    pthread_cond_signal(&numbererCond);

    return NULL;
}

void* numberer(void* arguments){
    muncherArgs *args = (muncherArgs*)arguments;

    while (mPoses->numberer < total_lines_in_file){
        // numberer spinlock
        pthread_mutex_lock(&numbererMutex);
        while(!(mPoses->numberer < mPoses->measurer || mPoses->measurer < 0)){
            pthread_cond_signal(&measurerCond);
            pthread_cond_wait(&numbererCond, &numbererMutex);
        }
        pthread_mutex_unlock(&numbererMutex);

        // do work
        int num = mPoses->numberer;
        if (mPoses->numberer > 999999) num = 999999;
        char num_str[9];
        sprintf(num_str, "%d: ", num);

        int shamt = strlen(num_str);
        int line_length = strlen(args->buffer[mPoses->numberer % args->buffer_size]);

        // shift string over by length of number label
        for(int i = line_length - 1; i >= 0; i--){
            args->buffer[mPoses->numberer % args->buffer_size][i + shamt] = args->buffer[mPoses->numberer % args->buffer_size][i];
        }

        // fill in number at front
        for(int i = 0; i < shamt; i++){
            args->buffer[mPoses->numberer % args->buffer_size][i] = num_str[i];
        }

        mPoses->numberer++;

        pthread_cond_signal(&printerCond);
    }

    mPoses->numberer = -1; // numberer is finished
    pthread_cond_signal(&printerCond);
}

void* printer(void* arguments){
    muncherArgs *args = (muncherArgs*)arguments;

    while (mPoses->printer < total_lines_in_file){
        // printer spinlock
        pthread_mutex_lock(&printerMutex);
        while(!(mPoses->printer < mPoses->numberer || mPoses->numberer < 0)){
            pthread_cond_signal(&numbererCond);
            pthread_cond_wait(&printerCond, &printerMutex);
        }
        pthread_mutex_unlock(&printerMutex);

        // check the looping condition again, it may have changed while in the spinlock
        // this is only necessary in this thread as it is the last thread to go.
        if(mPoses->printer >= total_lines_in_file) break;

        // do the printing
        printf("%s\n", args->buffer[mPoses->printer % args->buffer_size]);
        mPoses->printer++;

        pthread_cond_signal(&readerCond);
    }

    mPoses->printer = -1; // printer is finished
    // no need to signal because this is the last thread to go.

    return NULL;
}
