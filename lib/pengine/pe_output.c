/*
 * Copyright 2019 the Pacemaker project contributors
 *
 * The version control history for this file may have further details.
 *
 * This source code is licensed under the GNU Lesser General Public License
 * version 2.1 or later (LGPLv2.1+) WITHOUT ANY WARRANTY.
 */

#include <crm_internal.h>
#include <crm/common/iso8601_internal.h>
#include <crm/pengine/internal.h>

#define s_if_plural(i) (((i) == 1)? "" : "s")

static char *
last_changed_string(const char *last_written, const char *user,
                    const char *client, const char *origin) {
    if (last_written != NULL || user != NULL || client != NULL || origin != NULL) {
        return crm_strdup_printf("%s%s%s%s%s%s%s",
                                 last_written ? last_written : "",
                                 user ? " by " : "",
                                 user ? user : "",
                                 client ? " via " : "",
                                 client ? client : "",
                                 origin ? " on " : "",
                                 origin ? origin : "");
    } else {
        return strdup("");
    }
}

char *
pe__node_display_name(node_t *node, bool print_detail)
{
    char *node_name;
    const char *node_host = NULL;
    const char *node_id = NULL;
    int name_len;

    CRM_ASSERT((node != NULL) && (node->details != NULL) && (node->details->uname != NULL));

    /* Host is displayed only if this is a guest node */
    if (pe__is_guest_node(node)) {
        node_t *host_node = pe__current_node(node->details->remote_rsc);

        if (host_node && host_node->details) {
            node_host = host_node->details->uname;
        }
        if (node_host == NULL) {
            node_host = ""; /* so we at least get "uname@" to indicate guest */
        }
    }

    /* Node ID is displayed if different from uname and detail is requested */
    if (print_detail && safe_str_neq(node->details->uname, node->details->id)) {
        node_id = node->details->id;
    }

    /* Determine name length */
    name_len = strlen(node->details->uname) + 1;
    if (node_host) {
        name_len += strlen(node_host) + 1; /* "@node_host" */
    }
    if (node_id) {
        name_len += strlen(node_id) + 3; /* + " (node_id)" */
    }

    /* Allocate and populate display name */
    node_name = malloc(name_len);
    CRM_ASSERT(node_name != NULL);
    strcpy(node_name, node->details->uname);
    if (node_host) {
        strcat(node_name, "@");
        strcat(node_name, node_host);
    }
    if (node_id) {
        strcat(node_name, " (");
        strcat(node_name, node_id);
        strcat(node_name, ")");
    }
    return node_name;
}

int
pe__name_and_nvpairs_xml(pcmk__output_t *out, bool is_list, const char *tag_name
                         , size_t pairs_count, ...)
{
    xmlNodePtr xml_node = NULL;
    va_list args;

    CRM_ASSERT(tag_name != NULL);

    xml_node = pcmk__output_xml_peek_parent(out);
    CRM_ASSERT(xml_node != NULL);
    xml_node = is_list
        ? create_xml_node(xml_node, tag_name)
        : xmlNewChild(xml_node, NULL, (pcmkXmlStr) tag_name, NULL);

    va_start(args, pairs_count);
    while(pairs_count--) {
        const char *param_name = va_arg(args, const char *);
        const char *param_value = va_arg(args, const char *);
        if (param_name && param_value) {
            xmlSetProp(xml_node, (pcmkXmlStr)param_name, (pcmkXmlStr)param_value);
        }
    };
    va_end(args);

    if (is_list) {
        pcmk__output_xml_push_parent(out, xml_node);
    }
    return 0;
}

int
pe__ban_html(pcmk__output_t *out, va_list args) {
    pe_node_t *pe_node = va_arg(args, pe_node_t *);
    pe__location_t *location = va_arg(args, pe__location_t *);
    gboolean print_clone_detail = va_arg(args, gboolean);

    char *node_name = pe__node_display_name(pe_node, print_clone_detail);
    char *buf = crm_strdup_printf("%s\tprevents %s from running %son %s",
                                  location->id, location->rsc_lh->id,
                                  location->role_filter == RSC_ROLE_MASTER ? "as Master " : "",
                                  node_name);

    pcmk__output_create_html_node(out, "li", NULL, NULL, buf);

    free(node_name);
    free(buf);
    return 0;
}

