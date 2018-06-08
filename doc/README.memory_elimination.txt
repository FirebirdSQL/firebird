-------------------------
Wipe memory.
-------------------------

To meet the requirements for the protection of personal data in information systems,
RAM must be cleaned prior to the data release, as well as external memory when
deleting any data. Therefore, when you delete tables, indexes and records performed
wipe memory with blocks 0x00, 0xFF in accordance with said
wipePasses parameter in the file firebird.conf.

If the value is 0, then wipe is not performed.

If the value is 1, then wipe memory with blocks 0x00.

If the value is more than 1, the memory blocks sequentially overwritten with blocks 0xFF and 0x00,
but the latest wipe is always performed with blocks 0x00.

Also, when deleting temporary files on the hard disk memory is overwritten in
accordance with the specified value of the variable wipePasses. Each wipe is performed
sequentially by three values: 0x00, 0xFF, a random character.