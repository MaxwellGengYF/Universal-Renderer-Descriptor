#include <taskflow/taskflow.hpp>
namespace tf {

static ObjectPool<Node> node_pool;
TF_API ObjectPool<Node>* get_node_pool() {
	return &node_pool;
}
// Procedure: _increment_topology
void Executor::_increment_topology() {
	std::lock_guard<std::mutex> lock(_topology_mutex);
	++_num_topologies;
}

// Procedure: _decrement_topology_and_notify
void Executor::_decrement_topology_and_notify() {
	std::lock_guard<std::mutex> lock(_topology_mutex);
	if (--_num_topologies == 0) {
		_topology_cv.notify_all();
	}
}

// Procedure: _decrement_topology
void Executor::_decrement_topology() {
	std::lock_guard<std::mutex> lock(_topology_mutex);
	--_num_topologies;
}

// Procedure: wait_for_all
void Executor::wait_for_all() {
	std::unique_lock<std::mutex> lock(_topology_mutex);
	_topology_cv.wait(lock, [&]() { return _num_topologies == 0; });
}

// Function: _set_up_topology
void Executor::_set_up_topology(Worker* worker, Topology* tpg) {

	// ---- under taskflow lock ----

	tpg->_sources.clear();
	tpg->_taskflow._graph._clear_detached();

	// scan each node in the graph and build up the links
	for (auto node : tpg->_taskflow._graph._nodes) {

		node->_topology = tpg;
		node->_state.store(0, std::memory_order_relaxed);

		if (node->num_dependents() == 0) {
			tpg->_sources.push_back(node);
		}

		node->_set_up_join_counter();
	}

	tpg->_join_counter = tpg->_sources.size();

	if (worker) {
		_schedule(*worker, tpg->_sources);
	} else {
		_schedule(tpg->_sources);
	}
}

// Function: _tear_down_topology
void Executor::_tear_down_topology(Worker& worker, Topology* tpg) {

	auto& f = tpg->_taskflow;

	//assert(&tpg == &(f._topologies.front()));

	// case 1: we still need to run the topology again
	if (!tpg->_is_cancelled && !tpg->_pred()) {
		//assert(tpg->_join_counter == 0);
		std::lock_guard<std::mutex> lock(f._mutex);
		tpg->_join_counter = tpg->_sources.size();
		_schedule(worker, tpg->_sources);
	}
	// case 2: the final run of this topology
	else {

		// TODO: if the topology is cancelled, need to release all semaphores

		if (tpg->_call != nullptr) {
			tpg->_call();
		}

		// If there is another run (interleave between lock)
		if (std::unique_lock<std::mutex> lock(f._mutex); f._topologies.size() > 1) {
			//assert(tpg->_join_counter == 0);

			// Set the promise
			tpg->_promise.set_value();
			f._topologies.pop();
			tpg = f._topologies.front().get();

			// decrement the topology but since this is not the last we don't notify
			_decrement_topology();

			// set up topology needs to be under the lock or it can
			// introduce memory order error with pop
			_set_up_topology(&worker, tpg);
		} else {
			//assert(f._topologies.size() == 1);

			// Need to back up the promise first here becuz taskflow might be
			// destroy soon after calling get
			auto p{std::move(tpg->_promise)};

			// Back up lambda capture in case it has the topology pointer,
			// to avoid it releasing on pop_front ahead of _mutex.unlock &
			// _promise.set_value. Released safely when leaving scope.
			auto c{std::move(tpg->_call)};

			// Get the satellite if any
			auto s{f._satellite};

			// Now we remove the topology from this taskflow
			f._topologies.pop();

			//f._mutex.unlock();
			lock.unlock();

			// We set the promise in the end in case taskflow leaves the scope.
			// After set_value, the caller will return from wait
			p.set_value();

			_decrement_topology_and_notify();

			// remove the taskflow if it is managed by the executor
			// TODO: in the future, we may need to synchronize on wait
			// (which means the following code should the moved before set_value)
			if (s) {
				std::scoped_lock<std::mutex> lock(_taskflow_mutex);
				_taskflows.erase(*s);
			}
		}
	}
}

// ############################################################################
// Forward Declaration: Subflow
// ############################################################################

void Subflow::join() {

	// assert(this_worker().worker == &_worker);

	if (!_joinable) {
		TF_THROW("subflow not joinable");
	}

	// only the parent worker can join the subflow
	_executor._join_dynamic_task_external(_worker, _parent, _graph);
	_joinable = false;
}

void Subflow::detach() {

	// assert(this_worker().worker == &_worker);

	if (!_joinable) {
		TF_THROW("subflow already joined or detached");
	}

	// only the parent worker can detach the subflow
	_executor._detach_dynamic_task(_worker, _parent, _graph);
	_joinable = false;
}
tf::Future<void> Executor::run(Taskflow& f) {
	return run_n(f, 1, []() {});
}

// Function: run
tf::Future<void> Executor::run(Taskflow&& f) {
	return run_n(std::move(f), 1, []() {});
}
tf::Future<void> Executor::run_n(Taskflow& f, size_t repeat) {
	return run_n(f, repeat, []() {});
}

// Function: run_n
tf::Future<void> Executor::run_n(Taskflow&& f, size_t repeat) {
	return run_n(std::move(f), repeat, []() {});
}
// Function: num_observers
size_t Executor::num_observers() const noexcept {
	return _observers.size();
}
// Constructor
Executor::Executor(size_t N) : _workers{N},
							   _notifier{N} {

	if (N == 0) {
		TF_THROW("no cpu workers to execute taskflows");
	}

	_spawn(N);

	// instantite the default observer if requested
	if (has_env(TF_ENABLE_PROFILER)) {
		TFProfManager::get()._manage(make_observer<TFProfObserver>());
	}
}

// Destructor
Executor::~Executor() {

	// wait for all topologies to complete
	wait_for_all();

	// shut down the scheduler
	_done = true;

	_notifier.notify(true);

	for (auto& t : _threads) {
		t.join();
	}
}
// Procedure: _schedule
void Executor::_schedule(Worker& worker, Node* node) {

	node->_state.fetch_or(Node::READY, std::memory_order_release);

	// caller is a worker to this pool
	if (worker._executor == this) {
		worker._wsq.push(node);
		return;
	}

	{
		std::lock_guard<std::mutex> lock(_wsq_mutex);
		_wsq.push(node);
	}

	_notifier.notify(false);
}

// Procedure: _schedule
void Executor::_schedule(Node* node) {

	node->_state.fetch_or(Node::READY, std::memory_order_release);

	{
		std::lock_guard<std::mutex> lock(_wsq_mutex);
		_wsq.push(node);
	}

	_notifier.notify(false);
}

// Procedure: _schedule
void Executor::_schedule(
	Worker& worker, const SmallVector<Node*>& nodes) {

	// We need to cacth the node count to avoid accessing the nodes
	// vector while the parent topology is removed!
	const auto num_nodes = nodes.size();

	if (num_nodes == 0) {
		return;
	}

	// make the node ready
	for (size_t i = 0; i < num_nodes; ++i) {
		nodes[i]->_state.fetch_or(Node::READY, std::memory_order_release);
	}

	if (worker._executor == this) {
		for (size_t i = 0; i < num_nodes; ++i) {
			worker._wsq.push(nodes[i]);
		}
		return;
	}

	{
		std::lock_guard<std::mutex> lock(_wsq_mutex);
		for (size_t k = 0; k < num_nodes; ++k) {
			_wsq.push(nodes[k]);
		}
	}

	_notifier.notify_n(num_nodes);
}

// Procedure: _schedule
void Executor::_schedule(const SmallVector<Node*>& nodes) {
	// parent topology may be removed!
	const auto num_nodes = nodes.size();

	if (num_nodes == 0) {
		return;
	}

	// make the node ready
	for (size_t i = 0; i < num_nodes; ++i) {
		nodes[i]->_state.fetch_or(Node::READY, std::memory_order_release);
	}

	{
		std::lock_guard<std::mutex> lock(_wsq_mutex);
		for (size_t k = 0; k < num_nodes; ++k) {
			_wsq.push(nodes[k]);
		}
	}

	_notifier.notify_n(num_nodes);
}

// Procedure: _invoke
void Executor::_invoke(Worker& worker, Node* node) {

	// synchronize all outstanding memory operations caused by reordering
	while (!(node->_state.load(std::memory_order_acquire) & Node::READY))
		;

begin_invoke:

	// no need to do other things if the topology is cancelled
	if (node->_is_cancelled()) {
		_cancel_invoke(worker, node);
		return;
	}

	// if acquiring semaphore(s) exists, acquire them first
	if (node->_semaphores && !node->_semaphores->to_acquire.empty()) {
		SmallVector<Node*> nodes;
		if (!node->_acquire_all(nodes)) {
			_schedule(worker, nodes);
			return;
		}
		node->_state.fetch_or(Node::ACQUIRED, std::memory_order_release);
	}

	// condition task
	//int cond = -1;
	SmallVector<int> conds;

	// switch is faster than nested if-else due to jump table
	switch (node->_handle.index()) {
		// static task
		case Node::STATIC: {
			_invoke_static_task(worker, node);
		} break;

		// dynamic task
		case Node::DYNAMIC: {
			_invoke_dynamic_task(worker, node);
		} break;

		// condition task
		case Node::CONDITION: {
			_invoke_condition_task(worker, node, conds);
		} break;

		// multi-condition task
		case Node::MULTI_CONDITION: {
			_invoke_multi_condition_task(worker, node, conds);
		} break;

		// module task
		case Node::MODULE: {
			_invoke_module_task(worker, node);
		} break;

		// async task
		case Node::ASYNC: {
			_invoke_async_task(worker, node);
			_tear_down_async(node);
			return;
		} break;

		// silent async task
		case Node::SILENT_ASYNC: {
			_invoke_silent_async_task(worker, node);
			_tear_down_async(node);
			return;
		} break;

		// cudaflow task
		case Node::CUDAFLOW: {
			_invoke_cudaflow_task(worker, node);
		} break;

		// syclflow task
		case Node::SYCLFLOW: {
			_invoke_syclflow_task(worker, node);
		} break;

		// runtime task
		case Node::RUNTIME: {
			_invoke_runtime_task(worker, node);
		} break;

		// monostate (placeholder)
		default:
			break;
	}

	// if releasing semaphores exist, release them
	if (node->_semaphores && !node->_semaphores->to_release.empty()) {
		_schedule(worker, node->_release_all());
	}

	// We MUST recover the dependency since the graph may have cycles.
	// This must be done before scheduling the successors, otherwise this might cause
	// race condition on the _dependents
	if ((node->_state.load(std::memory_order_relaxed) & Node::CONDITIONED)) {
		node->_join_counter = node->num_strong_dependents();
	} else {
		node->_join_counter = node->num_dependents();
	}

	// acquire the parent flow counter
	auto& j = (node->_parent) ? node->_parent->_join_counter : node->_topology->_join_counter;

	Node* cache{nullptr};

	// At this point, the node storage might be destructed (to be verified)
	// case 1: non-condition task
	switch (node->_handle.index()) {

		// condition and multi-condition tasks
		case Node::CONDITION:
		case Node::MULTI_CONDITION: {
			for (auto cond : conds) {
				if (cond >= 0 && static_cast<size_t>(cond) < node->_successors.size()) {
					auto s = node->_successors[cond];
					// zeroing the join counter for invariant
					s->_join_counter.store(0, std::memory_order_relaxed);
					j.fetch_add(1);
					if (cache) {
						_schedule(worker, cache);
					}
					cache = s;
				}
			}
		} break;

		// non-condition task
		default: {
			for (size_t i = 0; i < node->_successors.size(); ++i) {
				if (--(node->_successors[i]->_join_counter) == 0) {
					j.fetch_add(1);
					if (cache) {
						_schedule(worker, cache);
					}
					cache = node->_successors[i];
				}
			}
		} break;
	}

	// tear_down the invoke
	_tear_down_invoke(worker, node);

	// perform tail recursion elimination for the right-most child to reduce
	// the number of expensive pop/push operations through the task queue
	if (cache) {
		node = cache;
		//node->_state.fetch_or(Node::READY, std::memory_order_release);
		goto begin_invoke;
	}
}

// Procedure: _tear_down_async
void Executor::_tear_down_async(Node* node) {
	if (node->_parent) {
		node->_parent->_join_counter.fetch_sub(1);
	} else {
		_decrement_topology_and_notify();
	}
	node_pool.recycle(node);
}

// Proecdure: _tear_down_invoke
void Executor::_tear_down_invoke(Worker& worker, Node* node) {
	// we must check parent first before substracting the join counter,
	// or it can introduce data race
	if (node->_parent == nullptr) {
		if (node->_topology->_join_counter.fetch_sub(1) == 1) {
			_tear_down_topology(worker, node->_topology);
		}
	} else {// joined subflow
		node->_parent->_join_counter.fetch_sub(1);
	}
}

// Procedure: _cancel_invoke
void Executor::_cancel_invoke(Worker& worker, Node* node) {

	switch (node->_handle.index()) {
		// async task needs to carry out the promise
		case Node::ASYNC:
			std::get_if<Node::Async>(&(node->_handle))->work(true);
			_tear_down_async(node);
			break;

		// silent async doesn't need to carry out the promise
		case Node::SILENT_ASYNC:
			_tear_down_async(node);
			break;

		// tear down topology if the node is the last leaf
		default: {
			_tear_down_invoke(worker, node);
		} break;
	}
}

// Procedure: _observer_prologue
void Executor::_observer_prologue(Worker& worker, Node* node) {
	for (auto& observer : _observers) {
		observer->on_entry(WorkerView(worker), TaskView(*node));
	}
}

// Procedure: _observer_epilogue
void Executor::_observer_epilogue(Worker& worker, Node* node) {
	for (auto& observer : _observers) {
		observer->on_exit(WorkerView(worker), TaskView(*node));
	}
}

// Procedure: _invoke_static_task
void Executor::_invoke_static_task(Worker& worker, Node* node) {
	_observer_prologue(worker, node);
	std::get_if<Node::Static>(&node->_handle)->work();
	_observer_epilogue(worker, node);
}

// Procedure: _invoke_dynamic_task
void Executor::_invoke_dynamic_task(Worker& w, Node* node) {

	_observer_prologue(w, node);

	auto handle = std::get_if<Node::Dynamic>(&node->_handle);

	handle->subgraph._clear();

	Subflow sf(*this, w, node, handle->subgraph);

	handle->work(sf);

	if (sf._joinable) {
		_join_dynamic_task_internal(w, node, handle->subgraph);
	}

	_observer_epilogue(w, node);
}

// Procedure: _detach_dynamic_task
void Executor::_detach_dynamic_task(
	Worker& w, Node* p, Graph& g) {

	// graph is empty and has no async tasks
	if (g.empty() && p->_join_counter == 0) {
		return;
	}

	SmallVector<Node*> src;

	for (auto n : g._nodes) {

		n->_state.store(Node::DETACHED, std::memory_order_relaxed);
		n->_set_up_join_counter();
		n->_topology = p->_topology;
		n->_parent = nullptr;

		if (n->num_dependents() == 0) {
			src.push_back(n);
		}
	}

	{
		std::lock_guard<std::mutex> lock(p->_topology->_taskflow._mutex);
		p->_topology->_taskflow._graph._merge(std::move(g));
	}

	p->_topology->_join_counter.fetch_add(src.size());
	_schedule(w, src);
}

// Procedure: _join_dynamic_task_external
void Executor::_join_dynamic_task_external(
	Worker& w, Node* p, Graph& g) {

	// graph is empty and has no async tasks
	if (g.empty() && p->_join_counter == 0) {
		return;
	}

	SmallVector<Node*> src;

	for (auto n : g._nodes) {

		n->_state.store(0, std::memory_order_relaxed);
		n->_set_up_join_counter();
		n->_topology = p->_topology;
		n->_parent = p;

		if (n->num_dependents() == 0) {
			src.push_back(n);
		}
	}
	p->_join_counter.fetch_add(src.size());
	_schedule(w, src);
	_consume_task(w, p);
}

// Procedure: _join_dynamic_task_internal
void Executor::_join_dynamic_task_internal(
	Worker& w, Node* p, Graph& g) {

	// graph is empty and has no async tasks
	if (g.empty() && p->_join_counter == 0) {
		return;
	}

	SmallVector<Node*> src;

	for (auto n : g._nodes) {
		n->_topology = p->_topology;
		n->_state.store(0, std::memory_order_relaxed);
		n->_set_up_join_counter();
		n->_parent = p;
		if (n->num_dependents() == 0) {
			src.push_back(n);
		}
	}
	p->_join_counter.fetch_add(src.size());
	_schedule(w, src);
	_consume_task(w, p);
}

// Procedure: _invoke_condition_task
void Executor::_invoke_condition_task(
	Worker& worker, Node* node, SmallVector<int>& conds) {
	_observer_prologue(worker, node);
	conds = {std::get_if<Node::Condition>(&node->_handle)->work()};
	_observer_epilogue(worker, node);
}

// Procedure: _invoke_multi_condition_task
void Executor::_invoke_multi_condition_task(
	Worker& worker, Node* node, SmallVector<int>& conds) {
	_observer_prologue(worker, node);
	conds = std::get_if<Node::MultiCondition>(&node->_handle)->work();
	_observer_epilogue(worker, node);
}

// Procedure: _invoke_cudaflow_task
void Executor::_invoke_cudaflow_task(Worker& worker, Node* node) {
	_observer_prologue(worker, node);
	std::get_if<Node::cudaFlow>(&node->_handle)->work(*this, node);
	_observer_epilogue(worker, node);
}

// Procedure: _invoke_syclflow_task
void Executor::_invoke_syclflow_task(Worker& worker, Node* node) {
	_observer_prologue(worker, node);
	std::get_if<Node::syclFlow>(&node->_handle)->work(*this, node);
	_observer_epilogue(worker, node);
}

// Procedure: _invoke_module_task
void Executor::_invoke_module_task(Worker& w, Node* node) {
	_observer_prologue(w, node);
	_join_dynamic_task_internal(
		w, node, std::get_if<Node::Module>(&node->_handle)->graph);
	_observer_epilogue(w, node);
}

// Procedure: _invoke_async_task
void Executor::_invoke_async_task(Worker& w, Node* node) {
	_observer_prologue(w, node);
	std::get_if<Node::Async>(&node->_handle)->work(false);
	_observer_epilogue(w, node);
}

// Procedure: _invoke_silent_async_task
void Executor::_invoke_silent_async_task(Worker& w, Node* node) {
	_observer_prologue(w, node);
	std::get_if<Node::SilentAsync>(&node->_handle)->work();
	_observer_epilogue(w, node);
}

// Procedure: _invoke_runtime_task
void Executor::_invoke_runtime_task(Worker& w, Node* node) {
	_observer_prologue(w, node);
	Runtime rt(*this, w, node);
	std::get_if<Node::Runtime>(&node->_handle)->work(rt);
	_observer_epilogue(w, node);
}
void Runtime::schedule(Task task) {
	auto node = task._node;
	auto& j = node->_parent ? node->_parent->_join_counter : node->_topology->_join_counter;
	j.fetch_add(1);
	_executor._schedule(_worker, node);
}

// Function: num_workers
size_t Executor::num_workers() const noexcept {
	return _workers.size();
}

// Function: num_topologies
size_t Executor::num_topologies() const {
	return _num_topologies;
}

// Function: num_taskflows
size_t Executor::num_taskflows() const {
	return _taskflows.size();
}

// Function: _this_worker
Worker* Executor::_this_worker() {
	auto itr = _wids.find(std::this_thread::get_id());
	return itr == _wids.end() ? nullptr : &_workers[itr->second];
}

// Procedure: _exploit_task
void Executor::_exploit_task(Worker& w, Node*& t) {

	if (t) {

		if (_num_actives.fetch_add(1) == 0 && _num_thieves == 0) {
			_notifier.notify(false);
		}

		while (t) {
			_invoke(w, t);
			t = w._wsq.pop();
		}

		--_num_actives;
	}
}

// Function: _wait_for_task
bool Executor::_wait_for_task(Worker& worker, Node*& t) {

wait_for_task:

	//assert(!t);

	++_num_thieves;

explore_task:

	_explore_task(worker, t);

	if (t) {
		if (_num_thieves.fetch_sub(1) == 1) {
			_notifier.notify(false);
		}
		return true;
	}

	_notifier.prepare_wait(worker._waiter);

	//if(auto vtm = _find_vtm(me); vtm != _workers.size()) {
	if (!_wsq.empty()) {

		_notifier.cancel_wait(worker._waiter);
		//t = (vtm == me) ? _wsq.steal() : _workers[vtm].wsq.steal();

		t = _wsq.steal();// must steal here
		if (t) {
			if (_num_thieves.fetch_sub(1) == 1) {
				_notifier.notify(false);
			}
			return true;
		} else {
			worker._vtm = worker._id;
			goto explore_task;
		}
	}

	if (_done) {
		_notifier.cancel_wait(worker._waiter);
		_notifier.notify(true);
		--_num_thieves;
		return false;
	}

	if (_num_thieves.fetch_sub(1) == 1) {
		if (_num_actives) {
			_notifier.cancel_wait(worker._waiter);
			goto wait_for_task;
		}
		// check all queues again
		for (auto& w : _workers) {
			if (!w._wsq.empty()) {
				worker._vtm = w._id;
				_notifier.cancel_wait(worker._waiter);
				goto wait_for_task;
			}
		}
	}

	// Now I really need to relinguish my self to others
	_notifier.commit_wait(worker._waiter);

	return true;
}

// Function: this_worker_id
int Executor::this_worker_id() const {
	auto i = _wids.find(std::this_thread::get_id());
	return i == _wids.end() ? -1 : static_cast<int>(_workers[i->second]._id);
}

// Procedure: _spawn
void Executor::_spawn(size_t N) {

	std::mutex mutex;
	std::condition_variable cond;
	size_t n = 0;

	for (size_t id = 0; id < N; ++id) {

		_workers[id]._id = id;
		_workers[id]._vtm = id;
		_workers[id]._executor = this;
		_workers[id]._waiter = &_notifier._waiters[id];

		_threads.emplace_back([this](
								  Worker& w, std::mutex& mutex, std::condition_variable& cond, size_t& n) -> void {
			// enables the mapping
			{
				std::scoped_lock lock(mutex);
				_wids[std::this_thread::get_id()] = w._id;
				if (n++; n == num_workers()) {
					cond.notify_one();
				}
			}

			//this_worker().worker = &w;

			Node* t = nullptr;

			// must use 1 as condition instead of !done
			while (1) {

				// execute the tasks.
				_exploit_task(w, t);

				// wait for tasks
				if (_wait_for_task(w, t) == false) {
					break;
				}
			}
		},
							  std::ref(_workers[id]), std::ref(mutex), std::ref(cond), std::ref(n));
	}

	std::unique_lock<std::mutex> lock(mutex);
	cond.wait(lock, [&]() { return n == N; });
}

// Function: _consume_task
void Executor::_consume_task(Worker& w, Node* p) {

	std::uniform_int_distribution<size_t> rdvtm(0, _workers.size() - 1);

	while (p->_join_counter != 0) {

	exploit:

		if (auto t = w._wsq.pop(); t) {
			_invoke(w, t);
		} else {
			size_t num_steals = 0;
			//size_t num_pauses = 0;
			size_t max_steals = ((_workers.size() + 1) << 1);

		explore:

			t = (w._id == w._vtm) ? _wsq.steal() : _workers[w._vtm]._wsq.steal();

			if (t) {
				_invoke(w, t);
				goto exploit;
			} else if (p->_join_counter != 0) {

				if (num_steals++ > max_steals) {
					//(num_pauses++ < 100) ? relax_cpu() : std::this_thread::yield();
					std::this_thread::yield();
				}

				//std::this_thread::yield();
				w._vtm = rdvtm(w._rdgen);
				goto explore;
			} else {
				break;
			}
		}
	}
}

// Function: _explore_task
void Executor::_explore_task(Worker& w, Node*& t) {

	//assert(_workers[w].wsq.empty());
	//assert(!t);

	size_t num_steals = 0;
	size_t num_yields = 0;
	size_t max_steals = ((_workers.size() + 1) << 1);

	std::uniform_int_distribution<size_t> rdvtm(0, _workers.size() - 1);

	do {
		t = (w._id == w._vtm) ? _wsq.steal() : _workers[w._vtm]._wsq.steal();

		if (t) {
			break;
		}

		if (num_steals++ > max_steals) {
			std::this_thread::yield();
			if (num_yields++ > 100) {
				break;
			}
		}

		w._vtm = rdvtm(w._rdgen);
	} while (!_done);
}

// Constructor
Taskflow::Taskflow(const std::string& name) : FlowBuilder{_graph},
											  _name{name} {
}

// Constructor
Taskflow::Taskflow() : FlowBuilder{_graph} {
}

// Move constructor
Taskflow::Taskflow(Taskflow&& rhs) : FlowBuilder{_graph} {

	std::scoped_lock<std::mutex> lock(rhs._mutex);

	_name = std::move(rhs._name);
	_graph = std::move(rhs._graph);
	_topologies = std::move(rhs._topologies);
	_satellite = rhs._satellite;

	rhs._satellite.reset();
}

// Move assignment
Taskflow& Taskflow::operator=(Taskflow&& rhs) {
	if (this != &rhs) {
		std::scoped_lock<std::mutex, std::mutex> lock(_mutex, rhs._mutex);
		_name = std::move(rhs._name);
		_graph = std::move(rhs._graph);
		_topologies = std::move(rhs._topologies);
		_satellite = rhs._satellite;
		rhs._satellite.reset();
	}
	return *this;
}

// Procedure:
void Taskflow::clear() {
	_graph._clear();
}

// Function: num_tasks
size_t Taskflow::num_tasks() const {
	return _graph.size();
}

// Function: empty
bool Taskflow::empty() const {
	return _graph.empty();
}

// Function: name
void Taskflow::name(const std::string& name) {
	_name = name;
}

// Function: name
const std::string& Taskflow::name() const {
	return _name;
}

// Function: graph
Graph& Taskflow::graph() {
	return _graph;
}

// Procedure: dump
std::string Taskflow::dump() const {
	std::ostringstream oss;
	dump(oss);
	return oss.str();
}

// Function: dump
void Taskflow::dump(std::ostream& os) const {
	os << "digraph Taskflow {\n";
	_dump(os, &_graph);
	os << "}\n";
}

// Procedure: _dump
void Taskflow::_dump(std::ostream& os, const Graph* top) const {

	Dumper dumper;

	dumper.id = 0;
	dumper.stack.push_back({nullptr, top});
	dumper.visited.Find(top).Value() = dumper.id++;

	while (!dumper.stack.empty()) {

		auto [p, f] = dumper.stack.erase_last();

		os << "subgraph cluster_p" << f << " {\nlabel=\"";

		// n-level module
		if (p) {
			os << 'm' << dumper.visited.Find(f).Value();
		}
		// top-level taskflow graph
		else {
			os << "Taskflow: ";
			if (_name.empty()) os << 'p' << this;
			else
				os << _name;
		}

		os << "\";\n";

		_dump(os, f, dumper);
		os << "}\n";
	}
}

// Procedure: _dump
void Taskflow::_dump(
	std::ostream& os, const Node* node, Dumper& dumper) const {

	os << 'p' << node << "[label=\"";
	if (node->_name.empty()) os << 'p' << node;
	else
		os << node->_name;
	os << "\" ";

	// shape for node
	switch (node->_handle.index()) {

		case Node::CONDITION:
		case Node::MULTI_CONDITION:
			os << "shape=diamond color=black fillcolor=aquamarine style=filled";
			break;

		case Node::RUNTIME:
			os << "shape=component";
			break;

		case Node::CUDAFLOW:
			os << " style=\"filled\""
			   << " color=\"black\" fillcolor=\"purple\""
			   << " fontcolor=\"white\""
			   << " shape=\"folder\"";
			break;

		case Node::SYCLFLOW:
			os << " style=\"filled\""
			   << " color=\"black\" fillcolor=\"red\""
			   << " fontcolor=\"white\""
			   << " shape=\"folder\"";
			break;

		default:
			break;
	}

	os << "];\n";

	for (size_t s = 0; s < node->_successors.size(); ++s) {
		if (node->_is_conditioner()) {
			// case edge is dashed
			os << 'p' << node << " -> p" << node->_successors[s]
			   << " [style=dashed label=\"" << s << "\"];\n";
		} else {
			os << 'p' << node << " -> p" << node->_successors[s] << ";\n";
		}
	}

	// subflow join node
	if (node->_parent && node->_parent->_handle.index() == Node::DYNAMIC && node->_successors.size() == 0) {
		os << 'p' << node << " -> p" << node->_parent << ";\n";
	}

	// node info
	switch (node->_handle.index()) {

		case Node::DYNAMIC: {
			auto& sbg = std::get_if<Node::Dynamic>(&node->_handle)->subgraph;
			if (!sbg.empty()) {
				os << "subgraph cluster_p" << node << " {\nlabel=\"Subflow: ";
				if (node->_name.empty()) os << 'p' << node;
				else
					os << node->_name;

				os << "\";\n"
				   << "color=blue\n";
				_dump(os, &sbg, dumper);
				os << "}\n";
			}
		} break;

		case Node::CUDAFLOW: {
			std::get_if<Node::cudaFlow>(&node->_handle)->graph->dump(os, node, node->_name);
		} break;

		case Node::SYCLFLOW: {
			std::get_if<Node::syclFlow>(&node->_handle)->graph->dump(os, node, node->_name);
		} break;

		default:
			break;
	}
}

// Procedure: _dump
void Taskflow::_dump(
	std::ostream& os, const Graph* graph, Dumper& dumper) const {

	for (const auto& n : graph->_nodes) {

		// regular task
		if (n->_handle.index() != Node::MODULE) {
			_dump(os, n, dumper);
		}
		// module task
		else {
			//auto module = &(std::get_if<Node::Module>(&n->_handle)->module);
			auto module = &(std::get_if<Node::Module>(&n->_handle)->graph);

			os << 'p' << n << "[shape=box3d, color=blue, label=\"";
			if (n->_name.empty()) os << 'p' << n;
			else
				os << n->_name;
			auto ite = dumper.visited.Emplace(
				module,
				vstd::MakeLazyEval([&] {
					dumper.stack.push_back({n, module});
					return dumper.id++;
				}));

			os << " [m" << ite.Value() << "]\"];\n";

			for (const auto s : n->_successors) {
				os << 'p' << n << "->" << 'p' << s << ";\n";
			}
		}
	}
}
Semaphore::~Semaphore() {}

Semaphore::Semaphore(size_t max_workers) : _counter(max_workers) {
}

bool Semaphore::_try_acquire_or_wait(Node* me) {
	std::lock_guard<std::mutex> lock(_mtx);
	if (_counter > 0) {
		--_counter;
		return true;
	} else {
		_waiters.push_back(me);
		return false;
	}
}

vstd::vector<Node*> Semaphore::_release() {
	std::lock_guard<std::mutex> lock(_mtx);
	++_counter;
	vstd::vector<Node*> r{std::move(_waiters)};
	return r;
}

size_t Semaphore::count() const {
	return _counter;
}
Taskflow::~Taskflow() {}

// Destructor
Node::~Node() {
	// this is to avoid stack overflow

	if (_handle.index() == DYNAMIC) {
		// using std::get_if instead of std::get makes this compatible
		// with older macOS versions
		// the result of std::get_if is guaranteed to be non-null
		// due to the index check above
		auto& subgraph = std::get_if<Dynamic>(&_handle)->subgraph;
		vstd::vector<Node*> nodes;
		nodes.reserve(subgraph.size());

		std::move(
			subgraph._nodes.begin(), subgraph._nodes.end(), std::back_inserter(nodes));
		subgraph._nodes.clear();

		size_t i = 0;

		while (i < nodes.size()) {

			if (nodes[i]->_handle.index() == DYNAMIC) {
				auto& sbg = std::get_if<Dynamic>(&(nodes[i]->_handle))->subgraph;
				std::move(
					sbg._nodes.begin(), sbg._nodes.end(), std::back_inserter(nodes));
				sbg._nodes.clear();
			}

			++i;
		}

		//auto& np = Graph::_node_pool();
		for (i = 0; i < nodes.size(); ++i) {
			node_pool.recycle(nodes[i]);
		}
	}
}

// Procedure: _precede
void Node::_precede(Node* v) {
	_successors.push_back(v);
	v->_dependents.push_back(this);
}

// Function: num_successors
size_t Node::num_successors() const {
	return _successors.size();
}

// Function: dependents
size_t Node::num_dependents() const {
	return _dependents.size();
}

// Function: num_weak_dependents
size_t Node::num_weak_dependents() const {
	size_t n = 0;
	for (size_t i = 0; i < _dependents.size(); i++) {
		//if(_dependents[i]->_handle.index() == Node::CONDITION) {
		if (_dependents[i]->_is_conditioner()) {
			n++;
		}
	}
	return n;
}

// Function: num_strong_dependents
size_t Node::num_strong_dependents() const {
	size_t n = 0;
	for (size_t i = 0; i < _dependents.size(); i++) {
		//if(_dependents[i]->_handle.index() != Node::CONDITION) {
		if (!_dependents[i]->_is_conditioner()) {
			n++;
		}
	}
	return n;
}

// Function: name
const std::string& Node::name() const {
	return _name;
}

// Function: _is_conditioner
bool Node::_is_conditioner() const {
	return _handle.index() == Node::CONDITION || _handle.index() == Node::MULTI_CONDITION;
}

// Function: _is_cancelled
bool Node::_is_cancelled() const {
	if (_handle.index() == Node::ASYNC) {
		auto h = std::get_if<Node::Async>(&_handle);
		if (h->topology && h->topology->_is_cancelled.load(std::memory_order_relaxed)) {
			return true;
		}
		// async tasks spawned from subflow does not have topology
	}
	return _topology && _topology->_is_cancelled.load(std::memory_order_relaxed);
}

// Procedure: _set_up_join_counter
void Node::_set_up_join_counter() {
	size_t c = 0;
	for (auto p : _dependents) {
		//if(p->_handle.index() == Node::CONDITION) {
		if (p->_is_conditioner()) {
			_state.fetch_or(Node::CONDITIONED, std::memory_order_relaxed);
		} else {
			c++;
		}
	}
	_join_counter.store(c, std::memory_order_release);
}

// Function: _acquire_all
bool Node::_acquire_all(SmallVector<Node*>& nodes) {

	auto& to_acquire = _semaphores->to_acquire;

	for (size_t i = 0; i < to_acquire.size(); ++i) {
		if (!to_acquire[i]->_try_acquire_or_wait(this)) {
			for (size_t j = 1; j <= i; ++j) {
				auto r = to_acquire[i - j]->_release();
				nodes.insert(std::end(nodes), std::begin(r), std::end(r));
			}
			return false;
		}
	}
	return true;
}

// Function: _release_all
SmallVector<Node*> Node::_release_all() {

	auto& to_release = _semaphores->to_release;

	SmallVector<Node*> nodes;
	for (const auto& sem : to_release) {
		auto r = sem->_release();
		nodes.insert(std::end(nodes), std::begin(r), std::end(r));
	}

	return nodes;
}

// ----------------------------------------------------------------------------
// Graph definition
// ----------------------------------------------------------------------------

// Destructor
Graph::~Graph() {
	_clear();
}

// Move constructor
Graph::Graph(Graph&& other) : _nodes{std::move(other._nodes)} {
}

// Move assignment
Graph& Graph::operator=(Graph&& other) {
	_clear();
	_nodes = std::move(other._nodes);
	return *this;
}

// Procedure: clear
void Graph::clear() {
	_clear();
}

// Procedure: clear
void Graph::_clear() {
	for (auto node : _nodes) {
		node_pool.recycle(node);
	}
	_nodes.clear();
}

// Procedure: clear_detached
void Graph::_clear_detached() {

	auto mid = std::partition(_nodes.begin(), _nodes.end(), [](Node* node) {
		return !(node->_state.load(std::memory_order_relaxed) & Node::DETACHED);
	});

	for (auto itr = mid; itr != _nodes.end(); ++itr) {
		node_pool.recycle(*itr);
	}
	_nodes.resize(std::distance(_nodes.begin(), mid));
}

// Procedure: merge
void Graph::_merge(Graph&& g) {
	for (auto n : g._nodes) {
		_nodes.push_back(n);
	}
	g._nodes.clear();
}

// Function: erase
void Graph::_erase(Node* node) {
	if (auto I = std::find(_nodes.begin(), _nodes.end(), node); I != _nodes.end()) {
		_nodes.erase(I);
		node_pool.recycle(node);
	}
}

// Function: size
size_t Graph::size() const {
	return _nodes.size();
}

// Function: empty
bool Graph::empty() const {
	return _nodes.empty();
}

// Function: emplace_back

// Function: emplace_back
Node* Graph::_emplace_back() {
	_nodes.push_back(node_pool.animate());
	return _nodes.back();
}

/**
@brief convert an observer type to a human-readable string
*/
const char* to_string(ObserverType type) {
	switch (type) {
		case ObserverType::TFPROF: return "tfprof";
		case ObserverType::CHROME: return "chrome";
		default: return "undefined";
	}
}
// Procedure: set_up
void TFProfObserver::set_up(size_t num_workers) {
	_timeline.uid = unique_id<size_t>();
	_timeline.origin = observer_stamp_t::clock::now();
	_timeline.segments.resize(num_workers);
	_stacks.resize(num_workers);
}

// Procedure: on_entry
void TFProfObserver::on_entry(WorkerView wv, TaskView) {
	_stacks[wv.id()].push(observer_stamp_t::clock::now());
}

// Procedure: on_exit
void TFProfObserver::on_exit(WorkerView wv, TaskView tv) {

	size_t w = wv.id();

	assert(!_stacks[w].empty());

	if (_stacks[w].size() > _timeline.segments[w].size()) {
		_timeline.segments[w].resize(_stacks[w].size());
	}

	auto beg = _stacks[w].top();
	_stacks[w].pop();

	_timeline.segments[w][_stacks[w].size()].emplace_back(
		tv.name(), tv.type(), beg, observer_stamp_t::clock::now());
}

// Function: clear
void TFProfObserver::clear() {
	for (size_t w = 0; w < _timeline.segments.size(); ++w) {
		for (size_t l = 0; l < _timeline.segments[w].size(); ++l) {
			_timeline.segments[w][l].clear();
		}
		while (!_stacks[w].empty()) {
			_stacks[w].pop();
		}
	}
}

// Procedure: dump
void TFProfObserver::dump(std::ostream& os) const {

	size_t first;

	for (first = 0; first < _timeline.segments.size(); ++first) {
		if (_timeline.segments[first].size() > 0) {
			break;
		}
	}

	// not timeline data to dump
	if (first == _timeline.segments.size()) {
		os << "{}\n";
		return;
	}

	os << "{\"executor\":\"" << _timeline.uid << "\",\"data\":[";

	bool comma = false;

	for (size_t w = first; w < _timeline.segments.size(); w++) {
		for (size_t l = 0; l < _timeline.segments[w].size(); l++) {

			if (_timeline.segments[w][l].empty()) {
				continue;
			}

			if (comma) {
				os << ',';
			} else {
				comma = true;
			}

			os << "{\"worker\":" << w << ",\"level\":" << l << ",\"data\":[";
			for (size_t i = 0; i < _timeline.segments[w][l].size(); ++i) {

				const auto& s = _timeline.segments[w][l][i];

				if (i) os << ',';

				// span
				os << "{\"span\":["
				   << std::chrono::duration_cast<std::chrono::microseconds>(
						  s.beg - _timeline.origin)
						  .count()
				   << ","
				   << std::chrono::duration_cast<std::chrono::microseconds>(
						  s.end - _timeline.origin)
						  .count()
				   << "],";

				// name
				os << "\"name\":\"";
				if (s.name.empty()) {
					os << w << '_' << i;
				} else {
					os << s.name;
				}
				os << "\",";

				// category "type": "Condition Task",

				os << "}";
			}
			os << "]}";
		}
	}

	os << "]}\n";
}

// Function: dump
std::string TFProfObserver::dump() const {
	std::ostringstream oss;
	dump(oss);
	return oss.str();
}

// Function: num_tasks
size_t TFProfObserver::num_tasks() const {
	return std::accumulate(
		_timeline.segments.begin(), _timeline.segments.end(), size_t{0},
		[](size_t sum, const auto& exe) {
			return sum + exe.size();
		});
}

// ----------------------------------------------------------------------------
// TFProfManager
// ----------------------------------------------------------------------------

/**
@private
*/

// constructor
TFProfManager::TFProfManager() : _fpath{get_env(TF_ENABLE_PROFILER)} {
}

// Procedure: manage
void TFProfManager::_manage(std::shared_ptr<TFProfObserver> observer) {
	std::lock_guard lock(_mutex);
	_observers.push_back(std::move(observer));
}

// Procedure: dump
void TFProfManager::dump(std::ostream& os) const {
	for (size_t i = 0; i < _observers.size(); ++i) {
		if (i) os << ',';
		_observers[i]->dump(os);
	}
}

// Destructor
TFProfManager::~TFProfManager() {
	std::ofstream ofs(_fpath);
	if (ofs) {
		//// .tfp
		//if(_fpath.rfind(".tfp") != std::string::npos) {
		//  ProfileData data;
		//  data.timelines.reserve(_observers.size());
		//  for(size_t i=0; i<_observers.size(); ++i) {
		//    data.timelines.push_back(std::move(_observers[i]->_timeline));
		//  }
		//  Serializer<std::ofstream> serializer(ofs);
		//  serializer(data);
		//}
		//// .json
		//else {
		//  ofs << "[\n";
		//  for(size_t i=0; i<_observers.size(); ++i) {
		//    if(i) ofs << ',';
		//    _observers[i]->dump(ofs);
		//  }
		//  ofs << "]\n";
		//}

		ofs << "[\n";
		for (size_t i = 0; i < _observers.size(); ++i) {
			if (i) ofs << ',';
			_observers[i]->dump(ofs);
		}
		ofs << "]\n";
	}
}

// Function: get
TFProfManager& TFProfManager::get() {
	static TFProfManager mgr;
	return mgr;
}
// Constructor
Task::Task(Node* node) : _node{node} {
}

// Constructor
Task::Task(const Task& rhs) : _node{rhs._node} {
}

/**
@brief convert a task type to a human-readable string

The name of each task type is the litte-case string of its characters.

@code{.cpp}
TaskType::PLACEHOLDER     ->  "placeholder"
TaskType::CUDAFLOW        ->  "cudaflow"
TaskType::SYCLFLOW        ->  "syclflow"
TaskType::STATIC          ->  "static"
TaskType::DYNAMIC         ->  "subflow"
TaskType::CONDITION       ->  "condition"
TaskType::MULTI_CONDITION ->  "multi_condition"
TaskType::MODULE          ->  "module"
TaskType::ASYNC           ->  "async"
TaskType::RUNTIME         ->  "runtime"
@endcode
*/
const char* to_string(TaskType type) {

	const char* val;

	switch (type) {
		case TaskType::PLACEHOLDER: val = "placeholder"; break;
		case TaskType::CUDAFLOW: val = "cudaflow"; break;
		case TaskType::SYCLFLOW: val = "syclflow"; break;
		case TaskType::STATIC: val = "static"; break;
		case TaskType::DYNAMIC: val = "subflow"; break;
		case TaskType::CONDITION: val = "condition"; break;
		case TaskType::MULTI_CONDITION: val = "multi_condition"; break;
		case TaskType::MODULE: val = "module"; break;
		case TaskType::ASYNC: val = "async"; break;
		case TaskType::RUNTIME: val = "runtime"; break;
		default: val = "undefined"; break;
	}

	return val;
}

// Operator =
Task& Task::operator=(const Task& rhs) {
	_node = rhs._node;
	return *this;
}

// Operator =
Task& Task::operator=(std::nullptr_t ptr) {
	_node = ptr;
	return *this;
}

// Operator ==
bool Task::operator==(const Task& rhs) const {
	return _node == rhs._node;
}

// Operator !=
bool Task::operator!=(const Task& rhs) const {
	return _node != rhs._node;
}

// Function: name
Task& Task::name(const std::string& name) {
	_node->_name = name;
	return *this;
}

// Function: acquire
Task& Task::acquire(Semaphore& s) {
	if (!_node->_semaphores) {
		_node->_semaphores = std::make_unique<Node::Semaphores>();
	}
	_node->_semaphores->to_acquire.push_back(&s);
	return *this;
}

// Function: release
Task& Task::release(Semaphore& s) {
	if (!_node->_semaphores) {
		//_node->_semaphores.emplace();
		_node->_semaphores = std::make_unique<Node::Semaphores>();
	}
	_node->_semaphores->to_release.push_back(&s);
	return *this;
}

// Procedure: reset
void Task::reset() {
	_node = nullptr;
}

// Procedure: reset_work
void Task::reset_work() {
	_node->_handle.emplace<std::monostate>();
}

// Function: name
const std::string& Task::name() const {
	return _node->_name;
}

// Function: num_dependents
size_t Task::num_dependents() const {
	return _node->num_dependents();
}

// Function: num_strong_dependents
size_t Task::num_strong_dependents() const {
	return _node->num_strong_dependents();
}

// Function: num_weak_dependents
size_t Task::num_weak_dependents() const {
	return _node->num_weak_dependents();
}

// Function: num_successors
size_t Task::num_successors() const {
	return _node->num_successors();
}

// Function: empty
bool Task::empty() const {
	return _node == nullptr;
}

// Function: has_work
bool Task::has_work() const {
	return _node ? _node->_handle.index() != 0 : false;
}

// Function: hash_value
size_t Task::hash_value() const {
	return std::hash<Node*>{}(_node);
}

// Procedure: dump
void Task::dump(std::ostream& os) const {
	os << "task ";
	if (name().empty()) os << _node;
	else
		os << name();
	os << " [type=" << to_string(type()) << ']';
}
// Function: name
void* Task::data() const {
	return _node->_data;
}

// Function: name
Task& Task::data(void* data) {
	_node->_data = data;
	return *this;
}

// ----------------------------------------------------------------------------
// global ostream
// ----------------------------------------------------------------------------

/**
@brief overload of ostream inserter operator for cudaTask
*/
std::ostream& operator<<(std::ostream& os, const Task& task) {
	task.dump(os);
	return os;
}

// Constructor
TaskView::TaskView(const Node& node) : _node{node} {
}

// Function: name
const std::string& TaskView::name() const {
	return _node._name;
}

// Function: num_dependents
size_t TaskView::num_dependents() const {
	return _node.num_dependents();
}

// Function: num_strong_dependents
size_t TaskView::num_strong_dependents() const {
	return _node.num_strong_dependents();
}

// Function: num_weak_dependents
size_t TaskView::num_weak_dependents() const {
	return _node.num_weak_dependents();
}

// Function: num_successors
size_t TaskView::num_successors() const {
	return _node.num_successors();
}

// Function: type
TaskType TaskView::type() const {
	switch (_node._handle.index()) {
		case Node::PLACEHOLDER: return TaskType::PLACEHOLDER;
		case Node::STATIC: return TaskType::STATIC;
		case Node::DYNAMIC: return TaskType::DYNAMIC;
		case Node::CONDITION: return TaskType::CONDITION;
		case Node::MULTI_CONDITION: return TaskType::MULTI_CONDITION;
		case Node::MODULE: return TaskType::MODULE;
		case Node::ASYNC: return TaskType::ASYNC;
		case Node::SILENT_ASYNC: return TaskType::ASYNC;
		case Node::CUDAFLOW: return TaskType::CUDAFLOW;
		case Node::SYCLFLOW: return TaskType::SYCLFLOW;
		case Node::RUNTIME: return TaskType::RUNTIME;
		default: return TaskType::UNDEFINED;
	}
}

// Function: hash_value
size_t TaskView::hash_value() const {
	return std::hash<const Node*>{}(&_node);
}
TaskType Task::type() const {
	switch (_node->_handle.index()) {
		case Node::PLACEHOLDER: return TaskType::PLACEHOLDER;
		case Node::STATIC: return TaskType::STATIC;
		case Node::DYNAMIC: return TaskType::DYNAMIC;
		case Node::CONDITION: return TaskType::CONDITION;
		case Node::MULTI_CONDITION: return TaskType::MULTI_CONDITION;
		case Node::MODULE: return TaskType::MODULE;
		case Node::ASYNC: return TaskType::ASYNC;
		case Node::SILENT_ASYNC: return TaskType::ASYNC;
		case Node::CUDAFLOW: return TaskType::CUDAFLOW;
		case Node::SYCLFLOW: return TaskType::SYCLFLOW;
		case Node::RUNTIME: return TaskType::RUNTIME;
		default: return TaskType::UNDEFINED;
	}
}
// constructor
Runtime::Runtime(Executor& e, Worker& w, Node* p) : _executor{e},
													_worker{w},
													_parent{p} {
}

// Function: executor
Executor& Runtime::executor() {
	return _executor;
}

// Constructor
Subflow::Subflow(
	Executor& executor, Worker& worker, Node* parent, Graph& graph) : FlowBuilder{graph},
																	  _executor{executor},
																	  _worker{worker},
																	  _parent{parent} {
	// assert(_parent != nullptr);
}

// Function: joined
bool Subflow::joinable() const noexcept {
	return _joinable;
}

// Function: executor
Executor& Subflow::executor() {
	return _executor;
}

// Procedure: reset
void Subflow::reset() {
	_graph._clear();
	_joinable = true;
}

// Procedure: linearize
void FlowBuilder::linearize(vstd::vector<Task>& keys) {
	_linearize(keys);
}

// Procedure: linearize
void FlowBuilder::linearize(std::initializer_list<Task> keys) {
	_linearize(keys);
}
// Function: placeholder
Task FlowBuilder::placeholder() {
	auto node = _graph._emplace_back();
	return Task(node);
}

// Function: erase
void FlowBuilder::erase(Task task) {

	if (!task._node) {
		return;
	}

	task.for_each_dependent([&](Task dependent) {
		auto& S = dependent._node->_successors;
		if (auto I = std::find(S.begin(), S.end(), task._node); I != S.end()) {
			S.erase(I);
		}
	});

	task.for_each_successor([&](Task dependent) {
		auto& D = dependent._node->_dependents;
		if (auto I = std::find(D.begin(), D.end(), task._node); I != D.end()) {
			D.erase(I);
		}
	});

	_graph._erase(task._node);
}
Notifier::Notifier(size_t N) : _waiters{N} {
	assert(_waiters.size() < (1 << kWaiterBits) - 1);
	// Initialize epoch to something close to overflow to test overflow.
	_state = kStackMask | (kEpochMask - kEpochInc * _waiters.size() * 2);
}

Notifier::~Notifier() {
	// Ensure there are no waiters.
	assert((_state.load() & (kStackMask | kWaiterMask)) == kStackMask);
}

// prepare_wait prepares for waiting.
// After calling this function the thread must re-check the wait predicate
// and call either cancel_wait or commit_wait passing the same Waiter object.
void Notifier::prepare_wait(Waiter* w) {
	w->epoch = _state.fetch_add(kWaiterInc, std::memory_order_relaxed);
	std::atomic_thread_fence(std::memory_order_seq_cst);
}

// commit_wait commits waiting.
void Notifier::commit_wait(Waiter* w) {
	w->state = Waiter::kNotSignaled;
	// Modification epoch of this waiter.
	uint64_t epoch =
		(w->epoch & kEpochMask) + (((w->epoch & kWaiterMask) >> kWaiterShift) << kEpochShift);
	uint64_t state = _state.load(std::memory_order_seq_cst);
	for (;;) {
		if (int64_t((state & kEpochMask) - epoch) < 0) {
			// The preceeding waiter has not decided on its fate. Wait until it
			// calls either cancel_wait or commit_wait, or is notified.
			std::this_thread::yield();
			state = _state.load(std::memory_order_seq_cst);
			continue;
		}
		// We've already been notified.
		if (int64_t((state & kEpochMask) - epoch) > 0) return;
		// Remove this thread from prewait counter and add it to the waiter list.
		assert((state & kWaiterMask) != 0);
		uint64_t newstate = state - kWaiterInc + kEpochInc;
		//newstate = (newstate & ~kStackMask) | (w - &_waiters[0]);
		newstate = static_cast<uint64_t>((newstate & ~kStackMask) | static_cast<uint64_t>(w - &_waiters[0]));
		if ((state & kStackMask) == kStackMask)
			w->next.store(nullptr, std::memory_order_relaxed);
		else
			w->next.store(&_waiters[state & kStackMask], std::memory_order_relaxed);
		if (_state.compare_exchange_weak(state, newstate,
										 std::memory_order_release))
			break;
	}
	_park(w);
}

// cancel_wait cancels effects of the previous prepare_wait call.
void Notifier::cancel_wait(Waiter* w) {
	uint64_t epoch =
		(w->epoch & kEpochMask) + (((w->epoch & kWaiterMask) >> kWaiterShift) << kEpochShift);
	uint64_t state = _state.load(std::memory_order_relaxed);
	for (;;) {
		if (int64_t((state & kEpochMask) - epoch) < 0) {
			// The preceeding waiter has not decided on its fate. Wait until it
			// calls either cancel_wait or commit_wait, or is notified.
			std::this_thread::yield();
			state = _state.load(std::memory_order_relaxed);
			continue;
		}
		// We've already been notified.
		if (int64_t((state & kEpochMask) - epoch) > 0) return;
		// Remove this thread from prewait counter.
		assert((state & kWaiterMask) != 0);
		if (_state.compare_exchange_weak(state, state - kWaiterInc + kEpochInc,
										 std::memory_order_relaxed))
			return;
	}
}

// notify wakes one or all waiting threads.
// Must be called after changing the associated wait predicate.
void Notifier::notify(bool all) {
	std::atomic_thread_fence(std::memory_order_seq_cst);
	uint64_t state = _state.load(std::memory_order_acquire);
	for (;;) {
		// Easy case: no waiters.
		if ((state & kStackMask) == kStackMask && (state & kWaiterMask) == 0)
			return;
		uint64_t waiters = (state & kWaiterMask) >> kWaiterShift;
		uint64_t newstate;
		if (all) {
			// Reset prewait counter and empty wait list.
			newstate = (state & kEpochMask) + (kEpochInc * waiters) + kStackMask;
		} else if (waiters) {
			// There is a thread in pre-wait state, unblock it.
			newstate = state + kEpochInc - kWaiterInc;
		} else {
			// Pop a waiter from list and unpark it.
			Waiter* w = &_waiters[state & kStackMask];
			Waiter* wnext = w->next.load(std::memory_order_relaxed);
			uint64_t next = kStackMask;
			//if (wnext != nullptr) next = wnext - &_waiters[0];
			if (wnext != nullptr) next = static_cast<uint64_t>(wnext - &_waiters[0]);
			// Note: we don't add kEpochInc here. ABA problem on the lock-free stack
			// can't happen because a waiter is re-pushed onto the stack only after
			// it was in the pre-wait state which inevitably leads to epoch
			// increment.
			newstate = (state & kEpochMask) + next;
		}
		if (_state.compare_exchange_weak(state, newstate,
										 std::memory_order_acquire)) {
			if (!all && waiters) return;// unblocked pre-wait thread
			if ((state & kStackMask) == kStackMask) return;
			Waiter* w = &_waiters[state & kStackMask];
			if (!all) w->next.store(nullptr, std::memory_order_relaxed);
			_unpark(w);
			return;
		}
	}
}

// notify n workers
void Notifier::notify_n(size_t n) {
	if (n >= _waiters.size()) {
		notify(true);
	} else {
		for (size_t k = 0; k < n; ++k) {
			notify(false);
		}
	}
}

size_t Notifier::size() const {
	return _waiters.size();
}
void Notifier::_park(Waiter* w) {
	std::unique_lock<std::mutex> lock(w->mu);
	while (w->state != Waiter::kSignaled) {
		w->state = Waiter::kWaiting;
		w->cv.wait(lock);
	}
}

void Notifier::_unpark(Waiter* waiters) {
	Waiter* next = nullptr;
	for (Waiter* w = waiters; w; w = next) {
		next = w->next.load(std::memory_order_relaxed);
		unsigned state;
		{
			std::unique_lock<std::mutex> lock(w->mu);
			state = w->state;
			w->state = Waiter::kSignaled;
		}
		// Avoid notifying if it wasn't waiting.
		if (state == Waiter::kWaiting) w->cv.notify_one();
	}
}

// constructor
ChromeObserver::Segment::Segment(
	const std::string& n, observer_stamp_t b, observer_stamp_t e) : name{n}, beg{b}, end{e} {
}

// Procedure: set_up
void ChromeObserver::set_up(size_t num_workers) {
	_timeline.segments.resize(num_workers);
	_timeline.stacks.resize(num_workers);

	for (size_t w = 0; w < num_workers; ++w) {
		_timeline.segments[w].reserve(32);
	}

	_timeline.origin = observer_stamp_t::clock::now();
}

// Procedure: on_entry
void ChromeObserver::on_entry(WorkerView wv, TaskView) {
	_timeline.stacks[wv.id()].push(observer_stamp_t::clock::now());
}

// Procedure: on_exit
void ChromeObserver::on_exit(WorkerView wv, TaskView tv) {

	size_t w = wv.id();

	assert(!_timeline.stacks[w].empty());

	auto beg = _timeline.stacks[w].top();
	_timeline.stacks[w].pop();

	_timeline.segments[w].emplace_back(
		tv.name(), beg, observer_stamp_t::clock::now());
}

// Function: clear
void ChromeObserver::clear() {
	for (size_t w = 0; w < _timeline.segments.size(); ++w) {
		_timeline.segments[w].clear();
		while (!_timeline.stacks[w].empty()) {
			_timeline.stacks[w].pop();
		}
	}
}

// Procedure: dump
void ChromeObserver::dump(std::ostream& os) const {

	size_t first;

	for (first = 0; first < _timeline.segments.size(); ++first) {
		if (_timeline.segments[first].size() > 0) {
			break;
		}
	}

	os << '[';

	for (size_t w = first; w < _timeline.segments.size(); w++) {

		if (w != first && _timeline.segments[w].size() > 0) {
			os << ',';
		}

		for (size_t i = 0; i < _timeline.segments[w].size(); i++) {

			os << '{'
			   << "\"cat\":\"ChromeObserver\",";

			// name field
			os << "\"name\":\"";
			if (_timeline.segments[w][i].name.empty()) {
				os << w << '_' << i;
			} else {
				os << _timeline.segments[w][i].name;
			}
			os << "\",";

			// segment field
			os << "\"ph\":\"X\","
			   << "\"pid\":1,"
			   << "\"tid\":" << w << ','
			   << "\"ts\":" << std::chrono::duration_cast<std::chrono::microseconds>(_timeline.segments[w][i].beg - _timeline.origin).count() << ','
			   << "\"dur\":" << std::chrono::duration_cast<std::chrono::microseconds>(_timeline.segments[w][i].end - _timeline.segments[w][i].beg).count();

			if (i != _timeline.segments[w].size() - 1) {
				os << "},";
			} else {
				os << '}';
			}
		}
	}
	os << "]\n";
}

// Function: dump
std::string ChromeObserver::dump() const {
	std::ostringstream oss;
	dump(oss);
	return oss.str();
}

// Function: num_tasks
size_t ChromeObserver::num_tasks() const {
	return std::accumulate(
		_timeline.segments.begin(), _timeline.segments.end(), size_t{0},
		[](size_t sum, const auto& exe) {
			return sum + exe.size();
		});
}
}// namespace tf