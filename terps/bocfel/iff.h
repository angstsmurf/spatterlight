// vim: set ft=cpp:

#ifndef ZTERP_IFF_H
#define ZTERP_IFF_H

#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "io.h"
#include "types.h"

class IFF {
public:
    class TypeID {
    public:
        TypeID(unsigned char a, unsigned char b, unsigned char c, unsigned char d) :
            m_type((static_cast<uint32_t>(a) << 24) |
                   (static_cast<uint32_t>(b) << 16) |
                   (static_cast<uint32_t>(c) <<  8) |
                   (static_cast<uint32_t>(d) <<  0)) {
        }

        TypeID() : m_type(0) {
        }

        explicit TypeID(uint32_t type) : m_type(type) {
        }

        explicit TypeID(const char (&type)[5]) : TypeID(type[0], type[1], type[2], type[3]) {
        }

        [[nodiscard]] uint32_t val() const {
            return m_type;
        }

        [[nodiscard]] std::string name() const {
            auto ascii_filter = [](uint8_t c) -> char {
                if (c >= 32 && c <= 126) {
                    return c;
                } else {
                    return '?';
                }
            };

            return {
                ascii_filter(m_type >> 24),
                ascii_filter(m_type >> 16),
                ascii_filter(m_type >>  8),
                ascii_filter(m_type >>  0)
            };
        }

        [[nodiscard]] bool empty() const {
            return m_type == 0;
        }

        bool operator==(const TypeID &rhs) const {
            return val() == rhs.val();
        }

        bool operator!=(const TypeID &rhs) const {
            return !operator==(rhs);
        }

    private:
        uint32_t m_type;
    };

    struct Entry {
        Entry(TypeID type_, long offset_, uint32_t size_) :
            type(type_), offset(offset_), size(size_) {
        }
        TypeID type;
        long offset;
        uint32_t size;
    };

    class InvalidFile : public std::exception {
    };

    IFF(std::shared_ptr<IO> io, TypeID type);
    bool find(TypeID type, uint32_t &size);

    [[nodiscard]] std::shared_ptr<IO> io() const {
        return m_io;
    }

private:
    std::shared_ptr<IO> m_io;
    std::vector<Entry> m_entries;
};

#endif
