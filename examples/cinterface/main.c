#include <CloudProvider.h>
#include <CloudStorage.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOKEN_PATH "cloudstorage-token"
#define BUFFER_SIZE 1024

void print_available_providers(struct cloud_storage* cloud_storage) {
  struct cloud_string_list* providers =
      cloud_storage_provider_list(cloud_storage);

  size_t length = cloud_string_list_length(providers);
  for (size_t i = 0; i < length; i++) {
    cloud_string* string = cloud_string_list_get(providers, i);
    fprintf(stderr, "%s\n", string);
    cloud_string_release(string);
  }

  cloud_string_list_release(providers);
}

enum cloud_status user_consent_required(const struct cloud_provider* c,
                                        void* d) {
  cloud_string* url = cloud_provider_authorize_library_url(c);
  fprintf(stderr, "user consent required at %s\n", url);
  cloud_string_release(url);
  return cloud_wait_for_authorization_code;
}

void auth_done(struct cloud_either_void* e, void* d) {
  struct cloud_error* error = cloud_either_void_error(e);
  if (error) {
    fprintf(stderr, "error while authenticating\n");
    cloud_error_release(error);
  } else {
    fprintf(stderr, "login successful\n");
  }
}

int find_session(cloud_string** token, struct cloud_hints** hints) {
  FILE* file = fopen(TOKEN_PATH, "r");
  if (!file) {
    return -1;
  }
  char* result = malloc(BUFFER_SIZE);
  memset(result, 0, BUFFER_SIZE);
  char letter;
  int index = 0;
  while (fscanf(file, "%c", &letter) != EOF && index < BUFFER_SIZE) {
    result[index++] = letter;
  }
  fclose(file);
  return cloud_provider_deserialize_session(result, token, hints);
}

struct cloud_provider* create_provider(struct cloud_storage* cloud_storage,
                                       cloud_string* name) {
  cloud_string* token = NULL;
  struct cloud_hints* hints = NULL;
  find_session(&token, &hints);
  struct cloud_provider_init_data_args args = {
      .token = token,
      .permission = cloud_read_write,
      .auth_callback = {.user_consent_required = user_consent_required,
                        .done = auth_done},
      .auth_data = NULL,
      .crypto = NULL,
      .http = NULL,
      .http_server = NULL,
      .thread_pool = NULL,
      .hints = hints};
  struct cloud_provider_init_data* init_data =
      cloud_provider_init_data_create(&args);
  struct cloud_provider* result =
      cloud_storage_provider(cloud_storage, name, init_data);
  cloud_provider_init_data_release(init_data);
  if (token) cloud_string_release(token);
  if (hints) cloud_hints_release(hints);
  return result;
}

void save_session(struct cloud_provider* p) {
  FILE* file = fopen(TOKEN_PATH, "w");
  cloud_string* token = cloud_provider_token(p);
  struct cloud_hints* hints = cloud_provider_hints(p);
  cloud_string* session = cloud_provider_serialize_session(token, hints);

  fprintf(file, "%s\n", session);

  cloud_string_release(session);
  cloud_string_release(token);
  cloud_hints_release(hints);
  fclose(file);
}

void list_directory(struct cloud_provider* provider,
                    struct cloud_item* directory) {
  struct cloud_request_list_directory* list_request =
      cloud_provider_list_directory(provider, directory, NULL, NULL);
  struct cloud_either_item_list* result =
      cloud_request_list_directory_result(list_request);

  struct cloud_item_list* list = cloud_either_item_list_result(result);
  if (list) {
    int length = cloud_item_list_length(list);
    for (int i = 0; i < length; i++) {
      struct cloud_item* item = cloud_item_list_get(list, i);
      cloud_string* name = cloud_item_filename(item);
      fprintf(stderr, "%s\n", name);
      cloud_string_release(name);
      cloud_item_release(item);
    }
    cloud_item_list_release(list);
  }

  cloud_either_item_list_release(result);
  cloud_request_release((struct cloud_request*)list_request);
}

void exchange_code(struct cloud_provider* provider, cloud_string* code) {
  struct cloud_request_exchange_code* request =
      cloud_provider_exchange_code(provider, code, NULL, NULL);
  struct cloud_either_token* result =
      cloud_request_exchange_code_result(request);

  struct cloud_token* token = cloud_either_token_result(result);
  if (token) {
    fprintf(stderr, "token available\n");
    cloud_token_release(token);
  } else {
    struct cloud_error* error = cloud_either_token_error(result);
    cloud_string* string = cloud_error_description(error);
    fprintf(stderr, "token error: %s\n", string);
    cloud_string_release(string);
    cloud_error_release(error);
  }

  cloud_either_token_release(result);
  cloud_request_release((struct cloud_request*)request);
}

