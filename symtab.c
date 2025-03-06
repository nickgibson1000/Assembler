#include "symtab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct node {
  char *symbol; // Key
  void *data; // data
  struct node *next; // Pointer to next node
} node_t;


typedef struct control {
  int symtab_size; // Table size
  node_t **table; // Pointer to an array of linked list heads
} control_t; 

typedef struct iterator {
  node_t *symbol_node; // Node holding current symbol in iterator
  control_t *control_pointer; // Pointer to control structure
  int index; // Current index iterator is at in the symtab
} iterator_t;

typedef struct bst_node {
  struct bst_node *right; // Right child
  struct bst_node *left; // Left child
  struct bst_node *parent; // Parent
  char *symbol; // Key
  void *data; // Data
  bool visited; 
} bst_node_t;

typedef struct bst_iterator {
  bst_node_t *current; 
} bst_iterator_t;


//---------------PROTOTYPES---------------

// Lookup helper 
static node_t* lookup_helper(void *symtabHandle, const char *symbol);

// Hash function
static unsigned int hash(const char *str);

// Create a new node
static bst_node_t *create_node(char *symbol, void *data);

// Add new node into BST
static bst_node_t *insert_node(bst_node_t *root, bst_node_t *node);

// Help free all nodes
static void free_nodes(bst_node_t *node);


//---------------FUNCTIONS---------------

void *symtabCreate(int sizeHint) {

  // Intialize control struct
  control_t *control = malloc(sizeof(control_t));
  if(control == NULL) {
    return NULL;
  }

  // Store size of symbol table in the control struct
  control->symtab_size = sizeHint;

  // Allocate memory for the array of linked lists
  control->table = malloc(sizeHint * sizeof(node_t *));
  if(control->table == NULL) {
    free(control);
    return NULL;
  }

  // Initialize the table entries to NULL
  for (int i = 0; i < sizeHint; i++) {
    control->table[i] = NULL;
  }

  // Return void pointer to the control structure
  return (void*)control;
}

int symtabInstall(void *symtabHandle, const char *symbol, void *data) {

  // Store symtab handle in a control struct
  control_t *control = symtabHandle;

  // Check if node exists
  node_t *node = lookup_helper(control, symbol);

  if(node != NULL) {
    node->data = data;
  } else {

    node_t *new_node = malloc(sizeof(node_t));
    if(new_node == NULL) {
      free(new_node);
      return 0;
    }

    new_node->symbol = malloc(strlen(symbol) + 1);
    if(new_node->symbol == NULL) {
      free(new_node);
      return 0;
    }

    strcpy(new_node->symbol, symbol);
    new_node->data = data;

    int index = hash(symbol) % control->symtab_size;

    new_node->next = control->table[index];

    control->table[index] = new_node;
  }

  return 1;
}

static node_t* lookup_helper(void *symtabHandle, const char *symbol) {

  control_t *control = symtabHandle;

  int index = hash(symbol) % control->symtab_size;

  node_t *head = control->table[index];


  while(head != NULL) {

    if(strcmp(head->symbol, symbol) == 0) {
      return head;
    }

    head = head->next;
  }

  // If symbol is not found
  return NULL;
}

void *symtabLookup(void *symtabHandle, const char *symbol) {

  control_t *control = symtabHandle;

  node_t *node = lookup_helper(control, symbol);

  // Symbol found
  if(node != NULL) {
    return node->data;

  // Symbol not found
  } else {
    return NULL;
  }
}

void symtabDelete(void *symtabHandle) {

  control_t *control = symtabHandle;

  // Free all nodes and symbols in symtab
  for(int i = 0; i < control->symtab_size; i++) {

    node_t *current_node = control->table[i];

    while(current_node != NULL) {
      node_t *prev_node = current_node;
      current_node = current_node->next;
      free(prev_node->symbol);
      free(prev_node);
    }
  } 

  // Free our symtab
  free(control->table);

  // Free control structure
  free(control);
}

void *symtabCreateIterator(void *symtabHandle) {

  control_t *control = symtabHandle;

  iterator_t *iterator = malloc(sizeof(iterator_t));
  if(iterator == NULL) {
    return NULL;
  }

  iterator->control_pointer = control;

  // POSSIBLY DONT NEED: FOR DEBUGGING PURPOSES
  iterator->index = 0;
  iterator->symbol_node = NULL;

  // Find first index
  for(int i = 0; i < control->symtab_size; i++) {

    if(control->table[i] != NULL) {

      iterator->index = i;
      //node_t *first_node = control->table[i];
      iterator->symbol_node = control->table[i];
      return (void*) iterator;
    }
  }

  
  iterator->control_pointer->table = NULL;
  
  return (void*) iterator;
}

const char *symtabNext(void *iteratorHandle, void **returnData) {


  iterator_t *iterator = iteratorHandle;

  // Get the node containing iterators current symbol
  node_t *node = iterator->symbol_node;

  // Validate node is not null
  if(node == NULL) {
    return NULL;
  }
  
  // Store the nodes data in output parameter
  *returnData = node->data;

  // Search for next node 
  if(node->next != NULL) {
    iterator->symbol_node = node->next; // Node found in same linked list
  
  // Node not found so we must increase our index and search the rest of the table
  } else {

    iterator->index++;

    while (iterator->index < iterator->control_pointer->symtab_size && iterator->control_pointer->table[iterator->index] == NULL) {

      iterator->index++;
    }

    if(iterator->index < iterator->control_pointer->symtab_size) {
      // Store next symbol in iterator
      iterator->symbol_node = iterator->control_pointer->table[iterator->index];
    } else {
      iterator->symbol_node = NULL;
    }
    
  }
  // Return new symbol
  return node->symbol;
}

