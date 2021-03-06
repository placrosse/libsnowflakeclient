/*
 * Copyright (c) 2017-2018 Snowflake Computing, Inc. All rights reserved.
 */

#include <string.h>
#include "connection.h"
#include <snowflake/logger.h>
#include "snowflake/platform.h"
#include "memory.h"
#include "client_int.h"
#include "constants.h"
#include "error.h"

#define curl_easier_escape(curl, string) curl_easy_escape(curl, string, 0)
#define QUERYCODE_LEN 7
#define REQUEST_GUID_KEY_SIZE 13

/*
 * Debug functions from curl example. Should update at somepoint, and possibly remove from header since these are private functions
 */
static void
dump(const char *text, FILE *stream, unsigned char *ptr, size_t size,
     char nohex);

static int my_trace(CURL *handle, curl_infotype type, char *data, size_t size,
                    void *userp);

static
void dump(const char *text,
          FILE *stream, unsigned char *ptr, size_t size,
          char nohex) {
    size_t i;
    size_t c;

    unsigned int width = 0x10;

    if (nohex)
        /* without the hex output, we can fit more on screen */
        width = 0x40;

    fprintf(stream, "%s, %10.10ld bytes (0x%8.8lx)\n",
            text, (long) size, (long) size);

    for (i = 0; i < size; i += width) {

        fprintf(stream, "%4.4lx: ", (long) i);

        if (!nohex) {
            /* hex not disabled, show it */
            for (c = 0; c < width; c++)
                if (i + c < size)
                    fprintf(stream, "%02x ", ptr[i + c]);
                else
                    fputs("   ", stream);
        }

        for (c = 0; (c < width) && (i + c < size); c++) {
            /* check for 0D0A; if found, skip past and start a new line of output */
            if (nohex && (i + c + 1 < size) && ptr[i + c] == 0x0D &&
                ptr[i + c + 1] == 0x0A) {
                i += (c + 2 - width);
                break;
            }
            fprintf(stream, "%c",
                    (ptr[i + c] >= 0x20) && (ptr[i + c] < 0x80) ? ptr[i + c]
                                                                : '.');
            /* check again for 0D0A, to avoid an extra \n if it's at width */
            if (nohex && (i + c + 2 < size) && ptr[i + c + 1] == 0x0D &&
                ptr[i + c + 2] == 0x0A) {
                i += (c + 3 - width);
                break;
            }
        }
        fputc('\n', stream); /* newline */
    }
    fflush(stream);
}

static
int my_trace(CURL *handle, curl_infotype type,
             char *data, size_t size,
             void *userp) {
    struct data *config = (struct data *) userp;
    const char *text;
    (void) handle; /* prevent compiler warning */

    switch (type) {
        case CURLINFO_TEXT:
            fprintf(stderr, "== Info: %s", data);
            /* FALLTHROUGH */
        default: /* in case a new one is introduced to shock us */
            return 0;

        case CURLINFO_HEADER_OUT:
            text = "=> Send header";
            break;
        case CURLINFO_DATA_OUT:
            text = "=> Send data";
            break;
        case CURLINFO_SSL_DATA_OUT:
            text = "=> Send SSL data";
            break;
        case CURLINFO_HEADER_IN:
            text = "<= Recv header";
            break;
        case CURLINFO_DATA_IN:
            text = "<= Recv data";
            break;
        case CURLINFO_SSL_DATA_IN:
            text = "<= Recv SSL data";
            break;
    }

    dump(text, stderr, (unsigned char *) data, size, config->trace_ascii);
    return 0;
}

static uint32 uimin(uint32 a, uint32 b) {
    return (a < b) ? a : b;
}

static uint32 uimax(uint32 a, uint32 b) {
    return (a > b) ? a : b;
}


cJSON *STDCALL create_auth_json_body(SF_CONNECT *sf,
                                     const char *application,
                                     const char *int_app_name,
                                     const char *int_app_version,
                                     const char *timezone,
                                     sf_bool autocommit) {
    cJSON *body;
    cJSON *data;
    cJSON *client_env;
    cJSON *session_parameters;
    char os_version[128];

    //Create Client Environment JSON blob
    client_env = snowflake_cJSON_CreateObject();
    snowflake_cJSON_AddStringToObject(client_env, "APPLICATION", application);
    snowflake_cJSON_AddStringToObject(client_env, "OS", sf_os_name());
    sf_os_version(os_version);
    snowflake_cJSON_AddStringToObject(client_env, "OS_VERSION", os_version);

    session_parameters = snowflake_cJSON_CreateObject();
    snowflake_cJSON_AddStringToObject(
        session_parameters,
        "AUTOCOMMIT",
        autocommit == SF_BOOLEAN_TRUE ? SF_BOOLEAN_INTERNAL_TRUE_STR
                                      : SF_BOOLEAN_INTERNAL_FALSE_STR);

    snowflake_cJSON_AddStringToObject(session_parameters, "TIMEZONE", timezone);

    //Create Request Data JSON blob
    data = snowflake_cJSON_CreateObject();
    snowflake_cJSON_AddStringToObject(data, "CLIENT_APP_ID", int_app_name);
    snowflake_cJSON_AddStringToObject(data, "CLIENT_APP_VERSION", int_app_version);
    snowflake_cJSON_AddStringToObject(data, "ACCOUNT_NAME", sf->account);
    snowflake_cJSON_AddStringToObject(data, "LOGIN_NAME", sf->user);
    // Add password if one exists
    if (sf->password && *(sf->password)) {
        snowflake_cJSON_AddStringToObject(data, "PASSWORD", sf->password);
    }
    snowflake_cJSON_AddItemToObject(data, "CLIENT_ENVIRONMENT", client_env);
    snowflake_cJSON_AddItemToObject(data, "SESSION_PARAMETERS",
                                  session_parameters);

    //Create body
    body = snowflake_cJSON_CreateObject();
    snowflake_cJSON_AddItemToObject(body, "data", data);


    return body;
}

