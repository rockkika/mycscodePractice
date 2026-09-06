//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// b_plus_tree_internal_page.cpp
//
// Identification: src/storage/page/b_plus_tree_internal_page.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <iostream>
#include <sstream>

#include "common/exception.h"
#include "storage/page/b_plus_tree_internal_page.h"

namespace bustub {
/********************************
 * HELPER METHODS AND UTILITIES *
 ********************************/

/**
 * @brief Init method after creating a new internal page.
 *
 * Writes the necessary header information to a newly created page,
 * including set page type, set current size, set page id, set parent id and set max page size,
 * must be called after the creation of a new page to make a valid BPlusTreeInternalPage.
 *
 * @param max_size Maximal size of the page
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::Init(int max_size) {
  this->SetPageType(IndexPageType::INTERNAL_PAGE);
  this->SetMaxSize(max_size);
  this->SetSize(0);
}

/**
 * @brief Helper method to get/set the key associated with input "index"(a.k.a
 * array offset).
 *
 * @param index The index of the key to get. Index must be non-zero.
 * @return Key at index
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::KeyAt(int index) const -> KeyType {
  BUSTUB_ASSERT(index > 0 && index < GetSize(), "internal page key index out of range");
  return key_array_[index];
}

/**
 * @brief Set key at the specified index.
 *
 * @param index The index of the key to set. Index must be non-zero.
 * @param key The new value for key
 */
INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::SetKeyAt(int index, const KeyType &key) {
  BUSTUB_ASSERT(index > 0 && index < GetSize(), "internal page key index out of range");
  key_array_[index] = key;
}

/**
 * @brief Helper method to get the value associated with input "index"(a.k.a array
 * offset)
 *
 * @param index The index of the value to get.
 * @return Value at index
 */
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::ValueAt(int index) const -> ValueType {
  BUSTUB_ASSERT(index >= 0 && index < GetSize(), "internal page key index out of range");
  return this->page_id_array_[index];
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::Lookup(const KeyType &key, const KeyComparator &comparator) const -> ValueType {
  BUSTUB_ASSERT(GetSize() > 0, "cannot lookup an empty internal page");

  int left = 1;
  int right = GetSize();

  while (left < right) {
    const int mid = left + (right - left) / 2;

    if (comparator(key, KeyAt(mid)) < 0) {
      right = mid;
    } else {
      left = mid + 1;
    }
  }

  return ValueAt(left - 1);
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::ValueIndex(const ValueType &value) const -> int {
  BUSTUB_ASSERT(GetSize() > 0, "cannot find value's index an empty internal page");
  for (int i = 0; i < GetSize(); i++) {
    if (page_id_array_[i] == value) {
      return i;
    }
  }

  return -1;
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::InsertAfter(const ValueType &old_value, const KeyType &new_key,
                                                 const ValueType &new_value) {
  BUSTUB_ASSERT(GetSize() < GetMaxSize(), "cannot insert into a full internal page");
  const int old_index = ValueIndex(old_value);

  BUSTUB_ASSERT(old_index >= 0, "old child does not exist in internal page");
  const int insert_index = old_index + 1;
  for (int i = GetSize(); i > insert_index; i--) {
    key_array_[i] = key_array_[i - 1];
    page_id_array_[i] = page_id_array_[i - 1];
  }
  key_array_[insert_index] = new_key;
  page_id_array_[insert_index] = new_value;
  ChangeSizeBy(1);
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::RemoveAt(int value_index) {
  BUSTUB_ASSERT(value_index >= 0 && value_index < GetSize(), "internal remove index is out of range");

  if (value_index == 0) {
    for (int i = 0; i + 1 < GetSize(); i++) {
      page_id_array_[i] = page_id_array_[i + 1];
    }
    for (int i = 1; i + 1 < GetSize(); i++) {
      key_array_[i] = key_array_[i + 1];
    }
  } else {
    for (int i = value_index; i + 1 < GetSize(); i++) {
      key_array_[i] = key_array_[i + 1];
      page_id_array_[i] = page_id_array_[i + 1];
    }
  }
  ChangeSizeBy(-1);
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveFirstToEndOf(BPlusTreeInternalPage &recipient, const KeyType &parent_key)
    -> KeyType {
  BUSTUB_ASSERT(GetSize() > GetMinSize(), "source internal page cannot spare a child");
  BUSTUB_ASSERT(recipient.GetSize() < recipient.GetMaxSize(), "recipient internal page is full");

  const KeyType new_parent_key = key_array_[1];
  const int recipient_size = recipient.GetSize();
  recipient.key_array_[recipient_size] = parent_key;
  recipient.page_id_array_[recipient_size] = page_id_array_[0];
  recipient.ChangeSizeBy(1);
  RemoveAt(0);
  return new_parent_key;
}

INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveLastToFrontOf(BPlusTreeInternalPage &recipient, const KeyType &parent_key)
    -> KeyType {
  BUSTUB_ASSERT(GetSize() > GetMinSize(), "source internal page cannot spare a child");
  BUSTUB_ASSERT(recipient.GetSize() < recipient.GetMaxSize(), "recipient internal page is full");

  const int source_index = GetSize() - 1;
  const KeyType new_parent_key = key_array_[source_index];

  for (int i = recipient.GetSize(); i > 0; i--) {
    recipient.page_id_array_[i] = recipient.page_id_array_[i - 1];
  }
  for (int i = recipient.GetSize(); i > 1; i--) {
    recipient.key_array_[i] = recipient.key_array_[i - 1];
  }
  recipient.page_id_array_[0] = page_id_array_[source_index];
  recipient.key_array_[1] = parent_key;
  recipient.ChangeSizeBy(1);
  ChangeSizeBy(-1);
  return new_parent_key;
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::MoveAllTo(BPlusTreeInternalPage &recipient, const KeyType &parent_key) {
  BUSTUB_ASSERT(recipient.GetSize() + GetSize() <= recipient.GetMaxSize(), "merged internal pages do not fit");

  const int recipient_size = recipient.GetSize();
  recipient.key_array_[recipient_size] = parent_key;
  recipient.page_id_array_[recipient_size] = page_id_array_[0];
  for (int i = 1; i < GetSize(); i++) {
    recipient.key_array_[recipient_size + i] = key_array_[i];
    recipient.page_id_array_[recipient_size + i] = page_id_array_[i];
  }
  recipient.SetSize(recipient_size + GetSize());
  SetSize(0);
}

INDEX_TEMPLATE_ARGUMENTS
void B_PLUS_TREE_INTERNAL_PAGE_TYPE::InitRoot(const ValueType &left_value, const KeyType &separator,
                                              const ValueType &right_value) {
  BUSTUB_ASSERT(GetSize() == 0, "InitRoot requires an empty internal page");

  BUSTUB_ASSERT(GetMaxSize() >= 2, "internal root must have space for two children");

  page_id_array_[0] = left_value;
  key_array_[1] = separator;
  page_id_array_[1] = right_value;

  SetSize(2);
}
INDEX_TEMPLATE_ARGUMENTS
auto B_PLUS_TREE_INTERNAL_PAGE_TYPE::SplitAndInsert(BPlusTreeInternalPage &right, const ValueType &old_value,
                                                    const KeyType &new_key, const ValueType &new_value) -> KeyType {
  BUSTUB_ASSERT(GetSize() == GetMaxSize(), "internal page must be full");
  BUSTUB_ASSERT(right.GetSize() == 0, "right internal page must be empty");

  const int old_index = ValueIndex(old_value);
  BUSTUB_ASSERT(old_index >= 0, "old child not found");

  const int insert_index = old_index + 1;
  const int total_size = GetSize() + 1;
  const int promote_index = total_size / 2;

  auto combined_key_at = [&](int index) -> KeyType {
    if (index < insert_index) {
      return key_array_[index];
    }
    if (index == insert_index) {
      return new_key;
    }
    return key_array_[index - 1];
  };

  auto combined_value_at = [&](int index) -> ValueType {
    if (index < insert_index) {
      return page_id_array_[index];
    }
    if (index == insert_index) {
      return new_value;
    }
    return page_id_array_[index - 1];
  };
  const KeyType promoted_key = combined_key_at(promote_index);

  const int right_size = total_size - promote_index;

  right.page_id_array_[0] = combined_value_at(promote_index);

  for (int i = 1; i < right_size; i++) {
    const int source = promote_index + i;
    right.key_array_[i] = combined_key_at(source);
    right.page_id_array_[i] = combined_value_at(source);
  }

  right.SetSize(right_size);

  const int left_size = promote_index;

  for (int i = left_size - 1; i >= 1; i--) {
    key_array_[i] = combined_key_at(i);
    page_id_array_[i] = combined_value_at(i);
  }

  page_id_array_[0] = combined_value_at(0);
  SetSize(left_size);

  return promoted_key;
}

// valuetype for internalNode should be page id_t
template class BPlusTreeInternalPage<GenericKey<4>, page_id_t, GenericComparator<4>>;
template class BPlusTreeInternalPage<GenericKey<8>, page_id_t, GenericComparator<8>>;
template class BPlusTreeInternalPage<GenericKey<16>, page_id_t, GenericComparator<16>>;
template class BPlusTreeInternalPage<GenericKey<32>, page_id_t, GenericComparator<32>>;
template class BPlusTreeInternalPage<GenericKey<64>, page_id_t, GenericComparator<64>>;
}  // namespace bustub
