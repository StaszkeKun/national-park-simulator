#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define VECTOR_MIN_CAPACITY 5
#define VECTOR_ALLOCATE_THRESHOLD 0.7
#define VECTOR_DEALLOCATE_THRESHOLD 0.3
#define VECTOR_SHRINK 0.6
#define VECTOR_EXTEND 2

typedef struct {
    size_t element_size;
    size_t capacity;
    size_t length;
    void* ptr;
} vector_t;

vector_t* vector_new(size_t element_size)
{
    vector_t* vec = (vector_t*)malloc(sizeof(vector_t));
    if(vec == NULL)
    {
        errno = ENOMEM;
        perror("[ERROR]: vector_new");
        exit(EXIT_FAILURE);
    }

    vec->element_size = element_size;
    vec->capacity = VECTOR_MIN_CAPACITY;
    vec->length = 0;
    vec->ptr = malloc(element_size * VECTOR_MIN_CAPACITY);
    if(vec->ptr == NULL)
    {
        errno = ENOMEM;
        perror("[ERROR]: vector_new");
        exit(EXIT_FAILURE);
    }

    return vec;
}

void vector_free(vector_t* vec)
{
    printf("freed\n");
    free(vec->ptr);
    free(vec);
}

void _vector_reallocate(vector_t* vec, size_t new_capacity)
{
    void* temp = realloc(vec->ptr, vec->element_size * new_capacity);
    if(temp == NULL)
    {
        errno = ENOMEM;
        perror("[ERROR]: _vector_reallocate");
        exit(EXIT_FAILURE);
    }

    vec->ptr = temp;
    vec->capacity = new_capacity;
}

void _vector_set_length(vector_t* vec, size_t new_length)
{
    vec->length = new_length;

    if(vec->length > vec->capacity * VECTOR_ALLOCATE_THRESHOLD)
    {
        _vector_reallocate(vec, vec->capacity * VECTOR_EXTEND);
    }
    else if(vec->length < vec->capacity * VECTOR_DEALLOCATE_THRESHOLD && (int)(vec->capacity * VECTOR_SHRINK) >= VECTOR_MIN_CAPACITY)
    {
        _vector_reallocate(vec, vec->capacity * VECTOR_SHRINK);
    }
}

void vector_at(vector_t* vec, size_t pos, void* out_element)
{
    if (pos >= vec->length) {
        printf("[ERROR]: ATTEMPTING TO ACCESS VECTOR OUT OF BOUNDS\n");
        return;
    }
    memcpy(out_element, vec->ptr + pos * vec->element_size, vec->element_size);
}

void vector_push_back(vector_t* vec, void* new_element)
{
    _vector_set_length(vec, vec->length + 1);
    memcpy(vec->ptr + (vec->length - 1) * vec->element_size, new_element, vec->element_size);
}

void vector_pop_back(vector_t* vec, void* out_element)
{
    if (vec->length == 0)
    {
        printf("[ERROR]: attempting to pop an empty vector\n");
        return;
    }

    if (out_element != NULL)
    {
        memcpy(out_element, vec->ptr + (vec->length - 1) * vec->element_size, vec->element_size);
    }
    _vector_set_length(vec, vec->length - 1);
}

void vector_push_front(vector_t* vec, void* new_element)
{
    _vector_set_length(vec, vec->length + 1);
    memmove(vec->ptr + vec->element_size, vec->ptr, (vec->length - 1) * vec->element_size);
    memcpy(vec->ptr, new_element, vec->element_size);
}

void vector_pop_front(vector_t* vec, void* out_element)
{
    if (vec->length == 0)
    {
        printf("[ERROR]: attempting to pop an empty vector\n");
        return;
    }

    if (out_element != NULL)
    {
        memcpy(out_element, vec->ptr, vec->element_size);
    }
    memmove(vec->ptr, vec->ptr + vec->element_size, (vec->length - 1) * vec->element_size);
    _vector_set_length(vec, vec->length - 1);
}

void vector_remove(vector_t* vec, void* element)
{
    for (int i = 0; i < vec->length; i++)
    {
        void* curr = vec->ptr + i * vec->element_size;
        if (memcmp(curr, element, vec->element_size) == 0)
        {
            size_t len = vec->length - i - 1;
            if (len > 0)
            {
                memmove(curr, curr + vec->element_size, (vec->length - i - 1) * vec->element_size);
            }
            _vector_set_length(vec, vec->length - 1);
            return;
        }
    }
}