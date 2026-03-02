#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define MAX_INPUT_SIZE  1024
#define MAX_ARGS        64
#define MAX_PIPES       16
#define SHELL_PROMPT    "myshell> "

int running = 1;

typedef struct  {
  char* argv[MAX_ARGS];
  int argc;
  char* input_file;
  char* output_file;
  int append_mode;
} Command;

typedef struct  {
  struct termios original_termios;
  int raw_mode_enabled;
} TerminalState;

TerminalState terminal_state = {0};

typedef struct {
  char buffer[MAX_INPUT_SIZE];
  int cursor_pos;
  int input_len;
} LineEditBuffer;



typedef struct  {
  Command commands[MAX_PIPES];
  int num_commands;
} Pipeline;


void move_cursor_left();
void move_cursor_up();
void move_cursor_right();
void move_cursor_down();
void clear_line_and_reposition();

void init_line_buffer(LineEditBuffer *buf);
void insert_character(LineEditBuffer *buf, char ch);
void delete_character_before_cursor(LineEditBuffer *buf);



char* trim_string(char* str);
void signal_handler(int sig);
void shell_output(const char* str);
void parse_redirections(char* cmd_string, Command *cmd, char** cmd_out);
void tokenize_command(char* cmd_string, Command *cmd);
void parsing_pipes(char* input, Pipeline* pipeline);
void parse_input(char* input, Pipeline* pipeline);
void free_pipeline(Pipeline* pipeline);
void enable_raw_mode();
void disable_raw_mode();
