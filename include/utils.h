#pragma once

#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "types.h"
#include "constants.h"

#define CREATE_NEW IPC_CREAT|0600

key_t _get_key(int id)
{
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
        if (execl(path, path, arg1, NULL) < 0)
        {
            perror("[ERROR]: execl error");
            exit(EXIT_FAILURE);
        }
    }

    return new_pid;
}

pthread_t b_execute_thread(void(*func)())
{
    pthread_t tid;
    if (pthread_create(&tid, NULL, (void*(*)(void*))func, NULL) < 0)
    {
        perror("[ERROR]: execute thread error");
        exit(EXIT_FAILURE);
    }
    return tid;
}

void b_signal(pid_t target, int signal)
{
    if(kill(target, signal) < 0)
    {
        char msg[50];
        sprintf(msg, "[ERROR]: signal (%d) sending error to (%d)", signal, target);
        perror(msg);
    }
}

void b_raise(int signal)
{
    b_signal(getpid(), signal);
}

void b_wait_for_wakeup()
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIG_WAKE_UP);
    
    sigwaitinfo(&mask, NULL);
}

bool b_process_exist(pid_t pid)
{
    if (pid <= 0) return false;
    kill(pid, 0);
    if (errno == ESRCH)
    {
        return false;
    }
    return true;
}

void b_sleep(double time)
{
    if (time <= 0.0) return;

    struct timespec tv, rem;
    tv.tv_sec = (time_t)time;
    tv.tv_nsec = (long)((time - tv.tv_sec) * 1e9);

    if (tv.tv_nsec >= 1000000000L)
    {
        tv.tv_sec++;
        tv.tv_nsec -= 1000000000L;
    }

    while (nanosleep(&tv, &rem) == -1)
    {
        if (errno != EINTR)
        {
            perror("[ERROR]: sleep error");
            exit(EXIT_FAILURE);
        }
        tv = rem;
    }
}

// microsecond-precision UNIX epoch
double b_tick()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec/1000000.0;
}

double b_get_time_of_day(double start_time)
{
    return fmod(b_tick() - start_time, CYCLE_TIME);
}

float b_randf(float min, float max)
{
    return min + (float)rand() / RAND_MAX * (max-min);
}

int b_randi(int min, int max)
{
    return min + rand() % (max - min);
}

visitor_data_t* b_get_visitor_by_pid(visitor_data_t* visitor_data_array, pid_t searched_pid)
{
    if(searched_pid == 0) return NULL;

    for (int i = 0; i < VISITORS_LIMIT; i++)
    {
        if (visitor_data_array[i].pid == searched_pid)
        {
            return visitor_data_array + i;
        }
    }
    return NULL;
}

//shared memory
int b_shm_get_id(int id, size_t size)
{
    int shmid = shmget(_get_key('M'+id), size, CREATE_NEW);
    if (shmid == -1)
    {
        perror("[ERROR]: Shared memory get id error");
        exit(EXIT_FAILURE);
    }
    return shmid;
}

