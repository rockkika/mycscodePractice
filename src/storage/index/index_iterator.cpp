//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// index_iterator.cpp
//
// Identification: src/storage/index/index_iterator.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

/**
 * index_iterator.cpp
 */
#include "storage/index/index_iterator.h"

namespace bustub {

/**
 * @note you can change the destructor/constructor method here
 * set your own input parameters
 */
FULL_INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::IndexIterator() = default;

FULL_INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::IndexIterator(std::shared_ptr<TracedBufferPoolManager> bpm, ReadPageGuard guard, int index)
    : bpm_(std::move(bpm)), guard_(std::move(guard)), index_(index), is_end_(false) {
  AdvanceToValid();
}

FULL_INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::~IndexIterator() = default;  // NOLINT

FULL_INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::IsEnd() -> bool { return is_end_; }

FULL_INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator*() -> std::pair<const KeyType &, const ValueType &> {
  BUSTUB_ASSERT(!is_end_, "cannot dereference the end iterator");
  const auto *leaf = guard_.template As<LeafPage>();
  BUSTUB_ASSERT(index_ >= 0 && index_ < leaf->GetSize(), "iterator index is out of range");
  BUSTUB_ASSERT(!leaf->IsTombstone(index_), "iterator points to a tombstone");
  return leaf->GetItem(index_);
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator++() -> INDEXITERATOR_TYPE & {
  BUSTUB_ASSERT(!is_end_, "cannot increment the end iterator");
  index_++;
  AdvanceToValid();
  return *this;
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator==(const IndexIterator &itr) const -> bool {
  if (is_end_ || itr.is_end_) {
    return is_end_ == itr.is_end_;
  }
  return bpm_.get() == itr.bpm_.get() && guard_.GetPageId() == itr.guard_.GetPageId() && index_ == itr.index_;
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator!=(const IndexIterator &itr) const -> bool { return !(*this == itr); }

FULL_INDEX_TEMPLATE_ARGUMENTS
void INDEXITERATOR_TYPE::AdvanceToValid() {
  while (!is_end_) {
    const auto *leaf = guard_.template As<LeafPage>();
    while (index_ < leaf->GetSize()) {
      if (!leaf->IsTombstone(index_)) {
        return;
      }
      index_++;
    }

    const page_id_t next_page_id = leaf->GetNextPageId();
    guard_.Drop();
    if (next_page_id == INVALID_PAGE_ID) {
      is_end_ = true;
      bpm_.reset();
      index_ = 0;
      return;
    }

    guard_ = bpm_->ReadPage(next_page_id);
    index_ = 0;
  }
}

template class IndexIterator<GenericKey<4>, RID, GenericComparator<4>>;

template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>>;
template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>, 3>;
template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>, 2>;
template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>, 1>;
template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>, -1>;

template class IndexIterator<GenericKey<16>, RID, GenericComparator<16>>;

template class IndexIterator<GenericKey<32>, RID, GenericComparator<32>>;

template class IndexIterator<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub
