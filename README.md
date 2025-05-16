# RAVEN_VSFS

A consistency checker and repair tool for the **Very Simple File System (VSFS)**.

> 🛠️ Developed by: **Tahmid Raven**  
> 📚 Course: **CSE321 - Operating Systems**  
> 📅 Date: **May 09, 2025**

---

## 📝 Project Overview

**RAVEN_VSFS** is a file system image validator and repair utility built in C. It simulates the behavior of tools like `fsck`, tailored for the simplified **Very Simple File System (VSFS)** format. The tool validates core filesystem structures and performs automated repairs when inconsistencies are detected.

---

## 📦 File Structure

```plaintext
.
├── raven_vsfs.c       # Main program source
├── Makefile           # Build instructions and rules
├── LICENSE            # GNU GENERAL PUBLIC LICENSE
├── README.md          # This file (project documentation)
└── vsfs.img           # Default VSFS filesystem image used for testing

```
---
## ⚙️ Build Instructions

### Using Makefile (recommended)

To compile and build the project executable using the provided Makefile, run:

```bash
make run
```

To clean build files
```
make clean
```

### Manual Compilation with gcc

```
gcc -o raven_vsfs raven_vsfs.c
```

To Run the program
```
./raven_vsfs <filesystem_image.img>
```

---

## 🔍 Features

- ✅ **Superblock validation**
- ✅ **Inode bitmap consistency checking**
- ✅ **Data block bitmap consistency checking**
- ✅ **Block usage and ownership tracking**
- ✅ **Detection of duplicate and orphaned blocks**
- ✅ **Detection of bad block references**
- ✅ **Interactive repair mode**
- ✅ **Post-repair verification**

---

## 🧱 File System Layout

| Block Number | Description            |
|--------------|------------------------|
| 0            | Superblock             |
| 1            | Inode Bitmap           |
| 2            | Data Block Bitmap      |
| 3 - 7        | Inode Table (5 blocks) |
| 8 - 63       | Data Blocks            |

---

## 🧾 Inode Structure

Each inode is **256 bytes**, containing:

- Metadata: `mode`, `uid`, `gid`, `size`, `time`
- Integrity fields: `links_count`, `blocks_count`
- Pointers:
  - 12 direct block pointers
  - 1 indirect block pointer
- Padding for alignment

---

## 🚀 How It Works

The program performs a series of steps:

1. **Read the filesystem image** using `fread()`:
   - Parse superblock, bitmaps, inode table, and data blocks.
2. **Validate superblock values**:
   - Magic number, block size, block count, inode/data block pointers.
3. **Scan inodes**:
   - Identify valid inodes (`links_count > 0`, `dtime == 0`)
   - Track all data blocks used by inodes.
4. **Bitmap consistency checks**:
   - Compare inode/data bitmap with actual inode and block usage.
5. **Detect inconsistencies**:
   - Duplicate data blocks
   - Bad (out-of-range) block references
   - Orphaned/unused blocks
6. **Interactive repair**:
   - Optionally fix inconsistencies (e.g., incorrect bitmaps or metadata).
   - Summarize changes and rerun validation after fixing.

---

## 🛠️ Repair Capabilities

- 🔧 **Superblock Fix**: Ensures critical metadata is accurate.
- 🔧 **Inode Bitmap Fix**: Syncs bitmap to actual valid inodes.
- 🔧 **Data Bitmap Fix**: Syncs bitmap to reflect real block usage.
- 🔄 **Re-validation After Fixing**: Ensures file system reaches consistent state.

---

## Screenshots of the Program

Here are some screenshots of RAVEN_VSFS running:

  ![vsfs11](https://github.com/user-attachments/assets/5fe6c0f3-7b90-4503-83ac-e40bb1bd6a58)
  ![vsfs12](https://github.com/user-attachments/assets/3966c518-6603-4267-8107-83b003129c7b)
  ![vsfs13](https://github.com/user-attachments/assets/51b275bb-6bf7-4f99-bf81-3d6d87f57d97)



