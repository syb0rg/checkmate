#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <search.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define TABLE_SIZE 5013
#define ALPHABET_SIZE (sizeof(alphabet) - 1)

char *dictionary = "5k.txt";
const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                        "abcdefghijklmnopqrstuvwxyz"
                        " '";

void *checkedMalloc(size_t len)
{
    void *ret = malloc(len);
    if (!ret)
    {
        fputs("Out of memory!", stderr);
        exit(0);
    }
    return ret;
}

int arrayExist(char **array, int rows, char *word)
{
    for (int i = 0; i < rows; ++i)
    {
        if (!strcmp(array[i], word)) return 1;
    }
    return 0;
}

void arrayCleanup(char **array, int rows)
{
    for (int i = 0; i < rows; i++)
    {
        free(array[i]);
    }
}

ENTRY *find(char *word)
{
    return hsearch((ENTRY){.key = word}, FIND);
}

/**
 * Takes in a file name to open and read into the hast table dict
 *
 * @param fileName Name of file to open
 * @param dict Empty ENTRY that is at least the size of the number of lines in
               the file opened by fileName
 */
int readDictionary(const char* fileName, ENTRY dict)
{
    int fd = open(fileName, O_RDONLY);
    if (fd < 0) return 0;
    
    struct stat sb;
    if (stat(dictionary, &sb)) return 0;
    char *result = strdup(mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (result != MAP_FAILED)
    {
        char *w = NULL;
        char *delimiter = "\n";
        char *word = strtok(result, delimiter);
        for (size_t i = 0; word; ++i)
        {
            if ((w = strdup(word)))
            {
                dict.key  = w;
                // this next line assumes that the dictinary is a frequency
                // list ordered with the most frequent words first
                dict.data = (void*) i;
                if(!hsearch(dict, ENTER))
                {
                    puts("Error adding entry to table");
                    close(fd);
                    exit(-1);
                }
            }
            word = strtok(NULL, delimiter);
        }
        close(fd);
        return 1;
    }
    return -1;
}

/**
 * Takes a part of the source string and appends it to the destination string.
 *
 * @param dst       Destination string to append to.
 * @param dstLen    Current length of the destination string.  This will
 *                  be updated with the new length after appending.
 * @param src       Source string.
 * @param srcBegin  Starting index in the source string to copy from.
 * @param len       Length of portion to copy.
 */
void append(char *dst, int *dstLen, const char *src, int srcBegin, size_t length)
{
    if (length > 0)
    {
        memcpy(&dst[*dstLen], &src[srcBegin], length);
        *dstLen += length;
    }
    dst[*dstLen] = 0;
}

int deletion(char *word, char **array, int start)
{
    int i = 0;
    size_t length = strlen(word);
    
    for (; i < length; i++)
    {
        int pos = 0;
        array[i + start] = checkedMalloc(length);
        append(array[i + start], &pos, word, 0, i);
        append(array[i + start], &pos, word, i + 1, length - (i + 1));
    }
    return i;
}

int transposition(char *word, char **array, int start)
{
    int i = 0;
    size_t length = strlen(word);
    
    for (; i < length-1; i++)
    {
        int pos = 0;
        array[i + start] = checkedMalloc(length + 1);
        append(array[i + start], &pos, word, 0, i);
        append(array[i + start], &pos, word, i + 1, 1);
        append(array[i + start], &pos, word, i, 1);
        append(array[i + start], &pos, word, i + 2, length - (i + 2));
    }
    return i;
}

// bottle-neck #1
int alteration(char *word, char **array, int start)
{
    int k = 0;
    size_t length = strlen(word);
    char c[2] = {};
    
    for (int i = 0; i < length; ++i)
    {
        for (int j = 0; j < ALPHABET_SIZE; ++j, ++k)
        {
            int pos = 0;
            c[0] = alphabet[j];
            array[k + start] = checkedMalloc(length + 1);
            append(array[k + start], &pos, word, 0, i);
            append(array[k + start], &pos, c, 0, 1);
            append(array[k + start], &pos, word, i + 1, length - (i + 1));
        }
    }
    return k;
}

// bottle-neck #2
int insertion(char *word, char **array, int start)
{
    int k = 0;
    size_t length = strlen(word);
    char c[2] = {};
    
    for (int i = 0; i <= length; ++i)
    {
        for (int j = 0; j < ALPHABET_SIZE; ++j, ++k)
        {
            int pos = 0;
            c[0] = alphabet[j];
            array[k + start] = checkedMalloc(length + 2);
            append(array[k + start], &pos, word, 0, i);
            append(array[k + start], &pos, c, 0, 1);
            append(array[k + start], &pos, word, i, length - i);
        }
    }
    return k;
}

size_t totalEdits(char *word)
{
    size_t length = strlen(word);
    
    return (length)                + // deletion
    (length - 1)                   + // transposition
    (length * ALPHABET_SIZE)       + // alteration
    (length + 1) * ALPHABET_SIZE;    // insertion
}

char **edits(char *word)
{
    int index;
    char **array = malloc(totalEdits(word) * sizeof(char*));
    
    if (!array) return NULL;
    
    index  = deletion(word, array, 0);
    index += transposition(word, array, index);
    index += alteration(word, array, index);
    insertion(word, array, index);
    
    return array;
}

char **knownEdits(char **array, int rows, int *e2_rows)
{
    int res_size = 0;
    int res_max  = 0;
    char **res = NULL;
    
    for (int i = 0; i < rows; i++)
    {
        char **e1 = edits(array[i]);
        size_t e1_rows = totalEdits(array[i]);
        
        for (int j = 0; j < e1_rows; j++)
        {
            if (find(e1[j]) && !arrayExist(res, res_size, e1[j]))
            {
                if (res_size >= res_max)
                {
                    // initially allocate 50 entries, after double the size
                    if (res_max == 0) res_max = 50;
                    else res_max *= 2;
                }
                res = realloc(res, sizeof(char*) * res_max);
                res[res_size++] = e1[j];
            }
        }
    }
    
    *e2_rows = res_size;
    
    return res;
}

char *max(char **array, int rows)
{
    char *max_word = NULL;
    int max_size = TABLE_SIZE;
    ENTRY *e;
    for (int i = 0; i < rows; i++)
    {
        e = find(array[i]);
        if (e && ((int) e->data < max_size))
        {
            max_size = (int) e->data;
            max_word = e->key;
        }
    }
    return max_word;
}

char *correct(char *word)
{
    char **e1 = NULL;
    char **e2 = NULL;
    char *e1_word = NULL;
    char *e2_word = NULL;
    char *res_word = word;
    int e1_rows = 0;
    char e2_rows = 0;
    
    if (find(word)) return word;
    
    e1_rows = (unsigned) totalEdits(word);
    if (e1_rows)
    {
        e1 = edits(word);
        e1_word = max(e1, e1_rows);
        
        if (e1_word)
        {
            arrayCleanup(e1, e1_rows);
            free(e1);
            return e1_word;
        }
    }
    
    e2 = knownEdits(e1, e1_rows, (int*)&e2_rows);
    if (e2_rows)
    {
        e2_word = max(e2, e2_rows);
        if (e2_word) res_word = e2_word;
    }
    
    arrayCleanup(e1, e1_rows);
    arrayCleanup(e2, e2_rows);
    
    free(e1);
    free(e2);
    return res_word;
}

int main(int argc, char **argv)
{
//    if (argc != 2)
//    {
//        puts("Usage: ./check <word>");
//        return 1;
//    }
    
    ENTRY dict = {};
    hcreate(TABLE_SIZE);
    
    if (!readDictionary(dictionary, dict)) return -1;
    
//    char *corrected_word = correct(argv[argc - 1]);
//    puts(corrected_word);
    char* correctArray[] = {"baccalaureate", "basketball", "beautiful",
                            "course", "desire", "discotheque","engineering",
                            "English", "examination","example", "favorite",
                            "family", "follow", "finish", "friend", "finally",
                            "gas", "graduate", "have", "holiday", "ideal",
                            "important", "interested", "language", "leisure",
                            "like", "libraries", "masters", "matches",
                            "mechanicals", "prepare", "pretty", "Russian",
                            "second", "secondary", "situated", "sixty", "spent",
                            "snooker", "study", "succeed", "teaching",
                            "university", "week", "with"};
    char* checkArray[] =   {"bacalaureat", "baskett ball", "beautifull",
                            "cours", "desir", "discotec","engeneering",
                            "enlgish", "examinition", "exemple","favrit",
                            "familly", "folow", "finisch", "freind", "finaly",
                            "gaz", "graduat", "hav", "hollyday", "ideale",
                            "importante", "intrested", "langage", "leasure",
                            "luke", "libraries", "mastes", "matchs",
                            "mechanials", "prepar", "prety", "rusian", "secund",
                            "secundry", "situed", "sixthy", "sepent", "snoker",
                            "studie", "succed", "theaching", "univercity",
                            "wik", "whith"};
    int arraySize = sizeof(checkArray)/sizeof(checkArray[0]);
    int correctSum = 0;
    char* guess = NULL;
    for (int i = 0; i < arraySize; ++i)
    {
        guess = correct(checkArray[i]);
        if (!strcmp(correctArray[i], guess))
        {
            printf("Successful correction: got \"%s\" from \"%s\"\n", guess, checkArray[i]);
            ++correctSum;
        }
        else printf("Erroneous correction: expected \"%s\", got \"%s\" from \"%s\"\n", correctArray[i], guess, checkArray[i]);
    }
    printf("Percent correct: %g%%\n", (double) correctSum / arraySize * 100);
}