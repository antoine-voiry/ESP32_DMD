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
};

class SceneRunner {
public:
    // Queues a scene. holdMs is the minimum time to wait after it finishes (or is aborted)
    // before the next queued scene may start.
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

private:
    struct Entry {
        std::unique_ptr<Scene> scene;
        uint32_t holdMs = 0;
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
};

}  // namespace dmd

#endif
