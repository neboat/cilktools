#ifndef __CILKSAN_H__
#define __CILKSAN_H__

#if defined (__cplusplus)
extern "C" {
#endif

// enclose function definitions with following; use REDUCE_STRAND for
// reduce operation and UPDATE_STRAND for everything else that updates reducer
#define BEGIN_REDUCE_STRAND   __cilksan_begin_reduce_strand(); do
#define END_REDUCE_STRAND     while( __cilksan_end_reduce_strand(), 0 )
#define BEGIN_UPDATE_STRAND   __cilksan_begin_update_strand(); do
#define END_UPDATE_STRAND     while( __cilksan_end_update_strand(), 0 )

// Need these to allow variable declaration within the strand and that is 
// still in scope when the strand annotation ends
#define BEGIN_REDUCE_STRAND_NOSCOPE __cilksan_begin_reduce_strand()
#define END_REDUCE_STRAND_NOSCOPE   __cilksan_end_reduce_strand()
#define BEGIN_UPDATE_STRAND_NOSCOPE __cilksan_begin_update_strand()
#define END_UPDATE_STRAND_NOSCOPE   __cilksan_end_update_strand()

int  __cilksan_error_count(void);
void __cilksan_enable_checking(void);
void __cilksan_disable_checking(void);
void __cilksan_begin_reduce_strand(void);
void __cilksan_end_reduce_strand(void);
void __cilksan_begin_update_strand(void);
void __cilksan_end_update_strand(void);

// This funciton parse the input supplied to the user program
// and get the params meant for cilksan (everything after "--").  
// It return the index in which it found "--" so the user program
// knows when to stop parsing inputs.
int __cilksan_parse_input(int argc, char *argv[]);

// stuff needed in runtime for checking reducer race
int __cilksan_is_running();
int __cilksan_check_for_simulate_steals();
unsigned int __cilksan_check_for_reduce_interval(int);
void __cilksan_invoke_reduce();
// runtime calls this when the parent is stolen and __cilkrts_leave_frame
// does not plan to return
void __cilksan_do_leave_stolen_callback();

#if defined (__cplusplus)
}
#endif

#endif // __CILKSAN_H__
