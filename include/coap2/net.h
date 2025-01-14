/*
 * net.h -- CoAP network interface
 *
 * Copyright (C) 2010-2021 Olaf Bergmann <bergmann@tzi.org>
 *
 * This file is part of the CoAP library libcoap. Please see README for terms
 * of use.
 */

#ifndef COAP_NET_H_
#define COAP_NET_H_

#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/time.h>
#endif
#include <time.h>

#ifdef WITH_LWIP
#include <lwip/ip_addr.h>
#endif

#include "coap_io.h"
#include "coap_dtls.h"
#include "coap_event.h"
#include "coap_time.h"
#include "option.h"
#include "pdu.h"
#include "coap_prng.h"
#include "coap_session.h"
#include "resource.h"

/**
 * Queue entry
 */
typedef struct coap_queue_t {
  struct coap_queue_t *next;
  coap_tick_t t;                /**< when to send PDU for the next time */
  unsigned char retransmit_cnt; /**< retransmission counter, will be removed
                                 *    when zero */
  unsigned int timeout;         /**< the randomized timeout value */
  coap_session_t *session;      /**< the CoAP session */
  coap_mid_t id;                /**< CoAP message id */
  coap_pdu_t *pdu;              /**< the CoAP PDU to send */
} coap_queue_t;

/**
 * Adds @p node to given @p queue, ordered by variable t in @p node.
 *
 * @param queue Queue to add to.
 * @param node Node entry to add to Queue.
 *
 * @return @c 1 added to queue, @c 0 failure.
 */
int coap_insert_node(coap_queue_t **queue, coap_queue_t *node);

/**
 * Destroys specified @p node.
 *
 * @param node Node entry to remove.
 *
 * @return @c 1 node deleted from queue, @c 0 failure.
 */
int coap_delete_node(coap_queue_t *node);

/**
 * Removes all items from given @p queue and frees the allocated storage.
 *
 * Internal function.
 *
 * @param queue The queue to delete.
 */
void coap_delete_all(coap_queue_t *queue);

/**
 * Creates a new node suitable for adding to the CoAP sendqueue.
 *
 * @return New node entry, or @c NULL if failure.
 */
coap_queue_t *coap_new_node(void);

typedef enum coap_response_t {
  COAP_RESPONSE_FAIL, /**< Response not liked - send CoAP RST packet */
  COAP_RESPONSE_OK    /**< Response is fine */
} coap_response_t;

/**
 * Response handler that is used as callback in coap_context_t.
 *
 * @param context CoAP session.
 * @param session CoAP session.
 * @param sent The PDU that was transmitted.
 * @param received The PDU that was received.
 * @param id CoAP transaction ID.

 * @return @c COAP_RESPONSE_OK if successful, else @c COAP_RESPONSE_FAIL which
 *         triggers sending a RST packet.
 */
typedef coap_response_t (*coap_response_handler_t)(struct coap_context_t *context,
                                                   coap_session_t *session,
                                                   coap_pdu_t *sent,
                                                   coap_pdu_t *received,
                                                   const coap_mid_t id);

/**
 * Negative Acknowedge handler that is used as callback in coap_context_t.
 *
 * @param context CoAP session.
 * @param session CoAP session.
 * @param sent The PDU that was transmitted.
 * @param reason The reason for the NACK.
 * @param id CoAP message ID.
 */
typedef void (*coap_nack_handler_t)(struct coap_context_t *context,
                                    coap_session_t *session,
                                    coap_pdu_t *sent,
                                    coap_nack_reason_t reason,
                                    const coap_mid_t id);

/**
 * Received Ping handler that is used as callback in coap_context_t.
 *
 * @param context CoAP session.
 * @param session CoAP session.
 * @param received The PDU that was received.
 * @param id CoAP message ID.
 */
typedef void (*coap_ping_handler_t)(struct coap_context_t *context,
                                    coap_session_t *session,
                                    coap_pdu_t *received,
                                    const coap_mid_t id);

/**
 * Received Pong handler that is used as callback in coap_context_t.
 *
 * @param context CoAP session.
 * @param session CoAP session.
 * @param received The PDU that was received.
 * @param id CoAP message ID.
 */
typedef void (*coap_pong_handler_t)(struct coap_context_t *context,
                                    coap_session_t *session,
                                    coap_pdu_t *received,
                                    const coap_mid_t id);

/**
 * The CoAP stack's global state is stored in a coap_context_t object.
 */
