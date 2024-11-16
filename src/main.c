#include <dirent.h>
#include <errno.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int rows, cols;
int row_position, col_position;
size_t traverse_position = 0;

void err_callback(const char *prefix, const char *string);
void print_callback(const char *prefix, const char *string);
int no_hidden_path(const char *string);
void list_draw(DirectoryInfo *buf, struct _win_st *win);
int *position_clamp(int *pos, int max);
DirectoryInfo *search_directory(const char *path, const size_t path_len);
void free_filesys_buffer(DirectoryInfo *buf, const int *ignore);

int main(int argc, char **argv) {
  FILE *stderr_file = freopen("errlog.txt", "a", stderr);
  if (!stderr_file) {
    err_callback("freopen failed!", strerror(errno));
  }

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

  const size_t max_panels = 32;
  DirectoryInfo *traverse_buffer[max_panels];
  memset(traverse_buffer, 0, sizeof(DirectoryInfo *) * max_panels);

  snprintf(search_buffer, search_path_length + 1, "%s/%s", home, "Music");
  traverse_buffer[0] = search_directory(search_buffer, search_path_length);
  if (!traverse_buffer[0]) {
    err_callback("Failed to read directories! (LAST ERR)", strerror(errno));
    return 1;
  }

  free(search_buffer);
  search_buffer = NULL;

  // expands to row = getmaxy, col = getmaxx()
  getmaxyx(win, rows, cols);
  list_draw(traverse_buffer[0], win);
  move(0, 0);

  int running = 1;
  while (running) {
    const int ch = getch();
    switch (ch) {

    case 'q': {
      running = 0;
    } break;

    case ' ': {
      switch (traverse_buffer[traverse_position][row_position].type) {
      default:
        break;

      case DT_REG: {

      } break;

      case DT_DIR: {

      } break;
      }
    } break;

    case KEY_UP: {
      row_position--;
      move(*position_clamp(&row_position,
                           traverse_buffer[traverse_position]->total_size),
           col_position);
    } break;

    case KEY_DOWN: {
      row_position++;
      move(*position_clamp(&row_position,
                           traverse_buffer[traverse_position]->total_size),
           col_position);
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

int *position_clamp(int *pos, int max) {
  if (*pos > max - 1) {
    *pos = 0;
    return pos;
  }

  if (*pos < 0) {
    *pos = max - 1;
    return pos;
  }

  return pos;
}

void list_draw(DirectoryInfo *buf, struct _win_st *win) {
  wclear(win);
  for (int i = 0; i < rows; i++) {
    if (buf && i < (int)buf->total_size) {
      if (buf[i].name) {
        mvprintw(i, 0, "%s -> %d", buf[i].name, buf[i].type);
      }
    } else {
      break;
    }
  }
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
        DirectoryInfo *tmp_ptr = realloc(buf, size_replace);
        if (!tmp_ptr) {
          // sets the first index of the allocated struct buffer to a bad valid
          // value for checks. if the first one is listed as invalid, then we
          // know shits fucked even if the following ones are "valid".
          buf->valid = 0;
          return buf;
        }
        default_size = size_replace;
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

void free_filesys_buffer(DirectoryInfo *buf, const int *ignore) {
  for (int i = 0; i < (int)buf->total_size; i++) {
    if (buf && ignore) {
      if (buf[i].name && i != *ignore) {
        free(buf[i].name);
        buf[i].name = NULL;
      }

      if (buf[i].path_str && i != *ignore) {
        free(buf[i].path_str);
        buf[i].path_str = NULL;
      }
    } else if (buf) {
      if (buf[i].name) {
        free(buf[i].name);
        buf[i].name = NULL;
      }

      if (buf[i].path_str) {
        free(buf[i].path_str);
        buf[i].path_str = NULL;
      }
    }
  }
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

void err_callback(const char *prefix, const char *string) {
  fprintf(stderr, "%s -> %s\n", prefix, string);
}

void print_callback(const char *prefix, const char *string) {
  fprintf(stdout, "%s -> %s\n", prefix, string);
}
