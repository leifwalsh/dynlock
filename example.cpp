#include "dynlock.hpp"

#include <mutex>
#include <string>

#include <boost/thread/shared_mutex.hpp>

#include <iostream>

int main(void) {
  dyn_lock<boost::shared_mutex> dl;
  typedef dyn_lock<boost::shared_mutex>::shared_timed_mutex DMutex;
  DMutex mut(dl);

  {
    std::unique_lock<DMutex> _(mut);
    std::cout << "in unique lock" << std::endl;
  }

  {
    boost::shared_lock<DMutex> _(mut);
    std::cout << "in shared lock" << std::endl;
  }

  return 0;
}
