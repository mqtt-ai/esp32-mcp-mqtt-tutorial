#ifndef MCP_MQTT_H
#define MCP_MQTT_H

#include <stdbool.h>

typedef enum {
    PROPERTY_STRING = 0,
    PROPERTY_REAL,
    PROPERTY_INTEGER,
    PROPERTY_BOOLEAN,
} property_type_e;

typedef union {
    double    real_value;
    long long integer_value;
    char     *string_value;
    bool      boolean_value;
} property_value_u;

typedef struct {
    char *name;
    char *description;

    property_type_e  type;
    property_value_u value;
} property_t;

typedef struct {
    char *name;
    char *description;

    int         property_count;
    property_t *properties;

    const char *(*call)(int n_args, property_t *args);
} mcp_tool_t;

typedef struct {
    char *name;
    char *description;

    int    n_allowed_methods;
    char **allowed_methods;

    int    n_allowed_tools;
    char **allowed_tools;

    int    n_allowed_resources;
    char **allowed_resources;
} mcp_mqtt_role_t;

typedef struct {
    char *uri;
    char *name;
    char *description;
    char *mime_type;
    char *title;
} mcp_resource_t;

#endif
