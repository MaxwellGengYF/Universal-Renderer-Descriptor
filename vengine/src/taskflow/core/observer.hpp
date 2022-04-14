#pragma once

#include "task.hpp"
#include "worker.hpp"

/** 
@file observer.hpp
@brief observer include file
*/

namespace tf {
class TF_API TFProfManager {

	friend class Executor;

public:
	~TFProfManager();

	TFProfManager(const TFProfManager&) = delete;
	TFProfManager& operator=(const TFProfManager&) = delete;

	static TFProfManager& get();

	void dump(std::ostream& ostream) const;

private:
	const std::string _fpath;

	std::mutex _mutex;
	vstd::vector<std::shared_ptr<TFProfObserver>> _observers;

	TFProfManager();

	void _manage(std::shared_ptr<TFProfObserver> observer);
};

// ----------------------------------------------------------------------------
// timeline data structure
// ----------------------------------------------------------------------------

/**
@brief default time point type of observers
*/
using observer_stamp_t = std::chrono::time_point<std::chrono::steady_clock>;

/**
@private
*/
struct Segment {

	std::string name;
	TaskType type;

	observer_stamp_t beg;
	observer_stamp_t end;

	//template <typename Archiver>
	//auto save(Archiver& ar) const {
	//  return ar(name, type, beg, end);
	//}

	//template <typename Archiver>
	//auto load(Archiver& ar) {
	//  return ar(name, type, beg, end);
	//}

	Segment() = default;

	Segment(
		const std::string& n, TaskType t, observer_stamp_t b, observer_stamp_t e) : name{n}, type{t}, beg{b}, end{e} {
	}

	auto span() const {
		return end - beg;
	}
};

/**
@private
*/
struct Timeline {

	size_t uid;

	observer_stamp_t origin;
	vstd::vector<vstd::vector<vstd::vector<Segment>>> segments;

	Timeline() = default;

	Timeline(const Timeline& rhs) = delete;
	Timeline(Timeline&& rhs) = default;

	Timeline& operator=(const Timeline& rhs) = delete;
	Timeline& operator=(Timeline&& rhs) = default;

	//template <typename Archiver>
	//auto save(Archiver& ar) const {
	//  return ar(uid, origin, segments);
	//}

	//template <typename Archiver>
	//auto load(Archiver& ar) {
	//  return ar(uid, origin, segments);
	//}
};

/**
@private
 */
struct ProfileData {

	vstd::vector<Timeline> timelines;

	ProfileData() = default;

	ProfileData(const ProfileData& rhs) = delete;
	ProfileData(ProfileData&& rhs) = default;

	ProfileData& operator=(const ProfileData& rhs) = delete;
	ProfileData& operator=(ProfileData&&) = default;

	//template <typename Archiver>
	//auto save(Archiver& ar) const {
	//  return ar(timelines);
	//}

	//template <typename Archiver>
	//auto load(Archiver& ar) {
	//  return ar(timelines);
	//}
};

// ----------------------------------------------------------------------------
// observer interface
// ----------------------------------------------------------------------------

/**
@class: ObserverInterface

@brief class to derive an executor observer 

The tf::ObserverInterface class let users define custom methods to monitor 
the behaviors of an executor. This is particularly useful when you want to 
inspect the performance of an executor and visualize when each thread 
participates in the execution of a task.
To prevent users from direct access to the internal threads and tasks, 
tf::ObserverInterface provides immutable wrappers,
tf::WorkerView and tf::TaskView, over workers and tasks.

Please refer to tf::WorkerView and tf::TaskView for details.

Example usage:

@code{.cpp}

struct MyObserver : public tf::ObserverInterface {

  MyObserver(const std::string& name) {
    std::cout << "constructing observer " << name << '\n';
  }

  void set_up(size_t num_workers) override final {
    std::cout << "setting up observer with " << num_workers << " workers\n";
  }

  void on_entry(WorkerView w, tf::TaskView tv) override final {
    std::ostringstream oss;
    oss << "worker " << w.id() << " ready to run " << tv.name() << '\n';
    std::cout << oss.str();
  }

  void on_exit(WorkerView w, tf::TaskView tv) override final {
    std::ostringstream oss;
    oss << "worker " << w.id() << " finished running " << tv.name() << '\n';
    std::cout << oss.str();
  }
};
  
tf::Taskflow taskflow;
tf::Executor executor;

// insert tasks into taskflow
// ...
  
// create a custom observer
std::shared_ptr<MyObserver> observer = executor.make_observer<MyObserver>("MyObserver");

// run the taskflow
executor.run(taskflow).wait();
@endcode
*/
class ObserverInterface {

	friend class Executor;

public:
	/**
  @brief virtual destructor
  */
	virtual ~ObserverInterface() = default;

