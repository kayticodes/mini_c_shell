#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

char *str_gsub(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub);
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);

// Dummy Handler for SIGINT
void handle_SIGINT(int signo)
{
}

int main(int argc, char *argv[])
{
  // Signal Handles
  // Register handle_SIGINT and ignore_action
  struct sigaction SIGINT_action = {0}, ignore_action = {0}, old_act = {0};
  SIGINT_action.sa_handler = handle_SIGINT;
  sigfillset(&SIGINT_action.sa_mask);
  SIGINT_action.sa_flags = 0;

  ignore_action.sa_handler = SIG_IGN;
  sigaction(SIGTSTP, &ignore_action, NULL);
  sigaction(SIGINT, &ignore_action, &old_act);

  // Input variables
  char *line = NULL;
  size_t size = 0;
  int fe_stat = 0;   // shell exit status of last foreground command - $? shell var
  char *bg_pid = ""; // process id of the most recent background process - $! shell var
  int childStatus;

  while (1)
  {

    int ch_pid;
    while ((ch_pid = waitpid(0, &childStatus, WUNTRACED | WNOHANG)) > 0)
    {
      // check if exited
      // fprintf(stderr, "%s", "in the bg check loop\n");
      if (WIFEXITED(childStatus))
      {
        fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t)ch_pid, WEXITSTATUS(childStatus));
      }
      if (WIFSIGNALED(childStatus))
      {
        fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t)ch_pid, WTERMSIG(childStatus));
      }
      if (WIFSTOPPED(childStatus))
      {
        kill(ch_pid, SIGCONT);
        fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t)ch_pid);
      }
      // WIFCONTINUED(childStatus);
      // ch_pid = waitpid(0, &childStatus, WCONTINUED | WUNTRACED);
    }

    /* INPUT STEP */
    char *value = getenv("PS1");
    if (value == NULL)
    {
      value = "\0";
    }

    // Do not ignore SIGINT during getline
    sigaction(SIGINT, &SIGINT_action, NULL);
    fprintf(stderr, "%s", value);                // print command prompt - PS1 is used - $ for user # for root user
    ssize_t read = getline(&line, &size, stdin); // Read a line of input from stdin using getline()

    // handle EOF
    if (feof(stdin))
    {
      fprintf(stdin, "%s\n", "EOF found. Continuing.");
      fe_stat = 1;
      goto end;
    }
    // if getline fails
    if (read == -1)
    {
      errno = 0;
      clearerr(stdin);
      goto end;
    }

    sigaction(SIGINT, &ignore_action, NULL);
    // line[strcspn(line, "\n")] = 0;  // if getline is successful, remove the \n char

    /* WORD SPLITTING */
    char *wrds[550] = {NULL}; // min of 512 words must be supported. // TODO figure out a non-hardcoded implementation - tried strlen()
    char *ifs_c = getenv("IFS");
    int num_wrds = 0;
    char *wrd_ptr;

    if (ifs_c == NULL)
    {
      ifs_c = " \t\n";
    }

    wrd_ptr = strtok(line, ifs_c); // get the first token
    if (wrd_ptr == NULL)
    {
      goto end;
    }

    while (wrd_ptr != NULL)
    { // loop through the string to split up the words
      wrds[num_wrds] = strdup(wrd_ptr);
      num_wrds++;
      wrd_ptr = strtok(NULL, ifs_c);
    }

    /* EXPANSION */
    size_t n = 2;
    char *wrd;
    char *home_req = "~/";
    char *pid_req = "$$";
    char *f_req = "$?";
    char *b_req = "$!";
    char *home_char = "~";
    char *home = getenv("HOME");
    char *ret;
    int pid;
    char *fground;

    for (int i = 0; i < num_wrds; i++)
    {
      wrd = wrds[i];
      // HOME EXPANSION - phone home!!
      if (strncmp(wrd, home_req, n) == 0)
      {
        if (home == NULL)
        {
          home = "";
        }
        {
          ret = str_gsub(&wrd, home_char, home); // REFACTOR - put this chunk in a func and call when expanding to reduce repetive code
          if (!ret)
            exit(1);
          wrd = ret;
          wrds[i] = wrd;
        }
      }

      // PID EXPANSION
      if (strstr(wrd, pid_req) != NULL)
      { // REFACTOR - bc str_gsub also uses strstr it would be good to save this result and then pass it to the func
        //  and eleminate the call to strstr in str_gsub to improve efficiency
        pid = getpid();
        char pid_str[50];
        sprintf(pid_str, "%d", pid);
        ret = str_gsub(&wrd, pid_req, pid_str);
        if (!ret)
          exit(1);
        wrd = ret;
        wrds[i] = wrd;
      }

      // FORGROUND COMMAND EXIT STATUS EXPANSION
      if (strstr(wrd, f_req) != NULL)
      {
        char fe_str[50];
        sprintf(fe_str, "%d", fe_stat);
        fground = fe_str;
        ret = str_gsub(&wrd, f_req, fground);

        if (!ret)
          exit(1);
        wrd = ret;
        wrds[i] = wrd;
      }

      // BACKGROUND PID EXPANSION
      if (strstr(wrd, b_req) != NULL)
      {
        char *bground = bg_pid;
        char *ret = str_gsub(&wrd, b_req, bground);
        if (!ret)
          exit(1);
        wrd = ret;
        wrds[i] = wrd;
      }
    }

    /* PARSING */
    int end = 0;
    wrd = wrds[end];
    char *cmnt_char = "#";
    char *is_amp = "&";
    char *l_char = "<";
    char *g_char = ">";
    int insp_pt;           // inspection point
    int run_b = 0;         // if "&" is last word run_b will be set to 1 and command will be run in the background
    char *in_file = NULL;  // if "<" is found, in_file keeps track of the filename's location in wrds
    char *out_file = NULL; // if ">" is found, out_file keeps track of the fileename's location in wrds

    // Iterate over the wordlist forwards until reaching the end (Null) or a "#"
    while (strcmp(wrd, cmnt_char) != 0 && end < num_wrds)
    { // REFACTOR THIS WHOLE THING - also might want to change end_ptr to compare to NULL
      end += 1;
      if (end < num_wrds)
      {
        wrd = wrds[end];
      }
    }
    wrds[end] = NULL; // comments are ignored

    end = end - 1; // want to preserve end for execution phase
    insp_pt = end;

    // look backwards for a "&"
    if (strcmp(is_amp, wrds[end]) == 0)
    {
      run_b = run_b + 1;
      wrds[end] = NULL;
      insp_pt -= 1;
      end -= 1;
    }
    // Then look two steps back looking for "<" and ">"
    if (insp_pt > 0)
    {
      int cmp = strcmp(wrds[insp_pt - 1], l_char);
      if (cmp == 0)
      {
        in_file = wrds[insp_pt]; // by default in_file has a NULL value
        wrds[insp_pt - 1] = NULL;
        end = insp_pt - 1;
        if (insp_pt > 2)
        {
          if (strcmp(wrds[insp_pt - 3], g_char) == 0)
          {
            out_file = wrds[insp_pt - 2]; // out_file default is NULL
            wrds[insp_pt - 3] = NULL;
            end = insp_pt - 3;
          }
        }
      }
      else
      {
        // Then look two steps back looking for ">" and "<"
        if (strcmp(wrds[insp_pt - 1], g_char) == 0)
        {
          out_file = wrds[insp_pt]; // by default in_file has a NULL value
          wrds[insp_pt - 1] = NULL;
          end = insp_pt - 1;
          if (insp_pt > 2)
          {
            if (strcmp(wrds[insp_pt - 3], l_char) == 0)
            {
              in_file = wrds[insp_pt - 2]; // out_file default is NULL
              wrds[insp_pt - 3] = NULL;
              end = insp_pt - 3;
            }
          }
        }
      }
    }

    /* EXECUTION */
    char *ex_cmnd = "exit";
    char *cd_cmnd = "cd";
    int int_arg = 0;
    char *_cwd;
    char *slash = "/";
    char dir[50];
    // check for exit
    if (strcmp(ex_cmnd, wrds[0]) == 0)
    {
      // arg must be an int
      if (wrds[2] != NULL)
      {
        fprintf(stderr, "only one numrical argument can be given to exit\n");
        goto end;
      }
      if (wrds[1] != NULL)
      {
        if (sscanf(wrds[1], "%d", &int_arg) == 0)
        { // exit process id num is saved in int_arg
          fprintf(stderr, "at least one numrical argument must be given to exit\n");
          goto end;
        }
        fprintf(stderr, "%s", "\nexit\n");
        // printf("%d\n", int_arg);
        //  Exit with the implied or specified value
        //  all child processes in the same process group shall be sent a SIGINT signal before exiting (no need to wait)
        kill(getpid(), SIGINT);
        for (int i = 0; i < num_wrds; i++)
        {
          free(wrds[i]);
        }
        exit(int_arg);
      }
      else
      {
        // Expansion of $?
        char str[50];
        sprintf(str, "%d", fe_stat);
        char *ret = str_gsub(&wrd, "$?", str);
        if (!ret)
          exit(1);
        wrd = ret;
        wrds[0] = wrd;
        free(wrd);
      }
    }

    // check for cd
    if (strcmp(cd_cmnd, wrds[0]) == 0)
    {
      if (wrds[2] != NULL)
      { // case: two args were given
        // TODO set up error message and exit
        fprintf(stderr, "only one arg can be given to cd\n");
        goto end;
      }
      else if (wrds[1] != NULL)
      { // case: one arg was given
        // get cwd and concatenate with "/" and input command
        _cwd = getcwd(dir, sizeof(dir));
        strcat(_cwd, slash);
        strcat(_cwd, wrds[1]);
        if (chdir(_cwd) == 0)
        {
          goto end;
        }
        else
        {
          fprintf(stderr, "could not change directory\n"); // case: change directory failed
          goto end;
        }
      }
      else
      {
        char *home = getenv("HOME"); // case: zero args were given
        if (chdir(home) == 0)
        {
          goto end;
        }
        else
        {
          fprintf(stderr, "could not change directory\n"); // case: change directory failed
          goto end;
        }
        wrds[1] = home;
      }
    }

    // Non built ins are executed in a child process via fork()
    pid_t spawnPid = fork();
    switch (spawnPid)
    {
    case -1:
      perror("fork()\n");
      exit(1);
      break;
    case 0:
      sigaction(SIGINT, &old_act, NULL);
      // In the Chilc Process
      // printf("CHILD(%d) running\n", getpid());
      // case where only infile is given
      if (in_file != NULL && out_file == NULL)
      {
        int sourceFD = open(in_file, O_RDONLY);
        if (sourceFD == -1)
        {
          perror("source open()");
          exit(1);
        }
        int result = dup2(sourceFD, 0);
        close(sourceFD);
        if (result == -1)
        {
          perror("source dup2()");
          exit(2);
        }

        execvp(wrds[0], wrds);
        return (0);
      }
      // case where only outfile is given
      if (in_file == NULL && out_file != NULL)
      {
        int targetFD = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (targetFD == -1)
        {
          perror("open()");
          exit(1);
        }
        int result = dup2(targetFD, 1);
        close(targetFD);
        if (result == -1)
        {
          perror("dup2");
          exit(2);
        }
        execvp(wrds[0], wrds);
        return (0);
      }
      // case where both infie and outfile are given
      if (in_file != NULL && out_file != NULL)
      {
        int sourceFD = open(in_file, O_RDONLY);
        if (sourceFD == -1)
        {
          perror("source open()");
          exit(1);
        }
        int result = dup2(sourceFD, 0);
        if (result == -1)
        {
          perror("source dup2()");
          exit(2);
        }
        close(sourceFD);
        int targetFD = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0777);
        if (targetFD == -1)
        {
          perror("target open()");
          exit(1);
        }
        result = dup2(targetFD, 1);
        close(targetFD);
        if (result == -1)
        {
          perror("target dup2()");
          exit(2);
        }
        execvp(wrds[0], wrds);
        return (0);
      }

      // Check that the command doesn't include a "/"
      if (strchr(wrds[0], 47) == NULL)
      {
        execvp(wrds[0], wrds);
        fflush(stdout);
        // fprintf(stderr, "%s\n", "error running execvp");
        exit(2);
        break;
      }

    default:
      if (run_b == 0)
      {
        spawnPid = waitpid(spawnPid, &childStatus, 0);
        if (WIFEXITED(childStatus))
        {
          // printf("child %d exited normally with status %d\n", spawnPid,
          WEXITSTATUS(childStatus);
          // fflush(stdout);
          fe_stat = WEXITSTATUS(childStatus);
        }
        if (WIFSIGNALED(childStatus))
        {
          printf("child exited abnormally");
          int sig = WTERMSIG(childStatus);
          fe_stat = 128 + sig;
        }
        if (WIFSTOPPED(childStatus))
        {
          printf("stopped");
          // send SIGCONT signal`
          kill(spawnPid, SIGCONT); // Might also use WTERMSIG(childStatus) later if this doesn't quite work
          fprintf(stderr, "Child process %jd stopped. Continuing. \n", (intmax_t)spawnPid);
          char str[50];
          sprintf(str, "%d", spawnPid);
          bg_pid = str;
        }
      }
      else
      {
        char str[50];
        sprintf(str, "%jd", (intmax_t)spawnPid);
        bg_pid = str;
      }
      break;
    }

  end:
    continue;
  }
  return 0;
}

/* GENERIC STRING SEARCH AND REPLACE FUNCTION FROM COURSE LECTURE */
char *str_gsub(char *restrict *restrict haystack, char const *restrict needle, char const *restrict sub)
{
  char *str = *haystack;
  size_t haystack_len = strlen(str);
  size_t const needle_len = strlen(needle),
               sub_len = strlen(sub);

  for (; (str = strstr(str, needle));)
  {
    ptrdiff_t off = str - *haystack;
    if (sub_len > needle_len)
    {
      str = realloc(*haystack, sizeof **haystack * (haystack_len * sub_len - needle_len + 1));
      if (!str)
        goto exit;
      *haystack = str;
      str = *haystack + off;
    }
    memmove(str + sub_len, str + needle_len, haystack_len + 1 - off - needle_len);
    memcpy(str, sub, sub_len);
    haystack_len = haystack_len + sub_len - needle_len;
    str += sub_len;
  }
  str = *haystack;
  if (sub_len < needle_len)
  {
    str = realloc(*haystack, sizeof **haystack * (haystack_len + 1));
    if (!str)
      goto exit;
    *haystack = str;
  }
exit:
  return str;
}
