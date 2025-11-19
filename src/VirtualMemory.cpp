#include "VirtualMemory.h"
#include "PhysicalMemory.h"
#include <cmath>

#define SUCCESS 1
#define FAILURE 0

typedef struct dfs_attributes{
    word_t maxFrame;
    uint64_t maxDistance;
    word_t parent_table;
    word_t offset;
    word_t cyclicFrame;
    uint64_t page_address;
    uint64_t cyclicPage;
}dfs_attributes;


uint64_t extractBits(uint64_t number, uint64_t from, uint64_t to) {
  uint64_t mask = ((1LL << ((to-from))) - 1LL) << from;
  return (number & mask) >> from;
}

bool not_occupied(int* occupied, int frame){
  for (int i = 0; i < TABLES_DEPTH; ++i)
  {
    if (occupied[i] == frame)
    {
      return false;
    }
  }
  return true;
}

void make_occupied(int* occupied, int frame){
  for (int i = 0; i < TABLES_DEPTH; ++i)
  {
    if (occupied[i] == 0){
      occupied[i] = frame;
      break;
    }
  }
}

uint64_t min_cyclic(uint64_t page_swapped_in, uint64_t p){
  uint64_t x = NUM_PAGES - abs (page_swapped_in-p);
  uint64_t y = abs (page_swapped_in-p);
  uint64_t z = x < y ? x : y;
  return z;
}

uint64_t modifyPageAddress(dfs_attributes* attributes, int i,
                       uint64_t maxValue, uint64_t layerSize){
  uint64_t sum = 0;
  for (uint64_t j = layerSize-1; j >= 0; --j)
  {
    uint64_t x = extractBits (i, j, j+1);
    sum += x * maxValue;
    attributes->page_address += x * maxValue;
    maxValue /= (1LL << 1);
    if (j == 0){
      return sum;
    }
  }
}

void dfs (int layer, word_t *value, word_t cur_frame, uint64_t
page_swapped_in, dfs_attributes *attributes, uint64_t* maxValue, uint64_t 
*layerSize)
{
  if (layer >= TABLES_DEPTH)
  {
    return;
  }
  if (layer == TABLES_DEPTH-1){
    for (int i = 0; i < PAGE_SIZE; ++i)
    {
      PMread ((cur_frame * PAGE_SIZE) + i, value);
      if (*value != 0)
      {
       uint64_t sum = modifyPageAddress (attributes, i, maxValue[layer],
                                         layerSize[layer]);
        uint64_t x = min_cyclic (page_swapped_in, attributes->page_address);
        if (x > attributes->maxDistance)
        {
          attributes->maxDistance = x;
          attributes->cyclicFrame = *value;
          attributes->parent_table = cur_frame;
          attributes->offset = i;
          attributes->cyclicPage = attributes->page_address;
        }
        attributes->page_address -= sum;
      }
    }
  }
  for (int i = 0; i < PAGE_SIZE; ++i)
  {
    PMread ((cur_frame * PAGE_SIZE) + i, value);
//    std::cout << "cur frame: " << cur_frame << std::endl;
    if (*value == 0)
    {
      continue;
    }
    uint64_t sum = modifyPageAddress(attributes, i, maxValue[layer],
                                     layerSize[layer]);
    if (attributes->maxFrame < *value)
    {
      attributes->maxFrame = *value;
    }
    dfs (layer + 1, value, *value, page_swapped_in, attributes, maxValue, 
         layerSize);
    attributes->page_address -= sum;
  }
}

word_t findEmptyTable (int layer, word_t *value, int cur_frame, int
                          *child_changed, int *occupied)
{
  if (layer >= TABLES_DEPTH)
  {
    return 0;
  }
  if (not_occupied (occupied, cur_frame))
  {
    int zero_entries_in_table = 0;
    for (int i = 0; i < PAGE_SIZE; ++i)
    {
      PMread ((cur_frame * PAGE_SIZE) + i, value);
      if (*value != 0)
      {
        break;
      }
      zero_entries_in_table++;
    }
    if (zero_entries_in_table == PAGE_SIZE)
    {
      *child_changed = 1;
      return cur_frame;
    }
  }
  for (int i = 0; i < PAGE_SIZE; ++i)
  {
    PMread ((cur_frame * PAGE_SIZE) + i, value);
    if (*value == 0)
    {
      continue;
    }
    word_t p = findEmptyTable (layer+1, value, *value, child_changed,
                                 occupied);
    if (p==0){
      continue;
    }
    if (*child_changed){
      *child_changed = 0;
      PMwrite ((cur_frame * PAGE_SIZE) + i, 0);
    }
    return p;
  }
  return 0;
}


