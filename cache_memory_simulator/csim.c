#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>


typedef struct Line {
    int valid; // valid bit
    unsigned long tag; // tag bits
    int usage; // used to determine LRU line
} Line;

typedef struct Set {
    int E; // number of lines
    Line *lines; // array of cache lines
} Set;

typedef struct Cache {
    long int S; // number of sets
    int t, s, b; // number of tag, set, and block bits
    Set **sets; // array of cache sets
} Cache;

Set *Set_create(int E);
Cache *Cache_create(int s, int E, int b);
void Set_destroy(Set *set);
void Cache_destroy(Cache *cache);

FILE *arg_parse(char **argv, int *s, int *E, int *b);
void simulate(Cache *c, FILE *tr, int *h, int *m, int *e);

int main(int argc, char **argv)
{
    int s, E, b;
    FILE *tr = arg_parse(argv, &s, &E, &b);
    Cache *cache = Cache_create(s, E, b);
    int h, m, e;
    simulate(cache, tr, &h, &m, &e);
    printSummary(h, m, e);
    fclose(tr);
    Cache_destroy(cache);
    return 0;
}

Set *Set_create(int E)
{
    Set *result = malloc(sizeof(Set));
    result->E = E;
    result->lines = malloc(sizeof(Line)*E);

    int i;
    for(i = 0; i < E; i++) {
        result->lines[i].valid = 0;
        result->lines[i].tag = 0;
        result->lines[i].usage = 0;
    }

    return result;
}

Cache *Cache_create(int s, int E, int b)
{
    Cache *result = malloc(sizeof(Cache));
    long int S = (int)pow(2,s);
    result->S = S;
    result->t = 64 - s - b;
    result->s = s;
    result->b = b;
    result->sets = malloc(sizeof(Set *) * S);

    int i;
    for(i = 0; i < S; i++) {
        result->sets[i] = Set_create(E);
    }

    return result;
}

void Set_destroy(Set *set)
{
    free(set->lines);
    free(set);
}

void Cache_destroy(Cache *cache)
{
    int i, S = cache->S;
    Set **sets = cache->sets;
    for(i = 0; i < S; i++) {
        Set_destroy(sets[i]);
    }

    free(sets);
    free(cache);
}

void Cache_print(Cache *cache)
{
    int S = cache->S;
    int i, j;
    for(i = 0; i < S; ++i) {
        Line *lines = cache->sets[i]->lines;
        int E = cache->sets[i]->E;
        for(j = 0; j < E; j++) {
            printf("%d %d %lx\n", i, lines[j].valid, lines[j].tag);
        }
        printf("\n");
    }
}

FILE *arg_parse(char **argv, int *s, int *E, int *b)
{
    *s = atoi(argv[2]);
    *E = atoi(argv[4]);
    *b = atoi(argv[6]);
    FILE *tr = fopen(argv[8], "r");

    return tr;
}

int find_line(Line *lines, int E, unsigned long tag)
{
    int i;
    for(i = 0; i < E; i++) {
        if(lines[i].valid && lines[i].tag == tag) {
            return i;
        }
    }

    return -1;
}

int find_empty_line(Line *lines, int E)
{
    int i;
    for(i = 0; i < E; i++) {
        if(!lines[i].valid) {
            return i;
        }
    }

    return -1;
}

int find_lru_line(Line *lines, int E)
{
    int i, m, max = 0;
    for(i = 0; i < E; i++) {
        int u = lines[i].usage;
        if(u > max) {
            max = u;
            m = i;
        }
    }

    return m;
}

int memory_op(Set *set, unsigned long tag, char *op)
{
    int r = 0;
    int E = set->E;
    Line *lines = set->lines;

    int i = find_line(lines, E, tag);

    if(i == -1) {
        r++;
        i = find_empty_line(lines, E);
        if(i == -1) {
            r++;
            i = find_lru_line(lines, E);
        } else {
            lines[i].valid = 1;
        }
        lines[i].tag = tag;
    }

    // update usage
    int j;
    for(j = 0; j < E; j++) {
        lines[j].usage++;
    }
    lines[i].usage = 0;

    if(op[0] == 'M') {
        r += 3;
    }

    return r;
}

void simulate(Cache *cache, FILE *tr, int *h, int *m, int *e)
{
    int t = cache->t;
    int s = cache->s;
    int b = cache->b;

    int u = 0;
    int v = 0;
    int w = 0;

    char op[2];
    while(fscanf(tr, "%s", op) > 0) {
        if(op[0] == 'I') {
            fscanf(tr, "%*x,%*d\n");
            continue;
        }
        
        unsigned long addr;
        fscanf(tr, "%lx,%*d\n", &addr);


        unsigned long tag = (addr>>(64-t))&((1<<t) - 1);
        unsigned int S = (addr>>b)&((1<<s) - 1);

        Set *set = cache->sets[S];
        int r = memory_op(set, tag, op);

        switch(r) {
            case 0:
                u++;
                break;
            case 1:
                v++;
                break;
            case 2:
                v++;
                w++;
                break;
            case 3:
                u += 2;
                break;
            case 4:
                v++;
                u++;
                break;
            case 5:
                v++;
                w++;
                u++;
                break;
        }
    }

    *h = u;
    *m = v;
    *e = w;
}