idf_component_register(SRCS "mcp_server.c" "jsonrpc.c" "mcp.c"
                    REQUIRES "mqtt" "json"
                    INCLUDE_DIRS "include")
