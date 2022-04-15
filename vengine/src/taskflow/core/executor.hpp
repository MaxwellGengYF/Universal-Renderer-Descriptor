#pragma once

#include "observer.hpp"
#include "taskflow.hpp"

/**
@file executor.hpp
@brief executor include file
*/

namespace tf {

// ----------------------------------------------------------------------------
// Executor Definition
// ----------------------------------------------------------------------------

/** @class Executor

@brief class to create an executor for running a taskflow graph

An executor manages a set of worker threads to run one or multiple taskflows
using an efficient work-stealing scheduling algorithm.

@code{.cpp}
// Declare an executor and a taskflow
tf::Executor executor;
tf::Taskflow taskflow;

// Add three tasks into the taskflow
tf::Task A = taskflow.emplace([] () { std::cout << "This is TaskA\n"; });
tf::Task B = taskflow.emplace([] () { std::cout << "This is TaskB\n"; });
tf::Task C = taskflow.emplace([] () { std::cout << "This is TaskC\n"; });

// Build precedence between tasks
A.precede(B, C);

tf::Future<void> fu = executor.run(taskflow);
fu.wait();                // block until the execution completes

executor.run(taskflow, [](){ std::cout << "end of 1 run"; }).wait();
executor.run_n(taskflow, 4);
executor.wait_for_all();  // block until all associated executions finish
executor.run_n(taskflow, 4, [](){ std::cout << "end of 4 runs"; }).wait();
executor.run_until(taskflow, [cnt=0] () mutable { return ++cnt == 10; });
@endcode

All the @c run methods are @em thread-safe. You can submit multiple
taskflows at the same time to an executor from different threads.
*/
class TF_API Executor {

  friend class FlowBuilder;
  friend class Subflow;
  friend class Runtime;

  public:

    /**
    @brief constructs the executor with @c N worker threads

    The constructor spawns @c N worker threads to run tasks in a
    work-stealing loop. The number of workers must be greater than zero
    or an exception will be thrown.
    By default, the number of worker threads is equal to the maximum
    hardware concurrency returned by std::thread::hardware_concurrency.
    */
    explicit Executor(size_t N = std::thread::hardware_concurrency());

    /**
    @brief destructs the executor

    The destructor calls Executor::wait_for_all to wait for all submitted
    taskflows to complete and then notifies all worker threads to stop
    and join these threads.
    */
    ~Executor();

    /**
    @brief runs a taskflow once

    @param taskflow a tf::Taskflow object

    @return a tf::Future that holds the result of the execution

    This member function executes the given taskflow once and returns a tf::Future
    object that eventually holds the result of the execution.

    @code{.cpp}
    tf::Future<void> future = executor.run(taskflow);
    // do something else
    future.wait();
    @endcode

    This member function is thread-safe.

    @attention
    The executor does not own the given taskflow. It is your responsibility to
    ensure the taskflow remains alive during its execution.
    */
    tf::Future<void> run(Taskflow& taskflow);

    /**
    @brief runs a moved taskflow once

    @param taskflow a moved tf::Taskflow object

    @return a tf::Future that holds the result of the execution

    This member function executes a moved taskflow once and returns a tf::Future
    object that eventually holds the result of the execution.
    The executor will take care of the lifetime of the moved taskflow.

    @code{.cpp}
    tf::Future<void> future = executor.run(std::move(taskflow));
    // do something else
    future.wait();
    @endcode

    This member function is thread-safe.
    */
    tf::Future<void> run(Taskflow&& taskflow);

    /**
    @brief runs a taskflow once and invoke a callback upon completion

    @param taskflow a tf::Taskflow object
    @param callable a callable object to be invoked after this run

    @return a tf::Future that holds the result of the execution

    This member function executes the given taskflow once and invokes the given
    callable when the execution completes.
    This member function returns a tf::Future object that
    eventually holds the result of the execution.

    @code{.cpp}
    tf::Future<void> future = executor.run(taskflow, [](){ std::cout << "done"; });
    // do something else
    future.wait();
    @endcode

    This member function is thread-safe.

    @attention
    The executor does not own the given taskflow. It is your responsibility to
    ensure the taskflow remains alive during its execution.
    */
    template<typename C>
    tf::Future<void> run(Taskflow& taskflow, C&& callable);

