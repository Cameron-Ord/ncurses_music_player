#include <dirent.h>
#include <errno.h>
#include <ncurses.h>
#include <pipewire/pipewire.h>
#include <sndfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct {
  int16_t *buffer;
  uint32_t length;
  uint32_t position;
  size_t samples;
  size_t bytes;
  size_t frames;
  int channels;
  int SR;
  int format;
  float volume;
} AudioData;

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
int current_node_key = 0;

size_t hash(size_t key);
void zero_data(AudioData *a);
void data_set(AudioData *a, const SF_INFO *sfinfo);
int read_audio_file(AudioData *a, const char *path);
// I will implement wide chars later. just gotta get the program's skeleton
// finished so im forcing ascii for files right now
const char *force_ascii(char *string);
int check_range(int c);
void elipsize(char *elipsis_buf, const char *original, const size_t max_size);
const char *type_to_string(unsigned char type);
int create_node(Table *ptr, size_t key);
void table_set_buffer(Table *table, size_t key, DirectoryInfo *dbuf);
Node *search_table(Table *ptr, size_t key);
void err_callback(const char *prefix, const char *string);
void print_callback(const char *prefix, const char *string);
int no_hidden_path(const char *string);
int list_draw(DirectoryInfo *buf);
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

  if (!has_colors()) {
    err_callback("Your terminal does not support colors!", "(no error)");
    endwin();
    return 1;
  }
  start_color();
  use_default_colors();

  AudioData audio_data;
  zero_data(&audio_data);

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

  int running = 1;
  while (running) {
    getmaxyx(win, rows, cols);
    clear();
    rows_limit = rows * 0.75;

    position_clamp(&current_node_key, MAX_NODES);
    Node *current_node = search_table(table, current_node_key);
    const size_t list_size = current_node->info_ptr->total_size;

    if (rows_limit > (int)list_size) {
      rows_limit = (int)list_size;
    }

    list_draw(current_node->info_ptr);
    int cursor_pos = row_position;
    move(*position_clamp(&cursor_pos, rows_limit), col_position);
    refresh();

    const int ch = getch();

    switch (ch) {

    case 'q': {
      running = 0;
    } break;

    case ' ': {
      switch (current_node->info_ptr->type) {
      default:
        break;

      case DT_REG: {
        search_buffer = current_node->info_ptr[row_position].path_str;
        read_audio_file(&audio_data, search_buffer);

      } break;

      case DT_DIR: {
        // I don't need to worry about accessing outside bounds here since the
        // row_position index is always clamped to the size of the buffer.
        search_buffer = current_node->info_ptr[row_position].path_str;
        const size_t plength = current_node->info_ptr[row_position].path_length;

        current_node_key++;
        position_clamp(&current_node_key, MAX_NODES);
        if (current_node_key != 0) {
          Node *next_node = search_table(table, current_node_key);
          DirectoryInfo *dbuf = search_directory(search_buffer, plength);
          search_buffer = NULL;
          free_filesys_buffer(next_node->info_ptr);
          table_set_buffer(table, current_node_key, dbuf);
          row_position = 0;
        }
      } break;
      }
    } break;

    case KEY_LEFT: {
      const int tmp_key = current_node_key;
      current_node_key--;
      position_clamp(&current_node_key, MAX_NODES);
      Node *next_node = search_table(table, current_node_key);

      if (next_node->info_ptr != NULL) {
        row_position = 0;
      } else {
        current_node_key = tmp_key;
      }
    } break;

    case KEY_RIGHT: {
      const int tmp_key = current_node_key;
      current_node_key++;
      position_clamp(&current_node_key, MAX_NODES);
      Node *next_node = search_table(table, current_node_key);

      if (next_node->info_ptr != NULL) {
        row_position = 0;
      } else {
        current_node_key = tmp_key;
      }
    } break;

    case KEY_UP: {
      row_position--;
      position_clamp(&row_position, current_node->info_ptr->total_size);
    } break;

    case KEY_DOWN: {
      row_position++;
      position_clamp(&row_position, current_node->info_ptr->total_size);
    } break;
    }
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

int list_draw(DirectoryInfo *buf) {
  if (buf && buf->total_size == 0) {
    return -1;
  }

  int j = 0;
  if (row_position >= rows_limit) {
    j = row_position - (rows_limit - 1);
  }

  const int left = cols - (cols - 2);

  for (int i = j, k = 0; i < (int)buf->total_size && k < rows_limit; i++, k++) {
    const char *name = buf[i].name;
    const size_t name_length = buf[i].name_length;
    if (name) {
      char *display_buffer = malloc(name_length + 1);
      if (!display_buffer) {
        err_callback("Failed to allocate memory!", strerror(errno));
        return -1;
      }

      int iter = 0;
      while (buf[i].name[iter] != '\0') {
        display_buffer[iter] = name[iter];
        iter++;
      }

      display_buffer[iter] = '\0';
      force_ascii(display_buffer);
      const size_t string_size = strlen(display_buffer);

      if ((int)string_size >= (cols * 0.95)) {
        size_t reduced_size = (size_t)(cols * 0.95) - 1;
        display_buffer[reduced_size++] = '~';
        display_buffer[reduced_size++] = '\0';
      }

      mvprintw(k, left, "%s", display_buffer);

      free(display_buffer);
    }
  }

  mvprintw(rows - (rows * 0.1), left, "%d/%zu", row_position + 1,
           buf->total_size);

  return 1;
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
      char conv_buffer[combined_path_len + 1];

      snprintf(tmp_buffer, combined_path_len + 1, "%s/%s", search_path,
               buf[accumulator].name);

      snprintf(conv_buffer, combined_path_len + 1, "%s/%s", search_path,
               buf[accumulator].name);

      rename(tmp_buffer, force_ascii(conv_buffer));

      strcpy(buf[accumulator].path_str, conv_buffer);
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

int check_range(int c) { return c > 127; }

const char *force_ascii(char *string) {
  int i = 0;
  while (string[i] != '\0') {
    if (check_range(string[i])) {
      string[i] = '~';
    }
    i++;
  }
  return string;
}

void zero_data(AudioData *a) {
  a->format = 0;
  a->SR = 0;
  a->channels = 0;
  a->bytes = 0;
  a->samples = 0;
  a->position = 0;
  a->length = 0;
  a->frames = 0;
}

void data_set(AudioData *a, const SF_INFO *sfinfo) {
  a->format = sfinfo->format;
  a->channels = sfinfo->channels;
  a->SR = sfinfo->samplerate;
  a->frames = sfinfo->frames;
  a->samples = a->frames * a->channels;
  a->bytes = a->samples * sizeof(uint16_t);
}

int read_audio_file(AudioData *a, const char *path) {
  SNDFILE *sndfile = NULL;
  SF_INFO sfinfo = {.frames = 0,
                    .samplerate = 0,
                    .channels = 0,
                    .format = 0,
                    .sections = 0,
                    .seekable = 0};
  zero_data(a);
  sndfile = sf_open(path, SFM_READ, &sfinfo);
  if (!sndfile) {
    err_callback("Could not open file for reading!", strerror(errno));
    return -1;
  }

  data_set(a, &sfinfo);
  a->buffer = malloc(a->bytes);
  if (!a->buffer) {
    err_callback("Failed to allocate pointer!", strerror(errno));
    sf_close(sndfile);
    return -1;
  }

  sf_count_t read = sf_read_short(sndfile, a->buffer, a->samples);
  if (read <= 0) {
    err_callback("Failed to read audio data!", sf_strerror(sndfile));
    free(a->buffer);
    a->buffer = NULL;
    sf_close(sndfile);
    return -1;
  }

  sf_close(sndfile);
  return 1;
}
