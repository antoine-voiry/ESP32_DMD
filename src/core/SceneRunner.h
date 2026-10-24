#ifndef DMD_CORE_SCENE_RUNNER_H
#define DMD_CORE_SCENE_RUNNER_H

// Non-blocking replacement for the Raspy2DMD threads + _stopAffichage flags.
//
// Original behaviour being reproduced (ServerRaspy2DMD.py):
//  - A valid incoming message calls Stop(): whatever is animating aborts.
//  - Messages are rendered one at a time, in arrival order.
//  - "msg|Hello|3": after rendering, the dispatcher sleeps 3 s before taking the next message.
//    That hold is not interrupted by new messages; they wait in the queue.
//
// Everything here is driven by update(nowMs) from loop(), so MQTT and the web server keep running.

#include <cstdint>
#include <deque>
#include <memory>

namespace dmd {

class Scene {
public:
    virtual ~Scene() = default;
    // Draw the first frame.
    virtual void start(uint32_t nowMs) = 0;
    // Advance the animation. Return false once the animation is over (the last frame stays shown).
    virtual bool tick(uint32_t nowMs) = 0;
    // Called when the animation is cut short by a newer message.
    virtual void abort() {}
    // Run time for scenes that run for a set time (effects, the clock), 0 for the others.
    virtual uint32_t durationMs() const { return 0; }
    // Shortens such a scene to about ms (SceneRunner's queue time budget).
    virtual void limitDuration(uint32_t ms) { (void)ms; }
};

class SceneRunner {
public:
    // A scene queued while this many are waiting is refused (and counted in dropped()): holds are
    // not interrupted, so a burst of "msg|x|60" would otherwise grow the queue until the heap runs
    // out. Refusing the newest keeps what is already queued whole and in order.
    static constexpr size_t kMaxQueued = 128;
    // The time waiting in the queue (holds, plus the run time of timed scenes such as effects)
    // adds up to about this at most: beyond it, a scene keeps only kMinHoldMs of its hold or run
    // time. Without it, 128 queued "msg|x|600" or "fx|plasma|600" would take the display for 21
    // hours; with it, what is queued takes at most kMaxQueuedMs + kMaxQueued * 2 * kMinHoldMs
    // (about 31 minutes), after the scene playing now and its hold (which had nothing ahead of
    // them, so kept their full time: 10 min each at most, see MessageHandler::kMaxHoldSec). Scenes
    // without a set run time (text, GIFs, weather pages) are bounded where they are made.
    static constexpr uint32_t kMaxQueuedMs = 10u * 60u * 1000u;
    static constexpr uint32_t kMinHoldMs = 5000;

    // How much of wantedMs a scene queued now may take: all of it when nothing is ahead of it,
    // else what is left of kMaxQueuedMs (at least kMinHoldMs, or wantedMs if less).
    uint32_t allowance(uint32_t wantedMs) const;

    // Queues a scene. holdMs is the minimum time to wait after it finishes (or is aborted)
    // before the next queued scene may start. The hold and the scene's durationMs() both count
    // towards kMaxQueuedMs and are shortened to allowance().
    void enqueue(std::unique_ptr<Scene> scene, uint32_t holdMs = 0);

    // Port of DMDRenderer.Stop(): aborts the scene currently animating. Its hold still applies.
    void interrupt();

    // Drops everything queued and aborts the current scene, ignoring holds (used by reboot/reload).
    void reset();

    void update(uint32_t nowMs);

    // True when nothing is animating, holding, or queued (the attract mode may take over).
    bool idle(uint32_t nowMs) const;
    bool animating() const { return _current && _animating; }
    size_t queued() const { return _queue.size(); }
    size_t dropped() const { return _dropped; }
    uint32_t queuedMs() const { return _queuedMs; }

private:
    struct Entry {
        std::unique_ptr<Scene> scene;
        uint32_t holdMs = 0;
        uint32_t queuedMs = 0;  // what it counts towards kMaxQueuedMs
    };

    void finishCurrent(uint32_t nowMs);
    static bool reached(uint32_t now, uint32_t deadline) {
        return static_cast<int32_t>(now - deadline) >= 0;
    }

    std::deque<Entry> _queue;
    std::unique_ptr<Scene> _current;
    uint32_t _currentHoldMs = 0;
    bool _animating = false;
    bool _abortRequested = false;
    bool _holding = false;
    uint32_t _holdUntil = 0;
    size_t _dropped = 0;
    uint32_t _queuedMs = 0;
};

}  // namespace dmd

#endif
