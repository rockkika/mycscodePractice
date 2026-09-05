//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// b_plus_tree.cpp
//
// Identification: src/storage/index/b_plus_tree.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "storage/index/b_plus_tree.h"
#include "buffer/traced_buffer_pool_manager.h"
#include "storage/index/b_plus_tree_debug.h"

namespace bustub {

FULL_INDEX_TEMPLATE_ARGUMENTS
BPLUSTREE_TYPE::BPlusTree(std::string name, page_id_t header_page_id, BufferPoolManager *buffer_pool_manager,
                          const KeyComparator &comparator, int leaf_max_size, int internal_max_size)
    : bpm_(std::make_shared<TracedBufferPoolManager>(buffer_pool_manager)),
      index_name_(std::move(name)),
      comparator_(std::move(comparator)),
      leaf_max_size_(leaf_max_size),
      internal_max_size_(internal_max_size),
      header_page_id_(header_page_id) {
  WritePageGuard guard = bpm_->WritePage(header_page_id_);
  const auto root_page = guard.AsMut<BPlusTreeHeaderPage>();
  root_page->root_page_id_ = INVALID_PAGE_ID;
}

/**
 * @brief Helper function to decide whether current b+tree is empty
 * @return Returns true if this B+ tree has no keys and values.
 */
FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::IsEmpty() const -> bool {
  auto header_guard = bpm_->ReadPage(header_page_id_);
  const auto *header =
      header_guard.As<BPlusTreeHeaderPage>();

  return header->root_page_id_ == INVALID_PAGE_ID;
}

/*****************************************************************************
 * SEARCH
 *****************************************************************************/
/**
 * @brief Return the only value that associated with input key
 *
 * This method is used for point query
 *
 * @param key input key
 * @param[out] result vector that stores the only value that associated with input key, if the value exists
 * @return : true means key exists
 */
FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetValue(const KeyType &key, std::vector<ValueType> *result) -> bool {
  // Declaration of context instance. Using the Context is not necessary but advised.
  Context ctx;
  auto header_guard = bpm_->ReadPage(header_page_id_);
  const auto *header =
      header_guard.As<BPlusTreeHeaderPage>();

  if (header->root_page_id_ == INVALID_PAGE_ID) {
    return false;
  }
  ctx.read_set_.push_back(bpm_->ReadPage(header->root_page_id_));
  header_guard.Drop();
  while (true) {
    const auto *page = ctx.read_set_.back().As<BPlusTreePage>();
    if (page->IsLeafPage()) {
      break;
    }
    const auto *internal = ctx.read_set_.back().As<InternalPage>();
    const page_id_t child_id = internal->Lookup(key,comparator_)
    BUSTUB_ASSERT(child_id != INVALID_PAGE_ID, "child_id is not valid");
    auto child_guard = bpm_->ReadPage(child_id);
    ctx.read_set_.pop_back();
    ctx.read_set_.push_back(std::move(child_guard));
  }
  const auto *leaf = ctx.read_set_.back().As<LeafPage>();
  int left = leaf->KeyIndex(key,comparator_);
  if (left == leaf->GetSize() ||
      comparator_(leaf->KeyAt(left), key) != 0 ||
      leaf->IsTombstone(left)) {
    return false;
      }

  result->push_back(leaf->ValueAt(left));
  return true;
}



/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/**
 * @brief Insert constant key & value pair into b+ tree
 *
 * if current tree is empty, start new tree, update root page id and insert
 * entry; otherwise, insert into leaf page.
 *
 * @param key the key to insert
 * @param value the value associated with key
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false; otherwise, return true.
 */
FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Insert(const KeyType &key, const ValueType &value) -> bool {
  Context ctx;
  auto header_guard = bpm_->WritePage(header_page_id_);
  auto *header = header_guard.AsMut<BPlusTreeHeaderPage>();
  if (header->root_page_id_ == INVALID_PAGE_ID) {
    header->root_page_id_ = bpm_->NewPage();
    auto leaf_guard = bpm_->WritePage(header->root_page_id_);
    auto *leaf = leaf_guard.AsMut<LeafPage>();
    leaf->Init(leaf_max_size_);
    if (leaf->TryInsert(key,value,comparator_) == LeafInsertStatus::INSERTED) {
      return true;
    }
    return false;
  }
  ctx.write_set_.push_back(bpm_->WritePage(header->root_page_id_));
  header_guard.Drop();
  while (true) {
    const auto *page = ctx.write_set_.back().As<BPlusTreePage>();
    if (page->IsLeafPage()) {
      break;
    }
    const auto *internal = ctx.write_set_.back().As<InternalPage>();
    const page_id_t child_id = internal->Lookup(key,comparator_)
    BUSTUB_ASSERT(child_id != INVALID_PAGE_ID, "child_id is not valid");
    auto child_guard = bpm_->WritePage(child_id);
  }
  auto *leaf = ctx.write_set_.back().AsMut<LeafPage>();
  auto insert_result = leaf->TryInsert(key, value, comparator_);
  if (insert_result == LeafInsertStatus::INSERTED) {
    return true;
  }
  if (insert_result == LeafInsertStatus::DUPLICATE) {
    return false;
  }
  if (insert_result == LeafInsertStatus::NEED_SPLIT) {

  }


}
/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/**
 * @brief Delete key & value pair associated with input key
 * If current tree is empty, return immediately.
 * If not, User needs to first find the right leaf page as deletion target, then
 * delete entry from leaf page. Remember to deal with redistribute or merge if
 * necessary.
 *
 * @param key input key
 */
FULL_INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Remove(const KeyType &key) {
  // Declaration of context instance.
  Context ctx;
  UNIMPLEMENTED("TODO(P2): Add implementation.");
}

/*****************************************************************************
 * INDEX ITERATOR
 *****************************************************************************/
/**
 * @brief Input parameter is void, find the leftmost leaf page first, then construct
 * index iterator
 *
 * You may want to implement this while implementing Task #3.
 *
 * @return : index iterator
 */
FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin() -> INDEXITERATOR_TYPE { UNIMPLEMENTED("TODO(P2): Add implementation."); }

/**
 * @brief Input parameter is low key, find the leaf page that contains the input key
 * first, then construct index iterator
 * @return : index iterator
 */
FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin(const KeyType &key) -> INDEXITERATOR_TYPE { UNIMPLEMENTED("TODO(P2): Add implementation."); }

/**
 * @brief Input parameter is void, construct an index iterator representing the end
 * of the key/value pair in the leaf node
 * @return : index iterator
 */
FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::End() -> INDEXITERATOR_TYPE { UNIMPLEMENTED("TODO(P2): Add implementation."); }

/**
 * @return Page id of the root of this tree
 *
 * You may want to implement this while implementing Task #3.
 */
FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetRootPageId() -> page_id_t { UNIMPLEMENTED("TODO(P2): Add implementation."); }

template class BPlusTree<GenericKey<4>, RID, GenericComparator<4>>;

template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>>;
template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>, 3>;
template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>, 2>;
template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>, 1>;
template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>, -1>;

template class BPlusTree<GenericKey<16>, RID, GenericComparator<16>>;

template class BPlusTree<GenericKey<32>, RID, GenericComparator<32>>;

template class BPlusTree<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub
