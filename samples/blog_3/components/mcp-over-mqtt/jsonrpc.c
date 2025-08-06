#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"

#include "jsonrpc.h"

struct jsonrpc_id {
    enum {
        JSONRPC_ID_NONE   = -1,
        JSONRPC_ID_INT    = 0,
        JSONRPC_ID_STRING = 1,
    } id_type;

    union {
        int64_t i;
        char   *s;
    } id;
};

struct jsonrpc_result {
    enum {
        JSONRPC_RESULT_NONE   = -1,
        JSONRPC_RESULT_RESULT = 0,
        JSONRPC_RESULT_ERROR  = 1,
    } result_type;

    union {
        struct {
            int64_t code;
            char   *message;
            char   *data;
        } error;
        cJSON *obj;
    } resp;
};

struct jsonrpc {
    jsonrpc_id_t id;

    char  *method;
    cJSON *params;

    jsonrpc_result_t result;
};

char *jsonrpc_encode(jsonrpc_t *jsonrpc)
{
    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    if (jsonrpc->method) {
        cJSON_AddStringToObject(root, "method", jsonrpc->method);
    }
    if (jsonrpc->params) {
        cJSON_AddItemToObject(root, "params", jsonrpc->params);
    }
    if (jsonrpc->id.id_type == JSONRPC_ID_INT) {
        cJSON_AddNumberToObject(root, "id", jsonrpc->id.id.i);
    } else if (jsonrpc->id.id_type == JSONRPC_ID_STRING) {
        cJSON_AddStringToObject(root, "id", jsonrpc->id.id.s);
    }
    if (jsonrpc->result.result_type == JSONRPC_RESULT_RESULT) {
        cJSON_AddItemToObject(root, "result", jsonrpc->result.resp.obj);
    } else if (jsonrpc->result.result_type == JSONRPC_RESULT_ERROR) {
        cJSON *error = cJSON_CreateObject();
        cJSON_AddNumberToObject(error, "code", jsonrpc->result.resp.error.code);
        if (jsonrpc->result.resp.error.message) {
            cJSON_AddStringToObject(error, "message",
                                    jsonrpc->result.resp.error.message);
        }
        if (jsonrpc->result.resp.error.data) {
            cJSON_AddStringToObject(error, "data",
                                    jsonrpc->result.resp.error.data);
        }
        cJSON_AddItemToObject(root, "error", error);
    }

    char *result = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    free(jsonrpc);

    return result;
}

jsonrpc_t *jsonrpc_decode(const char *json_str)
{
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        return NULL;
    }

    jsonrpc_t *jsonrpc = calloc(1, sizeof(jsonrpc_t));

    cJSON *jsonrpc_version = cJSON_GetObjectItem(root, "jsonrpc");
    if (!cJSON_IsString(jsonrpc_version) ||
        strcmp(jsonrpc_version->valuestring, "2.0") != 0) {
        cJSON_Delete(root);
        free(jsonrpc);
        return NULL;
    }

    cJSON *id = cJSON_GetObjectItem(root, "id");
    if (cJSON_IsNumber(id)) {
        jsonrpc->id.id_type = JSONRPC_ID_INT;
        jsonrpc->id.id.i    = id->valueint;
    } else if (cJSON_IsString(id)) {
        jsonrpc->id.id_type = JSONRPC_ID_STRING;
        jsonrpc->id.id.s    = strdup(id->valuestring);
    } else {
        jsonrpc->id.id_type = JSONRPC_ID_NONE;
    }

    cJSON *method = cJSON_GetObjectItem(root, "method");
    if (cJSON_IsString(method)) {
        jsonrpc->method = strdup(method->valuestring);
    }

    cJSON *params = cJSON_GetObjectItem(root, "params");
    if (params) {
        jsonrpc->params = params; // ownership is transferred to jsonrpc
    }

    cJSON *result = cJSON_GetObjectItem(root, "result");
    if (result) {
        jsonrpc->result.result_type = JSONRPC_RESULT_RESULT;
        jsonrpc->result.resp.obj =
            result; // ownership is transferred to jsonrpc
    } else {
        cJSON *error = cJSON_GetObjectItem(root, "error");
        if (error) {
            jsonrpc->result.result_type = JSONRPC_RESULT_ERROR;
            jsonrpc->result.resp.error.code =
                cJSON_GetObjectItem(error, "code")->valueint;
            cJSON *message = cJSON_GetObjectItem(error, "message");
            if (message && cJSON_IsString(message)) {
                jsonrpc->result.resp.error.message =
                    strdup(message->valuestring);
            }
            cJSON *data = cJSON_GetObjectItem(error, "data");
            if (data && cJSON_IsString(data)) {
                jsonrpc->result.resp.error.data = strdup(data->valuestring);
            }
        }
    }

    // cJSON_Delete(root);
    return jsonrpc;
}