cJSON *STDCALL create_query_json_body(const char *sql_text, int64 sequence_id, const char *request_id) {
    cJSON *body;
    // Create body
    body = snowflake_cJSON_CreateObject();
    snowflake_cJSON_AddStringToObject(body, "sqlText", sql_text);
    snowflake_cJSON_AddBoolToObject(body, "asyncExec", SF_BOOLEAN_FALSE);
    snowflake_cJSON_AddNumberToObject(body, "sequenceId", (double) sequence_id);
    snowflake_cJSON_AddNumberToObject(body, "querySubmissionTime", (double) time(NULL) * 1000);
    if (request_id)
    {
        snowflake_cJSON_AddStringToObject(body, "requestId", request_id);
    }
    return body;
}

cJSON *STDCALL create_renew_session_json_body(const char *old_token) {
    cJSON *body;
    // Create body
    body = snowflake_cJSON_CreateObject();
    snowflake_cJSON_AddStringToObject(body, "oldSessionToken", old_token);
    snowflake_cJSON_AddStringToObject(body, "requestType", REQUEST_TYPE_RENEW);

    return body;
}

struct curl_slist *STDCALL create_header_no_token(sf_bool use_application_json_accept_type) {
    struct curl_slist *header = NULL;
    header = curl_slist_append(header, HEADER_CONTENT_TYPE_APPLICATION_JSON);
    header = curl_slist_append(header,
                               use_application_json_accept_type ?
                               HEADER_ACCEPT_TYPE_APPLICATION_JSON :
                               HEADER_ACCEPT_TYPE_APPLICATION_SNOWFLAKE);
    header = curl_slist_append(header, HEADER_C_API_USER_AGENT);
    return header;
}

struct curl_slist *STDCALL create_header_token(const char *header_token,
                                               sf_bool use_application_json_accept_type){
    struct curl_slist *header = NULL;
    header = curl_slist_append(header, header_token);
    header = curl_slist_append(header, HEADER_CONTENT_TYPE_APPLICATION_JSON);
    header = curl_slist_append(header,
                               use_application_json_accept_type ?
                               HEADER_ACCEPT_TYPE_APPLICATION_JSON :
                               HEADER_ACCEPT_TYPE_APPLICATION_SNOWFLAKE);
    header = curl_slist_append(header, HEADER_C_API_USER_AGENT);
    return header;
}

sf_bool STDCALL curl_post_call(SF_CONNECT *sf,
                               CURL *curl,
                               char *url,
                               struct curl_slist *header,
                               char *body,
                               cJSON **json,
                               SF_ERROR_STRUCT *error) {
    const char *error_msg;
    SF_JSON_ERROR json_error;
    char query_code[QUERYCODE_LEN];
    char *result_url = NULL;
    cJSON *data = NULL;
    struct curl_slist *new_header = NULL;
    size_t header_token_size;
    char *header_token = NULL;
    sf_bool ret = SF_BOOLEAN_FALSE;
    sf_bool stop = SF_BOOLEAN_FALSE;

    // Set to 0
    memset(query_code, 0, QUERYCODE_LEN);

    do {
        if (!http_perform(curl, POST_REQUEST_TYPE, url, header, body, json,
                          sf->network_timeout, SF_BOOLEAN_FALSE, error, sf->insecure_mode) ||
            !*json) {
            // Error is set in the perform function
            break;
        }
        if ((json_error = json_copy_string_no_alloc(query_code, *json, "code",
                                                    QUERYCODE_LEN)) !=
            SF_JSON_ERROR_NONE &&
            json_error != SF_JSON_ERROR_ITEM_NULL) {
            JSON_ERROR_MSG(json_error, error_msg, "Query code");
            SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_JSON, error_msg,
                                SF_SQLSTATE_UNABLE_TO_CONNECT);
            break;
        }

        // No query code means things went well, just break and return
        if (query_code[0] == '\0') {
            ret = SF_BOOLEAN_TRUE;
            break;
        }

        if (strcmp(query_code, SESSION_EXPIRE_CODE) == 0) {
            if (!renew_session(curl, sf, error)) {
                // Error is set in renew session function
                break;
            } else {
                // TODO refactor creating the header token string
                // Create new header since we have a new token
                header_token_size = strlen(HEADER_SNOWFLAKE_TOKEN_FORMAT) - 2 +
                                    strlen(sf->token) + 1;
                header_token = (char *) SF_CALLOC(1, header_token_size);
                if (!header_token) {
                    SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_OUT_OF_MEMORY,
                                        "Ran out of memory trying to create header token",
                                        SF_SQLSTATE_UNABLE_TO_CONNECT);
                    break;
                }
                snprintf(header_token, header_token_size,
                         HEADER_SNOWFLAKE_TOKEN_FORMAT, sf->token);
                new_header = create_header_token(header_token, SF_BOOLEAN_FALSE);
                if (!curl_post_call(sf, curl, url, new_header, body, json,
                                    error)) {
                    // Error is set in curl call
                    break;
                }
            }
        }

        while (strcmp(query_code, QUERY_IN_PROGRESS_CODE) == 0 ||
               strcmp(query_code, QUERY_IN_PROGRESS_ASYNC_CODE) == 0) {
            // Remove old result URL and query code if this isn't our first rodeo
            SF_FREE(result_url);
            memset(query_code, 0, QUERYCODE_LEN);
            data = snowflake_cJSON_GetObjectItem(*json, "data");
            if (json_copy_string(&result_url, data, "getResultUrl") !=
                SF_JSON_ERROR_NONE) {
                stop = SF_BOOLEAN_TRUE;
                JSON_ERROR_MSG(json_error, error_msg, "Result URL");
                SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_JSON, error_msg,
                                    SF_SQLSTATE_UNABLE_TO_CONNECT);
                break;
            }

            log_trace("ping pong starting...");
            if (!request(sf, json, result_url, NULL, 0, NULL, header,
                         GET_REQUEST_TYPE, error, SF_BOOLEAN_FALSE)) {
                // Error came from request up, just break
                stop = SF_BOOLEAN_TRUE;
                break;
            }

            if (
              (json_error = json_copy_string_no_alloc(query_code, *json, "code",
                                                      QUERYCODE_LEN)) !=
              SF_JSON_ERROR_NONE &&
              json_error != SF_JSON_ERROR_ITEM_NULL) {
                stop = SF_BOOLEAN_TRUE;
                JSON_ERROR_MSG(json_error, error_msg, "Query code");
                SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_JSON, error_msg,
                                    SF_SQLSTATE_UNABLE_TO_CONNECT);
                break;
            }
        }

        if (stop) {
            break;
        }

        ret = SF_BOOLEAN_TRUE;
    }
    while (0); // Dummy loop to break out of

    SF_FREE(result_url);
    SF_FREE(header_token);
    curl_slist_free_all(new_header);

    return ret;
}