struct coap_context_t {
  coap_opt_filter_t known_options;
  coap_resource_t *resources; /**< hash table or list of known
                                   resources */
  coap_resource_t *unknown_resource; /**< can be used for handling
                                          unknown resources */
  coap_resource_t *proxy_uri_resource; /**< can be used for handling
                                            proxy URI resources */
  coap_resource_release_userdata_handler_t release_userdata;
                                        /**< function to  release user_data
                                             when resource is deleted */

#ifndef WITHOUT_ASYNC
  /**
   * list of asynchronous message ids */
  struct coap_async_state_t *async_state;
#endif /* WITHOUT_ASYNC */

  /**
   * The time stamp in the first element of the sendqeue is relative
   * to sendqueue_basetime. */
  coap_tick_t sendqueue_basetime;
  coap_queue_t *sendqueue;
  coap_endpoint_t *endpoint;      /**< the endpoints used for listening  */
  coap_session_t *sessions;       /**< client sessions */

#ifdef WITH_CONTIKI
  struct uip_udp_conn *conn;      /**< uIP connection object */
  struct etimer retransmit_timer; /**< fires when the next packet must be sent */
  struct etimer notify_timer;     /**< used to check resources periodically */
#endif /* WITH_CONTIKI */

#ifdef WITH_LWIP
  uint8_t timer_configured;       /**< Set to 1 when a retransmission is
                                   *   scheduled using lwIP timers for this
                                   *   context, otherwise 0. */
#endif /* WITH_LWIP */

  coap_response_handler_t response_handler;
  coap_nack_handler_t nack_handler;
  coap_ping_handler_t ping_handler;
  coap_pong_handler_t pong_handler;

  /**
   * Callback function that is used to signal events to the
   * application.  This field is set by coap_set_event_handler().
   */
  coap_event_handler_t handle_event;

  ssize_t (*network_send)(coap_socket_t *sock, const coap_session_t *session, const uint8_t *data, size_t datalen);

  ssize_t (*network_read)(coap_socket_t *sock, struct coap_packet_t *packet);

  size_t(*get_client_psk)(const coap_session_t *session, const uint8_t *hint, size_t hint_len, uint8_t *identity, size_t *identity_len, size_t max_identity_len, uint8_t *psk, size_t max_psk_len);
  size_t(*get_server_psk)(const coap_session_t *session, const uint8_t *identity, size_t identity_len, uint8_t *psk, size_t max_psk_len);
  size_t(*get_server_hint)(const coap_session_t *session, uint8_t *hint, size_t max_hint_len);

  void *dtls_context;

  coap_dtls_spsk_t spsk_setup_data;  /**< Contains the initial PSK server setup data */

  unsigned int session_timeout;    /**< Number of seconds of inactivity after which an unused session will be closed. 0 means use default. */
  unsigned int max_idle_sessions;  /**< Maximum number of simultaneous unused sessions per endpoint. 0 means no maximum. */
  unsigned int max_handshake_sessions; /**< Maximum number of simultaneous negotating sessions per endpoint. 0 means use default. */
  unsigned int ping_timeout;           /**< Minimum inactivity time before sending a ping message. 0 means disabled. */
  unsigned int csm_timeout;           /**< Timeout for waiting for a CSM from the remote side. 0 means disabled. */
  uint8_t observe_pending;         /**< Observe response pending */
  uint8_t block_mode;              /**< Zero or more COAP_BLOCK_ or'd options */
  uint64_t etag;                   /**< Next ETag to use */

  coap_cache_entry_t *cache;       /**< CoAP cache-entry cache */
  uint16_t *cache_ignore_options;  /**< CoAP options to ignore when creating a cache-key */
  size_t cache_ignore_count;       /**< The number of CoAP options to ignore when creating a cache-key */
  void *app;                       /**< application-specific data */
#ifdef COAP_EPOLL_SUPPORT
  int epfd;                        /**< External FD for epoll */
  int eptimerfd;                   /**< Internal FD for timeout */
  coap_tick_t next_timeout;        /**< When the next timeout is to occur */
#endif /* COAP_EPOLL_SUPPORT */
};

/**
 * Registers a new message handler that is called whenever a response is
 * received.
 *
 * @param context The context to register the handler for.
 * @param handler The response handler to register.
 */
COAP_STATIC_INLINE void
coap_register_response_handler(coap_context_t *context,
                               coap_response_handler_t handler) {
  context->response_handler = handler;
}

