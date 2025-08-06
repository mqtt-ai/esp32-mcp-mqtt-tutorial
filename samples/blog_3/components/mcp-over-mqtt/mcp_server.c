#include <stdio.h>

#include "esp_log.h"
#include "mqtt_client.h"

#include "jsonrpc.h"
#include "mcp_server.h"

typedef struct {
    char *client_id;
} connect_mcp_client;

struct mcp_server {
    char *name;
    char *description;

    char *broker_uri;
    char *client_id;
    char *user;
    char *password;
    char *cert;

    int         n_tools;
    mcp_tool_t *tools;

    int               n_resources;
    mcp_resource_t   *resources;
    mcp_resource_read read_callback;

    esp_mqtt_client_handle_t client;

    char *control_topic;
    char *presence_topic;
    char *capability_topic;

    int                 n_clients;
    connect_mcp_client *clients;
};

static esp_mqtt5_user_property_item_t connect_property_arr[] = {
    { "MCP-COMPONENT-TYPE", "mcp-server" }
};

static esp_mqtt5_subscribe_property_config_t subscribe_property = {
    .no_local_flag = true,
};

mcp_server_t *mcp_server_init(const char *name, const char *description,
                              const char *broker_uri, const char *client_id,
                              const char *user, const char *password,
                              const char *cert)
{
    if (!name || !broker_uri || !client_id) {
        return NULL;
    }

    if (strncmp(broker_uri, "mqtt://", 7) != 0 &&
        strncmp(broker_uri, "mqtts://", 8) != 0) {
        return NULL;
    }

    char *server_control_topic    = calloc(1, 128);
    char *server_presence_topic   = calloc(1, 128);
    char *server_capability_topic = calloc(1, 128);

    snprintf(server_control_topic, 128, "$mcp-server/%s/%s", client_id, name);
    snprintf(server_presence_topic, 128, "$mcp-server/presence/%s/%s",
             client_id, name);
    snprintf(server_capability_topic, 128, "$mcp-server/capability/%s/%s",
             client_id, name);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri      = broker_uri,
        .session.keepalive       = 10,
        .session.protocol_ver    = MQTT_PROTOCOL_V_5,
        .credentials.client_id   = client_id,
        .session.last_will.topic = server_presence_topic,
        .session.last_will.msg   = "",
        .buffer.size             = 81920,
    };

    if (user) {
        mqtt_cfg.credentials.username = user;
    }
    if (password) {
        mqtt_cfg.credentials.authentication.password = password;
    }
    if (cert) {
        mqtt_cfg.credentials.authentication.certificate     = cert;
        mqtt_cfg.credentials.authentication.certificate_len = strlen(cert);
    }

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        free(server_control_topic);
        free(server_presence_topic);
        free(server_capability_topic);
        return NULL;
    }

    mcp_server_t *server = calloc(1, sizeof(mcp_server_t));

    server->name = strdup(name);
    if (description) {
        server->description = strdup(description);
    } else {
        server->description = NULL;
    }
    server->broker_uri = strdup(broker_uri);
    server->client_id  = strdup(client_id);
    if (user) {
        server->user = strdup(user);
    } else {
        server->user = NULL;
    }
    if (password) {
        server->password = strdup(password);
    } else {
        server->password = NULL;
    }
    if (cert) {
        server->cert = strdup(cert);
    } else {
        server->cert = NULL;
    }

    esp_mqtt5_connection_property_config_t connect_property = {
        .session_expiry_interval  = 10,
        .maximum_packet_size      = 81920,
        .receive_maximum          = 1024,
        .topic_alias_maximum      = 2,
        .will_delay_interval      = 1,
        .payload_format_indicator = false,
        .message_expiry_interval  = 10,
    };

    esp_mqtt5_client_set_user_property(
        &connect_property.user_property, connect_property_arr,
        sizeof(connect_property_arr) / sizeof(esp_mqtt5_user_property_item_t));
    esp_mqtt5_client_set_connect_property(client, &connect_property);
    esp_mqtt5_client_delete_user_property(connect_property.user_property);
    esp_mqtt5_client_set_subscribe_property(client, &subscribe_property);

    server->client           = client;
    server->control_topic    = server_control_topic;
    server->presence_topic   = server_presence_topic;
    server->capability_topic = server_capability_topic;

    return server;
}

