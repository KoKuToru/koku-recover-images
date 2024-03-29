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
#include "read_webp.h"
#include "utils.h"

constexpr ssize_t MAX_SIZE = 1 * 1024 * 1024 * 1024; // 1GiB

bool update_print = true;
bool alarm_running = false;
void handle_alarm( int sig ) {
    alarm_running = false;
    update_print = true;
}

std::string format_bytes(size_t size, bool atty) {
    double v = size;
    const char* u = nullptr;
    if (v < 1024 * 1024) {
        v /= 1024;
        u = "KiB";
    } else if (v < 1024 * 1024 * 1024) {
        v /= 1024 * 1024;
        u = "MiB";
    } else {
        v /= 1024 * 1024 * 1024;
        u = "GiB";
    }
    if (atty) {
        return std::format("\33[1m{:7.2f}\33[0m{}", v, u);
    }
    return std::format("{:.2f}{}", v, u);
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
            fprintf(stderr, "Usage: %s <disk-image>\n", argv[0]);
        } else {
            fprintf(stderr, "Usage: koku-recover-images <disk-image>\n");
        }
        fprintf(stderr, "Description:\n\tExtracts unfragmented JPEGs, PNGs, GIFs and TIFFs from <disk-image>\n");
        exit(-1);
    }
    auto fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "couldn't open file %s\n", argv[1]);
        exit(-1);
    }
    struct stat  sb;
    fstat(fd, &sb);
    auto addr = mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (addr == MAP_FAILED) {
        fprintf(stderr, "couldn't memory map file %s\n", argv[1]);
        exit(-1);
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
    size_t found_webp = 0;

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

        if (atty_stderr && (update_print || span.empty())) {
            update_print = false;
            const auto offset = std::distance(start, span.data());
            const auto percent = offset / double(sb.st_size) * 100;
            const auto pos = format_bytes(offset, atty_stderr);
            const auto pos_max = format_bytes(sb.st_size, atty_stderr);
            last_print = std::format("\33[2K\r\33[1m{:6.2f}\33[0m% {}/{} \33[1m{:11d}\33[0m images", percent, pos, pos_max, found);
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

        // quick skip
        {
            constexpr auto FIRST_BYTES = std::array{
                FIRST_BYTE_JPG,
                FIRST_BYTE_PNG,
                FIRST_BYTE_GIF,
                FIRST_BYTE_TIF_LITTLE,
                FIRST_BYTE_TIF_BIG,
                FIRST_BYTE_WEBP
            };
            auto tmp = subspan(span, 0, MAX_SIZE);
            span = span.subspan(std::distance(tmp.begin(), std::find_first_of(tmp.begin(), tmp.end(), FIRST_BYTES.begin(), FIRST_BYTES.end())));
            if (span.empty()) {
                continue;
            }
        }

        decltype(span) img_data = subspan(span, 0, MAX_SIZE);
        const char* ext = nullptr;
        size_t* found_ext;
        switch (span[0]) {
            case FIRST_BYTE_JPG:
                img_data = read_jpg(img_data);
                ext = "jpg";
                found_ext = &found_jpg;
                break;
            case FIRST_BYTE_PNG:
                img_data = read_png(img_data);
                ext = "png";
                found_ext = &found_png;
                break;
            case FIRST_BYTE_TIF_BIG:
            case FIRST_BYTE_TIF_LITTLE:
                img_data = read_tif(img_data);
                ext = "tif";
                found_ext = &found_tif;
                break;
            case FIRST_BYTE_GIF:
                img_data = read_gif(img_data);
                ext = "gif";
                found_ext = &found_gif;
                break;
            case FIRST_BYTE_WEBP:
                img_data = read_webp(img_data);
                ext = "webp";
                found_ext = &found_webp;
                break;
        }
        if (ext && !img_data.empty()) {
            update_print = true;
            save(fd, found, start, img_data, ext);
            ++found;
            ++(*found_ext);
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

    auto dir = std::format("{:08d}", img_count / 4096);
    auto name = std::format("{}/{:020d}.{}", dir, offset, ext);

    if (atty_stdout) {
        if (atty_stderr) {
            fprintf(stderr, "\33[2K\r");
            fflush(stderr);
        }
        fprintf(stdout, "%-34s", name.c_str());
        if (atty_stderr) {
            auto size = format_bytes(data.size(), atty_stdout);
            fflush(stdout);
            fprintf(stderr, " %s\n%s", size.c_str(), last_print.c_str());
            fflush(stderr);
        } else {
            fprintf(stdout, "\n");
        }
    } else {
        fprintf(stdout, "%s\n", name.c_str());
    }

    mkdir(dir.c_str(), 0750);
    auto fd_jpg = open(name.c_str(), O_WRONLY|O_CREAT, 0640);
    if (fd_jpg == -1) {
        if (atty_stderr) {
            fprintf(stderr, "\33[2K\r");
        }
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
                if (atty_stderr) {
                    fprintf(stderr, "\33[2K\r");
                }
                fprintf(stderr, "couldn't write to file %s\n", name.c_str());
                exit(-1);
            }
            off_out += res;
        }
    }
    close(fd_jpg);
}