word_t findEmptyFrame(int *occupied, uint64_t page_swapped_in, uint64_t 
                      *maxValue, uint64_t *layerSize){
  word_t value;
  int child_changed = 0;
  word_t frame = findEmptyTable (0, &value, 0, &child_changed, occupied);
  if (frame != 0){
    make_occupied (occupied, frame);
    return frame;
  }
  dfs_attributes attributes = {0};
  dfs (0, &value, 0, page_swapped_in, &attributes, maxValue, layerSize);
  if (attributes.maxFrame+1 < NUM_FRAMES && not_occupied (occupied,
                                                          attributes
                                                          .maxFrame+1)){
    make_occupied (occupied, attributes.maxFrame+1);
    return attributes.maxFrame+1;
  }
  PMevict (attributes.cyclicFrame, attributes.cyclicPage);
  PMwrite ((attributes.parent_table * PAGE_SIZE) + attributes.offset, 0);
  make_occupied (occupied, attributes.cyclicFrame);
  return attributes.cyclicFrame;
}


void determine_layer_size (uint64_t address, uint64_t *sizes_array)
{
  uint64_t mod = address % TABLES_DEPTH;
  for (int i = 0; i < TABLES_DEPTH; ++i)
  {
    sizes_array[i] = address / TABLES_DEPTH;
    if (mod>0){
      sizes_array[i]++;
      mod--;
    }
  }
}

uint64_t determine_address (int layer, uint64_t *layerSize, uint64_t address)
{
  uint64_t sum = 0;
  for (int i = 0; i <= layer; ++i)
  {
    sum += layerSize[i];
  }
  if (layer == 0){
    return extractBits (address, VIRTUAL_ADDRESS_WIDTH-layerSize[1],
                        VIRTUAL_ADDRESS_WIDTH);
  }
  return extractBits (address, VIRTUAL_ADDRESS_WIDTH - sum,
                      (VIRTUAL_ADDRESS_WIDTH - sum) + layerSize[layer]);
}

void determineMaxValue (uint64_t *maxValue, const uint64_t *layerSize)
{
  for (int i = 0; i < TABLES_DEPTH; ++i)
  {
    uint64_t sum = 0;
    for (int j = 0; j < i; ++j)
    {
      sum += layerSize[j];
    }
    maxValue[i] = 1LL << (VIRTUAL_ADDRESS_WIDTH-(OFFSET_WIDTH + sum + 1));
  }
}

word_t findFrame(uint64_t virtualAddress){
  uint64_t address = extractBits (virtualAddress, OFFSET_WIDTH,
                                  VIRTUAL_ADDRESS_WIDTH);
  uint64_t layerSize[TABLES_DEPTH];
  determine_layer_size(VIRTUAL_ADDRESS_WIDTH-OFFSET_WIDTH, layerSize);
  uint64_t maxValue[TABLES_DEPTH];
  determineMaxValue(maxValue, layerSize);
  word_t curValue;
  word_t occupied[TABLES_DEPTH] = {0};
  word_t i = 0;
  for (int layer = 0; layer < TABLES_DEPTH; ++layer)
  {
    uint64_t curAddress = determine_address(layer, layerSize, virtualAddress);
//    std::cout << "cur address: " << curAddress << std::endl;
    PMread (i+curAddress, &curValue);
    if (curValue == 0){
      word_t frame = findEmptyFrame(occupied, address, maxValue, layerSize);
      if (layer < TABLES_DEPTH-1)
      {
        for (int j = 0; j < PAGE_SIZE; ++j)
        {
          PMwrite ((frame * PAGE_SIZE) + j, 0);
        }
      }
      else {
        PMrestore (frame, address);
      }
      PMwrite (i+curAddress, frame);
      curValue = frame;
    }
    i = curValue * PAGE_SIZE;
  }
  uint64_t offset = extractBits (virtualAddress, 0, OFFSET_WIDTH);
  return offset + i;
}

int checkValidity (uint64_t virtualAddress, const word_t *value)
{
  if (virtualAddress > VIRTUAL_MEMORY_SIZE || value == nullptr){
    return FAILURE;
  }
  return SUCCESS;
}


void VMinitialize(){
  for (int i = 0; i < PAGE_SIZE; ++i)
  {
    PMwrite (i, 0);
  }
}


int VMread(uint64_t virtualAddress, word_t* value){
  if (checkValidity (virtualAddress, value) == 0){
    return FAILURE;
  }
  word_t frame = findFrame (virtualAddress);
  PMread (frame, value);
  return SUCCESS;
}


int VMwrite(uint64_t virtualAddress, word_t value){
  if (checkValidity (virtualAddress, &value) == 0){
    return FAILURE;
  }
  word_t frame = findFrame (virtualAddress);
  PMwrite(frame, value);
  return SUCCESS;
}
//
//int main(int argc, char** argv){
//  dfs_attributes attributes = {0};
//  int layer = 0;
//  uint64_t layerSize = 3;
//  uint64_t maxValue = 1LL << 3;
//  uint64_t x = modifyPageAddress (&attributes, 5, maxValue, layerSize);
//  std::cout << x << std::endl << attributes.page_address;
//}
