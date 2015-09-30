#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef WIN32
#include <unistd.h>
#endif

#include "buffer.h"
#include "log.h"
#include "memory.h"
#include "message.h"
#include "packet.h"
#include "session.h"

/* Set to TRUE after getting the 'shutdown' message. */
NBBOOL is_shutdown = FALSE;

/* The maximum length of packets. */
size_t max_packet_length = 10000;

typedef enum
{
  SESSION_STATE_NEW,
  SESSION_STATE_ESTABLISHED
} session_state_t;

typedef struct
{
  /* Session information */
  uint16_t        id;
  session_state_t state;
  uint16_t        their_seq;
  uint16_t        my_seq;
  NBBOOL          is_closed;
  char           *name;

  char           *download;
  uint32_t        download_first_chunk;
  uint32_t        download_current_chunk;

  NBBOOL          is_command;

  buffer_t       *outgoing_data;

  time_t          last_transmit;

  options_t       options;
} session_t;
typedef struct _session_entry_t
{
  session_t *session;
  struct _session_entry_t *next;
} session_entry_t;

static session_entry_t *first_session;

#define RETRANSMIT_DELAY 1 /* Seconds */

/* Allow anything to go out. Call this at the start or after receiving legit data. */
static void reset_counter(session_t *session)
{
  session->last_transmit = 0;
}

/* Wait for a delay or incoming data before retransmitting. Call this after transmitting data. */
static void update_counter(session_t *session)
{
  session->last_transmit = time(NULL);
}

/* Decide whether or not we should transmit data yet. */
static NBBOOL can_i_transmit_yet(session_t *session)
{
  if(time(NULL) - session->last_transmit > RETRANSMIT_DELAY)
    return TRUE;
  return FALSE;
}

static session_t *sessions_get_by_id(uint16_t session_id)
{
  session_entry_t *entry;

  for(entry = first_session; entry; entry = entry->next)
    if(entry->session->id == session_id)
      return entry->session;
  return NULL;
}

static void do_send_packet(session_t *session, packet_t *packet)
{
  size_t length;
  uint8_t *data = packet_to_bytes(packet, &length, session->options);

  message_post_packet_out(data, length);

  safe_free(data);
}

static void do_send_stuff(session_t *session)
{
  packet_t *packet;
  uint8_t  *data;
  size_t    length;

  /* Don't transmit too quickly without receiving anything. */
  if(!can_i_transmit_yet(session))
  {
    LOG_INFO("Retransmission timer hasn't expired, not re-sending...");
    return;
  }

  switch(session->state)
  {
    case SESSION_STATE_NEW:
      LOG_INFO("In SESSION_STATE_NEW, sending a SYN packet (SEQ = 0x%04x)...", session->my_seq);
      packet = packet_create_syn(session->id, session->my_seq, 0);
      if(session->name)
        packet_syn_set_name(packet, session->name);
      if(session->download)
        packet_syn_set_download(packet, session->download);
      if(session->download_first_chunk)
        packet_syn_set_chunked_download(packet);
      if(session->is_command)
        packet_syn_set_is_command(packet);

      update_counter(session);
      do_send_packet(session, packet);

      packet_destroy(packet);
      break;

    case SESSION_STATE_ESTABLISHED:
      if(session->download_first_chunk)
      {
        /* We don't allow outgoing data in chunked mode */
        packet = packet_create_msg_chunked(session->id, session->download_current_chunk);
      }
      else
      {
        /* Read data without consuming it (ie, leave it in the buffer till it's ACKed) */
        data = buffer_read_remaining_bytes(session->outgoing_data, &length, max_packet_length - packet_get_msg_size(session->options), FALSE);
        LOG_INFO("In SESSION_STATE_ESTABLISHED, sending a MSG packet (SEQ = 0x%04x, ACK = 0x%04x, %zd bytes of data...", session->my_seq, session->their_seq, length);

        packet = packet_create_msg_normal(session->id, session->my_seq, session->their_seq, data, length);

        safe_free(data);
      }

      /* Send the packet */
      update_counter(session);
      do_send_packet(session, packet);

      /* Free everything */
      packet_destroy(packet);

      break;

    default:
      LOG_FATAL("Wound up in an unknown state: 0x%x", session->state);
      exit(1);
  }
}

