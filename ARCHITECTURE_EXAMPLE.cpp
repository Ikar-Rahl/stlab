// Example Implementation: Command Pattern with Message Queue Architecture
// This demonstrates the recommended architecture for C++ core with Qt/QML UI and Python plugins

#include <functional>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include <string>
#include <iostream>

// ============================================================================
// CORE INTERFACE (UI-agnostic)
// ============================================================================

// Forward declarations
struct State;
struct Result;
struct Command;

// State change callback type
using StateChangeCallback = std::function<void(const State&)>;

// Core worker interface - UI and Python both depend on this
class IWorker {
public:
    virtual ~IWorker() = default;
    virtual std::future<Result> executeCommand(std::unique_ptr<Command> cmd) = 0;
    virtual void subscribeToStateChanges(StateChangeCallback cb) = 0;
    virtual State getCurrentState() const = 0;
};

// ============================================================================
// CORE MODELS
// ============================================================================

struct State {
    int counter = 0;
    std::string last_operation;
    
    bool operator==(const State& other) const {
        return counter == other.counter && last_operation == other.last_operation;
    }
};

struct Result {
    bool success;
    std::string message;
    State new_state;
};

// ============================================================================
// COMMAND INTERFACE
// ============================================================================

class Command {
public:
    virtual ~Command() = default;
    virtual Result execute(State& current_state) = 0;
    virtual std::string getName() const = 0;
};

// Example command: Increment counter
class IncrementCommand : public Command {
    int amount_;
public:
    IncrementCommand(int amount = 1) : amount_(amount) {}
    
    Result execute(State& current_state) override {
        current_state.counter += amount_;
        current_state.last_operation = "increment";
        
        Result result;
        result.success = true;
        result.message = "Incremented by " + std::to_string(amount_);
        result.new_state = current_state;
        return result;
    }
    
    std::string getName() const override {
        return "IncrementCommand";
    }
};

// Example command: Reset counter
class ResetCommand : public Command {
public:
    Result execute(State& current_state) override {
        current_state.counter = 0;
        current_state.last_operation = "reset";
        
        Result result;
        result.success = true;
        result.message = "Reset counter";
        result.new_state = current_state;
        return result;
    }
    
    std::string getName() const override {
        return "ResetCommand";
    }
};

// ============================================================================
// WORKER IMPLEMENTATION (Single-threaded, queue-based)
// ============================================================================

class Worker : public IWorker {
private:
    // Thread management
    std::thread worker_thread_;
    std::atomic<bool> running_{true};
    
    // Command queue
    std::queue<std::pair<std::unique_ptr<Command>, std::shared_ptr<std::promise<Result>>>> command_queue_;
    std::mutex queue_mutex_;
    std::condition_variable cv_;
    
    // State management
    mutable std::mutex state_mutex_;
    State current_state_;
    
    // Subscribers
    std::vector<StateChangeCallback> callbacks_;
    std::mutex callbacks_mutex_;
    
public:
    Worker() {
        worker_thread_ = std::thread(&Worker::run, this);
    }
    
    ~Worker() {
        stop();
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }
    
    // Non-copyable
    Worker(const Worker&) = delete;
    Worker& operator=(const Worker&) = delete;
    
    std::future<Result> executeCommand(std::unique_ptr<Command> cmd) override {
        auto promise = std::make_shared<std::promise<Result>>();
        auto future = promise->get_future();
        
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            command_queue_.push({std::move(cmd), promise});
        }
        cv_.notify_one();
        return future;
    }
    
    void subscribeToStateChanges(StateChangeCallback cb) override {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        callbacks_.push_back(cb);
    }
    
    State getCurrentState() const override {
        std::lock_guard<std::mutex> lock(state_mutex_);
        return current_state_;
    }
    
    void stop() {
        running_ = false;
        cv_.notify_all();
    }
    
private:
    void run() {
        while (running_) {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            cv_.wait(lock, [this] { 
                return !command_queue_.empty() || !running_; 
            });
            
            while (!command_queue_.empty() && running_) {
                auto [cmd, promise] = std::move(command_queue_.front());
                command_queue_.pop();
                lock.unlock();
                
                // Execute command in worker thread
                try {
                    {
                        std::lock_guard<std::mutex> state_lock(state_mutex_);
                        Result result = cmd->execute(current_state_);
                        promise->set_value(result);
                        
                        // Notify subscribers
                        notifyStateChange(current_state_);
                    }
                } catch (const std::exception& e) {
                    Result error_result;
                    error_result.success = false;
                    error_result.message = std::string("Error: ") + e.what();
                    promise->set_value(error_result);
                } catch (...) {
                    Result error_result;
                    error_result.success = false;
                    error_result.message = "Unknown error";
                    promise->set_value(error_result);
                }
                
                lock.lock();
            }
        }
    }
    
    void notifyStateChange(const State& state) {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        for (auto& cb : callbacks_) {
            cb(state);
        }
    }
};

// ============================================================================
// QT/QML BRIDGE (Example - would use QObject in real implementation)
// ============================================================================

class QtWorkerBridge {
private:
    std::unique_ptr<IWorker> worker_;
    
public:
    QtWorkerBridge(std::unique_ptr<IWorker> worker) 
        : worker_(std::move(worker)) {
        // Subscribe to state changes
        worker_->subscribeToStateChanges([this](const State& state) {
            // In real Qt code, this would emit a signal
            // QMetaObject::invokeMethod(this, "onStateChanged", 
            //     Qt::QueuedConnection, Q_ARG(State, state));
            onStateChanged(state);
        });
    }
    
    void executeCommand(const std::string& command_name, int value = 0) {
        std::unique_ptr<Command> cmd;
        if (command_name == "increment") {
            cmd = std::make_unique<IncrementCommand>(value);
        } else if (command_name == "reset") {
            cmd = std::make_unique<ResetCommand>();
        }
        
        if (cmd) {
            worker_->executeCommand(std::move(cmd))
                .then([this](std::future<Result> fut) {
                    try {
                        Result result = fut.get();
                        // In real Qt code, this would emit a signal
                        // emit commandResult(result);
                        onCommandResult(result);
                    } catch (...) {
                        // Handle error
                    }
                });
        }
    }
    
    // In real Qt code, these would be slots
    void onStateChanged(const State& state) {
        std::cout << "[Qt Bridge] State changed: counter=" << state.counter 
                  << ", operation=" << state.last_operation << std::endl;
    }
    
    void onCommandResult(const Result& result) {
        std::cout << "[Qt Bridge] Command result: " << result.message << std::endl;
    }
    
    int getCounter() const {
        return worker_->getCurrentState().counter;
    }
};

// ============================================================================
// PYTHON BINDING EXAMPLE (Would use pybind11 in real implementation)
// ============================================================================

// In real implementation, this would be:
/*
#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>

PYBIND11_MODULE(worker_module, m) {
    py::class_<State>(m, "State")
        .def_readwrite("counter", &State::counter)
        .def_readwrite("last_operation", &State::last_operation);
    
    py::class_<Result>(m, "Result")
        .def_readwrite("success", &Result::success)
        .def_readwrite("message", &Result::message)
        .def_readwrite("new_state", &Result::new_state);
    
    py::class_<IWorker, std::shared_ptr<IWorker>>(m, "Worker")
        .def("execute_command", [](IWorker& w, const std::string& cmd_name, int value) {
            std::unique_ptr<Command> cmd;
            if (cmd_name == "increment") {
                cmd = std::make_unique<IncrementCommand>(value);
            } else if (cmd_name == "reset") {
                cmd = std::make_unique<ResetCommand>();
            }
            if (cmd) {
                return w.executeCommand(std::move(cmd));
            }
            throw std::runtime_error("Unknown command");
        })
        .def("subscribe_to_state_changes", [](IWorker& w, py::function callback) {
            w.subscribeToStateChanges([callback](const State& state) {
                callback(state);
            });
        })
        .def("get_current_state", &IWorker::getCurrentState);
    
    m.def("create_worker", []() {
        return std::make_shared<Worker>();
    });
}
*/

// ============================================================================
// USAGE EXAMPLE
// ============================================================================

int main() {
    std::cout << "=== Architecture Example ===" << std::endl;
    
    // Create worker
    auto worker = std::make_unique<Worker>();
    
    // Create Qt bridge
    QtWorkerBridge qt_bridge(std::move(worker));
    
    // Simulate Qt UI calls
    std::cout << "\n--- Qt UI calls ---" << std::endl;
    qt_bridge.executeCommand("increment", 5);
    qt_bridge.executeCommand("increment", 3);
    qt_bridge.executeCommand("reset");
    
    // Wait a bit for commands to execute
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::cout << "\nFinal counter value: " << qt_bridge.getCounter() << std::endl;
    
    // Simulate Python script calls (would use pybind11 in real implementation)
    std::cout << "\n--- Python script calls (simulated) ---" << std::endl;
    // In Python:
    // import worker_module
    // worker = worker_module.create_worker()
    // future = worker.execute_command("increment", 10)
    // result = future.get()  # Wait for result
    // print(f"Result: {result.message}")
    
    std::cout << "\n=== Example Complete ===" << std::endl;
    
    return 0;
}

// ============================================================================
// KEY ARCHITECTURAL POINTS DEMONSTRATED:
// ============================================================================
//
// 1. Core Interface (IWorker) is UI-agnostic
//    - No Qt dependencies
//    - No Python dependencies
//    - Can be used by any frontend
//
// 2. Single-threaded Worker
//    - Commands execute sequentially in worker thread
//    - No race conditions
//    - Clear call stack for debugging
//
// 3. Queue-based Communication
//    - Commands queued from any thread
//    - Natural serialization
//    - Thread-safe queue operations
//
// 4. Async Results
//    - Futures for async result delivery
//    - Callbacks for state changes
//    - Non-blocking UI
//
// 5. Clean Separation
//    - Core: Business logic
//    - Qt Bridge: UI-specific code
//    - Python Bindings: Scripting interface
//
// 6. Concurrent Access
//    - Multiple threads can submit commands
//    - Queue serializes execution
//    - Worker thread is single-threaded
//
// ============================================================================
