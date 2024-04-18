#include <ctype.h>
#include <omp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>
typedef char byte;

enum mesi_state { Invalid, Shared, Exclusive, Modified };

typedef enum mesi_state mesi_state;

#ifdef DEBUG
#define debug(...) ;printf(__VA_ARGS__)
#else
#define debug(...)
#endif

struct cache {
  byte address; // This is the address in memory.
  byte value;   // This is the value stored in cached memory.
  // State for you to implement MESI protocol.
  mesi_state state;
};

struct decoded_inst {
  int type; // 0 is RD, 1 is WR
  byte address;
  byte value; // Only used for WR
};
#include <stdbool.h>
typedef struct cache cache;
typedef struct decoded_inst decoded;

/*
 * This is a very basic C cache simulator.
 * The input files for each "Core" must be named core_1.txt, core_2.txt,
 * core_3.txt ... core_n.txt Input files consist of the following instructions:
 * - RD <address>
 * - WR <address> <val>
 */

byte *memory;

// Decode instruction lines
decoded decode_inst_line(char *buffer) {
  decoded inst;
  char inst_type[2];
  sscanf(buffer, "%s", inst_type);
  if (!strcmp(inst_type, "RD")) {
    inst.type = 0;
    int addr = 0;
    sscanf(buffer, "%s %d", inst_type, &addr);
    inst.value = -1;
    inst.address = addr;
  } else if (!strcmp(inst_type, "WR")) {
    inst.type = 1;
    int addr = 0;
    int val = 0;
    sscanf(buffer, "%s %d %d", inst_type, &addr, &val);
    inst.address = addr;
    inst.value = val;
  }
  return inst;
}

// send bus signal

// Helper function to print the cachelines
void print_cachelines(cache *c, int cache_size) {
  for (int i = 0; i < cache_size; i++) {
    cache cacheline = *(c + i);
    char state[10];
    switch (cacheline.state) {
    case Invalid:
      strcpy(state, "Invalid");
      break;
    case Shared:
      strcpy(state, "Shared");
      break;
    case Exclusive:
      strcpy(state, "Exclusive");
      break;
    case Modified:
      strcpy(state, "Modified");
      break;
    }
    debug("\t\tAddress: %d, State: %s, Value: %d\n", cacheline.address, state,
          cacheline.value);
  }
}

// This function implements the mock CPU loop that reads and writes data.
void cpu_loop(int num_threads) {
  int cache_size = 2;

  // initialize the cache of all the cores
  cache **c = (cache **)malloc(sizeof(cache *) * num_threads);
  for (int i = 0; i < num_threads; i++) {
    *(c + i) = (cache *)malloc(sizeof(cache) * cache_size);
  }


#pragma omp parallel num_threads(num_threads)
  {

    int tid = omp_get_thread_num();

    // Input file for the core
    char filename[20];
    sprintf(filename, "input_%d.txt", tid % 2);
    printf("Reading from file: %s\n", filename);
    // Read Input file
    FILE *inst_file = fopen(filename, "r");
    char inst_line[20];
    // Decode instructions and execute them.
    while (fgets(inst_line, sizeof(inst_line), inst_file)) {
#pragma omp single
      printf("\nClock tick\n");


        decoded inst = decode_inst_line(inst_line);

        // direct mapping hash
        int hash = inst.address % cache_size;

      // make memory access atomic
      #pragma omp critical
      {
        // replace the cacheline if the address is different and data is
        // modified
        if (c[tid][hash].address != inst.address && (c[tid][hash].state == Modified || c[tid][hash].state == Shared)) {
          // Flush current cacheline to memory
          debug("Flushing cacheline at address %d to memory\n",
                c[tid][hash].address);
          // #pragma omp critical
          memory[c[tid][hash].address] = c[tid][hash].value;
          c[tid][hash].value = memory[inst.address];
          c[tid][hash].address = inst.address;
        }
        if (inst.type == 1) /* Write operation */ {
          c[tid][hash].address = inst.address;
          c[tid][hash].value = inst.value;
          c[tid][hash].state = Modified;


          // invalidate other caches if data is not exclusive
          if (c[tid][hash].state != Exclusive) {

            // iterate and invalidate other caches
            for (int i = 0; i < num_threads; i++) {
              if (i == tid)
                continue;
              if (c[i][hash].address == inst.address) {
                debug("Core %d: Invalidating address %d\n", i, inst.address);
                c[i][hash].state = Invalid;
              }
            }
          }

          // set the state to modified

        } else /* Read Operation*/ {
          if(c[tid][hash].address != inst.address || c[tid][hash].state == Invalid) /* read miss */ {
            debug("Read Miss\n");
            bool found = false;
            for(int i=0; i<num_threads; i++) {
              if(i == tid || c[i][hash].address != inst.address)
                continue;
              if(c[i][hash].state != Invalid){
                // data found in other cache
                c[tid][hash] = c[i][hash];
                c[i][hash].state = Shared;
                c[tid][hash].state = Shared;
                found = true;
              }
            }
            if(!found) {
              // fetch data from mem
              c[tid][hash].value = memory[inst.address];
              c[tid][hash].state = Exclusive;
              c[tid][hash].address = inst.address;

            }

          }


        }
        // #pragma omp critical
        switch (inst.type) {
        case 0:
          printf("Core %d Reading from address %02d: %02d\n", tid,
                 c[tid][hash].address, c[tid][hash].value);
          break;

        case 1:
          printf("Core %d Writing   to address %02d: %02d\n", tid, c[tid][hash].address,
                 c[tid][hash].value);
          break;
        }
        debug("Memory: ");
        for (int i = 0; i < 24; i++) {
          debug("%02d:%02d ",i, memory[i]);
        }
        debug("\n");
        for (int i = 0; i < num_threads; i++) {
          debug("\tCore %d\n", i);
          print_cachelines(*(c + i), cache_size);
          debug("\n");
        }

#ifdef DEBUG
        getchar();
#endif

      }
      // synchronize clock tick
#pragma omp barrier
    }
    free(c);
  }
}

int main(int c, char *argv[]) {
  // Initialize Global memory
  // Let's assume the memory module holds about 24 bytes of data.
  int memory_size = 24;
  memory = (byte *)malloc(sizeof(byte) * memory_size);
  cpu_loop(2);
  free(memory);
}
