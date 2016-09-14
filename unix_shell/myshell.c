#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_CMDLINE 514

typedef struct Token {
    char *str;
    struct Token *next;
} Token;

typedef struct RedirToken {
    Token *tokens;
    struct RedirToken *next;
} RedirToken;

typedef struct Command {
    RedirToken *rtokens;
    struct Command *next;
} Command, *CommandLine;

Token *create_token(char *str);

void add_token(Token *token, RedirToken *redir_token);
void add_redir_token(RedirToken *redir_token, Command *cmd);
void add_command(Command *cmd, CommandLine *cmd_line);

RedirToken *parse_redir_token(char *redir_token);
Command *parse_command(char *cmd);
CommandLine parse_command_line(char *cmd_buff);

void destroy_redir_token(RedirToken *redir_token);
void destroy_command(Command *cmd);
void destroy_command_line(CommandLine cmd_line);

void print_redir_token(RedirToken *redir_token);
void print_command(Command *cmd);
void print_command_line(CommandLine cmd_line);

void change_dir(char *dir);
void path_working_dir();

void call_program(char *prog_name, char **args, char *filename);
void process_command(Command *cmd);
void process_command_line(CommandLine cmd_line);

void my_print(char *msg)
{
    write(STDOUT_FILENO, msg, strlen(msg));
}

void error()
{
    my_print("An error has occurred\n");
}

int blank_line(char *cmd_buff);

int too_long_line(char *cmd_buff)
{
    return strlen(cmd_buff) == 513 && cmd_buff[MAX_CMDLINE-2] != '\n';
}

int main(int argc, char *argv[]) 
{
    char cmd_buff[MAX_CMDLINE+1];
    char *pinput;
    CommandLine cmd_line;
    FILE *source;

    if(argc == 1) {
        source = stdin;
    } else if(argc == 2) {
        source = fopen(argv[1], "r");
        if(!source) {
            error();
            exit(0);
        }
    } else {
        error();
        exit(0);
    }

    while (1) {
        if(argc == 1) {
            my_print("myshell> ");
        }
        pinput = fgets(cmd_buff, MAX_CMDLINE, source);
        
        if (!pinput) {
            exit(0);
        }

        if(too_long_line(cmd_buff)) {
            do {
                my_print(cmd_buff);
                fgets(cmd_buff, MAX_CMDLINE, source);
            } while(too_long_line(cmd_buff));
            my_print(cmd_buff);
            error();
            continue;
        }

        if(blank_line(cmd_buff)) {
            continue;
        }
        
        my_print(cmd_buff);

        if(*cmd_buff == '>') {
            error();
            continue;
        }
   
        cmd_line = parse_command_line(cmd_buff);
        process_command_line(cmd_line);
        destroy_command_line(cmd_line);
    }

    exit(0);
}

Token *create_token(char *str)
{
    Token *result = malloc(sizeof(Token));
    if(!result) {
        exit(1);
    }

    result->str = str;
    result->next = NULL;
    return result;
}

void add_token(Token *token, RedirToken *redir_token)
{
    Token *p = redir_token->tokens;
    while(p) {
        if(!p->next) {
            break;
        }
        p = p->next;
    }

    if(!p) {
        redir_token->tokens = token;
        return;
    }

    p->next = token;
}

void add_redir_token(RedirToken *redir_token, Command *cmd)
{
    RedirToken *p = cmd->rtokens;
    while(p) {
        if(!p->next) {
            break;
        }
        p = p->next;
    }

    if(!p) {
        cmd->rtokens = redir_token;
        return;
    }

    p->next = redir_token;
}

void add_command(Command *cmd, CommandLine *cmd_line)
{
    Command *p = *cmd_line;
    while(p) {
        if(!p->next) {
            break;
        }
        p = p->next;
    }

    if(!p) {
        *cmd_line = cmd;
        return;
    }

    p->next = cmd;
}

RedirToken *parse_redir_token(char *redir_token)
{
    RedirToken *result = malloc(sizeof(RedirToken));
    result->tokens = NULL;
    result->next = NULL;

    char *ptr;
    char *str = strtok_r(redir_token, " \t\n", &ptr);

    if(!str) {
        return result;
    }

    add_token(create_token(str), result);

    while((str = strtok_r(NULL, " \t\n", &ptr))) {
        add_token(create_token(str), result);
    }

    return result;
}

Command *parse_command(char *cmd)
{
    Command *result = malloc(sizeof(Command));
    result->rtokens = NULL;
    result->next = NULL;

    char *ptr;
    char *redir_token = strtok_r(cmd, ">", &ptr);

    if(!redir_token) {
        return result;
    }

    add_redir_token(parse_redir_token(redir_token), result);

    while((redir_token = strtok_r(NULL, ">", &ptr))) {
        add_redir_token(parse_redir_token(redir_token), result);
    }

    return result;
}

