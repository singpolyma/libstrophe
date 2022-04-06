/* bot.c
** libstrophe XMPP client library -- basic usage example
**
** Copyright (C) 2005-2009 Collecta, Inc.
**
**  This software is provided AS-IS with no warranty, either express
**  or implied.
**
** This program is dual licensed under the MIT and GPLv3 licenses.
*/

/* simple bot example
**
** This example was provided by Matthew Wild <mwild1@gmail.com>.
**
** This bot responds to basic messages and iq version requests.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <strophe.h>

int version_handler(xmpp_conn_t *conn, xmpp_stanza_t *stanza, void *userdata)
{
    xmpp_stanza_t *reply, *query, *name, *version, *text;
    const char *ns;
    xmpp_ctx_t *ctx = (xmpp_ctx_t *)userdata;

    printf("Received version request from %s\n", xmpp_stanza_get_from(stanza));

    reply = xmpp_stanza_reply(stanza);
    xmpp_stanza_set_type(reply, "result");

    query = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(query, "query");
    ns = xmpp_stanza_get_ns(xmpp_stanza_get_children(stanza));
    if (ns) {
        xmpp_stanza_set_ns(query, ns);
    }

    name = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(name, "name");
    xmpp_stanza_add_child(query, name);
    xmpp_stanza_release(name);

    text = xmpp_stanza_new(ctx);
    xmpp_stanza_set_text(text, "libstrophe example bot");
    xmpp_stanza_add_child(name, text);
    xmpp_stanza_release(text);

    version = xmpp_stanza_new(ctx);
    xmpp_stanza_set_name(version, "version");
    xmpp_stanza_add_child(query, version);
    xmpp_stanza_release(version);

    text = xmpp_stanza_new(ctx);
    xmpp_stanza_set_text(text, "1.0");
    xmpp_stanza_add_child(version, text);
    xmpp_stanza_release(text);

    xmpp_stanza_add_child(reply, query);
    xmpp_stanza_release(query);

    xmpp_send(conn, reply);
    xmpp_stanza_release(reply);
    return 1;
}

int message_handler(xmpp_conn_t *conn, xmpp_stanza_t *stanza, void *userdata)
{
    xmpp_ctx_t *ctx = (xmpp_ctx_t *)userdata;
    xmpp_stanza_t *body, *reply;
    const char *type;
    char *intext, *replytext;
    int quit = 0;

    body = xmpp_stanza_get_child_by_name(stanza, "body");
    if (body == NULL)
        return 1;
    type = xmpp_stanza_get_type(stanza);
    if (type != NULL && strcmp(type, "error") == 0)
        return 1;

    intext = xmpp_stanza_get_text(body);

    printf("Incoming message from %s: %s\n", xmpp_stanza_get_from(stanza),
           intext);

    reply = xmpp_stanza_reply(stanza);
    if (xmpp_stanza_get_type(reply) == NULL)
        xmpp_stanza_set_type(reply, "chat");

    if (strcmp(intext, "quit") == 0) {
        replytext = strdup("bye!");
        quit = 1;
    } else {
        replytext = (char *)malloc(strlen(" to you too!") + strlen(intext) + 1);
        strcpy(replytext, intext);
        strcat(replytext, " to you too!");
    }
    xmpp_free(ctx, intext);
    xmpp_message_set_body(reply, replytext);

    xmpp_send(conn, reply);
    xmpp_stanza_release(reply);
    free(replytext);

    if (quit)
        xmpp_disconnect(conn);

    return 1;
}

/* define a handler for connection events */
void conn_handler(xmpp_conn_t *conn,
                  xmpp_conn_event_t status,
                  int error,
                  xmpp_stream_error_t *stream_error,
                  void *userdata)
{
    xmpp_ctx_t *ctx = (xmpp_ctx_t *)userdata;

    (void)error;
    (void)stream_error;

    if (status == XMPP_CONN_CONNECT) {
        xmpp_stanza_t *pres;
        fprintf(stderr, "DEBUG: connected\n");
        xmpp_handler_add(conn, version_handler, "jabber:iq:version", "iq", NULL,
                         ctx);
        xmpp_handler_add(conn, message_handler, NULL, "message", NULL, ctx);

        /* Send initial <presence/> so that we appear online to contacts */
        pres = xmpp_presence_new(ctx);
        xmpp_send(conn, pres);
        xmpp_stanza_release(pres);
    } else {
        fprintf(stderr, "DEBUG: disconnected\n");
        xmpp_stop(ctx);
    }
}