void symtabDeleteIterator(void *iteratorHandle) {
  
  // Free all memory consumed by iterator
  iterator_t *iterator = iteratorHandle;
  free(iterator);
}

void *symtabCreateBST(void *iteratorHandle) {

  bst_node_t *root = NULL;

  char *symbol;
  void *data;

  // Get our iterator 
  iterator_t *iterator = iteratorHandle;

  // Install all symbols into the BST
  while ((symbol = (char *)symtabNext(iterator, &data)) != NULL) {

    //printf("Symbol: %s\n", symbol);

    // Create our new node
    bst_node_t *new_node = create_node(symbol, data);

    // Insert the new node into the tree
    if (root == NULL) {
      // If root is NULL, assign it to the new node
      root = new_node;

    } else {
      // Otherwise, insert the new node into the existing tree
      insert_node(root, new_node);
    }
  } 

  // Return pointer to BST root
  return (void *)root;
}

static bst_node_t *create_node(char *symbol, void *data) {
  
  bst_node_t *new_node = malloc(sizeof(bst_node_t));
  if(new_node == NULL) {
    return NULL;
  }

  new_node->left = NULL;
  new_node->right = NULL;
  new_node->parent = NULL; // We set to null right now because we do not yet know who the parent is. We will update once we insert into BST
  new_node->visited = false;

  new_node->symbol = malloc(strlen(symbol) + 1); // Add one for null character
  if(new_node->symbol == NULL) {
    free(new_node);
    return NULL;
  }

  strcpy(new_node->symbol, symbol); // Copy char string 
  new_node->data = data; // Store data

  return new_node;
}

static bst_node_t *insert_node(bst_node_t *root, bst_node_t *new_node) {

  if (root == NULL) {
    return new_node;
  }

  bst_node_t *temp = root;

  while (temp != NULL) {

    // Go right
    if (strcmp(new_node->symbol, temp->symbol) > 0) {

      if (temp->right == NULL) {

        temp->right = new_node;
        new_node->parent = temp;
        return root;

      } else {
        temp = temp->right;
      }
    } 
    // Go left 
    else if(strcmp(new_node->symbol, temp->symbol) < 0) {

      if (temp->left == NULL) {
        temp->left = new_node;
        new_node->parent = temp;
        return root;

      } else {
        temp = temp->left;
      }
    }
  }
    return root;
}

void *symtabCreateBSTIterator(void *BSTRoot) {

  bst_node_t *root = (bst_node_t *)BSTRoot;
  if (root == NULL) {
    return NULL;
  }

  bst_iterator_t *iter = malloc(sizeof(bst_iterator_t));
  if (iter == NULL) {
    return NULL;
  }

  // Traverse to the leftmost node
  bst_node_t *curr = root;
  while (curr->left != NULL) {
    curr = curr->left;
  }
  iter->current = curr;
  return (void *)iter;
}

const char *symtabBSTNext(void *BSTiteratorNode, void **returnData) {

  bst_iterator_t *iter = (bst_iterator_t *)BSTiteratorNode;
  bst_node_t *curr_node = iter->current;
  
  if (curr_node == NULL) {
    return NULL;
  }

  while (curr_node != NULL) {
    if (curr_node->left != NULL && !curr_node->left->visited) {
      curr_node = curr_node->left;
    } 
    else if (!curr_node->visited) {
      // Visit current node
      *returnData = curr_node->data;
      curr_node->visited = true;
      
      // Update iterator state before returning
      iter->current = curr_node;
      return curr_node->symbol;
    } 
    else if (curr_node->right != NULL && !curr_node->right->visited) {
      curr_node = curr_node->right;

      // Then move as far left as possible
      while (curr_node->left != NULL) {
        curr_node = curr_node->left;
      }
    } 
    else {
      // Both subtrees visited so move up to the parent
      curr_node = curr_node->parent;
    }
  }

  // End of tree
  iter->current = NULL;
  return NULL;
}

void symtabDeleteBSTIterator(void *BSTiteratorHandle) {

  bst_node_t *bst_iterator = BSTiteratorHandle;
  free(bst_iterator);
}

void symtabBSTDelete(void *BSTRoot) {
  bst_node_t *root = (bst_node_t *)BSTRoot;

    free_nodes(root);

}

static void free_nodes(bst_node_t *node) {
  if (node == NULL) {
      return;
  }

  // Free all subtrees
  free_nodes(node->left);
  free_nodes(node->right);
  
  // Free symbol
  free(node->symbol);
  // Free node
  free(node);
}

static unsigned int hash(const char *str) {

  const unsigned int p = 16777619;
  unsigned int hash = 2166136261u;

  while (*str) {
  hash = (hash ^ *str) * p;
  str += 1;
  }

  hash += hash << 13;
  hash ^= hash >> 7;
  hash += hash << 3;
  hash ^= hash >> 17;
  hash += hash << 5;
  return hash;
}