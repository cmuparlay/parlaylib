
#ifndef PARLAY_INTERNAL_POSIX_FILE_MAP_IMPL_POSIX
#define PARLAY_INTERNAL_POSIX_FILE_MAP_IMPL_POSIX

#include <cassert>

#include <algorithm>
#include <string>

#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

namespace parlay {

class file_map {
  private:
    char* begin_p;
    char* end_p;
  public:
  
    using value_type = char;
    using reference = char&;
    using const_reference = const char&;
    using iterator = char*;
    using const_iterator =  const char*;
    using pointer = char*;
    using const_pointer = const char*;
    using difference_type = std::ptrdiff_t;
    using size_type = size_t;
  
    char operator[] (size_t i) const { return begin_p[i]; }
    const char* begin() const { return begin_p; }
    const char* end() const {return end_p; }

    size_t size() const { return end() - begin(); }

    explicit file_map(const std::string& filename) {
      
      struct stat sb;
      int fd = open(filename.c_str(), O_RDONLY);
      assert(fd != -1);
      [[maybe_unused]] auto fstat_res = fstat(fd, &sb);
      assert(fstat_res != -1);
      assert(S_ISREG(sb.st_mode));
    
      char *p = static_cast<char*>(mmap(0, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
      assert(p != MAP_FAILED);
      [[maybe_unused]] int close_p = ::close(fd);
      assert(close_p != -1);
      
      begin_p = p;
      end_p = p + sb.st_size;
    }
    
    ~file_map() {
      close();
    }

    // moveable but not copyable
    file_map(const file_map&) = delete;
    file_map& operator=(const file_map&) = delete;
    
    file_map(file_map&& other) noexcept : begin_p(other.begin_p), end_p(other.end_p) {
      other.begin_p = nullptr;
      other.end_p = nullptr;
    }
    
    file_map& operator=(file_map&& other) noexcept {
      close();
      swap(other);
      return *this;
    }
    
    void close() {
      if (begin_p != nullptr) {
        munmap(begin_p, end_p - begin_p);
      }
      begin_p = nullptr;
      end_p = nullptr;
    }
    
    void swap(file_map& other) {
      std::swap(other.begin_p, begin_p);
      std::swap(other.end_p, end_p);
    }
    
    bool empty() const {
      return begin_p == nullptr;
    }
};

}  // namespace parlay

#endif  // PARLAY_INTERNAL_POSIX_FILE_MAP_IMPL_POSIX
