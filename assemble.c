/*
 * assemble.c - handles the details of assembly for the asx20 assembler
 *
 *              This currently just contains stubs with debugging output.
 */
#include <stdio.h>
#include "defs.h"
#include "symtab.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define ERROR_PROGRAM_SIZE "Program consumes more than 2^20 words"
#define ERROR_LABEL_DEFINED "Label %s already defined"
#define ERROR_OPCODE_UNKNOWN "Unknown Opcode %s"
#define ERROR_OPERAND_FORMAT "Opcode does not match the given operands"
#define ERROR_CONSTANT_ZERO "Constant must be greater than zero"
#define ERROR_CONSTANT_INVALID "Constant %d will not fit into 20 bits"
#define ERROR_OFFSET_INVALID "Offset %d will not fit into 16 bits"
#define ERROR_MULTIPLE_EXPORT "Symbol %s exported more than once"
#define ERROR_MULTIPLE_IMPORT "Symbol %s imported more than once"
#define ERROR_LABEL_REFERENCE_NOT_FOUND "Label %s is referenced but not defined or imported"
#define ERROR_SYMBOL_IMPORT_EXPORT "Symbol %s is both imported and exported"
#define ERROR_SYMBOL_IMPORT_DEFINED "Symbol %s is both imported and defined"
#define ERROR_SYMBOL_IMPORT_NO_REFERENCE "Symbol %s is imported but not referenced"
#define ERROR_SYMBOL_EXPORT_NO_DEFINITION "Symbol %s is exported but not defined"
#define ERROR_SYMBOL_IMPORT_SIZE "Symbol %s is imported and longer than 16 characters"
#define ERROR_SYMBOL_EXPORT_SIZE "Symbol %s is exported and longer than 16 characters"
#define ERROR_LABEL_SIZE16 "Reference to label %s at address %d won't fit in 16 bits"
#define ERROR_LABEL_SIZE20 "Reference to label %s at address %d won't fit in 20 bits"

// enable debugging printout
#define DEBUG 0

// Global PC counter
static int pc = 0;

// Global symtab struct
static void *symtab;

// Global error tracker
static int error_count = 0;

// Struct to hold symbol and its information
typedef struct symbol_info {
  int address;
  bool exported;
  bool imported;
  bool referenced; 
} symbol_info_t;

// Number of codes + directives in vmx20 system
#define OPCODE_ARRAY_LENGTH 30

//Table of all opcodes/directives for error handling
static const char *opcodes[] = {
  "halt",
  "load",
  "store",
  "ldimm",
  "ldaddr",
  "ldind",
  "stind",
  "addf",
  "subf",
  "divf",
  "mulf",
  "addi",
  "subi",
  "divi",
  "muli",
  "call",
  "ret",
  "blt",
  "bgt",
  "beq",
  "jmp",
  "cmpxchg",
  "getpid",
  "getpn",
  "push",
  "pop",
  "word",
  "alloc",
  "import",
  "export"
};

typedef struct opcode_struct{
  char *opcode_string;
  int opcode_value;
  int format;
}opcode_struct_t;

static opcode_struct_t opcodes[] = {
{"halt",    0x00, 1},
{"load",    0x01, 5},
{"store",   0x02, 5},
{"ldimm",   0x03, 4},
{"ldaddr",  0x04, 5},
{"ldind",   0x05, 7},
{"stind",   0x06, 7},
{"addf",    0x07, 6},
{"subf",    0x08, 6},
{"divf",    0x09, 6},
{"mulf",    0x0A, 6},
{"addi",    0x0B, 6},
{"subi",    0x0C, 6},
{"divi",    0x0D, 6},
{"muli",    0x0E, 6},
{"call",    0x0F, 2},
{"ret",     0x10, 1},
{"blt",     0x11, 8},
{"bgt",     0x12, 8},
{"beq",     0x13, 8},
{"jmp",     0x14, 2},
{"cmpxchg", 0x15, 8},
{"getpid",  0x16, 3},
{"getpn",   0x17, 3},
{"push",    0x18, 3},
{"pop",     0x19, 3},
{"word",    0x00, 9}, 
{"alloc",   0x00, 9},
{"import",  0x00, 2},
{"export",  0x00, 2}
};






// this is called once so that the assembler can initialize any internal
// data structures.
void initAssemble(void) {

  // Intialize our symbol table
  // Size can be arbitrary
  symtab = symtabCreate(100);

#if DEBUG
  fprintf(stderr, "initAssemble called\n");
#endif
}