    /**
    @brief runs a moved taskflow once and invoke a callback upon completion

    @param taskflow a moved tf::Taskflow object
    @param callable a callable object to be invoked after this run

    @return a tf::Future that holds the result of the execution

    This member function executes a moved taskflow once and invokes the given
    callable when the execution completes.
    This member function returns a tf::Future object that
    eventually holds the result of the execution.
    The executor will take care of the lifetime of the moved taskflow.

    @code{.cpp}
    tf::Future<void> future = executor.run(
      std::move(taskflow), [](){ std::cout << "done"; }
    );
    // do something else
    future.wait();
    @endcode

    This member function is thread-safe.
    */
    template<typename C>
    tf::Future<void> run(Taskflow&& taskflow, C&& callable);

    /**
    @brief runs a taskflow for @c N times

    @param taskflow a tf::Taskflow object
    @param N number of runs

    @return a tf::Future that holds the result of the execution

    This member function executes the given taskflow @c N times and returns a tf::Future
    object that eventually holds the result of the execution.

    @code{.cpp}
    tf::Future<void> future = executor.run_n(taskflow, 2);  // run taskflow 2 times
    // do something else
    future.wait();
    @endcode

    This member function is thread-safe.

    @attention
    The executor does not own the given taskflow. It is your responsibility to
    ensure the taskflow remains alive during its execution.
    */
    tf::Future<void> run_n(Taskflow& taskflow, size_t N);

    /**
    @brief runs a moved taskflow for @c N times

    @param taskflow a moved tf::Taskflow object
    @param N number of runs

    @return a tf::Future that holds the result of the execution

    This member function executes a moved taskflow @c N times and returns a tf::Future
    object that eventually holds the result of the execution.
    The executor will take care of the lifetime of the moved taskflow.

    @code{.cpp}
    tf::Future<void> future = executor.run_n(
      std::move(taskflow), 2    // run the moved taskflow 2 times
    );
    // do something else
    future.wait();
    @endcode

    This member function is thread-safe.
    */
    tf::Future<void> run_n(Taskflow&& taskflow, size_t N);

    /**
    @brief runs a taskflow for @c N times and then invokes a callback

    @param taskflow a tf::Taskflow
    @param N number of runs
    @param callable a callable object to be invoked after this run

    @return a tf::Future that holds the result of the execution

    This member function executes the given taskflow @c N times and invokes the given
    callable when the execution completes.
    This member function returns a tf::Future object that
    eventually holds the result of the execution.

    @code{.cpp}
    tf::Future<void> future = executor.run(
      taskflow, 2, [](){ std::cout << "done"; }  // runs taskflow 2 times and invoke
                                                 // the lambda to print "done"
    );
    // do something else
    future.wait();
    @endcode

    This member function is thread-safe.

    @attention
    The executor does not own the given taskflow. It is your responsibility to
    ensure the taskflow remains alive during its execution.
    */
    template<typename C>
    tf::Future<void> run_n(Taskflow& taskflow, size_t N, C&& callable);

    /**
    @brief runs a moved taskflow for @c N times and then invokes a callback

    @param taskflow a moved tf::Taskflow
    @param N number of runs
    @param callable a callable object to be invoked after this run

    @return a tf::Future that holds the result of the execution

    This member function executes a moved taskflow @c N times and invokes the given
    callable when the execution completes.
    This member function returns a tf::Future object that
    eventually holds the result of the execution.

    @code{.cpp}
    tf::Future<void> future = executor.run(
      // run the moved taskflow 2 times and invoke the lambda to print "done"
      std::move(taskflow), 2, [](){ std::cout << "done"; }
    );
    // do something else
    future.wait();
    @endcode

    This member function is thread-safe.
    */
    template<typename C>
    tf::Future<void> run_n(Taskflow&& taskflow, size_t N, C&& callable);

