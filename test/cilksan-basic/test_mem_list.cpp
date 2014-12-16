#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <assert.h>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

#include "cilksan.h" 


// disable optimization for now to make sure the race reported makes sense
#pragma GCC optimize ("-O0")

char data[16];
uint64_t global = 0;
uint64_t global2 = 0;

// write the first 8 bytes of the array in all granularity
// Need to write the larger granularity first; or it would 
// replace the writes with smaller-granularity that came before 
void write_all_grain_sizes_large_first(char *array) {

  uint64_t *eight   = (uint64_t *)&array[0];
  uint32_t *four[2] = { (uint32_t *)&array[0], (uint32_t *)&array[4] };
  uint16_t *two[4]  = { (uint16_t *)&array[0], (uint16_t *)&array[2],
                        (uint16_t *)&array[4], (uint16_t *)&array[6] };
  uint8_t  *one[8]  = { (uint8_t *)&array[0], (uint8_t *)&array[1],
                        (uint8_t *)&array[2], (uint8_t *)&array[3],
                        (uint8_t *)&array[4], (uint8_t *)&array[5],
                        (uint8_t *)&array[6], (uint8_t *)&array[7] };

  *eight = 0xfeeefeeefeeefeee;
  *four[0] = 0xdcccdccc;
  *four[1] = 0xdcccdccc;
  for(int i=0; i < 4; i++) {
    *two[i] = 0xabbb;
  }
  for(int i=0; i < 8; i++) {
    *one[i] = 0x11;
  }
}

void write_all_grain_sizes_small_first(char *array) {

  uint8_t  *one[8]  = { (uint8_t *)&array[0], (uint8_t *)&array[1],
                        (uint8_t *)&array[2], (uint8_t *)&array[3],
                        (uint8_t *)&array[4], (uint8_t *)&array[5],
                        (uint8_t *)&array[6], (uint8_t *)&array[7] };
  uint16_t *two[4]  = { (uint16_t *)&array[0], (uint16_t *)&array[2],
                        (uint16_t *)&array[4], (uint16_t *)&array[6] };
  uint32_t *four[2] = { (uint32_t *)&array[0], (uint32_t *)&array[4] };
  uint64_t *eight   = (uint64_t *)&array[0];

  for(int i=0; i < 8; i++) {
    *one[i] = 0x11;
  }
  for(int i=0; i < 4; i++) {
    *two[i] = 0xabbb;
  }
  *four[0] = 0xdcccdccc;
  *four[1] = 0xdcccdccc;
  *eight = 0xfeeefeeefeeefeee;
}

// read the first 8 array of the array in all granularity
// Need to read the larger granularity first; or it would 
// replace the reads with smaller-granularity that came before 
uint64_t read_all_grain_sizes_large_first(char *array) {
  uint64_t ret = 0;

  ret  = *(uint64_t *)&array[0];
  ret  = *(uint32_t *)&array[0];
  ret += *(uint32_t *)&array[4];

  for(int i=0; i < 4; i++) {
    ret += *(uint16_t *)&array[i*2];
  }
  for(int i=0; i < 8; i++) {
    ret += *(uint8_t *)&array[i];
  }
  ret += *(uint8_t *)&array[3];
  ret += *(uint8_t *)&array[5];

  return ret;
}

uint64_t read_all_grain_sizes_small_first(char *array) {
  uint64_t ret = 0;

  for(int i=0; i < 8; i++) {
    ret += *(uint8_t *)&array[i];
  }
  for(int i=0; i < 4; i++) {
    ret += *(uint16_t *)&array[i*2];
  }
  ret  = *(uint32_t *)&array[0];
  ret += *(uint32_t *)&array[4];
  ret  = *(uint64_t *)&array[0];

  return ret;
}

