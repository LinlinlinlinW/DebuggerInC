#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <strings.h>



#include "instruction.h"
#include "printRoutines.h"

#define ERROR_RETURN -1
#define SUCCESS 0

#define MAX_LINE 256


// we use linkedlist to store all the breakpoints
struct Node {
  uint64_t address;
  struct Node* next;
};
struct Node * head = NULL;

static void addBreakpoint(uint64_t address);
static void deleteBreakpoint(uint64_t address);
static void deleteAllBreakpoints(void);
static int  hasBreakpoint(uint64_t address);

int main(int argc, char **argv) {
  int fd;
  struct stat st;

  machine_state_t state;
  y86_instruction_t nextInstruction;
  memset(&state, 0, sizeof(state));

  char line[MAX_LINE + 1], previousLine[MAX_LINE + 1] = "";
  char *command, *parameters;
  int c;

  // initialize the linkedlist
  head = malloc(sizeof(struct Node));
  head->address = -1;
  head->next = NULL;

  // Verify that the command line has an appropriate number of arguments
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: %s InputFilename [startingPC]\n", argv[0]);
    return ERROR_RETURN;
  }

  // First argument is the file to read, attempt to open it for
  // reading and verify that the open did occur.
  fd = open(argv[1], O_RDONLY);

  if (fd < 0) {
    fprintf(stderr, "Failed to open %s: %s\n", argv[1], strerror(errno));
    return ERROR_RETURN;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(stderr, "Failed to stat %s: %s\n", argv[1], strerror(errno));
    close(fd);
    return ERROR_RETURN;
  }

  state.programSize = st.st_size;
  // If there is a 2nd argument present it is an offset so convert it to a numeric value.
  if (3 <= argc) {
    errno = 0;
    state.programCounter = strtoul(argv[2], NULL, 0);
    if (errno != 0) {
      perror("Invalid program counter on command line");
      close(fd);
      return ERROR_RETURN;
    }
    if (state.programCounter > state.programSize) {
      fprintf(stderr, "Program counter on command line (%lu) "
	      "larger than file size (%lu).\n",
	      state.programCounter, state.programSize);
      close(fd);
      return ERROR_RETURN;
    }
  }

  // Maps the entire file to memory. This is equivalent to reading the
  // entire file using functions like fread, but the data is only
  // retrieved on demand, i.e., when the specific region of the file is needed.
  state.programMap = mmap(NULL, state.programSize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (state.programMap == MAP_FAILED) {
    fprintf(stderr, "Failed to map %s: %s\n", argv[1], strerror(errno));
    close(fd);
    return ERROR_RETURN;
  }

  // Move to first non-zero byte
  while (!state.programMap[state.programCounter]) state.programCounter++;

    printf("# Opened %s, starting PC 0x%lX\n", argv[1], state.programCounter);

    fetchInstruction(&state, &nextInstruction);
    printInstruction(stdout, &nextInstruction);

  while(1) {
    // Show prompt, but only if input comes from a terminal
    if (isatty(STDIN_FILENO))
      printf("> ");

    // Read one line, if EOF break loop
    if (!fgets(line, sizeof(line), stdin))
      break;


    // If line could not be read entirely
    if (!strchr(line, '\n')) {
      // Read to the end of the line
      while ((c = fgetc(stdin)) != EOF && c != '\n');
      if (c == '\n') {
	      printErrorCommandTooLong(stdout);
	      continue;
      }
      else {
	  // In this case there is an EOF at the end of a line.
	  // Process line as usual.
      }
    }

    // Obtain the command name, separate it from the arguments.
    command = strtok(line, " \t\n\f\r\v");
    // If line is blank, repeat previous command.
    if (!command) {
      strcpy(line, previousLine);
      command = strtok(line, " \t\n\f\r\v");
      // If there's no previous line, do nothing.
      if (!command) continue;
    }

    // Get the arguments to the command, if provided.
    parameters = strtok(NULL, "\n\r");

    sprintf(previousLine, "%s %s\n", command, parameters ? parameters : "");



    // we start coding here ---------------------------->

    char stepPT[] = "step";
    char quitPT[] = "quit";
    char exitPT[] = "exit";
    char runPT[] = "run";
    char nextPT[] = "next";
    char jumpPT[] = "jump";
    char breakPT[] = "break";
    char deletePT[] = "delete";
    char registerPT[] = "register";
    char examinePT[] = "examine";

    if(!strcasecmp(command, quitPT) || !strcasecmp(command, exitPT)) {
        break;
    }
    else if (!strcasecmp(command, stepPT)) {
      if (executeInstruction(&state, &nextInstruction) == 0) {
        // printInstruction(stdout, &nextInstruction);
      }
      if (nextInstruction.icode != I_HALT){
        fetchInstruction(&state, &nextInstruction);
        printInstruction(stdout, &nextInstruction);
      } else {
        printInstruction(stdout, &nextInstruction);
      }
    }

    else if (!strcasecmp(command,runPT)) {
      // while loop
      if (nextInstruction.icode == I_HALT) {
        executeInstruction(&state, &nextInstruction);
        printInstruction(stdout, &nextInstruction);  
      }

      while (1) {
        // check whether the execution is successful
        if (executeInstruction(&state, &nextInstruction) == 0) {
          break;
        }
        if (nextInstruction.icode == I_HALT) {
          break;
        }
        fetchInstruction(&state, &nextInstruction);
        printInstruction(stdout, &nextInstruction);
        if (hasBreakpoint(nextInstruction.location)) {
          break;
        }
      }
    }


    else if (!strcasecmp(command,nextPT)) {
      // if the next instruction is "call"
      if (nextInstruction.icode == I_CALL) {

        uint64_t curAddr = state.registerFile[4];
        executeInstruction(&state, &nextInstruction);

        while (state.registerFile[4] != curAddr) {

            fetchInstruction(&state, &nextInstruction);
            printInstruction(stdout, &nextInstruction);

          if (hasBreakpoint(nextInstruction.location)) {
            break;
          } else if (executeInstruction(&state, &nextInstruction) == 0) {
            break;
          } else if (nextInstruction.icode == I_HALT) {
            break;
          }
        }

        if (nextInstruction.icode != I_HALT) {
          fetchInstruction(&state, &nextInstruction);
          printInstruction(stdout, &nextInstruction);
        }

      }  else {
        if (executeInstruction(&state, &nextInstruction) == 0) {
          // break;
        }
        if (nextInstruction.icode != I_HALT) {
          fetchInstruction(&state, &nextInstruction);
          printInstruction(stdout, &nextInstruction);
        } else {
          printInstruction(stdout, &nextInstruction);
        }
        // fetchInstruction(&state, &nextInstruction);
        // printInstruction(stdout, &nextInstruction);

      }
    }


    else if (!strcasecmp(command,jumpPT)) {
      // if the command is "jump"
      state.programCounter = strtoul(parameters, NULL, 0);

      fetchInstruction(&state, &nextInstruction);
      printInstruction(stdout, &nextInstruction);
    }


    else if (!strcasecmp(command, breakPT)) {
      // call addbreakpt
      if (parameters != NULL) {
        addBreakpoint(strtoul(parameters, NULL, 0));
      } else {
        printErrorInvalidCommand(stdout, command, parameters);
      }

    }


    else if (!strcasecmp(command, deletePT)) {
      // call delete breakpt
      if (parameters != NULL) {
        deleteBreakpoint(strtoul(parameters, NULL, 0));
      } else {
        printErrorInvalidCommand(stdout, command, parameters);
      }

    }

    else if (!strcasecmp(command, registerPT)) {
      for (int i = 0; i < 15; i++) {
        printRegisterValue(stdout, &state, i);
      }
    }


    else if (!strcasecmp(command, examinePT)) {
      printMemoryValueQuad(stdout, &state, strtoul(parameters, NULL, 0));
    }

    else {
    }
    /* THIS PART TO BE COMPLETED BY THE STUDENT */
  }

  deleteAllBreakpoints();
  munmap(state.programMap, state.programSize);
  close(fd);
  return SUCCESS;
}