    /**
    @brief runs a taskflow multiple times until the predicate becomes true

    @param taskflow a tf::Taskflow
    @param pred a boolean predicate to return @c true for stop

    @return a tf::Future that holds the result of the execution

    This member function executes the given taskflow multiple times until
    the predicate returns @c true.
    This member function returns a tf::Future object that
    eventually holds the result of the execution.

    @code{.cpp}
    tf::Future<void> future = executor.run(
      taskflow, [](){ return rand()%10 == 0 }
    );
    // do something else
    future.wait();
    @endcode

    This member function is thread-safe.

    @attention
    The executor does not own the given taskflow. It is your responsibility to
    ensure the taskflow remains alive during its execution.
    */
    template<typename P>
    tf::Future<void> run_until(Taskflow& taskflow, P&& pred);

    /**
    @brief runs a moved taskflow and keeps running it
           until the predicate becomes true

    @param taskflow a moved tf::Taskflow object
    @param pred a boolean predicate to return @c true for stop

    @return a tf::Future that holds the result of the execution

    This member function executes a moved taskflow multiple times until
    the predicate returns @c true.
    This member function returns a tf::Future object that
    eventually holds the result of the execution.
    The executor will take care of the lifetime of the moved taskflow.

    @code{.cpp}
    tf::Future<void> future = executor.run(
      std::move(taskflow), [](){ return rand()%10 == 0 }
    );
    // do something else
    future.wait();
    @endcode

    This member function is thread-safe.
    */
    template<typename P>
    tf::Future<void> run_until(Taskflow&& taskflow, P&& pred);

    /**
    @brief runs a taskflow multiple times until the predicate becomes true and
           then invokes the callback

    @param taskflow a tf::Taskflow
    @param pred a boolean predicate to return @c true for stop
    @param callable a callable object to be invoked after this run completes

    @return a tf::Future that holds the result of the execution

    This member function executes the given taskflow multiple times until
    the predicate returns @c true and then invokes the given callable when
    the execution completes.
    This member function returns a tf::Future object that
    eventually holds the result of the execution.

    @code{.cpp}
    tf::Future<void> future = executor.run(
      taskflow, [](){ return rand()%10 == 0 }, [](){ std::cout << "done"; }
    );
    // do something else
    future.wait();
    @endcode

    This member function is thread-safe.

    @attention
    The executor does not own the given taskflow. It is your responsibility to
    ensure the taskflow remains alive during its execution.
    */
    template<typename P, typename C>
    tf::Future<void> run_until(Taskflow& taskflow, P&& pred, C&& callable);

    /**
    @brief runs a moved taskflow and keeps running
           it until the predicate becomes true and then invokes the callback

    @param taskflow a moved tf::Taskflow
    @param pred a boolean predicate to return @c true for stop
    @param callable a callable object to be invoked after this run completes

    @return a tf::Future that holds the result of the execution

    This member function executes a moved taskflow multiple times until
    the predicate returns @c true and then invokes the given callable when
    the execution completes.
    This member function returns a tf::Future object that
    eventually holds the result of the execution.
    The executor will take care of the lifetime of the moved taskflow.

    @code{.cpp}
    tf::Future<void> future = executor.run(
      std::move(taskflow),
      [](){ return rand()%10 == 0 }, [](){ std::cout << "done"; }
    );
    // do something else
    future.wait();
    @endcode

    This member function is thread-safe.
    */
    template<typename P, typename C>
    tf::Future<void> run_until(Taskflow&& taskflow, P&& pred, C&& callable);

    /**
    @brief wait for all tasks to complete

    This member function waits until all submitted tasks
    (e.g., taskflows, asynchronous tasks) to finish.

    @code{.cpp}
    executor.run(taskflow1);
    executor.run_n(taskflow2, 10);
    executor.run_n(taskflow3, 100);
    executor.wait_for_all();  // wait until the above submitted taskflows finish
    @endcode
    */
    void wait_for_all();

    /**
    @brief queries the number of worker threads

    Each worker represents one unique thread spawned by an executor
    upon its construction time.

    @code{.cpp}
    tf::Executor executor(4);
    std::cout << executor.num_workers();    // 4
    @endcode
    */
    size_t num_workers() const noexcept;

    /**
    @brief queries the number of running topologies at the time of this call

    When a taskflow is submitted to an executor, a topology is created to store
    runtime metadata of the running taskflow.
    When the execution of the submitted taskflow finishes,
    its corresponding topology will be removed from the executor.

    @code{.cpp}
    executor.run(taskflow);
    std::cout << executor.num_topologies();  // 0 or 1 (taskflow still running)
    @endcode
    */
    size_t num_topologies() const;

