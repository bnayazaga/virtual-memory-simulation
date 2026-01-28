#include "VirtualMemory.h"
#include "PhysicalMemory.h"


#define SUCCESS 1
#define FAILURE 0

/**
 * Struct to track state during DFS frame search
 * Used by the eviction algorithm to find optimal page to evict
 */
typedef struct dfs_attributes{
  word_t maxFrame;           // Highest frame number seen
  uint64_t maxDistance;      // Max cyclic distance found
  word_t parentTable;        // Parent of eviction candidate
  word_t offset;             // Offset in parent table
  word_t cyclicFrame;        // Frame to evict
  uint64_t pageAddress;      // Current page being examined
  uint64_t cyclicPage;       // Page number to evict
}dfs_attributes;

// ============================================================================
// INTERNAL FUNCTIONS
// ============================================================================

// extract the part of the binary number from start (inclusive) to end (exclusive)
uint64_t extractBits(const uint64_t number, const uint64_t start, const uint64_t end) {
  if (start >= end || end > 64) {
    return 0;
  }
  const uint64_t mask = ((1LL << (end - start)) - 1) << start;
  return (number & mask) >> start;
}

// Check if frame is not in the current traversal path
bool notOccupied(const int* occupied, const int frame){
  for (int i = 0; i < TABLES_DEPTH; ++i)
  {
    if (occupied[i] == frame)
    {
      return false;
    }
  }
  return true;
}

// Mark frame as occupied
void makeOccupied(int* occupied, int frame){
  for (int i = 0; i < TABLES_DEPTH; ++i)
  {
    if (occupied[i] == 0){
      occupied[i] = frame;
      break;
    }
  }
}

// Calculate minimum cyclic distance between two pages
uint64_t minCyclic(const uint64_t page_swapped_in, const uint64_t p){
  const uint64_t distance = page_swapped_in > p ? page_swapped_in - p : p - page_swapped_in;
  const uint64_t cyclic_distance = NUM_PAGES - distance;
  return cyclic_distance < distance ? cyclic_distance : distance;
}

// Update page address during DFS traversal
uint64_t modifyPageAddress(dfs_attributes* attributes, int i,
                       uint64_t maxValue, uint64_t layerSize){
  uint64_t sum = 0;
  for (uint64_t j = layerSize-1; j >= 0; --j)
  {
    uint64_t x = extractBits (i, j, j+1);
    sum += x * maxValue;
    attributes->pageAddress += x * maxValue;
    maxValue /= (1LL << 1);
    if (j == 0){
      return sum;
    }
  }
}

/**
 * DFS to find page with maximum cyclic distance for eviction.
 * Also tracks the maximum frame number encountered.
 */
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
        uint64_t x = minCyclic (page_swapped_in, attributes->pageAddress);
        if (x > attributes->maxDistance)
        {
          attributes->maxDistance = x;
          attributes->cyclicFrame = *value;
          attributes->parentTable = cur_frame;
          attributes->offset = i;
          attributes->cyclicPage = attributes->pageAddress;
        }
        attributes->pageAddress -= sum;
      }
    }
  }
  for (int i = 0; i < PAGE_SIZE; ++i)
  {
    PMread ((cur_frame * PAGE_SIZE) + i, value);
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
    attributes->pageAddress -= sum;
  }
}

/**
 * Search for completely empty page table to reuse.
 * Returns frame number if found, 0 otherwise.
 */