struct cloud_item* get_item(struct cloud_provider* provider,
                            cloud_string* path) {
  struct cloud_request_get_item* request =
      cloud_provider_get_item(provider, path, NULL, NULL);
  struct cloud_either_item* result = cloud_request_get_item_result(request);

  struct cloud_item* item = cloud_either_item_result(result);
  if (item) {
    cloud_string* name = cloud_item_filename(item);
    fprintf(stderr, "got item %s\n", name);
    cloud_string_release(name);
  }

  cloud_either_item_release(result);
  cloud_request_release((struct cloud_request*)request);
  return item;
}

void get_item_url(struct cloud_provider* provider, struct cloud_item* item) {
  struct cloud_request_get_item_url* request =
      cloud_provider_get_item_url(provider, item, NULL, NULL);
  struct cloud_either_string* result =
      cloud_request_get_item_url_result(request);

  cloud_string* url = cloud_either_string_result(result);
  if (url) {
    fprintf(stderr, "%s\n", url);
    cloud_string_release(url);
  }

  cloud_either_string_release(result);
  cloud_request_release((struct cloud_request*)request);
}

void get_item_data(struct cloud_provider* provider, cloud_string* id) {
  struct cloud_request_get_item_data* request =
      cloud_provider_get_item_data(provider, id, NULL, NULL);
  struct cloud_either_item* result =
      cloud_request_get_item_data_result(request);

  struct cloud_item* item = cloud_either_item_result(result);
  if (item) {
    fprintf(stderr, "item data found\n");
    cloud_item_release(item);
  }

  cloud_either_item_release(result);
  cloud_request_release((struct cloud_request*)request);
}

struct cloud_item* create_directory(struct cloud_provider* provider,
                                    struct cloud_item* parent,
                                    cloud_string* name) {
  struct cloud_request_create_directory* request =
      cloud_provider_create_directory(provider, parent, name, NULL, NULL);
  struct cloud_either_item* result =
      cloud_request_create_directory_result(request);

  struct cloud_item* item = cloud_either_item_result(result);
  if (item) {
    fprintf(stderr, "directory created\n");
  }

  cloud_either_item_release(result);
  cloud_request_release((struct cloud_request*)request);
  return item;
}

void delete_item(struct cloud_provider* provider, struct cloud_item* item) {
  struct cloud_request_delete_item* request =
      cloud_provider_delete_item(provider, item, NULL, NULL);
  struct cloud_either_void* result = cloud_request_delete_item_result(request);

  struct cloud_error* error = cloud_either_void_error(result);
  if (error) {
    fprintf(stderr, "delete item failed\n");
    cloud_error_release(error);
  } else {
    fprintf(stderr, "delete succeeded\n");
  }

  cloud_either_void_release(result);
  cloud_request_release((struct cloud_request*)request);
}

struct cloud_item* rename_item(struct cloud_provider* provider,
                               struct cloud_item* item,
                               cloud_string* new_name) {
  struct cloud_request_rename_item* request =
      cloud_provider_rename_item(provider, item, new_name, NULL, NULL);
  struct cloud_either_item* result = cloud_request_rename_item_result(request);

  struct cloud_item* new_item = cloud_either_item_result(result);
  if (new_item) {
    fprintf(stderr, "item succesfully renamed\n");
  }

  cloud_either_item_release(result);
  cloud_request_release((struct cloud_request*)request);
  return new_item;
}

struct cloud_item* move_item(struct cloud_provider* provider,
                             struct cloud_item* item,
                             struct cloud_item* new_parent) {
  struct cloud_request_move_item* request =
      cloud_provider_move_item(provider, item, new_parent, NULL, NULL);
  struct cloud_either_item* result = cloud_request_move_item_result(request);

  struct cloud_item* new_item = cloud_either_item_result(result);
  if (new_item) {
    fprintf(stderr, "item moved successfully\n");
  }

  cloud_either_item_release(result);
  cloud_request_release((struct cloud_request*)request);
  return new_item;
}

