#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#include "atomic.h"

struct Crb
{
    uint32_t curr;
    uint32_t size;
    uint32_t msize;
    void**   data;
};

typedef struct Crb Crb_t;

static inline bool 
is_power_of_2(uint32_t n)
{
    return (n != 0 && ((n & (n - 1)) == 0));
}

static inline uint32_t 
min(uint32_t a, uint32_t b)
{
    return a > b ? b : a;
}

static inline uint32_t 
max(uint32_t a, uint32_t b)
{
    return a > b ? a : b;
}

static inline 
uint32_t roundup_power_of_2(uint32_t a)
{
    if (is_power_of_2(a))
        return a;

    uint32_t position = 0;
    int i;
    for (i = a; i != 0; i >>= 1)
        position++;

    return (uint32_t)(1 << position);
}

Crb_t* 
crb_creat(uint32_t size, uint32_t msize)
{
    Crb_t* crb_ptr = (Crb_t*) malloc(sizeof(Crb_t));
    size_t i = 0;
    crb_ptr->curr = 0;
    crb_ptr->msize = msize;
    crb_ptr->size = roundup_power_of_2(size);
    crb_ptr->data = calloc(sizeof(void *), crb_ptr->size);
    for (i = 0; i < crb_ptr->size; i++) {
        crb_ptr->data[i] = malloc(crb_ptr->msize);
    }
    return crb_ptr;
}

bool 
is_crb_fill(Crb_t* rcb)
{
    return rcb->curr >= rcb->size;
}

bool 
is_crb_empoty(Crb_t* rcb)
{
    return rcb->curr == 0;
}

uint32_t 
crb_set(Crb_t* rcb, void* data )
{
    uint32_t curr = ATOM_FINC(&rcb->curr);
    uint32_t index = curr & (rcb->size - 1);
    memcpy(rcb->data[index], data, rcb->msize);
    return curr;
}

int 
crb_get_curr(Crb_t* rcb, void* data)
{
    uint32_t curr = rcb->curr - 1;
    uint32_t index = curr & (rcb->size - 1);
    memcpy(data, rcb->data[index], rcb->msize);
    return 0;
}

int 
crb_get_offset(Crb_t* rcb, void* data, uint32_t offset)
{
    uint32_t curr = rcb->curr;
    uint32_t index = (curr - offset) & (rcb->size - 1);
    if (offset >= rcb->size) {
        return -1;
    }
    if (curr < rcb->size && curr < offset) {
        return -1;
    }
    memcpy(data, rcb->data[index], rcb->msize);
    return 0;
}

typedef struct stat_cputime_t
{
    uint64_t user;        /* normal processes executing in user mode */      
    uint64_t nice;        /* niced processes executing in user mode */
    uint64_t system;      /* processes executing in kernel mode */   
    uint64_t idle;         
    uint64_t iowait;      /* waiting for I/O to complete */   
    uint64_t irq;         /* servicing interrupts */ 
    uint64_t softirq;     /* servicing softirqs */   
    uint64_t steal;       /* involuntary wait */
    uint64_t guest;       /* running a normal guest */  
    uint64_t guest_nice;  /* running a niced guest */
    struct timeval tv;
} stat_cputime_t;

typedef struct cpu_usagetime_t
{  
    uint64_t total;
    uint64_t load;
    uint64_t idle;
    uint64_t io;
    uint64_t system;
    uint64_t user;
    uint64_t irq;
    uint64_t guest;
    struct timeval tv;
} cpu_usagetime_t;

int 
read_cpu_stat(stat_cputime_t *stat) 
{
    FILE* fp = NULL;
    char line[1024] = {0};

    fp = fopen("/proc/stat", "r");
    if (NULL == fp) {
        printf("open file /proc/stat error\n");
        return -1;
    }
    fgets(line, sizeof(line), fp);
    fclose(fp);

    if (sscanf(line, "cpu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
             &stat->user,
             &stat->nice,
             &stat->system,
             &stat->idle,
             &stat->iowait,
             &stat->irq,
             &stat->softirq,
             &stat->steal,
             &stat->guest,
             &stat->guest_nice) < 10) {
            return -1;
    }
    gettimeofday(&stat->tv, NULL);
    return 0;
}

