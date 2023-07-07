#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <sys/wait.h>
#include <fcntl.h>
//https://stackoverflow.com/questions/29788983/split-char-string-with-multi-character-delimiter-in-c/29789623#29789623
//https://brennan.io/2015/01/16/write-a-shell-in-c/

#define MAX_INPUT_SIZE 100
#define MAX_JOBS 100
#define MAX_SIZE 100
#define MAX_ARGS 100
#define MAX_CMD_LENGTH 100

int my_cd(char **);
int my_exit(void);
int my_execute(char **);
int my_process(char **);
int redirect_handler(char **);
int redirect_check(char **);
char* convert_to_string(char**);
//void my_pipe(char **);
//void split_pipe_cmd(char* cmd, char cmds[][101], int* num_cmds)
// Define global variables
int split_args(char **args, char cmds[100][100][100]) ;
void my_pipe(char cmds[100][100][100], int cmd_nums) ;
void my_fg(int job_id);
void remove_job(int job_id);
int arg_index;
int job_id;

struct job {
  //int id;
  pid_t pid;
  char *command;//命令行命令
} jobs[MAX_JOBS];

int num_jobs = 0;

int my_cd(char **args){
    if(arg_index!=2){
        fprintf(stderr, "Error: invalid command\n");
        return 1;
    }
    //chdir is used to change the current working directory.
    if(chdir(args[1])!=0){
        fprintf(stderr, "Error: invalid directory\n");
    }
    return 1;
}

int my_exit(void){
    if(arg_index!=1){
        fprintf(stderr, "Error: invalid command\n");
        return 1;
    }
    return 0;
}

// 定义一个函数，用于将一个字符串数组转换为单个字符串
char* convert_to_string(char** args) {
    char* cmd = (char*)malloc(MAX_SIZE * sizeof(char)); // 声明一个指针，用于存储转换后的字符串
    strcpy(cmd, ""); // 初始化为空字符串

    // 循环遍历字符串数组，将每个字符串依次拼接到cmd中
    int i = 0;
    while (args[i] != NULL) {
        strcat(cmd, args[i]);
        strcat(cmd, " "); // 在每个字符串后面添加一个空格，以便将它们分开
        i++;
    }

    // 去除最后一个空格
    cmd[strlen(cmd) - 1] = '\0';

    return cmd;
}



void add_job(pid_t pid,char **args){
    char * command="";
    if (num_jobs >= MAX_JOBS) {
        fprintf(stderr, "Error: too many jobs\n");
        return;
    }
    jobs[num_jobs].pid = pid;
    command=convert_to_string(args);
    jobs[num_jobs].command = command;
//    printf("I am add_job, the command is %s\n", jobs[num_jobs].command);
    num_jobs++;
}


// This function realizes the input change from [ls -l | wc NULL] to the output [[ls -l NULL],[wc NULL]]
int split_args(char **args, char cmds[100][100][100]) {
    int cmd_index = 0;
    int arg_index = 0;
    int temp=0;

    while(args[arg_index] != NULL) {
        if(strcmp(args[arg_index], "|") == 0) {
            if(arg_index==0){
                fprintf(stderr, "Error: invalid command\n");
                return -1;
            }
            strcpy(cmds[cmd_index][temp], "\0");
            cmd_index++;
            temp=0;
        } else {
            strcpy(cmds[cmd_index][temp++], args[arg_index]);
        }
        arg_index++;
    }
    strcpy(cmds[cmd_index][temp], "\0");
    return cmd_index+1;

}


int my_process(char **args){
    // Fork a child process to run the command
    pid_t pid = fork();
    if (pid == -1) {
      perror("fork");
      exit(1);
    } else if (pid == 0) {
      // Child process
     //If execvp() executes successfully, it will replace the current process, load the specified command and execute it.
      execvp(args[0], args);
        fprintf(stderr, "invalid command\n");
      exit(1);
    } else {
      // Parent process
        //The parent process waits for the child process to complete using wait
      int status;
        pid_t result = waitpid(pid, &status, WUNTRACED);
        if (result == -1) {
            perror("waitpid");
            exit(1);
        } else if (result == 0) {
            // Child is still running
        } else {
            // Child has exited or been stopped
            if (WIFSTOPPED(status)) {
                // Child was stopped
                add_job(pid, args);
            }
            if (WIFSIGNALED(status)){
                add_job(pid, args);
            }
        }
        
    }
    return 1;
}

void my_fg(int job_id) {
   
    if (job_id < 1 || job_id > num_jobs) {
        //printf("%d",num_jobs); 打印出来是2
        //printf("test whether come here");
        fprintf(stderr, "Error: invalid job\n");
        return;
    }
    
    pid_t pid = jobs[job_id - 1].pid;
    int status;

    // Send SIGCONT signal to the job to resume it if it's stopped
    if (kill(pid, SIGCONT) < 0) {
        fprintf(stderr, "Error: invalid job\n");
        return;
    }
    
    // Wait for the job to finish or be stopped
    if (waitpid(pid, NULL, 0) < 0) {
        fprintf(stderr, "wait failed\n");
        return;
    }

    // Restore the shell's control over the terminal
    if (tcsetpgrp(STDIN_FILENO, getpid()) < 0) {
        fprintf(stderr, "restore failed\n");
        return;
    }

    // Remove the job from the job list if it's finished
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
        remove_job(job_id);
    }
    
    remove_job(job_id);
}


