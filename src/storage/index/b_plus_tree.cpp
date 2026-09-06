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
  const auto *header = header_guard.As<BPlusTreeHeaderPage>();

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
  const auto *header = header_guard.As<BPlusTreeHeaderPage>();

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
    const page_id_t child_id = internal->Lookup(key, comparator_);
    BUSTUB_ASSERT(child_id != INVALID_PAGE_ID, "child_id is not valid");
    auto child_guard = bpm_->ReadPage(child_id);
    ctx.read_set_.pop_back();
    ctx.read_set_.push_back(std::move(child_guard));
  }
  const auto *leaf = ctx.read_set_.back().As<LeafPage>();
  int left = leaf->KeyIndex(key, comparator_);
  if (left == leaf->GetSize() || comparator_(leaf->KeyAt(left), key) != 0 || leaf->IsTombstone(left)) {
    return false;
  }

  result->push_back(leaf->ValueAt(left));
  return true;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::FindLeafForOptimisticWrite(const KeyType &key) -> std::optional<WritePageGuard> {
  auto header_guard = bpm_->ReadPage(header_page_id_);
  const page_id_t root_page_id = header_guard.As<BPlusTreeHeaderPage>()->root_page_id_;
  if (root_page_id == INVALID_PAGE_ID) {
    return std::nullopt;
  }

  auto page_guard = bpm_->ReadPage(root_page_id);
  while (!page_guard.As<BPlusTreePage>()->IsLeafPage()) {
    const page_id_t child_page_id = page_guard.As<InternalPage>()->Lookup(key, comparator_);
    auto child_guard = bpm_->ReadPage(child_page_id);
    page_guard = std::move(child_guard);
  }

  const page_id_t leaf_page_id = page_guard.GetPageId();
  page_guard.Drop();
  auto leaf_guard = bpm_->WritePage(leaf_page_id);
  header_guard.Drop();
  return std::optional<WritePageGuard>{std::move(leaf_guard)};
}

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
  {
    auto leaf_guard = FindLeafForOptimisticWrite(key);
    if (leaf_guard.has_value()) {
      auto *leaf = leaf_guard->template AsMut<LeafPage>();
      const int index = leaf->KeyIndex(key, comparator_);
      const bool key_exists = index < leaf->GetSize() && comparator_(leaf->KeyAt(index), key) == 0;

      if (key_exists || leaf->GetSize() + 1 < leaf->GetMaxSize()) {
        const auto status = leaf->TryInsert(key, value, comparator_);
        BUSTUB_ASSERT(status != LeafInsertStatus::INSERTED_NEED_SPLIT,
                      "optimistic insert unexpectedly requires a split");
        return status == LeafInsertStatus::INSERTED;
      }
    }
  }

  Context ctx;
  auto header_guard = bpm_->WritePage(header_page_id_);
  auto *header = header_guard.AsMut<BPlusTreeHeaderPage>();
  if (header->root_page_id_ == INVALID_PAGE_ID) {
    header->root_page_id_ = bpm_->NewPage();
    auto leaf_guard = bpm_->WritePage(header->root_page_id_);
    auto *leaf = leaf_guard.AsMut<LeafPage>();
    leaf->Init(leaf_max_size_);
    if (leaf->TryInsert(key, value, comparator_) == LeafInsertStatus::INSERTED) {
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
    const page_id_t child_id = internal->Lookup(key, comparator_);
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
    page_id_t left_page_id = ctx.write_set_.back().GetPageId();

    page_id_t right_page_id = bpm_->NewPage();
    auto right_guard = bpm_->WritePage(right_page_id);
    auto *right = right_guard.AsMut<LeafPage>();

    right->Init(leaf_max_size_);

    KeyType separator = leaf->Split(*right, right_page_id);

    while (true) {
      // 情况一：刚刚分裂的是根
      if (ctx.IsRootPage(left_page_id)) {
        const page_id_t new_root_page_id = bpm_->NewPage();
        auto new_root_guard = bpm_->WritePage(new_root_page_id);
        auto *new_root = new_root_guard.AsMut<InternalPage>();

        new_root->Init(internal_max_size_);
        new_root->InitRoot(left_page_id, separator, right_page_id);

        ctx.header_page_->AsMut<BPlusTreeHeaderPage>()->root_page_id_ = new_root_page_id;

        return true;
      }

      // 当前 back() 是刚刚分裂的左节点，将它移除后得到父节点
      ctx.write_set_.pop_back();

      const page_id_t parent_page_id = ctx.write_set_.back().GetPageId();

      auto *parent = ctx.write_set_.back().AsMut<InternalPage>();

      // 情况二：父节点还有空间
      if (parent->GetSize() < parent->GetMaxSize()) {
        parent->InsertAfter(left_page_id, separator, right_page_id);
        return true;
      }

      // 情况三：父节点也满了
      const page_id_t new_right_page_id = bpm_->NewPage();
      auto new_right_guard = bpm_->WritePage(new_right_page_id);

      auto *new_right = new_right_guard.AsMut<InternalPage>();

      new_right->Init(internal_max_size_);

      KeyType promoted_key = parent->SplitAndInsert(*new_right, left_page_id, separator, right_page_id);

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
  {
    auto leaf_guard = FindLeafForOptimisticWrite(key);
    if (leaf_guard.has_value()) {
      auto *leaf = leaf_guard->template AsMut<LeafPage>();
      const int index = leaf->KeyIndex(key, comparator_);
      if (index == leaf->GetSize() || comparator_(leaf->KeyAt(index), key) != 0 || leaf->IsTombstone(index)) {
        return;
      }

      const bool applies_physical_delete =
          LEAF_PAGE_TOMB_CNT == 0 || leaf->GetTombstones().size() == static_cast<size_t>(LEAF_PAGE_TOMB_CNT);
      if (!applies_physical_delete || leaf->GetSize() - 1 >= leaf->GetMinSize()) {
        const bool removed = leaf->TryRemove(key, comparator_);
        BUSTUB_ASSERT(removed, "optimistic delete failed after locating the key");
        return;
      }
    }
  }

  Context ctx;
  auto header_guard = bpm_->WritePage(header_page_id_);
  auto *header = header_guard.AsMut<BPlusTreeHeaderPage>();
  if (header->root_page_id_ == INVALID_PAGE_ID) {
    return;
  }

  ctx.root_page_id_ = header->root_page_id_;
  ctx.header_page_.emplace(std::move(header_guard));
  ctx.write_set_.push_back(bpm_->WritePage(ctx.root_page_id_));

  while (!ctx.write_set_.back().As<BPlusTreePage>()->IsLeafPage()) {
    const auto *internal = ctx.write_set_.back().As<InternalPage>();
    const page_id_t child_id = internal->Lookup(key, comparator_);
    BUSTUB_ASSERT(child_id != INVALID_PAGE_ID, "child page id is invalid");
    ctx.write_set_.push_back(bpm_->WritePage(child_id));
  }

  auto *leaf = ctx.write_set_.back().AsMut<LeafPage>();
  if (!leaf->TryRemove(key, comparator_)) {
    return;
  }

  page_id_t current_page_id = ctx.write_set_.back().GetPageId();
  if (ctx.IsRootPage(current_page_id)) {
    if (leaf->GetSize() == 0) {
      ctx.header_page_->AsMut<BPlusTreeHeaderPage>()->root_page_id_ = INVALID_PAGE_ID;
      ctx.write_set_.pop_back();
      BUSTUB_ASSERT(bpm_->DeletePage(current_page_id), "failed to delete empty root leaf page");
    }
    return;
  }

  {
    auto *parent = ctx.write_set_[ctx.write_set_.size() - 2].AsMut<InternalPage>();
    const int child_index = parent->ValueIndex(current_page_id);
    BUSTUB_ASSERT(child_index >= 0, "leaf page is missing from its parent");
    if (child_index > 0 && leaf->GetSize() > 0) {
      parent->SetKeyAt(child_index, leaf->KeyAt(0));
    }
  }

  while (true) {
    auto *current_page = ctx.write_set_.back().AsMut<BPlusTreePage>();

    if (ctx.IsRootPage(current_page_id)) {
      BUSTUB_ASSERT(!current_page->IsLeafPage(), "root leaf should have been handled before rebalancing");
      auto *root = ctx.write_set_.back().AsMut<InternalPage>();
      if (root->GetSize() == 1) {
        const page_id_t new_root_page_id = root->ValueAt(0);
        ctx.header_page_->AsMut<BPlusTreeHeaderPage>()->root_page_id_ = new_root_page_id;
        ctx.write_set_.pop_back();
        BUSTUB_ASSERT(bpm_->DeletePage(current_page_id), "failed to delete old internal root page");
      }
      return;
    }

    if (current_page->GetSize() >= current_page->GetMinSize()) {
      return;
    }

    BUSTUB_ASSERT(ctx.write_set_.size() >= 2, "non-root page has no parent guard");
    auto *parent = ctx.write_set_[ctx.write_set_.size() - 2].AsMut<InternalPage>();
    const page_id_t parent_page_id = ctx.write_set_[ctx.write_set_.size() - 2].GetPageId();
    const int child_index = parent->ValueIndex(current_page_id);
    BUSTUB_ASSERT(child_index >= 0, "current page is missing from its parent");

    const bool has_left_sibling = child_index > 0;
    const bool has_right_sibling = child_index + 1 < parent->GetSize();
    BUSTUB_ASSERT(has_left_sibling || has_right_sibling, "underfull non-root page has no sibling");

    if (current_page->IsLeafPage()) {
      auto *current_leaf = ctx.write_set_.back().AsMut<LeafPage>();

      if (has_left_sibling) {
        const page_id_t left_page_id = parent->ValueAt(child_index - 1);
        auto left_guard = bpm_->WritePage(left_page_id);
        auto *left = left_guard.AsMut<LeafPage>();

        while (current_leaf->GetSize() < current_leaf->GetMinSize() && left->GetSize() > left->GetMinSize()) {
          left->MoveLastToFrontOf(*current_leaf, comparator_);
        }
        if (current_leaf->GetSize() >= current_leaf->GetMinSize()) {
          parent->SetKeyAt(child_index, current_leaf->KeyAt(0));
          return;
        }
      }

      if (has_right_sibling) {
        const page_id_t right_page_id = parent->ValueAt(child_index + 1);
        auto right_guard = bpm_->WritePage(right_page_id);
        auto *right = right_guard.AsMut<LeafPage>();

        while (current_leaf->GetSize() < current_leaf->GetMinSize() && right->GetSize() > right->GetMinSize()) {
          right->MoveFirstToEndOf(*current_leaf, comparator_);
        }
        if (current_leaf->GetSize() >= current_leaf->GetMinSize()) {
          parent->SetKeyAt(child_index + 1, right->KeyAt(0));
          return;
        }
      }

      page_id_t deleted_page_id;
      if (has_left_sibling) {
        const page_id_t left_page_id = parent->ValueAt(child_index - 1);
        auto left_guard = bpm_->WritePage(left_page_id);
        auto *left = left_guard.AsMut<LeafPage>();
        current_leaf->MoveAllTo(*left, comparator_);
        parent->RemoveAt(child_index);
        deleted_page_id = current_page_id;
        ctx.write_set_.pop_back();
      } else {
        const page_id_t right_page_id = parent->ValueAt(child_index + 1);
        auto right_guard = bpm_->WritePage(right_page_id);
        auto *right = right_guard.AsMut<LeafPage>();
        right->MoveAllTo(*current_leaf, comparator_);
        parent->RemoveAt(child_index + 1);
        deleted_page_id = right_page_id;
        right_guard.Drop();
        ctx.write_set_.pop_back();
      }
      BUSTUB_ASSERT(bpm_->DeletePage(deleted_page_id), "failed to delete merged leaf page");
    } else {
      auto *current_internal = ctx.write_set_.back().AsMut<InternalPage>();

      if (has_left_sibling) {
        const page_id_t left_page_id = parent->ValueAt(child_index - 1);
        auto left_guard = bpm_->WritePage(left_page_id);
        auto *left = left_guard.AsMut<InternalPage>();
        if (left->GetSize() > left->GetMinSize()) {
          const KeyType new_parent_key = left->MoveLastToFrontOf(*current_internal, parent->KeyAt(child_index));
          parent->SetKeyAt(child_index, new_parent_key);
          return;
        }
      }

      if (has_right_sibling) {
        const page_id_t right_page_id = parent->ValueAt(child_index + 1);
        auto right_guard = bpm_->WritePage(right_page_id);
        auto *right = right_guard.AsMut<InternalPage>();
        if (right->GetSize() > right->GetMinSize()) {
          const KeyType new_parent_key = right->MoveFirstToEndOf(*current_internal, parent->KeyAt(child_index + 1));
          parent->SetKeyAt(child_index + 1, new_parent_key);
          return;
        }
      }

      page_id_t deleted_page_id;
      if (has_left_sibling) {
        const page_id_t left_page_id = parent->ValueAt(child_index - 1);
        auto left_guard = bpm_->WritePage(left_page_id);
        auto *left = left_guard.AsMut<InternalPage>();
        current_internal->MoveAllTo(*left, parent->KeyAt(child_index));
        parent->RemoveAt(child_index);
        deleted_page_id = current_page_id;
        ctx.write_set_.pop_back();
      } else {
        const page_id_t right_page_id = parent->ValueAt(child_index + 1);
        auto right_guard = bpm_->WritePage(right_page_id);
        auto *right = right_guard.AsMut<InternalPage>();
        right->MoveAllTo(*current_internal, parent->KeyAt(child_index + 1));
        parent->RemoveAt(child_index + 1);
        deleted_page_id = right_page_id;
        right_guard.Drop();
        ctx.write_set_.pop_back();
      }
      BUSTUB_ASSERT(bpm_->DeletePage(deleted_page_id), "failed to delete merged internal page");
    }

    current_page_id = parent_page_id;
  }
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
auto BPLUSTREE_TYPE::Begin() -> INDEXITERATOR_TYPE {
  auto header_guard = bpm_->ReadPage(header_page_id_);
  const page_id_t root_page_id = header_guard.As<BPlusTreeHeaderPage>()->root_page_id_;
  if (root_page_id == INVALID_PAGE_ID) {
    return End();
  }

  auto page_guard = bpm_->ReadPage(root_page_id);
  header_guard.Drop();
  while (!page_guard.As<BPlusTreePage>()->IsLeafPage()) {
    const page_id_t child_page_id = page_guard.As<InternalPage>()->ValueAt(0);
    auto child_guard = bpm_->ReadPage(child_page_id);
    page_guard = std::move(child_guard);
  }

  return INDEXITERATOR_TYPE(bpm_, std::move(page_guard), 0);
}

/**
 * @brief Input parameter is low key, find the leaf page that contains the input key
 * first, then construct index iterator
 * @return : index iterator
 */
FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin(const KeyType &key) -> INDEXITERATOR_TYPE {
  auto header_guard = bpm_->ReadPage(header_page_id_);
  const page_id_t root_page_id = header_guard.As<BPlusTreeHeaderPage>()->root_page_id_;
  if (root_page_id == INVALID_PAGE_ID) {
    return End();
  }

  auto page_guard = bpm_->ReadPage(root_page_id);
  header_guard.Drop();
  while (!page_guard.As<BPlusTreePage>()->IsLeafPage()) {
    const page_id_t child_page_id = page_guard.As<InternalPage>()->Lookup(key, comparator_);
    auto child_guard = bpm_->ReadPage(child_page_id);
    page_guard = std::move(child_guard);
  }

  const int index = page_guard.As<LeafPage>()->KeyIndex(key, comparator_);
  return INDEXITERATOR_TYPE(bpm_, std::move(page_guard), index);
}

/**
 * @brief Input parameter is void, construct an index iterator representing the end
 * of the key/value pair in the leaf node
 * @return : index iterator
 */
FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::End() -> INDEXITERATOR_TYPE { return INDEXITERATOR_TYPE(); }

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
