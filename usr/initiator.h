/*
 * iSCSI Initiator
 *
 * Copyright (C) 2004 Dmitry Yusupov, Alex Aizman
 * maintained by open-iscsi@@googlegroups.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * See the file COPYING included with this distribution for more details.
 */

#ifndef INITIATOR_H
#define INITIATOR_H

#include <stdint.h>

#include "types.h"
#include "iscsi_proto.h"
#include "auth.h"
#include "ipc.h"
#include "config.h"
#include "actor.h"
#include "queue.h"

#define CONFIG_FILE		"/etc/iscsid.conf"
#define PID_FILE		"/var/run/iscsid.pid"
#define INITIATOR_NAME_FILE	"/etc/initiatorname.iscsi"
#define DISCOVERY_FILE		"/var/db/iscsi/discovery"
#define NODE_FILE		"/var/db/iscsi/node"

typedef enum cnx_login_status_e {
	CNX_LOGIN_SUCCESS		= 0,
	CNX_LOGIN_FAILED		= 1,
	CNX_LOGIN_IO_ERR		= 2,
	CNX_LOGIN_RETRY			= 3,
	CNX_LOGIN_IMM_RETRY		= 4,
	CNX_LOGIN_IMM_REDIRECT_RETRY	= 5,
} cnx_login_status_e;

enum iscsi_login_status {
	LOGIN_OK			= 0,
	LOGIN_IO_ERROR			= 1,
	LOGIN_FAILED			= 2,
	LOGIN_VERSION_MISMATCH		= 3,
	LOGIN_NEGOTIATION_FAILED	= 4,
	LOGIN_AUTHENTICATION_FAILED	= 5,
	LOGIN_WRONG_PORTAL_GROUP	= 6,
	LOGIN_REDIRECTION_FAILED	= 7,
	LOGIN_INVALID_PDU		= 8,
};

typedef enum iscsi_cnx_state_e {
	STATE_IDLE			= 0,
	STATE_WAIT_CONNECT		= 1,
	STATE_WAIT_PDU_RSP		= 2,
} iscsi_cnx_state_e;

typedef enum iscsi_event_e {
	EV_UNKNOWN			= 0,
	EV_CNX_RECV_PDU			= 1,
	EV_CNX_POLL			= 2,
	EV_CNX_TIMER			= 3,
} iscsi_event_e;

typedef struct iscsi_event {
	queue_item_t item;
	char payload[EVENT_PAYLOAD_MAX];
} iscsi_event_t;

typedef struct iscsi_login_context {
	int cid;
	char *buffer;
	size_t bufsize;
	uint8_t status_class;
	uint8_t status_detail;
	struct iscsi_acl *auth_client;
	iscsi_hdr_t pdu;
	iscsi_login_rsp_t *login_rsp;
	char *data;
	int received_pdu;
	int max_data_length;
	int timeout;
	int final;
	enum iscsi_login_status ret;
	int kernel_io;
} iscsi_login_context_t;

struct iscsi_session;

/* daemon's connection structure */
typedef struct iscsi_conn {
	int id;
	struct iscsi_session *session;
	iscsi_login_context_t login_context;
	uint8_t *rx_buffer;
	iscsi_cnx_state_e state;
	actor_t connect_timer;

	/* login state machine */
	int current_stage;
	int next_stage;
	int partial_response;
	cnx_login_status_e status;

	/* tcp/socket settings */
	int socket_fd;
	struct sockaddr_in addr;
	uint8_t ip_address[16];
	int ip_length;
	int port;
	int tcp_window_size;
	int type_of_service;

	/* timeouts */
	int login_timeout;
	int auth_timeout;
	int active_timeout;
	int idle_timeout;
	int ping_timeout;

	/* sequencing */
	uint32_t exp_statsn;

	/* negotiated parameters */
	int header_digest;
	int data_digest;
	int max_recv_data_segment_len;	/* the value we declare */
	int max_xmit_data_segment_len;	/* the value declared by the target */
} iscsi_conn_t;

typedef struct queue_task {
	iscsi_conn_t *conn;
	union {
		/* iSCSI requests originated via IPC */
		struct ipcreq_login {
			iscsiadm_req_t req;
			iscsiadm_rsp_t rsp;
			int ipc_fd;
		} login;
		struct ipcreq_logout {
			iscsiadm_req_t req;
			iscsiadm_rsp_t rsp;
			int ipc_fd;
		} logout;
		/* iSCSI requests originated via CTL */
		struct ctlreq_async_ev {
		} async_ev;
	} u;
} queue_task_t;

