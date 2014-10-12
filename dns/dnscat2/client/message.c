#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "memory.h"
#include "types.h"

#include "message.h"

typedef struct _message_handler_entry_t
{
  message_handler_t *handler;
  struct _message_handler_entry_t *next;
} message_handler_entry_t;

static message_handler_entry_t *handlers[MESSAGE_MAX_MESSAGE_TYPE];

static NBBOOL is_initialized = FALSE;

static message_handler_t *message_handler_create(message_callback_t *callback, void *param)
{
  message_handler_t *handler = (message_handler_t *)safe_malloc(sizeof(message_handler_t));

  handler->callback = callback;
  handler->param    = param;

  return handler;
}

/* Put the entry at the start of the linked list. */
void message_subscribe(message_type_t message_type, message_callback_t *callback, void *param)
{
  message_handler_t *handler = message_handler_create(callback, param);
  message_handler_entry_t *entry;

  /* So I don't mess up this array again. */
  assert(message_type < MESSAGE_MAX_MESSAGE_TYPE);

  if(!is_initialized)
  {
    size_t i;
    for(i = 0; i < MESSAGE_MAX_MESSAGE_TYPE; i++)
      handlers[i] = NULL;
    is_initialized = TRUE;
  }

  entry = (message_handler_entry_t *)safe_malloc(sizeof(message_handler_entry_t));
  entry->handler = handler;
  entry->next = handlers[message_type];
  handlers[message_type] = entry;
}

void message_unsubscribe(message_type_t message_type, message_callback_t *callback)
{
  /* TODO */
}

void message_cleanup()
{
  message_handler_entry_t *this;
  message_handler_entry_t *next;
  size_t type;

  for(type = 0; type < MESSAGE_MAX_MESSAGE_TYPE; type++)
  {
    for(this = handlers[type]; this; this = next)
    {
      next = this->next;
      safe_free(this->handler);
      safe_free(this);
    }
  }
}

void message_handler_destroy(message_handler_t *handler)
{
  safe_free(handler);
}

static message_t *message_create(message_type_t message_type)
{
  message_t *message = (message_t *) safe_malloc(sizeof(message_t));
  message->type = message_type;
  return message;
}

void message_destroy(message_t *message)
{
  safe_free(message);
}

void message_post(message_t *message)
{
  message_handler_entry_t *handler;

  for(handler = handlers[message->type]; handler; handler = handler->next)
    handler->handler->callback(message, handler->handler->param);
}

void message_post_config_int(char *name, int value)
{
  message_t *message = message_create(MESSAGE_CONFIG);
  message->message.config.name = name;
  message->message.config.type = CONFIG_INT;
  message->message.config.value.int_value = value;
  message_post(message);
  message_destroy(message);
}

void message_post_config_string(char *name, char *value)
{
  message_t *message = message_create(MESSAGE_CONFIG);
  message->message.config.name = name;
  message->message.config.type = CONFIG_STRING;
  message->message.config.value.string_value = value;
  message_post(message);
  message_destroy(message);
}

void message_post_shutdown()
{
  message_t *message = message_create(MESSAGE_SHUTDOWN);
  message_post(message);
  message_destroy(message);
}

uint16_t message_post_create_session(message_options_t options[])
{
  uint16_t session_id;

  /* Create the message structure */
  message_t *message = message_create(MESSAGE_CREATE_SESSION);

  /* Loop through the options */
  if(options)
  {
    size_t i = 0;
    while(options[i].name)
    {
      if(!strcmp(options[i].name, "name"))
        message->message.create_session.name = options[i].value.s;
      if(!strcmp(options[i].name, "download"))
        message->message.create_session.download = options[i].value.s;
      if(!strcmp(options[i].name, "first_chunk"))
        message->message.create_session.first_chunk = options[i].value.i;
      if(!strcmp(options[i].name, "is_command"))
        message->message.create_session.is_command = options[i].value.i;
      i++;
    }
  }

  message_post(message);

  session_id = message->message.create_session.out.session_id;

  message_destroy(message);

  return session_id;
}

void message_post_session_created(uint16_t session_id)
{
  message_t *message = message_create(MESSAGE_SESSION_CREATED);
  message->message.session_created.session_id = session_id;
  message_post(message);
  message_destroy(message);
}

void message_post_close_session(uint16_t session_id)
{
  message_t *message = message_create(MESSAGE_CLOSE_SESSION);
  message->message.session_created.session_id = session_id;
  message_post(message);
  message_destroy(message);
}

void message_post_session_closed(uint16_t session_id)
{
  message_t *message = message_create(MESSAGE_CLOSE_SESSION);
  message->message.session_created.session_id = session_id;
  message_post(message);
  message_destroy(message);
}

void message_post_data_out(uint16_t session_id, uint8_t *data, size_t length)
{
  message_t *message = message_create(MESSAGE_DATA_OUT);
  message->message.data_out.session_id = session_id;
  message->message.data_out.data = data;
  message->message.data_out.length = length;
  message_post(message);
  message_destroy(message);
}

void message_post_packet_out(uint8_t *data, size_t length)
{
  message_t *message = message_create(MESSAGE_PACKET_OUT);
  message->message.packet_out.data = data;
  message->message.packet_out.length = length;
  message_post(message);
  message_destroy(message);
}

void message_post_packet_in(uint8_t *data, size_t length)
{
  message_t *message = message_create(MESSAGE_PACKET_IN);
  message->message.packet_out.data = data;
  message->message.packet_out.length = length;
  message_post(message);
  message_destroy(message);
}

void message_post_data_in(uint16_t session_id, uint8_t *data, size_t length)
{
  message_t *message = message_create(MESSAGE_DATA_IN);
  message->message.data_in.session_id = session_id;
  message->message.data_in.data = data;
  message->message.data_in.length = length;
  message_post(message);
  message_destroy(message);
}

void message_post_heartbeat()
{
  message_t *message = message_create(MESSAGE_HEARTBEAT);
  message_post(message);
  message_destroy(message);
}

void message_post_ping_request(char *data)
{
  message_t *message = message_create(MESSAGE_PING_REQUEST);
  message->message.ping_request.data = data;
  message_post(message);
  message_destroy(message);
}
void message_post_ping_response(char *data)
{
  message_t *message = message_create(MESSAGE_PING_RESPONSE);
  message->message.ping_response.data = data;
  message_post(message);
  message_destroy(message);
}