void get_general_data(struct cloud_provider* provider) {
  struct cloud_request_get_general_data* request =
      cloud_provider_get_general_data(provider, NULL, NULL);
  struct cloud_either_general_data* result =
      cloud_request_get_general_data_result(request);

  struct cloud_general_data* data = cloud_either_general_data_result(result);
  if (data) {
    cloud_string* username = cloud_general_data_username(data);
    fprintf(stderr, "got general data %s: %" PRIu64 "/ %" PRIu64 "\n", username,
            cloud_general_data_space_used(data),
            cloud_general_data_space_total(data));
    cloud_string_release(username);
    cloud_general_data_release(data);
  }

  cloud_either_general_data_release(result);
  cloud_request_release((struct cloud_request*)request);
}

uint64_t upload_size(void* data) { return strlen((cloud_string*)data); }

uint32_t upload_put_data(char* buffer, uint32_t maxlength,
                         uint32_t requested_offset, void* data) {
  cloud_string* str = data;
  int length = strlen(str);
  int size = length - requested_offset >= maxlength ? maxlength
                                                    : length - requested_offset;
  memcpy(buffer, str + requested_offset, size);
  return size;
}

struct cloud_item* upload_file(struct cloud_provider* provider,
                               struct cloud_item* parent, cloud_string* name,
                               cloud_string* content) {
  struct cloud_request_upload_file_callback callback = {
      .size = upload_size, .put_data = upload_put_data};
  struct cloud_request_upload_file* request = cloud_provider_upload_file(
      provider, parent, name, &callback, (void*)content);
  struct cloud_either_item* result = cloud_request_upload_file_result(request);

  struct cloud_item* new_item = cloud_either_item_result(result);
  if (new_item) {
    fprintf(stderr, "item succesfully uploaded\n");
  }

  cloud_either_item_release(result);
  cloud_request_release((struct cloud_request*)request);
  return new_item;
}

struct download_data {
  char buffer[BUFFER_SIZE];
  int position;
};

void download_received_data(const char* data, uint32_t length, void* userdata) {
  struct download_data* d = userdata;
  memcpy(d->buffer + d->position, data, length);
  d->position += length;
}

void download_file(struct cloud_provider* provider, struct cloud_item* item) {
  struct download_data data = {.position = 0};
  struct cloud_request_download_file_callback callback = {
      .received_data = download_received_data};
  struct cloud_range range = FULL_RANGE;
  struct cloud_request_download_file* request =
      cloud_provider_download_file(provider, item, range, &callback, &data);
  struct cloud_either_void* result =
      cloud_request_download_file_result(request);

  struct cloud_error* error = cloud_either_void_error(result);
  if (error) {
    fprintf(stderr, "download error\n");
    cloud_error_release(error);
  } else {
    fprintf(stderr, "downloaded %.*s\n", data.position, data.buffer);
  }

  cloud_either_void_release(result);
  cloud_request_release((struct cloud_request*)request);
}

int main(int argc, char** argv) {
  struct cloud_storage* cloud_storage = cloud_storage_create();
  if (argc != 2) {
    fprintf(stderr, "Usage: %s cloud_provider\n", argv[0]);
    fprintf(stderr, "Available cloud providers:\n");
    print_available_providers(cloud_storage);
    cloud_storage_release(cloud_storage);
    return 1;
  }

  const char* cloud_name = argv[1];
  struct cloud_provider* provider = create_provider(cloud_storage, cloud_name);
  struct cloud_item* root = cloud_provider_root_directory(provider);

  get_general_data(provider);

  exchange_code(provider, "{}");
  list_directory(provider, root);
  struct cloud_item* directory = create_directory(provider, root, "Test");
  if (directory) {
    struct cloud_item* new_item =
        rename_item(provider, directory, "TestRenamed");
    cloud_item_release(directory);

    struct cloud_item* item =
        upload_file(provider, new_item, "TestFile", "0123456789");
    if (item != NULL) {
      cloud_string* id = cloud_item_id(item);

      get_item_url(provider, item);
      get_item_data(provider, id);

      download_file(provider, item);

      cloud_string_release(id);
      cloud_item_release(item);
    }

    struct cloud_item* one = create_directory(provider, new_item, "1");
    struct cloud_item* two = create_directory(provider, new_item, "2");
    struct cloud_item* new_two = move_item(provider, two, one);

    delete_item(provider, new_item);

    cloud_item_release(new_two);
    cloud_item_release(new_item);
    cloud_item_release(one);
    cloud_item_release(two);
  }

  save_session(provider);

  cloud_item_release(root);
  cloud_provider_release(provider);
  cloud_storage_release(cloud_storage);
  return 0;
}