    /**
    @brief queries the number of running taskflows with moved ownership

    @code{.cpp}
    executor.run(std::move(taskflow));
    std::cout << executor.num_taskflows();  // 0 or 1 (taskflow still running)
    @endcode
    */
    size_t num_taskflows() const;

    /**
    @brief queries the id of the caller thread in this executor

    Each worker has an unique id in the range of @c 0 to @c N-1 associated with
    its parent executor.
    If the caller thread does not belong to the executor, @c -1 is returned.

    @code{.cpp}
    tf::Executor executor(4);   // 4 workers in the executor
    executor.this_worker_id();  // -1 (main thread is not a worker)

    taskflow.emplace([&](){
      std::cout << executor.this_worker_id();  // 0, 1, 2, or 3
    });
    executor.run(taskflow);
    @endcode
    */
    int this_worker_id() const;

    /**
    @brief runs a given function asynchronously

    @tparam F callable type
    @tparam ArgsT parameter types

    @param f callable object to call
    @param args parameters to pass to the callable

    @return a tf::Future that will holds the result of the execution

    The method creates an asynchronous task to launch the given
    function on the given arguments.
    Unlike std::async, the return here is a @em tf::Future that holds
    an optional object to the result.
    If the asynchronous task is cancelled before it runs, the return is
    a @c std::nullopt, or the value returned by the callable.

    @code{.cpp}
    tf::Fugure<std::optional<int>> future = executor.async([](){
      std::cout << "create an asynchronous task and returns 1\n";
      return 1;
    });
    @endcode

    This member function is thread-safe.
    */
    template <typename F, typename... ArgsT>
    auto async(F&& f, ArgsT&&... args);

    /**
    @brief runs a given function asynchronously and gives a name to this task

    @tparam F callable type
    @tparam ArgsT parameter types

    @param name name of the asynchronous task
    @param f callable object to call
    @param args parameters to pass to the callable

    @return a tf::Future that will holds the result of the execution

    The method creates a named asynchronous task to launch the given
    function on the given arguments.
    Naming an asynchronous task is primarily used for profiling and visualizing
    the task execution timeline.
    Unlike std::async, the return here is a tf::Future that holds
    an optional object to the result.
    If the asynchronous task is cancelled before it runs, the return is
    a @c std::nullopt, or the value returned by the callable.

    @code{.cpp}
    tf::Fugure<std::optional<int>> future = executor.named_async("name", [](){
      std::cout << "create an asynchronous task with a name and returns 1\n";
      return 1;
    });
    @endcode

    This member function is thread-safe.
    */
    template <typename F, typename... ArgsT>
    auto named_async(const string& name, F&& f, ArgsT&&... args);

    /**
    @brief similar to tf::Executor::async but does not return a future object

    This member function is more efficient than tf::Executor::async
    and is encouraged to use when there is no data returned.

    @code{.cpp}
    executor.silent_async([](){
      std::cout << "create an asynchronous task with no return\n";
    });
    @endcode

    This member function is thread-safe.
    */
    template <typename F, typename... ArgsT>
    void silent_async(F&& f, ArgsT&&... args);

    /**
    @brief similar to tf::Executor::named_async but does not return a future object

    This member function is more efficient than tf::Executor::named_async
    and is encouraged to use when there is no data returned.

    @code{.cpp}
    executor.named_silent_async("name", [](){
      std::cout << "create an asynchronous task with a name and no return\n";
    });
    @endcode

    This member function is thread-safe.
    */
    template <typename F, typename... ArgsT>
    void named_silent_async(const string& name, F&& f, ArgsT&&... args);

    /**
    @brief constructs an observer to inspect the activities of worker threads

    @tparam Observer observer type derived from tf::ObserverInterface
    @tparam ArgsT argument parameter pack

    @param args arguments to forward to the constructor of the observer

    @return a shared pointer to the created observer

    Each executor manages a list of observers with shared ownership with callers.
    For each of these observers, the two member functions,
    tf::ObserverInterface::on_entry and tf::ObserverInterface::on_exit
    will be called before and after the execution of a task.

    This member function is not thread-safe.
    */
    template <typename Observer, typename... ArgsT>
    std::shared_ptr<Observer> make_observer(ArgsT&&... args);

