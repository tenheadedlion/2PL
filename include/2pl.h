#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <random>
#include <shared_mutex>
#include <thread>
#include <tuple>
#include <vector>

// runtime, a lot of work depends on it
class DB {
public:
  DB(size_t cap) : cap_(cap) {
    mutexes_ = std::vector<std::shared_mutex>(cap_);
    storage_.resize(cap_, 1);
  };
  int get(size_t i) const { return storage_[i % cap_]; }
  std::shared_mutex &get_mutex(size_t i) const { return mutexes_[i % cap_]; }
  void update(size_t j, int value) { storage_[j] = value; }
  int resolve_rw_conflict(size_t i, size_t j) {
    if (j >= i && j <= i + 2) {
      return j - i;
    } else if (i + 2 >= cap_ && (i + 2) % cap_ >= j) {
      return (j + cap_ - i);
    }
    return -1;
  }
  size_t rand() {

    static bool init = false;
    static std::random_device rd;
    static std::mt19937 eng;
    static std::uniform_int_distribution<int> dist(0, cap_ - 1);
    if (!init) {
      eng.seed(rd());
      init = true;
    }
    size_t r = dist(eng);
    return r;
  }

  void dump() {
    int sum = 0;
    std::map<size_t, size_t> map;
    for (auto i : storage_) {
      if (map.find(i) == map.end()) {
        map[i] = 1;
      } else {
        map[i] += 1;
      }
    }
    for (auto i : map) {
      std::cout << i.first << ": " << i.second << std::endl;
    }
  }

private:
  size_t cap_;
  mutable std::shared_mutex mutex_;
  std::vector<unsigned int> storage_;
  mutable std::vector<std::shared_mutex> mutexes_;
};

class Operation {
public:
  virtual int run(DB &db, int arg) = 0;
  virtual void rollback(DB &db) = 0;
  virtual void release_lock() = 0;
};

class Transaction {
public:
  Transaction(DB &db) : db(db), progress_(0), pipeline(0) {}
  void reset() {
    progress_ = 0;
    pipeline = 0;
  }
  void add(Operation *op) { operations.push_back(op); }
  bool run() {
    bool rollback = false;
    // the growing phase happens here, where new locks are acquired but none can
    // be released
    for (progress_ = 0; progress_ < operations.size(); ++progress_) {
      auto res = operations[progress_]->run(db, pipeline);
      if (res == -1) {
        rollback = true;
        break;
      }
      pipeline = res;
    }
    // the shrinking phase happens here
    if (rollback) {
      for (; progress_ >= 0; --progress_) {
        operations[progress_]->rollback(db);
      }
      reset();
      return false;
    }
    for (--progress_; progress_ >= 0; --progress_) {
      operations[progress_]->release_lock();
    }
    return true;
  }

private:
  Transaction() = delete;
  DB &db;
  // to simulate a pipeline
  // the output of the previous operation is passed as input to the next
  // opertion
  int pipeline;
  int progress_;
  std::vector<Operation *> operations;
};

class Read : public Operation {
public:
  Read(size_t start, size_t except) : i(start), j(except) {}
  ~Read() { release_lock(); }
  int run(DB &db, int arg) override {
    (void)arg;
    // std::vector<std::shared_lock<std::shared_mutex>> locks;
    locks.push_back(std::shared_lock(db.get_mutex(i)));
    locks.push_back(std::shared_lock(db.get_mutex(i + 1)));
    locks.push_back(std::shared_lock(db.get_mutex(i + 2)));
    // a transaction can run into deadlock itself
    // eg.: reading 0, 1, 2, attempt to write to 2
    int self_dead_pos = db.resolve_rw_conflict(i, j);
    if (self_dead_pos != -1) {
      locks[self_dead_pos].unlock();
    }
    return db.get(i) + db.get((i + 1)) + db.get((i + 2));
  }
  void rollback(DB &db) override { release_lock(); }
  void release_lock() override {
    while (!locks.empty()) {
      auto &lock = locks.back();
      if (lock.owns_lock()) {
        lock.unlock();
        lock.release();
      }
      locks.pop_back();
    }
  }

private:
  std::vector<std::shared_lock<std::shared_mutex>> locks;
  size_t i;
  size_t j;
};

class Write : public Operation {
public:
  Write(size_t j) : j(j) {}
  ~Write() { release_lock(); }
  int run(DB &db, int arg) override {
    lock_x = std::unique_lock(db.get_mutex(j), std::defer_lock);
    // a deadlock can occurs in this place
    // for example:
    //    transaction 1:  0, 1, 2 | 4
    //    transaction 2:  4, 5, 6 | 1
    // transaction 1 is holding 0, 1, 2 read lock, trying to grap 4's write
    // lock, however, 4's read lock is held by transaction 2, which is
    // attempting to lock 1, neither transaction can proceed. To solve the
    // problem, when the first transaction fails to acquire the write lock, it
    // should give up immediately, then rollback and restart
    if (!lock_x.try_lock()) {
      return -1;
    }
    prev_value = db.get(j);
    db.update(j, arg);
    return 0;
  }
  // rollback function, resume the previous value
  void rollback(DB &db) override {
    db.update(j, prev_value);
    release_lock();
  }
  void release_lock() override {
    if (lock_x.owns_lock()) {
      lock_x.unlock();
      lock_x.release();
    }
  }

private:
  int prev_value;
  std::unique_lock<std::shared_mutex> lock_x;
  size_t j;
};