void mcp_server_close(mcp_server_t *server)
{
    if (server) {
        esp_mqtt_client_destroy(server->client);

        free(server->name);
        free(server->broker_uri);
        free(server->client_id);

        free(server->control_topic);
        free(server->presence_topic);
        free(server->capability_topic);

        if (server->description) {
            free(server->description);
        }
        if (server->user) {
            free(server->user);
        }
        if (server->password) {
            free(server->password);
        }
        if (server->cert) {
            free(server->cert);
        }
        for (int i = 0; i < server->n_tools; i++) {
            free(server->tools[i].name);
            if (server->tools[i].description) {
                free(server->tools[i].description);
            }
            for (int j = 0; j < server->tools[i].property_count; j++) {
                free(server->tools[i].properties[j].name);
                if (server->tools[i].properties[j].description) {
                    free(server->tools[i].properties[j].description);
                }
            }
            free(server->tools[i].properties);
        }
        free(server->tools);

        for (int i = 0; i < server->n_resources; i++) {
            free(server->resources[i].uri);
            free(server->resources[i].name);
            if (server->resources[i].description) {
                free(server->resources[i].description);
            }
            if (server->resources[i].mime_type) {
                free(server->resources[i].mime_type);
            }
            if (server->resources[i].title) {
                free(server->resources[i].title);
            }
        }
        free(server->resources);

        if (server->n_clients > 0) {
            for (int i = 0; i < server->n_clients; i++) {
                free(server->clients[i].client_id);
            }
            free(server->clients);
        }
        free(server);
    }
}

int mcp_server_register_tool(mcp_server_t *server, int n_tools,
                             mcp_tool_t *tools)
{
    server->n_tools = n_tools;
    server->tools   = calloc(n_tools, sizeof(mcp_tool_t));

    for (int i = 0; i < n_tools; i++) {
        server->tools[i].name = strdup(tools[i].name);
        server->tools[i].description =
            tools[i].description ? strdup(tools[i].description) : NULL;
        server->tools[i].property_count = tools[i].property_count;
        server->tools[i].properties =
            calloc(tools[i].property_count, sizeof(property_t));
        for (int j = 0; j < tools[i].property_count; j++) {
            server->tools[i].properties[j] = tools[i].properties[j];
        }
        server->tools[i].call = tools[i].call;
        ESP_LOGI("mcp_server", "Registered tool: %s", server->tools[i].name);
    }

    return 0;
}

int mcp_server_register_resources(mcp_server_t *server, int n_resources,
                                  mcp_resource_t   *resources,
                                  mcp_resource_read read_callback)
{
    server->n_resources = n_resources;
    server->resources   = calloc(n_resources, sizeof(mcp_resource_t));

    for (int i = 0; i < n_resources; i++) {
        server->resources[i].uri  = strdup(resources[i].uri);
        server->resources[i].name = strdup(resources[i].name);
        server->resources[i].description =
            resources[i].description ? strdup(resources[i].description) : NULL;
        server->resources[i].mime_type =
            resources[i].mime_type ? strdup(resources[i].mime_type) : NULL;
        server->resources[i].title =
            resources[i].title ? strdup(resources[i].title) : NULL;
    }

    server->read_callback = read_callback;
    return 0;
}

static char *get_user_property(esp_mqtt5_event_property_t *property,
                               const char                 *key)
{
    if (property == NULL || property->user_property == NULL) {
        return NULL;
    }

    uint8_t item_num =
        esp_mqtt5_client_get_user_property_count(property->user_property);
    if (item_num == 0) {
        return NULL;
    }
    esp_mqtt5_user_property_item_t *items =
        calloc(item_num, sizeof(esp_mqtt5_user_property_item_t));

    esp_mqtt5_client_get_user_property(property->user_property, items,
                                       &item_num);
    char *value = NULL;
    for (int i = 0; i < item_num; i++) {
        if (strcmp(items[i].key, key) == 0) {
            value = strdup(items[i].value);
            break;
        }
    }
    // for (int i = 0; i < item_num; i++) {
    // free(items[i].key);
    // free(items[i].value);
    //}

    free(items);
    return value;
}

static bool insert_client(mcp_server_t *server, const char *client_id)
{
    for (int i = 0; i < server->n_clients; i++) {
        if (strcmp(server->clients[i].client_id, client_id) == 0) {
            // client already exists
            return false;
        }
    }

    if (server->n_clients == 0) {
        server->clients = calloc(1, sizeof(connect_mcp_client));
    } else {
        server->clients =
            realloc(server->clients,
                    (server->n_clients + 1) * sizeof(connect_mcp_client));
    }
    server->clients[server->n_clients].client_id = strdup(client_id);
    server->n_clients++;
    return true;
}

