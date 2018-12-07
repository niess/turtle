/*
 * Copyright (C) 2017 Université Clermont Auvergne, CNRS/IN2P3, LPC
 * Author: Valentin NIESS (niess@in2p3.fr)
 *
 * Topographic Utilities for tRansporting parTicules over Long rangEs (TURTLE)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

/* Generic double linked list for the TURTLE library */

/* C89 standard library */
#include <stdlib.h>
/* TURTLE library */
#include "turtle/list.h"

void turtle_list_append_(struct turtle_list * list, void * element_)
{
        struct turtle_list_element * element = element_;
        struct turtle_list_element * previous = list->tail;

        element->previous = previous;
        element->next = NULL;
        if (previous == NULL) {
                list->head = element;
        } else {
                previous->next = element;
        }
        list->tail = element;
        list->size++;
}

void turtle_list_insert_(
    struct turtle_list * list, void * element_, int position)
{
        struct turtle_list_element * element = element_;
        struct turtle_list_element * previous = NULL;
        struct turtle_list_element * next = list->head;
 
        for (; (position > 0) && (next != NULL); position--) {
                previous = next;
                next = next->next;
        }
        element->previous = previous;
        if (previous != NULL)
                previous->next = element;
        element->next = next;
        if (next != NULL)
                next->previous = element;

        if (next == list->head)
                list->head = element;
        if (previous == list->tail)
                list->tail = element;
        list->size++;
}

void turtle_list_remove_(struct turtle_list * list, void * element_)
{
        struct turtle_list_element * element = element_;
        struct turtle_list_element * previous = element->previous;
        struct turtle_list_element * next = element->next;

        if (previous == NULL) {
                list->head = next;
        } else {
                previous->next = next;
        }

        if (next == NULL) {
                list->tail = previous;
        } else {
                next->previous = previous;
        }
        list->size--;
}

void turtle_list_clear_(struct turtle_list * list)
{
        struct turtle_list_element * element = list->head;
        while (element != NULL) {
                struct turtle_list_element * next = element->next;
                free(element);
                element = next;
        }
        list->head = list->tail = NULL;
        list->size = 0;
}

void * turtle_list_pop_(struct turtle_list * list)
{
        if (list->tail == NULL)
                return NULL;

        struct turtle_list_element * element = list->tail;
        struct turtle_list_element * previous = element->previous;

        if (previous == NULL) {
                list->head = NULL;
        }
        else {
                previous->next = NULL;
                if (previous->previous == NULL)
                        list->head = previous;
        }
        list->tail = previous;
        list->size--;

        return element;
}
