#include "SceneRunner.h"

#include <utility>

namespace dmd {

uint32_t SceneRunner::allowance(uint32_t wantedMs) const {
    if (_queue.empty() && !(_current && _animating) && !_holding) return wantedMs;  // nothing ahead
    uint32_t allowed = _queuedMs >= kMaxQueuedMs ? 0 : kMaxQueuedMs - _queuedMs;
    if (allowed < kMinHoldMs) allowed = kMinHoldMs;
    return wantedMs > allowed ? allowed : wantedMs;
}

void SceneRunner::enqueue(std::unique_ptr<Scene> scene, uint32_t holdMs) {
    if (!scene) {
        return;
    }
    if (_queue.size() >= kMaxQueued) {
        ++_dropped;
        return;
    }
    uint32_t runMs = scene->durationMs();
    if (runMs > 0) {
        const uint32_t allowed = allowance(runMs);
        if (allowed < runMs) {
            scene->limitDuration(allowed);
            runMs = scene->durationMs();
        }
    }
    _queuedMs += runMs;
    Entry entry;
    entry.scene = std::move(scene);
    entry.holdMs = allowance(holdMs);
    entry.queuedMs = runMs + entry.holdMs;
    _queuedMs += entry.holdMs;
    _queue.push_back(std::move(entry));
}

void SceneRunner::interrupt() {
    if (_current && _animating) {
        _abortRequested = true;
    }
}

void SceneRunner::reset() {
    _queue.clear();
    _queuedMs = 0;
    if (_current && _animating) {
        _current->abort();
    }
    _current.reset();
    _animating = false;
    _abortRequested = false;
    _holding = false;
}

void SceneRunner::finishCurrent(uint32_t nowMs) {
    _animating = false;
    _abortRequested = false;
    _holding = _currentHoldMs > 0;
    _holdUntil = nowMs + _currentHoldMs;
}

void SceneRunner::update(uint32_t nowMs) {
    if (_current && _animating) {
        if (_abortRequested) {
            _current->abort();
            finishCurrent(nowMs);
        } else if (!_current->tick(nowMs)) {
            finishCurrent(nowMs);
        }
    }

    if (_holding && reached(nowMs, _holdUntil)) {
        _holding = false;
    }

    // Start queued scenes once the previous one is done and its hold has elapsed.
    // A scene that finishes on its first frame (static text with no hold) lets the next one start
    // in the same update, which matches the original handling a burst of messages back to back.
    while (!_holding && !(_current && _animating) && !_queue.empty()) {
        Entry next = std::move(_queue.front());
        _queue.pop_front();
        _queuedMs -= next.queuedMs;
        _current = std::move(next.scene);
        _currentHoldMs = next.holdMs;
        _animating = true;
        _abortRequested = false;
        _current->start(nowMs);
        if (!_current->tick(nowMs)) {
            finishCurrent(nowMs);
        }
    }
}

bool SceneRunner::idle(uint32_t nowMs) const {
    if (_current && _animating) return false;
    if (_holding && !reached(nowMs, _holdUntil)) return false;
    return _queue.empty();
}

}  // namespace dmd
