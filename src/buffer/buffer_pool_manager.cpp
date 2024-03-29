//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// buffer_pool_manager.cpp
//
// Identification: src/buffer/buffer_pool_manager.cpp
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/buffer_pool_manager.h"

#include <buffer/clock_replacer.h>
#include <unordered_map>

namespace bustub {

BufferPoolManager::BufferPoolManager(size_t pool_size, DiskManager *disk_manager, LogManager *log_manager)
    : pool_size_(pool_size), disk_manager_(disk_manager), log_manager_(log_manager) {
  // We allocate a consecutive memory space for the buffer pool.
  pages_ = new Page[pool_size_];
  replacer_ = new ClockReplacer(pool_size);

  // Initially, every page is in the free list.
  for (size_t i = 0; i < pool_size_; ++i) {
    free_list_.emplace_back(static_cast<int>(i));
  }
}

BufferPoolManager::~BufferPoolManager() {
  delete[] pages_;
  delete replacer_;
}

Page *BufferPoolManager::FetchPageImpl(page_id_t page_id) {
  // 1.     Search the page table for the requested page (P).
  // 1.1    If P exists, pin it and return it immediately.
  // 1.2    If P does not exist, find a replacement page (R) from either the free list or the replacer.
  //        Note that pages are always found from the free list first.
  // 2.     If R is dirty, write it back to the disk.
  // 3.     Delete R from the page table and insert P.
  // 4.     Update P's metadata, read in the page content from disk, and then return a pointer to P.
  Page *page = nullptr;
  auto p_iterator = page_table_.find(page_id);
  if (p_iterator != page_table_.end()) {
    replacer_->Pin(p_iterator->second);
    page = &pages_[p_iterator->second];
    page->pin_count_++;
    return page;
  }
  frame_id_t frame_id;
  page = GetVictimPage(&frame_id);
  if (page != nullptr) {
    if (page->is_dirty_) {
      disk_manager_->WritePage(page->page_id_, page->data_);
    }
    page_table_.erase(page->page_id_);
    page_table_[page_id] = frame_id;
    page->ResetMemory();
    page->page_id_ = page_id;
    page->pin_count_ = 1;
    disk_manager_->ReadPage(page_id, page->data_);
  }
  return page;
}

bool BufferPoolManager::UnpinPageImpl(page_id_t page_id, bool is_dirty) {
  auto p_iterator = page_table_.find(page_id);
  if (p_iterator != page_table_.end()) {
    auto *page = &pages_[p_iterator->second];
    if (page->pin_count_ > 0) {
      page->pin_count_--;
    }
    if (page->pin_count_ == 0) {
      if (is_dirty) {
        disk_manager_->WritePage(page->page_id_, page->data_);
      }
      replacer_->Unpin(p_iterator->second);
      page_table_.erase(p_iterator->first);
      return true;
    }
    return false;
  }
  return false;
}

bool BufferPoolManager::FlushPageImpl(page_id_t page_id) {
  // Make sure you call DiskManager::WritePage!
  auto p_iterator = page_table_.find(page_id);
  if (p_iterator != page_table_.end()) {
    auto page = &pages_[p_iterator->second];
    disk_manager_->WritePage(page_id, page->data_);
    replacer_->Unpin(p_iterator->second);
    page_table_.erase(page_id);
    return true;
  }
  return false;
}

Page *BufferPoolManager::NewPageImpl(page_id_t *page_id) {
  // 0.   Make sure you call DiskManager::AllocatePage!
  // 1.   If all the pages in the buffer pool are frame_pinned, return nullptr.
  // 2.   Pick a victim page P from either the free list or the replacer. Always pick from the free list first.
  // 3.   Update P's metadata, zero out memory and add P to the page table.
  // 4.   Set the page ID output parameter. Return a pointer to P.
  frame_id_t frame_id;
  Page *page = GetVictimPage(&frame_id);
  if (page != nullptr) {
    page->page_id_ = disk_manager_->AllocatePage();
    page->pin_count_ = 0;
    page->ResetMemory();
    page_table_[page->page_id_] = frame_id;
    *page_id = page->page_id_;
  }
  return page;
}

bool BufferPoolManager::DeletePageImpl(page_id_t page_id) {
  // 0.   Make sure you call DiskManager::DeallocatePage!
  // 1.   Search the page table for the requested page (P).
  // 1.   If P does not exist, return true.
  // 2.   If P exists, but has a non-zero pin-count, return false. Someone is using the page.
  // 3.   Otherwise, P can be deleted. Remove P from the page table, reset its metadata and return it to the free list.
  auto p_iterator = page_table_.find(page_id);
  if (p_iterator != page_table_.end()) {
    auto page = &pages_[p_iterator->second];
    if (page->pin_count_ > 0) {
      return false;
    }
    page_table_.erase(p_iterator->first);
    page->pin_count_ = 0;
    page->is_dirty_ = false;
    page->page_id_ = INVALID_PAGE_ID;
    free_list_.push_back(p_iterator->second);
    return true;
  }
  return true;
}

void BufferPoolManager::FlushAllPagesImpl() {
  // You can do it!
  for (auto p : page_table_) {
    auto page = &pages_[p.second];
    disk_manager_->WritePage(p.first, page->data_);
    replacer_->Unpin(p.second);
  }
  page_table_.clear();
}

Page *BufferPoolManager::GetVictimPage(frame_id_t *frame_id) {
  Page *page = nullptr;
  if (free_list_.empty()) {
    if (replacer_->Victim(frame_id)) {
      page = &pages_[*frame_id];
      return page;
    }

  } else {
    *frame_id = free_list_.front();
    free_list_.pop_front();
    page = &pages_[*frame_id];
  }
  return page;
}

}  // namespace bustub
