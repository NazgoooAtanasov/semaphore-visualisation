#include <raylib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <semaphore.h>
#include <unistd.h>
#include <assert.h>

#define WINDOW_W 1368
#define WINDOW_H 768

#define SEM_BOX_PADDING 5
#define GET_SEM_W(n) (float)(WINDOW_W - n * SEM_BOX_PADDING) / n
#define SEM_H (WINDOW_H / 2.0) - SEM_BOX_PADDING
#define FONT_SIZE 20

#define SEMS_N 2
#define THDS_N 4

typedef struct {
    char* names[255];
    int count;
} Queue;

bool is_empty(Queue* q) {
    return q->count == 0;
}

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
    char* name;
    bool blocked;
    Queue p_queue;
} Semaphore;
Semaphore sems[SEMS_N];

typedef struct {
    char name[255];
    bool blocked;
    bool has_locked_sem;
    bool dead;
    Semaphore* sem;
} Thread;
Thread thds[THDS_N];

void sem_handle_next_process(Semaphore* sem) {
    if (is_empty(&sem->p_queue)) {
        sem->blocked = false;
        return;
    }

    const char* name = sem->p_queue.names[0];
    dequeue(&sem->p_queue);

    Thread* thd = NULL;
    for (int i = 0; i < THDS_N; ++i) {
        if (strcmp(thds[i].name, name) == 0) {
            thd = &thds[i];
            break;
        }
    }
    assert(thd != NULL && "There is no thread running with that name");

    thd->blocked = false;
    thd->has_locked_sem = true;
}

void attach_thd_button_events(Thread* thd, Rectangle r) {
    Vector2 mouse = GetMousePosition();

    if (CheckCollisionPointRec(mouse, r) && ((IsMouseButtonPressed(MOUSE_BUTTON_LEFT)))) {
        printf("THD: [%s]. SEM: [%s]\n", thd->name, thd->sem->name);

        if (!thd->has_locked_sem) {
            if (!thd->sem->blocked) {
                thd->sem->blocked = true;
                thd->has_locked_sem = true;
            } else {
                enqueue(&thd->sem->p_queue, thd->name);
                thd->blocked = true;
                thd->has_locked_sem = true;
            }
        } else {
            thd->has_locked_sem = false;
            sem_handle_next_process(thd->sem);
        }
    }
}

void draw_thds(Thread* thds, size_t count) {
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
            DrawText(thds[i].name, ((r.width / 2) + r.x) - text_len / 2.0, r.y + SEM_BOX_PADDING, FONT_SIZE, WHITE);
            DrawText("[TERMINATED]", ((r.width / 2) + r.x) - text_len / 2.0, r.y + SEM_BOX_PADDING + FONT_SIZE, FONT_SIZE, WHITE);
            continue;
        }

        DrawRectangleRec(r, thds[i].blocked ? RED : GREEN);
        DrawText(thds[i].name, ((r.width / 2) + r.x) - text_len / 2.0, r.y + SEM_BOX_PADDING, FONT_SIZE, WHITE);
        DrawText(thds[i].blocked ? "[BLOCKED]" : "[RUNNING]", ((r.width / 2) + r.x) - text_len / 2.0, r.y + SEM_BOX_PADDING + FONT_SIZE, FONT_SIZE, WHITE);

        if (!thds[i].blocked) {
            Rectangle lock_btn = {
                .x = r.x + SEM_BOX_PADDING,
                .y = r.height - 40,
                .width = r.width - (SEM_BOX_PADDING * 2),
                .height = 40
            };
            DrawRectangleRec(lock_btn, GRAY);
            text_len = strlen(thds[i].has_locked_sem ? "UNLOCK" : "LOCK") * FONT_SIZE;
            DrawText(thds[i].has_locked_sem ? "UNLOCK" : "LOCK", lock_btn.x + (lock_btn.width / 2) - text_len / 2.0, lock_btn.y, FONT_SIZE, BLACK);

            attach_thd_button_events(&thds[i], lock_btn);
        }
    }
}

void draw_sems(Semaphore* sems, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        Rectangle r = {
            .x = (GET_SEM_W(count) * i) + SEM_BOX_PADDING * (i + 1),
            .y = WINDOW_H / 2.0 + (SEM_BOX_PADDING / 2.0),
            .width = GET_SEM_W(count) - SEM_BOX_PADDING,
            .height = SEM_H
        };

        DrawRectangleRec(r, sems[i].blocked ? RED : GREEN);
        int text_len = strlen(sems[i].name) * FONT_SIZE;
        DrawText(sems[i].name, ((r.width / 2) + r.x) - text_len / 2.0, r.y + SEM_BOX_PADDING, FONT_SIZE, WHITE);
         
        for (int j = 0; j < sems[i].p_queue.count; ++j) {
            DrawText(sems[i].p_queue.names[j], ((r.width / 2) + r.x) - text_len / 2.0, (r.y + (FONT_SIZE * (j + 1))) + SEM_BOX_PADDING, FONT_SIZE, WHITE);
        }
    }
}

int main(void) {
    InitWindow(WINDOW_W, WINDOW_H, "Semaphore visualisation");
    SetTargetFPS(60);

    sems[0].blocked = false;
    sems[0].name = "SEM0";

    sems[1].blocked = false;
    sems[1].name = "SEM1";

    for (int i = 0; i < THDS_N; ++i) {
        sprintf(thds[i].name, "%s%d", "PID", i);
        thds[i].blocked = false;
        thds[i].dead = false;
        thds[i].has_locked_sem = false;

        if (i < 2) {
            thds[i].sem = &sems[0];
        } else {
            thds[i].sem = &sems[1];
        }
    }

    while(!WindowShouldClose()) {
        BeginDrawing();
        draw_thds(thds, sizeof(thds) / sizeof(Thread));
        draw_sems(sems, sizeof(sems) / sizeof(Semaphore));
        EndDrawing();
    }

    return 0;
}