void session_recv(session_t *session, packet_t *packet)
{
}

static void session_destroy(session_t *session)
{
  if(session->name)
    safe_free(session->name);
  if(session->download)
    safe_free(session->download);

  buffer_destroy(session->outgoing_data);
  safe_free(session);
}

static void remove_completed_sessions()
{
  session_entry_t *this;
  session_entry_t *previous = NULL;
  session_entry_t *next;

  for(this = first_session; this; this = next)
  {
    session_t *session = this->session;
    next = this->next;

    if(session->is_closed && buffer_get_remaining_bytes(session->outgoing_data) == 0)
    {
      /* Send a final FIN */
      packet_t *packet = packet_create_fin(session->id, "Session closed");
      LOG_WARNING("Session %d is out of data and closed, killing it!", session->id);
      do_send_packet(session, packet);
      packet_destroy(packet);

      /* Let listeners know that the session is closed before we unlink the session. */
      message_post_session_closed(session->id);

      /* Destroy and unlink the session. */
      session_destroy(session);
      if(previous)
        previous->next = this->next;
      else
        first_session = this->next;
      safe_free(this);
    }
    else
    {
      previous = this;
    }
  }

  if(first_session == NULL && is_shutdown)
  {
    LOG_WARNING("Everything's done!");
    exit(0);
  }
}

static void handle_config_int(char *name, int value)
{
  if(!strcmp(name, "max_packet_length"))
    max_packet_length = value;
}

static void handle_config_string(char *name, char *value)
{
}

static void handle_shutdown()
{
  session_entry_t *entry;

  LOG_WARNING("Received SHUTDOWN message!");

  is_shutdown = TRUE;

  for(entry = first_session; entry; entry = entry->next)
    message_post_close_session(entry->session->id);
}

static uint16_t handle_create_session(char *name, char *download, uint32_t first_chunk, NBBOOL is_command)
{
  session_t *session     = (session_t*)safe_malloc(sizeof(session_t));
  session_entry_t *entry;

  session->id            = rand() % 0xFFFF;
  session->my_seq        = rand() % 0xFFFF; /* Random isn */

  session->state         = SESSION_STATE_NEW;
  session->their_seq     = 0;
  session->is_closed     = FALSE;

  session->outgoing_data = buffer_create(BO_BIG_ENDIAN);

  session->last_transmit = 0;

  session->name = NULL;
  if(name)
  {
    session->name = safe_strdup(name);
    LOG_INFO("Setting session->name to %s", session->name);
  }

  session->download = NULL;
  if(download)
  {
    session->download = safe_strdup(download);
    LOG_INFO("Setting session->download to %s", session->download);
  }

  session->download_first_chunk   = first_chunk;
  session->download_current_chunk = first_chunk;
  session->is_command = is_command;

  /* Add it to the linked list. */
  entry = safe_malloc(sizeof(session_entry_t));
  entry->session = session;
  entry->next = first_session;
  first_session = entry;

  message_post_session_created(session->id);

  return session->id;
}

static void handle_close_session(uint16_t session_id)
{
  session_t *session = sessions_get_by_id(session_id);
  if(!session)
  {
    LOG_ERROR("Tried to access a non-existent session (handle_close_session): %d", session_id);
    return;
  }

  if(session->is_closed)
  {
    LOG_WARNING("Trying to close a closed session: %d", session_id);
  }
  else
  {
    /* Mark the session as closed, it'll be removed in the heartbeat */
    session->is_closed = TRUE;
  }
}

static void handle_data_out(uint16_t session_id, uint8_t *data, size_t length)
{
  session_t *session = sessions_get_by_id(session_id);
  if(!session)
  {
    LOG_ERROR("Tried to access a non-existent session (handle_data_out): %d", session_id);
    return;
  }

  /* Add the bytes to the outgoing data buffer. */
  buffer_add_bytes(session->outgoing_data, data, length);

  /* Trigger a send. */
  do_send_stuff(session);
}