int
pe__ban_text(pcmk__output_t *out, va_list args) {
    pe_node_t *pe_node = va_arg(args, pe_node_t *);
    pe__location_t *location = va_arg(args, pe__location_t *);
    gboolean print_clone_detail = va_arg(args, gboolean);

    char *node_name = pe__node_display_name(pe_node, print_clone_detail);
    out->list_item(out, NULL, "%s\tprevents %s from running %son %s",
                   location->id, location->rsc_lh->id,
                   location->role_filter == RSC_ROLE_MASTER ? "as Master " : "",
                   node_name);

    free(node_name);
    return 0;
}

int
pe__ban_xml(pcmk__output_t *out, va_list args) {
    xmlNodePtr node = pcmk__output_create_xml_node(out, "ban");
    pe_node_t *pe_node = va_arg(args, pe_node_t *);
    pe__location_t *location = va_arg(args, pe__location_t *);

    char *weight_s = crm_itoa(pe_node->weight);

    xmlSetProp(node, (pcmkXmlStr) "id", (pcmkXmlStr) location->id);
    xmlSetProp(node, (pcmkXmlStr) "resource", (pcmkXmlStr) location->rsc_lh->id);
    xmlSetProp(node, (pcmkXmlStr) "node", (pcmkXmlStr) pe_node->details->uname);
    xmlSetProp(node, (pcmkXmlStr) "weight", (pcmkXmlStr) weight_s);
    xmlSetProp(node, (pcmkXmlStr) "master_only",
               (pcmkXmlStr) (location->role_filter == RSC_ROLE_MASTER ? "true" : "false"));

    free(weight_s);
    return 0;
}

int
pe__cluster_counts_html(pcmk__output_t *out, va_list args) {
    xmlNodePtr nodes_node = pcmk__output_create_xml_node(out, "li");
    xmlNodePtr resources_node = pcmk__output_create_xml_node(out, "li");

    unsigned int nnodes = va_arg(args, unsigned int);
    unsigned int nresources = va_arg(args, unsigned int);
    unsigned int ndisabled = va_arg(args, unsigned int);
    unsigned int nblocked = va_arg(args, unsigned int);

    char *nnodes_str = crm_strdup_printf("%d node%s configured", nnodes, s_if_plural(nnodes));

    pcmk_create_html_node(nodes_node, "span", NULL, NULL, nnodes_str);
    free(nnodes_str);

    if (ndisabled && nblocked) {
        char *s = crm_strdup_printf("%d resource instance%s configured (%d ",
                                    nresources, s_if_plural(nresources),
                                    ndisabled);
        pcmk_create_html_node(resources_node, "span", NULL, NULL, s);
        free(s);

        pcmk_create_html_node(resources_node, "span", NULL, "bold", "DISABLED");

        s = crm_strdup_printf(", %d ", nblocked);
        pcmk_create_html_node(resources_node, "span", NULL, NULL, s);
        free(s);

        pcmk_create_html_node(resources_node, "span", NULL, "bold", "BLOCKED");
        pcmk_create_html_node(resources_node, "span", NULL, NULL,
                              " from further action due to failure)");
    } else if (ndisabled && !nblocked) {
        char *s = crm_strdup_printf("%d resource instance%s configured (%d ",
                                    nresources, s_if_plural(nresources),
                                    ndisabled);
        pcmk_create_html_node(resources_node, "span", NULL, NULL, s);
        free(s);

        pcmk_create_html_node(resources_node, "span", NULL, "bold", "DISABLED");
        pcmk_create_html_node(resources_node, "span", NULL, NULL, ")");
    } else if (!ndisabled && nblocked) {
        char *s = crm_strdup_printf("%d resource instance%s configured (%d ",
                                    nresources, s_if_plural(nresources),
                                    nblocked);
        pcmk_create_html_node(resources_node, "span", NULL, NULL, s);
        free(s);

        pcmk_create_html_node(resources_node, "span", NULL, "bold", "BLOCKED");
        pcmk_create_html_node(resources_node, "span", NULL, NULL,
                              " from further action due to failure)");
    } else {
        char *s = crm_strdup_printf("%d resource instance%s configured",
                                    nresources, s_if_plural(nresources));
        pcmk_create_html_node(resources_node, "span", NULL, NULL, s);
        free(s);
    }

    return 0;
}

