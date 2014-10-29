
#ifndef PLUGINLOADER_H
#define PLUGINLOADER_H

typedef struct loaded_plugin *loaded_plugin;

#define PLUGINMGR_SUCCESS            0   // success!
#define PLUGINMGR_FILE_NOT_FOUND     1   // specified shared object was not found
#define PLUGINMGR_TMP_FILE_FAILED    2   // failed to create temp file
#define PLUGINMGR_COPY_ERROR         3   // copying library to temp file failed
#define PLUGINMGR_DLOPEN_FAILED      4   // dlopen failed to open library
#define PLUGINMGR_NO_SYMBOL          5   // plugin doesn't export a required symbol
#define PLUGINMGR_PLUGIN_ERROR       6   // generic error with plugin
#define PLUGINMGR_ALREADY_LOADED     7   // the plugin is already loaded, and the update flag wasn't set
#define PLUGINMGR_DOWNGRADE          8   // 'new' plugin version is less than already loaded plugin
#define PLUGINMGR_MAX                9

const char *pluginmgr_strerror(int err);
int plugin_load(char *shared_object, char update, loaded_plugin *out);
void plugin_unload(loaded_plugin plugin);
int plugin_refcount(loaded_plugin plugin, int refcount);

#endif // !PLUGINLOADER_H