void jsonrpc_decode_free(jsonrpc_t *jsonrpc)
{
    if (jsonrpc == NULL) {
        return;
    }

    if (jsonrpc->id.id_type == JSONRPC_ID_STRING && jsonrpc->id.id.s) {
        free(jsonrpc->id.id.s);
    }

    if (jsonrpc->method) {
        free(jsonrpc->method);
    }

    // if (jsonrpc->params) {
    // cJSON_Delete(jsonrpc->params);
    //}

    if (jsonrpc->result.result_type == JSONRPC_RESULT_ERROR) {
        free(jsonrpc->result.resp.error.message);
        free(jsonrpc->result.resp.error.data);
    } else if (jsonrpc->result.result_type == JSONRPC_RESULT_RESULT &&
               jsonrpc->result.resp.obj) {
        //   cJSON_Delete(jsonrpc->result.resp.obj);
    }

    free(jsonrpc);
}

char *jsonrpc_get_method(const jsonrpc_t *jsonrpc)
{
    if (jsonrpc == NULL || jsonrpc->method == NULL) {
        return NULL;
    }
    return jsonrpc->method;
}

const jsonrpc_id_t *jsonrpc_get_id(const jsonrpc_t *jsonrpc)
{
    if (jsonrpc == NULL) {
        return NULL;
    }
    return &jsonrpc->id;
}

bool jsonrpc_id_exists(const jsonrpc_id_t *id)
{
    if (id == NULL) {
        return false;
    }
    return id->id_type != JSONRPC_ID_NONE;
}

jsonrpc_t *jsonrpc_server_online(const char *server_name,
                                 const char *description, int n_roles,
                                 mcp_mqtt_role_t *roles)
{
    jsonrpc_t *jsonrpc    = calloc(1, sizeof(jsonrpc_t));
    cJSON     *root       = cJSON_CreateObject();
    cJSON     *meta       = cJSON_CreateObject();
    cJSON     *rbac       = cJSON_CreateObject();
    cJSON     *json_roles = cJSON_CreateArray();

    jsonrpc->id.id_type = JSONRPC_ID_NONE;
    jsonrpc->method     = "notifications/server/online";
    jsonrpc->params     = root;

    for (int i = 0; i < n_roles; i++) {
        cJSON *role = cJSON_CreateObject();

        cJSON_AddStringToObject(role, "name", roles[i].name);
        if (roles[i].description) {
            cJSON_AddStringToObject(role, "description", roles[i].description);
        }

        if (roles[i].n_allowed_methods > 0) {
            cJSON *methods = cJSON_CreateArray();
            for (int j = 0; j < roles[i].n_allowed_methods; j++) {
                cJSON_AddItemToArray(
                    methods, cJSON_CreateString(roles[i].allowed_methods[j]));
            }
            cJSON_AddItemToObject(role, "allowed_methods", methods);
        }

        if (roles[i].n_allowed_tools > 0) {
            cJSON *tools = cJSON_CreateArray();
            for (int j = 0; j < roles[i].n_allowed_tools; j++) {
                cJSON_AddItemToArray(
                    tools, cJSON_CreateString(roles[i].allowed_tools[j]));
            }
            cJSON_AddItemToObject(role, "allowed_tools", tools);
        }

        if (roles[i].n_allowed_resources > 0) {
            cJSON *resources = cJSON_CreateArray();
            for (int j = 0; j < roles[i].n_allowed_resources; j++) {
                cJSON_AddItemToArray(
                    resources,
                    cJSON_CreateString(roles[i].allowed_resources[j]));
            }
            cJSON_AddItemToObject(role, "allowed_resources", resources);
        }

        cJSON_AddItemToArray(json_roles, role);
    }

    cJSON_AddItemToObject(rbac, "roles", json_roles);
    cJSON_AddItemToObject(meta, "rbac", rbac);

    cJSON_AddStringToObject(root, "server_name", server_name);
    if (description) {
        cJSON_AddStringToObject(root, "description", description);
    }
    cJSON_AddItemToObject(root, "meta", meta);
    return jsonrpc;
}

jsonrpc_t *jsonrpc_error_response(const jsonrpc_id_t *id, int code,
                                  const char *message)
{
    jsonrpc_t *jsonrpc = calloc(1, sizeof(jsonrpc_t));

    jsonrpc->id                        = *id;
    jsonrpc->result.result_type        = JSONRPC_RESULT_ERROR;
    jsonrpc->result.resp.error.code    = code;
    jsonrpc->result.resp.error.message = message ? strdup(message) : NULL;
    jsonrpc->result.resp.error.data    = NULL;

    return jsonrpc;
}

