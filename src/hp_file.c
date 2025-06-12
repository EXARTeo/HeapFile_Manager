#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "hp_file.h"
#include "record.h"

#define CALL_BF(call)         \
  {                           \
    BF_ErrorCode code = call; \
    if (code != BF_OK)        \
    {                         \
      BF_PrintError(code);    \
      return HP_ERROR;        \
    }                         \
  }

int HP_CreateFile(char *fileName)
{
#define HP_ERROR -1

  int fd1;
  BF_Block *block;
  void *data;

  // create heap file and open it
  CALL_BF(BF_CreateFile(fileName));
  CALL_BF(BF_OpenFile(fileName, &fd1));

  // initialize block
  BF_Block_Init(&block);

  // allocate one block for file
  CALL_BF(BF_AllocateBlock(fd1, block));

  // get block data
  data = BF_Block_GetData(block);

  // add metadata to block 0
  HP_info *hp_info;
  hp_info = data;
  hp_info->last_block = 0;
  hp_info->last_block_records = 0;
  hp_info->records_per_block = BF_BLOCK_SIZE / sizeof(Record);

  // we set dirty and unpin block 0, because we close the file and we want the data to be saved
  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));

  // destroy the block we initialized
  BF_Block_Destroy(&block);

  // close file
  CALL_BF(BF_CloseFile(fd1));

#undef HP_ERROR

  return 0;
}

HP_info *HP_OpenFile(char *fileName, int *file_desc)
{
#define HP_ERROR NULL

  HP_info *hpInfo;
  BF_Block *block;

  // open file
  CALL_BF(BF_OpenFile(fileName, file_desc));

  // initialize block
  BF_Block_Init(&block);

  // get block data from block 0
  CALL_BF(BF_GetBlock(*file_desc, 0, block));

  // point HP_info to that data
  void *data = BF_Block_GetData(block);
  hpInfo = data;

  // destroy the block we initialized
  BF_Block_Destroy(&block);

#undef HP_ERROR

  return hpInfo;
}

int HP_CloseFile(int file_desc, HP_info *hp_info)
{
#define HP_ERROR -1

  BF_Block *block;

  // initialize block
  BF_Block_Init(&block);

  // get last block
  CALL_BF(BF_GetBlock(file_desc, hp_info->last_block, block));

  // set the last block dirty and unpin it, because we need to copy it to disk
  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));

  // get block 0
  CALL_BF(BF_GetBlock(file_desc, 0, block));

  // again, set it dirty, unpin it, because we copy it to disk
  BF_Block_SetDirty(block);
  CALL_BF(BF_UnpinBlock(block));

  // destroy the block we initialized
  BF_Block_Destroy(&block);

  // close file
  CALL_BF(BF_CloseFile(file_desc));

#undef HP_ERROR
  return 0;
}

int HP_InsertEntry(int file_desc, HP_info *hp_info, Record record)
{
#define HP_ERROR -1

  BF_Block *block;
  BF_Block_Init(&block);

  // if last block is 0, or if last block is full
  if (hp_info->last_block == 0 || hp_info->last_block_records == hp_info->records_per_block)
  {
    // if last block is full and not 0, set it dirty and unpin it
    // (all blocks are set dirty and unpinned when we want to insert an entry on a new block)
    if (hp_info->last_block != 0)
    {
      CALL_BF(BF_GetBlock(file_desc, hp_info->last_block, block));
      BF_Block_SetDirty(block);
      CALL_BF(BF_UnpinBlock(block));
    }

    // allocate the new block and update HP_info
    CALL_BF(BF_AllocateBlock(file_desc, block));
    hp_info->last_block++;
    hp_info->last_block_records = 0;
  }

  // get last block data and insert entry into it
  CALL_BF(BF_GetBlock(file_desc, hp_info->last_block, block));
  void *data = BF_Block_GetData(block);
  Record *rec = data;
  rec[hp_info->last_block_records] = record;

  // update HP_info
  hp_info->last_block_records++;

  // destroy the block we just initialized
  BF_Block_Destroy(&block);

#undef HP_ERROR

  return hp_info->last_block;
}

int HP_GetAllEntries(int file_desc, HP_info *hp_info, int value)
{
#define HP_ERROR -1

  BF_Block *block;

  // initialize block
  BF_Block_Init(&block);

  for (int i = 1; i < hp_info->last_block; i++)
  {
    // get i-th block
    CALL_BF(BF_GetBlock(file_desc, i, block));

    // make data point to block data
    void *data = BF_Block_GetData(block);

    // for each record of block i, check if its id is equal with value
    // if true, print it
    Record *rec = data;
    for (int j = 0; j < hp_info->records_per_block; j++)
    {
      if (rec[j].id == value)
        printf("\n %d %s %s %s\n", rec[j].id, rec[j].name, rec[j].surname, rec[j].city);
    }

    // unpin it when done
    CALL_BF(BF_UnpinBlock(block));
  }

  // destroy the block we just initialized
  BF_Block_Destroy(&block);

#undef HP_ERROR

  return hp_info->last_block;
}