static int
password_callback(char *pw, size_t pw_max, xmpp_conn_t *conn, void *userdata)
{
    (void)userdata;
    printf("Trying to unlock %s\n", xmpp_conn_get_keyfile(conn));
    char *pass = getpass("Please enter password: ");
    if (!pass)
        return -1;
    size_t passlen = strlen(pass);
    int ret;
    if (passlen >= pw_max) {
        ret = -1;
        goto out;
    }
    ret = passlen + 1;
    memcpy(pw, pass, ret);
out:
    memset(pass, 0, passlen);
    return ret;
}

static void usage(int exit_code)
{
    fprintf(stderr,
            "Usage: bot [options] <jid> <pass>\n\n"
            "Options:\n"
            "  --jid <jid>              The JID to use to authenticate.\n"
            "  --pass <pass>            The password of the JID.\n"
            "  --tls-cert <cert>        Path to client certificate.\n"
            "  --tls-key <key>          Path to private key or P12 file.\n\n"
            "  --tcp-keepalive          Configure TCP keepalive.\n"
            "  --disable-tls            Disable TLS.\n"
            "  --mandatory-tls          Deny plaintext connection.\n"
            "  --trust-tls              Trust TLS certificate.\n"
            "  --legacy-ssl             Use old style SSL.\n"
            "  --legacy-auth            Allow legacy authentication.\n"
            "Note: --disable-tls conflicts with --mandatory-tls or "
            "--legacy-ssl\n");

    exit(exit_code);
}

int main(int argc, char **argv)
{
    xmpp_ctx_t *ctx;
    xmpp_conn_t *conn;
    xmpp_log_t *log;
    char *jid = NULL, *password = NULL, *host = NULL, *cert = NULL, *key = NULL;
    long flags = 0;
    int i, tcp_keepalive = 0;
    unsigned long port = 0;

    /* take a jid and password on the command line */
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0)
            usage(0);
        else if (strcmp(argv[i], "--disable-tls") == 0)
            flags |= XMPP_CONN_FLAG_DISABLE_TLS;
        else if (strcmp(argv[i], "--mandatory-tls") == 0)
            flags |= XMPP_CONN_FLAG_MANDATORY_TLS;
        else if (strcmp(argv[i], "--trust-tls") == 0)
            flags |= XMPP_CONN_FLAG_TRUST_TLS;
        else if (strcmp(argv[i], "--legacy-ssl") == 0)
            flags |= XMPP_CONN_FLAG_LEGACY_SSL;
        else if (strcmp(argv[i], "--legacy-auth") == 0)
            flags |= XMPP_CONN_FLAG_LEGACY_AUTH;
        else if ((strcmp(argv[i], "--jid") == 0) && (++i < argc))
            jid = argv[i];
        else if ((strcmp(argv[i], "--pass") == 0) && (++i < argc))
            password = argv[i];
        else if ((strcmp(argv[i], "--tls-cert") == 0) && (++i < argc))
            cert = argv[i];
        else if ((strcmp(argv[i], "--tls-key") == 0) && (++i < argc))
            key = argv[i];
        else if (strcmp(argv[i], "--tcp-keepalive") == 0)
            tcp_keepalive = 1;
        else
            break;
    }
    if ((!jid && !key) || (argc - i) > 2) {
        usage(1);
    }

    if (i < argc)
        host = argv[i];
    if (i + 1 < argc)
        port = strtoul(argv[i + 1], NULL, 0);

    /* init library */
    xmpp_initialize();

    /* pass NULL instead to silence output */
    log = xmpp_get_default_logger(XMPP_LEVEL_DEBUG);
    /* create a context */
    ctx = xmpp_ctx_new(NULL, log);

    /* create a connection */
    conn = xmpp_conn_new(ctx);

    /* configure connection properties (optional) */
    xmpp_conn_set_flags(conn, flags);

    /* ask for a password if key is protected */
    xmpp_conn_set_password_callback(conn, password_callback, NULL);
    /* try at max 3 times in case the user enters the password wrong */
    xmpp_conn_set_password_retries(conn, 3);
    /* setup authentication information */
    if (key)
        xmpp_conn_set_client_cert(conn, cert, key);
    if (jid)
        xmpp_conn_set_jid(conn, jid);
    if (password)
        xmpp_conn_set_pass(conn, password);

    /* enable TCP keepalive, using canned callback function */
    if (tcp_keepalive)
        xmpp_conn_set_sockopt_callback(conn, xmpp_sockopt_cb_keepalive);

    /* initiate connection */
    if (xmpp_connect_client(conn, host, port, conn_handler, ctx) == XMPP_EOK) {

        /* enter the event loop -
           our connect handler will trigger an exit */
        xmpp_run(ctx);
    }

    /* release our connection and context */
    xmpp_conn_release(conn);
    xmpp_ctx_free(ctx);

    /* final shutdown of the library */
    xmpp_shutdown();

    return 0;
}