jsonrpc_t *jsonrpc_init_response(const jsonrpc_id_t *id, bool tools,
                                 bool resources)
{
    jsonrpc_t *jsonrpc          = calloc(1, sizeof(jsonrpc_t));
    jsonrpc->id                 = *id;
    jsonrpc->result.result_type = JSONRPC_RESULT_RESULT;
    jsonrpc->result.resp.obj    = cJSON_CreateObject();

    cJSON *server_info  = cJSON_CreateObject();
    cJSON *capabilities = cJSON_CreateObject();

    cJSON_AddStringToObject(server_info, "name", "mcp");
    cJSON_AddStringToObject(server_info, "version", "0.0.1");

    if (resources) {
        cJSON *resources = cJSON_CreateObject();
        cJSON_AddBoolToObject(resources, "listChanged", true);
        cJSON_AddItemToObject(capabilities, "resources", resources);
    }

    if (tools) {
        cJSON *tools = cJSON_CreateObject();
        cJSON_AddBoolToObject(tools, "listChanged", true);
        cJSON_AddItemToObject(capabilities, "tools", tools);
    }

    cJSON_AddStringToObject(jsonrpc->result.resp.obj, "protocolVersion",
                            "2024-11-05");
    cJSON_AddItemToObject(jsonrpc->result.resp.obj, "serverInfo", server_info);
    cJSON_AddItemToObject(jsonrpc->result.resp.obj, "capabilities",
                          capabilities);

    return jsonrpc;
}

jsonrpc_t *jsonrpc_tool_list_response(const jsonrpc_id_t *id, int n_tools,
                                      mcp_tool_t *tools)
{
    jsonrpc_t *jsonrpc          = calloc(1, sizeof(jsonrpc_t));
    jsonrpc->id                 = *id;
    jsonrpc->result.result_type = JSONRPC_RESULT_RESULT;
    jsonrpc->result.resp.obj    = cJSON_CreateObject();

    cJSON *tools_array = cJSON_CreateArray();

    for (int i = 0; i < n_tools; i++) {
        cJSON *tool          = cJSON_CreateObject();
        cJSON *input_schema  = cJSON_CreateObject();
        cJSON *required_args = cJSON_CreateArray();
        cJSON *properities   = cJSON_CreateObject();

        cJSON_AddStringToObject(tool, "name", tools[i].name);
        if (tools[i].description) {
            cJSON_AddStringToObject(tool, "description", tools[i].description);
        }

        for (int k = 0; k < tools[i].property_count; k++) {
            cJSON *arg = cJSON_CreateObject();

            if (tools[i].properties[k].description) {
                cJSON_AddStringToObject(arg, "description",
                                        tools[i].properties[k].description);
            }
            switch (tools[i].properties[k].type) {
            case PROPERTY_STRING:
                cJSON_AddStringToObject(arg, "type", "string");
                break;
            case PROPERTY_REAL:
                cJSON_AddStringToObject(arg, "type", "number");
                break;
            case PROPERTY_INTEGER:
                cJSON_AddStringToObject(arg, "type", "integer");
                break;
            case PROPERTY_BOOLEAN:
                cJSON_AddBoolToObject(arg, "type", true);
                break;
            }

            cJSON_AddItemToArray(
                required_args, cJSON_CreateString(tools[i].properties[k].name));
            cJSON_AddItemToObject(properities, tools[i].properties[k].name,
                                  arg);
        }

        cJSON_AddStringToObject(input_schema, "type", "object");
        cJSON_AddItemToObject(input_schema, "properties", properities);
        cJSON_AddItemToObject(input_schema, "required", required_args);
        cJSON_AddItemToObject(tool, "inputSchema", input_schema);
        cJSON_AddItemToArray(tools_array, tool);
    }

    cJSON_AddItemToObject(jsonrpc->result.resp.obj, "tools", tools_array);
    return jsonrpc;
}