void remove_job(int job_id) {
    // printf("i am in remove_job and the job_id is %d\n", job_id);
    
    // 找到指定的作业并删除
    for (int i = job_id - 1; i < num_jobs - 1; i++) {
        jobs[i] = jobs[i+1];
    }
    num_jobs--;
    
}




int checkinput(char **args){
    int i=0;
    //char *cmd;
    char cmds[100][100][100];
    while(args[i]!=NULL){
        if(strcmp(args[i],"|")==0){ //args=["ls", "-l", "|", "wc", "-l"]
            int num_cmd=split_args(args,cmds);
            if(num_cmd==-1){
                return 1;
            }
            my_pipe(cmds, num_cmd);
//            char *cmds_args[100][100];
//                for(int i = 0; i < 100; i++) {
//                    for(int j = 0; j < 100; j++) {
//                        cmds_args[i][j] = NULL;
//                    }
//                }
//
//                for(int i = 0; i < 100; i++) {
//                    int j = 0;
//                    while(cmds[i][j][0] != '\0') {
//                        cmds_args[i][j] = cmds[i][j];
//                        j++;
//                    }
//                    cmds_args[i][j] = NULL;
//                }
            //execvp(cmds_args[1][0],cmds_args[1]);
//            printf("%s",cmds[0][0]);
//            cmd=convert_to_string(args); //cmd = "ls -l | wc -l"
//            printf("%s",cmd);
//            split_pipe_cmd(char* cmd, char cmds[][101], int* num_cmds);
//            my_pipe(cmds[][101]); //cmds=[["ls", "-s", NULL], ["wc", "-l",NULL]]
            
            return 1;
        }
        i++;
    }
    
    // Check for built-in commands  ·····
      if(strcmp(args[0],"cd")==0){
          return my_cd(args);
      }
      else if(strcmp(args[0], "exit")==0){
          // to-do: check whether there exist suspended jobs
          return my_exit();
      }
      else if(strcmp(args[0], "jobs")==0){
        if(args[1]!=NULL){
            fprintf(stderr, "Error: invalid command\n");
            return 1;
        }
        // printf("I am here1");
          // fflush(stdout);
        for(int i=0; i<num_jobs; i++){
           printf("[%d] %s\n", i+1, jobs[i].command);
        }
          return 1;
      }
      else if(strcmp(args[0],"fg")==0){
          if(args[1]==NULL){
              fprintf(stderr, "Error: invalid command\n");
              return 1;
          }
          if(args[2]!=NULL){
              fprintf(stderr, "Error: invalid command\n");
              return 1;
          }
          int job_id = atoi(args[1]);
          my_fg(job_id);
      }
      else if(redirect_check(args)){
          return redirect_handler(args);
      }
    return my_process(args);
}




void my_pipe(char cmds[100][100][100], int num_cmds) {    
//     Create an array of file descriptors for the pipes
    int pipe_fds[num_cmds][2];
    for(int i = 0; i < num_cmds; i++) {
        if(pipe(pipe_fds[i]) < 0) {
            perror("pipe");
            exit(1);
        }
    }
    int pid_arr[10];
    // Create child processes to run each command in the pipeline
    for(int i = 0; i < num_cmds; i++) {
        // pipe(pipe_fds[i]);
        pid_t pid = fork();
        
        if(pid == 0) {
            // Child process code
            if (i == 0) {
                // 第一个命令：将 stdout 重定向到第一个管道的写入端
                dup2(pipe_fds[i][1], STDOUT_FILENO);
            } else if (i == num_cmds-1) {
                // 最后一个命令：将标准输入重定向到最后一个管道的读取端
                dup2(pipe_fds[i - 1][0], STDIN_FILENO);
            } else {
                // Middle command: redirect stdin to the read end of the previous pipe,
                // and redirect stdout to the write end of the current pipe
                dup2(pipe_fds[i - 1][0], STDIN_FILENO);
                dup2(pipe_fds[i][1], STDOUT_FILENO);
            }

            //closes all the pipe ends that are not used by the current child process.
            for(int j = 0; j < num_cmds; j++) {
                if (i==j){
                    continue;
                }
                close(pipe_fds[j][0]);
                close(pipe_fds[j][1]);
            }
            //initializes all elements of the cmds_args array to NULL to ensures that any uninitialized elements do not contain garbage values that could cause errors in the command execution.
            char *cmds_args[100][100];
            for(int j = 0; j < 100; j++) {
                for(int k = 0; k < 100; k++) {
                    cmds_args[j][k] = NULL;
                }
            }
            //使用子进程要执行的命令的参数填充 cmds_args 数组。
            for(int j = 0; j < 100; j++) {
                int k = 0;
                while(cmds[j][k][0] != '\0') {
                    cmds_args[j][k] = cmds[j][k];
                    k++;
                }
                cmds_args[j][k] = NULL;
            }
            checkinput(cmds_args[i]);
            exit(1);
        } else if(pid < 0) {
            // Fork failed
            exit(1);
        }
        //存储在pid_arr数组中,在后面的代码中用于等待所有子进程完成执行
        pid_arr[i]=pid;

    }

    // Close all pipe ends in the parent process
    for(int i = 0; i < num_cmds; i++) {
        close(pipe_fds[i][0]);
        close(pipe_fds[i][1]);
    }

    // Wait for all child processes to finish
    for(int i = 0; i < num_cmds; i++) {
        wait(&pid_arr[i]);
    }
}


