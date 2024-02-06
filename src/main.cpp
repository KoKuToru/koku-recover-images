#define _FILE_OFFSET_BITS 64
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <span>
#include <algorithm>
#include <iterator>
//#include <print>
#include <cstring>
#include <bit>
#include <format>
#include <signal.h>
#include <cstdint>
#include <sys/sendfile.h>

#include "read_jpg.h"
#include "read_png.h"
#include "read_tif.h"
#include "read_gif.h"
#include "utils.h"

constexpr ssize_t MAX_SIZE = 128 * 1024 * 1024;

bool update_print = true;
bool alarm_running = false;
void handle_alarm( int sig ) {
    alarm_running = false;
    update_print = true;
}

std::string format_bytes(size_t size, bool atty) {
    double v = size;
    v /= (1024 * 1024);
    if (v < 1024) {
        return std::vformat(atty ? "\33[1m{:.2f}\33[0mMiB" : "{:.2f}MiB", std::make_format_args(v));
    }
    v /= 1024;
    return std::vformat(atty ? "\33[1m{:.2f}\33[0mGiB" : "{:.2f}GiB", std::make_format_args(v));
}

enum MODE {
    COPY_FILE_RANGE,
    SENDFILE,
    WRITE
};

void save(int fd, int img_count, const uint8_t* start, const std::span<const uint8_t> data, const char* ext);

bool atty_stderr = isatty(fileno(stderr));
bool atty_stdout = isatty(fileno(stdout));
MODE mode = COPY_FILE_RANGE;
std::string last_print;

int main(int argc, const char** argv) {
    if (argc < 2) {
        if (argc > 0) {
            fprintf(stderr, "%s <disk-image>\n", argv[0]);
        } else {
            fprintf(stderr, "koku_recoverjpg <disk-image>\n");
        }
        return -1;
    }
    auto fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "couldn't open file %s\n", argv[1]);
        return -1;
    }
    struct stat  sb;
    fstat(fd, &sb);
    auto addr = mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED) {
        fprintf(stderr, "couldn't memory map file %s\n", argv[1]);
        return -1;
    }
    madvise(addr, sb.st_size, MADV_DONTDUMP);
    madvise(addr, sb.st_size, MADV_SEQUENTIAL);
    auto span = std::span<const uint8_t>{(unsigned char*)addr, (size_t)sb.st_size};

    signal(SIGALRM, handle_alarm);

    size_t found = 0;
    size_t found_jpg = 0;
    size_t found_png = 0;
    size_t found_tif = 0;
    size_t found_gif = 0;

    const auto start = span.data();
    auto last = start;
    const auto pagesize = getpagesize();

    while (true) {
        if (std::distance(last, span.data()) >= MAX_SIZE) {
            // memory manage a little..
            const auto source = (void*)(uintptr_t(last) / pagesize * pagesize);
            const auto size = uintptr_t(span.data()) / pagesize * pagesize - uintptr_t(source);
            madvise(source, size, MADV_DONTNEED);
            last = decltype(last)(uintptr_t(source) + size);
            madvise((void*)last, (2 * MAX_SIZE) / pagesize * pagesize, MADV_WILLNEED);
        }

        if (update_print || span.empty()) {
            update_print = false;
            const auto offset = std::distance(start, span.data());
            last_print = std::vformat(
                atty_stderr ? "\33[2K\r\33[1m{:3.2f}\33[0m% {}/{} \33[1m{}\33[0m images" : "\r{:3.2f}% {}/{} {} images",
                std::make_format_args(
                    offset / double(sb.st_size) * 100,
                    format_bytes(offset, atty_stderr), format_bytes(sb.st_size, atty_stderr),
                    found
                )
            );
            fprintf(stderr, "%s", last_print.c_str());
            fflush(stderr);
            if (!alarm_running) {
                alarm_running = true;
                alarm(1);
            }
        }

        if (span.empty()) {
            break;
        }

        {
            auto img_data = read_jpg(subspan(span, 0, MAX_SIZE));
            if (!img_data.empty()) {
                update_print = true;
                save(fd, found, start, img_data, "jpg");
                ++found;
                ++found_jpg;
            }
        }
        {
            auto img_data = read_png(subspan(span, 0, MAX_SIZE));
            if (!img_data.empty()) {
                update_print = true;
                save(fd, found, start, img_data, "png");
                ++found;
                ++found_png;
            }
        }
        {
            auto img_data = read_tif(subspan(span, 0, MAX_SIZE));
            if (!img_data.empty()) {
                update_print = true;
                save(fd, found, start, img_data, "tif");
                ++found;
                ++found_tif;
            }
        }
        {
            auto img_data = read_gif(subspan(span, 0, MAX_SIZE));
            if (!img_data.empty()) {
                update_print = true;
                save(fd, found, start, img_data, "gif");
                ++found;
                ++found_gif;
            }
        }

        span = subspan(span, sizeof(unsigned char));
    }

    munmap(addr, sb.st_size);
    close(fd);
    fprintf(stderr, "\n");

    exit(0);
}

void save(int fd, int img_count, const uint8_t* start, const std::span<const uint8_t> data, const char* ext) {
    // save the data
    auto offset = std::distance(start, data.data());
    auto dir = std::format("{:08x}", img_count / 512);
    auto name = std::format("{}/{:016x}.{}", dir, offset, ext);

    fprintf(stderr, atty_stderr ? "\33[2K\r" : "\r");

    auto size = format_bytes(data.size(), atty_stdout);
    fprintf(stdout, "%s %*s\n", name.c_str(), (atty_stdout ? 10 : 0) + 8, size.c_str());
    fprintf(stderr, "%s", last_print.c_str());
    fflush(stderr);

    mkdir(dir.c_str(), 0750);
    auto fd_jpg = open(name.c_str(), O_WRONLY|O_CREAT, 0640);
    if (fd_jpg == -1) {
        fprintf(stderr, "couldn't create new file %s\n", name.c_str());
        exit(-1);
    }
    if (mode == COPY_FILE_RANGE) {
        loff_t off_in  = offset;
        loff_t off_out = 0;
        while (true) {
            ssize_t size = data.size() - off_out;
            auto res = copy_file_range(fd, &off_in, fd_jpg, &off_out, size, 0);
            if (res == 0 || res == size) {
                break;
            }
            if (res == -1) {
                mode = SENDFILE;
                lseek(fd_jpg, 0, SEEK_SET);
                break;
            }
        }
    }
    if (mode == SENDFILE) {
        loff_t off_in  = offset;
        loff_t off_out = 0;
        while (true) {
            ssize_t size = data.size() - off_out;
            auto res = sendfile(fd_jpg, fd, &off_in, size);
            if (res == 0 || res == size) {
                // man page doesn't mention in_fd eof case as in copy_file_range
                // but it looks like it might return 0
                // but it should never occur
                break;
            }
            if (res == -1) {
                mode = WRITE;
                lseek(fd_jpg, 0, SEEK_SET);
                break;
            }
            off_out += res;
        }
    }
    if (mode == WRITE) {
        loff_t off_out = 0;
        while (true) {
            ssize_t size = data.size() - off_out;
            auto res = write(fd_jpg, data.data() + off_out, size);
            if (res == size) {
                break;
            }
            if (res == -1) {
                unlink(name.c_str());
                fprintf(stderr, "couldn't write to file %s\n", name.c_str());
                exit(-1);
            }
            off_out += res;
        }
    }
    close(fd_jpg);
}