int jsonrpc_tool_call_decode(const jsonrpc_t *jsonrpc, char **function_name,
                             int *n_args, property_t **args)
{
    if (jsonrpc == NULL || jsonrpc->params == NULL ||
        cJSON_IsObject(jsonrpc->params) == false) {
        return -1; // Invalid JSON-RPC or no parameters
    }

    cJSON *name = cJSON_GetObjectItem(jsonrpc->params, "name");
    if (name == NULL || !cJSON_IsString(name)) {
        return -2;
    }

    cJSON *json_args = cJSON_GetObjectItem(jsonrpc->params, "arguments");
    if (json_args == NULL || !cJSON_IsObject(json_args)) {
        return -3;
    }
    cJSON *json_kwargs = cJSON_GetObjectItem(json_args, "kwargs");
    if (json_kwargs == NULL || !cJSON_IsObject(json_kwargs)) {
        return -10;
    }

    *n_args        = 0;
    cJSON *current = json_kwargs->child;
    while (current != NULL) {
        (*n_args)++;
        *args = realloc(*args, (*n_args) * sizeof(property_t));
        property_t *current_arg = &(*args)[*n_args - 1];

        current_arg->name = current->string ? strdup(current->string) : NULL;
        if (cJSON_IsString(current)) {
            current_arg->type               = PROPERTY_STRING;
            current_arg->value.string_value = strdup(current->valuestring);
        } else if (cJSON_IsNumber(current)) {
            current_arg->type             = PROPERTY_REAL;
            current_arg->value.real_value = current->valuedouble;
        } else if (cJSON_IsBool(current)) {
            current_arg->type                = PROPERTY_BOOLEAN;
            current_arg->value.boolean_value = cJSON_IsTrue(current);
        } else {
            // Unsupported type
            return -4;
        }

        current = current->next;
    }

    *function_name = strdup(name->valuestring);
    return 0;
}

jsonrpc_t *jsonrpc_tool_call_response(const jsonrpc_id_t *id,
                                      const char         *result)
{
    jsonrpc_t *jsonrpc          = calloc(1, sizeof(jsonrpc_t));
    jsonrpc->id                 = *id;
    jsonrpc->result.result_type = JSONRPC_RESULT_RESULT;
    jsonrpc->result.resp.obj    = cJSON_CreateObject();

    cJSON *content = cJSON_CreateArray();
    cJSON *item    = cJSON_CreateObject();

    cJSON_AddStringToObject(item, "type", "text");
    cJSON_AddStringToObject(item, "text", result);

    cJSON_AddItemToArray(content, item);
    cJSON_AddItemToObject(jsonrpc->result.resp.obj, "content", content);
    return jsonrpc;
}

jsonrpc_t *jsonrpc_resource_list_response(const jsonrpc_id_t *id,
                                          int                 n_resources,
                                          mcp_resource_t     *resources)
{
    jsonrpc_t *jsonrpc          = calloc(1, sizeof(jsonrpc_t));
    jsonrpc->id                 = *id;
    jsonrpc->result.result_type = JSONRPC_RESULT_RESULT;
    jsonrpc->result.resp.obj    = cJSON_CreateObject();

    cJSON *contents = cJSON_CreateArray();

    for (int i = 0; i < n_resources; i++) {
        cJSON *resource = cJSON_CreateObject();

        cJSON_AddStringToObject(resource, "uri", resources[i].uri);
        cJSON_AddStringToObject(resource, "name", resources[i].name);
        if (resources[i].description) {
            cJSON_AddStringToObject(resource, "description",
                                    resources[i].description);
        }
        if (resources[i].mime_type) {
            cJSON_AddStringToObject(resource, "mimeType",
                                    resources[i].mime_type);
        }
        if (resources[i].title) {
            cJSON_AddStringToObject(resource, "title", resources[i].title);
        }

        cJSON_AddItemToArray(contents, resource);
    }

    cJSON_AddItemToObject(jsonrpc->result.resp.obj, "contents", contents);
    return jsonrpc;
}

jsonrpc_t *jsonrpc_resource_read_text_response(const jsonrpc_id_t *id,
                                               mcp_resource_t     *resource,
                                               const char         *content)
{
    jsonrpc_t *jsonrpc          = calloc(1, sizeof(jsonrpc_t));
    jsonrpc->id                 = *id;
    jsonrpc->result.result_type = JSONRPC_RESULT_RESULT;
    jsonrpc->result.resp.obj    = cJSON_CreateObject();

    cJSON *contents = cJSON_CreateArray();

    cJSON *resource_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(resource_obj, "uri", resource->uri);
    cJSON_AddStringToObject(resource_obj, "name", resource->name);
    if (resource->mime_type) {
        cJSON_AddStringToObject(resource_obj, "mimeType", resource->mime_type);
    }
    if (resource->title) {
        cJSON_AddStringToObject(resource_obj, "title", resource->title);
    }
    cJSON_AddStringToObject(resource_obj, "text", content);

    cJSON_AddItemToArray(contents, resource_obj);
    cJSON_AddItemToObject(jsonrpc->result.resp.obj, "contents", contents);

    return jsonrpc;
}

int jsonrpc_resource_read_decode(const jsonrpc_t *jsonrpc, char **uri)
{
    if (jsonrpc == NULL || jsonrpc->params == NULL ||
        cJSON_IsObject(jsonrpc->params) == false) {
        return -1;
    }

    cJSON *uri_item = cJSON_GetObjectItem(jsonrpc->params, "uri");
    if (uri_item == NULL || !cJSON_IsString(uri_item)) {
        return -2;
    }

    *uri = strdup(uri_item->valuestring);

    return 0;
}