int
pe__cluster_counts_text(pcmk__output_t *out, va_list args) {
    unsigned int nnodes = va_arg(args, unsigned int);
    unsigned int nresources = va_arg(args, unsigned int);
    unsigned int ndisabled = va_arg(args, unsigned int);
    unsigned int nblocked = va_arg(args, unsigned int);

    out->list_item(out, NULL, "%d node%s configured", nnodes, s_if_plural(nnodes));

    if (ndisabled && nblocked) {
        out->list_item(out, NULL, "%d resource instance%s configured "
                                  "(%d DISABLED, %d BLOCKED from "
                                  "further action due to failure)",
                       nresources, s_if_plural(nresources), ndisabled,
                       nblocked);
    } else if (ndisabled && !nblocked) {
        out->list_item(out, NULL, "%d resource instance%s configured "
                                  "(%d DISABLED)",
                       nresources, s_if_plural(nresources), ndisabled);
    } else if (!ndisabled && nblocked) {
        out->list_item(out, NULL, "%d resource instance%s configured "
                                  "(%d BLOCKED from further action "
                                  "due to failure)",
                       nresources, s_if_plural(nresources), nblocked);
    } else {
        out->list_item(out, NULL, "%d resource instance%s configured",
                       nresources, s_if_plural(nresources));
    }

    return 0;
}

int
pe__cluster_counts_xml(pcmk__output_t *out, va_list args) {
    xmlNodePtr nodes_node = pcmk__output_create_xml_node(out, "nodes_configured");
    xmlNodePtr resources_node = pcmk__output_create_xml_node(out, "resources_configured");

    unsigned int nnodes = va_arg(args, unsigned int);
    unsigned int nresources = va_arg(args, unsigned int);
    unsigned int ndisabled = va_arg(args, unsigned int);
    unsigned int nblocked = va_arg(args, unsigned int);

    char *s = crm_itoa(nnodes);
    xmlSetProp(nodes_node, (pcmkXmlStr) "number", (pcmkXmlStr) s);
    free(s);

    s = crm_itoa(nresources);
    xmlSetProp(resources_node, (pcmkXmlStr) "number", (pcmkXmlStr) s);
    free(s);

    s = crm_itoa(ndisabled);
    xmlSetProp(resources_node, (pcmkXmlStr) "disabled", (pcmkXmlStr) s);
    free(s);

    s = crm_itoa(nblocked);
    xmlSetProp(resources_node, (pcmkXmlStr) "blocked", (pcmkXmlStr) s);
    free(s);

    return 0;
}

int
pe__cluster_dc_html(pcmk__output_t *out, va_list args) {
    xmlNodePtr node = pcmk__output_create_xml_node(out, "li");

    node_t *dc = va_arg(args, node_t *);
    const char *quorum = va_arg(args, const char *);
    const char *dc_version_s = va_arg(args, const char *);
    const char *dc_name = va_arg(args, const char *);

    pcmk_create_html_node(node, "span", NULL, "bold", "Current DC: ");

    if (dc) {
        if (crm_is_true(quorum)) {
            char *buf = crm_strdup_printf("%s (version %s) - partition with quorum",
                                          dc_name, dc_version_s ? dc_version_s : "unknown");
            pcmk_create_html_node(node, "span", NULL, NULL, buf);
            free(buf);
        } else {
            char *buf = crm_strdup_printf("%s (version %s) - partition",
                                          dc_name, dc_version_s ? dc_version_s : "unknown");
            pcmk_create_html_node(node, "span", NULL, NULL, buf);
            free(buf);

            pcmk_create_html_node(node, "span", NULL, "warning", "WITHOUT");
            pcmk_create_html_node(node, "span", NULL, NULL, "quorum");
        }
    } else {
        pcmk_create_html_node(node ,"span", NULL, "warning", "NONE");
    }

    return 0;
}

int
pe__cluster_dc_text(pcmk__output_t *out, va_list args) {
    node_t *dc = va_arg(args, node_t *);
    const char *quorum = va_arg(args, const char *);
    const char *dc_version_s = va_arg(args, const char *);
    const char *dc_name = va_arg(args, const char *);

    if (dc) {
        out->list_item(out, "Current DC", "%s (version %s) - partition %s quorum",
                       dc_name, dc_version_s ? dc_version_s : "unknown",
                       crm_is_true(quorum) ? "with" : "WITHOUT");
    } else {
        out->list_item(out, "Current DC", "NONE");
    }

    return 0;
}

