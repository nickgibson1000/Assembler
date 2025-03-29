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
#include <stdint.h>


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

// Max value that can be stored in a 20 bit value
#define MAX_20_BIT_VALUE 0xFFFFF

// Max value tha can be stored in a 16 bit value
#define MAX_16_BIT_VALUE 0xFFFF

// Global PC counter
static int pc = 0;

static int pc2 = 0;

// Global symtab struct
static void *symtab;

// Global error tracker
static int error_count = 0;

// Global variable to track # of ERROR_OPERAND_FORMAT
static int bad_operand = 0;

// Global variable to track # of ERROR_CONSTANT_INVALID
static int constant_unfit = 0;

// Global variable to track # of ERROR_OPCODE_UNKOWN
static int unknown_opcode = 0;

// If value is 1, we are on pass one
// If value is 2, we are on pass two
static int pass_counter = 1;

static FILE *file_pointer = NULL; // Initialize to NULL

// A linked list used to track each time an imported symbol is referenced
typedef struct Node {
  int address; 
  struct Node *next;
} Node_t;


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
  Node_t *reference_address; // A linked list of all addresses when an imported symbol is referenced
} symbol_info_t;

// Number of opcodes + directives in vmx20 system
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


// Function Prototypes
// Descriptions can be found towards end of file

static void *create_data_node();

static void intialize_symbol_info(symbol_info_t *symbol_info, int address, bool referenced, 
                                  bool imported, bool exported, bool defined);

static int find_opcode(char *opcode);          

static void update_pc(int *pc_counter, INSTR instr);