/**
 * Registers a new message handler that is called whenever a confirmable
 * message (request or response) is dropped after all retries have been
 * exhausted, or a rst message was received, or a network or TLS level
 * event was received that indicates delivering the message is not possible.
 *
 * @param context The context to register the handler for.
 * @param handler The nack handler to register.
 */
COAP_STATIC_INLINE void
coap_register_nack_handler(coap_context_t *context,
                           coap_nack_handler_t handler) {
  context->nack_handler = handler;
}

/**
 * Registers a new message handler that is called whenever a CoAP Ping
 * message is received.
 *
 * @param context The context to register the handler for.
 * @param handler The ping handler to register.
 */
COAP_STATIC_INLINE void
coap_register_ping_handler(coap_context_t *context,
                           coap_ping_handler_t handler) {
  context->ping_handler = handler;
}

/**
 * Registers a new message handler that is called whenever a CoAP Pong
 * message is received.
 *
 * @param context The context to register the handler for.
 * @param handler The pong handler to register.
 */
COAP_STATIC_INLINE void
coap_register_pong_handler(coap_context_t *context,
                           coap_pong_handler_t handler) {
  context->pong_handler = handler;
}

/**
 * Registers the option type @p type with the given context object @p ctx.
 *
 * @param ctx  The context to use.
 * @param type The option type to register.
 */
COAP_STATIC_INLINE void
coap_register_option(coap_context_t *ctx, uint16_t type) {
  coap_option_filter_set(&ctx->known_options, type);
}

/**
 * Set sendqueue_basetime in the given context object @p ctx to @p now. This
 * function returns the number of elements in the queue head that have timed
 * out.
 */
unsigned int coap_adjust_basetime(coap_context_t *ctx, coap_tick_t now);

/**
 * Returns the next pdu to send without removing from sendqeue.
 */
coap_queue_t *coap_peek_next( coap_context_t *context );

/**
 * Returns the next pdu to send and removes it from the sendqeue.
 */
coap_queue_t *coap_pop_next( coap_context_t *context );

/**
 * Creates a new coap_context_t object that will hold the CoAP stack status.
 */
coap_context_t *coap_new_context(const coap_address_t *listen_addr);

/**
 * Set the context's default PSK hint and/or key for a server.
 *
 * @param context The current coap_context_t object.
 * @param hint    The default PSK server hint sent to a client. If NULL, PSK
 *                authentication is disabled. Empty string is a valid hint.
 * @param key     The default PSK key. If NULL, PSK authentication will fail.
 * @param key_len The default PSK key's length. If @p 0, PSK authentication will
 *                fail.
 *
 * @return @c 1 if successful, else @c 0.
 */
int coap_context_set_psk( coap_context_t *context, const char *hint,
                           const uint8_t *key, size_t key_len );

/**
 * Set the context's default PSK hint and/or key for a server.
 *
 * @param context    The current coap_context_t object.
 * @param setup_data If NULL, PSK authentication will fail. PSK
 *                   information required.
 *
 * @return @c 1 if successful, else @c 0.
 */
int coap_context_set_psk2(coap_context_t *context,
                          coap_dtls_spsk_t *setup_data);

/**
 * Set the context's default PKI information for a server.
 *
 * @param context        The current coap_context_t object.
 * @param setup_data     If NULL, PKI authentication will fail. Certificate
 *                       information required.
 *
 * @return @c 1 if successful, else @c 0.
 */
int
coap_context_set_pki(coap_context_t *context,
                     const coap_dtls_pki_t *setup_data);

/**
 * Set the context's default Root CA information for a client or server.
 *
 * @param context        The current coap_context_t object.
 * @param ca_file        If not NULL, is the full path name of a PEM encoded
 *                       file containing all the Root CAs to be used.
 * @param ca_dir         If not NULL, points to a directory containing PEM
 *                       encoded files containing all the Root CAs to be used.
 *
 * @return @c 1 if successful, else @c 0.
 */
int
coap_context_set_pki_root_cas(coap_context_t *context,
                              const char *ca_file,
                              const char *ca_dir);

