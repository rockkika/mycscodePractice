//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// b_plus_tree_leaf_page.cpp
//
// Identification: src/storage/page/b_plus_tree_leaf_page.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <sstream>

#include "common/exception.h"
#include "common/rid.h"
#include "storage/page/b_plus_tree_leaf_page.h"

namespace bustub {


/*****************************************************************************
 * HELPER METHODS AND UTILITIES
 *****************************************************************************/

/**
 * @brief Init method after creating a new leaf page
 *
 * After creating a new leaf page from buffer pool, must call initialize method to set default values,
 * including set page type, set current size to zero, set page id/parent id, set
 * next page id and set max size.
 *
 * @param max_size Max size of the leaf node
 */
FULL_INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::Init(int max_size) {
  this->SetPageType(IndexPageType::LEAF_PAGE);
  this->SetMaxSize(max_size);
  this->SetSize(0);
  next_page_id_ = INVALID_PAGE_ID;
  num_tombstones_ = 0;
}

/**
 * @brief Helper function for fetching tombstones of a page.
 * @return The last `NumTombs` keys with pending deletes in this page in order of recency (oldest at front).
 */
FULL_INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::GetTombstones() const -> std::vector<KeyType> {
  std::vector<KeyType> result;
  result.reserve(num_tombstones_);

  for (size_t i = 0; i < num_tombstones_; i++) {
    const size_t key_index = tombstones_[i];
    BUSTUB_ASSERT(key_index < static_cast<size_t>(GetSize()),
                  "tombstone index is out of range");
    result.push_back(key_array_[key_index]);
  }

  return result;

}

/**
 * Helper methods to set/get next page id
 */
FULL_INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::GetNextPageId() const -> page_id_t {
  return next_page_id_;
}

FULL_INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_LEAF_PAGE_TYPE::SetNextPageId(page_id_t next_page_id) {
  next_page_id_ = next_page_id;
}

/*
 * Helper method to find and return the key associated with input "index" (a.k.a
 * array offset)
 */
FULL_INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::KeyAt(int index) const -> KeyType {
  BUSTUB_ASSERT(index >= 0 && index < GetSize(),
                "leaf page key index is out of range");
  return key_array_[index];
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::ValueAt(int index) const -> ValueType {
  BUSTUB_ASSERT(index >= 0 && index < GetSize(),
                "leaf value index out of range");
  return rid_array_[index];
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::IsTombstone(int index) const -> bool {
  BUSTUB_ASSERT(index >= 0 && index < GetSize(),
                "leaf tombstone index out of range");

  for (size_t i = 0; i < num_tombstones_; i++) {
    if (tombstones_[i] == static_cast<size_t>(index)) {
      return true;
    }
  }
  return false;
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::KeyIndex(
    const KeyType &key,
    const KeyComparator &comparator) const -> int {
  int left = 0;
  int right = GetSize();

  while (left < right) {
    if (const int mid = left + (right - left) / 2; comparator(KeyAt(mid), key) < 0) {
      left = mid + 1;
    } else {
      right = mid;
    }
  }

  return left;
}

template<typename KeyType, typename ValueType, typename KeyComparator, ssize_t NumTombs>
auto BPlusTreeLeafPage<KeyType, ValueType, KeyComparator, NumTombs>::TryInsert(const KeyType &key, const ValueType &value, KeyComparator &comparator) -> LeafInsertStatus {
  auto index = KeyIndex(key, comparator);
  if (index < GetSize()&& comparator(key_array_[index], key) == 0){
    if (IsTombstone(index)) {
      key_array_[index] = key;
      rid_array_[index] = value;
      auto tombPos = 0;
      for (size_t i = 0; i < num_tombstones_; i++) {
        if (tombstones_[i] == static_cast<size_t>(index)) {
          tombPos = i;
        }
      }
      for (size_t i = tombPos; i < num_tombstones_-1; i++) {
        tombstones_[i] = tombstones_[i+1];
      }
      num_tombstones_--;
      return LeafInsertStatus::INSERTED;
    }
    return LeafInsertStatus::DUPLICATE;
  }

  if (GetSize() == GetMaxSize()) {
    return LeafInsertStatus::NEED_SPLIT;
  }
  for (size_t i = GetSize(); i > index; i--) {
    key_array_[i] = key_array_[i - 1];
    rid_array_[i] = rid_array_[i - 1];
  }
  key_array_[index] = key;
  rid_array_[index] = value;
  ChangeSizeBy(1);
  for (size_t i = 0; i < num_tombstones_; i++) {
    if (tombstones_[i] > index) {
      ++tombstones_[i];
    }
  }
  return LeafInsertStatus::INSERTED;

}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_LEAF_PAGE_TYPE::SplitAndInsert(
  BPlusTreeLeafPage &right,
  page_id_t right_page_id,
  const KeyType &key,
  const ValueType &value,
  const KeyComparator &comparator) -> KeyType {

  auto index = KeyIndex(key, comparator);
  if (  )

}


template class BPlusTreeLeafPage<GenericKey<4>, RID, GenericComparator<4>>;

template class BPlusTreeLeafPage<GenericKey<8>, RID, GenericComparator<8>>;
template class BPlusTreeLeafPage<GenericKey<8>, RID, GenericComparator<8>, 3>;
template class BPlusTreeLeafPage<GenericKey<8>, RID, GenericComparator<8>, 2>;
template class BPlusTreeLeafPage<GenericKey<8>, RID, GenericComparator<8>, 1>;
template class BPlusTreeLeafPage<GenericKey<8>, RID, GenericComparator<8>, -1>;

template class BPlusTreeLeafPage<GenericKey<16>, RID, GenericComparator<16>>;

template class BPlusTreeLeafPage<GenericKey<32>, RID, GenericComparator<32>>;

template class BPlusTreeLeafPage<GenericKey<64>, RID, GenericComparator<64>>;
}  // namespace bustub
