#pragma once

#include <stdio.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "globals.h"

#define CREATE_NEW IPC_CREAT|0600

key_t _get_key(int id) {
    return ftok(".", id);
}

pid_t b_execute(char* path, char* arg1)
{
    pid_t new_pid = fork();

    if (new_pid == -1)
    {
        perror("[ERROR]: fork error");
        return -1;
    }

    if (new_pid == 0)
    {
        printf("executing path...\n");
        if (execl(path, path, arg1, NULL) < 0)
        {
            perror("[ERROR]: execl error");
            exit(EXIT_FAILURE);
        }
    }

    return new_pid;
}

void b_send_signal(pid_t target, int signal_id)
{
    if(kill(target, signal_id) < 0)
    {
        char msg[50];
        sprintf(msg, "[ERROR]: signal (%d) sending error", signal_id);
        perror(msg);
    }
}


//shared memory
int b_shm_get_id(int id, size_t size)
{
    int shmid = shmget(_get_key('M'+id), size, CREATE_NEW);
    if (shmid == -1)
    {
        perror("[ERROR]: shmget error");
        exit(EXIT_FAILURE);
    }
    return shmid;
}

void b_shm_remove(int shmid)
{
    if(shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("[ERROR]: Shared memory remove error");
    }
}

void* b_shm_attach(int shmid)
{
    void* ptr = shmat(shmid, 0, 0);
    if (ptr < 0)
    {
        perror("[ERROR]: Shared memory attach error");
        exit(EXIT_FAILURE);
    }
    return ptr;
}