/**
 * Set the context keepalive timer for sessions.
 * A keepalive message will be sent after if a session has been inactive,
 * i.e. no packet sent or received, for the given number of seconds.
 * For unreliable protocols, a CoAP Empty message will be sent. If a
 * CoAP RST is not received, the CoAP Empty messages will get resent based
 * on the Confirmable retry parameters until there is a failure timeout,
 * at which point the session will be considered as disconnected.
 * For reliable protocols, a CoAP PING message will be sent. If a CoAP PONG
 * has not been received before the next PING is due to be sent, the session
 * will be considered as disconnected.
 *
 * @param context        The coap_context_t object.
 * @param seconds        Number of seconds for the inactivity timer, or zero
 *                       to disable CoAP-level keepalive messages.
 */
void coap_context_set_keepalive(coap_context_t *context, unsigned int seconds);

/**
 * Get the libcoap internal file descriptor for using in an application's
 * select() or returned as an event in an application's epoll_wait() call.
 *
 * @param context        The coap_context_t object.
 *
 * @return The libcoap file descriptor or @c -1 if epoll is not available.
 */
int coap_context_get_coap_fd(coap_context_t *context);

/**
 * Returns a new message id and updates @p session->tx_mid accordingly. The
 * message id is returned in network byte order to make it easier to read in
 * tracing tools.
 *
 * @param session The current coap_session_t object.
 *
 * @return        Incremented message id in network byte order.
 */
COAP_STATIC_INLINE uint16_t
coap_new_message_id(coap_session_t *session) {
  return ++session->tx_mid;
}

/**
 * CoAP stack context must be released with coap_free_context(). This function
 * clears all entries from the receive queue and send queue and deletes the
 * resources that have been registered with @p context, and frees the attached
 * endpoints.
 *
 * @param context The current coap_context_t object to free off.
 */
void coap_free_context(coap_context_t *context);

/**
 * Stores @p data with the given CoAP context. This function
 * overwrites any value that has previously been stored with @p
 * context.
 *
 * @param context The CoAP context.
 * @param data The data to store with wih the context. Note that this data
 *             must be valid during the lifetime of @p context.
 */
void coap_set_app_data(coap_context_t *context, void *data);

/**
 * Returns any application-specific data that has been stored with @p
 * context using the function coap_set_app_data(). This function will
 * return @c NULL if no data has been stored.
 *
 * @param context The CoAP context.
 *
 * @return The data previously stored or @c NULL if not data stored.
 */
void *coap_get_app_data(const coap_context_t *context);

/**
 * Creates a new ACK PDU with specified error @p code. The options specified by
 * the filter expression @p opts will be copied from the original request
 * contained in @p request. Unless @c SHORT_ERROR_RESPONSE was defined at build
 * time, the textual reason phrase for @p code will be added as payload, with
 * Content-Type @c 0.
 * This function returns a pointer to the new response message, or @c NULL on
 * error. The storage allocated for the new message must be released with
 * coap_free().
 *
 * @param request Specification of the received (confirmable) request.
 * @param code    The error code to set.
 * @param opts    An option filter that specifies which options to copy from
 *                the original request in @p node.
 *
 * @return        A pointer to the new message or @c NULL on error.
 */
coap_pdu_t *coap_new_error_response(coap_pdu_t *request,
                                    unsigned char code,
                                    coap_opt_filter_t *opts);

/**
 * Sends an error response with code @p code for request @p request to @p dst.
 * @p opts will be passed to coap_new_error_response() to copy marked options
 * from the request. This function returns the message id if the message was
 * sent, or @c COAP_INVALID_MID otherwise.
 *
 * @param session         The CoAP session.
 * @param request         The original request to respond to.
 * @param code            The response code.
 * @param opts            A filter that specifies the options to copy from the
 *                        @p request.
 *
 * @return                The message id if the message was sent, or @c
 *                        COAP_INVALID_MID otherwise.
 */
coap_mid_t coap_send_error(coap_session_t *session,
                           coap_pdu_t *request,
                           unsigned char code,
                           coap_opt_filter_t *opts);

/**
 * Helper function to create and send a message with @p type (usually ACK or
 * RST). This function returns @c COAP_INVALID_MID when the message was not
 * sent, a valid transaction id otherwise.
 *
 * @param session         The CoAP session.
 * @param request         The request that should be responded to.
 * @param type            Which type to set.
 * @return                message id on success or @c COAP_INVALID_MID
 *                        otherwise.
 */
coap_mid_t
coap_send_message_type(coap_session_t *session, coap_pdu_t *request, unsigned char type);