static bool remove_client(mcp_server_t *server, const char *topic_client_id)
{
    for (int i = 0; i < server->n_clients; i++) {
        char *find = strstr(topic_client_id, server->clients[i].client_id);
        if (find != NULL &&
            strlen(find) == strlen(server->clients[i].client_id)) {
            free(server->clients[i].client_id);
            for (int j = i; j < server->n_clients - 1; j++) {
                server->clients[j] = server->clients[j + 1];
            }
            server->n_clients--;
            if (server->n_clients == 0) {
                free(server->clients);
                server->clients = NULL;
            } else {
                server->clients =
                    realloc(server->clients,
                            server->n_clients * sizeof(connect_mcp_client));
            }
            return true;
        }
    }
    return false;
}

static bool is_client_init(mcp_server_t *server, const char *topic_client)
{
    for (int i = 0; i < server->n_clients; i++) {
        char *find = strstr(topic_client, server->clients[i].client_id);
        if (find != NULL &&
            strlen(find) == strlen(server->clients[i].client_id)) {
            return true; // client is initialized
        }
    }
    return false;
}

static mcp_resource_t *get_resource_by_uri(mcp_server_t *server,
                                           const char   *uri)
{
    for (int i = 0; i < server->n_resources; i++) {
        if (strcmp(server->resources[i].uri, uri) == 0) {
            return &server->resources[i];
        }
    }
    return NULL;
}

static bool mcp_server_tool_check(mcp_server_t *server, const char *tool_name,
                                  int n_args, property_t *args,
                                  mcp_tool_t **tool)
{
    if (server == NULL || tool_name == NULL || args == NULL || n_args <= 0) {
        return false; // Invalid parameters
    }

    for (int i = 0; i < server->n_tools; i++) {
        if (strcmp(server->tools[i].name, tool_name) == 0) {
            if (server->tools[i].property_count != n_args) {
                return false;
            }
            for (int j = 0; j < n_args; j++) {
                if (strcmp(server->tools[i].properties[j].name, args[j].name) !=
                    0) {
                    return false; // Argument name mismatch
                } else {
                    if (server->tools[i].properties[j].type ==
                        PROPERTY_INTEGER) {
                        args[j].type = PROPERTY_INTEGER;
                        args[j].value.integer_value =
                            (long long) args[j].value.real_value;
                    }
                }
            }
            *tool = &server->tools[i];
            return true;
        }
    }
    return false;
}

