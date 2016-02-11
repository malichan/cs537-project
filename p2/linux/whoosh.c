#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>

const char* ERROR_MESSAGE = "An error has occurred";
const char* DEFAULT_PATH = "/bin";
const char* STDOUT_POSTFIX = ".out";
const char* STDERR_POSTFIX = ".err";

char* getLine128() {
    static char buffer[130];
    if (fgets(buffer, 130, stdin) == NULL) {
        return NULL;
    }

    char* newline = strchr(buffer, '\n');
    if (newline == NULL) {
        while (fgets(buffer, 130, stdin) != NULL) {
            if (strchr(buffer, '\n') != NULL) {
                break;
            }
        }
        return NULL;
    }

    *newline = '\0';
    return buffer;
}

void createArgList(char* line, int* nargs, char*** args) {
    int _nargs = 0;
    char last = ' ';
    int i = -1;
    while (line[++i] != '\0') {
        if (last == ' ' && line[i] != ' ') {
            ++_nargs;
        }
        last = line[i];
    }

    char** _args = NULL;
    if (_nargs > 0) {
        _args = (char**)malloc((_nargs + 1) * sizeof(char*));
        last = ' ';
        i = -1;
        int k = 0;
        while (line[++i] != '\0') {
            if (last != ' ' && line[i] == ' ') {
                line[i] = '\0';
                last = ' ';
                continue;
            }
            if (last == ' ' && line[i] != ' ') {
                _args[k++] = &line[i];
                last = line[i];
            }
            last = line[i];
        }
        _args[k] = NULL;
    }

    *nargs = _nargs;
    *args = _args;
}

void clearArgList(int* nargs, char*** args) {
    free(*args);
    *nargs = 0;
    *args = NULL;
}

void createPathList(int nargs, char** args, int* npaths, char*** paths) {
    int _npaths = nargs - 1;
    char** _paths = NULL;
    if (_npaths > 0) {
        _paths = (char**)malloc(_npaths * sizeof(char*));
        int i = 0;
        for (; i < _npaths; ++i) {
            _paths[i] = strdup(args[i + 1]);
        }
    }

    *npaths = _npaths;
    *paths = _paths;
}

void clearPathList(int* npaths, char*** paths) {
    int i = 0;
    for (; i < *npaths; ++i) {
        free((*paths)[i]);
    }
    if (*npaths > 0) {
        free(*paths);
    }
    *npaths = 0;
    *paths = NULL;
}

char* findExecutable(char* name, int npaths, char** paths) {
    static struct stat buffer;
    int i = 0;
    for (; i < npaths; ++i) {
        int length = strlen(name) + strlen(paths[i]) + 1;
        char* filename = (char*)malloc((length + 1) * sizeof(char));
        strcpy(filename, paths[i]);
        strcat(filename, "/");
        strcat(filename, name);
        if (stat(filename, &buffer) == 0) {
            return filename;
        }
        free(filename);
    }
    return NULL;
}

int createRedirection(int nargs, char** args, char** out, char** err) {
    int i = 0;
    int index = 0;
    int count = 0;
    for (; i < nargs; ++i) {
        if (strcmp(args[i], ">") == 0) {
            index = i;
            ++count;
        }
    }
    if (count == 0) {
        return 0;
    } else if (count > 1 || index != nargs - 2) {
        return -1;
    } else {
        args[index] = NULL;

        int length_out = strlen(args[index + 1]) + strlen(STDOUT_POSTFIX);
        char* filename_out = (char*)malloc((length_out + 1) * sizeof(char));
        strcpy(filename_out, args[index + 1]);
        strcat(filename_out, STDOUT_POSTFIX);
        int length_err = strlen(args[index + 1]) + strlen(STDERR_POSTFIX);
        char* filename_err = (char*)malloc((length_err + 1) * sizeof(char));
        strcpy(filename_err, args[index + 1]);
        strcat(filename_err, STDERR_POSTFIX);

        int fd_out = open(filename_out, O_CREAT|O_TRUNC|O_WRONLY, S_IRUSR|S_IWUSR);
        int fd_err = open(filename_err, O_CREAT|O_TRUNC|O_WRONLY, S_IRUSR|S_IWUSR);
        close(fd_out);
        close(fd_err);

        if (fd_out < 0 || fd_err < 0) {
            return -1;
        } else {
            *out = filename_out;
            *err = filename_err;
            return 2;
        }
    }
}

void clearRedirection(char** out, char** err) {
    if (*out != NULL) {
        free(*out);
    }
    if (*err != NULL) {
        free(*err);
    }
    *out = NULL;
    *err = NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 1) {
        fprintf(stderr, "%s\n", ERROR_MESSAGE);
        return 1;
    }

    char* command_line = NULL;
    int nargs = 0;
    char** args = NULL;
    int npaths = 1;
    char** paths = (char**)malloc(sizeof(char*));
    paths[0] = strdup(DEFAULT_PATH);
    char* out = NULL;
    char* err = NULL;

    while (1) {
        printf("whoosh> ");
        fflush(stdout);
        if ((command_line = getLine128()) == NULL) {
            fprintf(stderr, "%s\n", ERROR_MESSAGE);
            continue;
        }

        createArgList(command_line, &nargs, &args);

        if (nargs > 0) {
            if (strcmp(args[0], "exit") == 0) {
                clearPathList(&npaths, &paths);
                clearArgList(&nargs, &args);
                exit(0);
            } else if (strcmp(args[0], "pwd") == 0) {
                char* pwd = getcwd(NULL, 0);
                printf("%s\n", pwd);
                free(pwd);
            } else if (strcmp(args[0], "cd") == 0) {
                char* path = nargs > 1? args[1]: getenv("HOME");
                if (chdir(path) != 0) {
                    fprintf(stderr, "%s\n", ERROR_MESSAGE);
                }
            } else if (strcmp(args[0], "path") == 0) {
                clearPathList(&npaths, &paths);
                createPathList(nargs, args, &npaths, &paths);
            } else if (((args[0] = findExecutable(args[0], npaths, paths)) != NULL)
                && (createRedirection(nargs, args, &out, &err) >= 0)) {
                int pid = fork();
                if (pid > 0) {
                    if (wait(NULL) != pid) {
                        fprintf(stderr, "%s\n", ERROR_MESSAGE);
                    }
                } else if (pid == 0) {
                    if (out != NULL && err != NULL) {
                        close(STDOUT_FILENO);
                        open(out, O_CREAT|O_TRUNC|O_WRONLY, S_IRUSR|S_IWUSR);
                        close(STDERR_FILENO);
                        open(err, O_CREAT|O_TRUNC|O_WRONLY, S_IRUSR|S_IWUSR);
                    }

                    execv(args[0], args);

                    fprintf(stderr, "%s\n", ERROR_MESSAGE);
                    free(args[0]);
                    clearRedirection(&out, &err);
                    clearPathList(&npaths, &paths);
                    clearArgList(&nargs, &args);
                    exit(1);
                } else {
                    fprintf(stderr, "%s\n", ERROR_MESSAGE);
                }
                free(args[0]);
                clearRedirection(&out, &err);
            } else {
                fprintf(stderr, "%s\n", ERROR_MESSAGE);
            }
            clearArgList(&nargs, &args);
        }
    }
}