#pragma once
#include "allocation.h"
#include <string>

// Sample data + save/load. Return false on any I/O or format error (state is left unchanged on
// format errors detected before changes begin).
int  loadSampleData(AllocationManager& mgr, int count);                 // returns number registered
bool saveToFile(const HashTable& table, const SeatGrid& grid, const std::string& path);
bool loadFromFile(HashTable& table, SeatGrid& grid, const std::string& path);
