/*
 * RAVEN VSFS: A Consistency Checker for Very Simple File System (VSFS)
 * 
 * Author: Tahmid Raven
 * Course: CSE321 - Operating Systems
 * Date: May 09, 2025
 * 
 * This program checks and repairs the consistency of a VSFS file system image.
 * It verifies the superblock, inode bitmap, data bitmap, and checks for duplicate
 * and bad block references.
 */


 #include <stdio.h>
 #include <stdlib.h>
 #include <stdint.h>
 #include <string.h>
 #include <stdbool.h>
 
 #define BLOCK_SIZE 4096
 #define TOTAL_BLOCKS 64
 #define INODE_SIZE 256
 #define INODES_PER_BLOCK (BLOCK_SIZE / INODE_SIZE)    
 #define INODE_TABLE_BLOCKS 5           
 #define INODE_COUNT (INODES_PER_BLOCK * INODE_TABLE_BLOCKS)
 #define SUPERBLOCK_MAGIC 0xd34d
 
 // layout
 #define SUPERBLOCK_BLOCK 0
 #define INODE_BITMAP_BLOCK 1
 #define DATA_BITMAP_BLOCK 2
 #define INODE_TABLE_START_BLOCK 3
 #define DATA_BLOCK_START 8
 #define DATA_BLOCK_COUNT (TOTAL_BLOCKS - DATA_BLOCK_START)  // 56 data blocks (64 - 8)
 
 // Inode structure
 typedef struct {
     uint32_t mode;
     uint32_t uid;
     uint32_t gid;
     uint32_t size;
     uint32_t atime;
     uint32_t ctime;
     uint32_t mtime;
     uint32_t dtime;
     uint32_t links_count;
     uint32_t blocks_count;
     uint32_t direct_block;
     uint32_t single_indirect;
     uint32_t double_indirect;
     uint32_t triple_indirect;
     uint8_t reserved[156];               // padding for inode 256 bytes.
 } __attribute__((packed)) Inode;    // avoid padding issues
 
 // Superblock structure
 typedef struct {
     uint16_t magic;
     uint32_t block_size;
     uint32_t total_blocks;
     uint32_t inode_bitmap_block;
     uint32_t data_bitmap_block;
     uint32_t inode_table_block;
     uint32_t first_data_block;
     uint32_t inode_size;
     uint32_t inode_count;
     uint8_t reserved[4058];  
 } __attribute__((packed)) Superblock;  
 
 
 FILE *img;
 Superblock sb;
 uint8_t inode_bitmap[BLOCK_SIZE];
 uint8_t data_bitmap[BLOCK_SIZE];
 Inode inodes[INODE_COUNT];       // array of inodes
 
 // Tracking arrays - verification
 bool inode_referenced[INODE_COUNT] = {false};
 bool data_block_referenced[DATA_BLOCK_COUNT] = {false};
 int data_block_owner[DATA_BLOCK_COUNT];  // Stores inode number that owns each block
 int errors_found = 0;
 int errors_fixed = 0;
 
 
 void read_block(int block_num, void *buffer) {
     fseek(img, block_num * BLOCK_SIZE, SEEK_SET);
     fread(buffer, BLOCK_SIZE, 1, img);
 }
 
 void write_block(int block_num, void *buffer) {
     fseek(img, block_num * BLOCK_SIZE, SEEK_SET);
     fwrite(buffer, BLOCK_SIZE, 1, img);
 }
 
 bool is_bit_set(uint8_t *bitmap, int bit) {
     int byte_index = bit / 8;
     int bit_pos = bit % 8;
     return (bitmap[byte_index] & (1 << bit_pos)) != 0;       // return true if bit is set 1
 }
 
 void set_bit(uint8_t *bitmap, int bit, bool value) {    // clear or set 
     int byte_index = bit / 8;
     int bit_pos = bit % 8;
     
     if (value) {
         bitmap[byte_index] |= (1 << bit_pos);
     } else {
         bitmap[byte_index] &= ~(1 << bit_pos);
     }
 }
 
 bool is_inode_valid(int inode_num) {
     return inodes[inode_num].links_count > 0 && inodes[inode_num].dtime == 0;  // logic: has on3 link or not deleted
 }
 
 bool is_block_valid(uint32_t block_num) {
                                                                                  // 0 - valid value for unused blocks
     return block_num == 0 || (block_num >= DATA_BLOCK_START && block_num < TOTAL_BLOCKS);
 }
 
 // Feature 1: Superblock Validator
 int check_superblock() {
     printf("\n=== Checking Superblock ===\n");
     int errors = 0;
     
     if (sb.magic != SUPERBLOCK_MAGIC) {
         printf("ERROR: Invalid magic number: 0x%X (should be 0x%X)\n", sb.magic, SUPERBLOCK_MAGIC);
         errors++;
     }
     
     if (sb.block_size != BLOCK_SIZE) {
         printf("ERROR: Invalid block size: %u (should be %u)\n", sb.block_size, BLOCK_SIZE);
         errors++;
     }
     
     if (sb.total_blocks != TOTAL_BLOCKS) {
         printf("ERROR: Invalid total blocks: %u (should be %u)\n", sb.total_blocks, TOTAL_BLOCKS);
         errors++;
     }
     
     if (sb.inode_bitmap_block != INODE_BITMAP_BLOCK) {
         printf("ERROR: Invalid inode bitmap block: %u (should be %u)\n", sb.inode_bitmap_block, INODE_BITMAP_BLOCK);
         errors++;
     }
     
     if (sb.data_bitmap_block != DATA_BITMAP_BLOCK) {
         printf("ERROR: Invalid data bitmap block: %u (should be %u)\n", sb.data_bitmap_block, DATA_BITMAP_BLOCK);
         errors++;
     }
     
     if (sb.inode_table_block != INODE_TABLE_START_BLOCK) {
         printf("ERROR: Invalid inode table block: %u (should be %u)\n", sb.inode_table_block, INODE_TABLE_START_BLOCK);
         errors++;
     }
     
     if (sb.first_data_block != DATA_BLOCK_START) {
         printf("ERROR: Invalid first data block: %u (should be %u)\n", sb.first_data_block, DATA_BLOCK_START);
         errors++;
     }
     
     if (sb.inode_size != INODE_SIZE) {
         printf("ERROR: Invalid inode size: %u (should be %u)\n", sb.inode_size, INODE_SIZE);
         errors++;
     }
     
     if (sb.inode_count != INODE_COUNT) {
         printf("ERROR: Invalid inode count: %u (should be %u)\n", sb.inode_count, INODE_COUNT);
         errors++;
     }
 
     if (errors == 0) {
         printf("Superblock is valid.\n");
     } else {
         printf("Superblock has %d errors.\n", errors);
     }
     
     errors_found += errors;
     return errors;
 }
 
 // Load inodes from disk
 void load_inodes() {
     for (int i = 0; i < INODE_TABLE_BLOCKS; i++) {
         uint8_t buffer[BLOCK_SIZE];
         read_block(INODE_TABLE_START_BLOCK + i, buffer);
         
         for (int j = 0; j < INODES_PER_BLOCK; j++) {
             memcpy(&inodes[i * INODES_PER_BLOCK + j], buffer + j * INODE_SIZE, sizeof(Inode));
         }
     }
 }
 
 // Process block pointers of an inode to track block usage
 void process_block_pointers(int inode_num) {
     Inode *inode = &inodes[inode_num];
     
     //  direct 
     if (inode->direct_block != 0) {
         if (is_block_valid(inode->direct_block)) {
             int data_idx = inode->direct_block - DATA_BLOCK_START;
             data_block_referenced[data_idx] = true;
             data_block_owner[data_idx] = inode_num;
         }
     }
     
     //  single indirect 
     if (inode->single_indirect != 0) {
         if (is_block_valid(inode->single_indirect)) {
             int data_idx = inode->single_indirect - DATA_BLOCK_START;
             data_block_referenced[data_idx] = true;
             data_block_owner[data_idx] = inode_num;
             
             
         }
     }
     
     //  double indirect block
     if (inode->double_indirect != 0) {
         if (is_block_valid(inode->double_indirect)) {
             int data_idx = inode->double_indirect - DATA_BLOCK_START;
             data_block_referenced[data_idx] = true;
             data_block_owner[data_idx] = inode_num;
             
            
         }
     }
     
     //  triple indirect block
     if (inode->triple_indirect != 0) {
         if (is_block_valid(inode->triple_indirect)) {
             int data_idx = inode->triple_indirect - DATA_BLOCK_START;
             data_block_referenced[data_idx] = true;
             data_block_owner[data_idx] = inode_num;
             
            
         }
     }
 }
 
 // Feature 2: Inode Bitmap Consistency Checker
 int check_inode_bitmap() {
     printf("\n=== Checking Inode Bitmap Consistency ===\n");
     int errors = 0;
     
     // First identify valid inodes and mark them as referenced
     for (int i = 0; i < INODE_COUNT; i++) {
         if (is_inode_valid(i)) {
             inode_referenced[i] = true;
             process_block_pointers(i);
         }
     }
     
     // bitmap consistency
     for (int i = 0; i < INODE_COUNT; i++) {
         bool is_marked_used = is_bit_set(inode_bitmap, i);
         
         // Case 1: Bitmap says used, but inode is not valid
         if (is_marked_used && !inode_referenced[i]) {
             printf("ERROR: Inode %d marked as used in bitmap but is not valid\n", i);
             errors++;
         }
         
         // Case 2: Bitmap says unused, but inode is valid
         if (!is_marked_used && inode_referenced[i]) {
             printf("ERROR: Inode %d is valid but marked as free in bitmap\n", i);
             errors++;
         }
     }
     
     if (errors == 0) {
         printf("Inode bitmap is consistent.\n");
     } else {
         printf("Inode bitmap has %d inconsistencies.\n", errors);
     }
     
     errors_found += errors;
     return errors;
 }
 
 // Feature 3: Data Bitmap Consistency Checker
 int check_data_bitmap() {
     printf("\n=== Checking Data Bitmap Consistency ===\n");
     int errors = 0;
     
     for (int i = 0; i < DATA_BLOCK_COUNT; i++) {
         bool is_marked_used = is_bit_set(data_bitmap, i);
         
         // Case 1: Bitmap says used, but block is not referenced
         if (is_marked_used && !data_block_referenced[i]) {
             printf("ERROR: Data block %d marked as used in bitmap but not referenced by any inode\n", i + DATA_BLOCK_START);
             errors++;
         }
         
         // Case 2: Bitmap says unused, but block is referenced
         if (!is_marked_used && data_block_referenced[i]) {
             printf("ERROR: Data block %d is referenced by inode %d but marked as free in bitmap\n", 
                    i + DATA_BLOCK_START, data_block_owner[i]);
             errors++;
         }
     }
     
     if (errors == 0) {
         printf("Data bitmap is consistent.\n");
     } else {
         printf("Data bitmap has %d inconsistencies.\n", errors);
     }
     
     errors_found += errors;
     return errors;
 }
 
 // Feature 4: Duplicate Checker
 int check_duplicate_blocks() {
     printf("\n=== Checking for Duplicate Block References ===\n");
     int errors = 0;
     
     // Reset arrayts
     memset(data_block_referenced, 0, sizeof(data_block_referenced));
     memset(data_block_owner, -1, sizeof(data_block_owner));
     
     // Track all block references
     for (int i = 0; i < INODE_COUNT; i++) {
         if (is_inode_valid(i)) {
             // Check direct block
             if (inodes[i].direct_block != 0 && is_block_valid(inodes[i].direct_block)) {
                 int data_idx = inodes[i].direct_block - DATA_BLOCK_START;
                 
                 if (data_block_referenced[data_idx]) {
                     printf("ERROR: Data block %d is referenced by multiple inodes (%d and %d)\n",
                            inodes[i].direct_block, data_block_owner[data_idx], i);
                     errors++;
                 } else {
                     data_block_referenced[data_idx] = true;
                     data_block_owner[data_idx] = i;
                 }
             }
             
             // Check single indirect block
             if (inodes[i].single_indirect != 0 && is_block_valid(inodes[i].single_indirect)) {
                 int data_idx = inodes[i].single_indirect - DATA_BLOCK_START;
                 
                 if (data_block_referenced[data_idx]) {
                     printf("ERROR: Data block %d is referenced by multiple inodes (%d and %d)\n",
                            inodes[i].single_indirect, data_block_owner[data_idx], i);
                     errors++;
                 } else {
                     data_block_referenced[data_idx] = true;
                     data_block_owner[data_idx] = i;
                 }
             }
             
             // Check double indirect block
             if (inodes[i].double_indirect != 0 && is_block_valid(inodes[i].double_indirect)) {
                 int data_idx = inodes[i].double_indirect - DATA_BLOCK_START;
                 
                 if (data_block_referenced[data_idx]) {
                     printf("ERROR: Data block %d is referenced by multiple inodes (%d and %d)\n",
                            inodes[i].double_indirect, data_block_owner[data_idx], i);
                     errors++;
                 } else {
                     data_block_referenced[data_idx] = true;
                     data_block_owner[data_idx] = i;
                 }
             }
             
             // Check triple indirect block
             if (inodes[i].triple_indirect != 0 && is_block_valid(inodes[i].triple_indirect)) {
                 int data_idx = inodes[i].triple_indirect - DATA_BLOCK_START;
                 
                 if (data_block_referenced[data_idx]) {
                     printf("ERROR: Data block %d is referenced by multiple inodes (%d and %d)\n",
                            inodes[i].triple_indirect, data_block_owner[data_idx], i);
                     errors++;
                 } else {
                     data_block_referenced[data_idx] = true;
                     data_block_owner[data_idx] = i;
                 }
             }
         }
     }
     
     if (errors == 0) {
         printf("No duplicate block references found.\n");
     } else {
         printf("Found %d duplicate block references.\n", errors);
     }
     
     errors_found += errors;
     return errors;
 }
 
 // Feature 5: Bad Block Checker
 int check_bad_blocks() {
     printf("\n=== Checking for Bad Block References ===\n");
     int errors = 0;
     
     for (int i = 0; i < INODE_COUNT; i++) {     // non zero = used 
         if (is_inode_valid(i)) {
             //direct block
             if (inodes[i].direct_block != 0 && !is_block_valid(inodes[i].direct_block)) {
                 printf("ERROR: Inode %d has invalid direct block pointer (%u)\n", i, inodes[i].direct_block);
                 errors++;
             }
             
             // indirect block
             if (inodes[i].single_indirect != 0 && !is_block_valid(inodes[i].single_indirect)) {
                 printf("ERROR: Inode %d has invalid single indirect block pointer (%u)\n", i, inodes[i].single_indirect);
                 errors++;
             }
             
             //  double indirect block
             if (inodes[i].double_indirect != 0 && !is_block_valid(inodes[i].double_indirect)) {
                 printf("ERROR: Inode %d has invalid double indirect block pointer (%u)\n", i, inodes[i].double_indirect);
                 errors++;
             }
             
             // triple indirect block
             if (inodes[i].triple_indirect != 0 && !is_block_valid(inodes[i].triple_indirect)) {
                 printf("ERROR: Inode %d has invalid triple indirect block pointer (%u)\n", i, inodes[i].triple_indirect);
                 errors++;
             }
         }
     }
     
     if (errors == 0) {
         printf("No bad block references found.\n");
     } else {
         printf("Found %d bad block references.\n", errors);
     }
     
     errors_found += errors;
     return errors;
 }
 
 // Fix functions
 void fix_superblock() {
     printf("\n=== Fixing Superblock ===\n");
     bool fixed = false;
     
     if (sb.magic != SUPERBLOCK_MAGIC) {
         sb.magic = SUPERBLOCK_MAGIC;
         printf("Fixed: Set magic number to 0x%X\n", SUPERBLOCK_MAGIC);
         fixed = true;
     }
     
     if (sb.block_size != BLOCK_SIZE) {
         sb.block_size = BLOCK_SIZE;
         printf("Fixed: Set block size to %u\n", BLOCK_SIZE);
         fixed = true;
     }
     
     if (sb.total_blocks != TOTAL_BLOCKS) {
         sb.total_blocks = TOTAL_BLOCKS;
         printf("Fixed: Set total blocks to %u\n", TOTAL_BLOCKS);
         fixed = true;
     }
     
     if (sb.inode_bitmap_block != INODE_BITMAP_BLOCK) {
         sb.inode_bitmap_block = INODE_BITMAP_BLOCK;
         printf("Fixed: Set inode bitmap block to %u\n", INODE_BITMAP_BLOCK);
         fixed = true;
     }
     
     if (sb.data_bitmap_block != DATA_BITMAP_BLOCK) {
         sb.data_bitmap_block = DATA_BITMAP_BLOCK;
         printf("Fixed: Set data bitmap block to %u\n", DATA_BITMAP_BLOCK);
         fixed = true;
     }
     
     if (sb.inode_table_block != INODE_TABLE_START_BLOCK) {
         sb.inode_table_block = INODE_TABLE_START_BLOCK;
         printf("Fixed: Set inode table block to %u\n", INODE_TABLE_START_BLOCK);
         fixed = true;
     }
     
     if (sb.first_data_block != DATA_BLOCK_START) {
         sb.first_data_block = DATA_BLOCK_START;
         printf("Fixed: Set first data block to %u\n", DATA_BLOCK_START);
         fixed = true;
     }
     
     if (sb.inode_size != INODE_SIZE) {
         sb.inode_size = INODE_SIZE;
         printf("Fixed: Set inode size to %u\n", INODE_SIZE);
         fixed = true;
     }
     
     if (sb.inode_count != INODE_COUNT) {
         sb.inode_count = INODE_COUNT;
         printf("Fixed: Set inode count to %u\n", INODE_COUNT);
         fixed = true;
     }
     
     if (fixed) {
         write_block(SUPERBLOCK_BLOCK, &sb);
         errors_fixed++;
         printf("Superblock fixes written to disk.\n");
     } else {
         printf("No superblock fixes needed.\n");
     }
 }
 
 void fix_inode_bitmap() {
     printf("\n=== Fixing Inode Bitmap ===\n");
     bool fixed = false;
     
     for (int i = 0; i < INODE_COUNT; i++) {
         bool should_be_used = is_inode_valid(i);
         bool is_marked_used = is_bit_set(inode_bitmap, i);
         
         if (should_be_used != is_marked_used) {
             set_bit(inode_bitmap, i, should_be_used);
             printf("Fixed: Set inode %d bitmap bit to %d\n", i, should_be_used);
             fixed = true;
         }
     }
     
     if (fixed) {
         write_block(INODE_BITMAP_BLOCK, inode_bitmap);
         errors_fixed++;
         printf("Inode bitmap fixes written to disk.\n");
     } else {
         printf("No inode bitmap fixes needed.\n");
     }
 }
 
 void fix_data_bitmap() {
     printf("\n=== Fixing Data Bitmap ===\n");
     bool fixed = false;
     
     // Reset tracking arrays and reprocess all valid inodes
     memset(data_block_referenced, 0, sizeof(data_block_referenced));
     memset(data_block_owner, -1, sizeof(data_block_owner));
     
     for (int i = 0; i < INODE_COUNT; i++) {
         if (is_inode_valid(i)) {
             process_block_pointers(i);
         }
     }
     
     for (int i = 0; i < DATA_BLOCK_COUNT; i++) {
         bool should_be_used = data_block_referenced[i];
         bool is_marked_used = is_bit_set(data_bitmap, i);
         
         if (should_be_used != is_marked_used) {
             set_bit(data_bitmap, i, should_be_used);
             printf("Fixed: Set data block %d bitmap bit to %d\n", i + DATA_BLOCK_START, should_be_used);
             fixed = true;
         }
     }
     
     if (fixed) {
         write_block(DATA_BITMAP_BLOCK, data_bitmap);
         errors_fixed++;
         printf("Data bitmap fixes written to disk.\n");
     } else {
         printf("No data bitmap fixes needed.\n");
     }
 }
 
 // TODO FUTURE IMPLEMENTATION: In a complete implementation, we would have functions to:
 // 1. Fix duplicate blocks by allocating new blocks and copying data
 // 2. Fix bad block references by clearing them or pointing to valid blocks
 
 int main(int argc, char *argv[]) {
     char *filename = "vsfs.img";
     if (argc > 1) {
         filename = argv[1];
     }
     
 
     printf("=============================================\n");
     printf("=============================================\n");
     printf("          RAVEN VSFS : Filesystem Checker Tool\n");
     printf("          Built by Tahmid Raven\n");
     printf("          GitHub: https://github.com/TahmidRaven\n");
     printf("=============================================\n");
 
     printf("   @@@@@@@    @@@@@@   @@@  @@@  @@@@@@@@  @@@  @@@                 @@@  @@@   @@@@@@   @@@@@@@@   @@@@@@   \n");
     printf("   @@@@@@@@  @@@@@@@@  @@@  @@@  @@@@@@@@  @@@@ @@@                 @@@  @@@  @@@@@@@   @@@@@@@@  @@@@@@@  \n");
     printf("   @@!  @@@  @@!  @@@  @@!  @@@  @@!       @@!@!@@@                 @@!  @@@  !@@       @@!       !@@       \n");
     printf("   !@!  @!@  !@!  @!@  !@!  @!@  !@!       !@!!@!@!                 !@!  @!@  !@!       !@!       !@!        \n");
     printf("   @!@!!@!   @!@!@!@!  @!@  !@!  @!!!:!    @!@ !!@!                 @!@  !@!  !!@@!!    @!!!:!    !!@@!!    \n");
     printf("   !!@!@!    !!!@!!!!  !@!  !!!  !!!!!:    !@!  !!!                 !@!  !!!    !!@!!!   !!!!!:     !!@!!!      \n");
     printf("   !!: :!!   !!:  !!!  :!:  !!:  !!:       !!:  !!!                 :!:  !!:       !:!  !!:            !:!    \n");
     printf("   :!:  !:!  :!:  !:!   ::!!:!   :!:       :!:  !:!                  ::!!:!       !:!   :!:           !:!     \n");
     printf("   ::   :::  ::   :::    ::::     :: ::::   ::   ::  :::::::::::::    ::::    :::: ::    ::       :::: ::        \n");
     printf("    :   : :   :   : :     :      : :: ::   ::    :   :::::::::::::     :      :: : :     :        :: : :      \n");
     printf("                                                                            \n");
     printf("                                                                            \n");
 
     printf("=============================================\n\n");
     printf("VSFS Consistency Checker (vsfsck)\n");
     printf("=================================\n");
     printf("Checking file system image: %s\n", filename);
     
     img = fopen(filename, "r+b");
     if (!img) {
         perror("Failed to open file system image");
         return 1;
     }
     
     // Initialize data_block_owner array
     for (int i = 0; i < DATA_BLOCK_COUNT; i++) {
         data_block_owner[i] = -1;
     }
     
     // Read superblock
     read_block(SUPERBLOCK_BLOCK, &sb);
     
     // Read bitmaps
     read_block(INODE_BITMAP_BLOCK, inode_bitmap);
     read_block(DATA_BITMAP_BLOCK, data_bitmap);
     
     // Load inodes
     load_inodes();
     
     // Perform checks
     check_superblock();
     check_inode_bitmap();
     check_data_bitmap();
     check_duplicate_blocks();
     check_bad_blocks();
     
     // Print summary
     printf("\n=== Summary ===\n");
     printf("Total errors found: %d\n", errors_found);
     
     // Fix errors if found
     if (errors_found > 0) {
         char choice;
         printf("\nDo you want to fix these errors? (y/n): ");
         scanf(" %c", &choice);
         
         if (choice == 'y' || choice == 'Y') {
             // Apply fixes
             fix_superblock();
             fix_inode_bitmap();
             fix_data_bitmap();
             
             printf("\n=== Repair Summary ===\n");
             printf("Errors fixed: %d\n", errors_fixed);
             
             // Re-check to confirm fixes
             printf("\nRe-checking file system...\n");
             errors_found = 0;
             check_superblock();
             check_inode_bitmap();
             check_data_bitmap();
             check_duplicate_blocks();
             check_bad_blocks();
             
             if (errors_found == 0) {
                 printf("\nFile system is now consistent.\n");
             } else {
                 printf("\nSome errors could not be fixed. Manual intervention required.\n");
             }
         } else {
             printf("No changes made to the file system.\n");
         }
     } else {
         printf("File system is consistent. No errors found.\n");
     }
     
     fclose(img);
     return 0;
 }