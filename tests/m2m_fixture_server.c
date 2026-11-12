#include "lonejson.h"

#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define M2M_REQUEST_CAPACITY 8192u

typedef struct m2m_fixture_server {
  lonejson *runtime;
  lonejson_owned_buffer store_json;
  lonejson_m2m_credential initial_credential;
  lonejson_m2m_signup signup;
  int port;
  int listen_fd;
  int stop;
  char *request_bytes;
} m2m_fixture_server;

static int write_all(int fd, const char *data, size_t len) {
  while (len != 0u) {
    ssize_t wrote = write(fd, data, len);
    if (wrote < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    data += (size_t)wrote;
    len -= (size_t)wrote;
  }
  return 0;
}

static void respond_json(int fd, int status, const char *body) {
  const char *reason = status == 200   ? "OK"
                       : status == 400 ? "Bad Request"
                                       : "Unauthorized";
  char header[256];
  int header_len = snprintf(header, sizeof(header),
                            "HTTP/1.1 %d %s\r\n"
                            "Content-Type: application/json\r\n"
                            "Content-Length: %lu\r\n"
                            "Connection: close\r\n\r\n",
                            status, reason, (unsigned long)strlen(body));
  if (header_len > 0) {
    (void)write_all(fd, header, (size_t)header_len);
    (void)write_all(fd, body, strlen(body));
  }
}

static int append_store(m2m_fixture_server *server,
                        const lonejson_m2m_credential *credential,
                        const lonejson_m2m_signup *signup,
                        lonejson_error *error) {
  lonejson_owned_buffer next;
  lonejson_status status;

  lonejson_owned_buffer_init(&next);
  status = lonejson_owned_buffer_sink(&next, "{\"credentials\":[", 16u, error);
  if (status == LONEJSON_STATUS_OK && credential != NULL) {
    status = lonejson_owned_buffer_sink(&next, credential->record_json.data,
                                        credential->record_json.len, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_owned_buffer_sink(&next, "]", 1u, error);
  }
  if (status == LONEJSON_STATUS_OK && signup != NULL) {
    status = lonejson_owned_buffer_sink(&next, ",\"signups\":[", 12u, error);
  }
  if (status == LONEJSON_STATUS_OK && signup != NULL) {
    status = lonejson_owned_buffer_sink(&next, signup->record_json.data,
                                        signup->record_json.len, error);
  }
  if (status == LONEJSON_STATUS_OK && signup != NULL) {
    status = lonejson_owned_buffer_sink(&next, "]", 1u, error);
  }
  if (status == LONEJSON_STATUS_OK) {
    status = lonejson_owned_buffer_sink(&next, "}", 1u, error);
  }
  if (status != LONEJSON_STATUS_OK) {
    lonejson_owned_buffer_free(&next);
    return -1;
  }
  lonejson_owned_buffer_free(&server->store_json);
  server->store_json = next;
  return 0;
}

static const char *find_authorization_header(char *request) {
  char *line = request;

  while (line != NULL && *line != '\0') {
    char *next = strstr(line, "\r\n");
    if (next != NULL) {
      *next = '\0';
      next += 2;
    }
    if (strncmp(line, "Authorization:", strlen("Authorization:")) == 0) {
      char *value = line + strlen("Authorization:");
      while (*value == ' ' || *value == '\t') {
        ++value;
      }
      return value;
    }
    line = next;
  }
  return NULL;
}

static int from_hex(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + c - 'a';
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + c - 'A';
  }
  return -1;
}

static void query_decode(char *value) {
  char *readp = value;
  char *writep = value;

  while (*readp != '\0') {
    if (*readp == '+') {
      *writep++ = ' ';
      ++readp;
    } else if (readp[0] == '%' && readp[1] != '\0' && readp[2] != '\0') {
      int hi = from_hex(readp[1]);
      int lo = from_hex(readp[2]);
      if (hi >= 0 && lo >= 0) {
        *writep++ = (char)((hi << 4) | lo);
        readp += 3;
      } else {
        *writep++ = *readp++;
      }
    } else {
      *writep++ = *readp++;
    }
  }
  *writep = '\0';
}

typedef struct signup_query {
  char *signup_id;
  char *signup_secret;
  char *email;
} signup_query;

static void parse_signup_query(char *query, signup_query *out) {
  char *part = query;

  memset(out, 0, sizeof(*out));
  while (part != NULL && *part != '\0') {
    char *next = strchr(part, '&');
    char *value = strchr(part, '=');
    if (next != NULL) {
      *next++ = '\0';
    }
    if (value != NULL) {
      *value++ = '\0';
      query_decode(value);
      if (strcmp(part, "signup_id") == 0) {
        out->signup_id = value;
      } else if (strcmp(part, "signup_secret") == 0) {
        out->signup_secret = value;
      } else if (strcmp(part, "email") == 0) {
        out->email = value;
      }
    }
    part = next;
  }
}

static void handle_signup(m2m_fixture_server *server, int client_fd,
                          char *query) {
  lonejson_m2m_store store;
  lonejson_m2m_signup_complete_request request;
  lonejson_m2m_signup_completion complete;
  lonejson_error error;
  lonejson_status status;
  char body[4096];
  signup_query values;

  if (query == NULL) {
    respond_json(client_fd, 400, "{\"ok\":false,\"error\":\"missing_query\"}");
    return;
  }
  parse_signup_query(query, &values);

  memset(&store, 0, sizeof(store));
  memset(&request, 0, sizeof(request));
  lonejson_error_init(&error);
  lonejson_m2m_signup_complete_init(&complete);
  store.json = server->store_json.data;
  store.len = server->store_json.len;
  request.store = &store;
  request.signup_id = values.signup_id;
  request.signup_secret = values.signup_secret;
  request.email = values.email;
  request.credential_auth_modes = LONEJSON_M2M_AUTH_DEFAULT;
  status = lonejson_m2m_signup_complete(server->runtime, &request, &complete,
                                        &error);
  if (status != LONEJSON_STATUS_OK) {
    fprintf(stderr, "signup completion failed: %s\n", error.message);
    lonejson_m2m_signup_complete_cleanup(&complete);
    respond_json(client_fd, 401, "{\"ok\":false,\"signed_up\":false}");
    return;
  }
  if (append_store(server, &complete.credential, NULL, &error) != 0) {
    lonejson_m2m_signup_complete_cleanup(&complete);
    respond_json(client_fd, 500, "{\"ok\":false,\"error\":\"store_update\"}");
    return;
  }
  snprintf(body, sizeof(body),
           "{\"ok\":true,\"signed_up\":true,\"client_id\":\"%s\","
           "\"client_secret\":\"%s\",\"api_key\":\"%s\",\"email\":\"%s\"}",
           complete.credential.client_id, complete.credential.client_secret,
           complete.credential.api_key, complete.email);
  respond_json(client_fd, 200, body);
  lonejson_m2m_signup_complete_cleanup(&complete);
}

static void handle_client(m2m_fixture_server *server, int client_fd) {
  char *request_bytes = server->request_bytes;
  size_t used = 0u;
  ssize_t got;
  char *path;
  char *path_end;
  char path_token[2048];
  size_t path_len;
  const char *authorization;
  lonejson_m2m_store store;
  lonejson_m2m_verify_request verify;
  lonejson_m2m_authentication auth;
  lonejson_error error;
  lonejson_status status;

  request_bytes[0] = '\0';
  while (used < M2M_REQUEST_CAPACITY - 1u) {
    got =
        read(client_fd, request_bytes + used, M2M_REQUEST_CAPACITY - 1u - used);
    if (got < 0 && errno == EINTR) {
      continue;
    }
    if (got <= 0) {
      return;
    }
    used += (size_t)got;
    request_bytes[used] = '\0';
    if (strstr(request_bytes, "\r\n\r\n") != NULL) {
      break;
    }
  }
  if (strstr(request_bytes, "\r\n\r\n") == NULL) {
    respond_json(client_fd, 400,
                 "{\"ok\":false,\"error\":\"headers_too_large\"}");
    return;
  }
  if (strncmp(request_bytes, "GET ", 4u) != 0) {
    respond_json(client_fd, 401, "{\"ok\":false,\"error\":\"unsupported\"}");
    return;
  }
  path = request_bytes + 4u;
  path_end = strchr(path, ' ');
  if (path_end == NULL) {
    respond_json(client_fd, 400, "{\"ok\":false,\"error\":\"bad_request\"}");
    return;
  }
  path_len = (size_t)(path_end - path);
  if (path_len >= sizeof(path_token)) {
    respond_json(client_fd, 400, "{\"ok\":false,\"error\":\"path_too_long\"}");
    return;
  }
  memcpy(path_token, path, path_len);
  path_token[path_len] = '\0';
  if (strcmp(path_token, "/health") == 0) {
    respond_json(client_fd, 200, "{\"ok\":true}");
    return;
  }
  if (strcmp(path_token, "/shutdown") == 0) {
    server->stop = 1;
    respond_json(client_fd, 200, "{\"ok\":true,\"stopped\":true}");
    return;
  }
  if (strncmp(path_token, "/signup?", 8u) == 0) {
    handle_signup(server, client_fd, path_token + 8u);
    return;
  }
  if (strcmp(path_token, "/protected") != 0) {
    respond_json(client_fd, 401, "{\"ok\":false,\"error\":\"not_found\"}");
    return;
  }

  memset(&store, 0, sizeof(store));
  memset(&verify, 0, sizeof(verify));
  lonejson_error_init(&error);
  lonejson_m2m_authentication_init(&auth);
  authorization = find_authorization_header(request_bytes);
  store.json = server->store_json.data;
  store.len = server->store_json.len;
  verify.store = &store;
  verify.authorization_header = authorization;
  status = lonejson_m2m_verify_authorization(server->runtime, &verify, &auth,
                                             &error);
  if (status == LONEJSON_STATUS_OK &&
      auth.failure == LONEJSON_AUTH_FAILURE_NONE) {
    respond_json(client_fd, 200, "{\"ok\":true,\"authorized\":true}");
  } else {
    respond_json(client_fd, 401, "{\"ok\":false,\"authorized\":false}");
  }
  lonejson_m2m_authentication_cleanup(&auth);
}

static int open_listener(m2m_fixture_server *server) {
  int yes = 1;
  struct sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);

  server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server->listen_fd < 0) {
    perror("socket M2M fixture");
    return 1;
  }
  (void)setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes,
                   sizeof(yes));
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons((uint16_t)server->port);
  if (bind(server->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    perror("bind M2M fixture");
    return 1;
  }
  if (listen(server->listen_fd, 8) != 0) {
    perror("listen M2M fixture");
    return 1;
  }
  if (getsockname(server->listen_fd, (struct sockaddr *)&addr, &addr_len) !=
      0) {
    perror("getsockname M2M fixture");
    return 1;
  }
  server->port = (int)ntohs(addr.sin_port);
  return 0;
}

