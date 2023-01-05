#include "lstalk.h"

#include <stdio.h>
#include <stdlib.h>

//
// Version infor
//

#define MAJOR 0
#define MINOR 0
#define REVISION 1

//
// Platform definitions
//

#if _WIN32 || _WIN64
    #define WINDOWS 1
#elif __APPLE__
    #define APPLE 1
#elif __linux__
    #define LINUX 1
#else
    #error "Current platform is not supported."
#endif

//
// Dynamic Array
//
// Very simple dynamic array structure to aid in managing storage and
// retrieval of data.

typedef struct Vector {
    char* data;
    size_t element_size;
    size_t length;
    size_t capacity;
} Vector;

Vector vector_create(size_t element_size) {
    Vector result;
    result.element_size = element_size;
    result.length = 0;
    result.capacity = 1;
    result.data = malloc(element_size * result.capacity);
    return result;
}

void vector_destroy(Vector* vector) {
    if (vector == NULL) {
        return;
    }

    if (vector->data != NULL) {
        free(vector->data);
    }

    vector->data = NULL;
    vector->element_size = 0;
    vector->length = 0;
    vector->capacity = 0;
}

void vector_push(Vector* vector, void* element) {
    if (vector == NULL || element == NULL || vector->element_size == 0) {
        return;
    }

    if (vector->length == vector->capacity) {
        vector->capacity = vector->capacity * 2;
        vector->data = realloc(vector->data, vector->element_size * vector->capacity);
    }

    size_t offset = vector->length * vector->element_size;
    memcpy(vector->data + offset, element, vector->element_size);
    vector->length++;
}

char* vector_get(Vector* vector, size_t index) {
    if (vector == NULL || vector->element_size == 0 || index >= vector->length) {
        return NULL;
    }

    return &vector->data[vector->element_size * index];
}

//
// lstalk API
//
// This is the beginning of the exposed API functions for the library.

int lstalk_init() {
    printf("Initialized lstalk version %d.%d.%d!\n", MAJOR, MINOR, REVISION);
    return 1;
}

void lstalk_shutdown() {
}