sf_bool STDCALL curl_get_call(SF_CONNECT *sf,
                              CURL *curl,
                              char *url,
                              struct curl_slist *header,
                              cJSON **json,
                              SF_ERROR_STRUCT *error) {
    SF_JSON_ERROR json_error;
    const char *error_msg;
    char query_code[QUERYCODE_LEN];
    char *result_url = NULL;
    struct curl_slist *new_header = NULL;
    size_t header_token_size;
    char *header_token = NULL;
    sf_bool ret = SF_BOOLEAN_FALSE;

    // Set to 0
    memset(query_code, 0, QUERYCODE_LEN);

    do {
        if (!http_perform(curl, GET_REQUEST_TYPE, url, header, NULL, json,
                          sf->network_timeout, SF_BOOLEAN_FALSE, error, sf->insecure_mode) ||
            !*json) {
            // Error is set in the perform function
            break;
        }
        if ((json_error = json_copy_string_no_alloc(query_code, *json, "code",
                                                    QUERYCODE_LEN)) !=
            SF_JSON_ERROR_NONE &&
            json_error != SF_JSON_ERROR_ITEM_NULL) {
            JSON_ERROR_MSG(json_error, error_msg, "Query code");
            SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_JSON, error_msg,
                                SF_SQLSTATE_UNABLE_TO_CONNECT);
            break;
        }

        // No query code means things went well, just break and return
        if (query_code[0] == '\0') {
            ret = SF_BOOLEAN_TRUE;
            break;
        }

        if (strcmp(query_code, SESSION_EXPIRE_CODE) == 0) {
            if (!renew_session(curl, sf, error)) {
                // Error is set in renew session function
                break;
            } else {
                // TODO refactor creating the header token string
                // Create new header since we have a new token
                header_token_size = strlen(HEADER_SNOWFLAKE_TOKEN_FORMAT) - 2 +
                                    strlen(sf->token) + 1;
                header_token = (char *) SF_CALLOC(1, header_token_size);
                if (!header_token) {
                    SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_OUT_OF_MEMORY,
                                        "Ran out of memory trying to create header token",
                                        SF_SQLSTATE_UNABLE_TO_CONNECT);
                    break;
                }
                snprintf(header_token, header_token_size,
                         HEADER_SNOWFLAKE_TOKEN_FORMAT, sf->token);
                new_header = create_header_token(header_token, SF_BOOLEAN_FALSE);
                if (!curl_get_call(sf, curl, url, new_header, json, error)) {
                    // Error is set in curl call
                    break;
                }
            }
        }

        ret = SF_BOOLEAN_TRUE;
    }
    while (0); // Dummy loop to break out of

    SF_FREE(result_url);
    SF_FREE(header_token);
    curl_slist_free_all(new_header);

    return ret;
}

void STDCALL decorrelate_jitter_free(DECORRELATE_JITTER_BACKOFF *djb) {
    SF_FREE(djb);
}

DECORRELATE_JITTER_BACKOFF *
STDCALL decorrelate_jitter_init(uint32 base, uint32 cap) {
    DECORRELATE_JITTER_BACKOFF *djb = (DECORRELATE_JITTER_BACKOFF *) SF_CALLOC(
      1, sizeof(DECORRELATE_JITTER_BACKOFF));
    djb->base = base;
    djb->cap = cap;
    return djb;
}

uint32
decorrelate_jitter_next_sleep(DECORRELATE_JITTER_BACKOFF *djb, uint32 sleep) {
    return uimin(djb->cap, uimax(djb->base, (uint32) (rand() % (sleep * 3))));
}

