#include <linux/fs.h>
#include <linux/uaccess.h>
#include "osfs.h"

/**
 * Function: osfs_read
 * Description: Reads data from a file.
 * Inputs:
 *   - filp: The file pointer representing the file to read from.
 *   - buf: The user-space buffer to copy the data into.
 *   - len: The number of bytes to read.
 *   - ppos: The file position pointer.
 * Returns:
 *   - The number of bytes read on success.
 *   - 0 if the end of the file is reached.
 *   - -EFAULT if copying data to user space fails.
 */
static ssize_t osfs_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
    struct inode *inode = file_inode(filp);
    struct osfs_inode *osfs_inode = inode->i_private;
    struct osfs_sb_info *sb_info = inode->i_sb->s_fs_info;
    void *data_block;
    ssize_t bytes_read = 0;
    uint32_t current_block_index;
    size_t offset_in_block;
    size_t bytes_to_read;

    if (*ppos >= osfs_inode->i_size)
        return 0;

    if (*ppos + len > osfs_inode->i_size)
        len = osfs_inode->i_size - *ppos;

    current_block_index = *ppos / BLOCK_SIZE;
    offset_in_block = *ppos % BLOCK_SIZE;

    while (bytes_read < len) {
        // Check if we have this block
        if (current_block_index >= osfs_inode->i_blocks)
            break;

        bytes_to_read = min(len - bytes_read, 
                          (size_t)(BLOCK_SIZE - offset_in_block));

        // Get the current data block using the block number from the array
        data_block = sb_info->data_blocks + 
                    osfs_inode->i_block[current_block_index] * BLOCK_SIZE;

        if (copy_to_user(buf + bytes_read, 
                        data_block + offset_in_block, 
                        bytes_to_read))
            return -EFAULT;

        bytes_read += bytes_to_read;
        *ppos += bytes_to_read;

        current_block_index++;
        offset_in_block = 0;
    }

    return bytes_read;
}


/**
 * Function: osfs_write
 * Description: Writes data to a file.
 * Inputs:
 *   - filp: The file pointer representing the file to write to.
 *   - buf: The user-space buffer containing the data to write.
 *   - len: The number of bytes to write.
 *   - ppos: The file position pointer.
 * Returns:
 *   - The number of bytes written on success.
 *   - -EFAULT if copying data from user space fails.
 *   - Adjusted length if the write exceeds the block size.
 */
static ssize_t osfs_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{   
    struct inode *inode = file_inode(filp);
    struct osfs_inode *osfs_inode = inode->i_private;
    struct osfs_sb_info *sb_info = inode->i_sb->s_fs_info;
    void *data_block;
    ssize_t bytes_written = 0;
    uint32_t current_block_index;
    size_t offset_in_block;
    size_t bytes_to_write;

    // Calculate which block we need and the offset within that block
    current_block_index = *ppos / BLOCK_SIZE;
    offset_in_block = *ppos % BLOCK_SIZE;

    while (bytes_written < len) {
        // Check if we've hit the maximum number of blocks per file
        if (current_block_index >= MAX_BLOCKS_PER_FILE) {
            pr_err("Maximum file size reached\n");
            break;
        }

        // If we need a new block
        if (current_block_index >= osfs_inode->i_blocks) {
            uint32_t new_block_no;
            int ret = osfs_alloc_data_block(sb_info, &new_block_no);
            if (ret < 0) {
                if (bytes_written > 0)
                    break;
                return ret;
            }
            // Store the new block number in the inode's block array
            osfs_inode->i_block[current_block_index] = new_block_no;
            osfs_inode->i_blocks++;
            pr_info("Allocated new block %u for position %u\n", 
                   new_block_no, current_block_index);
        }

        // Calculate how much we can write in this block
        bytes_to_write = min(len - bytes_written, 
                           (size_t)(BLOCK_SIZE - offset_in_block));

        // Get the current data block using the block number from the array
        data_block = sb_info->data_blocks + 
                    osfs_inode->i_block[current_block_index] * BLOCK_SIZE;

        // Write data to the block
        if (copy_from_user(data_block + offset_in_block, 
                          buf + bytes_written, bytes_to_write))
            return -EFAULT;

        // Update counters and position
        bytes_written += bytes_to_write;
        *ppos += bytes_to_write;
        
        // Update file size if necessary
        if (*ppos > osfs_inode->i_size) {
            osfs_inode->i_size = *ppos;
            inode->i_size = *ppos;
        }

        // Move to next block if necessary
        current_block_index++;
        offset_in_block = 0;
    }

    pr_info("write: wrote %zd bytes, new i_size %lld, blocks used %u\n", 
            bytes_written, osfs_inode->i_size, osfs_inode->i_blocks);
    
    return bytes_written;
}

/**
 * Struct: osfs_file_operations
 * Description: Defines the file operations for regular files in osfs.
 */
const struct file_operations osfs_file_operations = {
    .open = generic_file_open, // Use generic open or implement osfs_open if needed
    .read = osfs_read,
    .write = osfs_write,
    .llseek = default_llseek,
    // Add other operations as needed
};

/**
 * Struct: osfs_file_inode_operations
 * Description: Defines the inode operations for regular files in osfs.
 * Note: Add additional operations such as getattr as needed.
 */
const struct inode_operations osfs_file_inode_operations = {
    // Add inode operations here, e.g., .getattr = osfs_getattr,
};