// this is the "guts" of the assembler and is called for each line
// of the input that contains a label, instruction or directive
//
// note that there may be both a label and an instruction or directive
// present on a line
//
// note that the assembler directives "export" and "import" have structure
// identical to instructions with format 2, so that format is used for them
//
// for the directives "word" and "alloc" a special format, format 9, is used
//
// see defs.h for the details on how each instruction format is represented
// in the INSTR struct.
//
void assemble(char *label, INSTR instr) {

  // ERROR CHECK: BAD OPCODE
  if(instr.opcode != NULL) {
    int valid = 0;

    // Search our opcode array for the passed in instr opcode
    for(int i = 0; i < OPCODE_ARRAY_LENGTH; i++) {

      if (strcmp(instr.opcode, opcodes[i]) == 0) {
        valid = 1;  // Found a match
      }
    }

    // Opcode not found so report error
    if(!valid) {
      error(ERROR_OPCODE_UNKNOWN, instr.opcode);
      error_count++;
    }
  }


  // Case 1: Only a label is on the line 
  // Case 2: There is a label being defined and instructions on the line
  if(instr.format == 0 || label) {

    symbol_info_t *symbol_info;

    // If this is the first time the symbol appears in the asm file...install it
    // We dont only install a symbol from when its defined
    if((symbol_info = symtabLookup(symtab, label)) == NULL) {

      // Symbol doesnt exist so alloc space for its data structurev
      symbol_info = malloc(sizeof(symbol_info_t));
      if(symbol_info == NULL) {
        fprintf(stderr, "Symbol install failed\n");
        return;
      }

      // Set our data values
      symbol_info->address = pc;
      symbol_info->referenced = false;
      symbol_info->exported = false;
      symbol_info->imported = false;

      symtabInstall(symtab, label, symbol_info);

    // ERROR CHECK: LABEL ALREADY DEFINED
    } else if(symbol_info->address != -1) {
      error_count++;
      error(ERROR_LABEL_DEFINED, label);
    
      // Update a symbol to be defined and pass in proper address
    } else if(symbol_info->address == -1) {

      symbol_info->address = pc;
    }

      // We need to decrement the pc counter each time a symbol is defined
      // because the line the symbol is defined on and the next line are treated as one for expected outputs.
      // But to the parser they are treated seperately so we must account for extra pc counts...
      if(instr.format == 0) {
        pc--;
      }

  // Export directive
  } else if((strcmp(instr.opcode, "export")) == 0) {

    symbol_info_t *symbol_info;

    if((symbol_info = symtabLookup(symtab, instr.u.format2.addr)) == NULL) {

      // Symbol doesnt exist so alloc space for its data structure
      symbol_info = malloc(sizeof(symbol_info_t));
      if(symbol_info == NULL) {
        fprintf(stderr, "Struct Failed\n");
        return;
      }

      symbol_info->address = -1; // NOT YET DEFINED
      symbol_info->exported = true;
      symbol_info->imported = false;
      symbol_info->referenced = false;
      symtabInstall(symtab, instr.u.format2.addr, symbol_info);
    }



  // Import directive  
  } else if(strcmp(instr.opcode, "import") == 0) {

    symbol_info_t *symbol_info;

    if((symbol_info = symtabLookup(symtab, instr.u.format2.addr)) == NULL) {

      // Symbol doesnt exist so alloc space for its data structure
      symbol_info = malloc(sizeof(symbol_info_t));
      if(symbol_info == NULL) {
        fprintf(stderr, "Struct Failed\n");
        return;
      }

      symbol_info->address = -1; // NOT YET DEFINED
      symbol_info->exported = false;
      symbol_info->imported = true;
      symbol_info->referenced = false;
      symtabInstall(symtab, instr.u.format2.addr, symbol_info);
    } else {
      symbol_info->imported = true;
    }
  }


  // If we get here, we are getting a line from the parser which is simply an instruction line
  // So we need to check if symbols either need to be installed or referenced
  // Only 3 instruction formats hold addresses (2,5,8)

  // FORMAT 2 INSTR
  if(instr.format == 2) {
    
    symbol_info_t *symbol_info;

    if((symbol_info = symtabLookup(symtab, instr.u.format2.addr)) == NULL) {

      // Symbol doesnt exist so alloc space for its data structure
      symbol_info = malloc(sizeof(symbol_info_t));
      if(symbol_info == NULL) {
        fprintf(stderr, "Alloc failed for struct\n");
        return;
      }

      symbol_info->address = -1; // NOT YET DEFINED
      symbol_info->referenced = true;
      symbol_info->exported = false;
      symbol_info->imported = false;

      symtabInstall(symtab, instr.u.format2.addr, symbol_info);

    // Check if instr 2 format symbol is export/import directive
    // that has already been created, if so, these are label references
    } else {
      symbol_info->referenced = true;  
    }
  
  // FORMAT 5 INSTR
  } else if(instr.format == 5) {

    symbol_info_t *symbol_info;

    if((symbol_info = symtabLookup(symtab, instr.u.format5.addr)) == NULL) {

      // Symbol doesnt exist so alloc space for its data structure
      symbol_info = malloc(sizeof(symbol_info_t));
      if(symbol_info == NULL) {
        return;
      }

      symbol_info->address = -1; // NOT YET DEFINED
      symbol_info->referenced = true;
      symbol_info->exported = false;
      symbol_info->imported = false;

      symtabInstall(symtab, instr.u.format5.addr, symbol_info);

    } else {
      symbol_info->referenced = true;
    }

  // FORMAT 8 INSTR
  } else if(instr.format == 8) {

    symbol_info_t *symbol_info;

    if((symbol_info = symtabLookup(symtab, instr.u.format8.addr)) == NULL) {

      // Symbol doesnt exist so alloc space for its data structure
      symbol_info = malloc(sizeof(symbol_info_t));
      if(symbol_info == NULL) {
        return;
      }

      symbol_info->address = -1; // NOT YET DEFINED
      symbol_info->referenced = true;
      symbol_info->exported = false;
      symbol_info->imported = false;

      symtabInstall(symtab, instr.u.format8.addr, symbol_info);

    } else {
      symbol_info->referenced = true;
    }
  }
  
  

  // Update our pc counter
  if(instr.format == 9) {

    // 1 for word
    if(strcmp(instr.opcode, "word") == 0) {
      pc++;

    // N for alloc
    } else if(strcmp(instr.opcode, "alloc") == 0) {
      pc += instr.u.format9.constant;
    }

  // Update pc counter for everything elese besides import and export directives
  } else if(strcmp(instr.opcode, "import") != 0 && strcmp(instr.opcode, "export") != 0) {
    pc++;
  }

#if DEBUG
  fprintf(stderr, "assemble called:\n");
  if (label)
  {
    fprintf(stderr, "  label is %s\n", label);
  }
  if (instr.format != 0)
  {
    fprintf(stderr, "  instruction is %s", instr.opcode);
    switch(instr.format)
    {
      case 1:
        fprintf(stderr, "\n");
        break;
      case 2:
        fprintf(stderr, " %s\n", instr.u.format2.addr);
        break;
      case 3:
        fprintf(stderr, " r%d\n", instr.u.format3.reg);
        break;
      case 4:
        fprintf(stderr, " r%d,%d\n", instr.u.format4.reg,
          instr.u.format4.constant);
        break;
      case 5:
        fprintf(stderr, " r%d,%s\n", instr.u.format5.reg,
          instr.u.format5.addr);
        break;
      case 6:
        fprintf(stderr, " r%d,r%d\n", instr.u.format6.reg1,
          instr.u.format6.reg2);
        break;
      case 7:
        fprintf(stderr, " r%d,%d(r%d)\n", instr.u.format7.reg1,
          instr.u.format7.offset, instr.u.format7.reg2);
        break;
      case 8:
        fprintf(stderr, " r%d,r%d,%s\n", instr.u.format8.reg1,
          instr.u.format8.reg2, instr.u.format8.addr);
        break;
      case 9:
        fprintf(stderr, " %d\n", instr.u.format9.constant);
        break;
      default:
        bug("unexpected instruction format (%d) in assemble", instr.format);
        break;
    }
  }
#endif
}