word_t findEmptyTable (int layer, word_t *value, int cur_frame, int
                          *child_changed, int *occupied)
{
  if (layer >= TABLES_DEPTH)
  {
    return 0;
  }
  if (notOccupied (occupied, cur_frame))
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

/**
 * Find available frame using three-tier strategy:
 * 1. Search for empty table to reuse
 * 2. Allocate new unused frame if available
 * 3. Evict page with maximum cyclic distance
 */
word_t findEmptyFrame(int *occupied, uint64_t page_swapped_in, uint64_t 
                      *maxValue, uint64_t *layerSize){
  word_t value;
  int child_changed = 0;
  word_t frame = findEmptyTable (0, &value, 0, &child_changed, occupied);
  if (frame != 0){
    makeOccupied (occupied, frame);
    return frame;
  }
  dfs_attributes attributes = {0};
  dfs (0, &value, 0, page_swapped_in, &attributes, maxValue, layerSize);
  if (attributes.maxFrame+1 < NUM_FRAMES && notOccupied (occupied,
                                                          attributes
                                                          .maxFrame+1)){
    makeOccupied (occupied, attributes.maxFrame+1);
    return attributes.maxFrame+1;
  }
  PMevict (attributes.cyclicFrame, attributes.cyclicPage);
  PMwrite ((attributes.parentTable * PAGE_SIZE) + attributes.offset, 0);
  makeOccupied (occupied, attributes.cyclicFrame);
  return attributes.cyclicFrame;
}

// Distribute address bits evenly across page table layers
void determineLayerSize (const uint64_t address, uint64_t *sizes_array)
{
  const uint64_t extra = address % TABLES_DEPTH;
  const uint64_t base = address / TABLES_DEPTH;
  for (unsigned int i = 0; i < TABLES_DEPTH; ++i)
  {
    sizes_array[i] = base;
    if (i < extra){
      sizes_array[i]++;
    }
  }
}

// Extract index for specific layer from virtual address
uint64_t determineAddress (int layer, const uint64_t *layerSize, uint64_t address)
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

// Calculate maximum contribution value for each layer
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

// translate virtual address to physical address, find the correct frame and manage page faults
uint64_t findPhysicalAddress(uint64_t virtualAddress){
  // Extract page number (remove offset)
  uint64_t address = extractBits (virtualAddress, OFFSET_WIDTH,
                                  VIRTUAL_ADDRESS_WIDTH);
  uint64_t layerSize[TABLES_DEPTH];
  determineLayerSize(VIRTUAL_ADDRESS_WIDTH-OFFSET_WIDTH, layerSize);
  uint64_t maxValue[TABLES_DEPTH];
  determineMaxValue(maxValue, layerSize);
  word_t curValue;
  word_t occupied[TABLES_DEPTH] = {0};
  uint64_t i = 0;
  for (int layer = 0; layer < TABLES_DEPTH; ++layer)
  {
    uint64_t curAddress = determineAddress(layer, layerSize, virtualAddress);
    PMread (i+curAddress, &curValue);
    if (curValue == 0){
      word_t frame = findEmptyFrame(occupied, address, maxValue, layerSize);
      if (layer < TABLES_DEPTH-1)
      {
        // Initialize new page table
        for (int j = 0; j < PAGE_SIZE; ++j)
        {
          PMwrite ((frame * PAGE_SIZE) + j, 0);
        }
      }
      else {
        // Restore page from disk
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

// check validity of virtual address and pointer
int checkValidity (uint64_t virtualAddress, const word_t *value)
{
  if (virtualAddress >= VIRTUAL_MEMORY_SIZE || value == nullptr){
    return FAILURE;
  }
  return SUCCESS;
}

// ============================================================================
// PUBLIC API
// ============================================================================

/** Initialize the virtual memory by clearing root page table.
 * Must be called before any VMread or VMwrite operations.
 */
void VMinitialize(){
  for (int i = 0; i < PAGE_SIZE; ++i)
  {
    PMwrite (i, 0);
  }
}

/** reads a word from the given virtual address
 * and puts its content in value.
 * @return 1 on success and 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMread(uint64_t virtualAddress, word_t* value){
  if (checkValidity (virtualAddress, value) == 0){
    return FAILURE;
  }
  const uint64_t physicalAddress = findPhysicalAddress (virtualAddress);
  PMread (physicalAddress, value);
  return SUCCESS;
}

/** writes a word to the given virtual address
 * @return 1 on success and 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMwrite(uint64_t virtualAddress, word_t value){
  if (checkValidity (virtualAddress, &value) == 0){
    return FAILURE;
  }
  const uint64_t physicalAddress = findPhysicalAddress (virtualAddress);
  PMwrite(physicalAddress, value);
  return SUCCESS;
}