int
pe__cluster_dc_xml(pcmk__output_t *out, va_list args) {
    xmlNodePtr node = pcmk__output_create_xml_node(out, "current_dc");

    node_t *dc = va_arg(args, node_t *);
    const char *quorum = va_arg(args, const char *);
    const char *dc_version_s = va_arg(args, const char *);

    if (dc) {
        xmlSetProp(node, (pcmkXmlStr) "present", (pcmkXmlStr) "true");
        xmlSetProp(node, (pcmkXmlStr) "version", (pcmkXmlStr) (dc_version_s ? dc_version_s : ""));
        xmlSetProp(node, (pcmkXmlStr) "name", (pcmkXmlStr) dc->details->uname);
        xmlSetProp(node, (pcmkXmlStr) "id", (pcmkXmlStr) dc->details->id);
        xmlSetProp(node, (pcmkXmlStr) "with_quorum", (pcmkXmlStr) (crm_is_true(quorum) ? "true" : "false"));
    } else {
        xmlSetProp(node, (pcmkXmlStr) "present", (pcmkXmlStr) "false");
    }

    return 0;
}

int
pe__cluster_options_html(pcmk__output_t *out, va_list args) {
    pe_working_set_t *data_set = va_arg(args, pe_working_set_t *);

    out->list_item(out, NULL, "STONITH of failed nodes %s",
                   is_set(data_set->flags, pe_flag_stonith_enabled) ? "enabled" : "disabled");

    out->list_item(out, NULL, "Cluster is %s",
                   is_set(data_set->flags, pe_flag_symmetric_cluster) ? "symmetric" : "asymmetric");

    switch (data_set->no_quorum_policy) {
        case no_quorum_freeze:
            out->list_item(out, NULL, "No Quorum policy: Freeze resources");
            break;

        case no_quorum_stop:
            out->list_item(out, NULL, "No Quorum policy: Stop ALL resources");
            break;

        case no_quorum_ignore:
            out->list_item(out, NULL, "No Quorum policy: Ignore");
            break;

        case no_quorum_suicide:
            out->list_item(out, NULL, "No Quorum policy: Suicide");
            break;
    }

    if (is_set(data_set->flags, pe_flag_maintenance_mode)) {
        xmlNodePtr node = pcmk__output_create_xml_node(out, "li");

        pcmk_create_html_node(node, "span", NULL, "bold", "DISABLED");
        pcmk_create_html_node(node, "span", NULL, NULL,
                              " (the cluster will not attempt to start, stop, or recover services)");
    } else {
        out->list_item(out, NULL, "Resource management enabled");
    }

    return 0;
}

int
pe__cluster_options_text(pcmk__output_t *out, va_list args) {
    pe_working_set_t *data_set = va_arg(args, pe_working_set_t *);

    if (is_set(data_set->flags, pe_flag_maintenance_mode)) {
        fprintf(out->dest, "\n              *** Resource management is DISABLED ***");
        fprintf(out->dest, "\n  The cluster will not attempt to start, stop or recover services");
        fprintf(out->dest, "\n");
    }

    return 0;
}

int
pe__cluster_options_xml(pcmk__output_t *out, va_list args) {
    xmlNodePtr node = pcmk__output_create_xml_node(out, "cluster_options");
    pe_working_set_t *data_set = va_arg(args, pe_working_set_t *);

    xmlSetProp(node, (pcmkXmlStr) "stonith-enabled",
               (pcmkXmlStr) (is_set(data_set->flags, pe_flag_stonith_enabled) ? "true" : "false"));
    xmlSetProp(node, (pcmkXmlStr) "symmetric-cluster",
               (pcmkXmlStr) (is_set(data_set->flags, pe_flag_symmetric_cluster) ? "true" : "false"));

    switch (data_set->no_quorum_policy) {
        case no_quorum_freeze:
            xmlSetProp(node, (pcmkXmlStr) "no-quorum-policy", (pcmkXmlStr) "freeze");
            break;

        case no_quorum_stop:
            xmlSetProp(node, (pcmkXmlStr) "no-quorum-policy", (pcmkXmlStr) "stop");
            break;

        case no_quorum_ignore:
            xmlSetProp(node, (pcmkXmlStr) "no-quorum-policy", (pcmkXmlStr) "ignore");
            break;

        case no_quorum_suicide:
            xmlSetProp(node, (pcmkXmlStr) "no-quorum-policy", (pcmkXmlStr) "suicide");
            break;
    }

    xmlSetProp(node, (pcmkXmlStr) "maintenance-mode",
               (pcmkXmlStr) (is_set(data_set->flags, pe_flag_maintenance_mode) ? "true" : "false"));

    return 0;
}