char * STDCALL encode_url(CURL *curl,
                 const char *protocol,
                 const char *account,
                 const char *host,
                 const char *port,
                 const char *url,
                 URL_KEY_VALUE *vars,
                 int num_args,
                 SF_ERROR_STRUCT *error,
                 char *extraUrlParams) {
    int i;
    sf_bool host_empty = is_string_empty(host);
    sf_bool port_empty = is_string_empty(port);
    const char *format;
    char *encoded_url = NULL;
    // Size used for the url format
    size_t base_url_size = 1; //Null terminator
    // Size used to determine buffer size
    size_t encoded_url_size;
    // Initialize reqeust_guid with a blank uuid that will be replaced for each request
    URL_KEY_VALUE request_guid = {
      "request_guid=",
      "00000000-0000-0000-0000-000000000000",
      NULL,
      NULL,
      0,
      0
    };
    const char *amp = "&";
    size_t amp_size = strlen(amp);

    // Set proper format based on variables passed into encode URL.
    // The format includes format specifiers that will be consumed by empty fields
    // (i.e if port is empty, add an extra specifier so that we have 1 call to snprintf, vs. 4 different calls)
    // Format specifier order is protocol, then account, then host, then port, then url.
    // Base size increases reflect the number of static characters in the format string (i.e. ':', '/', '.')
    if (!port_empty && !host_empty) {
        format = "%s://%s%s:%s%s";
        base_url_size += 4;
        // Set account to an empty string since host overwrites account
        account = "";
    } else if (port_empty && !host_empty) {
        format = "%s://%s%s%s%s";
        base_url_size += 3;
        port = "";
        // Set account to an empty string since host overwrites account
        account = "";
    } else if (!port_empty && host_empty) {
        format = "%s://%s.%s:%s%s";
        base_url_size += 5;
        host = DEFAULT_SNOWFLAKE_BASE_URL;
    } else {
        format = "%s://%s.%s%s%s";
        base_url_size += 4;
        host = DEFAULT_SNOWFLAKE_BASE_URL;
        port = "";
    }
    base_url_size +=
      strlen(protocol) + strlen(account) + strlen(host) + strlen(port) +
      strlen(url) + strlen(URL_QUERY_DELIMITER);

    encoded_url_size = base_url_size;
    // Encode URL parameters and set size info
    for (i = 0; i < num_args; i++) {
        if (vars[i].value && *vars[i].value) {
            vars[i].formatted_key = vars[i].key;
            vars[i].formatted_value = curl_easier_escape(curl, vars[i].value);
        } else {
            vars[i].formatted_key = "";
            vars[i].formatted_value = curl_easier_escape(curl, "");
        }
        vars[i].key_size = strlen(vars[i].formatted_key);
        vars[i].value_size = strlen(vars[i].formatted_value);
        // Add an ampersand for each URL parameter since we are going to add request_guid to the end
        encoded_url_size += vars[i].key_size + vars[i].value_size + amp_size;
    }

    // Encode request_guid and set size info
    request_guid.formatted_key = request_guid.key;
    request_guid.formatted_value = curl_easier_escape(curl, request_guid.value);
    request_guid.key_size = strlen(request_guid.formatted_key);
    request_guid.value_size = strlen(request_guid.formatted_value);
    encoded_url_size += request_guid.key_size + request_guid.value_size;


    encoded_url_size += extraUrlParams ?
                        num_args > 0 ? strlen(extraUrlParams) + strlen(URL_PARAM_DELIM) : strlen(extraUrlParams)
                                       : 0;

    encoded_url = (char *) SF_CALLOC(1, encoded_url_size);
    if (!encoded_url) {
        SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_OUT_OF_MEMORY,
                            "Ran out of memory trying to create encoded url",
                            SF_SQLSTATE_UNABLE_TO_CONNECT);
        goto cleanup;
    }
    snprintf(encoded_url, base_url_size, format, protocol, account, host, port,
             url);

    // Initially add the query delimiter "?"
    strncat(encoded_url, URL_QUERY_DELIMITER, strlen(URL_QUERY_DELIMITER));

    // Add encoded URL parameters to encoded_url buffer
    for (i = 0; i < num_args; i++) {
        strncat(encoded_url, vars[i].formatted_key, vars[i].key_size);
        strncat(encoded_url, vars[i].formatted_value, vars[i].value_size);
        strncat(encoded_url, amp, amp_size);
    }

    // Add encoded request_guid to encoded_url buffer
    strncat(encoded_url, request_guid.formatted_key, request_guid.key_size);
    strncat(encoded_url, request_guid.formatted_value, request_guid.value_size);

    // Adding the extra url param (setter of extraUrlParams is responsible to make
    // sure extraUrlParams is correct)
    if (extraUrlParams && !is_string_empty(extraUrlParams))
    {
        if (num_args)
        {
            strncat(encoded_url, URL_PARAM_DELIM, 1);
        }
        strncat(encoded_url, extraUrlParams, strlen(extraUrlParams));
    }

    log_debug("URL: %s", encoded_url);

cleanup:
    // Free created memory
    for (i = 0; i < num_args; i++) {
        curl_free(vars[i].formatted_value);
    }
    curl_free(request_guid.formatted_value);

    return encoded_url;
}

sf_bool is_string_empty(const char *str) {
    return (str && strcmp(str, "") != 0) ? SF_BOOLEAN_FALSE : SF_BOOLEAN_TRUE;
}

SF_JSON_ERROR STDCALL
json_copy_string(char **dest, cJSON *data, const char *item) {
    size_t blob_size;
    cJSON *blob = snowflake_cJSON_GetObjectItem(data, item);
    if (!blob) {
        return SF_JSON_ERROR_ITEM_MISSING;
    } else if (snowflake_cJSON_IsNull(blob)) {
        return SF_JSON_ERROR_ITEM_NULL;
    } else if (!snowflake_cJSON_IsString(blob)) {
        return SF_JSON_ERROR_ITEM_WRONG_TYPE;
    } else {
        blob_size = strlen(blob->valuestring) + 1;
        SF_FREE(*dest);
        *dest = (char *) SF_CALLOC(1, blob_size);
        if (!*dest) {
            return SF_JSON_ERROR_OOM;
        }
        strncpy(*dest, blob->valuestring, blob_size);

        if (strcmp(item, "token") == 0 || strcmp(item, "masterToken") == 0) {
            log_debug("Item and Value; %s: ******", item);
        } else {
            log_debug("Item and Value; %s: %s", item, *dest);
        }
    }

    return SF_JSON_ERROR_NONE;
}

