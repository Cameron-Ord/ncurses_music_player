#include <dirent.h>
#include <errno.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_NODES 64

typedef struct {
  char *name;
  size_t name_length;
  char *path_str;
  size_t path_length;
  size_t index;
  unsigned char type;

  // This value only gets set on the pointer element
  size_t total_size;
  int valid;
} DirectoryInfo;

typedef struct Node Node;
typedef struct Table Table;

struct Node {
  size_t key;
  DirectoryInfo *info_ptr;
  Node *next;
};

struct Table {
  Node *table[MAX_NODES];
};

int rows, cols;
int rows_limit;
int row_position = 0, col_position = 0;
int buffer_pos = 0;
int current_node_key = 0;

size_t hash(size_t key);
void elipsize(char *elipsis_buf, const char *original, const size_t max_size);
const char *type_to_string(unsigned char type);
int create_node(Table *ptr, size_t key);
void table_set_buffer(Table *table, size_t key, DirectoryInfo *dbuf);
Node *search_table(Table *ptr, size_t key);
void err_callback(const char *prefix, const char *string);
void print_callback(const char *prefix, const char *string);
int no_hidden_path(const char *string);
void list_draw(DirectoryInfo *buf);
const int *position_clamp(int *pos, int max);
DirectoryInfo *search_directory(const char *path, const size_t path_len);
void free_filesys_buffer(DirectoryInfo *buf);

int main(int argc, char **argv) {
  FILE *stderr_file = freopen("errlog.txt", "a", stderr);
  if (!stderr_file) {
    err_callback("freopen failed!", strerror(errno));
  }

  err_callback("Errlog initialized!", "no error");

  char *home = getenv("HOME");
  if (!home) {
    return 1;
  }

  struct _win_st *win = initscr();
  if (!win) {
    err_callback("Failed to initialize ncurses!", strerror(errno));
    return 1;
  }

  raw();
  keypad(win, TRUE);
  noecho();

  size_t search_path_length = strlen(home) + strlen("Music") + strlen("/");
  char *search_buffer = malloc(search_path_length + 1);
  if (!search_buffer) {
    err_callback("Failed to allocate buffer!", strerror(errno));
    return 1;
  }

  Table *table = malloc(sizeof(Table) * MAX_NODES);
  if (!table) {
    err_callback("Could not allocate table!", strerror(errno));
    return 1;
  }

  for (size_t i = 0; i < MAX_NODES; i++) {
    table->table[i] = NULL;
    if (!create_node(table, i)) {
      return 1;
    }
  }

  snprintf(search_buffer, search_path_length + 1, "%s/%s", home, "Music");
  table_set_buffer(table, current_node_key,
                   search_directory(search_buffer, search_path_length));

  free(search_buffer);
  search_buffer = NULL;

  // expands to row = getmaxy, col = getmaxx()
  getmaxyx(win, rows, cols);
  list_draw(search_table(table, current_node_key)->info_ptr);
  move(0, 0);

  int running = 1;
  while (running) {

    getmaxyx(win, rows, cols);
    rows_limit = rows * 0.75;

    Node *current_node = search_table(table, current_node_key);
    const size_t list_size = current_node->info_ptr->total_size;

    if (rows_limit > list_size) {
      rows_limit = list_size;
    }

    list_draw(current_node->info_ptr);
    move(row_position, col_position);

    const int ch = getch();

    switch (ch) {

    case 'q': {
      running = 0;
    } break;

    case ' ': {
      switch (search_table(table, current_node_key)->info_ptr->type) {
      default:
        break;

      case DT_REG: {

      } break;

      case DT_DIR: {
        Node *current = search_table(table, current_node_key);
        // I don't need to worry about accessing outside bounds here since the
        // row_position index is always clamped to the size of the buffer.
        search_buffer = current->info_ptr[buffer_pos].path_str;
        const size_t length = current->info_ptr[buffer_pos].path_length;

        current_node_key++;
        position_clamp(&current_node_key, MAX_NODES);
        if (current_node_key != 0) {
          free_filesys_buffer(search_table(table, current_node_key)->info_ptr);
          table_set_buffer(table, current_node_key,
                           search_directory(search_buffer, length));
          row_position = 0;
          buffer_pos = 0;
        }
      } break;
      }
    } break;

    case KEY_LEFT: {
      const int tmp_key = current_node_key;
      current_node_key--;
      position_clamp(&current_node_key, MAX_NODES);

      if (search_table(table, current_node_key)->info_ptr != NULL) {
        row_position = 0;
        buffer_pos = 0;
      } else {
        current_node_key = tmp_key;
      }
    } break;

    case KEY_RIGHT: {
      const int tmp_key = current_node_key;
      current_node_key++;
      position_clamp(&current_node_key, MAX_NODES);

      if (search_table(table, current_node_key)->info_ptr != NULL) {
        row_position = 0;
        buffer_pos = 0;
      } else {
        current_node_key = tmp_key;
      }
    } break;

    case KEY_UP: {
      row_position--;
      buffer_pos--;
      position_clamp(&row_position, rows_limit);
      position_clamp(
          &buffer_pos,
          search_table(table, current_node_key)->info_ptr->total_size);
    } break;

    case KEY_DOWN: {
      row_position++;
      buffer_pos++;
      position_clamp(&row_position, rows_limit);
      position_clamp(
          &buffer_pos,
          search_table(table, current_node_key)->info_ptr->total_size);
    } break;
    }

    refresh();
  }

  if (stderr_file) {
    fclose(stderr_file);
  }

  endwin();
  return 0;
}