int
pe__cluster_stack_html(pcmk__output_t *out, va_list args) {
    xmlNodePtr node = pcmk__output_create_xml_node(out, "li");
    const char *stack_s = va_arg(args, const char *);

    pcmk_create_html_node(node, "span", NULL, "bold", "Stack: ");
    pcmk_create_html_node(node, "span", NULL, NULL, stack_s);

    return 0;
}

int
pe__cluster_stack_text(pcmk__output_t *out, va_list args) {
    const char *stack_s = va_arg(args, const char *);
    out->list_item(out, "Stack", "%s", stack_s);
    return 0;
}

int
pe__cluster_stack_xml(pcmk__output_t *out, va_list args) {
    xmlNodePtr node = pcmk__output_create_xml_node(out, "stack");
    const char *stack_s = va_arg(args, const char *);

    xmlSetProp(node, (pcmkXmlStr) "type", (pcmkXmlStr) stack_s);

    return 0;
}

int
pe__cluster_times_html(pcmk__output_t *out, va_list args) {
    xmlNodePtr updated_node = pcmk__output_create_xml_node(out, "li");
    xmlNodePtr changed_node = pcmk__output_create_xml_node(out, "li");

    const char *last_written = va_arg(args, const char *);
    const char *user = va_arg(args, const char *);
    const char *client = va_arg(args, const char *);
    const char *origin = va_arg(args, const char *);

    char *buf = last_changed_string(last_written, user, client, origin);

    pcmk_create_html_node(updated_node, "span", NULL, "bold", "Last updated: ");
    pcmk_create_html_node(updated_node, "span", NULL, NULL, crm_now_string(NULL));

    pcmk_create_html_node(changed_node, "span", NULL, "bold", "Last change: ");
    pcmk_create_html_node(changed_node, "span", NULL, NULL, buf);

    free(buf);
    return 0;
}

int
pe__cluster_times_xml(pcmk__output_t *out, va_list args) {
    xmlNodePtr updated_node = pcmk__output_create_xml_node(out, "last_update");
    xmlNodePtr changed_node = pcmk__output_create_xml_node(out, "last_change");

    const char *last_written = va_arg(args, const char *);
    const char *user = va_arg(args, const char *);
    const char *client = va_arg(args, const char *);
    const char *origin = va_arg(args, const char *);

    xmlSetProp(updated_node, (pcmkXmlStr) "time", (pcmkXmlStr) crm_now_string(NULL));

    xmlSetProp(changed_node, (pcmkXmlStr) "time", (pcmkXmlStr) (last_written ? last_written : ""));
    xmlSetProp(changed_node, (pcmkXmlStr) "user", (pcmkXmlStr) (user ? user : ""));
    xmlSetProp(changed_node, (pcmkXmlStr) "client", (pcmkXmlStr) (client ? client : ""));
    xmlSetProp(changed_node, (pcmkXmlStr) "origin", (pcmkXmlStr) (origin ? origin : ""));

    return 0;
}

int
pe__cluster_times_text(pcmk__output_t *out, va_list args) {
    const char *last_written = va_arg(args, const char *);
    const char *user = va_arg(args, const char *);
    const char *client = va_arg(args, const char *);
    const char *origin = va_arg(args, const char *);

    char *buf = last_changed_string(last_written, user, client, origin);

    out->list_item(out, "Last updated", "%s", crm_now_string(NULL));
    out->list_item(out, "Last change", " %s", buf);

    free(buf);
    return 0;
}