SF_JSON_ERROR STDCALL
json_copy_string_no_alloc(char *dest, cJSON *data, const char *item,
                          size_t dest_size) {
    cJSON *blob = snowflake_cJSON_GetObjectItem(data, item);
    if (!blob) {
        return SF_JSON_ERROR_ITEM_MISSING;
    } else if (snowflake_cJSON_IsNull(blob)) {
        return SF_JSON_ERROR_ITEM_NULL;
    } else if (!snowflake_cJSON_IsString(blob)) {
        return SF_JSON_ERROR_ITEM_WRONG_TYPE;
    } else {
        strncpy(dest, blob->valuestring, dest_size);
        // If string is not null terminated, then add the terminator yourself
        if (dest[dest_size - 1] != '\0') {
            dest[dest_size - 1] = '\0';
        }
        log_debug("Item and Value; %s: %s", item, dest);
    }

    return SF_JSON_ERROR_NONE;
}

SF_JSON_ERROR STDCALL
json_copy_bool(sf_bool *dest, cJSON *data, const char *item) {
    cJSON *blob = snowflake_cJSON_GetObjectItem(data, item);
    if (!blob) {
        return SF_JSON_ERROR_ITEM_MISSING;
    } else if (snowflake_cJSON_IsNull(blob)) {
        return SF_JSON_ERROR_ITEM_NULL;
    } else if (!snowflake_cJSON_IsBool(blob)) {
        return SF_JSON_ERROR_ITEM_WRONG_TYPE;
    } else {
        *dest = snowflake_cJSON_IsTrue(blob) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
        log_debug("Item and Value; %s: %i", item, *dest);
    }

    return SF_JSON_ERROR_NONE;
}

SF_JSON_ERROR STDCALL
json_copy_int(int64 *dest, cJSON *data, const char *item) {
    cJSON *blob = snowflake_cJSON_GetObjectItem(data, item);
    if (!blob) {
        return SF_JSON_ERROR_ITEM_MISSING;
    } else if (snowflake_cJSON_IsNull(blob)) {
        return SF_JSON_ERROR_ITEM_NULL;
    } else if (!snowflake_cJSON_IsNumber(blob)) {
        return SF_JSON_ERROR_ITEM_WRONG_TYPE;
    } else {
        *dest = (int64) blob->valuedouble;
        log_debug("Item and Value; %s: %i", item, *dest);
    }

    return SF_JSON_ERROR_NONE;
}

SF_JSON_ERROR STDCALL
json_detach_array_from_object(cJSON **dest, cJSON *data, const char *item) {
    cJSON *blob = snowflake_cJSON_DetachItemFromObject(data, item);
    if (!blob) {
        return SF_JSON_ERROR_ITEM_MISSING;
    } else if (snowflake_cJSON_IsNull(blob)) {
        return SF_JSON_ERROR_ITEM_NULL;
    } else if (!snowflake_cJSON_IsArray(blob)) {
        return SF_JSON_ERROR_ITEM_WRONG_TYPE;
    } else {
        if (*dest) {
            snowflake_cJSON_Delete(*dest);
        }
        *dest = blob;
        log_debug("Array: %s", item);
    }

    return SF_JSON_ERROR_NONE;
}

SF_JSON_ERROR STDCALL
json_detach_array_from_array(cJSON **dest, cJSON *data, int index) {
    cJSON *blob = snowflake_cJSON_DetachItemFromArray(data, index);
    if (!blob) {
        return SF_JSON_ERROR_ITEM_MISSING;
    } else if (snowflake_cJSON_IsNull(blob)) {
        return SF_JSON_ERROR_ITEM_NULL;
    } else if (!snowflake_cJSON_IsArray(blob)) {
        return SF_JSON_ERROR_ITEM_WRONG_TYPE;
    } else {
        if (*dest) {
            snowflake_cJSON_Delete(*dest);
        }
        *dest = blob;
        log_debug("Array at Index: %s", index);
    }

    return SF_JSON_ERROR_NONE;
}

SF_JSON_ERROR STDCALL
json_detach_object_from_array(cJSON **dest, cJSON *data, int index) {
    cJSON *blob = snowflake_cJSON_DetachItemFromArray(data, index);
    if (!blob) {
        return SF_JSON_ERROR_ITEM_MISSING;
    } else if (snowflake_cJSON_IsNull(blob)) {
        return SF_JSON_ERROR_ITEM_NULL;
    } else if (!snowflake_cJSON_IsObject(blob)) {
        return SF_JSON_ERROR_ITEM_WRONG_TYPE;
    } else {
        if (*dest) {
            snowflake_cJSON_Delete(*dest);
        }
        *dest = blob;
        log_debug("Object at index: %d", index);
    }

    return SF_JSON_ERROR_NONE;
}

ARRAY_LIST *json_get_object_keys(const cJSON *item) {
    if (!item || !snowflake_cJSON_IsObject(item)) {
        return NULL;
    }
    // Get the first key-value pair in the object
    const cJSON *next = item->child;
    ARRAY_LIST *al = sf_array_list_init();
    size_t counter = 0;
    char *key = NULL;

    while (next) {
        // Get key and add the arraylist
        key = next->string;
        sf_array_list_set(al, key, counter);
        // Increment counter and get the next element
        counter++;
        next = next->next;
    }

    return al;
}

size_t
json_resp_cb(char *data, size_t size, size_t nmemb, RAW_JSON_BUFFER *raw_json) {
    size_t data_size = size * nmemb;
    log_debug("Curl response size: %zu", data_size);
    raw_json->buffer = (char *) SF_REALLOC(raw_json->buffer,
                                           raw_json->size + data_size + 1);
    // Start copying where last null terminator existed
    memcpy(&raw_json->buffer[raw_json->size], data, data_size);
    raw_json->size += data_size;
    // Set null terminator
    raw_json->buffer[raw_json->size] = '\0';
    return data_size;
}

