/* Shuffled array of pointers. Build: gcc -O2 -static -fno-pie -no-pie pointer.c */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct node { uint64_t left, value, right, padding[5]; };
#define MAX_ENTRIES (1u << 16)
static _Alignas(64) struct node nodes[MAX_ENTRIES];
static _Alignas(64) unsigned char storage[MAX_ENTRIES * sizeof(uint64_t *) + 128];
static size_t order[MAX_ENTRIES];
extern uint64_t visit(uint64_t **, size_t, intptr_t, int, uint64_t *);
/* Exact symbols permit checking the actual load PCs, independently of compiler
 * scheduling. The independent control overwrites the producer register. */
__asm__(
".text\n.global visit\n.type visit,@function\nvisit:\n"
" cmp $2,%ecx\n jne 2f\n mov %r8,(%rdi,%rsi,8)\n2:\n"
".global p1_producer\np1_producer:\n mov (%rdi,%rsi,8), %r10\n"
" cmp $1,%ecx\n jne 1f\n mov %r8,%r10\n1:\n"
".global p1_dependent\np1_dependent:\n mov (%r10,%rdx),%rax\n ret\n"
".size visit,.-visit\n");
static uint64_t random_state = 0x552;
static uint64_t random64(void) {
    random_state ^= random_state << 13;
    random_state ^= random_state >> 7;
    random_state ^= random_state << 17;
    return random_state;
}
int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "positive";
    size_t n = argc > 2 ? strtoull(argv[2], 0, 0) : 8192;
    size_t rounds = argc > 3 ? strtoull(argv[3], 0, 0) : 8;
    if (!n || n > MAX_ENTRIES || !rounds || rounds > 10000 ||
        (strcmp(mode,"positive") && strcmp(mode,"negative") &&
         strcmp(mode,"zero") && strcmp(mode,"varying") && strcmp(mode,"independent") &&
         strcmp(mode,"split") && strcmp(mode,"forwarded"))) {
        fprintf(stderr,"usage: pointer positive|negative|zero|varying|independent|split|forwarded [n] [rounds]\n");
        return 2;
    }
    // Static storage avoids host-libc allocator/SE compatibility dependencies.
    uint64_t **pointers =
        (uint64_t **)(storage + (!strcmp(mode,"split") ? 60 : 0));
    for (size_t i=0; i<n; ++i) {
        nodes[i].left=3*i+1; nodes[i].value=3*i+2; nodes[i].right=3*i+3;
        order[i]=i;
    }
    for (size_t i=n-1; i; --i) {
        size_t j=random64()%(i+1), tmp=order[i]; order[i]=order[j]; order[j]=tmp;
    }
    for (size_t i=0; i<n; ++i) {
        uint64_t *ptr = &nodes[order[i]].value;
        memcpy((unsigned char *)pointers + i * sizeof(ptr), &ptr, sizeof(ptr));
    }
    uint64_t sum=0, expected=0;
    int independent=!strcmp(mode,"independent");
    int forwarded=!strcmp(mode,"forwarded");
    for (size_t r=0; r<rounds; ++r) {
        for (size_t i=0; i<n; ++i) {
            intptr_t offset=!strcmp(mode,"negative") ? -8 :
                !strcmp(mode,"zero") ? 0 :
                !strcmp(mode,"varying") ? (i%2 ? -8 : 8) : 8;
            sum+=visit(pointers,i,offset,forwarded ? 2 : independent,&nodes[i].value);
            size_t index=(independent || forwarded) ? i : order[i];
            expected+=3*index+2+offset/8;
        }
    }
    printf("pointer mode=%s n=%zu rounds=%zu checksum=%" PRIu64 " expected=%" PRIu64 "\n",
           mode,n,rounds,sum,expected);
    return sum != expected;
}
