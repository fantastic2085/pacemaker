/*
 * Copyright 2015-2020 the Pacemaker project contributors
 *
 * The version control history for this file may have further details.
 *
 * This source code is licensed under the GNU Lesser General Public License
 * version 2.1 or later (LGPLv2.1+) WITHOUT ANY WARRANTY.
 */

#ifndef LRMD_INTERNAL__H
#define LRMD_INTERNAL__H

#include <stdint.h>                     // uint32_t
#include <glib.h>                       // GList, GHashTable, gpointer
#include <libxml/tree.h>                // xmlNode
#include <crm/common/ipc.h>             // crm_ipc_t
#include <crm/common/mainloop.h>        // mainloop_io_t, ipc_client_callbacks
#include <crm/common/remote_internal.h> // pcmk__remote_t
#include <crm/lrmd.h>                   // lrmd_t, lrmd_event_data_t

int lrmd_send_attribute_alert(lrmd_t *lrmd, GList *alert_list,
                              const char *node, uint32_t nodeid,
                              const char *attr_name, const char *attr_value);
int lrmd_send_node_alert(lrmd_t *lrmd, GList *alert_list,
                         const char *node, uint32_t nodeid, const char *state);
int lrmd_send_fencing_alert(lrmd_t *lrmd, GList *alert_list,
                            const char *target, const char *task,
                            const char *desc, int op_rc);
int lrmd_send_resource_alert(lrmd_t *lrmd, GList *alert_list,
                             const char *node, lrmd_event_data_t *op);

int lrmd_tls_send_msg(pcmk__remote_t *session, xmlNode *msg, uint32_t id,
                      const char *msg_type);

/* Shared functions for IPC proxy back end */

typedef struct remote_proxy_s {
    char *node_name;
    char *session_id;

    gboolean is_local;

    crm_ipc_t *ipc;
    mainloop_io_t *source;
    uint32_t last_request_id;
    lrmd_t *lrm;

} remote_proxy_t;

remote_proxy_t *remote_proxy_new(lrmd_t *lrmd,
                                 struct ipc_client_callbacks *proxy_callbacks,
                                 const char *node_name, const char *session_id,
                                 const char *channel);

int  remote_proxy_check(lrmd_t *lrmd, GHashTable *hash);
void remote_proxy_cb(lrmd_t *lrmd, const char *node_name, xmlNode *msg);
void remote_proxy_ack_shutdown(lrmd_t *lrmd);
void remote_proxy_nack_shutdown(lrmd_t *lrmd);

int  remote_proxy_dispatch(const char *buffer, ssize_t length,
                           gpointer userdata);
void remote_proxy_disconnected(gpointer data);
void remote_proxy_free(gpointer data);

void remote_proxy_relay_event(remote_proxy_t *proxy, xmlNode *msg);
void remote_proxy_relay_response(remote_proxy_t *proxy, xmlNode *msg,
                                 int msg_id);

#endif