const int *position_clamp(int *pos, int max) {
  if (max == 0) {
    *pos = 0;
    return pos;
  }

  if (*pos > max - 1) {
    *pos = max - 1;
    return pos;
  }

  if (*pos < 0) {
    *pos = 0;
    return pos;
  }

  return pos;
}

void elipsize(char *elipsis_buffer, const char *original, size_t max_size) {
  for (size_t i = 0; i < max_size; i++) {
    elipsis_buffer[i] = original[i];
  }

  elipsis_buffer[max_size - 1] = '~';
  elipsis_buffer[max_size] = '\0';
}

void list_draw(DirectoryInfo *buf) {

  clear();
  if (buf && buf->total_size == 0) {
    return;
  }

  int j = 0;
  if (buffer_pos >= rows_limit) {
    j = buffer_pos - (rows_limit - 1);
  }

  const int left = cols - (cols - 2);

  for (int i = j, k = 0; i < (int)buf->total_size && k < rows_limit; i++, k++) {
    if (buf[i].name) {
      const size_t max_size = 50;
      char *name_ptr = NULL;
      if (strlen(buf[i].name) >= max_size) {
        char tmp_name_buf[max_size * 2];
        elipsize(tmp_name_buf, buf[i].name, max_size);
        name_ptr = tmp_name_buf;
      } else {
        name_ptr = buf[i].name;
      }

      mvprintw(k, left, "%s - (%s)", name_ptr, type_to_string(buf[i].type));
    }
  }

  mvprintw(rows - (rows * 0.1), left, "%d/%zu", buffer_pos + 1,
           buf->total_size);
}

