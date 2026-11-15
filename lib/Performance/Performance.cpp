#include "Performance.h"

#ifdef CROSSPOINT_PERFORMANCE_TRACE
#include <Arduino.h>
#include <Logging.h>
#ifndef SIMULATOR
#include <esp_timer.h>
#endif
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace performance {
namespace {
struct Measurement {
  uint64_t start = 0;
  const void* owner = nullptr;
  uint32_t id = 0;
  Operation kind = Operation::Navigation;
  uint64_t waitUs = 0, readUs = 0, writeUs = 0;
  uint64_t readBytes = 0, writeBytes = 0;
  uint32_t reads = 0, writes = 0;
  uint32_t counters[size_t(Counter::Count)]{};
};
// One bounded aggregate; no per-I/O event buffer or allocation in the storage path.
Measurement active;
uint32_t nextId = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

uint64_t now() {
#ifdef SIMULATOR
  return uint64_t(millis()) * 1000;
#else
  return esp_timer_get_time();
#endif
}

const char* operationName(Operation kind) {
  switch (kind) {
    case Operation::Navigation:
      return "navigation";
    case Operation::Format:
      return "format";
    case Operation::PageTurn:
      return "page_turn";
  }
  return "unknown";
}

const char* stageName(Stage stage) {
  switch (stage) {
    case Stage::Enter:
      return "enter";
    case Stage::Leave:
      return "leave";
    case Stage::Font:
      return "font";
    case Stage::Metadata:
      return "metadata";
    case Stage::Layout:
      return "layout";
    case Stage::PageLoad:
      return "page_load";
    case Stage::Extract:
      return "extract";
    case Stage::Decode:
      return "decode";
    case Stage::Draw:
      return "draw";
  }
  return "unknown";
}
}  // namespace

uint64_t timeMicros() { return now(); }

uint32_t begin(Operation kind, const void* owner, uint64_t requestedAt) {
  const auto at = requestedAt ? requestedAt : now();
  taskENTER_CRITICAL(&mux);
  // A pop with no parent routes through goHome after the outgoing onExit.
  const bool homeContinuation =
      active.id && active.kind == Operation::Navigation && !active.owner && kind == Operation::Navigation && owner;
  if (homeContinuation) {
    active.owner = owner;
  } else {
    if (++nextId == 0) ++nextId;
    active = {};
    active.id = nextId;
    active.kind = kind;
    active.owner = owner;
    active.start = at;
  }
  const auto id = active.id;
  taskEXIT_CRITICAL(&mux);
  return id;
}

uint32_t current() {
  taskENTER_CRITICAL(&mux);
  const auto id = active.id;
  taskEXIT_CRITICAL(&mux);
  return id;
}

Stamp stamp() {
  const auto id = current();
  return id ? Stamp{now(), id} : Stamp{};
}

void count(Counter counter) {
  taskENTER_CRITICAL(&mux);
  if (active.id && counter != Counter::Count) ++active.counters[size_t(counter)];
  taskEXIT_CRITICAL(&mux);
}

void span(Stamp start, Stage stage) {
  if (!start.operation) return;
  const auto elapsed = now() - start.at;
  LOG_INF("TRACE", "span id=%lu stage=%s ts=%llu dur=%llu task=%u", (unsigned long)start.operation, stageName(stage),
          (unsigned long long)start.at, (unsigned long long)elapsed,
          unsigned(reinterpret_cast<uintptr_t>(xTaskGetCurrentTaskHandle())));
}

void storageWait(Stamp start) {
  if (!start.operation) return;
  const auto elapsed = now() - start.at;
  taskENTER_CRITICAL(&mux);
  if (active.id == start.operation) active.waitUs += elapsed;
  taskEXIT_CRITICAL(&mux);
}

void storageIo(Stamp start, Io kind, size_t bytes) {
  if (!start.operation) return;
  const auto elapsed = now() - start.at;
  taskENTER_CRITICAL(&mux);
  if (active.id == start.operation) {
    switch (kind) {
      case Io::Read:
        ++active.reads;
        active.readUs += elapsed;
        active.readBytes += bytes;
        break;
      case Io::Write:
        ++active.writes;
        active.writeUs += elapsed;
        active.writeBytes += bytes;
        break;
    }
  }
  taskEXIT_CRITICAL(&mux);
}

void finish(uint32_t operation, const void* owner) {
  if (!operation) return;
  Measurement result;
  const auto ended = now();
  taskENTER_CRITICAL(&mux);
  const bool matching = active.id == operation && active.owner == owner;
  if (matching) {
    result = active;
    active = {};
  }
  taskEXIT_CRITICAL(&mux);
  if (!matching) return;
  static constexpr const char* COUNTERS[] = {
      "layout_hit",       "layout_miss",        "page_prefetch_hit", "page_prefetch_miss",
      "image_ram_hit",    "image_disk_hit",     "image_decode",      "event_memo_hit",
      "event_memo_miss",  "cover_source_reuse", "cover_source_read", "cover_scene_hit",
      "cover_scene_miss", "page_reuse_hit",     "page_reuse_miss",   "cover_pack_hit"};
  static_assert(sizeof(COUNTERS) / sizeof(*COUNTERS) == size_t(Counter::Count));
  for (size_t i = 0; i < size_t(Counter::Count); ++i) {
    if (result.counters[i])
      LOG_INF("TRACE", "counter id=%lu name=%s value=%lu", (unsigned long)result.id, COUNTERS[i],
              (unsigned long)result.counters[i]);
  }
  LOG_INF("TRACE", "operation id=%lu kind=%s ts=%llu dur=%llu", (unsigned long)result.id, operationName(result.kind),
          (unsigned long long)result.start, (unsigned long long)(ended - result.start));
  LOG_INF(
      "TRACE",
      "storage id=%lu reads=%lu read_bytes=%llu read_us=%llu writes=%lu write_bytes=%llu write_us=%llu wait_us=%llu",
      (unsigned long)result.id, (unsigned long)result.reads, (unsigned long long)result.readBytes,
      (unsigned long long)result.readUs, (unsigned long)result.writes, (unsigned long long)result.writeBytes,
      (unsigned long long)result.writeUs, (unsigned long long)result.waitUs);
}
}  // namespace performance
#endif