// write the first x array of this array
void write_x_bytes(char *array, int x) {
  switch(x) {
    case 8: 
    {
      uint64_t *eight = (uint64_t *)&array[0];
      *eight = 0;
      break;
    }
    case 4:
    {
      uint32_t *four  = (uint32_t *)&array[0];
      *four = 0;
      break;
    }
    case 2:
    {
      uint16_t *two   = (uint16_t *)&array[0];
      *two = 0;
      break;
    }
    case 1:
    {
      uint8_t *one    = (uint8_t *)&array[0];
      *one = 0;
      break;
    }
  }
}

void write_a_few() {
  write_x_bytes(&data[4], 4);
  write_x_bytes(&data[0], 8);
  write_x_bytes(&data[2], 2);
  write_x_bytes(&data[6], 1);
}

// read the first x array of this array
uint64_t read_x_bytes(char *array, int x) {
  uint64_t ret = 0;
  switch(x) {
    case 8:
    {
      uint64_t *eight = (uint64_t *)&array[0];
      ret += *eight;
      break;
    }
    case 4:
    {
      uint32_t *four  = (uint32_t *)&array[0];
      ret += *four;
      break;
    }
    case 2:
    {
      uint16_t *two   = (uint16_t *)&array[0];
      ret += *two;
      break;
    }
    case 1:
    {
      uint8_t *one    = (uint8_t *)&array[0];
      ret += *one;
      break;
    }
  }
 
  return ret;
}

uint64_t read_a_few() {
  uint64_t x = 0;
  x += read_x_bytes(&data[0], 8);
  x += read_x_bytes(&data[4], 4);
  x += read_x_bytes(&data[2], 2);
  x += read_x_bytes(&data[2], 1);

  return x;
}

void test_write_all_and_read_all() {
  cilk_spawn write_all_grain_sizes_large_first(data);
  global = read_all_grain_sizes_large_first(data);
  cilk_sync;

  cilk_spawn write_all_grain_sizes_small_first(data);
  global += read_all_grain_sizes_small_first(data);
  cilk_sync;
}

void test_write_all_and_write_all() {
  cilk_spawn write_all_grain_sizes_large_first(data);
  write_all_grain_sizes_small_first(data);
  cilk_sync;

  cilk_spawn write_all_grain_sizes_small_first(data);
  write_all_grain_sizes_large_first(data);
  cilk_sync;
}

void test_write_few_and_write_all() {
  cilk_spawn write_a_few();
  write_all_grain_sizes_small_first(data);
  cilk_sync;

  cilk_spawn write_a_few();
  write_all_grain_sizes_large_first(data);
  cilk_sync;
}

void test_write_all_and_write_few() {
  cilk_spawn write_all_grain_sizes_small_first(data);
  write_a_few();
  cilk_sync;

  cilk_spawn write_all_grain_sizes_large_first(data);
  write_a_few();
  cilk_sync;
}

void test_read_all_and_write_all() {
  global = cilk_spawn read_all_grain_sizes_large_first(data);
  write_all_grain_sizes_large_first(data);
  cilk_sync;

  global2 = cilk_spawn read_all_grain_sizes_small_first(data);
  write_all_grain_sizes_small_first(data);
  cilk_sync;
}

void test_write_all_and_read_few() {
  cilk_spawn write_all_grain_sizes_large_first(data);
  global = read_a_few();
  cilk_sync; 

  cilk_spawn write_all_grain_sizes_small_first(data);
  global += read_a_few();
  cilk_sync; 
}

void test_read_few_and_write_all() {
  global = cilk_spawn read_a_few();
  write_all_grain_sizes_large_first(data);
  cilk_sync; 

  global2 = cilk_spawn read_a_few();
  write_all_grain_sizes_small_first(data);
  cilk_sync; 
}

uint64_t read_a_couple() {
  uint64_t x = 0;
  x += read_x_bytes(&data[0], 1);
  x += read_x_bytes(&data[0], 4);
  return x;
}

