#include "utils.h"
#include "types.h"

guide_data_t* guides = NULL;

void handle_tower(int id);
void handle_park(int id);

void handle_kill(int sig)
{
    (void)sig;
    if (guides != NULL)
    {
        b_shm_dettach(guides);
        guides = NULL;
    }
    exit(EXIT_SUCCESS);
}

int main()
{

    struct sigaction sa;
    sa.sa_handler = handle_kill;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    printf("USAGE:\n");
    printf("send signal 1 (force guide\'s group to leave tower): type `tower {guide id}`\n");
    printf("send signal 2 (force guide\'s group to immediately start leaving park): type `leave {guide id}`\n");
    printf("exit CLI: type `exit`\n");
    printf("available guide ids: 0 - %d\n", GUIDES_NUMBER-1);

    int n = snprintf(NULL, 0, "%d", GUIDES_NUMBER-1);
    char choice[8 + n]; // 5 letter word + space + n(guide number) + \n + \0
    while(true)
    {
        printf("> ");
        if (!fgets(choice, sizeof(choice), stdin))
        {
            printf("fgets error... exiting\n");
            break;
        }

        //choice[strcspn(choice, "\n")] = '\0';

        if (strcmp(choice, "exit\n") == 0)
        {
            printf("exiting CLI\n");
            break;
        }

        char cmd[7];
        int id;

        if (sscanf(choice, "%6s %d", cmd, &id) == 2)
        {
            if (strcmp(cmd, "tower") == 0)
            {
                if (id >= 0 && id < GUIDES_NUMBER)
                {
                    handle_tower(id);
                }
                else
                {
                    printf("invalid guide id\n");
                }
            }
            else if (strcmp(cmd, "leave") == 0)
            {
                if (id >= 0 && id < GUIDES_NUMBER)
                {
                    handle_park(id);
                }
                else
                {
                    printf("invalid guide id\n");
                }
            }
            else
            {
                printf("invalid input, usage: `tower {guide id}` | `leave {guide id}` | `exit`\n");
            }
        }
        else
        {
            printf("invalid input, usage: `tower {guide id}` | `leave {guide id}` | `exit`\n");
        }

        if (!strchr(choice, '\n'))
        {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
        }
    }
}

void handle_tower(int id)
{
    pid_t pid;
    int shmid = b_shm_get_id_ifexist(SHM_GUIDES_DATA, sizeof(guide_data_t) * GUIDES_NUMBER);

    if (shmid == -1)
    {
        printf("couldn't access shared memory, maybe the simulation isn't running?\n");
        return;
    }

    guides = b_shm_attach(shmid);

    pid = guides[id].pid;

    b_shm_dettach(guides);
    guides = NULL;

    if (b_process_exist(pid))
    {
        b_signal(pid, SIG_LEAVE_TOWER);
        printf("send SIG_LEAVE_TOWER to [GUIDE %d] pid: %d\n", id, pid);
    }
    else
    {
        printf("couldn't find guide's pid\n");
    }
}

void handle_park(int id)
{
    pid_t pid;
    int shmid = b_shm_get_id_ifexist(SHM_GUIDES_DATA, sizeof(guide_data_t) * GUIDES_NUMBER);

    if (shmid == -1)
    {
        printf("couldn't access shared memory, maybe the simulation isn't running?\n");
        return;
    }

    guides = b_shm_attach(shmid);

    pid = guides[id].pid;

    b_shm_dettach(guides);
    guides = NULL;

    if (b_process_exist(pid))
    {
        b_signal(pid, SIG_LEAVE_PARK);
        printf("send SIG_LEAVE_PARK to [GUIDE %d] pid: %d\n", id, pid);
    }
    else
    {
        printf("couldn't find guide's pid\n");
    }
}