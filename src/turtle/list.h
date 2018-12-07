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
#ifndef TURTLE_LIST_H
#define TURTLE_LIST_H

/* Base list element */
struct turtle_list_element {
        struct turtle_list_element * previous;
        struct turtle_list_element * next;
};

/* List container */
struct turtle_list {
        struct turtle_list_element * head;
        struct turtle_list_element * tail;
        int size;
};

/* List operations */
void turtle_list_append_(struct turtle_list * list, void * element);
void turtle_list_insert_(
    struct turtle_list * list, void * element, int position);
void turtle_list_remove_(struct turtle_list * list, void * element);
void turtle_list_clear_(struct turtle_list * list);

#endif