int 
get_cpu_usagetime(cpu_usagetime_t *usage)
{   
    stat_cputime_t stat;
    int rc = read_cpu_stat(&stat);
    if (rc < 0) {
        return -1;
    }
    cpu_usagetime_convrt(&stat, usage);
}

static inline uint64_t 
cputime_get_total(stat_cputime_t *time) 
{ 
    return time->user + time->nice + time->system      
              + time->idle + time->iowait + time->irq + time->softirq      
              + time->steal + time->guest + time->guest_nice;
}

static inline uint64_t 
cputime_get_load(stat_cputime_t *time) 
{  
    return time->user + time->nice + time->system      
              + time->irq + time->softirq     
              + time->steal + time->guest + time->guest_nice;
}

static inline uint64_t 
cputime_get_idle(stat_cputime_t *time) 
{  
    return time->idle + time->iowait;
}

static inline uint64_t 
cputime_get_iowait(stat_cputime_t *time) 
{  
    return time->iowait;
}

static inline uint64_t 
cputime_get_system(stat_cputime_t *time) 
{ 
    return time->system;
}

static inline uint64_t 
cputime_get_user(stat_cputime_t *time) 
{ 
    return time->user + time->nice;
}

static inline uint64_t 
cputime_get_irq(stat_cputime_t *time)
{  
    return time->irq + time->softirq;
}
    
static inline uint64_t 
cputime_get_guest(stat_cputime_t *time)
{  
    return time->guest + time->guest_nice;
}

static void 
cal_cpu_usage_result(cpu_usagetime_t *b, cpu_usagetime_t *a, int delay) 
{
    uint64_t total  = a->total  -   b->total;
    uint64_t load   = a->load   -   b->load;
    uint64_t irq    = a->irq    -   b->irq;
    uint64_t system = a->system -   b->system;
    uint64_t user   = a->user   -   b->user;
    uint64_t io     = a->io     -   b->io;
    uint64_t guest  = a->guest  -   b->guest;
    uint64_t idle   = a->idle   -   b->idle;

    float usage = (float) load * 100 / total;

    char buf[1024] = {0};
    int buf_len = sizeof(buf);

    snprintf(buf, buf_len, "%5.2f%% in last %3ds "
                           "(total:%-5lu idle:%-5lu load:%-5lu io:%-5lu system:%-4lu user:%-5lu irq:%-5lu guest:%-4lu)\n",
            usage, delay, 
            total, idle, load, io, system, user, irq, guest);
    printf("%s\n", buf);
}

void 
cpu_usagetime_convrt(stat_cputime_t* stat, cpu_usagetime_t *usage)
{
    usage->total = cputime_get_total(stat);
    usage->load = cputime_get_load(stat);
    usage->idle = cputime_get_idle(stat);
    usage->io = cputime_get_iowait(stat);
    usage->system = cputime_get_system(stat);
    usage->irq = cputime_get_irq(stat);
    usage->guest = cputime_get_guest(stat);
    usage->user = cputime_get_user(stat);
    memcpy(&usage->tv, &stat->tv, sizeof(stat->tv));
}

static float 
cal_cpu_usage(cpu_usagetime_t *b, cpu_usagetime_t *a) 
{   
    uint64_t total  = a->total  -   b->total;
    uint64_t load   = a->load   -   b->load;
    if (total == 0) {
        return -1.0;
    }
    return (float)load / total * 100;
}

int main(int argc, char const *argv[])
{
    cpu_usagetime_t a, b;
    Crb_t* crb = crb_creat(60, sizeof(cpu_usagetime_t));
    while (1) {
        get_cpu_usagetime(&a);
        crb_set(crb, &a);
        if (crb_get_offset(crb, &b, 4) == 0) {
            cal_cpu_usage_result(&b, &a, 2);
        }
        usleep(500 * 1000);
    }
    return 0;
}
