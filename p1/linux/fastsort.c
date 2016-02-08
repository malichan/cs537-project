#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct lineWrapper {
    char* line;
    char* key;
    int key_length;
} lineWrapper;

int checkArg(char* arg) {
    int length = strlen(arg);        
    if (length < 2 || arg[0] != '-') {
        return -1;
    } else {
        int i = 1;
        for (; i < length; ++i) {
            if (arg[i] < '0' || arg[i] > '9') {
                break;
            }
        }
        if (i < length) {
            return -1;
        } else {
            return atoi(arg + 1) - 1;
        }
    }
}

int wrapLine(char* buffer, lineWrapper* line, int key_index) {
    int length = strlen(buffer);
    int count = 0;
    char last = ' ';
    int begin = 0;
    int end = 0;
    int i = 0;
    for (; i < length; ++i) {
        if (buffer[i] != ' ' && last == ' ') {
            begin = i;
        } else if (buffer[i] == ' ' && last != ' ') {
            end = i;
            if (key_index == count) {
                break;
            }
            count++;
        }
        last = buffer[i];
    }
    if (end <= begin) {
        end = length;
    }

    line->line = buffer;
    line->key = &buffer[begin];
    line->key_length = end - begin;
    return length + 1;
}

int charAtInKey(lineWrapper* line, int index) {
    if (index < line->key_length) {
        return (int)(line->key[index]);
    } else {
        return -1;
    }
}

void exchangeLines(lineWrapper lines[], int i, int j) {
    lineWrapper temp = lines[i];
    lines[i] = lines[j];
    lines[j] = temp;
}

void sortLines(lineWrapper lines[], int low, int high, int index) {
    if (high <= low) {
        return;
    }
    int less_than = low;
    int greater_than = high;
    int char_pivot = charAtInKey(&lines[low], index);
    int pointer = low + 1;
    while (pointer <= greater_than) {
        int char_key = charAtInKey(&lines[pointer], index);
        if (char_key < char_pivot) {
            exchangeLines(lines, less_than++, pointer++);
        } else if (char_key > char_pivot) {
            exchangeLines(lines, pointer, greater_than--);
        } else {
            pointer++;
        }
    }
    sortLines(lines, low, less_than - 1, index);
    if (char_pivot >= 0) {
        sortLines(lines, less_than, greater_than, index + 1);
    }
    sortLines(lines, greater_than + 1, high, index);
}

int main(int argc, char* argv[]) {
    int key_index = 0;
    if (!(argc == 2 || (argc == 3 && (key_index = checkArg(argv[1])) >= 0))) {
        fprintf(stderr, "Error: Bad command line parameters\n");
        return 1;
    }

    char* file_name = argv[argc - 1];
    FILE* file = NULL;
    file = fopen(file_name, "r");
    if (file == NULL) {
        fprintf(stderr, "Error: Cannot open file %s\n", file_name);
        return 1;
    }

    struct stat file_stat;
    fstat(fileno(file), &file_stat);
    int file_size = file_stat.st_size;
    char* buffer = malloc(file_size);
    if (buffer == NULL) {
        fclose(file);
        fprintf(stderr, "malloc failed\n");
        return 1;
    }

    int line_count = 0;
    char* buffer_ptr = buffer;
    char line[130];
    while (fgets(line, 130, file) != NULL) {
        char* newline = strchr(line, '\n');
        if (newline == NULL) {
            fclose(file);
            free(buffer);
            fprintf(stderr, "Line too long\n");
            return 1;
        }
        int line_length = newline - line;
        strncpy(buffer_ptr, line, line_length);
        *(buffer_ptr + line_length) = '\0';
        line_count += 1;
        buffer_ptr += line_length + 1;
    }
    fclose(file);

    lineWrapper* lines = malloc(line_count * sizeof(lineWrapper));
    if (lines == NULL) {
        free(buffer);
        fprintf(stderr, "malloc failed\n");
        return 1;
    }

    int line_index = 0;
    buffer_ptr = buffer;
    for (; line_index < line_count; ++line_index) {
        buffer_ptr += wrapLine(buffer_ptr, &lines[line_index], key_index);
    }

    sortLines(lines, 0, line_count - 1, 0);

    line_index = 0;
    for (; line_index < line_count; ++line_index) {
        printf("%s\n", lines[line_index].line);
    }

    free(buffer);
    free(lines);
    return 0;
}
