 
#ifndef PARLAY_INTERNAL_WINDOWS_FILE_MAP_IMPL_WINDOWS_H_
#define PARLAY_INTERNAL_WINDOWS_FILE_MAP_IMPL_WINDOWS_H_

#include <cassert>
#include <cstdio>

#include <algorithm>
#include <string>

#include <tchar.h>
#include <strsafe.h>
#include <windows.h>

namespace parlay {

struct file_map {
  private:
    HANDLE hMapFile;      // handle for the file's memory-mapped region
    HANDLE hFile;         // the file handle

    char* first;
    size_t size;
  public:
    using value_type = char;
    using reference = char&;
    using const_reference = const char&;
    using iterator = char*;
    using const_iterator = const char*;
    using pointer = char*;
    using const_pointer = const char*;
    using difference_type = std::ptrdiff_t;
    using size_type = size_t;

    char operator[] (size_t i) const { return begin()[i]; }
    const char* begin() const { return first; }
    const char* end() const { return first + size; }

    explicit file_map(const std::string& filename) {

      // Open file handle
      hFile = CreateFile(filename.c_str(),
        GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

      assert(hFile != INVALID_HANDLE_VALUE);

      // Create a file mapping object for the file
      // Note that it is a good idea to ensure the file size is not zero
      hMapFile = CreateFileMapping(hFile,          // current file handle
        NULL,            // default security
        PAGE_READONLY,   // read/write permission
        0,               // size of mapping object, high
        0,               // size of mapping object, low
        NULL);           // name of mapping object

      assert(hMapFile != NULL);

      first = static_cast<char*>(
        MapViewOfFile(hMapFile,       // handle to mapping object
        FILE_MAP_READ,                    // read/write
        0,
        0,
        0)
      );

      assert(first != nullptr);

      LARGE_INTEGER file_size;
      GetFileSizeEx(hFile, &file_size);
      size = static_cast<size_t>(file_size.QuadPart);
    }
    
    ~file_map() {
        close();
    }

    // moveable but not copyable
    file_map(const file_map&) = delete;
    file_map& operator=(const file_map&) = delete;
    
    file_map(file_map&& other) noexcept : hMapFile(other.hMapFile), hFile(other.hFile), first(other.first), size(other.size)  {
      other.hMapFile = NULL;
      other.hFile = NULL;
      other.first = nullptr;
      other.size = 0;
    }
    
    file_map& operator=(file_map&& other) noexcept {
      close();
      swap(other);
      return *this;
    }
    
    void close() {
      if (first != nullptr) {
        auto unmapped = UnmapViewOfFile(first);
        assert(unmapped);
        auto closedMapping = CloseHandle(hMapFile);
        assert(closedMapping);
        auto closedFile = CloseHandle(hFile);
        assert(closedFile);

        first = nullptr;
        size = 0;
        hFile = NULL;
        hMapFile = NULL;
      }
    }

    void swap(file_map& other) {
      std::swap(hFile, other.hFile);
      std::swap(hMapFile, other.hMapFile);
      std::swap(first, other.first);
      std::swap(size, other.size);
    }

    bool empty() const {
      return first == nullptr;
    }
};

}  // namespace parlay

#endif  // PARLAY_INTERNAL_WINDOWS_FILE_MAP_IMPL_WINDOWS_H_
 