/**
 * Sends an ACK message with code @c 0 for the specified @p request to @p dst.
 * This function returns the corresponding message id if the message was
 * sent or @c COAP_INVALID_MID on error.
 *
 * @param session         The CoAP session.
 * @param request         The request to be acknowledged.
 *
 * @return                The message id if ACK was sent or @c
 *                        COAP_INVALID_MID on error.
 */
coap_mid_t coap_send_ack(coap_session_t *session, coap_pdu_t *request);

/**
 * Sends an RST message with code @c 0 for the specified @p request to @p dst.
 * This function returns the corresponding message id if the message was
 * sent or @c COAP_INVALID_MID on error.
 *
 * @param session         The CoAP session.
 * @param request         The request to be reset.
 *
 * @return                The message id if RST was sent or @c
 *                        COAP_INVALID_MID on error.
 */
COAP_STATIC_INLINE coap_mid_t
coap_send_rst(coap_session_t *session, coap_pdu_t *request) {
  return coap_send_message_type(session, request, COAP_MESSAGE_RST);
}

/**
* Sends a CoAP message to given peer. The memory that is
* allocated for the pdu will be released by coap_send().
* The caller must not use the pdu after calling coap_send().
*
* @param session         The CoAP session.
* @param pdu             The CoAP PDU to send.
*
* @return                The message id of the sent message or @c
*                        COAP_INVALID_MID on error.
*/
coap_mid_t coap_send( coap_session_t *session, coap_pdu_t *pdu );

/**
 * Sends a CoAP message to given peer. The memory that is
 * allocated for the pdu will be released by coap_send_large().
 * The caller must not use the pdu after calling coap_send_large().
 *
 * If the response body is split into multiple payloads using blocks, libcoap
 * will handle asking for the subsequent blocks and any necessary recovery
 * needed.
 *
 * @param session   The CoAP session.
 * @param pdu       The CoAP PDU to send.
 *
 * @return          The message id of the sent message or @c
 *                  COAP_INVALID_MID on error.
 */
coap_mid_t coap_send_large(coap_session_t *session, coap_pdu_t *pdu);

/**
 * Handles retransmissions of confirmable messages
 *
 * @param context      The CoAP context.
 * @param node         The node to retransmit.
 *
 * @return             The message id of the sent message or @c
 *                     COAP_INVALID_MID on error.
 */
coap_mid_t coap_retransmit(coap_context_t *context, coap_queue_t *node);

/**
 * Parses and interprets a CoAP datagram with context @p ctx. This function
 * returns @c 0 if the datagram was handled, or a value less than zero on
 * error.
 *
 * @param ctx    The current CoAP context.
 * @param session The current CoAP session.
 * @param data The received packet'd data.
 * @param data_len The received packet'd data length.
 *
 * @return       @c 0 if message was handled successfully, or less than zero on
 *               error.
 */
int coap_handle_dgram(coap_context_t *ctx, coap_session_t *session, uint8_t *data, size_t data_len);

/**
 * Invokes the event handler of @p context for the given @p event and
 * @p data.
 *
 * @param context The CoAP context whose event handler is to be called.
 * @param event   The event to deliver.
 * @param session The session related to @p event.
 * @return The result from the associated event handler or 0 if none was
 * registered.
 */
int coap_handle_event(coap_context_t *context,
                      coap_event_t event,
                      coap_session_t *session);
/**
 * This function removes the element with given @p id from the list given list.
 * If @p id was found, @p node is updated to point to the removed element. Note
 * that the storage allocated by @p node is @b not released. The caller must do
 * this manually using coap_delete_node(). This function returns @c 1 if the
 * element with id @p id was found, @c 0 otherwise. For a return value of @c 0,
 * the contents of @p node is undefined.
 *
 * @param queue The queue to search for @p id.
 * @param session The session to look for.
 * @param id    The message id to look for.
 * @param node  If found, @p node is updated to point to the removed node. You
 *              must release the storage pointed to by @p node manually.
 *
 * @return      @c 1 if @p id was found, @c 0 otherwise.
 */
int coap_remove_from_queue(coap_queue_t **queue,
                           coap_session_t *session,
                           coap_mid_t id,
                           coap_queue_t **node);

coap_mid_t
coap_wait_ack( coap_context_t *context, coap_session_t *session,
               coap_queue_t *node);

/**
 * Cancels all outstanding messages for session @p session that have the specified
 * token.
 *
 * @param context      The context in use.
 * @param session      Session of the messages to remove.
 * @param token        Message token.
 * @param token_length Actual length of @p token.
 */