/* daemon's session structure */
typedef struct iscsi_session {
	node_rec_t nrec; /* copy of original Node record in database */
	int vendor_specific_keys;
	unsigned int irrelevant_keys_bitmap;
	int send_async_text;
	uint32_t itt;
	uint32_t cmdsn;
	uint32_t exp_cmdsn;
	uint32_t max_cmdsn;
	int immediate_data;
	int initial_r2t;
	int first_burst_len;
	int max_burst_len;
	int data_pdu_in_order;
	int data_seq_in_order;
	int def_time2wait;
	int def_time2retain;
	int type;
	int portal_group_tag;
	uint8_t isid[6];
	uint16_t tsih;
	int channel;
	int target_id;
	char target_name[TARGET_NAME_MAXLEN + 1];
	char *target_alias;
	char *initiator_name;
	char *initiator_alias;
	struct auth_str_block auth_recv_string_block;
	struct auth_str_block auth_send_string_block;
	struct auth_large_binary auth_recv_binary_block;
	struct auth_large_binary auth_send_binary_block;
	struct iscsi_acl auth_client_block;
	struct iscsi_acl *auth_client;
	int num_auth_buffers;
	struct auth_buffer_desc auth_buffers[5];
	int bidirectional_auth;
	char username[AUTH_STR_MAX_LEN];
	uint8_t password[AUTH_STR_MAX_LEN];
	int password_length;
	char username_in[AUTH_STR_MAX_LEN];
	uint8_t password_in[AUTH_STR_MAX_LEN];
	int password_length_in;
	iscsi_conn_t cnx[ISCSI_CNX_MAX];

	/* session's processing */
	actor_t mainloop;
	queue_t *queue;
} iscsi_session_t;

#define PROVIDER_MAX		16
#define PROVIDER_NAME_MAXLEN	64

typedef enum iscsi_provider_e {
	PROVIDER_UNKNOWN		= 0,
	PROVIDER_SOFT_TCP		= 1,
	PROVIDER_SOFT_ISER		= 2,
	PROVIDER_ISER			= 3,
	PROVIDER_ACCEL_ISCSI		= 4,
} iscsi_provider_e;

typedef enum iscsi_provider_status_e {
	PROVIDER_STATUS_UNKNOWN		= 0,
	PROVIDER_STATUS_OPERATIONAL	= 1,
	PROVIDER_STATUS_FAILED		= 2,
} iscsi_provider_status_e;

/* represents data path provider */
typedef struct iscsi_provider_t {
	iscsi_provider_e type;
	iscsi_provider_status_e status;
	char name[PROVIDER_NAME_MAXLEN];
	struct qelem sessions;
} iscsi_provider_t;

/* iscsid.c */
extern iscsi_provider_t provider[PROVIDER_MAX];

/* login.c */

#define ISCSI_SESSION_TYPE_NORMAL 0
#define ISCSI_SESSION_TYPE_DISCOVERY 1

/* not defined by iSCSI, but used in the login code to determine
 * when to send the initial Login PDU
 */
#define ISCSI_INITIAL_LOGIN_STAGE -1

#define ISCSI_TEXT_SEPARATOR     '='

/* implemented in iscsi-login.c for use on all platforms */
extern int iscsi_add_text(iscsi_hdr_t *hdr, char *data, int max_data_length,
			char *param, char *value);
extern enum iscsi_login_status iscsi_login(iscsi_session_t *session, int cid,
		   char *buffer, uint32_t bufsize, uint8_t * status_class,
		   uint8_t * status_detail);
extern int iscsi_update_address(iscsi_conn_t *conn, char *address);
extern int iscsi_login_begin(iscsi_session_t *session,
			     iscsi_login_context_t *c);
extern int iscsi_login_req(iscsi_session_t *session, iscsi_login_context_t *c);
extern int iscsi_login_rsp(iscsi_session_t *session, iscsi_login_context_t *c);

/* Digest types */
#define ISCSI_DIGEST_NONE  0
#define ISCSI_DIGEST_CRC32C 1
#define ISCSI_DIGEST_CRC32C_NONE 2	/* offer both, prefer CRC32C */
#define ISCSI_DIGEST_NONE_CRC32C 3	/* offer both, prefer None */

#define IRRELEVANT_MAXCONNECTIONS	0x01
#define IRRELEVANT_INITIALR2T		0x02
#define IRRELEVANT_IMMEDIATEDATA	0x04
#define IRRELEVANT_MAXBURSTLENGTH	0x08
#define IRRELEVANT_FIRSTBURSTLENGTH	0x10
#define IRRELEVANT_MAXOUTSTANDINGR2T	0x20
#define IRRELEVANT_DATAPDUINORDER	0x40
#define IRRELEVANT_DATASEQUENCEINORDER	0x80

/* io.c */
extern int iscsi_tcp_poll(iscsi_conn_t *conn);
extern int iscsi_tcp_connect(iscsi_conn_t *conn, int non_blocking);
extern int iscsi_connect(iscsi_conn_t *conn);
extern void iscsi_disconnect(iscsi_conn_t *conn);
extern int iscsi_send_pdu(iscsi_conn_t *conn, iscsi_hdr_t *hdr,
	       int hdr_digest, char *data, int data_digest, int timeout);
extern int iscsi_recv_pdu(iscsi_conn_t *conn, iscsi_hdr_t *hdr,
	int hdr_digest, char *data, int max_data_length, int data_digest,
	int timeout);

/* initiator.c */
extern iscsi_session_t* session_create(node_rec_t *rec);
extern void session_destroy(iscsi_session_t *session);
extern int session_cnx_create(iscsi_session_t *session, int cid);
extern void session_cnx_destroy(iscsi_session_t *session, int cid);

#endif /* INITIATOR_H */