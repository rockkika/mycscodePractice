// :bustub-keep-private:
//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// arc_replacer.cpp
//
// Identification: src/buffer/arc_replacer.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/arc_replacer.h"

#include <algorithm>
#include <optional>

#include "common/config.h"

namespace bustub {
/**
 *
 * TODO(P1): Add implementation
 *
 * @brief a new ArcReplacer, with lists initialized to be empty and target size to 0
 * @param num_frames the maximum number of frames the ArcReplacer will be required to cache
 */
ArcReplacer::ArcReplacer(size_t num_frames) : replacer_size_(num_frames) {}

/**
 * TODO(P1): Add implementation
 *
 * @brief Performs the Replace operation as described by the writeup
 * that evicts from either mfu_ or mru_ into its corresponding ghost list
 * according to balancing policy.
 *
 * If you wish to refer to the original ARC paper, please note that there are
 * two changes in our implementation:
 * 1. When the size of mru_ equals the target size, we don't check
 * the last access as the paper did when deciding which list to evict from.
 * This is fine since the original decision is stated to be arbitrary.
 * 2. Entries that are not evictable are skipped. If all entries from the desired side
 * (mru_ / mfu_) are pinned, we instead try victimize the other side (mfu_ / mru_),
 * and move it to its corresponding ghost list (mfu_ghost_ / mru_ghost_).
 *
 * @return frame id of the evicted frame, or std::nullopt if cannot evict
 */
auto ArcReplacer::Evict() -> std::optional<frame_id_t> {
  std::lock_guard guard(latch_);
  if (curr_size_ == 0) {
    return std::nullopt;
  }

  if (mru_.size() >= mru_target_size_ || mru_curr_size_ == curr_size_) {
    for (auto it = mru_.begin(); it != mru_.end(); ++it) {
      if (frame_id_t curr_frame = *it; alive_map_[curr_frame]->evictable_) {
        const auto pg = alive_map_[curr_frame]->page_id_;
        ghost_map_[pg] = alive_map_[curr_frame];
        ghost_map_[pg]->arc_status_ = ArcStatus::MRU_GHOST;
        mru_ghost_.push_back(pg);
        mru_ghost_map_[pg] = std::prev(mru_ghost_.end());
        alive_map_.erase(curr_frame);
        mru_map_.erase(curr_frame);
        mru_.erase(it);
        curr_size_--;
        mru_curr_size_--;
        return curr_frame;
      }
    }
  }
  for (auto it = mfu_.begin(); it != mfu_.end(); ++it) {
    if (frame_id_t curr_frame = *it; alive_map_[curr_frame]->evictable_) {
      const auto pg = alive_map_[curr_frame]->page_id_;
      ghost_map_[pg] = alive_map_[curr_frame];
      ghost_map_[pg]->arc_status_ = ArcStatus::MFU_GHOST;
      mfu_ghost_.push_back(pg);
      mfu_ghost_map_[pg] = std::prev(mfu_ghost_.end());
      alive_map_.erase(curr_frame);
      mfu_map_.erase(curr_frame);
      mfu_.erase(it);
      curr_size_--;
      return curr_frame;
    }
  }

  return std::nullopt;
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Record access to a frame, adjusting ARC bookkeeping accordingly
 * by bring the accessed page to the front of mfu_ if it exists in any of the lists
 * or the front of mru_ if it does not.
 *
 * Performs the operations EXCEPT REPLACE described in original paper, which is
 * handled by `Evict()`.
 *
 * Consider the following four cases, handle accordingly:
 * 1. Access hits mru_ or mfu_
 * 2/3. Access hits mru_ghost_ / mfu_ghost_
 * 4. Access misses all the lists
 *
 * This routine performs all changes to the four lists as preperation
 * for `Evict()` to simply find and evict a victim into ghost lists.
 *
 * Note that frame_id is used as identifier for alive pages and
 * page_id is used as identifier for the ghost pages, since page_id is
 * the unique identifier to the page after it's dead.
 * Using page_id for alive pages should be the same since it's one to one mapping,
 * but using frame_id is slightly more intuitive.
 *
 * @param frame_id id of frame that received a new access.
 * @param page_id id of page that is mapped to the frame.
 * @param access_type type of access that was received. This parameter is only needed for
 * leaderboard tests.
 */
void ArcReplacer::RecordAccess(frame_id_t frame_id, page_id_t page_id, [[maybe_unused]] AccessType access_type) {
  std::lock_guard guard(latch_);
  std::list<frame_id_t>::iterator mru_found, mru_ghost_found, mfu_found, mfu_ghost_found;
  bool in_mru = false, in_mfu = false, in_mru_ghost = false, in_mfu_ghost = false;
  if (mru_map_.find(frame_id) != mru_map_.end()) {
    mru_found = mru_map_[frame_id];
    in_mru = true;
  }
  if (mru_ghost_map_.find(page_id) != mru_ghost_map_.end()) {
    mru_ghost_found = mru_ghost_map_[page_id];
    in_mru_ghost = true;
  }
  if (mfu_map_.find(frame_id) != mfu_map_.end()) {
    mfu_found = mfu_map_[frame_id];
    in_mfu = true;
  }
  if (mfu_ghost_map_.find(page_id) != mfu_ghost_map_.end()) {
    mfu_ghost_found = mfu_ghost_map_[page_id];
    in_mfu_ghost = true;
  }
  if (in_mru || in_mfu) {
    if (in_mru) {
      mfu_.push_back(frame_id);
      mfu_map_[frame_id] = std::prev(mfu_.end());
      mru_.erase(mru_found);
      mru_map_.erase(frame_id);
      alive_map_[frame_id]->arc_status_ = ArcStatus::MFU;
      if (alive_map_[frame_id]->evictable_ == true) {
        mru_curr_size_--;
      }
    } else {
      mfu_.erase(mfu_found);
      mfu_.push_back(frame_id);
      mfu_map_[frame_id] = std::prev(mfu_.end());
    }
  } else if (in_mru_ghost) {
    if (mru_ghost_.size() >= mfu_ghost_.size()) {
      mru_target_size_++;
    } else {
      mru_target_size_ += mfu_ghost_.size() / mru_ghost_.size();
    }
    if (mru_target_size_ > replacer_size_) {
      mru_target_size_ = replacer_size_;
    }
    mfu_.push_back(frame_id);
    mfu_map_[frame_id] = std::prev(mfu_.end());
    alive_map_[frame_id] = ghost_map_[*mru_ghost_found];
    ghost_map_.erase(*mru_ghost_found);
    mru_ghost_map_.erase(*mru_ghost_found);
    mru_ghost_.erase(mru_ghost_found);
    alive_map_[frame_id]->arc_status_ = ArcStatus::MFU;
    alive_map_[frame_id]->evictable_ = false;
    alive_map_[frame_id]->frame_id_ = frame_id;
    alive_map_[frame_id]->page_id_ = page_id;
  } else if (in_mfu_ghost) {
    const size_t delta = mfu_ghost_.size() >= mru_ghost_.size() ? 1 : mru_ghost_.size() / mfu_ghost_.size();
    if (delta > mru_target_size_) {
      mru_target_size_ = 0;
    } else {
      mru_target_size_ -= delta;
    }
    mfu_.push_back(frame_id);
    mfu_map_[frame_id] = std::prev(mfu_.end());
    alive_map_[frame_id] = ghost_map_[*mfu_ghost_found];
    ghost_map_.erase(*mfu_ghost_found);
    mfu_ghost_map_.erase(*mfu_ghost_found);
    mfu_ghost_.erase(mfu_ghost_found);
    alive_map_[frame_id]->arc_status_ = ArcStatus::MFU;
    alive_map_[frame_id]->evictable_ = false;
    alive_map_[frame_id]->frame_id_ = frame_id;
    alive_map_[frame_id]->page_id_ = page_id;
  } else {
    if (mru_.size() + mru_ghost_.size() == replacer_size_) {
      const page_id_t removed_page = mru_ghost_.front();
      mru_ghost_.pop_front();
      mru_ghost_map_.erase(removed_page);
      ghost_map_.erase(removed_page);
    } else if (mru_.size() + mru_ghost_.size() + mfu_.size() + mfu_ghost_.size() == 2 * replacer_size_) {
      const page_id_t removed_page = mfu_ghost_.front();
      mfu_ghost_.pop_front();
      mfu_ghost_map_.erase(removed_page);
      ghost_map_.erase(removed_page);
    }
    mru_.push_back(frame_id);
    mru_map_[frame_id] = std::prev(mru_.end());
    alive_map_[frame_id] = std::make_shared<FrameStatus>(page_id, frame_id, false, ArcStatus::MRU);
  }
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Toggle whether a frame is evictable or non-evictable. This function also
 * controls replacer's size. Note that size is equal to number of evictable entries.
 *
 * If a frame was previously evictable and is to be set to non-evictable, then size should
 * decrement. If a frame was previously non-evictable and is to be set to evictable,
 * then size should increment.
 *
 * If frame id is invalid, throw an exception or abort the process.
 *
 * For other scenarios, this function should terminate without modifying anything.
 *
 * @param frame_id id of frame whose 'evictable' status will be modified
 * @param set_evictable whether the given frame is evictable or not
 */
void ArcReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
  std::lock_guard guard(latch_);
  const auto &alive_it = alive_map_.find(frame_id);
  if (alive_it == alive_map_.end()) {
    throw std::invalid_argument("Invalid frame id");
  }
  if (alive_it->second->evictable_ && !set_evictable) {
    curr_size_--;
    if (alive_it->second->arc_status_ == ArcStatus::MRU) {
      mru_curr_size_--;
    }
  } else if (!alive_it->second->evictable_ && set_evictable) {
    if (alive_it->second->arc_status_ == ArcStatus::MRU) {
      mru_curr_size_++;
    }
    curr_size_++;
  }
  alive_it->second->evictable_ = set_evictable;
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Remove an evictable frame from replacer.
 * This function should also decrement replacer's size if removal is successful.
 *
 * Note that this is different from evicting a frame, which always remove the frame
 * decided by the ARC algorithm.
 *
 * If Remove is called on a non-evictable frame, throw an exception or abort the
 * process.
 *
 * If specified frame is not found, directly return from this function.
 *
 * @param frame_id id of frame to be removed
 */
void ArcReplacer::Remove(frame_id_t frame_id) {
  std::lock_guard guard(latch_);
  // if (frame_id < 0) {
  //   throw std::invalid_argument("Invalid frame id");
  // }

  auto it = alive_map_.find(frame_id);
  if (it == alive_map_.end()) {
    return;
  }

  const auto &status = it->second;

  if (!status->evictable_) {
    throw std::invalid_argument("Cannot remove a non-evictable frame");
  }

  if (status->arc_status_ == ArcStatus::MRU) {
    const std::list<int>::const_iterator pos = mru_map_[frame_id];
    mru_.erase(pos);
    mru_map_.erase(frame_id);
    mru_curr_size_--;
  } else if (status->arc_status_ == ArcStatus::MFU) {
    const std::list<int>::const_iterator pos = mfu_map_[frame_id];
    mfu_.erase(pos);
    mfu_map_.erase(frame_id);
  }
  alive_map_.erase(it);
  curr_size_--;
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Return replacer's size, which tracks the number of evictable frames.
 *
 * @return size_t
 */
auto ArcReplacer::Size() -> size_t {
  std::lock_guard guard(latch_);
  return curr_size_;
}
}  // namespace bustub
