#ifndef _GEMDROID_TRACER_HEADER_H
#define _GEMDROID_TRACER_HEADER_H

#include "qemu-common.h"

/*#ifdef QEMU_ANDROID_QEMULATOR_H
  #define EXTERN
#else
  #define EXTERN extern
#endif*/

#define READ_ACCESS 0
#define WRITE_ACCESS 1

#define DRAM_latency 0

extern int IP_tracer;
extern int MMU_tracer;
extern int CPU_tracer;
extern int ICOUNT_tracer;
extern int twoAts;//prints from softmmu
extern int threeAts;//prints from target-arm/translate
extern int twoCarets;//prints from tcg-target

extern int notInsLoad, notPageTableLookup, _365_softmmu;
extern int current_pid, needed_pid, garb_pid;
extern int needed_tid_length;
extern int needed_tids[1000];
extern int print_tids[1000];
extern char *needed_proc_name, *garbage;
extern char pid_string[10], garb_string[10];
extern char current_pid_path[4096];

//Counters
extern unsigned long mmu1_ld;
extern unsigned long mmu2_ld;
extern unsigned long mmu1_st;
extern unsigned long mmu2_st;
extern unsigned long IO_ld;
extern unsigned long IO_st;

extern unsigned long cache_hits;
extern unsigned long cache_misses;
extern unsigned long this_phase_l1_miss;
extern unsigned long this_phase_l1_access;
extern unsigned long this_phase_l2_miss;

extern unsigned long long total_insts;
extern unsigned long long new_total_insts;
extern unsigned long long cpu_cycles;

extern int64_t curr_gemdroid_tick;

extern int timer_print_flag;
extern int cpu_inst_print_flag;
extern int64_t last_ticks;
extern int first_flag;
extern int miss_status;

extern int init_cache();
extern int check_miss(uint32_t addr, int size, int read_or_write);
#endif
