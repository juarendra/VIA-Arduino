#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>

namespace Adafruit_LittleFS_Namespace {
  enum {
    FILE_O_READ = 1,
    FILE_O_WRITE = 2
  };

  class File {
  public:
    File() : data_(nullptr), pos_(0), mode_(0), error_(true) {}
    File(std::vector<uint8_t>* data, int mode) : data_(data), pos_(0), mode_(mode), error_(false) {
        if (mode_ == FILE_O_WRITE) {
            data_->clear();
        }
    }
    
    operator bool() const { return !error_ && data_ != nullptr; }
    
    size_t read(uint8_t* buf, size_t size) {
        if (error_ || !data_ || mode_ != FILE_O_READ) return 0;
        size_t available = data_->size() - pos_;
        size_t to_read = (size < available) ? size : available;
        memcpy(buf, data_->data() + pos_, to_read);
        pos_ += to_read;
        return to_read;
    }

    size_t write(const uint8_t* buf, size_t size) {
        if (error_ || !data_ || mode_ != FILE_O_WRITE) return 0;
        
        // Simulating failure
        extern bool g_fake_fs_write_fail;
        if (g_fake_fs_write_fail) {
            return 0; 
        }

        data_->insert(data_->end(), buf, buf + size);
        return size;
    }

    void seek(size_t pos) {
        if (data_ && pos <= data_->size()) {
            pos_ = pos;
        }
    }

    void close() {
        data_ = nullptr;
        error_ = true;
    }

  private:
    std::vector<uint8_t>* data_;
    size_t pos_;
    int mode_;
    bool error_;
  };

  class LittleFS {
  public:
    bool begin() { return true; }
    
    File open(const char* filepath, int mode) {
        if (mode == FILE_O_READ) {
            auto it = files_.find(filepath);
            if (it == files_.end()) {
                return File();
            }
            return File(&it->second, mode);
        } else {
            return File(&files_[filepath], mode);
        }
    }

    bool remove(const char* filepath) {
        return files_.erase(filepath) > 0;
    }

    void format() {
        files_.clear();
    }

    std::map<std::string, std::vector<uint8_t>> files_;
  };
}

extern Adafruit_LittleFS_Namespace::LittleFS InternalFS;
extern bool g_fake_fs_write_fail;
