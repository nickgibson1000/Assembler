top:
  load r0, r1 # bad format for opcode
  lood r0, x  # bad opcode
  ldimm r0, 0x1FFFFF # bad constant
  ldind r0, 0x1FFFFF(r0) # bad offset

top: #duplicate label
  word r0 # bad format
  halt 100 # another bad format