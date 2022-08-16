#include "2pl.h"

class timer {
public:
  std::chrono::time_point<std::chrono::high_resolution_clock> lastTime;
  timer() : lastTime(std::chrono::high_resolution_clock::now()) {}
  inline double elapsed() {
    std::chrono::time_point<std::chrono::high_resolution_clock> thisTime =
        std::chrono::high_resolution_clock::now();
    double deltaTime =
        std::chrono::duration<double>(thisTime - lastTime).count();
    lastTime = thisTime;
    return deltaTime;
  }
};

std::mutex log_mutex;

int main(int argc, char **argv) {
  int capacity = std::stoi(argv[1]);
  int repetition = std::stoi(argv[2]);
  int num_workers = std::stoi(argv[3]);

  DB db(capacity);

  auto do_work = [&db, &repetition]() {
    timer stopwatch;
    int retry_cnt = 0;
    for (size_t k = 0; k < repetition; k++) {
      size_t i = db.rand();
      size_t j = db.rand();
      Transaction transaction(db);
      Read read(i, j);
      Write write(j);
      transaction.add(&read);
      transaction.add(&write);
      while (!transaction.run()) {
        ++retry_cnt;
      }
    }
    double time = stopwatch.elapsed() / (repetition + retry_cnt);
    std::lock_guard guard(log_mutex);
    std::cout.precision(10);
    std::cout << std::fixed << "transaction time: " << time
              << ", retry: " << retry_cnt << std::endl;
  };

  std::vector<std::thread> threads;

  for (int i = 0; i < num_workers; ++i) {
    threads.push_back(std::thread(do_work));
  }

  for (int i = 0; i < num_workers; ++i) {
    threads[i].join();
  }
  db.dump();
}