#ifndef __METACALL_H__
#define __METACALL_H__

#include <sstream>

#include <internal/abi.h>
#include <internal/metacall.h>

static void metacall_error_exit [[noreturn]] (void *data_in, const char *str) {
  metacall_data_t *data = (metacall_data_t *)data_in;
  std::ostringstream convert;
  convert << str;
  convert << " with tool " << data->tool;
  convert << " and code " << data->code;
  const char *cstr = convert.str().c_str();
  die(ERR_EXIT_CODE, "Metacall %s is currently not supported.\n", cstr);
}

extern "C" void metacall_from_runtime(void *data_in) {
  metacall_data_t *data = (metacall_data_t *)data_in;
   
/* Put it back once we include reducer stuff
  if(data->tool == METACALL_TOOL_SYSTEM || 
     data->tool == METACALL_TOOL_CILKSCREEN) {
*/
    switch(data->code) {
      // XXX Do we need this?  If not, make instrumentation static later
      case HYPER_DISABLE_INSTRUMENTATION:
        DBG_TRACE(DEBUG_CALLBACK, "Metacall disable instrumentation.\n");
        instrumentation = false;
        break;

      // XXX Do we need this?  If not, make instrumentation static later
      case HYPER_ENABLE_INSTRUMENTATION:
        DBG_TRACE(DEBUG_CALLBACK, "Metacall enable instrumentation.\n");
        instrumentation = true;
        break;

      case HYPER_ZERO_IF_SEQUENTIAL_PTOOL:
        *(char *)(data->data) = 0;
        break;

      default:
        metacall_error_exit(data_in, "");
        // no return
    }

/* XXX: Enable later
  } else if(data->tool == METACALL_TOOL_REDRACE) {
    switch(data->code) {
      case HYPER_ZERO_IF_SIMULATE_STEALS:
        // setting to 0 means that we want the runtime to simulate certain
        // subset of steals (but not every steal)
        *(char *)(data->data) = 
            !(cont_depth_to_check || check_reduce || simulate_all_steals);
        break;

      case HYPER_CHECK_REDUCE_INTERVAL:
        // data > 0 when this is called from a spawn return protocol
        bool spawn_ret = (*(unsigned int *)(data->data) != 0);
        *(unsigned int *)(data->data) = get_current_reduce_interval(spawn_ret); 
        break;

      case HYPER_INVOKE_REDUCE:
        // pop off the top PBag and merge it with the next PBag on top
        update_disjointsets();
        break;

      default:
        metacall_error_exit(data->data, ""); 
        // no return
    }
  }
*/
}

#endif // __METACALL_H__
