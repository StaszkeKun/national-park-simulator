#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

typedef struct {
    size_t capacity;
    size_t count;
    size_t head_idx;
    size_t tail_idx;
    pid_t data[VISITORS_LIMIT];
} ringbuffer_t;

void ringbuffer_push_back(ringbuffer_t* buf, pid_t new_element)
{
    if (buf->count >= buf->capacity)
    {
        perror("[ERROR]: tried to push into a full ringbuffer");
        return;
    }

    buf->data[buf->tail_idx] = new_element;

    buf->tail_idx = (buf->tail_idx + 1) % buf->capacity;
    buf->count++;
}

void ringbuffer_pop_front(ringbuffer_t* buf, pid_t* out_element)
{
    if (buf->count == 0)
    {
        return;
    }

    if (out_element != NULL)
    {
        *out_element = buf->data[buf->head_idx];
    }

    buf->head_idx = (buf->head_idx + 1) % buf->capacity;
    buf->count--;
}
void ringbuffer_at(ringbuffer_t* buf, size_t pos, pid_t* out_element)
{
    if (pos >= buf->capacity)
    {
        perror("[ERROR]: tried to check ringbuffer out of bounds");
        return;
    }

    if (pos >= buf->count) {
        return;
    }

    *out_element = buf->data[(buf->head_idx + pos) % buf->capacity];
}