CommandLine parse_command_line(char *cmd_buff)
{
    CommandLine result = NULL;

    char *ptr;
    char *cmd = strtok_r(cmd_buff, ";", &ptr);

    if(!cmd) {
        return result;
    }
    
    add_command(parse_command(cmd), &result);

    while((cmd = strtok_r(NULL, ";", &ptr))) {
        add_command(parse_command(cmd), &result);
    }

    return result;
}

void destroy_redir_token(RedirToken *redir_token)
{
    Token *cur = redir_token->tokens, *temp;
    while(cur) {
        temp = cur->next;
        free(cur);
        cur = temp;
    }
    free(redir_token);
}

void destroy_command(Command *cmd)
{
    RedirToken *cur = cmd->rtokens, *temp;
    while(cur) {
        temp = cur->next;
        destroy_redir_token(cur);
        cur = temp;
    }
    free(cmd);
}

void destroy_command_line(CommandLine cmd_line)
{
    Command *cur = cmd_line, *temp;
    while(cur) {
        temp = cur->next;
        destroy_command(cur);
        cur = temp;
    }
}

void print_redir_token(RedirToken *redir_token)
{
    Token *cur = redir_token->tokens;
    while(cur) {
        my_print(cur->str);
        cur = cur->next;
        if(cur) {
            my_print(" ");
        }
    }
}

void print_command(Command *cmd)
{
    RedirToken *cur = cmd->rtokens;
    while(cur) {
        print_redir_token(cur);
        cur = cur->next;
        if(cur) {
            my_print(" > ");
        }
    }
}

void print_command_line(CommandLine cmd_line)
{
    Command *cur = cmd_line;
    while(cur) {
        print_command(cur);
        cur = cur->next;
        if(cur) {
            my_print(" ; ");
        }
    }
    my_print("\n");
}

void change_dir(char *dir)
{
    if(!dir) {
        dir = getenv("HOME");
    }

    if(chdir(dir) == -1) {
        error();
        return;
    }
}

void path_working_dir()
{
    char *buf;
    if(!(buf = getcwd(NULL, 0))) {
        error();
        return;
    }
    my_print(buf);
    my_print("\n");
    free(buf);
}

void call_program(char *prog_name, char **args, char *filename)
{
    int pid = fork(), fd;
    if(pid) {
        if(waitpid(pid, NULL, 0) == -1) {
            error();
            return;
        }
    } else {
        if(filename) {
            fd = open(filename, O_RDWR|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
            if(fd == -1) {
                error();
                exit(0);
            }
            dup2(fd, STDOUT_FILENO);
        }

        if(execvp(prog_name, args) == -1) {
            error();
            exit(0);
        }
    }
}

void process_command(Command *cmd)
{
    RedirToken *rtp = cmd->rtokens;

    int redir_cnt = 0;
    while(rtp) {
        redir_cnt++;
        rtp = rtp->next;
    }

    char *filename = NULL;
    Token *tp;
    if(redir_cnt == 0) {
        return;
    } else if(redir_cnt == 2) {
        tp = cmd->rtokens->next->tokens;
        if(!tp) {
            error();
            return;
        }
        
        if(tp->next) {
            error();
            return;
        }
        
        filename = tp->str;

    } else if(redir_cnt > 2){
        error();
        return;
    }

    int arg_cnt = 0;
    tp = cmd->rtokens->tokens;

    while(tp) {
        arg_cnt++;
        tp = tp->next;
    }

    if(arg_cnt == 0) {
        return;
    }

    char **args = malloc(sizeof(char *)*(arg_cnt+1));
    int i;
    for(i = 0, tp = cmd->rtokens->tokens; i < arg_cnt; i++) {
        args[i] = tp->str;
        tp = tp->next;
    }

    args[i] = NULL;

    char *prog_name = args[0];

    if(strcmp(prog_name, "exit") == 0) {
        if((arg_cnt != 1) || (redir_cnt != 1)) {
            error();
            return;
        }
        exit(0);
    } else if(strcmp(prog_name, "cd") == 0) {
        if(((arg_cnt != 1) && (arg_cnt != 2)) || (redir_cnt != 1)) {
            error();
            return;
        }
        change_dir(args[1]);
    } else if(strcmp(prog_name, "pwd") == 0) {
        if((arg_cnt != 1) || (redir_cnt != 1)) {
            error();
            return;
        }
        path_working_dir();
    } else {
        call_program(prog_name, args, filename);
    }
}

void process_command_line(CommandLine cmd_line)
{
    Command *cp = cmd_line;

    while(cp) {
        process_command(cp);
        cp = cp->next;
    }
}

int blank_line(char *cmd_buff)
{
    char *p, c;
    for(p = cmd_buff; *p != '\0'; p++)
    {
        c = *p;
        if((c != ' ') && (c != '\t') && (c != '\n')) {
            return 0;
        }
    }

    return 1;
}