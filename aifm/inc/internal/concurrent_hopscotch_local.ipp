#pragma once

#include "hash.hpp"

#include <cstring>
#include <iostream>

namespace far_memory {

FORCE_INLINE GenericConcurrentHopscotchLocal::BucketEntry::BucketEntry() {
  bitmap = timestamp = 0;
  //ptr.nullify();
  ptr = nullptr;
}

FORCE_INLINE void GenericConcurrentHopscotchLocal::_get(uint8_t key_len,
                                                   const uint8_t *key,
                                                   uint16_t *val_len,
                                                   uint8_t *val,
                                                   bool *forwarded) {
  bool miss = __get(key_len, key, val_len, val);
  if (very_unlikely(miss)) {
    if (forwarded) {
      *forwarded = true;
    }
    forward_get(key_len, key, val_len, val);
  }
}

FORCE_INLINE void GenericConcurrentHopscotchLocal::get(const DerefScope &scope,
                                                  uint8_t key_len,
                                                  const uint8_t *key,
                                                  uint16_t *val_len,
                                                  uint8_t *val) {
  _get(key_len, key, val_len, val, nullptr);
}

FORCE_INLINE void GenericConcurrentHopscotchLocal::get_tp(uint8_t key_len,
                                                     const uint8_t *key,
                                                     uint16_t *val_len,
                                                     uint8_t *val) {
  DerefScope scope;
  get(scope, key_len, key, val_len, val);
}

FORCE_INLINE bool GenericConcurrentHopscotchLocal::put(const DerefScope &scope,
                                                  uint8_t key_len,
                                                  const uint8_t *key,
                                                  uint16_t val_len,
                                                  const uint8_t *val) {
  return _put(key_len, key, val_len, val, /* swap_in = */ false);
}

FORCE_INLINE bool GenericConcurrentHopscotchLocal::put_tp(uint8_t key_len,
                                                     const uint8_t *key,
                                                     uint16_t val_len,
                                                     const uint8_t *val) {
  DerefScope scope;
  return put(scope, key_len, key, val_len, val);
}

FORCE_INLINE bool GenericConcurrentHopscotchLocal::remove(const DerefScope &scope,
                                                     uint8_t key_len,
                                                     const uint8_t *key) {
  return _remove(key_len, key);
}

FORCE_INLINE bool GenericConcurrentHopscotchLocal::remove_tp(uint8_t key_len,
                                                        const uint8_t *key) {
  DerefScope scope;
  return remove(scope, key_len, key);
}

FORCE_INLINE void GenericConcurrentHopscotchLocal::process_evac_notifier_stash() {
  if (unlikely(evac_notifier_stash_.size())) {
    EvacNotifierMeta meta;
    while (evac_notifier_stash_.pop_front(&meta)) {
      do_evac_notifier(meta);
    }
  }
}

FORCE_INLINE void GenericConcurrentHopscotchLocal::evac_notifier(Object object) {
  process_evac_notifier_stash();
  auto *meta = reinterpret_cast<const EvacNotifierMeta *>(
      object.get_obj_id() - sizeof(EvacNotifierMeta));
  do_evac_notifier(*meta);
}

FORCE_INLINE bool GenericConcurrentHopscotchLocal::__get(uint8_t key_len,
                                                    const uint8_t *key,
                                                    uint16_t *val_len,
                                                    uint8_t *val) {
  uint32_t hash = hash_32(reinterpret_cast<const void *>(key), key_len);
  uint32_t bucket_idx = hash & kHashMask_;
  auto *bucket = buckets_ + bucket_idx;
  uint64_t timestamp;
  uint32_t retry_counter = 0;

  auto get_once = [&]<bool Lock>() -> bool {
    //retry:
      if constexpr (Lock) {
        while (unlikely(!bucket->spin.TryLockWp())) {
          thread_yield();
        }
      }
      auto spin_guard = helpers::finally([&]() {
        if constexpr (Lock) {
          bucket->spin.UnlockWp();
        }
      });
      timestamp = load_acquire(&(bucket->timestamp));
      uint32_t bitmap = bucket->bitmap;
      while (bitmap) {
        auto offset = helpers::bsf_32(bitmap);
        auto &ptr = buckets_[bucket_idx + offset].ptr;
        //if (likely(!ptr.is_null())) {
        if (likely(ptr)) {
          //auto *obj_val_ptr = ptr._deref<false, false>();
          auto *obj_val_ptr = reinterpret_cast<char *>(ptr) + Object::kHeaderSize;
          // Shi's guess:
          // When an object is swapped out, its local counterpart is removed from the local hash table, see `GenericConcurrentHopscotch::do_evac_notifier`.
          // The ptr is nullified before the bitmap. Therefore there may be times when a bucket is consider to be not empty according to the bitmap but the pointer is already nullified, so the if condition below is met.
          // Then we should flush all pending evac_notifier to make sure that bitmap is properly cleared before we do the get again.
          // Hence the retry.
          // For our tests we should never let objects be evacuated. Therefore there is no need to do this check.
          //if (unlikely(!obj_val_ptr)) {
          //  spin_guard.reset();
          //  process_evac_notifier_stash();
          //  thread_yield();
          //  goto retry;
          //}
          // Shi: structure of a hash table object (see object.hpp):
          // |<------------------ header ------------------>|
          // |ptr_addr(6B)|data_len(2B)|ds_id(1B)|id_len(1B)|obj_data(value+EvacNotifierMeta)|key(obj_id)
          auto obj = Object(reinterpret_cast<uint64_t>(obj_val_ptr) -
                            Object::kHeaderSize);
          if (obj.get_obj_id_len() == key_len) {
            auto obj_data_len = obj.get_data_len();
            if (strncmp(reinterpret_cast<const char *>(obj_val_ptr) +
                            obj_data_len,
                        reinterpret_cast<const char *>(key), key_len) == 0) {
              *val_len = obj_data_len - sizeof(EvacNotifierMeta);
              memcpy(val, obj_val_ptr, *val_len);
              return true;
            }
          }
        }
        bitmap ^= (1 << offset);
      }
      return false;
  };

  // Fast path.
  do {
    if (get_once.template operator()<false>()) {
      return false;
    }
  } while (timestamp != ACCESS_ONCE(bucket->timestamp) &&
           retry_counter++ < kMaxRetries);

  // Slow path.
  if (timestamp != ACCESS_ONCE(bucket->timestamp)) {
    if (get_once.template operator()<true>()) {
      return false;
    }
  }
  return true;
}

template <typename K, typename V>
FORCE_INLINE ConcurrentHopscotchLocal<K, V>::ConcurrentHopscotchLocal(
    uint8_t ds_id, uint32_t local_num_entries_shift,
    uint32_t remote_num_entries_shift, uint64_t remote_data_size)
    : GenericConcurrentHopscotchLocal(ds_id, local_num_entries_shift,
                                 remote_num_entries_shift, remote_data_size) {}

template <typename K, typename V>
FORCE_INLINE std::optional<V> ConcurrentHopscotchLocal<K, V>::_find(const K &key) {
  uint16_t val_len;
  V val;
  _get(sizeof(key), reinterpret_cast<const uint8_t *>(&key), &val_len,
       reinterpret_cast<uint8_t *>(&val), nullptr);
  if (val_len == 0) {
    return std::nullopt;
  } else {
    return val;
  }
}

template <typename K, typename V>
FORCE_INLINE void ConcurrentHopscotchLocal<K, V>::_insert(const K &key,
                                                     const V &val) {
  bool key_existed =
      _put(sizeof(key), reinterpret_cast<const uint8_t *>(&key), sizeof(val),
           reinterpret_cast<const uint8_t *>(&val), /* swap_in = */ false);
  if (!key_existed) {
    preempt_disable();
    per_core_size_[get_core_num()].data++;
    preempt_enable();
  }
}

template <typename K, typename V>
FORCE_INLINE bool ConcurrentHopscotchLocal<K, V>::_erase(const K &key) {
  bool key_existed =
      _remove(sizeof(key), reinterpret_cast<const uint8_t *>(&key));
  if (key_existed) {
    preempt_disable();
    per_core_size_[get_core_num()].data--;
    preempt_enable();
  }
  return key_existed;
}

template <typename K, typename V>
FORCE_INLINE bool ConcurrentHopscotchLocal<K, V>::empty() const {
  return size() == 0;
}

template <typename K, typename V>
FORCE_INLINE uint64_t ConcurrentHopscotchLocal<K, V>::size() const {
  int64_t sum = 0;
  FOR_ALL_SOCKET0_CORES(i) { sum += per_core_size_[i].data; }
  return sum;
}

template <typename K, typename V>
FORCE_INLINE std::optional<V>
ConcurrentHopscotchLocal<K, V>::find(const DerefScope &scope, const K &key) {
  return _find(key);
}

template <typename K, typename V>
FORCE_INLINE std::optional<V> ConcurrentHopscotchLocal<K, V>::find_tp(const K &key) {
  DerefScope scope;
  return _find(key);
}

template <typename K, typename V>
FORCE_INLINE void ConcurrentHopscotchLocal<K, V>::insert(const DerefScope &scope,
                                                    const K &key,
                                                    const V &val) {
  _insert(key, val);
}

template <typename K, typename V>
FORCE_INLINE void ConcurrentHopscotchLocal<K, V>::insert_tp(const K &key,
                                                       const V &val) {
  DerefScope scope;
  _insert(key, val);
}

template <typename K, typename V>
FORCE_INLINE bool ConcurrentHopscotchLocal<K, V>::erase(const DerefScope &scope,
                                                   const K &key) {
  return _erase(key);
}

template <typename K, typename V>
FORCE_INLINE bool ConcurrentHopscotchLocal<K, V>::erase_tp(const K &key) {
  DerefScope scope;
  return _erase(key);
}
} // namespace far_memory