sf_bool STDCALL http_perform(CURL *curl,
                             SF_REQUEST_TYPE request_type,
                             char *url,
                             struct curl_slist *header,
                             char *body,
                             cJSON **json,
                             int64 network_timeout,
                             sf_bool chunk_downloader,
                             SF_ERROR_STRUCT *error,
                             sf_bool insecure_mode) {
    CURLcode res;
    sf_bool ret = SF_BOOLEAN_FALSE;
    sf_bool retry = SF_BOOLEAN_FALSE;
    long int http_code = 0;
    DECORRELATE_JITTER_BACKOFF djb = {
      1,      //base
      16      //cap
    };
    /* TODO: let's remove this if not used.
    RETRY_CONTEXT retry_ctx = {
            0,      //retry_count
            network_timeout,
            1,      // time to sleep
            &djb    // Decorrelate jitter
    };
    */
    RAW_JSON_BUFFER buffer = {NULL, 0};
    struct data config;
    config.trace_ascii = 1;

    if (curl == NULL) {
        return SF_BOOLEAN_FALSE;
    }

    //TODO set error buffer

    // Find request GUID in the supplied URL
    char *request_guid_ptr = strstr(url, "request_guid=");
    // Set pointer to the beginning of the UUID string if request GUID exists
    if (request_guid_ptr) {
        request_guid_ptr = request_guid_ptr + REQUEST_GUID_KEY_SIZE;
    }

    do {
        // Reset buffer since this may not be our first rodeo
        SF_FREE(buffer.buffer);
        buffer.size = 0;

        // Generate new request guid, if request guid exists in url
        if (request_guid_ptr && uuid4_generate_non_terminated(request_guid_ptr)) {
            log_error("Failed to generate new request GUID");
            break;
        }

        // Set parameters
        res = curl_easy_setopt(curl, CURLOPT_URL, url);
        if (res != CURLE_OK) {
            log_error("Failed to set URL [%s]", curl_easy_strerror(res));
            break;
        }

        if (DEBUG) {
            curl_easy_setopt(curl, CURLOPT_DEBUGFUNCTION, my_trace);
            curl_easy_setopt(curl, CURLOPT_DEBUGDATA, &config);

            /* the DEBUGFUNCTION has no effect until we enable VERBOSE */
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
        }

        if (header) {
            res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
            if (res != CURLE_OK) {
                log_error("Failed to set header [%s]", curl_easy_strerror(res));
                break;
            }
        }

        // Post type stuffs
        if (request_type == POST_REQUEST_TYPE) {
            res = curl_easy_setopt(curl, CURLOPT_POST, 1);
            if (res != CURLE_OK) {
                log_error("Failed to set post [%s]", curl_easy_strerror(res));
                break;
            }

            if (body) {
                res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
            } else {
                res = curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
            }
            if (res != CURLE_OK) {
                log_error("Failed to set body [%s]", curl_easy_strerror(res));
                break;
            }
        }

        res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (void*)&json_resp_cb);
        if (res != CURLE_OK) {
            log_error("Failed to set writer [%s]", curl_easy_strerror(res));
            break;
        }

        res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &buffer);
        if (res != CURLE_OK) {
            log_error("Failed to set write data [%s]", curl_easy_strerror(res));
            break;
        }

        if (DISABLE_VERIFY_PEER) {
            res = curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            if (res != CURLE_OK) {
                log_error("Failed to disable peer verification [%s]",
                          curl_easy_strerror(res));
                break;
            }
        }

        if (CA_BUNDLE_FILE) {
            res = curl_easy_setopt(curl, CURLOPT_CAINFO, CA_BUNDLE_FILE);
            if (res != CURLE_OK) {
                log_error("Unable to set certificate file [%s]",
                          curl_easy_strerror(res));
                break;
            }
        }

        res = curl_easy_setopt(curl, CURLOPT_SSLVERSION, SSL_VERSION);
        if (res != CURLE_OK) {
            log_error("Unable to set SSL Version [%s]",
                      curl_easy_strerror(res));
            break;
        }

#ifndef _WIN32
        // If insecure mode is set to true, skip OCSP check not matter the value of SF_OCSP_CHECK (global OCSP variable)
        sf_bool ocsp_check;
        if (insecure_mode) {
            ocsp_check = SF_BOOLEAN_FALSE;
        } else {
            ocsp_check = SF_OCSP_CHECK;
        }
        res = curl_easy_setopt(curl, CURLOPT_SSL_SF_OCSP_CHECK, ocsp_check);
        if (res != CURLE_OK) {
            log_error("Unable to set OCSP check enable/disable [%s]",
                      curl_easy_strerror(res));
            break;
        }
#endif

        // Set chunk downloader specific stuff here
        if (chunk_downloader) {
            res = curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
            if (res != CURLE_OK) {
                log_error("Unable to set accepted content encoding",
                          curl_easy_strerror(res));
                break;
            }

            // Set the first character in the buffer as a bracket
            buffer.buffer = (char *) SF_CALLOC(1,
                                               2); // Don't forget null terminator
            buffer.size = 1;
            strncpy(buffer.buffer, "[", 2);
        }

        // Be optimistic
        retry = SF_BOOLEAN_FALSE;

        log_trace("Running curl call");
        res = curl_easy_perform(curl);
        /* Check for errors */
        if (res != CURLE_OK) {
            char msg[1024];
            if (res == CURLE_SSL_CACERT_BADFILE) {
                snprintf(msg, sizeof(msg), "curl_easy_perform() failed. err: %s, CA Cert file: %s",
                    curl_easy_strerror(res), CA_BUNDLE_FILE ? CA_BUNDLE_FILE : "Not Specified");
            }
            else {
                snprintf(msg, sizeof(msg), "curl_easy_perform() failed: %s", curl_easy_strerror(res));
            }
            msg[sizeof(msg)-1] = (char)0;
            log_error(msg);
            SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_CURL,
                                msg,
                                SF_SQLSTATE_UNABLE_TO_CONNECT);
        } else {
            if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code) !=
                CURLE_OK) {
                log_error("Unable to get http response code [%s]",
                          curl_easy_strerror(res));
                SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_CURL,
                                    "Unable to get http response code",
                                    SF_SQLSTATE_UNABLE_TO_CONNECT);
            } else if (http_code != 200) {
                retry = is_retryable_http_code(http_code);
                if (!retry) {
                    SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_RETRY,
                                        "Received unretryable http code",
                                        SF_SQLSTATE_UNABLE_TO_CONNECT);
                }
            } else {
                ret = SF_BOOLEAN_TRUE;
            }
        }

        // Reset everything
        reset_curl(curl);
        http_code = 0;
    }
    while (retry);

    // We were successful so parse JSON from text
    if (ret) {
        if (chunk_downloader) {
            buffer.buffer = (char *) SF_REALLOC(buffer.buffer, buffer.size +
                                                               2); // 1 byte for closing bracket, 1 for null terminator
            memcpy(&buffer.buffer[buffer.size], "]", 1);
            buffer.size += 1;
            // Set null terminator
            buffer.buffer[buffer.size] = '\0';
        }
        snowflake_cJSON_Delete(*json);
        *json = NULL;
        *json = snowflake_cJSON_Parse(buffer.buffer);
        if (*json) {
            ret = SF_BOOLEAN_TRUE;
        } else {
            SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_JSON,
                                "Unable to parse JSON text response.",
                                SF_SQLSTATE_UNABLE_TO_CONNECT);
            ret = SF_BOOLEAN_FALSE;
        }
    }

    SF_FREE(buffer.buffer);

    return ret;
}

