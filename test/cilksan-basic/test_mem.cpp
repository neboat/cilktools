#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <assert.h>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include "cilksan.h" 

typedef union data {
  int i;
  double d;
  char str[12];
} data_t;

data_t global_data; 
int global_int = 0;
double global_double = 0.0;
char global_str[12];

static void init(data_t *data) {
  data->i = 0;
  data->d = 0.5;
  for(int i=0; i < 12; i++) {
    data->str[i] = 'a'; 
  }
}

void increment_i(data_t *data) {
  data->i++;
} 

void read_i(data_t *data) {
  global_int += data->i;
} 

void mult_double(data_t *data) {
  data->d *= 2.0;
}

void read_double(data_t *data) {
  global_double += data->d;
} 

void update_str(data_t *data, int begin, int end) {
  for(int i=begin; i < end; i++) {
    data->str[i] = (char)('a' + i);
  } 
}

void read_str(data_t *data, int begin, int end) {
  for(int i=begin; i < end; i++) {
    global_str[i] = data->str[i];
  } 
}

void read_and_write(data_t *data) {
  cilk_spawn read_double(data);
  increment_i(data);
  cilk_sync;
}


int main(int argc, char* argv[]) {

  int which = 0;

  data_t *heap_data = (data_t *)malloc(sizeof(data_t));

  if (argc >= 2) {
    which = atoi(argv[1]);
  }
  init(&global_data);     
  init(heap_data);     

  switch(which) {
    case 0:
      cilk_spawn increment_i(&global_data); // write, read race
      mult_double(&global_data);
      cilk_sync;
      assert(__cilksan_error_count() == 3);
      break;

    case 1:
      cilk_spawn mult_double(&global_data);
      increment_i(&global_data); // write, read race
      cilk_sync;
      assert(__cilksan_error_count() == 3);
      break;

    case 2:
      cilk_spawn mult_double(&global_data);
      update_str(&global_data, 3, 8); // read, write race
      cilk_sync;
      assert(__cilksan_error_count() == 2);
      break;

    case 3:
      cilk_spawn increment_i(&global_data); // write, read race
      read_double(&global_data);
      cilk_sync;
      assert(__cilksan_error_count() == 1);
      break;

    case 4:
      cilk_spawn read_double(&global_data);
      increment_i(&global_data); // read, write race
      cilk_sync;
      assert(__cilksan_error_count() == 1);
      break;

    case 5:
      cilk_spawn read_double(&global_data);
      update_str(&global_data, 3, 8); // read, write race
      cilk_sync;
      assert(__cilksan_error_count() == 1);
      break;

    case 6:
      cilk_spawn read_str(&global_data, 2, 4);
      update_str(&global_data, 3, 8); // read, write race
      cilk_sync;
      assert(__cilksan_error_count() == 1);
      break;

    case 7:
      cilk_spawn increment_i(heap_data); // write, read race
      mult_double(heap_data);
      cilk_sync;
      assert(__cilksan_error_count() == 3);
      break;

    case 8:
      cilk_spawn mult_double(heap_data);
      increment_i(heap_data); // write, read race
      cilk_sync;
      assert(__cilksan_error_count() == 3);
      break;

    case 9:
      cilk_spawn mult_double(heap_data);
      update_str(heap_data, 3, 8); // write, write race
      cilk_sync;
      assert(__cilksan_error_count() == 2);
      break;

    case 10:
      cilk_spawn increment_i(heap_data); // write, read race
      read_double(heap_data);
      cilk_sync;
      assert(__cilksan_error_count() == 1);
      break;

    case 11:
      cilk_spawn read_double(heap_data);
      increment_i(heap_data); // read, write race
      cilk_sync;
      // to be sure that compiler doesn't optimize it away.
      global_int = heap_data->i; 
      assert(__cilksan_error_count() == 1);
      break;

    case 12:
      cilk_spawn read_double(heap_data);
      update_str(heap_data, 3, 8); // read, write race
      cilk_sync;
      assert(__cilksan_error_count() == 1);
      break;

    case 13:
      cilk_spawn update_str(heap_data, 8, 11);
      read_str(heap_data, 10, 12); // write, read race
      cilk_sync;
      assert(__cilksan_error_count() == 1);
      break;

    case 14:
      cilk_spawn update_str(&global_data, 8, 12); // no race
      mult_double(&global_data);
      cilk_sync;
      assert(__cilksan_error_count() == 0);
      break;

    case 15:
      cilk_spawn update_str(&global_data, 4, 12); // no race
      read_i(&global_data);
      cilk_sync;
      assert(__cilksan_error_count() == 0);
      break;

    case 16:
      cilk_spawn update_str(heap_data, 8, 12); // no race
      mult_double(heap_data);
      cilk_sync;
      assert(__cilksan_error_count() == 0);
      break;

    case 17:
      cilk_spawn update_str(heap_data, 4, 12); // no race
      read_i(heap_data);
      cilk_sync;
      assert(__cilksan_error_count() == 0);
      break;

    case 18:
      cilk_spawn update_str(heap_data, 3, 5); // no race 
      update_str(heap_data, 5, 8);
      cilk_sync;
      assert(__cilksan_error_count() == 0);
      break;

    case 19:
      read_and_write(&global_data); // read write race, but just 1
      read_and_write(heap_data);
      cilk_sync;
      assert(__cilksan_error_count() == 1);
      break;
  }
  cilk_sync;
  free(heap_data);

  return 0;
}

  