    /**
    @brief removes an observer from the executor

    This member function is not thread-safe.
    */
    template <typename Observer>
    void remove_observer(std::shared_ptr<Observer> observer);

    /**
    @brief queries the number of observers
    */
    size_t num_observers() const noexcept;

  private:

    std::condition_variable _topology_cv;
    std::mutex _taskflow_mutex;
    std::mutex _topology_mutex;
    std::mutex _wsq_mutex;

    size_t _num_topologies {0};

    vstd::HashMap<std::thread::id, size_t> _wids;
    vstd::vector<Worker> _workers;
    vstd::vector<std::thread> _threads;
    std::list<Taskflow> _taskflows;

    Notifier _notifier;

    TaskQueue<Node*> _wsq;

    std::atomic<size_t> _num_actives {0};
    std::atomic<size_t> _num_thieves {0};
    std::atomic<bool>   _done {0};

    std::unordered_set<std::shared_ptr<ObserverInterface>> _observers;

    Worker* _this_worker();

    bool _wait_for_task(Worker&, Node*&);

    void _observer_prologue(Worker&, Node*);
    void _observer_epilogue(Worker&, Node*);
    void _spawn(size_t);
    void _worker_loop(Worker&);
    void _exploit_task(Worker&, Node*&);
    void _explore_task(Worker&, Node*&);
    void _consume_task(Worker&, Node*);
    void _schedule(Worker&, Node*);
    void _schedule(Node*);
    void _schedule(Worker&, const SmallVector<Node*>&);
    void _schedule(const SmallVector<Node*>&);
    void _set_up_topology(Worker*, Topology*);
    void _tear_down_topology(Worker&, Topology*);
    void _tear_down_async(Node*);
    void _tear_down_invoke(Worker&, Node*);
    void _cancel_invoke(Worker&, Node*);
    void _increment_topology();
    void _decrement_topology();
    void _decrement_topology_and_notify();
    void _invoke(Worker&, Node*);
    void _invoke_static_task(Worker&, Node*);
    void _invoke_dynamic_task(Worker&, Node*);
    void _join_dynamic_task_external(Worker&, Node*, Graph&);
    void _join_dynamic_task_internal(Worker&, Node*, Graph&);
    void _detach_dynamic_task(Worker&, Node*, Graph&);
    void _invoke_condition_task(Worker&, Node*, SmallVector<int>&);
    void _invoke_multi_condition_task(Worker&, Node*, SmallVector<int>&);
    void _invoke_module_task(Worker&, Node*);
    void _invoke_async_task(Worker&, Node*);
    void _invoke_silent_async_task(Worker&, Node*);
    void _invoke_cudaflow_task(Worker&, Node*);
    void _invoke_syclflow_task(Worker&, Node*);
    void _invoke_runtime_task(Worker&, Node*);

    template <typename C,
      std::enable_if_t<is_cudaflow_task_v<C>, void>* = nullptr
    >
    void _invoke_cudaflow_task_entry(Node*, C&&);

