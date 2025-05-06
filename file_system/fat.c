#include "fat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#define MAX_DEPTH 16

FILE *in;
int i;
PartitionTable pt[4];
Fat16BootSector bs;
Fat16Entry entry;
Fat16Entry current_entry;

char current_directory[1024] = "/root";
int current_directory_offset = 0;
int root_directory_offset = 0;

int dir_depth = 0;

char *get_attributes(unsigned char attributes)
{
  if (attributes & 0x01)
  {
    return "<RDO>";
  }
  else if (attributes & 0x02)
  {
    return "<HID>";
  }
  else if (attributes & 0x04)
  {
    return "<SYS>";
  }
  else if (attributes & 0x08)
  {
    return "<VOL>";
  }
  else if (attributes & 0x10)
  {
    return "<DIR>";
  }
  else if (attributes & 0x20)
  {
    return "<FIL>";
  }
  else
  {
    return "<UNK>";
  }
}

void format_datetime(unsigned short date, unsigned short time, char *datetime_str)
{
  int year = ((date >> 9) & 0x7F) + 1980;
  int month = (date >> 5) & 0x0F;
  int day = date & 0x1F;

  int hour = (time >> 11) & 0x1F;
  int minute = (time >> 5) & 0x3F;

  static const char *month_names[] = {
      "Jan", "Feb", "Mar", "Apr", "May", "Jun",
      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  sprintf(datetime_str, "%02d-%s-%04d %02d:%02d",
          day, month_names[month - 1], year, hour, minute);
}

int read_fat_entry(int cluster)
{
  int fat_offset = (pt[0].start_sector + bs.reserved_sectors) * bs.sector_size;
  fseek(in, fat_offset + cluster * 2, SEEK_SET);
  unsigned short next_cluster;
  fread(&next_cluster, sizeof(unsigned short), 1, in);
  return next_cluster;
}

int get_data_region_offset()
{
  return (pt[0].start_sector + bs.reserved_sectors + (bs.number_of_fats * bs.fat_size_sectors) + ((bs.root_dir_entries * 32) / bs.sector_size)) * bs.sector_size;
}

int get_root_directory_offset()
{
  return (pt[0].start_sector + bs.reserved_sectors + (bs.number_of_fats * bs.fat_size_sectors)) * bs.sector_size;
}

void read(char *filename)
{
  if (filename == NULL)
  {
    fprintf(stderr, "Error: filename is NULL\n");
    return;
  }

  // 8.3 format (uppercase, padded)
  char name[9] = {0};
  char ext[4] = {0};
  char fat_name[8] = {' '};
  char fat_ext[3] = {' '};

  char *dot = strrchr(filename, '.');
  if (!dot || dot == filename || strlen(dot + 1) > 3)
  {
    fprintf(stderr, "Error: Invalid filename format. Expected NAME.EXT\n");
    return;
  }

  strncpy(name, filename, dot - filename);
  strncpy(ext, dot + 1, 3);

  // init with spaces
  memset(fat_name, ' ', 8);
  memset(fat_ext, ' ', 3);

  for (int i = 0; i < 8 && name[i]; i++)
  {
    fat_name[i] = toupper(name[i]);
  }
  for (int i = 0; i < 3 && ext[i]; i++)
  {
    fat_ext[i] = toupper(ext[i]);
  }

  FILE *in = fopen("sd.img", "rb");
  if (!in)
  {
    perror("Error opening disk image");
    return;
  }

  if (current_directory_offset == root_directory_offset)
  {
    // calculate locations
    int data_region_offset = get_data_region_offset();
    fseek(in, root_directory_offset, SEEK_SET);

    int found = 0;
    for (int i = 0; i < bs.root_dir_entries; i++)
    {
      fread(&entry, sizeof(Fat16Entry), 1, in);

      if (entry.filename[0] == 0x00)
        break; // No more entries
      if ((entry.attributes & 0x0F) == 0x0F)
        continue; // Skip long file names

      if (memcmp(entry.filename, fat_name, 8) == 0 &&
          memcmp(entry.ext, fat_ext, 3) == 0)
      {
        found = 1;
        printf("Found file: %.8s.%.3s, size: %u bytes\n", entry.filename, entry.ext, entry.file_size);
        break;
      }
    }

    if (!found)
    {
      printf("File not found: %s\n", filename);
      fclose(in);
      return;
    }

    // read file with multiple clusters
    unsigned short cluster = entry.starting_cluster;
    unsigned int file_size = entry.file_size;
    unsigned char *buffer = malloc(file_size);
    if (!buffer)
    {
      fprintf(stderr, "Memory allocation failed\n");
      fclose(in);
      return;
    }

    int bytes_per_cluster = bs.sectors_per_cluster * bs.sector_size;
    int fat_offset = (pt[0].start_sector + bs.reserved_sectors) * bs.sector_size;

    int bytes_read = 0;
    while (cluster >= 0x0002 && cluster < 0xFFF8 && bytes_read < file_size)
    {
      int cluster_offset = data_region_offset + (cluster - 2) * bytes_per_cluster;
      fseek(in, cluster_offset, SEEK_SET);

      int to_read = (file_size - bytes_read > bytes_per_cluster) ? bytes_per_cluster : (file_size - bytes_read);
      fread(buffer + bytes_read, 1, to_read, in);
      bytes_read += to_read;

      // read next cluster from FAT
      fseek(in, fat_offset + cluster * 2, SEEK_SET);
      fread(&cluster, 2, 1, in);
    }

    fwrite(buffer, 1, file_size, stderr);
    printf("\n\nRead complete (%u bytes)\n", file_size);

    free(buffer);
    fclose(in);
  }
  else
  {

    // TODO: ŠPATNĚ NAČÍTÁ ENTRY
    printf("ELSE\n");
    printf("current_directory_offset: %d\n", current_directory_offset);
    int data_region_offset = get_data_region_offset();
    int cluster_size = bs.sectors_per_cluster * bs.sector_size;

    printf("current_entry.filename: %.8s\n", current_entry.filename);
    int current_cluster = current_entry.starting_cluster;
    printf("current_cluster: %d\n", current_cluster);
    int bytes_read = 0;
    int bytes_per_cluster = bs.sectors_per_cluster * bs.sector_size;
    int fat_offset = (pt[0].start_sector + bs.reserved_sectors) * bs.sector_size;

    unsigned int file_size = entry.file_size;
    unsigned char *buffer = malloc(file_size);
    if (!buffer)
    {
      fprintf(stderr, "Memory allocation failed\n");
      fclose(in);
      return;
    }

    while (current_cluster < 0xFFFF) //&& bytes_read < file_size)
    {
      int cluster_offset = data_region_offset + ((current_cluster - 2) * bs.sectors_per_cluster) * bs.sector_size;
      // int cluster_offset = data_region_offset + (current_cluster - 2) * bytes_per_cluster;
      fseek(in, cluster_offset, SEEK_SET);

      for (int i = 0; i < cluster_size / sizeof(Fat16Entry); i++)
      {
        fread(&entry, sizeof(entry), 1, in);
        if (entry.filename[0] == 0x00)
          break;
        if (entry.filename[0] == 0xE5)
          continue;
        if ((entry.attributes & 0x0F) == 0x0F)
          continue;

        char name_buffer[13] = {0};
        for (int j = 0; j < 8 && entry.filename[j] != ' '; j++)
        {
          name_buffer[j] = entry.filename[j];
        }
        if (entry.ext[0] != ' ')
        {
          strcat(name_buffer, ".");
          strncat(name_buffer, entry.ext, 3);
          for (int j = strlen(name_buffer) - 1; j >= 0 && name_buffer[j] == ' '; j--)
          {
            name_buffer[j] = '\0';
          }
        }

        char datetime_str[30];
        format_datetime(entry.modify_date, entry.modify_time, datetime_str);

        printf("%s %s %8u bytes %s\n",
               name_buffer, get_attributes(entry.attributes),
               entry.file_size, datetime_str);
      }
    }

    fwrite(buffer, 1, file_size, stderr);
    printf("\n\nRead complete (%u bytes)\n", file_size);

    free(buffer);
    fclose(in);
    return;
  }
}

void print_directory(int dir_offset, int data_region_offset, int level, int is_root)
{
  Fat16Entry entries[512]; // max entries we expect (adjust if needed)
  int entry_count = 0;

  char indent[256] = {0};
  for (int i = 0; i < level; i++)
    strcat(indent, "  ");

  fseek(in, dir_offset, SEEK_SET);
  int max_entries = is_root
                        ? bs.root_dir_entries
                        : (bs.sectors_per_cluster * bs.sector_size) / sizeof(Fat16Entry);

  // === First pass: load entries into memory
  for (int i = 0; i < max_entries; i++)
  {

    if (fread(&entry, sizeof(Fat16Entry), 1, in) != 1)
      break;
    if (entry.filename[0] == 0x00)
      break;
    if (entry.filename[0] == 0xE5)
      continue;
    if ((entry.attributes & 0x0F) == 0x0F)
      continue;
    entries[entry_count++] = entry;
  }

  // === Second pass: print entries
  for (int i = 0; i < entry_count; i++)
  {
    entry = entries[i];

    char name_buffer[13] = {0};
    for (int j = 0; j < 8 && entry.filename[j] != ' '; j++)
    {
      name_buffer[j] = entry.filename[j];
    }
    if (entry.ext[0] != ' ')
    {
      strcat(name_buffer, ".");
      strncat(name_buffer, entry.ext, 3);
      for (int j = strlen(name_buffer) - 1; j >= 0 && name_buffer[j] == ' '; j--)
      {
        name_buffer[j] = '\0';
      }
    }

    char datetime_str[30];
    format_datetime(entry.modify_date, entry.modify_time, datetime_str);

    printf("%s%s %s %8u bytes %s\n",
           indent, name_buffer, get_attributes(entry.attributes),
           entry.file_size, datetime_str);

    if ((entry.attributes & 0x10) &&
        !(entry.filename[0] == '.' && (entry.filename[1] == '\0' || entry.filename[1] == ' ')) &&
        !(entry.filename[0] == '.' && entry.filename[1] == '.' && (entry.filename[2] == '\0' || entry.filename[2] == ' ')))
    {

      int bytes_per_cluster = bs.sectors_per_cluster * bs.sector_size;
      int cluster_offset = data_region_offset + (entry.starting_cluster - 2) * bytes_per_cluster;

      print_directory(cluster_offset, data_region_offset, level + 1, 0);
    }
  }
}

void print_tree()
{
  FILE *in = fopen("sd.img", "rb");
  if (!in)
  {
    perror("Error opening disk image");
    return;
  }

  fseek(in, 512 * pt[0].start_sector, SEEK_SET);
  fread(&bs, sizeof(Fat16BootSector), 1, in);

  int root_dir_offset = (pt[0].start_sector + bs.reserved_sectors +
                         bs.number_of_fats * bs.fat_size_sectors) *
                        bs.sector_size;
  int data_region_offset = root_dir_offset + bs.root_dir_entries * sizeof(Fat16Entry);

  printf("Directory Tree:\n");

  print_directory(root_dir_offset, data_region_offset, 0, 1);
  printf("\n");
  fclose(in);
}

void list_current_directory(int current_directory_offset, int data_region_offset)
{
  char name_buffer[13] = {0};
  char datetime_str[30];
  int cluster_size = bs.sectors_per_cluster * bs.sector_size;

  int seeked = fseek(in, current_directory_offset, SEEK_SET);

  if (seeked != 0)
  {
    perror("Error seeking to current directory");
    return;
  }


  int current_cluster = current_entry.starting_cluster;
  int bytes_read = 0;
  int bytes_per_cluster = bs.sectors_per_cluster * bs.sector_size;
  int fat_offset = (pt[0].start_sector + bs.reserved_sectors) * bs.sector_size;

  unsigned int file_size = entry.file_size;
  unsigned char *buffer = malloc(file_size);
  if (!buffer)
  {
    fprintf(stderr, "Memory allocation failed\n");
    fclose(in);
    return;
  }

    int cluster_offset = data_region_offset + ((current_cluster - 2) * bs.sectors_per_cluster) * bs.sector_size;
    // int cluster_offset = data_region_offset + (current_cluster - 2) * bytes_per_cluster;
    fseek(in, cluster_offset, SEEK_SET);

    for (int i = 0; i < cluster_size / sizeof(Fat16Entry); i++)
    {
      fread(&entry, sizeof(entry), 1, in);
      if (entry.filename[0] == 0x00)
        break;
      if (entry.filename[0] == 0xE5)
        continue;
      if ((entry.attributes & 0x0F) == 0x0F)
        continue;

      char name_buffer[13] = {0};
      for (int j = 0; j < 8 && entry.filename[j] != ' '; j++)
      {
        name_buffer[j] = entry.filename[j];
      }
      if (entry.ext[0] != ' ')
      {
        strcat(name_buffer, ".");
        strncat(name_buffer, entry.ext, 3);
        for (int j = strlen(name_buffer) - 1; j >= 0 && name_buffer[j] == ' '; j--)
        {
          name_buffer[j] = '\0';
        }
      }

      char datetime_str[30];
      format_datetime(entry.modify_date, entry.modify_time, datetime_str);

      printf("%s %s %8u bytes %s\n",
             name_buffer, get_attributes(entry.attributes),
             entry.file_size, datetime_str);
    }
  
}

void list_files()
{

  if (current_directory_offset == root_directory_offset)
  {
    printf("Listing files in root directory:\n");
    print_directory(root_directory_offset, get_root_directory_offset(), 0, 1);
  }
  else
  {
    printf("Listing files in current directory:\n");
    list_current_directory(current_directory_offset, get_data_region_offset());
  }
}

void fill_name(const char *input, char output[8])
{
  size_t len = strlen(input);
  size_t copy_len = len < 8 ? len : 8;

  // Copy up to 8 characters
  strncpy(output, input, copy_len);

  // Pad the rest with spaces if needed
  for (size_t i = copy_len; i < 8; i++)
  {
    output[i] = ' ';
  }

  // output[8] = '\0'; // Null-terminate the output string
}

void print_name(const char *input)
{
  for (int i = 0; i < 8; i++)
  {
    printf("%s: %c\n", input, input[i]);
  }
}

int change_directory(char *name)
{
  int seeked = fseek(in, current_directory_offset, SEEK_SET);

  if (strncmp(name, "..", 2) == 0)
  {
    // Go up one directory
    if (dir_depth > 0)
    {
      dir_depth--;
      char *last_slash = strrchr(current_directory, '/');
      if (last_slash)
      {
        *last_slash = '\0';
      }
      current_directory_offset = root_directory_offset;
      return 0;
    }
  }


  if (current_directory_offset == root_directory_offset)
  {
    for (i = 0; i < bs.root_dir_entries; i++)
    {
      fread(&entry, sizeof(entry), 1, in);
      // Skip if filename was never used, see http://www.tavi.co.uk/phobos/fat.html#file_attributes
      if (entry.filename[0] != 0x00 && entry.attributes == 0x10)
      {
        //printf("Entry: %.8s | EXT: %.3s | size: %.10u bytes | attributes: %s\n", entry.filename, entry.ext, entry.file_size, get_attributes(entry.attributes));

        // printf("comparing: %s with %s\n", name, entry.filename);

        char name_filled[8] = {0};
        fill_name(name, name_filled);
        if (strncmp(name_filled, entry.filename, 8) == 0)
        {
          //printf("IF: Found directory: %s\n", entry.filename);
          // Found the directory
          dir_depth++;
          current_directory_offset = entry.starting_cluster * bs.sector_size;
          // printf("Changed directory to: %s\n", entry.filename);
          strcat(current_directory, "/");
          strcat(current_directory, name);
          current_entry = entry;
          return 0;
        }
      }
    }
    return 0;
  }

  /*
    while (cluster >= 0x0002 && cluster < 0xFFF8 && bytes_read < file_size)
  {
    int cluster_offset = data_region_offset + (cluster - 2) * bytes_per_cluster;
    fseek(in, cluster_offset, SEEK_SET);

    int to_read = (file_size - bytes_read > bytes_per_cluster) ? bytes_per_cluster : (file_size - bytes_read);
    fread(buffer + bytes_read, 1, to_read, in);
    bytes_read += to_read;

    // read next cluster from FAT
    fseek(in, fat_offset + cluster * 2, SEEK_SET);
    fread(&cluster, 2, 1, in);
  }


  */
}

void delete_file(char *filename)
{
  // This function is not implemented yet.
  printf("Delete file %s is not implemented yet.\n", filename);
}
void write_file(char *filename)
{
  // This function is not implemented yet.
  printf("Write file %s is not implemented yet.\n", filename);
}

int main()
{
  in = fopen("sd.img", "rb");

  fseek(in, 0x1BE, SEEK_SET);               // go to partition table start, partitions start at offset 0x1BE, see http://www.cse.scu.edu/~tschwarz/coen252_07Fall/Lectures/HDPartitions.html
  fread(pt, sizeof(PartitionTable), 4, in); // read all entries (4)

  printf("Partition table\n-----------------------\n");
  for (i = 0; i < 4; i++)
  { // for all partition entries print basic info
    printf("Partition %d, type %02X, ", i, pt[i].partition_type);
    printf("start sector %8d, length %8d sectors\n", pt[i].start_sector, pt[i].length_sectors);
  }

  printf("\nSeeking to first partition by %d sectors\n", pt[0].start_sector);
  fseek(in, 512 * pt[0].start_sector, SEEK_SET); // Boot sector starts here (seek in bytes)
  fread(&bs, sizeof(Fat16BootSector), 1, in);    // Read boot sector content, see http://www.tavi.co.uk/phobos/fat.html#boot_block
  printf("Volume_label %.11s, %d sectors size\n", bs.volume_label, bs.sector_size);

  // Seek to the beginning of root directory, it's position is fixed
  fseek(in, (bs.reserved_sectors - 1 + bs.fat_size_sectors * bs.number_of_fats) * bs.sector_size, SEEK_CUR);

  root_directory_offset = ftell(in); // save root directory offset
  // Read all entries of root directory
  printf("\nFilesystem root directory listing\n-----------------------\n");
  for (i = 0; i < bs.root_dir_entries; i++)
  {
    fread(&entry, sizeof(entry), 1, in);
    // Skip if filename was never used, see http://www.tavi.co.uk/phobos/fat.html#file_attributes
    if (entry.filename[0] != 0x00)
    {
      char datetime_str[30];

      format_datetime(entry.modify_date, entry.modify_time, datetime_str);
      printf("NAME: %.8s EXT:%.3s ATTR:%.5s len %6d B %s\n",
             entry.filename, entry.ext, get_attributes(entry.attributes), entry.file_size, datetime_str);
    }
  }

  dir_depth = 0;
  current_directory_offset = root_directory_offset;

  char command[256];
  while (1)
  {
    printf("\n%s> ", current_directory);
    if (!fgets(command, sizeof(command), stdin))
    {
      break;
    }

    // del. trailing newline
    command[strcspn(command, "\n")] = '\0';

    if (strncmp(command, "read ", 5) == 0)
    {
      char *filename = command + 5;
      read(filename);
    }
    else if (strcmp(command, "exit") == 0)
    {
      break;
    }

    else if (strcmp(command, "ls") == 0)
    {
      list_files();
    }
    else if (strncmp(command, "cd ", 3) == 0)
    {
      char *path = command + 3;
      change_directory(path);
    }
    else if (strcmp(command, "tree") == 0)
    {
      print_tree();
    }
    else if (strcmp(command, "delete") == 0)
    {
      delete_file(command + 7);
    }

    else if (strcmp(command, "help") == 0)
    {
      printf("Commands:\n");
      printf("  read <filename> - Read a file from the FAT16 filesystem\n");
      printf("  exit           - Exit the program\n");
      printf("  help           - Show this help message\n");
      printf("  ls             - List files in the current directory (not implemented)\n");
      printf("  tree           - Print the directory tree\n");
      printf("  delete <filename> - Delete a file from the FAT16 filesystem (not implemented)\n");
      printf("  cd <path>      - Change directory (not implemented)\n");
    }

    else
    {
      printf("Unknown command. Try: using help command\n");
    }
  }
  return 0;
}
