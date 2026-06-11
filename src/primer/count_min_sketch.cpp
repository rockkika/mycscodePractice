//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// count_min_sketch.cpp
//
// Identification: src/primer/count_min_sketch.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "primer/count_min_sketch.h"

#include <atomic>
#include <stdexcept>
#include <string>

namespace bustub {

/**
 * Constructor for the count-min sketch.
 *
 * @param width The width of the sketch matrix.
 * @param depth The depth of the sketch matrix.
 * @throws std::invalid_argument if width or depth are zero.
 */
template <typename KeyType>
CountMinSketch<KeyType>::CountMinSketch(uint32_t width, uint32_t depth) : width_(width), depth_(depth) {
  /** @spring2026 PLEASE DO NOT MODIFY THE FOLLOWING */
  // Initialize seeded hash functions
  if (width < 1 || depth < 1) {
    throw std::invalid_argument("width and/or depth must be greater than zero");
  }
  hash_functions_.reserve(depth_);
  for (size_t i = 0; i < depth_; i++) {
    hash_functions_.push_back(this->HashFunction(i));
  }
  const size_t matrix_size = static_cast<size_t>(width_) * depth_;

  hash_matrix_ = std::make_unique<std::atomic<uint32_t>[]>(matrix_size);

  for (size_t i = 0; i < matrix_size; i++) {
    hash_matrix_[i].store(0, std::memory_order_relaxed);
  }
}

template <typename KeyType>
CountMinSketch<KeyType>::CountMinSketch(CountMinSketch &&other) noexcept : width_(other.width_), depth_(other.depth_) {
  this->hash_matrix_ = std::move(other.hash_matrix_);
  hash_functions_.reserve(depth_);
  for (size_t i = 0; i < depth_; i++) {
    hash_functions_.push_back(this->HashFunction(i));
  }
  other.width_ = 0;
  other.depth_ = 0;
  other.hash_functions_.clear();
}

template <typename KeyType>
auto CountMinSketch<KeyType>::operator=(CountMinSketch &&other) noexcept -> CountMinSketch & {
  if (this != &other) {
    depth_ = other.depth_;
    width_ = other.width_;
    this->hash_matrix_ = std::move(other.hash_matrix_);
    hash_functions_.clear();
    for (size_t i = 0; i < depth_; i++) {
      hash_functions_.push_back(this->HashFunction(i));
    }
    other.width_ = 0;
    other.depth_ = 0;
    other.hash_functions_.clear();
  }
  return *this;
}

template <typename KeyType>
void CountMinSketch<KeyType>::Insert(const KeyType &item) {
  for (size_t i = 0; i < depth_; i++) {
    const size_t idx = this->hash_functions_[i](item);
    const size_t pos = i * width_ + idx;
    hash_matrix_[pos].fetch_add(1, std::memory_order_relaxed);
  }
}

template <typename KeyType>
void CountMinSketch<KeyType>::Merge(const CountMinSketch<KeyType> &other) {
  if (width_ != other.width_ || depth_ != other.depth_) {
    throw std::invalid_argument("Incompatible CountMinSketch dimensions for merge.");
  }
  const size_t matrix_size = static_cast<size_t>(width_) * depth_;

  for (size_t i = 0; i < matrix_size; i++) {
    const uint32_t value = other.hash_matrix_[i].load(std::memory_order_relaxed);

    hash_matrix_[i].fetch_add(value, std::memory_order_relaxed);
  }
}

template <typename KeyType>
auto CountMinSketch<KeyType>::Count(const KeyType &item) const -> uint32_t {
  uint32_t result = std::numeric_limits<uint32_t>::max();
  for (size_t i = 0; i < depth_; i++) {
    const size_t idx = this->hash_functions_[i](item);
    const size_t pos = i * width_ + idx;
    uint32_t value = hash_matrix_[pos].load(std::memory_order_relaxed);
    result = std::min(result, value);
  }
  return result;
}

template <typename KeyType>
void CountMinSketch<KeyType>::Clear() {
  const size_t matrix_size = static_cast<size_t>(width_) * depth_;

  for (size_t i = 0; i < matrix_size; i++) {
    hash_matrix_[i].store(0, std::memory_order_relaxed);
  }
}

template <typename KeyType>
auto CountMinSketch<KeyType>::TopK(uint16_t k, const std::vector<KeyType> &candidates)
    -> std::vector<std::pair<KeyType, uint32_t>> {
  std::vector<std::pair<KeyType, uint32_t>> result;
  result.reserve(k);
  for (auto const &candidate : candidates) {
    result.emplace_back(candidate, this->Count(candidate));
  }
  std::sort(
      result.begin(), result.end(),
      [](const std::pair<KeyType, uint32_t> &a, const std::pair<KeyType, uint32_t> &b) { return a.second > b.second; });
  if (result.size() > k) {
    result.resize(k);
  }
  return result;
}

// Explicit instantiations for all types used in tests
template class CountMinSketch<std::string>;
template class CountMinSketch<int64_t>;  // For int64_t tests
template class CountMinSketch<int>;      // This covers both int and int32_t
}  // namespace bustub
