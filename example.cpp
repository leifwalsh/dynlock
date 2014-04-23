#include "dynlock.hpp"

#include <mutex>
#include <string>

#include <boost/thread/shared_mutex.hpp>

#include <iostream>

int main(void) {
  dyn_lock<boost::shared_mutex> dl;
  std::string key("fizz");
  typedef dyn_lock<boost::shared_mutex>::shared_mutex<std::string> DMutex;
  DMutex mut(dl, key);

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
