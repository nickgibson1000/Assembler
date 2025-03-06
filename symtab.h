//
// This is the interface for a generic symbol table. A table stores
// (symbol, data) pairs.
//
// A symbol is simply a C string (null-terminated char sequence).
//
// The data associated with a symbol is simply a void*.
//

void *symtabCreate(int sizeHint);
  // Creates a symbol table.
  // If successful, returns a handle for the new table.
  // If memory cannot be allocated for the table, returns NULL.
  // The parameter is a hint as to the expected number of (symbol, data)
  //   pairs to be stored in the table.

void symtabDelete(void *symtabHandle);
  // Deletes a symbol table.
  // Reclaims all memory used by the table.
  // Note that the memory associate with data items is not reclaimed since
  //   the symbol table does not know the actual type of the data. It only
  //   manipulates pointers to the data.
  // Also note that no validation is made of the symbol table handle passed
  //   in. If not a valid handle, then the behavior is undefined (but
  //   probably bad).

int symtabInstall(void *symtabHandle, const char *symbol, void *data);
  // Install a (symbol, data) pair in the table.
  // If the symbol is already installed in the table, then the data is
  //   overwritten.
  // If the symbol is not already installed, then space is allocated and
  //   a copy is made of the symbol, and the (symbol, data) pair is then
  //   installed in the table.
  // If successful, returns 1.
  // If memory cannot be allocated for a new symbol, then returns 0.
  // Note that no validation is made of the symbol table handle passed
  //   in. If not a valid handle, then the behavior is undefined (but
  //   probably bad).

void *symtabLookup(void *symtabHandle, const char *symbol);
  // Return the data item stored with the given symbol.
  // If the symbol is found, return the associated data item.
  // If the symbol is not found, returns NULL.
  // Note that no validation is made of the symbol table handle passed
  //   in. If not a valid handle, then the behavior is undefined (but
  //   probably bad).

void *symtabCreateIterator(void *symtabHandle);
  // Create an iterator for the contents of the symbol table.
  // If successful, a handle to the iterator is returned which can be
  // repeatedly passed to symtabNext to iterate over the contents
  // of the table.
  // If memory cannot be allocated for the iterator, returns NULL.
  // Note that no validation is made of the symbol table handle passed
  //   in. If not a valid handle, then the behavior is undefined (but
  //   probably bad).

const char *symtabNext(void *iteratorHandle, void **returnData);
  // Returns the next (symbol, data) pair for the iterator.
  // The symbol is returned as the return value and the data item
  // is placed in the location indicated by the second parameter.
  // If the whole table has already been traversed then NULL is
  //   returned and the location indicated by the second paramter
  //   is not modified.
  // Note that no validation is made of the iterator table handle passed
  //   in. If not a valid handle, then the behavior is undefined (but
  //   probably bad).
  // Also note that if there has been a symbtabInstall call since the
  //   iterator was created, the behavior is undefined (but probably
  //   benign).

void symtabDeleteIterator(void *iteratorHandle);
  // Delete the iterator indicated by the only parameter.
  // Reclaims all memory used by the iterator.
  // Note that no validation is made of the iterator table handle passed
  //   in. If not a valid handle, then the behavior is undefined (but
  //   probably bad).


void *symtabCreateBST(void *iteratorHandle);
  // Creates a BST from a hash table filled with symbols using the handle 
  // to the iterator passed to it. 
  // Returns a pointer to the root of the BST created
  
void *symtabCreateBSTIterator(void *BSTRoot);
  // Create an iterator for the BST symbol table.
  // If successful, a handle to the leftmost leaf noder is returned which can be
  // repeatedly passed to symtabBSTNext to iterate over the contents
  // of the BST one node at a time.
  // If memory cannot be allocated for the iterator, returns NULL.
  // Note that no validation is made of the root passed
  //   in. If not a valid root, then the behavior is undefined (but
  //   probably bad).

const char *symtabBSTNext(void *BSTiteratorNode, void **returnData);
  // Returns the next (symbol, data) pair for the iterator
  // The symbol is returned as the return value and the data item
  // is placed in the location indicated by the second parameter.
  // If the whole BST has already been traversed, then NULL is returned,
  // and the location indicated by the second parameter is not modified.

void symtabDeleteBSTIterator(void *BSTiteratorHandle);
  // Delete the BST iterator indicated by the only parameter.
  // Reclaims all memory used by the BST iterator.
  // Note that no validation is made of the iterator table handle passed
  //   in. If not a valid handle, then the behavior is undefined (but
  //   probably bad).

void symtabBSTDelete(void *BSTRoot);
  // Deletes a BST.
  // Reclaims all memory used by the table.
  // Note that the memory associate with data items is not reclaimed since
  //   the symbol table does not know the actual type of the data. It only
  //   manipulates pointers to the data.
  // Also note that no validation is made of the root passed
  //   in. If not a valid BST root, then the behavior is undefined (but
  //   probably bad).