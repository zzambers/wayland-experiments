#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <glib.h>
#include <gio/gio.h>

typedef struct {
    GDBusConnection *bus;
    gchar *sender;
    gchar *token;
    gchar *request_path;
    guint signal_id;
    int subscribed;
    GMainLoop *loop;
} portal_data;


#define PORTAL_BUS_NAME "org.freedesktop.portal.Desktop"
#define PORTAL_OBJECT_PATH  "/org/freedesktop/portal/desktop"
#define REQUEST_PATH_PREFIX "/org/freedesktop/portal/desktop/request/"
#define SESSION_PATH_PREFIX "/org/freedesktop/portal/desktop/session/"
#define REQUEST_INTERFACE "org.freedesktop.portal.Request"
#define SESSION_INTERFACE "org.freedesktop.portal.Session"


static int portalInit(portal_data *data) {
    const gchar *name = NULL;
    gchar *sender = NULL;

    data->bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, NULL);
    if (!data->bus) {
        fprintf(stderr, "%s\n", "Failed to get BUS");
        return -1;
    }
    /*
    Prepared sender and token variables, see:
    https://flatpak.github.io/xdg-desktop-portal/portal-docs.html#gdbus-org.freedesktop.portal.Request
    */
    name = g_dbus_connection_get_unique_name(data->bus);
    if (!name) {
        fprintf(stderr, "%s\n", "Failed to get connection name");
        return -1;
    }
    if (*name == ':') {
        ++name;
    }
    data->sender = g_strdup(name);
    if (!data->sender) {
        fprintf(stderr, "%s\n", "Failed to alloc sender");
        return -1;
    }
    for (sender = data->sender; *sender != 0; ++sender) {
        if (*sender == '.') {
            *sender = '_';
        }
    }
    data->token = g_strdup("awt_portal_1");
    if (!data->token) {
        fprintf(stderr, "%s\n", "Failed to alloc token");
        return -1;
    }
    data->request_path = g_strconcat(REQUEST_PATH_PREFIX, data->sender, "/", data->token, NULL);
    return 0;
}

void portalCleanup(portal_data *data) {
    if (data->loop) {
        g_main_loop_unref(data->loop);
        data->loop = NULL;
    }
    if (data->subscribed) {
        g_dbus_connection_signal_unsubscribe(data->bus, data->signal_id);
        data->subscribed = 0;
    }
    g_free(data->request_path);
    data->request_path = NULL;
    g_free(data->token);
    data->token = NULL;
    g_free(data->sender);
    data->sender = NULL;
    g_clear_object(&data->bus);
}

static void response_screenshot(GDBusConnection *bus,
                   const char *sender_name,
                   const char *object_path,
                   const char *interface_name,
                   const char *signal_name,
                   GVariant *parameters,
                   gpointer gdata)
{
    portal_data *data = gdata;
    guint32 retcode = 0;
    GVariant *ret = NULL;
    const char *uri = NULL;

    g_variant_get (parameters, "(u@a{sv})", &retcode, &ret);
    fprintf(stderr, "screenshot retcode: %d\n", retcode);
    if (retcode == 0) {
        g_variant_lookup (ret, "uri", "&s", &uri);
        fprintf(stderr, "screenshot uri: %s\n", uri);
    }
    g_variant_unref(ret);
    g_main_loop_quit(data->loop);
}

static int takeScreenshot(portal_data *data) {
    GVariant *returned = NULL;
    GVariantBuilder *options = NULL;
    gchar *url = NULL;
    data->signal_id = g_dbus_connection_signal_subscribe(
        data->bus,
        PORTAL_BUS_NAME,
        REQUEST_INTERFACE,
        "Response",
        data->request_path,
        NULL,
        G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
        response_screenshot,
        data,
        NULL);
    data->subscribed = 1;
    options = g_variant_builder_new(G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(options, "{sv}", "handle_token",  g_variant_new_string(data->token));
    g_variant_builder_add(options, "{sv}", "interactive", g_variant_new_boolean(0));
    g_variant_builder_add(options, "{sv}", "modal", g_variant_new_boolean(0));
    returned = g_dbus_connection_call_sync(
        data->bus,
        PORTAL_BUS_NAME,
        PORTAL_OBJECT_PATH,
        "org.freedesktop.portal.Screenshot",
        "Screenshot",
        g_variant_new("(sa{sv})", "", options),
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        NULL);
    g_variant_builder_unref(options);
    if (!returned) {
        fprintf(stderr, "%s\n", "Screenshot call failed");
        return -1;
    }
    g_variant_get(returned, "(o)", &url);
    if (strcmp(url, data->request_path) != 0) {
        fprintf(stderr, "Reqest path is different than expected: (%s != %s)", url, data->request_path);
        return -1;
    }
    g_variant_unref(returned);
    return 0;
}

static int portalMain(portal_data *data) {
    if (portalInit(data)) {
        return -1;
    }
    if (takeScreenshot(data)) {
        return -1;
    }
    /*
    We need main loop here to receive signal (with screenshot uri), see:
    https://www.freedesktop.org/software/gstreamer-sdk/data/docs/latest/gio/GDBusConnection.html#g-dbus-connection-signal-subscribe
    https://stackoverflow.com/questions/23737750/glib-usage-without-mainloop
    */
    data->loop = g_main_loop_new(NULL, 0);
    if (!data->loop) {
        fprintf(stderr, "%s\n", "Failed to create main loop");
        return -1;
    }
    /*
    while(1) {
        fprintf(stderr, "%s\n", "iteration pre");
        g_main_context_iteration(g_main_loop_get_context(data->loop), FALSE);
        fprintf(stderr, "%s\n", "iteration post");
    }
    */
    g_main_loop_run(data->loop);
    return 0;
}

int main(int argc, char **argv) {
    portal_data data = {0};
    int ret = portalMain(&data);
    portalCleanup(&data);
    return ret;
}