/* Adds an address to the list of breakpoints. If the address is
 * already in the list, it is not added again. */
static void addBreakpoint(uint64_t address) {
  if (!hasBreakpoint(address)) {
    struct Node * temp = (struct Node*)malloc(sizeof(struct Node));
    temp->address = address;
    temp->next = NULL;
    struct Node * end = head;
    while (end->next != NULL) {
      end = end->next;
    }
    end->next = temp;
    /* THIS PART TO BE COMPLETED BY THE STUDENT */
  }
}

/* Deletes an address from the list of breakpoints.
If the address is not in the list, nothing happens. */
static void deleteBreakpoint(uint64_t address) {
  for (struct Node* first = head; first ->next != NULL; first = first->next) {
    // if the middle or the tail is what we want
    if (first->next->address == address) {
        struct Node* temp = first->next;
        first->next = first->next->next;
        free(temp);
        temp = NULL;
        return;
      }
    }
    /* THIS PART TO BE COMPLETED BY THE STUDENT */
  }




/* Deletes and frees all breakpoints. */
static void deleteAllBreakpoints(void) {
  /* THIS PART TO BE COMPLETED BY THE STUDENT */
  while (head != NULL) {
    struct Node* temp = head;
    head = head->next;
    free(temp);
    temp = NULL;
  }
}

/* Returns true (non-zero) if the address corresponds to a breakpoint
 * in the list of breakpoints, or false (zero) otherwise. */
static int hasBreakpoint(uint64_t address) {
  for (struct Node* first = head; first != NULL; first = first->next) {
    if (first->address == address) {
      return 1;
    }
  }
  return 0;
  /* THIS PART TO BE COMPLETED BY THE STUDENT */
}