int
pe__node_attribute_text(pcmk__output_t *out, va_list args) {
    const char *name = va_arg(args, const char *);
    const char *value = va_arg(args, const char *);
    gboolean add_extra = va_arg(args, gboolean);
    int expected_score = va_arg(args, int);


    if (add_extra) {
        int v = crm_parse_int(value, "0");

        if (v <= 0) {
            out->list_item(out, NULL, "%-32s\t: %-10s\t: Connectivity is lost", name, value);
        } else if (v < expected_score) {
            out->list_item(out, NULL, "%-32s\t: %-10s\t: Connectivity is degraded (Expected=%d)", name, value, expected_score);
        } else {
            out->list_item(out, NULL, "%-32s\t: %-10s", name, value);
        }
    } else {
        out->list_item(out, NULL, "%-32s\t: %-10s", name, value);
    }

    return 0;
}

int
pe__node_attribute_html(pcmk__output_t *out, va_list args) {
    const char *name = va_arg(args, const char *);
    const char *value = va_arg(args, const char *);
    gboolean add_extra = va_arg(args, gboolean);
    int expected_score = va_arg(args, int);

    if (add_extra) {
        int v = crm_parse_int(value, "0");
        char *s = crm_strdup_printf("%s: %s", name, value);
        xmlNodePtr item_node = pcmk__output_create_xml_node(out, "li");

        pcmk_create_html_node(item_node, "span", NULL, NULL, s);
        free(s);

        if (v <= 0) {
            pcmk_create_html_node(item_node, "span", NULL, "bold", "(connectivity is lost)");
        } else if (v < expected_score) {
            char *buf = crm_strdup_printf("(connectivity is degraded -- expected %d", expected_score);
            pcmk_create_html_node(item_node, "span", NULL, "bold", buf);
            free(buf);
        }
    } else {
        out->list_item(out, NULL, "%s: %s", name, value);
    }

    return 0;
}

int
pe__node_attribute_xml(pcmk__output_t *out, va_list args) {
    const char *name = va_arg(args, const char *);
    const char *value = va_arg(args, const char *);
    gboolean add_extra = va_arg(args, gboolean);
    int expected_score = va_arg(args, int);

    xmlNodePtr node = pcmk__output_create_xml_node(out, "attribute");
    xmlSetProp(node, (pcmkXmlStr) "name", (pcmkXmlStr) name);
    xmlSetProp(node, (pcmkXmlStr) "value", (pcmkXmlStr) value);

    if (add_extra) {
        char *buf = crm_itoa(expected_score);
        xmlSetProp(node, (pcmkXmlStr) "expected", (pcmkXmlStr) buf);
        free(buf);
    }

    return 0;
}

static int
pe__ticket_html(pcmk__output_t *out, va_list args) {
    ticket_t *ticket = va_arg(args, ticket_t *);

    if (ticket->last_granted > -1) {
        char *time = pcmk_format_named_time("last-granted", ticket->last_granted);
        out->list_item(out, NULL, "%s:\t%s%s %s", ticket->id,
                       ticket->granted ? "granted" : "revoked",
                       ticket->standby ? " [standby]" : "",
                       time);
        free(time);
    } else {
        out->list_item(out, NULL, "%s:\t%s%s", ticket->id,
                       ticket->granted ? "granted" : "revoked",
                       ticket->standby ? " [standby]" : "");
    }

    return 0;
}

static int
pe__ticket_text(pcmk__output_t *out, va_list args) {
    ticket_t *ticket = va_arg(args, ticket_t *);

    if (ticket->last_granted > -1) {
        char *time = pcmk_format_named_time("last-granted", ticket->last_granted);
        out->list_item(out, ticket->id, "\t%s%s %s",
                       ticket->granted ? "granted" : "revoked",
                       ticket->standby ? " [standby]" : "",
                       time);
        free(time);
    } else {
        out->list_item(out, ticket->id, "\t%s%s",
                       ticket->granted ? "granted" : "revoked",
                       ticket->standby ? " [standby]" : "");
    }

    return 0;
}

static int
pe__ticket_xml(pcmk__output_t *out, va_list args) {
    xmlNodePtr node = NULL;

    ticket_t *ticket = va_arg(args, ticket_t *);

    node = pcmk__output_create_xml_node(out, "ticket");
    xmlSetProp(node, (pcmkXmlStr) "id", (pcmkXmlStr) ticket->id);
    xmlSetProp(node, (pcmkXmlStr) "status", (pcmkXmlStr) (ticket->granted ? "granted" : "revoked"));
    xmlSetProp(node, (pcmkXmlStr) "standby", (pcmkXmlStr) (ticket->standby ? "true" : "false"));

    if (ticket->last_granted > -1) {
        xmlSetProp(node, (pcmkXmlStr) "last-granted",
                   (pcmkXmlStr) crm_now_string(&ticket->last_granted));
    }

    return 0;
}

