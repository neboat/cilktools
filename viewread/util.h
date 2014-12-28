#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Linked list of mappings.
 */

typedef struct mapping_t {
  uintptr_t low, high;
  char *path;
} mapping_t;

typedef struct mapping_list_el_t {
  mapping_t map;
  struct mapping_list_el_t* next;
} mapping_list_el_t;

typedef struct mapping_list_t {
  mapping_list_el_t *head;
  mapping_list_el_t *tail;
} mapping_list_t;

mapping_list_t maps = { .head = NULL, .tail = NULL };


// Ensure that this tool is run serially
void ensure_serial_tool(void) {
  // assert(1 == __cilkrts_get_nworkers());
  fprintf(stderr, "Forcing CILK_NWORKERS=1.\n");
  char *e = getenv("CILK_NWORKERS");
  if (!e || 0!=strcmp(e, "1")) {
    // fprintf(err_io, "Setting CILK_NWORKERS to be 1\n");
    if( setenv("CILK_NWORKERS", "1", 1) ) {
      fprintf(stderr, "Error setting CILK_NWORKERS to be 1\n");
      exit(1);
    }
  }
}

void read_proc_maps(void) {
  pid_t pid = getpid();
  char path[100];
  snprintf(path, sizeof(path), "/proc/%d/maps", pid);
  if (0) printf("path=%s\n", path);
  FILE *f = fopen(path, "r");
  if (0) printf("file=%p\n", f);
  assert(f);
  char *lineptr = NULL;
  size_t n;
  while (1) {
    ssize_t s = getline(&lineptr, &n, f);
    if (s==-1) break;
    if (0) printf("Got %ld line=%s size=%ld\n", s, lineptr, n);
    uintptr_t start, end;
    char c0, c1, c2, c3;
    int off, major, minor, inode;
    char *pathname;
    sscanf(lineptr, "%lx-%lx %c%c%c%c %x %x:%x %x %ms",
	   &start, &end, &c0, &c1, &c2, &c3, &off, &major, &minor, &inode, &pathname);
    if (0) printf(" start=%lx end=%lx path=%s\n", start, end, pathname);
    // Make new map
    mapping_list_el_t *m = (mapping_list_el_t*)malloc(sizeof(mapping_list_el_t));
    m->map.low = start;
    m->map.high = end;
    m->map.path = pathname;
    m->next = NULL;
    // Push map onto list
    if (NULL == maps.head) {
      maps.head = m;
      maps.tail = m;
    } else {
      maps.tail->next = m;
    }
  }
  /* if (0) printf("maps.size()=%lu\n", maps->size()); */
  fclose(f);
}

void print_addr(uintptr_t a) {
  uintptr_t ai = a;
  /* if (1) printf(" PC= %lx\n", a); */
  mapping_list_el_t *map_lst_el = maps.head;
  while (NULL != map_lst_el) {
    mapping_t *map = &(map_lst_el->map);
    if (0) printf("Comparing %lx to %lx:%lx\n", ai, map->low, map->high);
    if (map->low <= ai && ai < map->high) {
      uintptr_t off = ai-map->low;
      const char *path = map->path;
      /* if (1) printf("%lx is offset 0x%lx in %s\n", a, off, path); */
      bool is_so = strcmp(".so", path+strlen(path)-3) == 0;
      if (0) {if (is_so) printf(" is so\n"); else printf(" not so\n");}
      char *command;
      if (is_so) {
	asprintf(&command, "echo %lx | addr2line -e %s", off, path);
      } else {
	asprintf(&command, "echo %lx | addr2line -e %s", ai, path);
      }
      /* if (debug_level>1) printf("Doing system(\"%s\");\n", command); */
      system(command);
      free(command);
      return;
    }
    map_lst_el = map_lst_el->next;
  }
  printf("%lx is not in range\n", a);
}
