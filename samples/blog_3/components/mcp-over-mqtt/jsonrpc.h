#ifndef MCP_JSONRPC_H
#define MCP_JSONRPC_H

#include <stdbool.h>

#include "mcp.h"

typedef struct jsonrpc        jsonrpc_t;
typedef struct jsonrpc_id     jsonrpc_id_t;
typedef struct jsonrpc_result jsonrpc_result_t;

char      *jsonrpc_encode(jsonrpc_t *jsonrpc);
jsonrpc_t *jsonrpc_decode(const char *json_str);

void jsonrpc_decode_free(jsonrpc_t *jsonrpc);

char               *jsonrpc_get_method(const jsonrpc_t *jsonrpc);
const jsonrpc_id_t *jsonrpc_get_id(const jsonrpc_t *jsonrpc);
bool                jsonrpc_id_exists(const jsonrpc_id_t *id);

int jsonrpc_tool_call_decode(const jsonrpc_t *jsonrpc, char **function_name,
                             int *n_args, property_t **args);
int jsonrpc_resource_read_decode(const jsonrpc_t *jsonrpc, char **uri);

jsonrpc_t *jsonrpc_server_online(const char *server_name,
                                 const char *description, int n_roles,
                                 mcp_mqtt_role_t *roles);
jsonrpc_t *jsonrpc_error_response(const jsonrpc_id_t *id, int code,
                                  const char *message);
jsonrpc_t *jsonrpc_init_response(const jsonrpc_id_t *id, bool tools,
                                 bool resources);
jsonrpc_t *jsonrpc_tool_list_response(const jsonrpc_id_t *id, int n_tools,
                                      mcp_tool_t *tools);
jsonrpc_t *jsonrpc_tool_call_response(const jsonrpc_id_t *id,
                                      const char         *result);

jsonrpc_t *jsonrpc_resource_list_response(const jsonrpc_id_t *id,
                                          int                 n_resources,
                                          mcp_resource_t     *resources);
jsonrpc_t *jsonrpc_resource_read_text_response(const jsonrpc_id_t *id,
                                               mcp_resource_t     *resource,
                                               const char         *content);

#endif