int main(int argc, char* argv[]) {

  int which = 0;

  if (argc >= 2) {
    which = atoi(argv[1]);
  }

  switch(which) {
    case 0:
      test_write_all_and_read_all(); 
      assert(__cilksan_error_count() == 12);
      break;
    case 1:
      test_write_all_and_read_few(); 
      assert(__cilksan_error_count() == 8);
      break;
    case 2:
      test_read_all_and_write_all(); 
      assert(__cilksan_error_count() == 18);
      break;
    case 3:
      test_read_few_and_write_all(); 
      assert(__cilksan_error_count() == 32);
      break;
    case 4:
      test_write_all_and_write_all(); 
      assert(__cilksan_error_count() == 9);
      break;
    case 5:
      test_write_all_and_write_few(); 
      assert(__cilksan_error_count() == 8);
      break;
    case 6:
      test_write_few_and_write_all(); 
      assert(__cilksan_error_count() == 26);
      break;
    case 7:
      global = cilk_spawn read_x_bytes(&data[0], 8);
      global2 = cilk_spawn read_x_bytes(&data[0], 4);
      write_x_bytes(&data[0], 2);
      cilk_sync;
      assert(__cilksan_error_count() == 1);
      break;
    case 8:
      global = cilk_spawn read_a_couple();
      write_x_bytes(&data[2], 2);
      cilk_sync;
      assert(__cilksan_error_count() == 1);
      break;
    case 9:
      global = cilk_spawn read_a_couple();
      write_x_bytes(&data[0], 4);
      cilk_sync;
      assert(__cilksan_error_count() == 1);
      break;
    case 10: 
    {
      int x = cilk_spawn read_x_bytes(&data[0], 8);
      int y = cilk_spawn read_x_bytes(&data[0], 4);
      int z = cilk_spawn read_x_bytes(&data[4], 4);
      write_x_bytes(&data[0], 8);
      cilk_sync;
      global = x + y + z;
      assert(__cilksan_error_count() == 1);
      break;
    }
    case 11:
    {
      int x = cilk_spawn read_x_bytes(&data[0], 4);
      int y = cilk_spawn read_x_bytes(&data[4], 4);
      int z = cilk_spawn read_x_bytes(&data[0], 8);
      write_x_bytes(&data[0], 8);
      cilk_sync;
      global = x + y + z;
      assert(__cilksan_error_count() == 1);
      break;
    }
    case 12:
    {
      int x = cilk_spawn read_x_bytes(&data[0], 2);
      int y = cilk_spawn read_x_bytes(&data[2], 2);
      cilk_sync;
      int z = cilk_spawn read_x_bytes(&data[0], 8); 
      write_x_bytes(&data[0], 8);
      cilk_sync;
      global = x + y + z;
      assert(__cilksan_error_count() == 1);
      break;
    }
    case 13:
    {
      int x = read_x_bytes(&data[0], 2);
      x += read_x_bytes(&data[2], 2);
      int y = cilk_spawn read_x_bytes(&data[0], 8);
      int z = cilk_spawn read_x_bytes(&data[0], 4);
      write_x_bytes(&data[0], 8);
      cilk_sync;
      global = x + y + z;
      assert(__cilksan_error_count() == 1);
      break;
    }
    case 14:
    {
      int x = cilk_spawn read_a_few();
      int y = cilk_spawn read_x_bytes(&data[0], 8);
      write_x_bytes(&data[0], 8);
      cilk_sync;
      global = x + y;
      assert(__cilksan_error_count() == 4);
      break;
    }
    case 15:
      cilk_spawn write_x_bytes(&data[0], 8);
      global = cilk_spawn read_x_bytes(&data[4], 4);
      global2 = read_x_bytes(&data[4], 2);
      cilk_sync;
      cilk_spawn write_x_bytes(&data[0], 8); 
      global += read_x_bytes(&data[4], 4);
      cilk_sync;
      assert(__cilksan_error_count() == 2);
      break;
  }

  return 0;
}