    template <typename C, typename Q,
      std::enable_if_t<is_syclflow_task_v<C>, void>* = nullptr
    >
    void _invoke_syclflow_task_entry(Node*, C&&, Q&);
};

TF_API ObjectPool<Node>* get_node_pool();
// Function: named_async
template <typename F, typename... ArgsT>
auto Executor::named_async(const string& name, F&& f, ArgsT&&... args) {

  _increment_topology();

  using T = std::invoke_result_t<F, ArgsT...>;
  using R = std::conditional_t<std::is_same_v<T, void>, void, std::optional<T>>;

  std::promise<R> p;

  auto tpg = std::make_shared<AsyncTopology>();

  Future<R> fu(p.get_future(), tpg);

  auto node = get_node_pool()->animate(
    std::in_place_type_t<Node::Async>{},
    [p=make_moc(std::move(p)), f=std::forward<F>(f), args...]
    (bool cancel) mutable {
      if constexpr(std::is_same_v<R, void>) {
        if(!cancel) {
          f(args...);
        }
        p.object.set_value();
      }
      else {
        p.object.set_value(cancel ? std::nullopt : std::make_optional(f(args...)));
      }
    },
    std::move(tpg)
  );

  node->_name = name;

  if(auto w = _this_worker(); w) {
    _schedule(*w, node);
  }
  else{
    _schedule(node);
  }

  return fu;
}

// Function: async
template <typename F, typename... ArgsT>
auto Executor::async(F&& f, ArgsT&&... args) {
  return named_async("", std::forward<F>(f), std::forward<ArgsT>(args)...);
}

// Function: named_silent_async
template <typename F, typename... ArgsT>
void Executor::named_silent_async(
  const string& name, F&& f, ArgsT&&... args
) {

  _increment_topology();

  Node* node = get_node_pool()->animate(
    std::in_place_type_t<Node::SilentAsync>{},
    [f=std::forward<F>(f), args...] () mutable {
      f(args...);
    }
  );

  node->_name = name;

  if(auto w = _this_worker(); w) {
    _schedule(*w, node);
  }
  else {
    _schedule(node);
  }
}

// Function: silent_async
template <typename F, typename... ArgsT>
void Executor::silent_async(F&& f, ArgsT&&... args) {
  named_silent_async("", std::forward<F>(f), std::forward<ArgsT>(args)...);
}


// Function: make_observer
template<typename Observer, typename... ArgsT>
std::shared_ptr<Observer> Executor::make_observer(ArgsT&&... args) {

  static_assert(
    std::is_base_of_v<ObserverInterface, Observer>,
    "Observer must be derived from ObserverInterface"
  );

  // use a local variable to mimic the constructor
  auto ptr = std::make_shared<Observer>(std::forward<ArgsT>(args)...);

  ptr->set_up(_workers.size());

  _observers.emplace(std::static_pointer_cast<ObserverInterface>(ptr));

  return ptr;
}

// Procedure: remove_observer
template <typename Observer>
void Executor::remove_observer(std::shared_ptr<Observer> ptr) {

  static_assert(
    std::is_base_of_v<ObserverInterface, Observer>,
    "Observer must be derived from ObserverInterface"
  );

  _observers.erase(std::static_pointer_cast<ObserverInterface>(ptr));
}


// Function: run


// Function: run
template <typename C>
tf::Future<void> Executor::run(Taskflow& f, C&& c) {
  return run_n(f, 1, std::forward<C>(c));
}

// Function: run
template <typename C>
tf::Future<void> Executor::run(Taskflow&& f, C&& c) {
  return run_n(std::move(f), 1, std::forward<C>(c));
}

// Function: run_n


// Function: run_n
template <typename C>
tf::Future<void> Executor::run_n(Taskflow& f, size_t repeat, C&& c) {
  return run_until(
    f, [repeat]() mutable { return repeat-- == 0; }, std::forward<C>(c)
  );
}

// Function: run_n
template <typename C>
tf::Future<void> Executor::run_n(Taskflow&& f, size_t repeat, C&& c) {
  return run_until(
    std::move(f), [repeat]() mutable { return repeat-- == 0; }, std::forward<C>(c)
  );
}

// Function: run_until
template<typename P>
tf::Future<void> Executor::run_until(Taskflow& f, P&& pred) {
  return run_until(f, std::forward<P>(pred), [](){});
}

// Function: run_until
template<typename P>
tf::Future<void> Executor::run_until(Taskflow&& f, P&& pred) {
  return run_until(std::move(f), std::forward<P>(pred), [](){});
}

// Function: run_until
template <typename P, typename C>
tf::Future<void> Executor::run_until(Taskflow& f, P&& p, C&& c) {

  _increment_topology();

  // Need to check the empty under the lock since dynamic task may
  // define detached blocks that modify the taskflow at the same time
  bool empty;
  {
    std::lock_guard<std::mutex> lock(f._mutex);
    empty = f.empty();
  }

  // No need to create a real topology but returns an dummy future
  if(empty || p()) {
    c();
    std::promise<void> promise;
    promise.set_value();
    _decrement_topology_and_notify();
    return tf::Future<void>(promise.get_future(), std::monostate{});
  }

  // create a topology for this run
  auto t = std::make_shared<Topology>(f, std::forward<P>(p), std::forward<C>(c));

  // need to create future before the topology got torn down quickly
  tf::Future<void> future(t->_promise.get_future(), t);

  // modifying topology needs to be protected under the lock
  {
    std::lock_guard<std::mutex> lock(f._mutex);
    f._topologies.push(t);
    if(f._topologies.size() == 1) {
      _set_up_topology(_this_worker(), t.get());
    }
  }

  return future;
}

// Function: run_until
template <typename P, typename C>
tf::Future<void> Executor::run_until(Taskflow&& f, P&& pred, C&& c) {

  std::list<Taskflow>::iterator itr;

  {
    std::scoped_lock<std::mutex> lock(_taskflow_mutex);
    itr = _taskflows.emplace(_taskflows.end(), std::move(f));
    itr->_satellite = itr;
  }

  return run_until(*itr, std::forward<P>(pred), std::forward<C>(c));
}


// Function: named_async
template <typename F, typename... ArgsT>
auto Subflow::named_async(const string& name, F&& f, ArgsT&&... args) {
  return _named_async(
    *_executor._this_worker(), name, std::forward<F>(f), std::forward<ArgsT>(args)...
  );
}

// Function: _named_async
template <typename F, typename... ArgsT>
auto Subflow::_named_async(
  Worker& w,
  const string& name,
  F&& f,
  ArgsT&&... args
) {

  _parent->_join_counter.fetch_add(1);

  using T = std::invoke_result_t<F, ArgsT...>;
  using R = std::conditional_t<std::is_same_v<T, void>, void, std::optional<T>>;

  std::promise<R> p;

  auto tpg = std::make_shared<AsyncTopology>();

  Future<R> fu(p.get_future(), tpg);

  auto node =  get_node_pool()->animate(
    std::in_place_type_t<Node::Async>{},
    [p=make_moc(std::move(p)), f=std::forward<F>(f), args...]
    (bool cancel) mutable {
      if constexpr(std::is_same_v<R, void>) {
        if(!cancel) {
          f(args...);
        }
        p.object.set_value();
      }
      else {
        p.object.set_value(cancel ? std::nullopt : std::make_optional(f(args...)));
      }
    },
    std::move(tpg)
  );

  node->_name = name;
  node->_topology = _parent->_topology;
  node->_parent = _parent;

  _executor._schedule(w, node);

  return fu;
}

// Function: async
template <typename F, typename... ArgsT>
auto Subflow::async(F&& f, ArgsT&&... args) {
  return named_async("", std::forward<F>(f), std::forward<ArgsT>(args)...);
}

// Function: _named_silent_async
template <typename F, typename... ArgsT>
void Subflow::_named_silent_async(
  Worker& w, const string& name, F&& f, ArgsT&&... args
) {

  _parent->_join_counter.fetch_add(1);

  auto node = get_node_pool()->animate(
    std::in_place_type_t<Node::SilentAsync>{},
    [f=std::forward<F>(f), args...] () mutable {
      f(args...);
    }
  );

  node->_name = name;
  node->_topology = _parent->_topology;
  node->_parent = _parent;

  _executor._schedule(w, node);
}

// Function: silent_async
template <typename F, typename... ArgsT>
void Subflow::named_silent_async(const string& name, F&& f, ArgsT&&... args) {
  _named_silent_async(
    *_executor._this_worker(), name, std::forward<F>(f), std::forward<ArgsT>(args)...
  );
}

// Function: named_silent_async
template <typename F, typename... ArgsT>
void Subflow::silent_async(F&& f, ArgsT&&... args) {
  named_silent_async("", std::forward<F>(f), std::forward<ArgsT>(args)...);
}

// ############################################################################
// Forward Declaration: Runtime
// ############################################################################

// Procedure: schedule


// Procedure: run
template <typename C>
void Runtime::run(C&& callable) {

  // dynamic task (subflow)
  if constexpr(is_dynamic_task_v<C>) {
    Graph graph;
    Subflow sf(_executor, _worker, _parent, graph);
    callable(sf);
    if(sf._joinable) {
      _executor._join_dynamic_task_internal(_worker, _parent, graph);
    }
  }
  else {
    static_assert(dependent_false_v<C>, "unsupported task callable to run");
  }
}

}  // end of namespace tf -----------------------------------------------------