void coap_cancel_all_messages(coap_context_t *context,
                              coap_session_t *session,
                              const uint8_t *token,
                              size_t token_length);

/**
* Cancels all outstanding messages for session @p session.
*
* @param context      The context in use.
* @param session      Session of the messages to remove.
* @param reason       The reasion for the session cancellation
*/
void
coap_cancel_session_messages(coap_context_t *context,
                             coap_session_t *session,
                             coap_nack_reason_t reason);

/**
 * Dispatches the PDUs from the receive queue in given context.
 */
void coap_dispatch(coap_context_t *context, coap_session_t *session,
                   coap_pdu_t *pdu);

/**
 * Returns 1 if there are no messages to send or to dispatch in the context's
 * queues. */
int coap_can_exit(coap_context_t *context);

/**
 * Returns the current value of an internal tick counter. The counter counts \c
 * COAP_TICKS_PER_SECOND ticks every second.
 */
void coap_ticks(coap_tick_t *);

/**
 * Verifies that @p pdu contains no unknown critical options. Options must be
 * registered at @p ctx, using the function coap_register_option(). A basic set
 * of options is registered automatically by coap_new_context(). This function
 * returns @c 1 if @p pdu is ok, @c 0 otherwise. The given filter object @p
 * unknown will be updated with the unknown options. As only @c COAP_MAX_OPT
 * options can be signalled this way, remaining options must be examined
 * manually.
 *
 * @code
  coap_opt_filter_t f = COAP_OPT_NONE;
  coap_opt_iterator_t opt_iter;

  if (coap_option_check_critical(ctx, pdu, f) == 0) {
    coap_option_iterator_init(pdu, &opt_iter, f);

    while (coap_option_next(&opt_iter)) {
      if (opt_iter.type & 0x01) {
        ... handle unknown critical option in opt_iter ...
      }
    }
  }
   @endcode
 *
 * @param ctx      The context where all known options are registered.
 * @param pdu      The PDU to check.
 * @param unknown  The output filter that will be updated to indicate the
 *                 unknown critical options found in @p pdu.
 *
 * @return         @c 1 if everything was ok, @c 0 otherwise.
 */
int coap_option_check_critical(coap_context_t *ctx,
                               coap_pdu_t *pdu,
                               coap_opt_filter_t unknown);

/**
 * Creates a new response for given @p request with the contents of @c
 * .well-known/core. The result is NULL on error or a newly allocated PDU that
 * must be either sent with coap_sent() or released by coap_delete_pdu().
 *
 * @param context The current coap context to use.
 * @param session The CoAP session.
 * @param request The request for @c .well-known/core .
 *
 * @return        A new 2.05 response for @c .well-known/core or NULL on error.
 */
coap_pdu_t *coap_wellknown_response(coap_context_t *context,
                                    coap_session_t *session,
                                    coap_pdu_t *request);

/**
 * Calculates the initial timeout based on the session CoAP transmission
 * parameters 'ack_timeout', 'ack_random_factor', and COAP_TICKS_PER_SECOND.
 * The calculation requires 'ack_timeout' and 'ack_random_factor' to be in
 * Qx.FRAC_BITS fixed point notation, whereas the passed parameter @p r
 * is interpreted as the fractional part of a Q0.MAX_BITS random value.
 *
 * @param session session timeout is associated with
 * @param r  random value as fractional part of a Q0.MAX_BITS fixed point
 *           value
 * @return   COAP_TICKS_PER_SECOND * 'ack_timeout' *
 *           (1 + ('ack_random_factor' - 1) * r)
 */
unsigned int coap_calc_timeout(coap_session_t *session, unsigned char r);

/**
 * Function interface for joining a multicast group for listening for the
 * currently defined endpoints that are UDP.
 *
 * @param ctx       The current context.
 * @param groupname The name of the group that is to be joined for listening.
 * @param ifname    Network interface to join the group on, or NULL if first
 *                  appropriate interface is to be chosen by the O/S.
 *
 * @return       0 on success, -1 on error
 */
int
coap_join_mcast_group_intf(coap_context_t *ctx, const char *groupname,
                           const char *ifname);

#define coap_join_mcast_group(ctx, groupname) \
	    (coap_join_mcast_group_intf(ctx, groupname, NULL))

/**
 * @defgroup app_io Application I/O Handling
 * API functions for Application Input / Output
 * @{
 */

