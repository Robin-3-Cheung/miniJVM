/*

Copyright (c) 2005-2008, Simon Howard

Permission to use, copy, modify, and/or distribute this software 
for any purpose with or without fee is hereby granted, provided 
that the above copyright notice and this permission notice appear 
in all copies. 

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL 
WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE 
AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR 
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, 
NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN      
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 

 */

#include <stdlib.h>
#include <string.h>

#include "d_type.h"
#include "arraylist.h"


/* malloc() / free() testing */

#ifdef ALLOC_TESTING
#include "alloc-testing.h"
#endif

void arraylist_remove_range(ArrayList *arraylist, int index, int length);

/**
 * Automatically resizing array
 *
 * */

ArrayList *arraylist_create(int length) {
    ArrayList *new_arraylist;

    /* If the arr_length is not specified, use a sensible default */

    if (length <= 0) {
        length = 16;
    }

    /* Allocate the new ArrayList and fill in the obj_fields.  There are
     * initially no entries. */

    new_arraylist = (ArrayList *) jvm_calloc(sizeof(ArrayList));

    if (new_arraylist == NULL) {
        return NULL;
    }

    new_arraylist->_alloced = length;
    new_arraylist->length = 0;

    /* Allocate the data array */

    new_arraylist->data = jvm_calloc(length * sizeof(ArrayListValue));

    if (new_arraylist->data == NULL) {
        jvm_free(new_arraylist);
        return NULL;
    }
    pthread_spin_init(&new_arraylist->spinlock, PTHREAD_PROCESS_PRIVATE);

    return new_arraylist;
}

void arraylist_destory(ArrayList *arraylist) {
    /* Do not free if a NULL pointer is passed */

    if (arraylist != NULL) {
        jvm_free(arraylist->data);
        pthread_spin_destroy(&arraylist->spinlock);
        jvm_free(arraylist);
    }
}

static int arraylist_enlarge(ArrayList *arraylist) {
    ArrayListValue *data;
    int newsize;

    /* Double the allocated size */

    newsize = arraylist->_alloced * 2;

    /* Reallocate the array to the new size */

    data = jvm_realloc(arraylist->data, sizeof(ArrayListValue) * newsize);

    if (data == NULL) {
        return 0;
    } else {
        arraylist->data = data;
        arraylist->_alloced = newsize;

        return 1;
    }
}

int arraylist_insert(ArrayList *arraylist, int index, ArrayListValue data) {
    /* Sanity check the index */

    pthread_spin_lock(&arraylist->spinlock);
    {
        if (index < 0 || index > arraylist->length) {
            return 0;
        }
        /* Increase the size if necessary */
        int doit = 1;
        if (arraylist->length + 1 > arraylist->_alloced) {
            if (!arraylist_enlarge(arraylist)) {
                doit = 0;
            }
        }
        if (doit) {
            /* Move the contents of the array forward from the index
             * onwards */

            memmove(&arraylist->data[index + 1],
                    &arraylist->data[index],
                    (arraylist->length - index) * sizeof(ArrayListValue));

            /* Insert the new entry at the index */

            arraylist->data[index] = data;
            ++arraylist->length;
        }
    }
    pthread_spin_unlock(&arraylist->spinlock);

    return 1;
}


int arraylist_remove(ArrayList *arraylist, ArrayListValue data) {
    int index = -1;
    pthread_spin_lock(&arraylist->spinlock);
    {
        int i;
        for (i = 0; i < arraylist->length; ++i) {
            if (arraylist_compare_ptr(arraylist->data[i], data)) {
                index = i;
                break;
            }
        }
        if (index >= 0)
            arraylist_remove_range(arraylist, index, 1);
    }
    pthread_spin_unlock(&arraylist->spinlock);
    return index;
}


int arraylist_push_front(ArrayList *arraylist, ArrayListValue data) {
    return arraylist_insert(arraylist, 0, data);
}

int arraylist_push_end(ArrayList *arraylist, ArrayListValue data) {
    return arraylist_insert(arraylist, arraylist->length, data);
}

void arraylist_remove_range(ArrayList *arraylist, int index, int length) {
    /* Check this is a valid range */


    if (index < 0 || length < 0 || index + length > arraylist->length) {
        return;
    }

    /* Move back the entries following the range to be removed */

    memmove(&arraylist->data[index],
            &arraylist->data[index + length],
            (arraylist->length - (index + length)) * sizeof(ArrayListValue));

    /* Decrease the counter */

    arraylist->length -= length;

}

int arraylist_compare_ptr(ArrayListValue a, ArrayListValue b) {
    return a == b;
}

