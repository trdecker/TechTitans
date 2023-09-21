/* 
 * tsh - A tiny shell program with job control
 * 
 * Tad Decker trdecker
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
char sbuf[MAXLINE];         /* for composing sprintf messages */

/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv); 
int parseargs(char **argv, int *cmds, int *stdin_redir, int *stdout_redir);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
	    break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
	    break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
	    break;
	default:
            usage();
	}
    }

    /* Execute the shell's read/eval loop */
    while (1) {

	/* Read command line */
	if (emit_prompt) {
	    printf("%s", prompt);
	    fflush(stdout);
	}
	if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
	    app_error("fgets error");
	if (feof(stdin)) { /* End of file (ctrl-d) */
	    fflush(stdout);
	    exit(0);
	}

	/* Evaluate the command line */
	eval(cmdline);
	fflush(stdout);
	fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}
  
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (e.g., "quit")
 * then execute it immediately.
 *
 * Otherwise, build a pipeline of commands
 * run the job in the context of the child.  Have the parent wait for child process to complete and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) 
{
    char *argv[MAXARGS];
    int cmds[20];
    int stdin_redir[20];
    int stdout_redir[20];

    parseline(cmdline, argv);
    int num_args = parseargs(argv, cmds, stdin_redir, stdout_redir);
    
    // If no args, return immediately
    if (num_args == 0) {
        return;
    }

    // If a user has a built in command (like "quit"), do that immediately.
    builtin_cmd(argv);

    int pipefd[2];

    // ####   1 CHILD   #### //
    if (num_args == 1) {    
        int child1 = fork();

        if (child1 == 0) {
            // Redirect necessary input
            if (stdin_redir[0] != -1) {
                FILE *fp = fopen(argv[stdin_redir[0]], "r");
                dup2(fileno(fp), fileno(stdin));
                close(fileno(fp));
            }

            // Redirect necessary output
            if (stdout_redir[0] != -1) {
                FILE *fp = fopen(argv[stdout_redir[0]], "w");
                dup2(fileno(fp), fileno(stdout));
                close(fileno(fp));
            }

            execve(argv[cmds[0]], &argv[cmds[0]], NULL);
            exit(0);
        }
        else {
            setpgid(child1, child1);
            waitpid(child1, NULL, 0);
        }
    }
    // #### 2 CHILDREN ##### //
    else if (num_args == 2) {
        int pipe_ret = pipe(pipefd);
        
        if (pipe_ret == -1) {
            fprintf(stderr, "Pipe failed \n");
            fflush(stderr);
            return;
        }

        int child1 = fork();
        int pgid = child1;

        int child2 = -1;
        // Child 1
        if (child1 == 0) {
            // Redirect necessary input
            if (stdin_redir[0] != -1) {
                FILE *fp = fopen(argv[stdin_redir[0]], "r");
                dup2(fileno(fp), fileno(stdin));
                close(fileno(fp));
            }

            // Redirect necessary output
            if (stdout_redir[0] != -1) {
                FILE *fp = fopen(argv[stdout_redir[0]], "w");
                dup2(fileno(fp), fileno(stdout));
                close(fileno(fp));
            }

            close(pipefd[0]);
            dup2(pipefd[1], fileno(stdout));
            close(pipefd[1]);

            execve(argv[cmds[0]], &argv[cmds[0]], NULL);
            exit(0);
        }
        // If parent, fork again
        else {
            setpgid(child1, pgid);
            child2 = fork();
        }

        // Child 2
        if (child2 == 0) {
            // Redirect necessary input
            if (stdin_redir[1] != -1) {
                FILE *fp = fopen(argv[stdin_redir[1]], "r");
                dup2(fileno(fp), fileno(stdin));
                close(fileno(fp));
            }

            // Redirect necessary output
            if (stdout_redir[1] != -1) {
                FILE *fp = fopen(argv[stdout_redir[1]], "w");
                dup2(fileno(fp), fileno(stdout));
                close(fileno(fp));
            }

            close(pipefd[1]);
            dup2(pipefd[0], fileno(stdin));
            close(pipefd[0]);

            execve(argv[cmds[1]], &argv[cmds[1]], NULL);

            exit(0);
        }
        // Last task for parent to do
        else if (child1 > 0) {            
            close(pipefd[0]);
            close(pipefd[1]);
            setpgid(child2, pgid);

            // Wait on every child
            int tmp = 0;
            while (tmp >= 0) {
                tmp = waitpid(-pgid, NULL, 0);
            }


        }
    }
    // #### 3+ CHILDREN #### //
    // Eventually (hopefully) combine this with the upper sections
    if (num_args > 2) {
        int pipe_r = -1;
        int pgid;

        int i;
        // Iterate through commands
        for (i = 0; i < num_args; i++) {
            int pipefds[2];

            int res = pipe(pipefds);
            if (res == -1) {
                unix_error("Pipe fail");
            }

            int child = fork();

            // First child 
            if (child == 0 && i == 0) {
                // Redirect necessary input
                if (stdin_redir[0] != -1) {
                    FILE *fp = fopen(argv[stdin_redir[0]], "r");
                    dup2(fileno(fp), fileno(stdin));
                    close(fileno(fp));
                }

                close(pipefds[0]);
                dup2(pipefds[1], fileno(stdout));
                close(pipefds[1]);

                execve(argv[cmds[i]], &argv[cmds[i]], NULL);
                exit(0);
            }
            // Last child
            else if (child == 0 && i == num_args - 1) {
            // Redirect necessary output
                if (stdout_redir[1] != -1) {
                    FILE *fp = fopen(argv[stdout_redir[1]], "w");
                    dup2(fileno(fp), fileno(stdout));
                    close(fileno(fp));
                }
                close(pipefds[1]);
                dup2(pipe_r, fileno(stdin));
                close(pipe_r);
                close(pipefds[0]);

                execve(argv[cmds[i]], &argv[cmds[i]], NULL);

                exit(0);
            }
            // Middle child
            else if (child == 0) {
                dup2(pipe_r, fileno(stdin));
                close(pipe_r);
                close(pipefds[0]);
                dup2(pipefds[1], fileno(stdout));
                close(pipefds[1]);

                execve(argv[cmds[i]], &argv[cmds[i]], NULL);

                exit(0);
            }
            else {
                if (i == 0) {
                    pgid = child;   
                }
                setpgid(child, pgid);
                if (pipe_r != -1) {
                    dup2(pipefds[0], pipe_r);
                    close(pipefds[0]);
                }
                else {
                    pipe_r = pipefds[0];
                }
                    close(pipefds[1]);
            }
        }

        // Wait on every child
        close(pipe_r);
        int tmp = 0;
        while (tmp >= 0) {
            tmp = waitpid(-pgid, NULL, 0);
        }
    }

    return;
}