#define COAP_IO_WAIT    0
#define COAP_IO_NO_WAIT ((uint32_t)-1)

/**
 * The main I/O processing function.  All pending network I/O is completed,
 * and then optionally waits for the next input packet.
 *
 * This internally calls coap_io_prepare_io(), then select() for the appropriate
 * sockets, updates COAP_SOCKET_CAN_xxx where appropriate and then calls
 * coap_io_do_io() before returning with the time spent in the function.
 *
 * Alternatively, if libcoap is compiled with epoll support, this internally
 * calls coap_io_prepare_epoll(), then epoll_wait() for waiting for any file
 * descriptors that have (internally) been set up with epoll_ctl() and
 * finally coap_io_do_epoll() before returning with the time spent in the
 * function.
 *
 * @param ctx The CoAP context
 * @param timeout_ms Minimum number of milliseconds to wait for new packets
 *                   before returning after doing any processing.
 *                   If COAP_IO_WAIT, the call will block until the next
 *                   internal action (e.g. packet retransmit) if any, or block
 *                   until the next packet is received whichever is the sooner
 *                   and do the necessary processing.
 *                   If COAP_IO_NO_WAIT, the function will return immediately
 *                   after processing without waiting for any new input
 *                   packets to arrive.
 *
 * @return Number of milliseconds spent in function or @c -1 if there was
 *         an error
 */
int coap_io_process(coap_context_t *ctx, uint32_t timeout_ms);

#ifndef RIOT_VERSION
/**
 * The main message processing loop with additional fds for internal select.
 *
 * @param ctx The CoAP context
 * @param timeout_ms Minimum number of milliseconds to wait for new packets
 *                   before returning after doing any processing.
 *                   If COAP_IO_WAIT, the call will block until the next
 *                   internal action (e.g. packet retransmit) if any, or block
 *                   until the next packet is received whichever is the sooner
 *                   and do the necessary processing.
 *                   If COAP_IO_NO_WAIT, the function will return immediately
 *                   after processing without waiting for any new input
 *                   packets to arrive.
 * @param nfds      The maximum FD set in readfds, writefds or exceptfds
 *                  plus one,
 * @param readfds   Read FDs to additionally check for in internal select()
 *                  or NULL if not required.
 * @param writefds  Write FDs to additionally check for in internal select()
 *                  or NULL if not required.
 * @param exceptfds Except FDs to additionally check for in internal select()
 *                  or NULL if not required.
 *
 *
 * @return Number of milliseconds spent in coap_io_process_with_fds, or @c -1
 *         if there was an error.  If defined, readfds, writefds, exceptfds
 *         are updated as returned by the internal select() call.
 */
int coap_io_process_with_fds(coap_context_t *ctx, uint32_t timeout_ms,
                        int nfds, fd_set *readfds, fd_set *writefds,
                        fd_set *exceptfds);
#endif /* !RIOT_VERSION */

/**@}*/

/**
 * @defgroup app_io_internal Application I/O Handling (Internal)
 * Internal API functions for Application Input / Output
 * @{
 */

/**
* Iterates through all the coap_socket_t structures embedded in endpoints or
* sessions associated with the @p ctx to determine which are wanting any
* read, write, accept or connect I/O (COAP_SOCKET_WANT_xxx is set). If set,
* the coap_socket_t is added to the @p sockets.
*
* Any now timed out delayed packet is transmitted, along with any packets
* associated with requested observable response.
*
* In addition, it returns when the next expected I/O is expected to take place
* (e.g. a packet retransmit).
*
* Prior to calling coap_io_do_io(), the @p sockets must be tested to see
* if any of the COAP_SOCKET_WANT_xxx have the appropriate information and if
* so, COAP_SOCKET_CAN_xxx is set. This typically will be done after using a
* select() call.
*
* Note: If epoll support is compiled into libcoap, coap_io_prepare_epoll() must
* be used instead of coap_io_prepare_io().
*
* Internal function.
*
* @param ctx The CoAP context
* @param sockets Array of socket descriptors, filled on output
* @param max_sockets Size of socket array.
* @param num_sockets Pointer to the number of valid entries in the socket
*                    arrays on output.
* @param now Current time.
*
* @return timeout Maxmimum number of milliseconds that can be used by a
*                 select() to wait for network events or 0 if wait should be
*                 forever.
*/
unsigned int
coap_io_prepare_io(coap_context_t *ctx,
  coap_socket_t *sockets[],
  unsigned int max_sockets,
  unsigned int *num_sockets,
  coap_tick_t now
);

