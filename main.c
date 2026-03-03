#include "main.h"

char* trim_string(char* str) {
  while(*str == ' ' || *str == '\t') {
    str++;
  }
  char *end = str + strlen(str) - 1;
  while(end >= str && (*end == ' ' || *end == '\t')) {
    *end = '\0';
    end--;
  }
  return str;
}


void enable_raw_mode() {
  if (tcgetattr(STDIN_FILENO, &terminal_state.original_termios) < 0) {
    perror("tcgetattr() failed\n");
    exit(EXIT_FAILURE);
  }

  struct termios raw = terminal_state.original_termios;
  raw.c_lflag &= ~(ICANON);
  raw.c_lflag &= ~(ECHO);
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;

  if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) < 0) {
    perror("tcsetattr() failed\n");
    exit(EXIT_FAILURE);
  }

  terminal_state.raw_mode_enabled = 1;
}

void disable_raw_mode() {
  if (!terminal_state.raw_mode_enabled) {
    return;
  }

  if (tcsetattr(STDIN_FILENO, TCSADRAIN, &terminal_state.original_termios) < 0) {
    perror("tcsetattr() restore failed\n");
    exit(EXIT_FAILURE);
  }

  terminal_state.raw_mode_enabled = 0;
}

void move_cursor_up() {
  printf("\x1b[A");
  fflush(stdout);
}
void move_cursor_down() {
  printf("\x1b[B");
  fflush(stdout);
}
void move_cursor_right() {
  printf("\x1b[C");
  fflush(stdout);
}
void move_cursor_left() {
  printf("\x1b[D");
  fflush(stdout);
}
void clear_line_and_reposition() {
  printf("\x1b[2K"); // clear line
  printf("\x1b[0G"); // move to column 0
  fflush(stdout);
}

void init_line_buffer(LineEditBuffer *buf) {
  buf->buffer[0] = '\0';
  buf->cursor_pos = 0;
  buf->input_len = 0;
}
void insert_character(LineEditBuffer* buf, char ch) {
  if (buf->input_len == MAX_INPUT_SIZE - 1) {
    return;
  }
  for(int i = buf->input_len; i > buf->cursor_pos; i--){
    buf->buffer[i] = buf->buffer[i-1];
  }
  buf->buffer[buf->cursor_pos] = ch;
  buf->input_len++;
  buf->cursor_pos++;
  buf->buffer[buf->input_len] = '\0';
}

void delete_character_before_cursor(LineEditBuffer* buf) {
  if (buf->cursor_pos == 0) {
    return;
  }
  // no actual delete, it's a overwrite (from end to cursor pos)
  for (int i = buf->cursor_pos-1; i < buf->input_len-1; i++) {
    buf->buffer[i] = buf->buffer[i+1];
  }

  buf->input_len--;
  buf->cursor_pos--;
  buf->buffer[buf->input_len] = '\0';
}

void redraw_line(LineEditBuffer* buf) {
  clear_line_and_reposition();
  printf("%s", SHELL_PROMPT);
  printf("%s", buf->buffer);
  int prompt_len = strlen(SHELL_PROMPT);
  int target_col = prompt_len + buf->cursor_pos;
  // move cursor to the currect position
  printf("\x1b[%dG", target_col);
  move_cursor_right();
  fflush(stdout);
}

int handle_escape_sequence(LineEditBuffer* buf) {
  unsigned char seq[2];
  if (read(STDIN_FILENO, seq, 2) == -1) {
    return 0;
  }

  if (seq[0] == '[') {
    switch (seq[1]) {
      case 'A': // up
        return 1;
      case 'B': // down
        return 1;
      case 'C': // right
        if (buf->cursor_pos < buf->input_len) {
          buf->cursor_pos++;
          move_cursor_right();
        }
        return 1;
      case 'D':
        if (buf->cursor_pos > 0) {
          buf->cursor_pos--;
          move_cursor_left();
        }
        return 1;
      default:
        return 0;
      }
  }
  return 0;
}

void read_line_raw(const char* prompt, char **input) {
  LineEditBuffer buf;
  init_line_buffer(&buf); // init the structure

  while(1) {
    unsigned char ch;
    if (read(STDIN_FILENO, &ch, 1) == -1) {
      perror("read() failed");
      break;
    }

    if (ch == '\n' || ch == '\r') {
      printf("\n");
      break;
    }

    // 127 DEL character
    else if (ch == 127 || ch == '\b') {
      delete_character_before_cursor(&buf);
      redraw_line(&buf);
    }

    // 27 escape character
    else if (ch == 27) {
      handle_escape_sequence(&buf);
    }

    // Ctrl + C
    else if (ch == 3) {

    }

    // printable ASCII character
    else if (ch >= 32 && ch < 127) {
      insert_character(&buf, ch);
      redraw_line(&buf);
    }
  }

  *input = (char*)malloc(buf.input_len+1);
  strcpy(*input, buf.buffer);
}


void signal_handler(int sig) {
  if (sig == SIGINT) {
    printf("\n");
    printf("%s", SHELL_PROMPT);
    fflush(stdout);
  }
}


