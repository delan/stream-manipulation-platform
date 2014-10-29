
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include "list.h"

#include "plugin.h"
#include "pluginloader.h"

struct loaded_plugin
{
    struct list_head list;
    struct plugin plugin;
    void *library;
    int refcount;
    int pending_unload;
};

static LIST_HEAD(plugin_list);

static const char *error_strings[] = {
    "Success",
    "File Not Found",
    "Temp file creation failed",
    "Error copying plugin",
    "dlopen failed",
    "Symbol is not exported",
    "Generic plugin error",
    "Plugin is already loaded",
    "Specified plugin upgrade version is lower than already loaded plugin",
};

const char *pluginmgr_strerror(int err)
{
    if(err >= PLUGINMGR_MAX || err < 0)
        return NULL;
    return error_strings[err];
}

/* we don't load directly from the shared object
 * this allows us to dynamically update */
int plugin_load(char *shared_object, char update, loaded_plugin *pl)
{
    /* check file exists */
    struct stat statbuf;
    if(stat(shared_object, &statbuf) != 0)
    {
        return PLUGINMGR_FILE_NOT_FOUND;
    }
    
    char template[256];
    snprintf(template, sizeof(template), "%s/%s.XXXXXX", P_tmpdir, shared_object);
    int fd = mkstemp(template);
    if(!fd)
    {
        return PLUGINMGR_TMP_FILE_FAILED;
    }
    printf("tmp: %s\n", template);

    FILE *new = fdopen(fd, "w");
    FILE *orig = fopen(shared_object, "r");

    char buffer[4096];
    while(!feof(orig))
    {
        size_t in = fread(buffer, 1, sizeof(buffer), orig);
        size_t out = fwrite(buffer, 1, in, new);
        if(out != in)
        {
            /* clean up time */
            int saved_errno = errno;
            fclose(new);
            fclose(orig);
            unlink(template);
            errno = saved_errno;
            return PLUGINMGR_COPY_ERROR;
        }
    }

    fclose(new);
    fclose(orig);

    /* shared object plugin has now been copied */
    void *library = dlopen(template, RTLD_LAZY);

    /* no need to keep this on the filesystem */
    unlink(template);

    if(!library)
    {
        return PLUGINMGR_DLOPEN_FAILED;
    }

    getplugininfo_func getplugininfo = dlsym(library, "getplugininfo");
    if(!getplugininfo)
    {
        return PLUGINMGR_NO_SYMBOL;
    }

    struct plugin *plugin = getplugininfo();

    /* not sure why they would give us NULL here .. */
    if(!plugin)
    {
        dlclose(library);
        return PLUGINMGR_PLUGIN_ERROR;
    }

    /* determine if this plugin is already loaded */
    char equals(struct loaded_plugin *lp, struct plugin *p)
    {
        return strncmp(lp->plugin.name, p->name, sizeof(p->name)) == 0 && lp->pending_unload == 0;
    }

    struct loaded_plugin *loaded_plugin;
    loaded_plugin = list_find(struct loaded_plugin, list, &plugin_list, &equals, plugin);
    if(loaded_plugin)
    {
        /* this plugin has already been loaded */
        if(!update)
        {
            dlclose(library);
            return PLUGINMGR_ALREADY_LOADED;
        }
        /* make sure we're going up a version */
        if(loaded_plugin->plugin.version >= plugin->version)
        {
            dlclose(library);
            return PLUGINMGR_DOWNGRADE;
        }
        /* mark this plugin has pending unload */
        loaded_plugin->pending_unload = 1;
    }

    loaded_plugin = (struct loaded_plugin*)malloc(sizeof(struct loaded_plugin));
    memcpy(&loaded_plugin->plugin, plugin, sizeof(*plugin));
    loaded_plugin->library = library;
    loaded_plugin->refcount = 0;

    list_add(&loaded_plugin->list, &plugin_list);

    *pl = loaded_plugin;
    return PLUGINMGR_SUCCESS;
}

static void plugin_unload_commit(loaded_plugin p)
{
    dlclose(p->library);
    list_del(&p->list);
    free(p);
}

void plugin_unload(loaded_plugin p)
{
    p->pending_unload = 1;
    if(!p->refcount)
        plugin_unload_commit(p);
}

int plugin_refcount(loaded_plugin p, int refcount)
{
    p->refcount += refcount;

    /* this is bad */
    if(p->refcount < 0)
    {
        p->refcount = 0;
    }
    if(p->refcount == 0 && p->pending_unload)
    {
        plugin_unload_commit(p);
        return 0;
    }
    return p->refcount;
}