	/**
  @brief constructor-like method to call when the executor observer is fully created
  @param num_workers the number of the worker threads in the executor
  */
	virtual void set_up(size_t num_workers) = 0;

	/**
  @brief method to call before a worker thread executes a closure 
  @param w an immutable view of this worker thread 
  @param task_view a constant wrapper object to the task 
  */
	virtual void on_entry(WorkerView w, TaskView task_view) = 0;

	/**
  @brief method to call after a worker thread executed a closure
  @param w an immutable view of this worker thread
  @param task_view a constant wrapper object to the task
  */
	virtual void on_exit(WorkerView w, TaskView task_view) = 0;
};

// ----------------------------------------------------------------------------
// ChromeObserver definition
// ----------------------------------------------------------------------------

/**
@class: ChromeObserver

@brief class to create an observer based on Chrome tracing format

A tf::ChromeObserver inherits tf::ObserverInterface and defines methods to dump
the observed thread activities into a format that can be visualized through
@ChromeTracing.

@code{.cpp}
tf::Taskflow taskflow;
tf::Executor executor;

// insert tasks into taskflow
// ...
  
// create a custom observer
std::shared_ptr<tf::ChromeObserver> observer = executor.make_observer<tf::ChromeObserver>();

// run the taskflow
executor.run(taskflow).wait();

// dump the thread activities to a chrome-tracing format.
observer->dump(std::cout);
@endcode
*/
class TF_API ChromeObserver : public ObserverInterface {

	friend class Executor;

	// data structure to record each task execution
	struct Segment {

		std::string name;

		observer_stamp_t beg;
		observer_stamp_t end;

		Segment(
			const std::string& n,
			observer_stamp_t b,
			observer_stamp_t e);
	};

	// data structure to store the entire execution timeline
	struct Timeline {
		observer_stamp_t origin;
		vstd::vector<vstd::vector<Segment>> segments;
		vstd::vector<std::stack<observer_stamp_t>> stacks;
	};

public:
	/**
    @brief dumps the timelines into a @ChromeTracing format through 
           an output stream 
    */
	void dump(std::ostream& ostream) const;

	/**
    @brief dumps the timelines into a @ChromeTracing format
    */
	 std::string dump() const;

	/**
    @brief clears the timeline data
    */
	 void clear();

	/**
    @brief queries the number of tasks observed
    */
	 size_t num_tasks() const;

private:
	 void set_up(size_t num_workers) override final;
	 void on_entry(WorkerView w, TaskView task_view) override final;
	 void on_exit(WorkerView w, TaskView task_view) override final;

	Timeline _timeline;
};


// ----------------------------------------------------------------------------
// TFProfObserver definition
// ----------------------------------------------------------------------------

/**
@class TFProfObserver

@brief class to create an observer based on the built-in taskflow profiler format

A tf::TFProfObserver inherits tf::ObserverInterface and defines methods to dump
the observed thread activities into a format that can be visualized through
@TFProf.

@code{.cpp}
tf::Taskflow taskflow;
tf::Executor executor;

// insert tasks into taskflow
// ...
  
// create a custom observer
std::shared_ptr<tf::TFProfObserver> observer = executor.make_observer<tf::TFProfObserver>();

// run the taskflow
executor.run(taskflow).wait();

// dump the thread activities to Taskflow Profiler format.
observer->dump(std::cout);
@endcode

We recommend using our @TFProf python script to observe thread activities 
instead of the raw function call.
The script will turn on environment variables needed for observing all executors 
in a taskflow program and dump the result to a valid, clean JSON file
compatible with the format of @TFProf.
*/
class TFProfObserver : public ObserverInterface {

	friend class Executor;
	friend class TFProfManager;

public:
	/**
    @brief dumps the timelines into a @TFProf format through 
           an output stream
    */
	void dump(std::ostream& ostream) const;

	/**
    @brief dumps the timelines into a JSON string
    */
	std::string dump() const;

	/**
    @brief clears the timeline data
    */
	void clear();

	/**
    @brief queries the number of tasks observed
    */
	size_t num_tasks() const;

private:
	Timeline _timeline;

	vstd::vector<std::stack<observer_stamp_t>> _stacks;

	void set_up(size_t num_workers) override final;
	void on_entry(WorkerView, TaskView) override final;
	void on_exit(WorkerView, TaskView) override final;
};

// ----------------------------------------------------------------------------
// Identifier for Each Built-in Observer
// ----------------------------------------------------------------------------

/** @enum ObserverType

@brief enumeration of all observer types

*/
enum class ObserverType : int {
	TFPROF = 0,
	CHROME,
	UNDEFINED
};

}// namespace tf