sf_bool STDCALL is_retryable_http_code(long int code) {
    return ((code >= 500 && code < 600) || code == 400 || code == 403 ||
            code == 408) ? SF_BOOLEAN_TRUE : SF_BOOLEAN_FALSE;
}

sf_bool STDCALL request(SF_CONNECT *sf,
                        cJSON **json,
                        const char *url,
                        URL_KEY_VALUE *url_params,
                        int num_url_params,
                        char *body,
                        struct curl_slist *header,
                        SF_REQUEST_TYPE request_type,
                        SF_ERROR_STRUCT *error,
                        sf_bool use_application_json_accept_type) {
    sf_bool ret = SF_BOOLEAN_FALSE;
    CURL *curl = NULL;
    char *encoded_url = NULL;
    struct curl_slist *my_header = NULL;
    char *header_token = NULL;
    size_t header_token_size;
    char *header_direct_query_token = NULL;
    size_t header_direct_query_token_size;
    curl = curl_easy_init();
    if (curl) {
        // Use passed in header if one exists
        if (header) {
            my_header = header;
        } else {
            // Create header
            if (sf->token) {
                header_token_size = strlen(HEADER_SNOWFLAKE_TOKEN_FORMAT) - 2 +
                                    strlen(sf->token) + 1;
                header_token = (char *) SF_CALLOC(1, header_token_size);
                if (!header_token) {
                    SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_OUT_OF_MEMORY,
                                        "Ran out of memory trying to create header token",
                                        SF_SQLSTATE_UNABLE_TO_CONNECT);
                    goto cleanup;
                }
                snprintf(header_token, header_token_size,
                         HEADER_SNOWFLAKE_TOKEN_FORMAT, sf->token);
                my_header = create_header_token(header_token, use_application_json_accept_type);
            } else if (sf->direct_query_token) {
                header_direct_query_token_size = strlen(HEADER_DIRECT_QUERY_TOKEN_FORMAT) - 2 +
                                    strlen(sf->direct_query_token) + 1;
                header_direct_query_token = (char *) SF_CALLOC(1, header_direct_query_token_size);
                if (!header_direct_query_token) {
                    SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_OUT_OF_MEMORY,
                                        "Ran out of memory trying to create header direct query token",
                                        SF_SQLSTATE_UNABLE_TO_CONNECT);
                    goto cleanup;
                }
                snprintf(header_direct_query_token, header_direct_query_token_size,
                         HEADER_DIRECT_QUERY_TOKEN_FORMAT, sf->direct_query_token);
                my_header = create_header_token(header_direct_query_token, use_application_json_accept_type);
            } else {
                my_header = create_header_no_token(use_application_json_accept_type);
            }
            log_trace("Created header");
        }

        encoded_url = encode_url(curl, sf->protocol, sf->account, sf->host,
                                 sf->port, url, url_params, num_url_params,
                                 error, sf->directURL_param);
        if (encoded_url == NULL) {
            goto cleanup;
        }

        // Execute request and set return value to result
        if (request_type == POST_REQUEST_TYPE) {
            ret = curl_post_call(sf, curl, encoded_url, my_header, body, json,
                                 error);
        } else if (request_type == GET_REQUEST_TYPE) {
            ret = curl_get_call(sf, curl, encoded_url, my_header, json, error);
        } else {
            SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_REQUEST,
                                "An unknown request type was passed to the request function",
                                SF_SQLSTATE_UNABLE_TO_CONNECT);
            goto cleanup;
        }
    }

cleanup:
    // If we created our own header, then delete it
    if (!header) {
        curl_slist_free_all(my_header);
    }
    curl_easy_cleanup(curl);
    SF_FREE(header_token);
    SF_FREE(encoded_url);

    return ret;
}

