#ifndef MCP_SERVER_MQTT_H
#define MCP_SERVER_MQTT_H

#include "mcp.h"

typedef struct mcp_server mcp_server_t;

mcp_server_t *mcp_server_init(const char *name, const char *description,
                              const char *broker_uri, const char *client_id,
                              const char *user, const char *password,
                              const char *cert);
void          mcp_server_close(mcp_server_t *server);

int mcp_server_register_tool(mcp_server_t *server, int n_tools,
                             mcp_tool_t *tools);

typedef const char *(*mcp_resource_read)(const char *uri);
int mcp_server_register_resources(mcp_server_t *server, int n_resources,
                                  mcp_resource_t   *resources,
                                  mcp_resource_read read_callback);

int mcp_server_run(mcp_server_t *server);

#endif