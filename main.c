#include <raylib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <semaphore.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>

#define WINDOW_W 1368
#define WINDOW_H 768

typedef struct {
    char* names[255];
    int count;
} Queue;

void enqueue(Queue* q, char* v) {
    assert(q->count + 1 < 255 && "Queue cannot hold more than 255 elements");

    q->names[q->count] = v;
    q->count++;
}

void dequeue(Queue* q) {
    char* names[255] = {0};

    for (int i = 1; i < q->count; ++i) {
        names[i-1] = q->names[i];
    }

    q->count--;
    memset(q->names, '\0', sizeof(q->names));
    memcpy(q->names, names, sizeof(char*) * q->count);
}

typedef struct {
    sem_t sem;
    char* name;
    bool blocked;
    Queue p_queue;
} RenderingEntry;

typedef struct {
    pthread_t pid;
    char name[255];
    bool blocked;
    bool dead;
} RenderingEntryThd;

typedef struct {
    RenderingEntryThd* thd;
    RenderingEntry* sem;
} ThreadArguments;

#define SEM_BOX_PADDING 5
#define GET_SEM_W(n) (float)(WINDOW_W - n * SEM_BOX_PADDING) / n
#define SEM_H (WINDOW_H / 2.0) - SEM_BOX_PADDING
#define FONT_SIZE 20

void draw_thds(RenderingEntryThd* thds, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        Rectangle r = {
            .x = (GET_SEM_W(count) * i) + SEM_BOX_PADDING * (i + 1),
            .y = SEM_BOX_PADDING,
            .width = GET_SEM_W(count) - SEM_BOX_PADDING,
            .height = SEM_H - (SEM_BOX_PADDING / 2.0)
        };

        int text_len = strlen(thds[i].name) * FONT_SIZE;

        if (thds[i].dead) {
            DrawRectangleRec(r, GRAY);
            DrawText(
                thds[i].name,
                ((r.width / 2) + r.x) - text_len / 2.0,
                r.y + SEM_BOX_PADDING,
                FONT_SIZE,
                WHITE
            );
            DrawText(
                "[TERMINATED]",
                ((r.width / 2) + r.x) - text_len / 2.0,
                r.y + SEM_BOX_PADDING + FONT_SIZE,
                FONT_SIZE,
                WHITE
            );
            continue;
        }

        DrawRectangleRec(r, thds[i].blocked ? RED : GREEN);
        DrawText(
            thds[i].name,
            ((r.width / 2) + r.x) - text_len / 2.0,
            r.y + SEM_BOX_PADDING,
            FONT_SIZE,
            WHITE
        );

        DrawText(
            thds[i].blocked ? "[BLOCKED]" : "[RUNNING]",
            ((r.width / 2) + r.x) - text_len / 2.0,
            r.y + SEM_BOX_PADDING + FONT_SIZE,
            FONT_SIZE,
            WHITE
        );
    }
}

void draw_sems(RenderingEntry* sems, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        Rectangle r = {
            .x = (GET_SEM_W(count) * i) + SEM_BOX_PADDING * (i + 1),
            .y = WINDOW_H / 2.0 + (SEM_BOX_PADDING / 2.0),
            .width = GET_SEM_W(count) - SEM_BOX_PADDING,
            .height = SEM_H
        };

        if (sem_trywait(&sems[i].sem) == 0) {
            sem_post(&sems[i].sem);
            sems[i].blocked = false;
        } else {
            sems[i].blocked = true;
        }

        DrawRectangleRec(r, sems[i].blocked ? RED : GREEN);
        int text_len = strlen(sems[i].name) * FONT_SIZE;
        DrawText(
            sems[i].name,
            ((r.width / 2) + r.x) - text_len / 2.0,
            r.y + SEM_BOX_PADDING,
            FONT_SIZE,
            WHITE
        );
         
        for (int j = 0; j < sems[i].p_queue.count; ++j) {
            DrawText(
                sems[i].p_queue.names[j], 
                ((r.width / 2) + r.x) - text_len / 2.0,
                (r.y + (FONT_SIZE * (j+1))) + SEM_BOX_PADDING,
                FONT_SIZE,
                WHITE
            );
        }
    }
}

#define SEMS_N 2
RenderingEntry sems[SEMS_N];

#define THDS_N 4
RenderingEntryThd thds[THDS_N];
ThreadArguments thd_args[THDS_N] = {0};

void* thread(void* args) {
    ThreadArguments* thd_arg = (ThreadArguments*) args;

    time_t t;
    srand((unsigned) time(&t));

    while(true) {
        thd_arg->thd->blocked = true;
        enqueue(&thd_arg->sem->p_queue, thd_arg->thd->name);

        sem_wait(&thd_arg->sem->sem);
        thd_arg->thd->blocked = false;
        dequeue(&thd_arg->sem->p_queue);

        sleep(rand() % 10);

        sem_post(&thd_arg->sem->sem);

        sleep(1);

        if (rand() % 4 == 0) {
            break;
        }
    }

    thd_arg->thd->dead = true;
    pthread_exit(NULL);
}

int main(void) {
    InitWindow(WINDOW_W, WINDOW_H, "Semaphore visualisation");
    SetTargetFPS(60);

    sems[0].blocked = false;
    sems[0].name = "SEM0";
    sem_init(&sems[0].sem, 0, 1);

    sems[1].blocked = false;
    sems[1].name = "SEM1";
    sem_init(&sems[1].sem, 0, 1);

    for (int i = 0; i < THDS_N; ++i) {
        sprintf(thds[i].name, "%s%d", "PID", i);
        thds[i].blocked = false;
        thds[i].dead = false;

        if (i < 2) {
            thd_args[i].sem = &sems[0];
        } else {
            thd_args[i].sem = &sems[1];
        }
        thd_args[i].thd = &thds[i]; 

        int x = pthread_create(&thds[i].pid, NULL, &thread, (void*)&thd_args[i]);
        assert(x == 0 && "Failed to do thread");
    }

    while(!WindowShouldClose()) {
        BeginDrawing();
        draw_thds(thds, sizeof(thds) / sizeof(RenderingEntryThd));
        draw_sems(sems, sizeof(sems) / sizeof(RenderingEntry));
        EndDrawing();
    }

    sem_destroy(&sems[0].sem);

    return 0;
}
