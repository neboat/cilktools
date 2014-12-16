#include <cstring>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>

#include <execinfo.h>
#include <malloc.h>
#include <inttypes.h>
#include <unistd.h>

#include "cilksan_internal.h"
#include "debug_util.h"

// A map keeping track of races found, keyed by the larger instruction
// address involved in the race.  Races that have same instructions that 
// made the same types of accesses are considered as the the same race (even 
// for races where one is read followed by write and the other is write 
// followed by read, they are still considered as the same race).
// Races that have the same instruction addresses but different address
// for memory location is considered as a duplicate.
// The value of the map stores the number duplicates for the given race.
typedef std::unordered_multimap<uint64_t, RaceInfo_t> RaceMap_t;
static RaceMap_t races_found;
// The number of duplicated races found
static uint32_t duplicated_races = 0;

class ProcMapping_t {
  public:
    unsigned long low,high;
    std::string path;
    ProcMapping_t(unsigned long low, unsigned long high, std::string path) : 
                  low(low), high(high), path(path) { }
};

static std::vector<ProcMapping_t> *proc_maps = NULL;

// declared in cilksan.cpp
extern uint64_t stack_low_addr; 
extern uint64_t stack_high_addr;

void read_proc_maps(void) {
    if (proc_maps) return;

    proc_maps = new std::vector<ProcMapping_t>;
    pid_t pid = getpid();
    char path[100];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    DBG_TRACE(DEBUG_BACKTRACE, "path = %s\n", path);
    FILE *f = fopen(path, "r");
    DBG_TRACE(DEBUG_BACKTRACE, "file = %p\n", f);
    assert(f);

    char *lineptr = NULL;
    size_t n;
    while (1) {
        ssize_t s = getline(&lineptr, &n, f);
        if (s==-1) break;
        DBG_TRACE(DEBUG_BACKTRACE, "Got %ld line = %s size = %ld\n", 
                  s, lineptr, n);
        unsigned long start, end;
        char c0, c1, c2, c3;
        int off, major, minor, inode;
        char *pathname;
        sscanf(lineptr, "%lx-%lx %c%c%c%c %x %x:%x %x %ms",
               &start, &end, &c0, &c1, &c2, &c3, &off, &major, &minor, 
               &inode, &pathname);
        DBG_TRACE(DEBUG_BACKTRACE, " start = %lx end = %lx path = %s\n", 
                  start, end, pathname);
        std::string paths(pathname ? pathname : "");
        ProcMapping_t m(start, end, paths);
        if (paths.compare("[stack]") == 0) {
            assert(stack_low_addr == 0);
            stack_low_addr = start;
            stack_high_addr = end;
            DBG_TRACE(DEBUG_BACKTRACE, " stack = %lx--%lx\n", start, end);
        }
        free(pathname);
        proc_maps->push_back(m);
    }
    DBG_TRACE(DEBUG_BACKTRACE, "proc_maps size = %lu\n", proc_maps->size());
    fclose(f);
    if (lineptr) free(lineptr);
}

void delete_proc_maps() {
    if (proc_maps) {
      delete proc_maps;
      proc_maps = NULL;
    }
}

static void get_info_on_inst_addr(uint64_t addr, int *line_no, std::string *file) {

    for (unsigned int i=0; i < proc_maps->size(); i++) {
        if ((*proc_maps)[i].low <= addr && addr < (*proc_maps)[i].high) {
            unsigned long off = addr - (*proc_maps)[i].low;
            const char *path = (*proc_maps)[i].path.c_str();
            bool is_so = strcmp(".so", path+strlen(path)-3) == 0;
            char *command;
            if (is_so) {
                asprintf(&command, "echo %lx | addr2line -e %s", off, path);
            } else {
                asprintf(&command, "echo %lx | addr2line -e %s", addr, path);
            }
            FILE *afile = popen(command, "r");
            if (afile) {
                size_t linelen = -1;
                char *line = NULL;
                if (getline(&line, &linelen, afile)>=0) {
                    const char *path = strtok(line, ":");
                    const char *lno = strtok(NULL, ":");
                    *file = std::string(path); 
                    *line_no = atoi(lno);
                }
                if (line) free(line);
                pclose(afile);
            }
            free(command);
            return;
        }
    }

    fprintf(stderr, "%p is not in range\n", addr);
}

static std::string 
get_info_on_mem_access(uint64_t inst_addr 
                       /*, DisjointSet_t<SPBagInterface *> *d*/) {
  
  std::string file;
  int32_t line_no;
  std::ostringstream convert;
  // SPBagInterface *bag = d->get_node();
  // racedetector_assert(bag);

  get_info_on_inst_addr(inst_addr, &line_no, &file);
  convert << std::hex << "0x" << inst_addr << std::dec
          << ": (" << file << ":" << std::dec << line_no << ")"; 
  // XXX: Let's not do this for now; maybe come back later
  //   // convert << "\t called at " << bag->get_call_context();
  
  return convert.str();
}

