#ifndef INCLUDED_REDUCERTOOL_DOT_H
#define INCLUDED_REDUCERTOOL_DOT_H

#if defined (__cplusplus)
#define EXTERN_C extern "C" {
#define EXTERN_C_END }
#else
#define EXTERN_C
#define EXTERN_C_END
#endif

EXTERN_C

void cilk_begin_reduce_strand(void);
void cilk_end_reduce_strand(void);
void cilk_begin_update_strand(void);
void cilk_end_update_strand(void);

void cilk_set_reducer(void *reducer, void *rip, const char *function, int line);
void cilk_read_reducer(void *reducer, void *rip, const char *function, int line);

// enclose function definitions with following; use REDUCE_STRAND for
// reduce operation and UPDATE_STRAND for everything else that updates reducer
#define BEGIN_REDUCE_STRAND   cilk_begin_reduce_strand(); do
#define END_REDUCE_STRAND     while( cilk_end_reduce_strand(), 0 )
#define BEGIN_UPDATE_STRAND   cilk_begin_update_strand(); do
#define END_UPDATE_STRAND     while( cilk_end_update_strand(), 0 )

// Need these to allow variable declaration within the strand and that is 
// still in scope when the strand annotation ends
#define BEGIN_REDUCE_STRAND_NOSCOPE cilk_begin_reduce_strand()
#define END_REDUCE_STRAND_NOSCOPE   cilk_end_reduce_strand()
#define BEGIN_UPDATE_STRAND_NOSCOPE cilk_begin_update_strand()
#define END_UPDATE_STRAND_NOSCOPE   cilk_end_update_strand()

EXTERN_C_END

#endif
