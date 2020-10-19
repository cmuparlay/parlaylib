
#ifndef PARLAY_INTERNAL_FILE_MAP_H_
#define PARLAY_INTERNAL_FILE_MAP_H_

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
#define PARLAY_WINDOWS_FILE_MAP
#else
#include <unistd.h>
#if defined(_POSIX_MAPPED_FILES) && ((_POSIX_MAPPED_FILES + 0) > 0)
#define PARLAY_POSIX_FILE_MAP
#endif
#endif

#if !defined(PARLAY_WINDOWS_FILE_MAP) && !defined(PARLAY_POSIX_FILE_MAP)
#define PARLAY_NO_FILE_MAP
#endif

#if defined(PARLAY_WINDOWS_FILE_MAP) && !defined(PARLAY_USE_FALLBACK_FILE_MAP)
#include "windows/file_map_impl_windows.h"
#endif

#if defined(PARLAY_POSIX_FILE_MAP) && !defined(PARLAY_USE_FALLBACK_FILE_MAP)
#include "posix/file_map_impl_posix.h"
#endif

#if defined(PARLAY_NO_FILE_MAP) || defined(PARLAY_USE_FALLBACK_FILE_MAP)

#include <fstream>
#include <string>

namespace parlay {

// A platform-independent simulation of file map. This one reads in the entire file
// to main memory all at once. This could be slow, and even worse, could fail badly
// if the file is so large that it does not fit in main memory!

class file_map {
  private:
    std::string contents;
  public:
    using value_type = std::string::value_type;
    using reference = std::string::reference;
    using const_reference = std::string::const_reference;
    using iterator = std::string::iterator;
    using const_iterator = std::string::const_iterator;
    using pointer = std::string::pointer;
    using const_pointer = std::string::const_pointer;
    using difference_type = std::string::difference_type;
    using size_type = std::string::size_type;

    char operator[] (size_t i) const { return contents[i]; }
    auto begin() const { return contents.begin(); }
    auto end() const { return contents.end(); }
    
    size_t size() const { return end() - begin(); }

    explicit file_map(const std::string& filename) {
      std::ifstream in(filename);
      assert(in.is_open());
      in.seekg(0, std::ios::end);
      size_t sz = static_cast<size_t>(in.tellg());
      contents.resize(sz);
      in.seekg(0);
      in.read(&(*contents.begin()), sz);
    }

    ~file_map() {
      contents.clear();
    }
    
    file_map(file_map&& other) noexcept : contents(std::move(other.contents)) {
      other.contents.clear();
    }
    
    file_map& operator=(file_map&& other) {
      contents = std::move(other.contents);
      other.contents.clear();
      return *this;
    }
    
    file_map(const file_map&) = delete;
    file_map operator=(const file_map&) = delete;
    
    void swap(file_map& other) {
      std::swap(contents, other.contents);
    }

    bool empty() const {
      return contents.empty();
    }
};

}

#endif  // PARLAY_NO_FILE_MAP

namespace std {
  inline void swap(parlay::file_map& first, parlay::file_map& second) {
    first.swap(second);
  }
}

#endif  // PARLAY_INTERNAL_FILE_MAP_H_