extern void print_current_function_info();

static void print_race_info(const RaceInfo_t& race) {
  
  std::cerr << "Race detected at address " 
            // << (is_on_stack(race.addr) ? "stack address " : "address ")
            << std::hex << "0x" << race.addr << std::dec << std::endl;
  std::string first_acc_info = get_info_on_mem_access(race.first_inst); 
  std::string second_acc_info = get_info_on_mem_access(race.second_inst);

  switch(race.type) {
    case RW_RACE:
      std::cerr << "  read access at " << first_acc_info << std::endl;
      std::cerr << "  write access at " << second_acc_info << std::endl;
      break;

    case WW_RACE:
      std::cerr << "  write access at " << first_acc_info << std::endl;
      std::cerr << "  write access at " << second_acc_info << std::endl;
      break;

    case WR_RACE:
      std::cerr << "  write access at " << first_acc_info << std::endl;
      std::cerr << "  read access at " << second_acc_info << std::endl;
      break;
  }
  std::cerr << std::endl;

  print_current_function_info();
}

// Log the race detected
void report_race(uint64_t first_inst, uint64_t second_inst, 
                 uint64_t addr, enum RaceType_t race_type) {

  bool found = false;
  uint64_t key = first_inst < second_inst ? first_inst : second_inst;
  RaceInfo_t race(first_inst, second_inst, addr, race_type);
    
  std::pair<RaceMap_t::iterator, RaceMap_t::iterator> range;
  range = races_found.equal_range(key);
  while(range.first != range.second) {
    const RaceInfo_t& in_map = range.first->second;
    if( race.is_equivalent_race(in_map) ) {
      found = true;
      break;
    }
    range.first++;
  }
  if(found) { // increment the dup count
    duplicated_races++;
  } else {
    // have to get the info before user program exits
    print_race_info(race);
    races_found.insert( std::make_pair(key, race) );
  }
}

// Report viewread race
void report_viewread_race(uint64_t first_inst, uint64_t second_inst, 
                          uint64_t addr) {

  // For now, just print the viewread race
  std::cerr << "Race detected at address " 
            // << (is_on_stack(race.addr) ? "stack address " : "address ")
            << std::hex << "0x" << addr << std::dec << std::endl;
  std::string first_acc_info = get_info_on_mem_access(first_inst); 
  std::string second_acc_info = get_info_on_mem_access(second_inst);
  std::cerr << "  read access at " << first_acc_info << std::endl;
  std::cerr << "  write access at " << second_acc_info << std::endl;
  std::cerr << std::endl;
}

int get_num_races_found() {
    return races_found.size();
}

void print_race_report() {

  std::cerr << std::endl;
  std::cerr << "Race detector detected total of " << races_found.size()
            << " races." << std::endl;
  std::cerr << "Race detector suppressed " << duplicated_races 
            << " duplicate error messages " << std::endl;
  std::cerr << std::endl;

}

void print_addr(FILE *f, void *a) {
    read_proc_maps();
    unsigned long ai = (long)a;
    DBG_TRACE(DEBUG_BACKTRACE, "print addr = %p.\n", a);

    for (unsigned int i=0; i < proc_maps->size(); i++) {
        DBG_TRACE(DEBUG_BACKTRACE, "Comparing %lu to %lu:%lu.\n",
                  ai, (*proc_maps)[i].low, (*proc_maps)[i].high);
        if ((*proc_maps)[i].low <= ai && ai < (*proc_maps)[i].high) {
            unsigned long off = ai-(*proc_maps)[i].low;
            const char *path = (*proc_maps)[i].path.c_str();
            DBG_TRACE(DEBUG_BACKTRACE, 
                      "%p is offset 0x%lx in %s\n", a, off, path);
            bool is_so = strcmp(".so", path+strlen(path)-3) == 0;
            char *command;
            if (is_so) {
                asprintf(&command, "echo %lx | addr2line -e %s", off, path);
            } else {
                asprintf(&command, "echo %lx | addr2line -e %s", ai, path);
            }
            DBG_TRACE(DEBUG_BACKTRACE, "Doing system(\"%s\");\n", command);
            FILE *afile = popen(command, "r");
            if (afile) {
                size_t linelen = -1;
                char *line = NULL;
                while (getline(&line, &linelen, afile)>=0) {
                    fputs(line, f);
                }
                if (line) free(line);
                pclose(afile);
            }
            free(command);
            return;
        }
    }
    fprintf(stderr, "%p is not in range\n", a);
}