// this is called once so that the assembler can initialize any internal
// data structures.
void initAssemble(void) {

  // Intialize our symbol table
  // Size can be arbitrary
  symtab = symtabCreate(100);


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

  if(pass_counter == 1) {
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
      unknown_opcode++;
      error_count++;
    }
}
  // Create a handle that will be used to store symbol information
  symbol_info_t *symbol_info;



  if (instr.format == 0 || label) {

    // If this is the first time the symbol appears in the asm file...install it
    if ((symbol_info = symtabLookup(symtab, label)) == NULL) {

      // Symbol doesnt exist, create a new symbol_info struct for it
      symbol_info = create_data_node();

      intialize_symbol_info(symbol_info, pc, false, false, false, true);

      // Install symbol into our table
      symtabInstall(symtab, label, symbol_info);

      /*
        
      */
      if (instr.format == 2) {

        if ((symbol_info = symtabLookup(symtab, instr.u.format2.addr)) == NULL) {

          // Symbol doesnt exist, create a new symbol_info struct for it
          symbol_info = create_data_node();

          intialize_symbol_info(symbol_info, -1, true, false, false, false);

          symtabInstall(symtab, instr.u.format2.addr, symbol_info);

          symbol_info->import_reference_count++;
              
          Node_t *node = malloc(sizeof(Node_t));
          if(node == NULL) {
            exit(-1);
          }

          node->address = pc;
          node->next = symbol_info->reference_address;
          symbol_info->reference_address = node;
              

            } else {
                symbol_info->referenced = true;
            }

        } else if (instr.format == 5) {

            if ((symbol_info = symtabLookup(symtab, instr.u.format5.addr)) == NULL) {

                // Symbol doesnt exist, create a new symbol_info struct for it
                symbol_info = create_data_node();

                intialize_symbol_info(symbol_info, -1, true, false, false, false);

                symtabInstall(symtab, instr.u.format5.addr, symbol_info);
                symbol_info->import_reference_count++;

                Node_t *node = malloc(sizeof(Node_t));
                if(node == NULL) {
                  exit(-1);
                }

                node->address = pc;
                node->next = symbol_info->reference_address;
                symbol_info->reference_address = node;

            } else {
                symbol_info->referenced = true;
            }
        } else if (instr.format == 8) {
            if ((symbol_info = symtabLookup(symtab, instr.u.format8.addr)) == NULL) {
                
                // Symbol doesnt exist, create a new symbol_info struct for it
                symbol_info = create_data_node();

                intialize_symbol_info(symbol_info, -1, true, false, false, false);

                symtabInstall(symtab, instr.u.format8.addr, symbol_info);
                symbol_info->import_reference_count++;

                Node_t *node = malloc(sizeof(Node_t));
                if(node == NULL) {
                  exit(-1);
                }

                node->address = pc;
                node->next = symbol_info->reference_address;
                symbol_info->reference_address = node;
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

        Node_t *node = malloc(sizeof(Node_t));
        if(node == NULL) {
          exit(-1);
        }

        node->address = pc;
        node->next = symbol_info->reference_address;
        symbol_info->reference_address = node;
        
    } 

    if(instr.format == 2) {

      symbol_info = symtabLookup(symtab, instr.u.format2.addr);
      symbol_info->referenced = true;

      if(strcmp(instr.opcode,"import") == 0) {

        Node_t *node = malloc(sizeof(Node_t));
        if(node == NULL) {
          exit(-1);
        }

        node->address = pc;
        node->next = symbol_info->reference_address;
        symbol_info->reference_address = node;
       
        symbol_info->reference_address = node;

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

    

    // We need to decrement the pc counter each time a symbol is defined in this format
    // because of the two possiblities
    if (instr.format == 0) {
        pc--;
    }


  // Export directive
  } else if((strcmp(instr.opcode, "export")) == 0) {

    

    if(bad_operand < 1) {
      if((symbol_info = symtabLookup(symtab, instr.u.format2.addr)) == NULL) {

        // Symbol doesnt exist, create a new symbol_info struct for it
        symbol_info = create_data_node();

        intialize_symbol_info(symbol_info, -1, false, false, true, false);
        
        symbol_info->export_count = 1;
      
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

      // Symbol doesnt exist, create a new symbol_info struct for it
      symbol_info = create_data_node();

      intialize_symbol_info(symbol_info, -1, false, true, false, false);

      symbol_info->import_count = 1;

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
  // So we need to check if symbols either need to be installed or referenced
  } else if(instr.format == 2) {
      
      if((symbol_info = symtabLookup(symtab, instr.u.format2.addr)) == NULL) {

        // Symbol doesnt exist, create a new symbol_info struct for it
        symbol_info = create_data_node();

        intialize_symbol_info(symbol_info, -1, true, false, false, false);

        symtabInstall(symtab, instr.u.format2.addr, symbol_info);
        symbol_info->import_reference_count++;
        Node_t *node = malloc(sizeof(Node_t));
        if(node == NULL) {
          exit(-1);
        }

        node->address = pc;
        node->next = symbol_info->reference_address;
        symbol_info->reference_address = node;
       
        symbol_info->reference_address = node;


      // Check if instr 2 format symbol is export/import directive
      // that has already been created, if so, these are label references
      } else {
      
        symbol_info->referenced = true;  
        symbol_info->import_reference_count++;
        Node_t *node = malloc(sizeof(Node_t));
        if(node == NULL) {
          exit(-1);
        }

        node->address = pc;
        node->next = symbol_info->reference_address;
        symbol_info->reference_address = node;
       
        symbol_info->reference_address = node;
      }
      
    } else if(instr.format == 5) {

      if((symbol_info = symtabLookup(symtab, instr.u.format5.addr)) == NULL) {

        // Symbol doesnt exist, create a new symbol_info struct for it
        symbol_info = create_data_node();

        intialize_symbol_info(symbol_info, -1, true, false, false, false);


        symtabInstall(symtab, instr.u.format5.addr, symbol_info);
        symbol_info->import_reference_count++;
        Node_t *node = malloc(sizeof(Node_t));
        if(node == NULL) {
          exit(-1);
        }

        node->address = pc;
        node->next = symbol_info->reference_address;
        symbol_info->reference_address = node;
       
        symbol_info->reference_address = node;


      } else {
        symbol_info->referenced = true;
        symbol_info->import_reference_count++;
        
        Node_t *node = malloc(sizeof(Node_t));
        if(node == NULL) {
          exit(-1);
        }

        node->address = pc;
        node->next = symbol_info->reference_address;
        symbol_info->reference_address = node;
       
        symbol_info->reference_address = node;
      }


    } else if(instr.format == 8) {

      if((symbol_info = symtabLookup(symtab, instr.u.format8.addr)) == NULL) {

        
        // Symbol doesnt exist, create a new symbol_info struct for it
        symbol_info = create_data_node();

        intialize_symbol_info(symbol_info, -1, true, false, false, false);


        symtabInstall(symtab, instr.u.format8.addr, symbol_info);
        symbol_info->import_reference_count++;

        Node_t *node = malloc(sizeof(Node_t));
        if(node == NULL) {
          exit(-1);
        }

        node->address = pc;
        node->next = symbol_info->reference_address;
        symbol_info->reference_address = node;
       
        symbol_info->reference_address = node;


      } else {
        symbol_info->referenced = true;
        symbol_info->import_reference_count++;

        Node_t *node = malloc(sizeof(Node_t));
        if(node == NULL) {
          exit(-1);
        }

        node->address = pc;
        node->next = symbol_info->reference_address;
        symbol_info->reference_address = node;
       
        symbol_info->reference_address = node;
      }
  }


  // Update pc counter for first pass
  update_pc(&pc, instr);


  // ERROR CHECK: PROGRAM EXCEEDS MAX WORD SIZE
  if(pc > MAX_WORDS) {
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
      constant_unfit++;
      error_count++;
    }
  }
} // END OF PASS 1 CHECK





// SECOND PASS
if(pass_counter == 2) {
 
  if(instr.format == 1) {


    /*
    Halt
    Ret
    */

    int opcode = find_opcode(instr.opcode);

    int encoding = (opcode & 0xFF); // bits 0-7

  
    fwrite(&encoding, sizeof(int), 1, file_pointer);


  } else if(instr.format == 2) {

    /*
    call
    jmp
    import
    export

    We skip encodings for import and export
    */

    if(strcmp(instr.opcode, "jmp") == 0 || strcmp(instr.opcode, "call") == 0) {

      symbol_info_t *symbol_info = symtabLookup(symtab, instr.u.format2.addr);

      int opcode = find_opcode(instr.opcode);


      int address = 0;
      if(!symbol_info->imported) {

        int pc_current = pc2 + 1;
        int pc_new = symbol_info->address;

        /*
        Pc New = Pc Current + Address
        */
        address = pc_new - pc_current;
      }

      // ERROR CHECK: ADDRESS DOES NOT FIT IN 20 BITS
      if(address > 0x7FFFF || address < -0x80000) {
        error(ERROR_LABEL_SIZE20, instr.u.format2.addr);
        error_count++;
      }

      int encoding = ((address & 0xFFFFF) << 12) |
                      (opcode & 0xFF); 

      fwrite(&encoding, sizeof(int), 1, file_pointer);
    }

  } else if(instr.format == 3) {

    int opcode = find_opcode(instr.opcode);
    int r1 = instr.u.format3.reg;


    int encoding = ((r1 & 0xF) << 8) |
                    (opcode & 0xFF);

    fwrite(&encoding, sizeof(int), 1, file_pointer);


  } else if(instr.format == 4) {

    int opcode = find_opcode(instr.opcode);


    int encoding = ((instr.u.format4.constant & 0xFFFFF) << 12) |
                    ((instr.u.format4.reg & 0xF) << 8) |
                    (opcode & 0xFF);

    fwrite(&encoding, sizeof(int), 1, file_pointer);



  } else if(instr.format == 5) {


    symbol_info_t *symbol_info = symtabLookup(symtab, instr.u.format5.addr);

    int encoding = 0;
    int opcode = find_opcode(instr.opcode);
    int register_num = instr.u.format5.reg;

    int address = 0;

    if(!symbol_info->imported) {
      int pc_current = pc2;
      pc_current++;

      int pc_new = symbol_info->address;

      address = pc_new - pc_current;
    }

     // ERROR CHECK: ADDRESS DOES NOT FIT IN 20 BITS
     if(address > 0x7FFFF || address < -0x80000) {
      error(ERROR_LABEL_SIZE20, instr.u.format5.addr);
      error_count++;
    }

    encoding = ((address & 0xFFFFF) << 12) | // Address in upper bits
           ((register_num & 0xF) << 8) |     // Register in middle 4 bits
           (opcode & 0xFF);                  // Opcode in lower 8 bits

    fwrite(&encoding, sizeof(int), 1, file_pointer);




  } else if(instr.format == 6) {

    int opcode = find_opcode(instr.opcode);

    int encoding = ((instr.u.format6.reg2 & 0xF) << 12) |  // reg2 in bits 12-15
                   ((instr.u.format6.reg1 & 0xF) << 8)  |   // reg1 in bits 8-11
                   (opcode & 0xFF);         // opcode in bits 0-7

    fwrite(&encoding, sizeof(int), 1, file_pointer);


  } else if(instr.format == 7) {

    // Get the instructions numeric opcode
    int opcode = find_opcode(instr.opcode);

    int reg1 = instr.u.format7.reg1;
    int reg2 = instr.u.format7.reg2;

    int encoding = ((instr.u.format7.offset & 0xFFFFF) << 16) | // Address in upper bits
                    ((reg2 & 0xF) << 12) |  // reg2 in bits 12-15
                    ((reg1 & 0xF) << 8)  |   // reg1 in bits 8-11
                    (opcode & 0xFF);         // opcode in bits 0-7


    fwrite(&encoding, sizeof(int), 1, file_pointer);


  } else if(instr.format == 8) {

    symbol_info_t *symbol_info = symtabLookup(symtab, instr.u.format8.addr);


    int opcode = find_opcode(instr.opcode);
    int reg1 = instr.u.format8.reg1;
    int reg2 = instr.u.format8.reg2;

    int address = 0;
      if(!symbol_info->imported) {
      int pc_current = pc2;
      pc_current++;

      int pc_new = symbol_info->address;

      address = pc_new - pc_current;
    }

     // ERROR CHECK: ADDRESS DOES NOT FIT IN 16 BITS
     if(address > 0x7FFF || address < -0x8000) {
      error(ERROR_LABEL_SIZE16, instr.u.format8.addr);
      error_count++;
     }

    int encoding = ((address & 0xFFFFF) << 16) | // Address in upper bits
                    ((reg2 & 0xF) << 12) |  // reg2 in bits 12-15
                    ((reg1 & 0xF) << 8)  |   // reg1 in bits 8-11
                    (opcode & 0xFF);         // opcode in bits 0-7


    fwrite(&encoding, sizeof(int), 1, file_pointer);


  } else if(instr.format == 9) {

    /*
    word
    alloc
    */
    int zero = 0;
    if(strcmp(instr.opcode, "word") == 0) {
      fwrite(&instr.u.format9.constant, sizeof(int), 1, file_pointer);
    } else {

      for(int i = 0; i < instr.u.format9.constant; i++) {
        fwrite(&zero, sizeof(int), 1, file_pointer);
      }
    }

  }
}






  // 
  if(label || instr.format == 0) {
    if(instr.format == 0) {
      pc2--;
    }
  }
  // Update pc2 counter for the second pass
  update_pc(&pc2, instr);
}



// this is called between passes and provides the assembler the file
// pointer to use for outputing the object file
//
// it returns the number of errors seen on pass1
//
int betweenPasses(FILE *outf) {

  // Assign the file pointer parameter to a static variable so it can 
  // be accessed in assemble during our second pass for encodings
  if (file_pointer == NULL) { 
    file_pointer = outf; 
  }

  // Set pc2 to 0 after we finish our first pass
  if (pass_counter == 1) {
    pc2 = 0;
  }


  // Error Checking
  // Identical to proccessing all symbols and printing appropriate information to our object file
  // But we use error names to signify all error

  /*
  Due to reasons I hadn't enough time to invesitgate why, I had to make sure that if certain errors happened,
  I never looped through my BST or else a seg fault would happen
  */
  if(pc  < MAX_WORDS && bad_operand < 1 && constant_unfit < 1 && unknown_opcode < 1) { 
    

    // Create dummy error structs to iterate through our symbols
    void *error_iterator = symtabCreateIterator(symtab);
    if(error_iterator == NULL) {
      fprintf(stderr, "ERROR Symtab iterator creation failed.");
    }

    void *error_BSTroot = symtabCreateBST(error_iterator);
    if(error_BSTroot == NULL) {
      fprintf(stderr, "ERROR BST ROOT IS NULL!!\n");
    }
    void *error_BSTiterator = symtabCreateBSTIterator(error_BSTroot);
    if(error_BSTiterator == NULL) {
      fprintf(stderr, "ERROR BST ITERATOR IS NULL!!\n");
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

    // Get the root of our BST 
    void *BSTroot = symtabCreateBST(iterator);
    if(BSTroot == NULL) {
      fprintf(stderr, "BST ROOT IS NULL!!\n");
    }

    // Create our iterator to go through our BST and get all symbols
    // and associated data
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


    /*
    Go through our BST again and write out
      1. All exported symbols and their address to obj file
    */

    // Create our symbol table iterator
    void *iterator2 = symtabCreateIterator(symtab);
    if(iterator == NULL) {
      fprintf(stderr, "Symtab iterator2 creation failed.");
    }

    // Get the root of our BST 
    void *BSTroot2 = symtabCreateBST(iterator2);
    if(BSTroot == NULL) {
      fprintf(stderr, "BST ROOT2 IS NULL!!\n");
    }

    // Create our iterator to go through our BST and get all symbols
    // and associated data
    void *BSTiterator2 = symtabCreateBSTIterator(BSTroot2);
    if(BSTiterator == NULL) {
      fprintf(stderr, "BST ITERATOR2 IS NULL!!\n");
    }


    const char *symbol2;
    void *return_data2;

    while((symbol2 = symtabBSTNext(BSTiterator2, &return_data2)) != NULL) {

      
      symbol_info_t *symbol_info2 = return_data2;

      
        // Write inSymbols to object file

        if(symbol_info2->exported == true) {

        

          // Buffer for symbol name exactly 16 bytes long
          char symbol_name_buffer[100] = {0};
        
          // Copy symbol name, truncating or padding as necessary
          strncpy(symbol_name_buffer, symbol2, sizeof(symbol_name_buffer) - 1);

          // Write the 16-byte symbol name (4 words)
          fwrite(symbol_name_buffer, sizeof(char), 16, outf);

          // Write the address as the 5th word
          fwrite(&symbol_info2->address, sizeof(uint32_t), 1, outf);
    
    
        }
    
    }


    /*
    Go through our BST again and write out
      2. All imported symbols and their referenced addresses to obj file
    */

    // Create our symbol table iterator
    void *iterator3 = symtabCreateIterator(symtab);
    if(iterator == NULL) {
      fprintf(stderr, "Symtab iterator3 creation failed.");
    }

    // Get the root of our BST 
    void *BSTroot3 = symtabCreateBST(iterator3);
    if(BSTroot == NULL) {
      fprintf(stderr, "BST3 ROOT IS NULL!!\n");
    }

    // Create our iterator to go through our BST and get all symbols
    // and associated data
    void *BSTiterator3 = symtabCreateBSTIterator(BSTroot3);
    if(BSTiterator == NULL) {
      fprintf(stderr, "BST3 ITERATOR IS NULL!!\n");
    }


    const char *symbol3;
    void *return_data3;

    while((symbol3 = symtabBSTNext(BSTiterator3, &return_data3)) != NULL) {
      
      symbol_info_t *symbol_info3 = return_data3;
    

      // Write inSymbols to object file
      if(symbol_info3->imported == true) {

        Node_t *current = symbol_info3->reference_address;

        while(current != NULL) {

        
          // Buffer for symbol name exactly 16 bytes long
          char symbol_name_buffer[100] = {0};
        
          // Copy symbol name, truncating or padding as necessary
          strncpy(symbol_name_buffer, symbol3, sizeof(symbol_name_buffer) - 1);


          // Write the 16-byte symbol name (4 words)
          fwrite(symbol_name_buffer, sizeof(char), 16, outf);

          // Write the address as the 5th word
          fwrite(&current->address, sizeof(uint32_t), 1, outf);

          current = current->next;
        }

      }
    }
  }

  // Set pass counter to 2 as right before we enter the pass
  pass_counter = 2;

  // The number of errors encountered on pass 1
  return error_count;
}


/*
Param: A handle to an empty symbol_info structure
Return: A void pointer to our newly created symbol info struct

Allocate memory for a new symbol_info struct 
*/
static void *create_data_node() {

  symbol_info_t *symbol_info = malloc(sizeof(symbol_info_t));

  if(symbol_info == NULL) {
    fprintf(stderr, "Failed to create symbol node...\n");
    exit(-1);
  }

  return (void *) symbol_info;
}

/*
Params: handle to symbol_info struct, various information contained in the symbol data struct

Note: export_count, import_count, and import_reference_count are dealt with in their respective stmts
in assemble function

Intialize all symbol data for passed in symbol_info handle
*/
static void intialize_symbol_info(symbol_info_t *symbol_info, int address, bool referenced, 
                                  bool imported, bool exported, bool defined) {

  symbol_info->address =  address;
  symbol_info->referenced = referenced;
  symbol_info->imported = imported;
  symbol_info->exported = exported;
  symbol_info->defined = defined;
  symbol_info->export_count = 0;
  symbol_info->import_count = 0;
  symbol_info->import_reference_count = 0;

}


/*
Param: A char pointer to an instructions opcode string

Return: The numeric value associated with the passed in opcode string
*/
static int find_opcode(char *opcode) {

  int opcode_val = 0;

  // Find our symbol in the symbol array and grab its numeric opcode
  for(int i = 0; i < OPCODE_ARRAY_LENGTH; i++) {

    if(strcmp(opcodes[i].opcode_string, opcode) == 0) {
      opcode_val = opcodes[i].opcode_value; // Opcode found
  
    }
  }

  return opcode_val;
}                                  


/*
Params: A pointer to whatever pc counter we are choosing to increment (pc or pc2)
        An INSTR variable containing the current instruction we have received from the parser
*/
static void update_pc(int *pc_counter, INSTR instr) {

  // Update our pc counter
  if(instr.format == 9) {

    // N words for alloc
    if(strcmp(instr.opcode, "alloc") == 0) {

      // ERROR CHECK: IF ALLOC CONSTANT IS 0
      if(instr.u.format9.constant <= 0) {

        error(ERROR_CONSTANT_ZERO, instr.u.format9.constant);
        error_count++;

      } else {
        (*pc_counter) += instr.u.format9.constant; // Else update pc by alloc constant
      }

    // 1 word for word
    } else if(strcmp(instr.opcode, "word") == 0) {
      (*pc_counter)++;
    }

  // Check if we have an import or export directive
  } else if(instr.format == 2) {

    // Dont update pc counter if import or export
    if(strcmp(instr.opcode, "import") != 0 && strcmp(instr.opcode, "export") != 0) {
      (*pc_counter)++;
    }

  // Update pc counter for everything else
  } else {
    (*pc_counter)++;
  }
}