void arraylist_remove_at(ArrayList *arraylist, int index) {
    pthread_spin_lock(&arraylist->spinlock);
    {
        arraylist_remove_range(arraylist, index, 1);
    }
    pthread_spin_unlock(&arraylist->spinlock);
}

int arraylist_index_of(ArrayList *arraylist,
                       ArrayListEqualFunc equals,
                       ArrayListValue data) {
    int index = -1;
    int i;
    pthread_spin_lock(&arraylist->spinlock);
    {
        for (i = 0; i < arraylist->length; ++i) {
            if (equals(arraylist->data[i], data) != 0) {
                index = i;
                break;
            }
        }
    }
    pthread_spin_unlock(&arraylist->spinlock);
    return index;
}

ArrayListValue arraylist_get_value(ArrayList *arraylist, int index) {
    ArrayListValue value = NULL;
    pthread_spin_lock(&arraylist->spinlock);
    {
        if (index >= 0 && index < arraylist->length)
            value = arraylist->data[index];
    }
    pthread_spin_unlock(&arraylist->spinlock);
    return value;
}

ArrayListValue arraylist_pop_front(ArrayList *arraylist) {
    ArrayListValue v = NULL;
    pthread_spin_lock(&arraylist->spinlock);
    {
        if (arraylist->length > 0) {
            v = arraylist->data[0];
            arraylist_remove_range(arraylist, 0, 1);
        }
    }
    pthread_spin_unlock(&arraylist->spinlock);
    return v;
}

ArrayListValue arraylist_pop_back(ArrayList *arraylist) {
    ArrayListValue v = NULL;
    pthread_spin_lock(&arraylist->spinlock);
    {
        if (arraylist->length > 0) {
            v = arraylist->data[arraylist->length - 1];
            arraylist->length--;
        }
    }
    pthread_spin_unlock(&arraylist->spinlock);
    return v;
}

void arraylist_clear(ArrayList *arraylist) {
    /* To clear the list, simply set the arr_length to zero */

    arraylist->length = 0;
}

static void arraylist_sort_internal(ArrayListValue *list_data, int list_length,
                                    ArrayListCompareFunc compare_func) {
    ArrayListValue pivot;
    ArrayListValue tmp;
    int i;
    int list1_length;
    int list2_length;

    /* If less than two items, it is always sorted. */

    if (list_length <= 1) {
        return;
    }

    /* Take the last item as the pivot. */

    pivot = list_data[list_length - 1];

    /* Divide the list into two lists:
     *
     * List 1 contains data less than the pivot.
     * List 2 contains data more than the pivot.
     *
     * As the lists are build up, they are stored sequentially after
     * each other, ie. list_data[list1_length-1] is the last item
     * in list 1, list_data[list1_length] is the first item in
     * list 2.
     */

    list1_length = 0;

    for (i = 0; i < list_length - 1; ++i) {

        if (compare_func(list_data[i], pivot) < 0) {

            /* This should be in list 1.  Therefore it is in the wrong
             * position. Swap the data immediately following the last
             * item in list 1 with this data. */

            tmp = list_data[i];
            list_data[i] = list_data[list1_length];
            list_data[list1_length] = tmp;

            ++list1_length;

        } else {
            /* This should be in list 2.  This is already in the right
             * position. */
        }
    }

    /* The arr_length of list 2 can be calculated. */

    list2_length = list_length - list1_length - 1;

    /* list_data[0..list1_length-1] now contains all items which are
     * before the pivot.
     * list_data[list1_length..list_length-2] contains all items after
     * or equal to the pivot. */

    /* Move the pivot into place, by swapping it with the item
     * immediately following the end of list 1.  */

    list_data[list_length - 1] = list_data[list1_length];
    list_data[list1_length] = pivot;

    /* Recursively sort the sublists. */

    arraylist_sort_internal(list_data, list1_length, compare_func);

    arraylist_sort_internal(&list_data[list1_length + 1], list2_length,
                            compare_func);
}

void arraylist_sort(ArrayList *arraylist, ArrayListCompareFunc compare_func) {
    /* Perform the recursive sort */
    pthread_spin_lock(&arraylist->spinlock);
    {
        arraylist_sort_internal(arraylist->data, arraylist->length, compare_func);
    }
    pthread_spin_unlock(&arraylist->spinlock);
}


void arraylist_iter_safe(ArrayList *arraylist, ArrayListIteratorFunc func, void *para) {
    pthread_spin_lock(&arraylist->spinlock);
    {
        int i, len;
        for (i = 0, len = arraylist->length; i < len; i++) {
            ArrayListValue value = arraylist->data[i];
            func(value, para);
        }
    }
    pthread_spin_unlock(&arraylist->spinlock);
}