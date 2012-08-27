/*
   Copyright (c) 2012, Stanislav Yakush(st.yakush@yandex.ru)
   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the EagleMQ nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
   DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
   ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "eagle.h"
#include "queue_t.h"
#include "object.h"
#include "xmalloc.h"
#include "queue.h"
#include "list.h"

void free_subscribed_client_handler(void *ptr);

Queue_t *create_queue_t(const char *name, uint32_t max_msg, uint32_t max_msg_size, uint32_t flags)
{
	Queue_t *queue_t = (Queue_t*)xmalloc(sizeof(*queue_t));

	memcpy(queue_t->name, name, 64);

	queue_t->max_msg = max_msg;
	queue_t->max_msg_size = max_msg_size;
	queue_t->flags = flags;

	queue_t->declared_clients = list_create();
	queue_t->subscribed_clients = list_create();
	queue_t->queue = queue_create();

	queue_t->declare_client = NULL;
	queue_t->undeclare_client = NULL;
	queue_t->subscribe_client = NULL;
	queue_t->unsubscribe_client = NULL;

	EG_QUEUE_SET_FREE_METHOD(queue_t->queue, free_object_list_handler);
	EG_QUEUE_SET_FREE_METHOD(queue_t->subscribed_clients, free_subscribed_client_handler);

	return queue_t;
}

void delete_queue_t(Queue_t *queue_t)
{
	eject_queue_clients(queue_t);

	list_release(queue_t->declared_clients);
	list_release(queue_t->subscribed_clients);

	queue_release(queue_t->queue);

	xfree(queue_t);
}

int push_value_queue_t(Queue_t *queue_t, Object *value)
{
	if (EG_QUEUE_LENGTH(queue_t->queue) >= queue_t->max_msg ||
		OBJECT_SIZE(value) > queue_t->max_msg_size) {
		return EG_STATUS_ERR;
	}

	queue_push_value(queue_t->queue, value);

	return EG_STATUS_OK;
}

Object *get_value_queue_t(Queue_t *queue_t)
{
	return queue_get_value(queue_t->queue);
}

void pop_value_queue_t(Queue_t *queue_t)
{
	Object *object = queue_pop_value(queue_t->queue);

	decrement_references_count(object);
}

Queue_t *find_queue_t(List *list, const char *name)
{
	Queue_t *queue_t;
	ListNode *node;
	ListIterator iterator;

	list_rewind(list, &iterator);
	while ((node = list_next_node(&iterator)) != NULL)
	{
		queue_t = EG_LIST_NODE_VALUE(node);
		if (!strncmp(queue_t->name, name, 64)) {
			return queue_t;
		}
	}

	return NULL;
}

uint32_t get_size_queue_t(Queue_t *queue_t)
{
	return EG_QUEUE_LENGTH(queue_t->queue);
}

void purge_queue_t(Queue_t *queue_t)
{
	queue_purge(queue_t->queue);
}

void declare_queue_client(Queue_t *queue_t, void *client)
{
	list_add_value_tail(queue_t->declared_clients, client);
}

int undeclare_queue_client(Queue_t *queue_t, void *client)
{
	return list_delete_value(queue_t->declared_clients, client);
}

void subscribe_queue_client(Queue_t *queue_t, void *client, uint32_t flags)
{
	QueueClient *queue_client = (QueueClient*)xmalloc(sizeof(*queue_client));

	queue_client->client = client;
	queue_client->flags = flags;

	list_add_value_tail(queue_t->subscribed_clients, queue_client);
}

int unsubscribe_queue_client(Queue_t *queue_t, void *client)
{
	ListNode *node;
	ListIterator iterator;
	QueueClient *queue_client;

	list_rewind(queue_t->subscribed_clients, &iterator);
	while ((node = list_next_node(&iterator)) != NULL) {
		queue_client = EG_LIST_NODE_VALUE(node);
		if (queue_client->client == client) {
			return list_delete_value(queue_t->subscribed_clients, queue_client);
		}
	}

	return EG_STATUS_ERR;
}

void eject_queue_clients(Queue_t *queue_t)
{
	ListNode *node;
	ListIterator iterator;

	if (queue_t->undeclare_client) {
		list_rewind(queue_t->declared_clients, &iterator);
		while ((node = list_next_node(&iterator)) != NULL) {
			queue_t->undeclare_client(queue_t, EG_LIST_NODE_VALUE(node));
		}
	}

	if (queue_t->unsubscribe_client) {
		list_rewind(queue_t->subscribed_clients, &iterator);
		while ((node = list_next_node(&iterator)) != NULL) {
			queue_t->unsubscribe_client(queue_t, EG_LIST_NODE_VALUE(node));
		}
	}
}

void add_queue_list(List *list, Queue_t *queue_t)
{
	list_add_value_tail(list, queue_t);
}

int delete_queue_list(List *list, Queue_t *queue_t)
{
	return list_delete_value(list, queue_t);
}

void free_queue_list_handler(void *ptr)
{
	delete_queue_t(ptr);
}

void free_subscribed_client_handler(void *ptr)
{
	xfree(ptr);
}