/**
 * Processes any outstanding read, write, accept or connect I/O as indicated
 * in the coap_socket_t structures (COAP_SOCKET_CAN_xxx set) embedded in
 * endpoints or sessions associated with @p ctx.
 *
 * Note: If epoll support is compiled into libcoap, coap_io_do_epoll() must
 * be used instead of coap_io_do_io().
 *
 * Internal function.
 *
 * @param ctx The CoAP context
 * @param now Current time
 */
void coap_io_do_io(coap_context_t *ctx, coap_tick_t now);

/**
 * Any now timed out delayed packet is transmitted, along with any packets
 * associated with requested observable response.
 *
 * In addition, it returns when the next expected I/O is expected to take place
 * (e.g. a packet retransmit).
 *
 * Note: If epoll support is compiled into libcoap, coap_io_prepare_epoll() must
 * be used instead of coap_io_prepare_io().
 *
 * Internal function.
 *
 * @param ctx The CoAP context
 * @param now Current time.
 *
 * @return timeout Maxmimum number of milliseconds that can be used by a
 *                 epoll_wait() to wait for network events or 0 if wait should be
 *                 forever.
 */
unsigned int
coap_io_prepare_epoll(coap_context_t *ctx, coap_tick_t now);

struct epoll_event;

/**
 * Process all the epoll events
 *
 * Note: If epoll support is compiled into libcoap, coap_io_do_epoll() must
 * be used instead of coap_io_do_io().
 *
 * Internal function
 *
 * @param ctx    The current CoAP context.
 * @param events The list of events returned from an epoll_wait() call.
 * @param nevents The number of events.
 *
 */
void coap_io_do_epoll(coap_context_t *ctx, struct epoll_event* events,
                      size_t nevents);

/**@}*/

/**
 * @deprecated Use coap_io_process() instead.
 *
 * This function just calls coap_io_process().
 *
 * @param ctx The CoAP context
 * @param timeout_ms Minimum number of milliseconds to wait for new packets
 *                   before returning after doing any processing.
 *                   If COAP_IO_WAIT, the call will block until the next
 *                   internal action (e.g. packet retransmit) if any, or block
 *                   until the next packet is received whichever is the sooner
 *                   and do the necessary processing.
 *                   If COAP_IO_NO_WAIT, the function will return immediately
 *                   after processing without waiting for any new input
 *                   packets to arrive.
 *
 * @return Number of milliseconds spent in function or @c -1 if there was
 *         an error
 */
COAP_STATIC_INLINE COAP_DEPRECATED int
coap_run_once(coap_context_t *ctx, uint32_t timeout_ms)
{
  return coap_io_process(ctx, timeout_ms);
}

/**
* @deprecated Use coap_io_prepare_io() instead.
*
* This function just calls coap_io_prepare_io().
*
* Internal function.
*
* @param ctx The CoAP context
* @param sockets Array of socket descriptors, filled on output
* @param max_sockets Size of socket array.
* @param num_sockets Pointer to the number of valid entries in the socket
*                    arrays on output.
* @param now Current time.
*
* @return timeout Maxmimum number of milliseconds that can be used by a
*                 select() to wait for network events or 0 if wait should be
*                 forever.
*/
COAP_STATIC_INLINE COAP_DEPRECATED unsigned int
coap_write(coap_context_t *ctx,
  coap_socket_t *sockets[],
  unsigned int max_sockets,
  unsigned int *num_sockets,
  coap_tick_t now
) {
  return coap_io_prepare_io(ctx, sockets, max_sockets, num_sockets, now);
}

/**
 * @deprecated Use coap_io_do_io() instead.
 *
 * This function just calls coap_io_do_io().
 *
 * Internal function.
 *
 * @param ctx The CoAP context
 * @param now Current time
 */
COAP_STATIC_INLINE COAP_DEPRECATED void
coap_read(coap_context_t *ctx, coap_tick_t now
) {
  coap_io_do_io(ctx, now);
}

/* Old definitions which may be hanging around in old code - be helpful! */
#define COAP_RUN_NONBLOCK COAP_RUN_NONBLOCK_deprecated_use_COAP_IO_NO_WAIT
#define COAP_RUN_BLOCK COAP_RUN_BLOCK_deprecated_use_COAP_IO_WAIT

#endif /* COAP_NET_H_ */