/* 
 * parseargs - Parse the arguments to identify pipelined commands
 * 
 * Walk through each of the arguments to find each pipelined command.  If the
 * argument was | (pipe), then the next argument starts the new command on the
 * pipeline.  If the argument was < or >, then the next argument is the file
 * from/to which stdin or stdout should be redirected, respectively.  After it
 * runs, the arrays for cmds, stdin_redir, and stdout_redir all have the same
 * number of items---which is the number of commands in the pipeline.  The cmds
 * array is populated with the indexes of argv corresponding to the start of
 * each command sequence in the pipeline.  For each slot in cmds, there is a
 * corresponding slot in stdin_redir and stdout_redir.  If the slot has a -1,
 * then there is no redirection; if it is >= 0, then the value corresponds to
 * the index in argv that holds the filename associated with the redirection.
 * 
 */
int parseargs(char **argv, int *cmds, int *stdin_redir, int *stdout_redir) 
{
    int argindex = 0;    /* the index of the current argument in the current cmd */
    int cmdindex = 0;    /* the index of the current cmd */

    if (!argv[argindex]) {
        return 0;
    }

    cmds[cmdindex] = argindex;
    stdin_redir[cmdindex] = -1;
    stdout_redir[cmdindex] = -1;
    argindex++;
    while (argv[argindex]) {
        if (strcmp(argv[argindex], "<") == 0) {
            argv[argindex] = NULL;
            argindex++;
            if (!argv[argindex]) { /* if we have reached the end, then break */
                break;
	    }
            stdin_redir[cmdindex] = argindex;
	} else if (strcmp(argv[argindex], ">") == 0) {
            argv[argindex] = NULL;
            argindex++;
            if (!argv[argindex]) { /* if we have reached the end, then break */
                break;
	    }
            stdout_redir[cmdindex] = argindex;
	} else if (strcmp(argv[argindex], "|") == 0) {
            argv[argindex] = NULL;
            argindex++;
            if (!argv[argindex]) { /* if we have reached the end, then break */
                break;
	    }
            cmdindex++;
            cmds[cmdindex] = argindex;
            stdin_redir[cmdindex] = -1;
            stdout_redir[cmdindex] = -1;
	}
        argindex++;
    }

    return cmdindex + 1;
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv) 
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
	buf++;
	delim = strchr(buf, '\'');
    }
    else {
	delim = strchr(buf, ' ');
    }

    while (delim) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* ignore spaces */
	       buf++;

	if (*buf == '\'') {
	    buf++;
	    delim = strchr(buf, '\'');
	}
	else {
	    delim = strchr(buf, ' ');
	}
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* ignore blank line */
	return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0) {
	argv[--argc] = NULL;
    }
    return bg;
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) 
{
    if (strcmp(argv[0], "quit") == 0) {
        exit(0);
    }
    return 0;     /* not a builtin command */
}

/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

