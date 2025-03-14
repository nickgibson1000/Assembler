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


// Error definitions
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

// Max number of words(data) that can appear in a file
#define MAX_WORDS 1048576

// Global PC counter
static int pc = 0;

// Global symtab struct
static void *symtab;

// Global error tracker
static int error_count = 0;

// Global variable to track # of ERROR_OPERAND_FORMAT
static int bad_operand = 0;

// Struct to hold information
typedef struct symbol_info {
  int address; // Pc address
  bool exported; 
  bool imported;
  bool referenced; 
  bool defined;
  int import_reference_count; // # times an imported symbol has been referenced
  int export_count; // # times a symbol has been exported (for error checking)
  int import_count; // # times a symbol has been imported (for error checking)
} symbol_info_t;

// Number of codes + directives in vmx20 system
#define OPCODE_ARRAY_LENGTH 30

// Struct used for the below table 
typedef struct opcode_struct{
  char *opcode_string;
  int opcode_value;
  int format;
}opcode_struct_t;

// Tables holding all opcodes, their associated hex value and their format number
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

  // ERROR CHECK: UNKNOWN OPCODE AND INVALID OPERANDS FOR FORMAT
  if (instr.opcode != NULL) {

    int valid = 0;

    for (int i = 0; i < OPCODE_ARRAY_LENGTH; i++) {

      if (strcmp(instr.opcode, opcodes[i].opcode_string) == 0) {
        valid = 1;  // Found a match
        
        // Check if operands match the expected format
        if (instr.format != opcodes[i].format) {

          //ERROR CHECK: OPCODE HAS THE INCORRECT OPERAND FORMAT
          error(ERROR_OPERAND_FORMAT);
          bad_operand++;
          error_count++;
        }

        break; // Exit loop once a match is found
      }
    }

    if (!valid) {

      // ERROR CHECK: UNKNOWN OPCODE ENCOUNTERED
      error(ERROR_OPCODE_UNKNOWN, instr.opcode);
      error_count++;
    }
}

  symbol_info_t *symbol_info;

  if (instr.format == 0 || label) {

    // If this is the first time the symbol appears in the asm file...install it
    if ((symbol_info = symtabLookup(symtab, label)) == NULL) {

        symbol_info = malloc(sizeof(symbol_info_t));
        if (symbol_info == NULL) {
            fprintf(stderr, "Symbol install failed\n");
            return;
        }

        symbol_info->address = pc;
        symbol_info->referenced = false;
        symbol_info->imported = false;
        symbol_info->exported = false;
        symbol_info->export_count = 0;
        symbol_info->import_count = 0;
        symbol_info->defined = true;
        symbol_info->import_reference_count = 0;

        symtabInstall(symtab, label, symbol_info);

        if (instr.format == 2) {

            if ((symbol_info = symtabLookup(symtab, instr.u.format2.addr)) == NULL) {

                symbol_info = malloc(sizeof(symbol_info_t));
                if (symbol_info == NULL) {
                    fprintf(stderr, "Alloc failed for struct\n");
                    return;
                }

                symbol_info->address = -1; // NOT DEFINED
                symbol_info->referenced = true;
                symbol_info->exported = false;
                symbol_info->imported = false;
                symbol_info->import_count = 0;
                symbol_info->export_count = 0;
                symbol_info->import_reference_count = 0;

                symtabInstall(symtab, instr.u.format2.addr, symbol_info);
                symbol_info->import_reference_count++;
            } else {
                symbol_info->referenced = true;
            }

        } else if (instr.format == 5) {

            if ((symbol_info = symtabLookup(symtab, instr.u.format5.addr)) == NULL) {

                symbol_info = malloc(sizeof(symbol_info_t));
                if (symbol_info == NULL) {
                    return;
                }

                symbol_info->address = -1; // NOT DEFINED
                symbol_info->referenced = true;
                symbol_info->exported = false;
                symbol_info->imported = false;
                symbol_info->import_count = 0;
                symbol_info->import_reference_count = 0;

                symtabInstall(symtab, instr.u.format5.addr, symbol_info);
                symbol_info->import_reference_count++;
            } else {
                symbol_info->referenced = true;
            }
        } else if (instr.format == 8) {
            if ((symbol_info = symtabLookup(symtab, instr.u.format8.addr)) == NULL) {
                symbol_info = malloc(sizeof(symbol_info_t));
                if (symbol_info == NULL) {
                    return;
                }

                symbol_info->address = -1; // NOT DEFINED
                symbol_info->referenced = true;
                symbol_info->exported = false;
                symbol_info->imported = false;
                symbol_info->import_count = 0;
                symbol_info->import_reference_count = 0;

                symtabInstall(symtab, instr.u.format8.addr, symbol_info);
                symbol_info->import_reference_count++;
            } else {
                symbol_info->referenced = true;
            }
        }

    // ERROR: Duplicate symbol definition
    } else if (symbol_info->address != -1) {

        error_count++;
        error(ERROR_LABEL_DEFINED, label);

    } else if (symbol_info->address == -1) {
        symbol_info->address = pc;
        symbol_info->defined = true;
        symbol_info->import_reference_count++;
        
    } 

    if(instr.format == 2) {

      symbol_info = symtabLookup(symtab, instr.u.format2.addr);
      symbol_info->referenced = true;

      if(strcmp(instr.opcode,"import") == 0) {
        symbol_info->import_reference_count++;
      }
    }

    if(instr.format == 5) {

      symbol_info = symtabLookup(symtab, instr.u.format5.addr);
      symbol_info->referenced = true;
    }

    if(instr.format == 8) {

      symbol_info = symtabLookup(symtab, instr.u.format8.addr);
      symbol_info->referenced = true;
    }

    

    // We need to decrement the pc counter each time a symbol is defined
    if (instr.format == 0) {
        pc--;
    }
  // Export directive
  } else if((strcmp(instr.opcode, "export")) == 0) {

    

    if(bad_operand < 1) {
      if((symbol_info = symtabLookup(symtab, instr.u.format2.addr)) == NULL) {

        symbol_info = malloc(sizeof(symbol_info_t));
        if(symbol_info == NULL) {
          fprintf(stderr, "Struct Failed\n");
          return;
        }

        symbol_info->address = -1; // NOT YET DEFINED
        symbol_info->exported = true;
        symbol_info->imported = false;
        symbol_info->referenced = false;
        symbol_info->defined = false;
        symbol_info->export_count = 1;
        symbol_info->import_count = 0;

        symtabInstall(symtab, instr.u.format2.addr, symbol_info);
      } else {
        symbol_info->exported = true;
        symbol_info->export_count++;

        if(symbol_info->export_count > 1) {
          error(ERROR_MULTIPLE_EXPORT, instr.u.format2.addr);
          error_count++;
        }

      }
    }


  // Import directive  
  } else if(strcmp(instr.opcode, "import") == 0) {

    if(bad_operand < 1) {
    if((symbol_info = symtabLookup(symtab, instr.u.format2.addr)) == NULL) {

      symbol_info = malloc(sizeof(symbol_info_t));
      if(symbol_info == NULL) {
        fprintf(stderr, "Struct Failed\n");
        return;
      }

      symbol_info->address = -1; // NOT YET DEFINED
      symbol_info->exported = false;
      symbol_info->imported = true;
      symbol_info->referenced = false;
      symbol_info->defined = false;
      symbol_info->export_count = 0;
      symbol_info->import_count = 1;
      symbol_info->import_reference_count = 0;

      symtabInstall(symtab, instr.u.format2.addr, symbol_info);
    } else {
      // Check if the symbol is already marked as imported
      if (!symbol_info->imported) {
        symbol_info->imported = true;
      }

      symbol_info->import_count++;

    }
  }


  // If we get here, we are getting a line from the parser which is simply an instruction line
  // So we need to check if symbols either need ot be installed or referenced
  } else if(instr.format == 2) {
      
      if((symbol_info = symtabLookup(symtab, instr.u.format2.addr)) == NULL) {

        // Symbol doesnt exist so alloc space for its data structure
        symbol_info = malloc(sizeof(symbol_info_t));
        if(symbol_info == NULL) {
          fprintf(stderr, "Alloc failed for struct\n");
          return;
        }

        symbol_info->address = -1; // NOT DEFINED
        symbol_info->referenced = true;
        symbol_info->exported = false;
        symbol_info->imported = false;
        symbol_info->export_count = 0;

        symtabInstall(symtab, instr.u.format2.addr, symbol_info);
        symbol_info->import_reference_count++;


      // Check if instr 2 format symbol is export/import directive
      // that has already been created, if so, these are label references
      } else {
        symbol_info->referenced = true;  
        symbol_info->import_reference_count++;
      }
      
    } else if(instr.format == 5) {
      if((symbol_info = symtabLookup(symtab, instr.u.format5.addr)) == NULL) {

        // Symbol doesnt exist so alloc space for its data structure
        symbol_info = malloc(sizeof(symbol_info_t));
        if(symbol_info == NULL) {
          return;
        }

        symbol_info->address = -1; // NOT DEFINED
        symbol_info->referenced = true;
        symbol_info->exported = false;
        symbol_info->imported = false;
        symbol_info->export_count = 0;

        symtabInstall(symtab, instr.u.format5.addr, symbol_info);
        symbol_info->import_reference_count++;


      } else {
        symbol_info->referenced = true;
        symbol_info->import_reference_count++;
      }


    } else if(instr.format == 8) {

      if((symbol_info = symtabLookup(symtab, instr.u.format8.addr)) == NULL) {

        // Symbol doesnt exist so alloc space for its data structure
        symbol_info = malloc(sizeof(symbol_info_t));
        if(symbol_info == NULL) {
          return;
        }

        symbol_info->address = -1; // NOT DEFINED
        symbol_info->referenced = true;
        symbol_info->exported = false;
        symbol_info->imported = false;
        symbol_info->export_count = 0;

        symtabInstall(symtab, instr.u.format8.addr, symbol_info);
        symbol_info->import_reference_count++;


      } else {
        symbol_info->referenced = true;
        symbol_info->import_reference_count++;
      }
  }
  
  
  // Update our pc counter
  if(instr.format == 9) {

    // N words for alloc
    if(strcmp(instr.opcode, "alloc") == 0) {

      // ERROR CHECK: IF ALLOC CONSTANT IS 0
      if(instr.u.format9.constant <= 0) {

        error(ERROR_CONSTANT_ZERO, instr.u.format9.constant);
        error_count++;

      } else {
        pc += instr.u.format9.constant; // Else update pc by alloc constant
      }

    // 1 word for word
    } else if(strcmp(instr.opcode, "word") == 0) {
      pc++;
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


  // ERROR CHECK: PROGRAM EXCEEDS MAX WORD SIZE
  if((pc * 5) > MAX_WORDS) {
    error(ERROR_PROGRAM_SIZE);
    error_count++;
  }

  // ERROR CHECK: OFFSET DOES NOT FIT IN 16 BITS
  if(instr.format == 7) {
    if(instr.u.format7.offset >= (1 << 15) || instr.u.format7.offset < -(1 << 15)) {
      error(ERROR_OFFSET_INVALID, instr.u.format7.offset);
      error_count++;
    }
  }

  // ERROR CHECK: CONSTANT DOES NOT FIT IN 20 BITS
  if(instr.format == 4) {
    if(instr.u.format4.constant >= (1 << 19) || instr.u.format4.constant < -(1 << 19)) {
      error(ERROR_CONSTANT_INVALID, instr.u.format4.constant);
      error_count++;
    }
  }
}

// this is called between passes and provides the assembler the file
// pointer to use for outputing the object file
//
// it returns the number of errors seen on pass1
//
int betweenPasses(FILE *outf) {


  // Error Checking
  // Identical to proccessing all symbols and printing appropriate information to our object file
  // But we use error names to signify all error

  if((pc * 5) < MAX_WORDS && bad_operand < 1) { 
    

    // Create dummy error symtab structs
    void *error_iterator = symtabCreateIterator(symtab);
    if(error_iterator == NULL) {
      fprintf(stderr, "Symtab iterator creation failed.");
    }

    void *error_BSTroot = symtabCreateBST(error_iterator);
    if(error_BSTroot == NULL) {
      fprintf(stderr, "BST ROOT IS NULL!!\n");
    }
    void *error_BSTiterator = symtabCreateBSTIterator(error_BSTroot);
    if(error_BSTiterator == NULL) {
      fprintf(stderr, "BST ITERATOR IS NULL!!\n");
    }

    const char *error_symbol;
    void *error_return_data;


    // Go through our symtab tree and check all symbols for possible errors
    while((error_symbol = symtabBSTNext(error_BSTiterator, &error_return_data)) != NULL) {
      
      symbol_info_t *error_symbol_info = error_return_data;

      // ERROR: SYMBOL IS IMPORTED AND EXPORTED
      if(error_symbol_info->imported == true && error_symbol_info->exported == true) {
        error(ERROR_SYMBOL_IMPORT_EXPORT, error_symbol);
        error_count++;
      }

      // ERROR CHECK: IF SYMBOL IS IMPORTED BUT ALSO DEFINED
      if(error_symbol_info->defined == true && error_symbol_info->imported == true) {
        error(ERROR_SYMBOL_IMPORT_DEFINED, error_symbol);
        error_count++;
      }

      // ERROR CHECK: IF SYMBOL IS IMPORTED MULTIPLE TIMES
      if(error_symbol_info->import_count > 1) {
        error(ERROR_MULTIPLE_IMPORT, error_symbol);
        error_count++;
      }

      // ERROR CHECK: IF SYMBOL IS IMPORTED BUT NOT REFERENCD
      if(error_symbol_info->referenced == false && error_symbol_info->imported == true) {
        error(ERROR_SYMBOL_IMPORT_NO_REFERENCE, error_symbol);
        error_count++;
      }

      // ERROR CHECK: IF SYMBOL IS REFERENCED BUT NOT DEFINED OR IMPORTED
      if(error_symbol_info->referenced == true && error_symbol_info->defined == false && error_symbol_info->imported == false) {
        error(ERROR_LABEL_REFERENCE_NOT_FOUND, error_symbol);
        error_count++;
      }

      // ERROR CHECK: IF SYMBOL IS EXPORTED BUT HAS NO DEFINITION
      if(error_symbol_info->exported == true && error_symbol_info->defined == false) {
        error(ERROR_SYMBOL_EXPORT_NO_DEFINITION, error_symbol);
        error_count++;
      }

    }
  }


  // If we have no errors, we can proceed with processing all required information
  

  if(error_count == 0) {
    
    int import_symbol_references = 0;
  
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
    int exported_count = 0;

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
        exported_count++;
      }
      if(symbol_info->imported == true) {
        printf(" imported");
        import_symbol_references += symbol_info->import_reference_count;
      }
      printf("\n");
    }

  
  int inSymbol_size = exported_count * 5;
  int outSymbol_size = import_symbol_references * 5;
  int program_size = pc;

 // Write header of object file
  fwrite(&inSymbol_size, sizeof(int), 1, outf);
  fwrite(&outSymbol_size, sizeof(int), 1, outf);
  fwrite(&program_size, sizeof(int), 1, outf);

  }
  
  return error_count;
}