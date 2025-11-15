# Architecture Quick Reference Guide

## Recommended Architecture: Command Pattern with Message Queue

### Core Principles

1. **Worker owns state** - Single source of truth
2. **Single-threaded worker** - Eliminates race conditions
3. **Queue-based communication** - Natural serialization
4. **Async results** - Futures/callbacks for responsiveness
5. **Clean architecture** - Core is UI-agnostic

---

## Architecture Diagram

```
┌─────────────┐      ┌──────────────┐      ┌─────────────┐
│   Qt/QML    │─────▶│   Command    │─────▶│   Worker    │
│    UI       │      │    Queue     │      │   Thread    │
└─────────────┘      └──────────────┘      └─────────────┘
┌─────────────┐           │                      │
│   Python    │──────────┘                      │
│  Scripts    │                                  │
└─────────────┘                                  │
                                                 ▼
                                         ┌──────────────┐
                                         │   Results    │
                                         │   Callbacks  │
                                         └──────────────┘
```

---

## Key Components

### 1. Core Interface (`IWorker`)
```cpp
class IWorker {
public:
    virtual std::future<Result> executeCommand(std::unique_ptr<Command> cmd) = 0;
    virtual void subscribeToStateChanges(StateChangeCallback cb) = 0;
    virtual State getCurrentState() const = 0;
};
```

### 2. Worker Implementation
- Single-threaded execution
- Command queue with mutex
- State management
- Subscriber notifications

### 3. Commands
- Inherit from `Command` base class
- Execute in worker thread
- Return `Result` with new state

### 4. Qt Bridge
- Wraps `IWorker`
- Converts Qt signals to commands
- Converts callbacks to Qt signals
- Runs on main thread

### 5. Python Bindings (pybind11)
- Exposes `IWorker` to Python
- Commands can be called from Python
- State changes via callbacks

---

## Threading Model

| Thread | Purpose | Responsibilities |
|--------|---------|------------------|
| **Main Thread** | Qt event loop | UI updates, user input |
| **Worker Thread** | Command execution | Business logic, state management |
| **Python Thread** | Script execution | Python script execution (if needed) |

### Communication Flow

1. **UI/Python → Worker**: Commands via queue (async)
2. **Worker → UI/Python**: Callbacks/signals (async, queued)

---

## Why This Architecture?

### ✅ Performance
- Queue operations are fast (O(1) enqueue/dequeue)
- No locking during command execution
- Efficient state management

### ✅ Thread Safety
- Single-threaded worker eliminates race conditions
- Queue operations protected by mutex
- Callbacks are thread-safe

### ✅ Debuggability
- Sequential execution = clear call stack
- Command pattern = easy to trace
- State changes are logged

### ✅ Maintainability
- Clear separation of concerns
- Core is UI-agnostic
- Easy to test independently

### ✅ Concurrent Access
- Multiple threads can submit commands
- Queue naturally serializes execution
- No complex synchronization needed

---

## Implementation Checklist

### Phase 1: Core Worker
- [ ] Define `IWorker` interface
- [ ] Implement `Worker` with command queue
- [ ] Create `Command` base class
- [ ] Implement example commands
- [ ] Add state management
- [ ] Add subscriber notifications

### Phase 2: Qt Integration
- [ ] Create `QtWorkerBridge`
- [ ] Convert Qt signals to commands
- [ ] Convert callbacks to Qt signals
- [ ] Test with QML

### Phase 3: Python Integration
- [ ] Create pybind11 bindings
- [ ] Expose `IWorker` to Python
- [ ] Test Python scripts
- [ ] Add Python examples

### Phase 4: Advanced Features
- [ ] Add command priority
- [ ] Implement undo/redo
- [ ] Add command history
- [ ] Optimize performance

---

## Common Patterns

### Submitting a Command
```cpp
// From UI or Python
auto future = worker->executeCommand(std::make_unique<MyCommand>(args));
future.then([](Result result) {
    // Handle result
});
```

### Subscribing to State Changes
```cpp
worker->subscribeToStateChanges([](const State& state) {
    // Update UI or Python state
});
```

### Getting Current State
```cpp
State current = worker->getCurrentState();
```

---

## "UI as Slave" vs "UI as Peer"

### Recommendation: **Hybrid Approach**

- ✅ **Worker owns authoritative state** (UI is slave for state)
- ✅ **UI can have local/temporary state** (for responsiveness)
- ✅ **Worker validates all operations** (prevents corruption)
- ✅ **Worker emits state changes** (UI syncs to worker)
- ✅ **UI can show optimistic updates** (but syncs to worker result)

### Why This Works

1. **Single source of truth** - Worker owns state
2. **Responsive UI** - Optimistic updates
3. **State integrity** - Worker validates
4. **Easy debugging** - Clear state flow
5. **Unified pattern** - Same for UI and Python

---

## Performance Tips

1. **Use move semantics** - Avoid copying commands
2. **Batch operations** - Group multiple commands
3. **Priority queue** - For urgent commands
4. **Lock-free queue** - For high-throughput (optional)
5. **Debounce updates** - Throttle frequent state changes

---

## Debugging Tips

1. **Log commands** - Track what's being executed
2. **Log state changes** - See state evolution
3. **Command history** - Replay for debugging
4. **State snapshots** - Inspect state at any point
5. **Breakpoints** - Set in worker thread

---

## Testing Strategy

1. **Unit tests** - Test worker logic independently
2. **Integration tests** - Test UI/Worker interaction
3. **Python tests** - Test Python bindings
4. **Mock IWorker** - For UI tests

---

## References

- **Full Analysis**: See `ARCHITECTURE_ANALYSIS.md`
- **Example Code**: See `ARCHITECTURE_EXAMPLE.cpp`
- **Clean Architecture**: Robert C. Martin
- **Command Pattern**: Gang of Four
- **STLab Library**: Your codebase (channels, futures)

---

## Similar Projects

Projects using similar architecture:
- **Blender** - Command queue with Python scripting
- **Maya** - Command-based system
- **Qt Creator** - Core separated from UI
- **GIMP** - Core library (GEGL) separate from UI
- **Krita** - Command pattern with Python support

---

## Questions to Consider

1. **Do you need undo/redo?** → Command pattern supports this naturally
2. **Do you need distributed processing?** → Consider actor model
3. **Do you need complex data transformations?** → Consider reactive streams
4. **Do you need high throughput?** → Consider lock-free queue
5. **Do you need priority?** → Use priority queue

---

## Next Steps

1. Review `ARCHITECTURE_ANALYSIS.md` for detailed analysis
2. Review `ARCHITECTURE_EXAMPLE.cpp` for implementation example
3. Prototype core worker with command queue
4. Test with simple commands
5. Integrate Qt/QML
6. Add Python bindings
7. Iterate and refine

---

## Summary

**Recommended Architecture**: Command Pattern with Message Queue

**Key Benefits**:
- ✅ Performance (efficient queue operations)
- ✅ Thread safety (single-threaded worker)
- ✅ Debuggability (clear call stack)
- ✅ Maintainability (clean separation)
- ✅ Concurrent access (natural serialization)

**Implementation**: See `ARCHITECTURE_EXAMPLE.cpp` for working example.
