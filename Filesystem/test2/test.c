#include "kvstore.h"
#define RUNS 10
#define RD_RUNS 500
#define VERBOSE 0
#define INPUT "100KB.txt"

int main()
{
    // Open INPUT for testing on input text
    char *value = NULL;
    long length;
    FILE *f = fopen(INPUT, "rb");
    if (f)
    {
        fseek(f, 0, SEEK_END);
        length = ftell(f);
        fseek(f, 0, SEEK_SET);
        value = malloc(length);
        if (value)
            fread(value, 1, length, f);
        fclose(f);
    }
    else
    {
        printf("Could not open file for reading\n");
        return -1;
    }

    if (!value)
    {
        printf("Input file: %s missing\n", INPUT);
        free(value);
        return -1;
    }

    char *obtained = (char*) malloc(length);
    char key[32], keyp[32];
    sprintf(key, "testfile");
    sprintf(keyp, "newtestfile");

    // Testing for RUNS no. of runs
    printf("Testing file text.txt of length %ld\n", length);
    int fail = 0;
    for (int i = 1; i <= RUNS; i++)
    {
        printf("Run: %d\n", i);
        // Clean earlier runs, testing lookup and delete
        if (lookup_key(key) > 0)
            delete_key(key);
        if (lookup_key(keyp) > 0)
            delete_key(keyp);

        // Testing write
        if (put_key(key, value, length) < 0)
        {
            printf("ERROR: Could not write key during run: %d\n", i);
            break;
        }

        // Testing rename
        if (rename_key(key, keyp) < 0)
        {
            printf("ERROR: Could not rename key during run: %d\n", i);
            break;
        }

        // Testing read for RD_RUNS no. of runs
        for (int j = 1; j <= RD_RUNS; j++)
        {
            printf("\rRead run: %d", j);
            if (get_key(keyp, obtained) < 0)
            {
                printf("\nERROR: Could not read key during run: %d, read run: %d", i, j);
                fail = 1;
                break;
            }
            else if (strcmp(value, obtained))
            {
                printf("\nERROR: Obtained value doesn't match initial value during run: %d, read run: %d", i, j);
                if (VERBOSE)
                {
                    printf("\nValue:\n%s\n", value);
                    printf("============================================\n");
                    printf("Obtained:\n%s\n", obtained);
                    printf("============================================\n");
                }
                fail = 1;
                break;
            }
        }
        printf("\n");

        if (fail)
            break;
        if (i == RUNS)
            printf("SUCCESS: Test passed\n");
    }

    free(obtained);
    free(value);
    return fail;
}