static void handle_ping_request(char *ping_data)
{
  packet_t *packet = packet_create_ping(ping_data);
  size_t length;
  uint8_t *data = packet_to_bytes(packet, &length, 0);

  message_post_packet_out(data, length);

  packet_destroy(packet);
  safe_free(data);
}

static void handle_packet_in(uint8_t *data, size_t length)
{
  NBBOOL poll_right_away = FALSE;

  /* Parse the packet to get the session id */
  packet_t *packet = packet_parse(data, length, 0);
  session_t *session;

  /* Check if it's a ping packet, since those don't need a session. */
  if(packet->packet_type == PACKET_TYPE_PING)
  {
    /* Let listeners know that a ping happened */
    message_post_ping_response(packet->body.ping.data);

    packet_destroy(packet);
    return;
  }

  /* If it's not a ping packet, find the session and handle accordingly. */
  session = sessions_get_by_id(packet->session_id);
  packet_destroy(packet);

  if(!session)
  {
    LOG_ERROR("Tried to access a non-existent session (handle_packet_in): %d", packet->session_id);
    return;
  }

  /* Now that we know the session, parse it properly */
  packet = packet_parse(data, length, session->options);


  switch(session->state)
  {
    case SESSION_STATE_NEW:
      if(packet->packet_type == PACKET_TYPE_SYN)
      {
        LOG_INFO("In SESSION_STATE_NEW, received SYN (ISN = 0x%04x)", packet->body.syn.seq);
        session->their_seq = packet->body.syn.seq;
        session->options   = packet->body.syn.options;
        session->state = SESSION_STATE_ESTABLISHED;
      }
      else if(packet->packet_type == PACKET_TYPE_MSG)
      {
        LOG_WARNING("In SESSION_STATE_NEW, received unexpected MSG (ignoring)");
      }
      else if(packet->packet_type == PACKET_TYPE_FIN)
      {
        LOG_FATAL("In SESSION_STATE_NEW, received FIN: %s", packet->body.fin.reason);

        exit(0);
      }
      else
      {
        LOG_FATAL("Unknown packet type: 0x%02x", packet->packet_type);
        exit(1);
      }

      break;
    case SESSION_STATE_ESTABLISHED:
      if(packet->packet_type == PACKET_TYPE_SYN)
      {
        LOG_WARNING("In SESSION_STATE_ESTABLISHED, recieved SYN (ignoring)");
      }
      else if(packet->packet_type == PACKET_TYPE_MSG)
      {
        LOG_INFO("In SESSION_STATE_ESTABLISHED, received a MSG");

        if(session->download_first_chunk)
        {
          if(packet->body.msg.options.chunked.chunk == session->download_current_chunk)
          {
            /* Let listeners know that data has arrived. */
            message_post_data_in(session->id, packet->body.msg.data, packet->body.msg.data_length);

            /* Go to the next chunk. */
            session->download_current_chunk++;

            /* Don't wait to poll again. */
            reset_counter(session);
            poll_right_away = TRUE;
          }
          else
          {
            LOG_WARNING("Bad chunk received (%d instead of %d)", packet->body.msg.options.chunked.chunk, session->download_current_chunk);
            packet_destroy(packet);
            return;
          }
        }
        else
        {
          /* Validate the SEQ */
          if(packet->body.msg.options.normal.seq == session->their_seq)
          {
            /* Verify the ACK is sane */
            uint16_t bytes_acked = packet->body.msg.options.normal.ack - session->my_seq;

            if(bytes_acked <= buffer_get_remaining_bytes(session->outgoing_data))
            {
              /* Reset the retransmit counter since we got some valid data. */
              reset_counter(session);

              /* Increment their sequence number */
              session->their_seq = (session->their_seq + packet->body.msg.data_length) & 0xFFFF;

              /* Remove the acknowledged data from the buffer */
              buffer_consume(session->outgoing_data, bytes_acked);

              /* Increment my sequence number */
              if(bytes_acked != 0)
              {
                session->my_seq = (session->my_seq + bytes_acked) & 0xFFFF;
                poll_right_away = TRUE;
              }

              /* Print the data, if we received any, and then immediately receive more. */
              if(packet->body.msg.data_length > 0)
              {
                message_post_data_in(session->id, packet->body.msg.data, packet->body.msg.data_length);
                poll_right_away = TRUE;
              }
            }
            else
            {
              LOG_WARNING("Bad ACK received (%d bytes acked; %d bytes in the buffer)", bytes_acked, buffer_get_remaining_bytes(session->outgoing_data));
              packet_destroy(packet);
              return;
            }
          }
          else
          {
            LOG_WARNING("Bad SEQ received (Expected %d, received %d)", session->their_seq, packet->body.msg.options.normal.seq);
            packet_destroy(packet);
            return;
          }
        }
      }
      else if(packet->packet_type == PACKET_TYPE_FIN)
      {
        LOG_FATAL("In SESSION_STATE_ESTABLISHED, received FIN: %s", packet->body.fin.reason);
        message_post_close_session(session->id);
      }
      else
      {
        LOG_FATAL("Unknown packet type: 0x%02x", packet->packet_type);
        message_post_close_session(session->id);
      }

      break;
    default:
      LOG_FATAL("Wound up in an unknown state: 0x%x", session->state);
      packet_destroy(packet);
      message_post_close_session(session->id);
      exit(1);
  }

  /* If there is still outgoing data to be sent, and new data has been ACKed
   * (ie, this isn't a retransmission), send it. */
  if(poll_right_away)
    do_send_stuff(session);

  packet_destroy(packet);
}