// this is called between passes and provides the assembler the file
// pointer to use for outputing the object file
//
// it returns the number of errors seen on pass1
//
int betweenPasses(FILE *outf) {

  // If there are no errors, continue with symbol printing
  if(error_count == 0) {
  
    // Create our symbol table iterator
    void *iterator = symtabCreateIterator(symtab);
    if(iterator == NULL) {
      fprintf(stderr, "Symtab iterator creation failed.");
    }

    // Create our BST 
    void *BSTroot = symtabCreateBST(iterator);
    if(BSTroot == NULL) {
      fprintf(stderr, "BST ROOT IS NULL!!\n");
    }

    // Create our BST iterator
    void *BSTiterator = symtabCreateBSTIterator(BSTroot);
    if(BSTiterator == NULL) {
      fprintf(stderr, "BST ITERATOR IS NULL!!\n");
    }


    const char *symbol;
    void *return_data;

    // Loop through BST and print symbols and associated data values
    while((symbol = symtabBSTNext(BSTiterator, &return_data)) != NULL) {

      symbol_info_t *symbol_info = return_data;

      printf("%s",symbol);

      if(symbol_info->address != -1) {
        printf(" %i", symbol_info->address);
      }

      if(symbol_info->referenced == true) {
        printf(" referenced");
      }
      if(symbol_info->exported == true) {
        printf(" exported");
      }
      if(symbol_info->imported == true) {
        printf(" imported");
      }
      printf("\n");
    }
  }
  
#if DEBUG
  fprintf(stderr, "betweenPasses called\n");
#endif
  return 0;
}