static void event_handler(void *args, esp_event_base_t base, int32_t event_id,
                          void *event_data)
{
    mcp_server_t           *server = (mcp_server_t *) args;
    esp_mqtt_event_handle_t event  = event_data;
    // esp_mqtt_client_handle_t client = event->client;
    int msg_id = 0;

    switch ((esp_mqtt_event_id_t) event_id) {
    case MQTT_EVENT_CONNECTED: {
        ESP_LOGI("mcp_server", "MQTT client connected");
        msg_id =
            esp_mqtt_client_subscribe(server->client, server->control_topic, 0);
        if (msg_id < 0) {
            ESP_LOGE("mcp_server", "subscribe control topic failed: %d",
                     msg_id);
        }
        char *data = jsonrpc_encode(
            jsonrpc_server_online(server->name, server->description, 0, NULL));

        msg_id = esp_mqtt_client_publish(server->client, server->presence_topic,
                                         data, strlen(data), 0, 1);
        if (msg_id < 0) {
            ESP_LOGE("mcp_server", "publish presence msg failed: %d", msg_id);
        }
        free(data);
        break;
    }
    case MQTT_EVENT_DATA: {
        jsonrpc_t *jsonrpc = jsonrpc_decode(event->data);

        if (jsonrpc == NULL) {
            ESP_LOGW("mcp_server", "decode jsonrpc failed, data: %.*s",
                     event->data_len, event->data);
            return;
        }
        char               *method = jsonrpc_get_method(jsonrpc);
        const jsonrpc_id_t *id     = jsonrpc_get_id(jsonrpc);
        if (method == NULL) {
            ESP_LOGW("mcp_server", "jsonrpc method not found");
            jsonrpc_decode_free(jsonrpc);
            return;
        }

        if (strncmp(event->topic, server->control_topic,
                    strlen(server->control_topic)) == 0) {
            if (strcmp(method, "initialize") != 0) {
                ESP_LOGW("mcp_server", "unknown method: %s", method);
                jsonrpc_decode_free(jsonrpc);
                return;
            }

            if (!jsonrpc_id_exists(id)) {
                ESP_LOGW("mcp_server", "jsonrpc id not exists");
                jsonrpc_decode_free(jsonrpc);
                return;
            }

            char *client_id =
                get_user_property(event->property, "MCP-MQTT-CLIENT-ID");
            if (client_id == NULL) {
                ESP_LOGW("mcp_server",
                         "MCP-MQTT-CLINET-ID not found in user property");
                jsonrpc_decode_free(jsonrpc);
                return;
            }

            char *topic = calloc(1, 128);
            snprintf(topic, 128, "$mcp-rpc/%s/%s/%s", client_id,
                     server->client_id, server->name);

            ESP_LOGI("mcp_server", "MCP client initialized: %s", client_id);
            if (insert_client(server, client_id)) {
                esp_mqtt5_client_set_subscribe_property(server->client,
                                                        &subscribe_property);
                esp_mqtt_client_subscribe(server->client, topic, 0);
            }

            char *response = jsonrpc_encode(jsonrpc_init_response(
                id, server->n_tools > 0, server->n_resources > 0));

            msg_id = esp_mqtt_client_publish(server->client, topic, response,
                                             strlen(response), 0, 0);
            free(response);
            free(topic);
        }

        if (strncmp(event->topic, "$mcp-client/presence/",
                    strlen("$mcp-client/presence/")) == 0) {
            if (event->data_len == 0) {
                char *topic = calloc(1, event->topic_len + 1);
                strncpy(topic, event->topic, event->topic_len);
                remove_client(server, topic);
                free(topic);
            }
        }

        if (strncmp(event->topic, "$mcp-rpc/", strlen("$mcp-rpc/")) == 0) {
            char *topic = calloc(1, event->topic_len + 1);
            strncpy(topic, event->topic, event->topic_len);
            if (strcmp(method, "notifications/initialized") == 0) {
                // client initialized
            }
            char *response = NULL;
            if (strcmp(method, "tools/list") == 0) {
                ESP_LOGI("mcp_server", "tools/list request received from %s",
                         topic);
                response = jsonrpc_encode(jsonrpc_tool_list_response(
                    id, server->n_tools, server->tools));
            }
            if (strcmp(method, "tools/call") == 0) {
                char       *f_name = NULL;
                int         n_args = 0;
                mcp_tool_t *tool   = NULL;
                property_t *args   = NULL;

                ESP_LOGI("mcp_server", "tools/call request received from %s",
                         topic);
                int ret =
                    jsonrpc_tool_call_decode(jsonrpc, &f_name, &n_args, &args);
                if (ret != 0) {
                    ESP_LOGW("mcp_server", "decode tool call failed: %d", ret);
                    response = jsonrpc_encode(
                        jsonrpc_error_response(id, -32600, "Invalid params"));
                } else {
                    if (mcp_server_tool_check(server, f_name, n_args, args,
                                              &tool)) {
                        const char *result = tool->call(n_args, args);
                        response           = jsonrpc_encode(
                            jsonrpc_tool_call_response(id, result));
                    } else {
                        response = jsonrpc_encode(jsonrpc_error_response(
                            id, -32601, "Method not found"));
                    }

                    for (int i = 0; i < server->n_tools; i++) {
                        if (args[i].type == PROPERTY_STRING) {
                            free(args[i].value.string_value);
                        }
                    }
                    free(args);
                }
            }
            if (strcmp(method, "resources/list") == 0) {
                ESP_LOGI("mcp_server",
                         "resources/list request received from %s", topic);
                response = jsonrpc_encode(jsonrpc_resource_list_response(
                    id, server->n_resources, server->resources));
            }
            if (strcmp(method, "resources/read") == 0) {
                ESP_LOGI("mcp_server",
                         "resources/read request received from %s", topic);

                char *uri = NULL;
                int   ret = jsonrpc_resource_read_decode(jsonrpc, &uri);

                if (ret == 0) {
                    mcp_resource_t *resource = get_resource_by_uri(server, uri);
                    if (resource) {
                        const char *content = server->read_callback(uri);
                        response =
                            jsonrpc_encode(jsonrpc_resource_read_text_response(
                                id, resource, content));
                    }

                    free(uri);
                }
            }

            if (response) {
                msg_id = esp_mqtt_client_publish(
                    server->client, topic, response, strlen(response), 0, 0);
                free(response);
            }

            free(topic);
        }

        jsonrpc_decode_free(jsonrpc);
        break;
    }
    default:
        ESP_LOGD("mcp_server", "other event_id: %d", event_id);
    }
}

int mcp_server_run(mcp_server_t *server)
{
    int ret = esp_mqtt_client_register_event(server->client, ESP_EVENT_ANY_ID,
                                             event_handler, server);
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_LOGI("mcp_server", "Connecting to MQTT broker: %s", server->broker_uri);
    return esp_mqtt_client_start(server->client);
}