int redirect_check(char **args){
  int i=0;
    while(args[i]!=NULL){
      if(strcmp(args[i],">")==0){
        return 1;
      }else if(strcmp(args[i],"<")==0){
        return 1;
      }else if(strcmp(args[i],">>")==0){
        return 1;
      }
      i++;
    }
    return 0;
}

int redirect_handler(char **args) {
    // while(args[i]!=NULL){
    //     printf("%s\n", args[i]);
    //     i++;
    // }
    int fd0 =0, fd1 = 1; 
    // check if redirection exists
    int count=0;
    while (args[count]!=NULL){
        count++;
    }

    if((strcmp(">",args[0])==0)||(strcmp(">>",args[0])==0)||(strcmp("<",args[0])==0)||(strcmp("<<",args[0])==0)||(strcmp(">",args[count-1])==0)||(strcmp(">>",args[count-1])==0)||(strcmp("<",args[count-1])==0)||(strcmp("<<",args[count-1])==0)){
        fprintf(stderr, "Error: invalid command\n");
        return 1;
    }

    for(int i=0; i<count; i++){
        if (strcmp(args[i], ">") == 0) {
            char * filename = args[i+1];
            fd1 = open(filename, O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR | S_IWUSR);
            if (fd1 == -1){
                fprintf(stderr, "Error: invalid file\n");
                return 1;
            }
            args[i] = NULL;  
        } 

        else if (strcmp(args[i], ">>") == 0) {
            char * filename = args[i+1];
            fd1 = open(filename, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
            if (fd1 == -1){
                fprintf(stderr, "Error: invalid file\n");
                return 1;
            }
            args[i] = NULL;  
            
        }  else if (strcmp(args[i], "<") == 0) {
            char * filename = args[i+1];
            fd0 = open(filename, O_RDONLY);
            if (fd0 == -1){
                fprintf(stderr, "Error: invalid file\n");
                return 1;
            }
            args[i] = NULL;  

        } 
    }

    pid_t pid = fork();
    if(pid == -1){
        perror("fork");
        exit(EXIT_FAILURE);
    }else if(pid==0){
        // child process
        if(fd0 != 0){
            if(dup2(fd0, STDIN_FILENO) == -1){
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            close(fd0);
        }
        if(fd1!=1){
            if(dup2(fd1, STDOUT_FILENO) == -1){
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            close(fd1);
        }
        execvp(args[0], args);

    }else{
      // parent process
       wait(NULL);
    }

    return 1;
}


int main(void) {

  char *cwd, *input;
  char prompt[MAX_INPUT_SIZE];
  size_t len = 0;
  ssize_t read;
  int status = 1;

  while (status)
  {
//using getcwd to get the current working directory
    cwd = getcwd(NULL, 0);
//using basename to get the base name of the current directory
//uses snprintf to construct the prompt string and prints it to stdout using printf.
    snprintf(prompt, MAX_INPUT_SIZE, "[nyush %s]$ ", basename(cwd));
    free(cwd);
    printf("%s", prompt);
//fflush(stdout) ensures that the prompt is immediately displayed on the terminal.
    fflush(stdout);

    input = NULL;
//reads input from stdin using getline.
    read = getline(&input, &len, stdin);
    // printf("string is %s", input);
    
    if(read == -1) {
      // End of file reached (e.g. Ctrl-D was pressed)
      break;
    }

    // Remove trailing newline
    if (input[read - 1] == '\n') {
      input[read - 1] = '\0';
      read--;
    }

//The input is parsed into a program and arguments using strtok.
    char *token = strtok(input, " ");
    char *args[MAX_INPUT_SIZE];
    arg_index = 0;

    while (token != NULL) {
      args[arg_index] = token;
      arg_index++;
      token = strtok(NULL, " ");
    }

    if (arg_index == 0) {
      // Empty input
      continue;
    }

    args[arg_index] = NULL;
      
    status = checkinput(args);
    
  }

  return 0;
}