DirectoryInfo *search_directory(const char *search_path,
                                const size_t path_len) {
  size_t default_size = 4;
  DirectoryInfo *buf = malloc(sizeof(DirectoryInfo) * default_size);
  if (!buf) {
    err_callback("Failed to allocate buffer!", strerror(errno));
    return NULL;
  }

  buf->total_size = 0;
  int written = 0;

  DIR *dir = opendir(search_path);
  if (!dir) {
    err_callback("Failed to open directory!", strerror(errno));
    free(buf);
    return NULL;
  }

  size_t accumulator = 0;
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (no_hidden_path(entry->d_name)) {
      if (accumulator >= default_size) {
        size_t size_replace = default_size * 2;
        DirectoryInfo *tmp_ptr =
            realloc(buf, size_replace * sizeof(DirectoryInfo));
        if (!tmp_ptr) {
          // sets the first index of the allocated struct buffer to a bad valid
          // value for checks. if the first one is listed as invalid, then we
          // know shits fucked even if the following ones are "valid".
          err_callback("Realloc failed!", strerror(errno));
          buf->valid = 0;
          return buf;
        }

        default_size = size_replace;
        buf = tmp_ptr;
      }

      buf[accumulator].type = entry->d_type;
      buf[accumulator].valid = 1;
      buf[accumulator].index = accumulator;
      buf[accumulator].name_length = 0;
      buf[accumulator].path_length = 0;
      buf[accumulator].path_str = NULL;
      buf[accumulator].name = NULL;

      size_t name_length = strlen(entry->d_name);
      buf[accumulator].name = malloc(name_length + 1);
      if (!buf[accumulator].name) {
        buf->valid = 0;
        return buf;
      }

      strcpy(buf[accumulator].name, entry->d_name);
      buf[accumulator].name_length = name_length;

      size_t combined_path_len = name_length + path_len + strlen("/");
      buf[accumulator].path_str = malloc(combined_path_len + 1);
      if (!buf[accumulator].path_str) {
        buf->valid = 0;
        return buf;
      }

      char tmp_buffer[combined_path_len + 1];
      snprintf(tmp_buffer, combined_path_len + 1, "%s/%s", search_path,
               buf[accumulator].name);

      strcpy(buf[accumulator].path_str, tmp_buffer);
      buf[accumulator].path_length = combined_path_len;

      accumulator++;
    }
  }

  // Set the first element's total size value to the accumulated value.
  buf->total_size = accumulator;
  return buf;
}

void free_filesys_buffer(DirectoryInfo *buf) {
  if (!buf) {
    return;
  }

  for (int i = 0; i < (int)buf->total_size; i++) {
    if (buf[i].name) {
      free(buf[i].name);
      buf[i].name = NULL;
    }

    if (buf[i].path_str) {
      free(buf[i].path_str);
      buf[i].path_str = NULL;
    }
  }

  free(buf);
  buf = NULL;
}

int no_hidden_path(const char *string) {
  if (strcmp(string, "..") == 0) {
    return 0;
  }

  if (strcmp(string, ".") == 0) {
    return 0;
  }

  return 1;
}

Node *search_table(Table *ptr, size_t key) {
  Node *node = ptr->table[hash(key)];
  while (node != NULL) {
    if (node->key == hash(key)) {
      return node;
    }

    node = node->next;
  }

  return NULL;
}

int create_node(Table *ptr, size_t key) {
  Node *node = malloc(sizeof(Node));
  if (!node) {
    err_callback("Failed to allocate node!", strerror(errno));
    return false;
  }

  node->key = hash(key);
  node->info_ptr = NULL;
  node->next = ptr->table[node->key];
  ptr->table[node->key] = node;

  return true;
}

void err_callback(const char *prefix, const char *string) {
  fprintf(stderr, "%s -> %s\n", prefix, string);
}

void print_callback(const char *prefix, const char *string) {
  fprintf(stdout, "%s -> %s\n", prefix, string);
}

size_t hash(size_t key) { return key % MAX_NODES; }

void table_set_buffer(Table *table, size_t key, DirectoryInfo *dbuf) {
  Node *node = search_table(table, key);
  node->info_ptr = dbuf;
}

const char *type_to_string(unsigned char type) {
  switch (type) {
  default: {
    return "Unknown type";
  } break;
  case DT_REG: {
    return "Regular file";
  };
  case DT_DIR: {
    return "Directory";
  } break;
  }
}