void shell_output(const char* str) {
  printf("%s", str);
  fflush(stdout);
}



void parse_redirections(char* cmd_string, Command *cmd, char** cmd_out) 
{
  *cmd_out = malloc(strlen(cmd_string)+1);
  strcpy(*cmd_out, cmd_string);

  cmd->input_file = NULL;
  cmd->output_file = NULL;
  cmd->append_mode = 0;

  char *append_redirect = strstr(*cmd_out, ">>");
  if (append_redirect != NULL) {
    *append_redirect = '\0';
    char* filename = append_redirect + 2;
    filename = trim_string(filename);
    cmd->output_file = strdup(filename);
    cmd->append_mode = 1;
    return;
  }

  char* output_redirect = strchr(*cmd_out, '>');
  if (output_redirect != NULL) {
    *output_redirect = '\0';
    char* filename = output_redirect + 1;
    filename = trim_string(filename);
    cmd->output_file = strdup(filename);
    cmd->append_mode = 0;
    return;
  }

  char* input_redirect = strchr(*cmd_out, '<');
  if (input_redirect != NULL) {
    *input_redirect = '\0';
    char* filename = input_redirect + 1;
    filename = trim_string(filename);
    cmd->input_file = strdup(filename);
    return;
  }
}

void tokenize_command(char* cmd_string, Command* cmd) {
  char* str_copy = malloc(strlen(cmd_string)+1);
  strcpy(str_copy, cmd_string);
  cmd->argc = 0;
  char* strtok_outer = NULL;
  char* token = strtok_r(str_copy, " \t", &strtok_outer);
  while(token != NULL && cmd->argc < MAX_ARGS-1) { // the last one must be null
    cmd->argv[cmd->argc] = strdup(token);
    cmd->argc++;
    token = strtok_r(NULL, "\t", &strtok_outer);
  }
  cmd->argv[cmd->argc] = NULL;
  free(str_copy);
}

void parsing_pipes(char* input, Pipeline* pipeline) {
  char* str_copy = malloc(strlen(input)+1);
  strcpy(str_copy, input);
  printf("str_copy: %s\n", str_copy);
  pipeline->num_commands = 0;
  char* strtok_outer = NULL;
  char* pipe_part = strtok_r(str_copy, "|", &strtok_outer);
  while(pipe_part != NULL && (pipeline->num_commands < MAX_PIPES)) {
    pipe_part = trim_string(pipe_part);
    Command* cmd = &pipeline->commands[pipeline->num_commands];

    char *cmd_without_redirect;
    parse_redirections(pipe_part, cmd, &cmd_without_redirect);
    tokenize_command(cmd_without_redirect, cmd);

    free(cmd_without_redirect);

    pipeline->num_commands++;
    pipe_part = strtok_r(NULL, "|", &strtok_outer);
    printf("pipe_part: %s\n", pipe_part);
  }
  free(str_copy);
}

void parse_input(char* input, Pipeline *pipeline) {
  input = trim_string(input);
  if (strlen(input)==0) {
    pipeline->num_commands = 0;
    return;
  }
  parsing_pipes(input, pipeline);
}


void free_pipeline(Pipeline* pipeline) {
  for(int i = 0; i < pipeline->num_commands; ++i) {
    Command* cmd = &pipeline->commands[i];
    for(int j = 0; j < cmd->argc; ++j) {
      free(cmd->argv[j]);
    }
    if (cmd->input_file != NULL) {
      free(cmd->input_file);
    }
    if (cmd->output_file != NULL) {
      free(cmd->output_file);
    }
  }
  pipeline->num_commands = 0;
}



int main() {
  signal(SIGINT, signal_handler);
  enable_raw_mode();
  atexit(disable_raw_mode);
  while(running) {
    shell_output(SHELL_PROMPT);
    fflush(stdout);

    char* input = NULL;
    read_line_raw(SHELL_PROMPT,&input);

    size_t input_len = strlen(input);
    if (input_len > 0 && input[input_len - 1] == '\n') {
      input[input_len - 1] = '\0';
    }

    if (strlen(input) == 0) {
      free(input);
      continue;
    }

    if (strcmp(input, "exit") == 0) {
      shell_output("Exit\n");
      running = 0;
      free(input);
      break;
    }

    printf("Your input: %s\n", input);

    Pipeline pipeline = {0};
    parse_input(input, &pipeline);
    if (pipeline.num_commands > 0) {
      printf("Num commands: %d\n", pipeline.num_commands);
      for (int i = 0; i < pipeline.num_commands; ++i) {
        printf("Command %d: ", i);
        for (int j = 0; j < pipeline.commands[i].argc; ++j) {
          printf("%s ", pipeline.commands[i].argv[j]);
        }
        printf("\n");
        if (pipeline.commands[i].input_file) {
          printf("Input from: %s\n", pipeline.commands[i].input_file);
        }
        if (pipeline.commands[i].output_file) {
          printf("Output to: %s (%s mode)\n", 
                 pipeline.commands[i].output_file,
                 pipeline.commands[i].append_mode ? "append" : "overwrite");
        }
      }
    }

    free_pipeline(&pipeline);
    
    free(input);
  }
  return 0;
}
