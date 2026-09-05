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
    const page_id_t child_id = internal->Lookup(key,comparator_);
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
  ctx.root_page_id_ = header->root_page_id_;
  ctx.header_page_.emplace(std::move(header_guard));
  while (true) {
    const auto *page = ctx.write_set_.back().As<BPlusTreePage>();
    if (page->IsLeafPage()) {
      break;
    }
    const auto *internal = ctx.write_set_.back().As<InternalPage>();
    const page_id_t child_id = internal->Lookup(key,comparator_);
    BUSTUB_ASSERT(child_id != INVALID_PAGE_ID, "child_id is not valid");
    auto child_guard = bpm_->WritePage(child_id);
    ctx.write_set_.push_back(std::move(child_guard));
  }
  auto *leaf = ctx.write_set_.back().AsMut<LeafPage>();

  auto insert_result = leaf->TryInsert(key, value, comparator_);
  if (insert_result == LeafInsertStatus::INSERTED) {

    return true;
  }
  if (insert_result == LeafInsertStatus::DUPLICATE) {
    return false;
  }

  BUSTUB_ASSERT(insert_result == LeafInsertStatus::INSERTED_NEED_SPLIT, "unexpected leaf insertion status");

  // key 已在原叶节点中，现在分裂
  {
    page_id_t left_page_id =
        ctx.write_set_.back().GetPageId();

    page_id_t right_page_id = bpm_->NewPage();
    auto right_guard = bpm_->WritePage(right_page_id);
    auto *right = right_guard.AsMut<LeafPage>();

    right->Init(leaf_max_size_);

    KeyType separator =
        leaf->Split(*right, right_page_id);

    while (true) {
      // 情况一：刚刚分裂的是根
      if (ctx.IsRootPage(left_page_id)) {
        const page_id_t new_root_page_id = bpm_->NewPage();
        auto new_root_guard = bpm_->WritePage(new_root_page_id);
        auto *new_root = new_root_guard.AsMut<InternalPage>();

        new_root->Init(internal_max_size_);
        new_root->InitRoot(
            left_page_id,
            separator,
            right_page_id);

        ctx.header_page_->AsMut<BPlusTreeHeaderPage>()
            ->root_page_id_ = new_root_page_id;

        return true;
      }

      // 当前 back() 是刚刚分裂的左节点，将它移除后得到父节点
      ctx.write_set_.pop_back();

      const page_id_t parent_page_id =
          ctx.write_set_.back().GetPageId();

      auto *parent =
          ctx.write_set_.back().AsMut<InternalPage>();

      // 情况二：父节点还有空间
      if (parent->GetSize() < parent->GetMaxSize()) {
        parent->InsertAfter(
            left_page_id,
            separator,
            right_page_id);
        return true;
      }

      // 情况三：父节点也满了
      const page_id_t new_right_page_id = bpm_->NewPage();
      auto new_right_guard =
          bpm_->WritePage(new_right_page_id);

      auto *new_right =
          new_right_guard.AsMut<InternalPage>();

      new_right->Init(internal_max_size_);

      KeyType promoted_key =
          parent->SplitAndInsert(
              *new_right,
              left_page_id,
              separator,
              right_page_id);

      // 将父节点分裂结果变成下一轮待插入的信息
      left_page_id = parent_page_id;
      separator = promoted_key;
      right_page_id = new_right_page_id;
    }
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
auto BPLUSTREE_TYPE::GetRootPageId() -> page_id_t {
  auto header_guard = bpm_->ReadPage(header_page_id_);
  return header_guard.As<BPlusTreeHeaderPage>()->root_page_id_;
}

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