static int run_server(m2m_fixture_server *server) {
  while (!server->stop) {
    int client_fd = accept(server->listen_fd, NULL, NULL);
    if (client_fd < 0) {
      if (errno == EINTR) {
        continue;
      }
      perror("accept M2M fixture");
      return 1;
    }
    handle_client(server, client_fd);
    close(client_fd);
  }
  return 0;
}

static int publish_ready(const m2m_fixture_server *server, const char *path) {
  size_t path_size = strlen(path) + sizeof(".tmp");
  char *pending = (char *)malloc(path_size);
  FILE *secrets;
  int wrote;
  int closed;
  int result = 1;

  if (pending == NULL) {
    fprintf(stderr, "failed to allocate M2M readiness path\n");
    return 1;
  }
  snprintf(pending, path_size, "%s.tmp", path);
  secrets = fopen(pending, "wb");
  if (secrets == NULL) {
    perror("open M2M readiness file");
  } else {
    wrote = fprintf(secrets,
                    "{\"port\":%d,\"client_id\":\"%s\",\"client_secret\":\"%"
                    "s\",\"api_key\":\"%s\","
                    "\"signup_url\":\"%s\",\"signup_id\":\"%s\",\"signup_"
                    "secret\":\"%s\"}\n",
                    server->port, server->initial_credential.client_id,
                    server->initial_credential.client_secret,
                    server->initial_credential.api_key, server->signup.url.data,
                    server->signup.signup_id, server->signup.signup_secret);
    closed = fclose(secrets);
    if (wrote < 0 || closed != 0) {
      perror("write M2M readiness file");
    } else if (rename(pending, path) != 0) {
      perror("publish M2M readiness file");
    } else {
      result = 0;
    }
  }
  if (result != 0) {
    unlink(pending);
  }
  free(pending);
  return result;
}

int main(int argc, char **argv) {
  lonejson_error error;
  lonejson_auth_provider provider;
  lonejson_config config;
  lonejson_m2m_credential_request request;
  lonejson_m2m_signup_request signup_request;
  m2m_fixture_server server;
  lonejson_status status;
  char signup_base_url[128];
  char *port_end;
  long port;
  int result = 1;

  if (argc != 3) {
    fprintf(stderr, "usage: %s PORT READY_PATH (PORT=0 selects a free port)\n",
            argv[0]);
    return 2;
  }
  errno = 0;
  port = strtol(argv[1], &port_end, 10);
  if (errno != 0 || port_end == argv[1] || *port_end != '\0' || port < 0 ||
      port > 65535) {
    fprintf(stderr, "M2M port must be an integer from 0 through 65535\n");
    return 2;
  }
  memset(&server, 0, sizeof(server));
  memset(&request, 0, sizeof(request));
  memset(&signup_request, 0, sizeof(signup_request));
  lonejson_m2m_credential_init(&server.initial_credential);
  lonejson_m2m_signup_init(&server.signup);
  lonejson_error_init(&error);
  lonejson_owned_buffer_init(&server.store_json);
  server.port = (int)port;
  server.listen_fd = -1;

  signal(SIGPIPE, SIG_IGN);
  config = lonejson_default_config();
  status = lonejson_auth_provider_init_openssl(&provider, NULL, &error);
  if (status == LONEJSON_STATUS_OK) {
    config.auth_provider = &provider;
    server.runtime = lonejson_new(&config, &error);
  }
  if (status != LONEJSON_STATUS_OK || server.runtime == NULL) {
    fprintf(stderr, "runtime setup failed: %s\n", error.message);
    goto done;
  }
  server.request_bytes = (char *)malloc(M2M_REQUEST_CAPACITY);
  if (server.request_bytes == NULL) {
    fprintf(stderr, "failed to allocate M2M request buffer\n");
    goto done;
  }
  if (open_listener(&server) != 0) {
    goto done;
  }

  request.claim_json = "{\"scope\":[\"read\"],\"fixture\":\"m2m\"}";
  status = lonejson_m2m_credential_generate(server.runtime, &request,
                                            &server.initial_credential, &error);
  snprintf(signup_base_url, sizeof(signup_base_url),
           "http://127.0.0.1:%d/signup", server.port);
  if (status == LONEJSON_STATUS_OK) {
    signup_request.base_url = signup_base_url;
    signup_request.claim_json = "{\"scope\":[\"signup\"],\"fixture\":\"m2m\"}";
    status = lonejson_m2m_signup_generate(server.runtime, &signup_request,
                                          &server.signup, &error);
  }
  if (status == LONEJSON_STATUS_OK &&
      append_store(&server, &server.initial_credential, &server.signup,
                   &error) != 0) {
    status = error.code;
  }
  if (status != LONEJSON_STATUS_OK) {
    fprintf(stderr, "credential setup failed: %s\n", error.message);
    goto done;
  }

  if (publish_ready(&server, argv[2]) != 0) {
    goto done;
  }

  result = run_server(&server);
done:
  if (server.listen_fd >= 0) {
    close(server.listen_fd);
  }
  free(server.request_bytes);
  lonejson_m2m_signup_cleanup(&server.signup);
  lonejson_m2m_credential_cleanup(&server.initial_credential);
  lonejson_owned_buffer_free(&server.store_json);
  lonejson_free(server.runtime);
  return result;
}
