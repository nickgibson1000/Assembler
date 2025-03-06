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

// Struct to hold symbol and its information
typedef struct symbol_info {
  int address;
  bool exported;
  bool imported;
  bool referenced; 
  //void *symtab_handle; // Symbol table reference
} symbol_info_t;




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


  // Import/Export
  if(instr.format == 2 && (strcmp(instr.opcode, "export") == 0 || strcmp(instr.opcode, "import") == 0 )) {

    if(strcmp(instr.opcode, "export") == 0) {

      if(symtabLookup(symtab, instr.u.format2.addr) == NULL) {

        symbol_info_t *symbol_info = malloc(sizeof(symbol_info_t));
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

    } else if(strcmp(instr.opcode, "import") == 0) {

      if(symtabLookup(symtab, instr.u.format2.addr) == NULL) {

        symbol_info_t *symbol_info = malloc(sizeof(symbol_info_t));
        if(symbol_info == NULL) {
          fprintf(stderr, "Struct Failed\n");
          return;
        }

        symbol_info->address = -1; // NOT YET DEFINED
        symbol_info->exported = false;
        symbol_info->imported = true;
        symbol_info->referenced = false;

        symtabInstall(symtab, label, symbol_info);
      }
    }

  // A symbol is being defined on its own line
  } else if(label != NULL) {
    symbol_info_t *symbol_info;

    // If this is the first time the symbol appears in the asm file...install it
    if((symbol_info = symtabLookup(symtab, label)) == NULL) {

      symbol_info = malloc(sizeof(symbol_info_t));
      if(symbol_info == NULL) {
        fprintf(stderr, "Symbol install failed\n");
        return;
      }
    }

      symbol_info->address = pc;
      symbol_info->referenced = false;

      symtabInstall(symtab, label, symbol_info);

      // We need to decrement the pc counter each time a symbol is defined
      // because the line the symbol is defined on and the next line are treated as one for expected outputs.
      // But to the parser they are treated seperately so we must account for extra pc counts...
      pc--;
  }  

  if(instr.format == 2 && (strcmp(instr.opcode, "export") == 0 || strcmp(instr.opcode, "import") == 0 )) {
    symbol_info_t *symbol_info = symtabLookup(symtab, instr.u.format2.addr);
    symbol_info->referenced = true;
  }
  if (instr.format == 5) {
    symbol_info_t *symbol_info = symtabLookup(symtab, instr.u.format5.addr);
    symbol_info->referenced = true;


  } else if (instr.format == 8) {
    symbol_info_t *symbol_info = symtabLookup(symtab, instr.u.format8.addr);
    symbol_info->referenced = true;
  }

  

  // Update our pc counter
  if(instr.format == 9) {

    // 1 for word
    if(strcmp(instr.opcode, "word") == 0) {
      pc++;
    // n for alloc
    } else if(strcmp(instr.opcode, "alloc") == 0) {
      pc += instr.u.format9.constant;
    }

  // Check if we have an import or export directive
  } else if(instr.format == 2) {

    // Dont update pc counter if import or export
    if(strcmp(instr.opcode, "import") != 0 && strcmp(instr.opcode, "export") != 0) {
      pc++;
    }
    
  // Update pc counter for everything else
  } else {
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
  
  // Create our symbol table iterator
  void *iterator = symtabCreateIterator(symtab);
  if(iterator == NULL) {
    fprintf(stderr, "Symtab iterator creation failed.");
  }

  void *BSTroot = symtabCreateBST(iterator);
  if(BSTroot == NULL) {
    fprintf(stderr, "BST ROOT IS NULL!!\n");
  }
  void *BSTiterator = symtabCreateBSTIterator(BSTroot);
  if(BSTiterator == NULL) {
    fprintf(stderr, "BST ITERATOR IS NULL!!\n");
  }
  

  const char *symbol;
  void *return_data;

  while((symbol = symtabBSTNext(BSTiterator, &return_data)) != NULL) {

    symbol_info_t *symbol_info = return_data;

    printf("%s",symbol);
    printf(" %i", symbol_info->address);

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
  

#if DEBUG
  fprintf(stderr, "betweenPasses called\n");
#endif
  return 0;
}