static void handle_heartbeat()
{
  session_entry_t *entry;

  for(entry = first_session; entry; entry = entry->next)
  {
    /* Cleanup the incoming/outgoing buffers, if we can, to save memory */
    if(buffer_get_remaining_bytes(entry->session->outgoing_data) == 0)
      buffer_clear(entry->session->outgoing_data);

    /* Send stuff if we can */
    do_send_stuff(entry->session);
  }

  /* Remove any completed sessions. */
  remove_completed_sessions();
}

static void handle_message(message_t *message, void *param)
{
  switch(message->type)
  {
    case MESSAGE_CONFIG:
      if(message->message.config.type == CONFIG_INT)
        handle_config_int(message->message.config.name, message->message.config.value.int_value);
      else if(message->message.config.type == CONFIG_STRING)
        handle_config_string(message->message.config.name, message->message.config.value.string_value);
      break;

    case MESSAGE_SHUTDOWN:
      handle_shutdown();
      break;

    case MESSAGE_CREATE_SESSION:
      message->message.create_session.out.session_id = handle_create_session(message->message.create_session.name, message->message.create_session.download, message->message.create_session.first_chunk, message->message.create_session.is_command);
      break;

    case MESSAGE_CLOSE_SESSION:
      handle_close_session(message->message.close_session.session_id);
      break;

    case MESSAGE_DATA_OUT:
      handle_data_out(message->message.data_out.session_id, message->message.data_out.data, message->message.data_out.length);
      break;

    case MESSAGE_PING_REQUEST:
      handle_ping_request(message->message.ping_request.data);
      break;

    case MESSAGE_PACKET_IN:
      handle_packet_in(message->message.packet_in.data, message->message.packet_in.length);
      break;

    case MESSAGE_HEARTBEAT:
      handle_heartbeat();
      break;

    default:
      break;
  }
}

void sessions_init()
{
  message_subscribe(MESSAGE_CONFIG,         handle_message, NULL);
  message_subscribe(MESSAGE_SHUTDOWN,       handle_message, NULL);
  message_subscribe(MESSAGE_CREATE_SESSION, handle_message, NULL);
  message_subscribe(MESSAGE_CLOSE_SESSION,  handle_message, NULL);
  message_subscribe(MESSAGE_DATA_OUT,       handle_message, NULL);
  message_subscribe(MESSAGE_PING_REQUEST,   handle_message, NULL);
  message_subscribe(MESSAGE_PACKET_IN,      handle_message, NULL);
  message_subscribe(MESSAGE_HEARTBEAT,      handle_message, NULL);
}
