#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const char* ERR_MSG = "An error has occurred\n";
const int ERR_MSG_LEN = 22;

/* Data Structures */

typedef struct listNode {
    struct listNode* next;
    char* value;
} listNode;

typedef struct list {
    listNode* head;
    listNode* tail;
    int size;
} list;

list* createList() {
    list* l = (list*)malloc(sizeof(list));
    l->head = NULL;
    l->tail = NULL;
    l->size = 0;
    return l;
}

void appendToList(list* l, char* v) {
    listNode* node = (listNode*)malloc(sizeof(listNode));
    node->next = NULL;
    node->value = v;
    if (l->tail == NULL) {
        l->head = node;
        l->tail = node;
        l->size = 1;
    } else {
        l->tail->next = node;
        l->tail = node;
        l->size += 1;
    }
}

void deleteList(list* l) {
    listNode* node = l->head;
    while (node != NULL) {
        listNode* next = node->next;
        free(node);
        node = next;
    }
    free(l);
}

/* Utility Functions */

char* getLine128() {
    static char buffer[130];
    if (fgets(buffer, 130, stdin) == NULL) {
        write(STDERR_FILENO, ERR_MSG, ERR_MSG_LEN);
        return NULL;
    }

    char* newline = strchr(buffer, '\n');
    if (newline == NULL) {
        write(STDERR_FILENO, ERR_MSG, ERR_MSG_LEN);
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

void parseArgList(char* line, int* argc, char*** argv) {
    list* token_list = createList();
    char* token = strtok (line, " ");
    while (token != NULL)
    {
        appendToList(token_list, token);
        token = strtok (NULL, " ");
    }

    int my_argc = token_list->size;
    char** my_argv = malloc((my_argc + 1) * sizeof(char*));
    listNode* node = token_list->head;
    int i = 0;
    for (; i < my_argc; ++i) {
        my_argv[i] = strdup(node->value);
        node = node->next;
    }
    my_argv[my_argc] = NULL;
    deleteList(token_list);

    *argc = my_argc;
    *argv = my_argv;
}

void clearArgList(int* argc, char*** argv) {
    int i = 0;
    for (; i < *argc; ++i) {
        free((*argv)[i]);
    }
    free(*argv);
    *argc = 0;
    *argv = NULL;
}

/* Main Program */

int main(int argc, char* argv[]) {
    if (argc != 1) {
        write(STDERR_FILENO, ERR_MSG, ERR_MSG_LEN);
        return 1;
    }

    while (1) {
        printf("whoosh> ");
        char* command_line = getLine128();
        if (command_line == NULL) {
            continue;
        }

        int my_argc = 0;
        char** my_argv = NULL;
        parseArgList(command_line, &my_argc, &my_argv);

        printf("argc = %d\n", my_argc);
        int i = 0;
        for (;i < my_argc; ++i) {
            printf("argv[%d] = %s\n", i, my_argv[i]);
        }

        if (strcmp(my_argv[0], "exit") == 0) {
            clearArgList(&my_argc, &my_argv);
            return 0;
        }

        clearArgList(&my_argc, &my_argv);
    }
}