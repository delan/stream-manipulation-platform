
#ifndef PLUGIN_H
#define PLUGIN_H

struct plugin
{
    char name[80];
    int version;
};

typedef struct plugin *(*getplugininfo_func)(void);

#endif // !PLUGIN_H
