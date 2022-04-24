#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/wait.h>

#define maxLineLen 1024
int numBG = 0;

struct bg {
    pid_t pid;
    char command[maxLineLen];
    struct bg* next;
};

struct bg* ROOT = NULL;


/*
Purpose: Take current directory name and name of the
host running the program to print out a prompt like a normal shell.
*/
void print_dir() {
    char* user_name;
    char host_name[maxLineLen];
    char dir[180];

    user_name = getlogin();
    gethostname(host_name, maxLineLen);
    getcwd(dir, sizeof(dir));
    
	printf("%s@%s: %s > ",user_name, host_name, dir);

}

/*
Purpose: prompt the user of this program to input command that will
    be execute.
*/
char* get_command() {
    char *command_line = NULL;
    size_t size = maxLineLen;

    int num_char = getline(&command_line, &size, stdin);

    if (num_char > 1) {
        return command_line;
    } else {
        return NULL;
    }

}

/*
Purpose: To store command from user input into 2D array.
Parameter: char *command from get_command(), 2D array command_arr to
    stored each command.
*/
int split_command(char *command, char command_arr[200][200]) {
    char *command_pointer;
    char splited[200][200];
    int i = 0;

    command_pointer = strtok(command, " \n");
    while (command_pointer != NULL) {
        strcpy(command_arr[i], command_pointer);
        i++;
        command_pointer = strtok (NULL, " \n");
    }

    return i;

}

/*
Purpose: Execute command that are store inside char* parameter using fork()
    and execvp() to execute.
Parameter: char *parameter[maxLineLen] with command storing insde.
*/
void exe_command(char *parameter[maxLineLen]) {
    pid_t pid = fork();

    if (pid < 0) {
        printf("fork() fail\n");
        exit(1);
    }

    if (pid == 0) {
        int status = execvp(parameter[0], parameter);

        if (status == -1) {
            printf("Not a valid command\n");
        }
    } else {
        waitpid(pid, NULL, 0);
        //wait(NULL);
    }

}


/*
Purpose: Take 2D array and move each string of command into char *parameter[maxLineLen].
Parameter: char command_arr[200][200] is the array to move the string from.
    char *parameter[maxLineLen] is the array to move the string to.
    int paraLen is the number of string in command_arr.
*/
void storing(char command_arr[200][200], char *parameter[maxLineLen], int paraLen) {

    for (int i=0; i < paraLen; i++) {
        parameter[i] = command_arr[i];
    }
    parameter[paraLen++] = NULL;

}


/*
Purpose: This fuction enable the use of cd command in simple shell interpreter.
Parameter: parameter contain the cd and argument of cd command.
    cmd_len is the length of command in parameter.
*/
void changeDir(char *parameter[maxLineLen], int cmd_len) {
    int checkErr;
    
    if (cmd_len == 1 || strcmp(parameter[1],"~") == 0) {
        checkErr = chdir(getenv("HOME"));
    } else if (cmd_len == 2) {
        checkErr = chdir(parameter[1]);
    } else {
        printf("Too much argument for cd command\n"); 
    }

    if (checkErr < 0) {
        printf("error changing directory\n");
        exit(0);
    }

}

/*
Purpose: To be able to execute command in the background of the shell.
Parameter:  parameter contain the bg and the command to execute.
    cmd_len is the length of command in parameter.
*/
void exe_bg(char* com, char *parameter[maxLineLen], int cmd_len) {
    char* newPara[maxLineLen];
    struct bg* cur = (struct bg*)malloc(sizeof(struct bg));
    struct bg* last = ROOT;
    int count = 0;
    char newCommand[maxLineLen] = "";
    char* temp;
    int j=0;
    int k=0;
    int cb = maxLineLen * sizeof(char);

    for (int i=0; i<cmd_len-1; i++) {
        newPara[i] = parameter[i+1];
        newPara[i+1] = NULL;
    }

    while (newPara[count] != NULL) {
        strcat(newCommand, newPara[count]);
        strcat(newCommand, " ");
        count++;
    }
    

    pid_t id = fork();
    if (id < 0) {
        printf("fork() fail\n");
        exit(1);
    }


    if (id == 0) {
        int status = execvp(newPara[0], newPara);
        if (status == -1) {
            printf("Not a valid command after bg\n");
        }
    } else {
        if (numBG == 0) {
            cur->pid = id;
            strcpy(cur->command, newCommand);
            ROOT = cur;
            numBG++;
        } else {
            cur->pid = id;
            strcpy(cur->command, newCommand);

            while (last->next != NULL) {
                last = last->next;
            } 
            last->next = cur;
            numBG++;
        }
    }


}

/*
Purpose: print out command that executing in the background
*/
void exe_bglist() {

    struct bg* cur = ROOT;
    int id;
    int count = 0;
    int bgcount = 0;

    
    while (cur != NULL && bgcount <= numBG) {
        id = (int) cur->pid;
        printf("%d:", id);
        printf(" %s", cur->command);
        printf("\n");
        cur = cur->next;

    }
    

    printf("Total Background jobs: %d\n", numBG);

}

/*
Purpose: free background execution that is storing in linked list struct
    when exiting the shell.
*/
void freeBG() {
    struct bg* cur = ROOT;
    struct bg* temp;

    while (cur != NULL) {
        temp = cur;
        cur = cur->next;
        free(temp);
    }

}


/*
Purpose: Check for execulting background termination and
    print what being terminated and remove that command from linked list
*/
void checkTerminated() {
    struct bg* cur = ROOT;
    struct bg* temp;
    int count = 0;

    if (numBG > 0) {
        pid_t ter = waitpid(0, NULL, WNOHANG);
        if (ter == -1) {
            printf("waitpid error\n");
            exit(1);
        }

        while (ter > 0) {
            if (ROOT->pid == ter || numBG == 1) {
                temp = ROOT;
                printf("%d:", ROOT->pid); 
                printf(" %s", ROOT->command);
                printf(" has terminated\n");
                ROOT = ROOT->next;
                free(temp);
                numBG--;
            } else {
                while (cur->next->pid != ter && cur->next != NULL) {
                    cur = cur->next;
                }
                temp = cur->next;
                printf("%d:", cur->next->pid); 
                printf(" %s", cur->next->command);
                printf(" has terminated\n");
                cur->next = cur->next->next;
                free(temp);
                numBG--;
            }
            ter = waitpid(0, NULL, WNOHANG);
        }
    }
}


/*
Purpose: This is the main function of a simple shell interpreter
    Command that can be use are 
        cd - to change directory
        exit - to exit shell
        bg - to execute command in background
        bglist - check background execute
        and command that work with execvp() function like ls.
*/
int main(int argc, char *argv[]) {
    char *command;
    char command_arr[200][200];
    int cmd_len;
    char *parameter[maxLineLen];
    
    while(1) {
        printf("\n");
        print_dir();
        command = get_command();

        if (command != NULL && strcmp(command, "exit\n") == 0) {
            freeBG();
            exit(0);
        } else if (command != NULL) {
            cmd_len = split_command(command, command_arr);
            storing(command_arr, parameter, cmd_len);

            if (strcmp(parameter[0], "cd") == 0) {
                changeDir(parameter, cmd_len);
            } else if (strcmp(parameter[0], "bg") == 0) {
                exe_bg(command, parameter, cmd_len);
            } else if (strcmp(parameter[0], "bglist") == 0) {
                exe_bglist();
            } else {
                exe_command(parameter);
            }

        }

        checkTerminated();  
        command = NULL;
    }

}