sf_bool STDCALL renew_session(CURL *curl, SF_CONNECT *sf, SF_ERROR_STRUCT *error) {
    sf_bool ret = SF_BOOLEAN_FALSE;
    if (!is_string_empty(sf->directURL))
    {
      SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_REQUEST,
                          "Attempt to renew session with XPR direct URL",
                          SF_SQLSTATE_GENERAL_ERROR);
        return ret;
    }
    SF_JSON_ERROR json_error;
    const char *error_msg = NULL;
    char request_id[SF_UUID4_LEN];
    struct curl_slist *header = NULL;
    cJSON *body = NULL;
    cJSON *json = NULL;
    char *s_body = NULL;
    char *encoded_url = NULL;
    char *header_token = NULL;
    size_t header_token_size;
    sf_bool success = SF_BOOLEAN_FALSE;
    cJSON *data = NULL;
    cJSON_bool has_token = 0;
    URL_KEY_VALUE url_params[] = {
      {.key="request_id=", .value=NULL, .formatted_key=NULL, .formatted_value=NULL, .key_size=0, .value_size=0},
    };
    if (!curl) {
        return ret;
    }
    if (is_string_empty(sf->master_token)) {
        SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_REQUEST,
                            "Missing master token when trying to renew session. "
                              "Are you sure your connection was properly setup?",
                            SF_SQLSTATE_UNABLE_TO_CONNECT);
        return ret;
    }
    log_debug("Updating session. Master token: *****");
    // Create header
    header_token_size =
      strlen(HEADER_SNOWFLAKE_TOKEN_FORMAT) - 2 + strlen(sf->master_token) + 1;
    header_token = (char *) SF_CALLOC(1, header_token_size);
    if (!header_token) {
        SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_OUT_OF_MEMORY,
                            "Ran out of memory trying to create header token",
                            SF_SQLSTATE_UNABLE_TO_CONNECT);
        goto cleanup;
    }
    snprintf(header_token, header_token_size, HEADER_SNOWFLAKE_TOKEN_FORMAT,
             sf->master_token);
    header = create_header_token(header_token, SF_BOOLEAN_FALSE);

    // Create body and convert to string
    body = create_renew_session_json_body(sf->token);
    s_body = snowflake_cJSON_Print(body);

    // Create request id, set in url parameter and encode url
    uuid4_generate(request_id);
    url_params[0].value = request_id;
    encoded_url = encode_url(curl, sf->protocol, sf->account, sf->host,
                             sf->port, RENEW_SESSION_URL, url_params, 1, error,
                             sf->directURL_param);
    if (!encoded_url) {
        goto cleanup;
    }

    // Successful call, non-null json, successful success code, data object and session token must all be present
    // otherwise set an error
    if (!curl_post_call(sf, curl, encoded_url, header, s_body, &json, error) ||
        !json) {
        // Do nothing, let error propogate up from post call
        log_error("Curl call failed during renew session");
        goto cleanup;
    } else if ((json_error = json_copy_bool(&success, json, "success"))) {
        log_error("Error finding success in JSON response for renew session");
        JSON_ERROR_MSG(json_error, error_msg, "Success");
        SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_JSON, error_msg,
                            SF_SQLSTATE_UNABLE_TO_CONNECT);
        goto cleanup;
    } else if (!success) {
        log_error("Renew session was unsuccessful");
        SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_RESPONSE,
                            "Request returned as being unsuccessful",
                            SF_SQLSTATE_UNABLE_TO_CONNECT);
        goto cleanup;
    } else if (!(data = snowflake_cJSON_GetObjectItem(json, "data"))) {
        log_error("Missing data field in response");
        SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_JSON,
                            "No data object in JSON response",
                            SF_SQLSTATE_UNABLE_TO_CONNECT);
        goto cleanup;
    } else if (!(has_token = snowflake_cJSON_HasObjectItem(data, "sessionToken"))) {
        log_error("No session token in JSON response");
        SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_JSON,
                            "No session token in JSON response",
                            SF_SQLSTATE_UNABLE_TO_CONNECT);
        goto cleanup;
    } else {
        log_debug("Successful renew session");
        if (!set_tokens(sf, data, "sessionToken", "masterToken", error)) {
            goto cleanup;
        }
        log_debug("Finished updating session");
    }

    ret = SF_BOOLEAN_TRUE;

cleanup:
    curl_slist_free_all(header);
    snowflake_cJSON_Delete(body);
    snowflake_cJSON_Delete(json);
    SF_FREE(s_body);
    SF_FREE(header_token);
    SF_FREE(encoded_url);

    return ret;
}

void STDCALL reset_curl(CURL *curl) {
    curl_easy_reset(curl);
}

void STDCALL retry_ctx_free(RETRY_CONTEXT *retry_ctx) {
    SF_FREE(retry_ctx);
}

RETRY_CONTEXT *STDCALL retry_ctx_init(uint64 timeout) {
    RETRY_CONTEXT *retry_ctx = (RETRY_CONTEXT *) SF_CALLOC(1,
                                                           sizeof(RETRY_CONTEXT));
    retry_ctx->retry_timeout = timeout;
    retry_ctx->retry_count = 0;
    retry_ctx->sleep_time = 1;
    retry_ctx->djb = decorrelate_jitter_init(1, 16);
    return retry_ctx;
}

uint32 STDCALL retry_ctx_next_sleep(RETRY_CONTEXT *retry_ctx) {
    retry_ctx->sleep_time = decorrelate_jitter_next_sleep(retry_ctx->djb,
                                                          retry_ctx->sleep_time);
    return retry_ctx->sleep_time;
}

sf_bool STDCALL set_tokens(SF_CONNECT *sf,
                           cJSON *data,
                           const char *session_token_str,
                           const char *master_token_str,
                           SF_ERROR_STRUCT *error) {
    // Get token
    if (json_copy_string(&sf->token, data, session_token_str)) {
        log_error("No valid token found in response");
        SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_JSON,
                            "Cannot find valid session token in response",
                            SF_SQLSTATE_UNABLE_TO_CONNECT);
        return SF_BOOLEAN_FALSE;
    }
    // Get master token
    if (json_copy_string(&sf->master_token, data, master_token_str)) {
        log_error("No valid master token found in response");
        SET_SNOWFLAKE_ERROR(error, SF_STATUS_ERROR_BAD_JSON,
                            "Cannot find valid master token in response",
                            SF_SQLSTATE_UNABLE_TO_CONNECT);
        return SF_BOOLEAN_FALSE;
    }

    return SF_BOOLEAN_TRUE;
}
