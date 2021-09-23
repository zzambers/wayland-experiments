#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <wayland-client.h>
#include <unistd.h>
#include <sys/types.h>

typedef struct {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shell *shell;
    struct wl_shm *shm;
    struct wl_surface *surface;
    struct wl_shell_surface *shell_surface;
    struct wl_shm_pool *pool;
    struct wl_buffer *buffer;
    struct wl_callback *frame_callback;
    uint32_t *pool_mem;
    int width, height;
    int fd;
    int cntr;
} prog_data;

static void global_registry_handler(void *data, struct wl_registry *registry, uint32_t id, const char *interface, uint32_t version) {
    prog_data *pdata = (prog_data *) data;
    fprintf(stdout, "global_registry_handler: %s %d\n", interface, id);
    if (strcmp(interface, "wl_compositor") == 0) {
        pdata->compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 1);
    } else if (strcmp(interface, "wl_shell") == 0) {
        pdata->shell = wl_registry_bind(registry, id, &wl_shell_interface, 1);
    } else if (strcmp(interface, "wl_shm") == 0) {
        pdata->shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
    }
}

static void global_registry_remover(void *data, struct wl_registry *registry, uint32_t id) {
    fprintf(stdout, "global_registry_remover: %d\n", id);
}

static const struct wl_registry_listener registry_listener = {
    global_registry_handler,
    global_registry_remover
};

int createBuffers(prog_data *pdata) {
    size_t bufsize = pdata->width * pdata->height * 4;
    pdata->fd = shm_open("/shm-experiment", O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    if (pdata->fd < 0) {
        fprintf(stderr, "%s\n",  "Failed to open shm");
        return -1;
    }
    shm_unlink("/shm-experiment");
    if (ftruncate(pdata->fd, bufsize * 2) < 0) {
        fprintf(stderr, "%s\n",  "Failed to truncate");
        return -1;
    }
    pdata->pool_mem = mmap(NULL, bufsize * 2, PROT_READ | PROT_WRITE, MAP_SHARED, pdata->fd, 0);
    if (pdata->pool_mem == NULL) {
        fprintf(stderr, "%s\n",  "Failed mmap file");
        return -1;
    }
    pdata->pool = wl_shm_create_pool(pdata->shm, pdata->fd, bufsize);
    if (pdata->pool == NULL) {
        fprintf(stderr, "%s\n",  "Failed create pool");
        return -1;
    }
    pdata->buffer = wl_shm_pool_create_buffer(pdata->pool, 0, pdata->width, pdata->height, pdata->width * 4, WL_SHM_FORMAT_XRGB8888);
    if (pdata->buffer == NULL) {
        fprintf(stderr, "%s\n",  "Failed create buffer");
        return -1;
    }
    return 0;
}

static void
handle_ping(void *data, struct wl_shell_surface *shell_surface,
                            uint32_t serial)
{
    wl_shell_surface_pong(shell_surface, serial);
    //fprintf(stderr, "ping\n");
}

static void
handle_configure(void *data, struct wl_shell_surface *shell_surface,
         uint32_t edges, int32_t width, int32_t height)
{
}

static void
handle_popup_done(void *data, struct wl_shell_surface *shell_surface)
{
}

static const struct wl_shell_surface_listener shell_surface_listener = {
    handle_ping,
    handle_configure,
    handle_popup_done
};


static const struct wl_callback_listener frame_listener;

void redraw(void *data, struct wl_callback *callback, uint32_t time) {
    prog_data *pdata = data;
    uint32_t *pixels = pdata->pool_mem;
    int x, y;
    int radius = pdata->cntr % pdata->width;
    if (pdata->frame_callback) {
        wl_callback_destroy(pdata->frame_callback);
    }
    pdata->frame_callback = wl_surface_frame(pdata->surface);
    wl_callback_add_listener(pdata->frame_callback, &frame_listener, pdata);

    memset(pixels, 0, pdata->width * pdata->height * 4);
    for (y = 0; y < pdata->height; ++y) {
        for (x = 0; x < pdata->width; ++x) {
            if (x*x + y*y < radius * radius) {
                pixels[y * pdata->width + x] = 0x00FFFF00;
            } else  {
                pixels[y * pdata->width + x] = 0x000000FF;
            }
        }
    }
    //fprintf(stderr, "Redrawn\n");
    wl_surface_attach(pdata->surface, pdata->buffer, 0, 0);
    wl_surface_damage(pdata->surface, 0, 0, pdata->width, pdata->height);
    wl_surface_commit(pdata->surface);
    ++pdata->cntr;
}

static const struct wl_callback_listener frame_listener = {
    redraw
};


int progMain(prog_data *pdata) {
    pdata->display = wl_display_connect(NULL);
    if (pdata->display == NULL) {
        fprintf(stderr, "%s\n",  "Failed to connect");
        return -1;
    }
    pdata->registry = wl_display_get_registry(pdata->display);
    wl_registry_add_listener(pdata->registry, &registry_listener, pdata);
    wl_display_roundtrip(pdata->display);
    if (pdata->compositor == NULL) {
        fprintf(stderr, "%s\n",  "Failed to get compositor");
        return -1;
    }
    if (pdata->shell == NULL) {
        fprintf(stderr, "%s\n",  "Failed to get shell");
        return -1;
    }
    if (pdata->shm == NULL) {
        fprintf(stderr, "%s\n",  "Failed to get shm");
        return -1;
    }
    pdata->surface = wl_compositor_create_surface(pdata->compositor);
    if (pdata->surface == NULL) {
        fprintf(stderr, "%s\n",  "Failed to create surface");
        return -1;
    }
    pdata->shell_surface = wl_shell_get_shell_surface(pdata->shell, pdata->surface);
    if (pdata->shell_surface == NULL) {
        fprintf(stderr, "%s\n",  "Failed to get shell surface");
        return -1;
    }
    wl_shell_surface_set_toplevel(pdata->shell_surface);
    wl_shell_surface_add_listener(pdata->shell_surface,
                  &shell_surface_listener, NULL);
    pdata->width=512;
    pdata->height=512;
    if (createBuffers(pdata) < 0) {
        return -1;
    }
    redraw(pdata, NULL, 0);
    while (wl_display_dispatch(pdata->display) != -1) {}
    return 0;
}

int main(int argc, char **argv) {
    prog_data data = {0};
    int ret = progMain(&data);
    wl_display_disconnect(data.display);
    return ret;
}