void b_shm_remove(int shmid)
{
    if (shmctl(shmid, IPC_RMID, NULL) == -1)
    {
        if (errno == EINVAL) return;
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

void b_shm_dettach(void* addr)
{
    if (shmdt(addr) == -1)
    {
        perror("[ERROR]: Shared memory detach error");
        exit(EXIT_FAILURE);
    }
}

//semaphores
int b_sem_get_id()
{
    int semid = semget(_get_key(0), NSEMS, CREATE_NEW);
    if (semid == -1)
    {
        perror("[ERROR]: Semaphore get id error");
        exit(EXIT_FAILURE);
    }
    return semid;
}

void b_sem_set(int semid, int semnum, int val)
{
    if (semctl(semid, semnum, SETVAL, val) < 0)
    {
        perror("[ERROR]: Semaphore set error");
    }
}

int b_sem_check(int semid, int semnum)
{
    int value = semctl(semid, semnum, GETVAL);
    if (value < 0)
    {
        perror("[ERROR]: Semaphore check value error");
    }
    return value;
}

void b_sem_remove(int semid)
{
    if (semctl(semid, 0, IPC_RMID) < 0)
    {
        if (errno == EINVAL) return;
        perror("[ERROR]: Semaphore remove error");
    }
}

void b_sem_v(int semid, int semnum, int val)
{
    struct sembuf op;
    op.sem_num = semnum;
    op.sem_op = val;
    op.sem_flg = 0;

    if (semop(semid, &op, 1) < 0)
    {
        perror("[ERROR]: Semaphore P operation error");
        exit(EXIT_FAILURE);
    }
}

void b_sem_p(int semid, int semnum, int val)
{
    struct sembuf op;
    op.sem_num = semnum;
    op.sem_op = -val;
    op.sem_flg = 0;

    if (semop(semid, &op, 1) < 0)
    {
        if (errno == EINTR)
        {
            b_sem_p(semid, semnum, val);
            return;
        }
        perror("[ERROR]: Semaphore P operation error");
        exit(EXIT_FAILURE);
    }
}

//message queue
typedef struct
{
    long mtype;
    long message;
} message_t;

int b_msq_get_id(int id)
{
    int msqid = msgget(_get_key('a'+id), CREATE_NEW);
    if (msqid == -1)
    {
        perror("[ERROR]: message queue get id error");
        exit(EXIT_FAILURE);
    }
    return msqid;
}

void b_msq_send(int msqid, long type, long message)
{
    message_t msg;
    msg.mtype = type;
    msg.message = message;
    if (msgsnd(msqid, &msg, sizeof(long), 0) == -1)
    {
        perror("[ERROR]: message queue send error");
        exit(EXIT_FAILURE);
    }
}

long b_msq_receive(int msqid, long type)
{
    message_t msg;
    if (msgrcv(msqid, &msg, sizeof(long), type, 0) == -1)
    {
        if (errno == EINTR)
        {
            return b_msq_receive(msqid, type);
        }
        perror("[ERROR]: message queue receive error");
        exit(EXIT_FAILURE);
    }
    return msg.message;
}

long b_msq_receive_nowait(int msqid, long type)
{
    message_t msg;
    if (msgrcv(msqid, &msg, sizeof(long), type, IPC_NOWAIT) == -1)
    {
        if (errno == ENOMSG)
        {
            return -1;
        }
        perror("[ERROR]: message queue receive error");
        exit(EXIT_FAILURE);
    }
    return msg.message;
}

void b_msq_remove(int msqid)
{
    if (msgctl(msqid, IPC_RMID, NULL) == -1)
    {
        if (errno == EINVAL) return;
        perror("[ERROR]: message queue remove error");
        exit(EXIT_FAILURE);
    }
}

//fifo
void b_fifo_delete(char* path)
{
    if (unlink(path) == -1)
    {
        if (errno != ENOENT)
        {
            perror("[ERROR]: fifo delete error");
            exit(EXIT_FAILURE);
        }
    }
}

void b_fifo_create(char* path)
{
    b_fifo_delete(path);
    if (mkfifo(path, CREATE_NEW) == -1)
    {
        perror("[ERROR]: fifo create error");
        exit(EXIT_FAILURE);
    }
}

int b_fifo_open(char* path, int oflag)
{
    int fd = open(path, oflag);
    if (fd < 0)
    {
        perror("[ERROR]: fd open error");
        exit(EXIT_FAILURE);
    }
    return fd;
}

void b_fifo_close(int fd)
{
    if (close(fd) < 0)
    {
        perror("[ERROR]: fd close error");
    }
}

ssize_t b_fifo_write(int fd, const void* buf, size_t size)
{
    ssize_t result = write(fd, buf, size);
    if (result < 0)
    {
        if (errno == EINTR || errno == EAGAIN)
        {
            return b_fifo_write(fd, buf, size);
        }
        perror("[ERROR]: fd write error");
        exit(EXIT_FAILURE);
    }
    return result;
}

ssize_t b_fifo_read(int fd, void* buf, size_t size)
{
    ssize_t result = read(fd, buf, size);
    if (result < 0)
    {
        if (errno == EINTR)
        {
            return b_fifo_read(fd, buf, size);
        }
        perror("[ERROR]: fd read error");
        exit(EXIT_FAILURE);
    }
    return result;
}