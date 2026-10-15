#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <stddef.h>
#include <stdint.h>

#include <type_traits>

// A FreeRTOS queue with static storage that accepts only items of type T.
// FreeRTOS copies items as raw bytes through void* (xQueueGenericSend, xQueueReceive), and a
// QueueHandle_t does not record the item type. This class ties the handle, the storage size and
// the item type together, so a wrong item type is a compile error.
template <typename T, size_t Length>
class StaticTypedQueue {
    static_assert(std::is_trivially_copyable<T>::value, "FreeRTOS copies queue items with memcpy");

   public:
    // Deletes a queue made by an earlier call, then makes the queue again on the same storage.
    // Static creation with non-null buffers cannot return NULL (IDF 4.4 freertos/queue.h:202).
    void create() {
        if (handle_ != nullptr) {
            vQueueDelete(handle_);
        }

        handle_ = xQueueCreateStatic(Length, sizeof(T), storage_, &control_);
    }

    bool isCreated() const { return handle_ != nullptr; }

    bool send(const T& item, TickType_t wait_ticks) {
        return xQueueSend(handle_, &item, wait_ticks) == pdTRUE;
    }

    // Replaces the held item, so the queue always has the latest one. Only for length 1.
    void overwrite(const T& item) {
        static_assert(Length == 1, "xQueueOverwrite needs a queue of length 1");
        xQueueOverwrite(handle_, &item);
    }

    bool receive(T& out_item, TickType_t wait_ticks) {
        return xQueueReceive(handle_, &out_item, wait_ticks) == pdTRUE;
    }

    bool peek(T& out_item, TickType_t wait_ticks) {
        return xQueuePeek(handle_, &out_item, wait_ticks) == pdTRUE;
    }

    void reset() { xQueueReset(handle_); }

   private:
    StaticQueue_t control_ = {};
    uint8_t storage_[Length * sizeof(T)] = {};
    QueueHandle_t handle_ = nullptr;
};