static pcmk__message_entry_t fmt_functions[] = {
    { "ban", "html", pe__ban_html },
    { "ban", "log", pe__ban_text },
    { "ban", "text", pe__ban_text },
    { "ban", "xml", pe__ban_xml },
    { "bundle", "xml",  pe__bundle_xml },
    { "bundle", "html",  pe__bundle_html },
    { "bundle", "text",  pe__bundle_text },
    { "bundle", "log",  pe__bundle_text },
    { "clone", "xml",  pe__clone_xml },
    { "clone", "html",  pe__clone_html },
    { "clone", "text",  pe__clone_text },
    { "clone", "log",  pe__clone_text },
    { "cluster-counts", "html", pe__cluster_counts_html },
    { "cluster-counts", "log", pe__cluster_counts_text },
    { "cluster-counts", "text", pe__cluster_counts_text },
    { "cluster-counts", "xml", pe__cluster_counts_xml },
    { "cluster-dc", "html", pe__cluster_dc_html },
    { "cluster-dc", "log", pe__cluster_dc_text },
    { "cluster-dc", "text", pe__cluster_dc_text },
    { "cluster-dc", "xml", pe__cluster_dc_xml },
    { "cluster-options", "html", pe__cluster_options_html },
    { "cluster-options", "log", pe__cluster_options_text },
    { "cluster-options", "text", pe__cluster_options_text },
    { "cluster-options", "xml", pe__cluster_options_xml },
    { "cluster-stack", "html", pe__cluster_stack_html },
    { "cluster-stack", "log", pe__cluster_stack_text },
    { "cluster-stack", "text", pe__cluster_stack_text },
    { "cluster-stack", "xml", pe__cluster_stack_xml },
    { "cluster-times", "html", pe__cluster_times_html },
    { "cluster-times", "log", pe__cluster_times_text },
    { "cluster-times", "text", pe__cluster_times_text },
    { "cluster-times", "xml", pe__cluster_times_xml },
    { "group", "xml",  pe__group_xml },
    { "group", "html",  pe__group_html },
    { "group", "text",  pe__group_text },
    { "group", "log",  pe__group_text },
    { "node-attribute", "html", pe__node_attribute_html },
    { "node-attribute", "log", pe__node_attribute_text },
    { "node-attribute", "text", pe__node_attribute_text },
    { "node-attribute", "xml", pe__node_attribute_xml },
    { "primitive", "xml",  pe__resource_xml },
    { "primitive", "html",  pe__resource_html },
    { "primitive", "text",  pe__resource_text },
    { "primitive", "log",  pe__resource_text },
    { "ticket", "html", pe__ticket_html },
    { "ticket", "text", pe__ticket_text },
    { "ticket", "xml", pe__ticket_xml },

    { NULL, NULL, NULL }
};

void
pe__register_messages(pcmk__output_t *out) {
    pcmk__register_messages(out, fmt_functions);
}

void
pe__output_node(node_t *node, gboolean details, pcmk__output_t *out)
{
    if (node == NULL) {
        crm_trace("<NULL>");
        return;
    }

    CRM_ASSERT(node->details);
    crm_trace("%sNode %s: (weight=%d, fixed=%s)",
              node->details->online ? "" : "Unavailable/Unclean ",
              node->details->uname, node->weight, node->fixed ? "True" : "False");

    if (details) {
        char *pe_mutable = strdup("\t\t");
        GListPtr gIter = node->details->running_rsc;

        crm_trace("\t\t===Node Attributes");
        g_hash_table_foreach(node->details->attrs, print_str_str, pe_mutable);
        free(pe_mutable);

        crm_trace("\t\t=== Resources");

        for (; gIter != NULL; gIter = gIter->next) {
            resource_t *rsc = (resource_t *) gIter->data;

            // @TODO pe_print_log probably doesn't belong here
            out->message(out, crm_map_element_name(rsc->xml),
                         pe_print_log|pe_print_pending, rsc);
        }
    }
}
