#ifndef OSMIUM_MEMORY_BUFFER_HPP
#define OSMIUM_MEMORY_BUFFER_HPP

/*

This file is part of Osmium (http://osmcode.org/libosmium).

Copyright 2013-2015 Jochen Topf <jochen@topf.org> and others (see README).

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <exception>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <vector>

#include <osmium/memory/item.hpp>
#include <osmium/memory/item_iterator.hpp>
#include <osmium/osm/entity.hpp>

namespace osmium {

    /**
     * Exception thrown by the osmium::memory::Buffer class when somebody tries
     * to write data into a buffer and it doesn't fit. Buffers with internal
     * memory management will not throw this exception, but increase their size.
     */
    struct buffer_is_full : public std::runtime_error {

        buffer_is_full() :
            std::runtime_error("Osmium buffer is full") {
        }

    }; // struct buffer_is_full

    /**
     * @brief Memory management of items in buffers and iterators over this data.
     */
    namespace memory {

        /**
         * A memory area for storing OSM objects and other items. Each item stored
         * has a type and a length. See the Item class for details.
         *
         * Data can be added to a buffer piece by piece using reserve_space() and
         * add_item(). After all data that together forms an item is added, it must
         * be committed using the commit() call. Usually this is done through the
         * Builder class and its derived classes.
         *
         * You can iterate over all items in a buffer using the iterators returned
         * by begin(), end(), cbegin(), and cend().
         *
         * Buffers exist in two flavours, those with external memory management and
         * those with internal memory management. If you already have some memory
         * with data in it (for instance read from disk), you create a Buffer with
         * external memory managment. It is your job then to free the memory once
         * the buffer isn't used any more. If you don't have memory already, you can
         * create a Buffer object and have it manage the memory internally. It will
         * dynamically allocate memory and free it again after use.
         *
         * By default, if a buffer gets full it will throw a buffer_is_full exception.
         * You can use the set_full_callback() method to set a callback functor
         * which will be called instead of throwing an exception.
         */
        class Buffer {

        public:

            enum class auto_grow : bool {
                yes = true,
                no  = false
            }; // enum class auto_grow

        private:

            std::vector<unsigned char> m_memory;
            unsigned char* m_data;
            size_t m_capacity;
            size_t m_written;
            size_t m_committed;
            auto_grow m_auto_grow {auto_grow::no};
            std::function<void(Buffer&)> m_full;

        public:

            typedef Item value_type;

            /**
             * The constructor without any parameters creates a non-initialized
             * buffer, ie an empty hull of a buffer that has no actual memory
             * associated with it. It can be used to signify end-of-input.
             */
            Buffer() noexcept :
                m_memory(),
                m_data(nullptr),
                m_capacity(0),
                m_written(0),
                m_committed(0) {
            }

            /**
             * Constructs an externally memory-managed buffer using the given
             * memory and size.
             *
             * @param data A pointer to some already initialized data.
             * @param size The size of the initialized data.
             * @throws std::invalid_argument When the size isn't a multiple of the alignment.
             */
            explicit Buffer(unsigned char* data, size_t size) :
                m_memory(),
                m_data(data),
                m_capacity(size),
                m_written(size),
                m_committed(size) {
                if (size % align_bytes != 0) {
                    throw std::invalid_argument("buffer size needs to be multiple of alignment");
                }
            }

            /**
             * Constructs an externally memory-managed buffer with the given
             * capacity that already contains 'committed' bytes of data.
             *
             * @param data A pointer to some (possibly initialized) data.
             * @param capacity The size of the memory for this buffer.
             * @param committed The size of the initialized data. If this is 0, the buffer startes out empty.
             * @throws std::invalid_argument When the capacity or committed isn't a multiple of the alignment.
             */
            explicit Buffer(unsigned char* data, size_t capacity, size_t committed) :
                m_memory(),
                m_data(data),
                m_capacity(capacity),
                m_written(committed),
                m_committed(committed) {
                if (capacity % align_bytes != 0) {
                    throw std::invalid_argument("buffer capacity needs to be multiple of alignment");
                }
                if (committed % align_bytes != 0) {
                    throw std::invalid_argument("buffer parameter 'committed' needs to be multiple of alignment");
                }
            }

            /**
             * Create an internally memory-managed buffer with the given capacity.
             * different in that it internally gets dynamic memory of the
             * required size. The dynamic memory will be automatically
             * freed when the Buffer is destroyed.
             */
            explicit Buffer(size_t capacity, auto_grow auto_grow = auto_grow::yes) :
                m_memory(capacity),
                m_data(m_memory.data()),
                m_capacity(capacity),
                m_written(0),
                m_committed(0),
                m_auto_grow(auto_grow) {
                if (capacity % align_bytes != 0) {
                    throw std::invalid_argument("buffer capacity needs to be multiple of alignment");
                }
            }

            // buffers can not be copied
            Buffer(const Buffer&) = delete;
            Buffer& operator=(const Buffer&) = delete;

            // buffers can be moved
            Buffer(Buffer&&) = default;
            Buffer& operator=(Buffer&&) = default;

            ~Buffer() = default;

            /**
             * Return a pointer to data inside the buffer.
             */
            unsigned char* data() const noexcept {
                return m_data;
            }

            /**
             * Returns the capacity of the buffer, ie how many bytes it can contain.
             */
            size_t capacity() const noexcept {
                return m_capacity;
            }

            /**
             * Returns the number of bytes already filled in this buffer.
             */
            size_t committed() const noexcept {
                return m_committed;
            }

            /**
             * Returns the number of bytes currently filled in this buffer that
             * are not yet committed.
             */
            size_t written() const noexcept {
                return m_written;
            }

            /**
             * This tests if the current state of the buffer is aligned
             * properly. Can be used for asserts.
             */
            bool is_aligned() const noexcept {
                return (m_written % align_bytes == 0) && (m_committed % align_bytes == 0);
            }

            /**
             * Set functor to be called whenever the buffer is full
             * instead of throwing buffer_is_full.
             */
            void set_full_callback(std::function<void(Buffer&)> full) {
                m_full = full;
            }

            /**
             * Grow capacity of this buffer to the given size.
             * This works only with internally memory-managed buffers.
             * If the given size is not larger than the current capacity, nothing is done.
             * Already written but not committed data is discarded.
             *
             * @param size New capacity.
             */
            void grow(size_t size) {
                if (m_memory.empty()) {
                    throw std::logic_error("Can't grow Buffer if it doesn't use internal memory management.");
                }
                if (m_capacity < size) {
                    if (size % align_bytes != 0) {
                        throw std::invalid_argument("buffer capacity needs to be multiple of alignment");
                    }
                    m_memory.resize(size);
                    m_data = m_memory.data();
                    m_capacity = size;
                }
            }

            /**
             * Mark currently written bytes in the buffer as committed.
             *
             * @returns Last number of committed bytes before this commit.
             */
            size_t commit() {
                assert(is_aligned());

                const size_t offset = m_committed;
                m_committed = m_written;
                return offset;
            }

            /**
             * Roll back changes in buffer to last committed state.
             */
            void rollback() {
                m_written = m_committed;
            }

            /**
             * Clear the buffer.
             *
             * @returns Number of bytes in the buffer before it was cleared.
             */
            size_t clear() {
                const size_t committed = m_committed;
                m_written = 0;
                m_committed = 0;
                return committed;
            }

            /**
             * Get the data in the buffer at the given offset.
             *
             * @tparam T Type we want to the data to be interpreted as.
             * @returns Reference of given type pointing to the data in the buffer.
             */
            template <class T>
            T& get(const size_t offset) const {
                return *reinterpret_cast<T*>(&m_data[offset]);
            }

            /**
             * Reserve space of given size in buffer and return pointer to it.
             * This is the only way of adding data to the buffer. You reserve
             * the space and then fill it.
             *
             * Note that you have to eventually call commit() to actually
             * commit this data.
             *
             * If there isn't enough space in the buffer, one of three things
             * can happen:
             *
             * * If you have set a callback with set_full_callback(), it is
             *   called. After the call returns, you must have either grown
             *   the buffer or cleared it by calling buffer.clear().
             * * If no callback is defined and this buffer uses internal
             *   memory management, the buffers capacity is grown, so that
             *   the new data will fit.
             * * Else the buffer_is_full exception is thrown.
             *
             * @param size Number of bytes to reserve.
             * @returns Pointer to reserved space. Note that this pointer is
             *         only guaranteed to be valid until the next call to
             *         reserve_space().
             * @throws osmium::buffer_is_full Might be thrown if the buffer is full.
             */
            unsigned char* reserve_space(const size_t size) {
                if (m_written + size > m_capacity) {
                    if (m_full) {
                        m_full(*this);
                    } else if (!m_memory.empty() && (m_auto_grow == auto_grow::yes)) {
                        // double buffer size until there is enough space
                        size_t new_capacity = m_capacity * 2;
                        while (m_written + size > new_capacity) {
                            new_capacity *= 2;
                        }
                        grow(new_capacity);
                    } else {
                        throw osmium::buffer_is_full();
                    }
                }
                unsigned char* data = &m_data[m_written];
                m_written += size;
                return data;
            }

            /**
             * Add an item to the buffer. The size of the item is stored inside
             * the item, so we know how much memory to copy.
             *
             * Note that you have to eventually call commit() to actually
             * commit this data.
             *
             * @tparam T Class of the item to be copied.
             * @param item Reference to the item to be copied.
             * @returns Reference to newly copied data in the buffer.
             */
            template <class T>
            T& add_item(const T& item) {
                unsigned char* target = reserve_space(item.padded_size());
                std::copy_n(reinterpret_cast<const unsigned char*>(&item), item.padded_size(), target);
                return *reinterpret_cast<T*>(target);
            }

            /**
             * Add committed contents of the given buffer to this buffer.
             *
             * Note that you have to eventually call commit() to actually
             * commit this data.
             */
            void add_buffer(const Buffer& buffer) {
                unsigned char* target = reserve_space(buffer.committed());
                std::copy_n(reinterpret_cast<const unsigned char*>(buffer.data()), buffer.committed(), target);
            }

            /**
             * Add an item to the buffer. This function is provided so that
             * you can use std::back_inserter.
             */
            void push_back(const osmium::memory::Item& item) {
                add_item(item);
                commit();
            }

            /**
             * These iterators can be used to iterate over all items in
             * a buffer.
             */
            template <class T>
            using t_iterator = osmium::memory::ItemIterator<T>;

            template <class T>
            using t_const_iterator = osmium::memory::ItemIterator<const T>;

            typedef t_iterator<osmium::OSMEntity> iterator;
            typedef t_const_iterator<osmium::OSMEntity> const_iterator;

            template <class T>
            t_iterator<T> begin() {
                return t_iterator<T>(m_data, m_data + m_committed);
            }

            iterator begin() {
                return iterator(m_data, m_data + m_committed);
            }

            template <class T>
            t_iterator<T> end() {
                return t_iterator<T>(m_data + m_committed, m_data + m_committed);
            }

            iterator end() {
                return iterator(m_data + m_committed, m_data + m_committed);
            }

            template <class T>
            t_const_iterator<T> cbegin() const {
                return t_const_iterator<T>(m_data, m_data + m_committed);
            }

            const_iterator cbegin() const {
                return const_iterator(m_data, m_data + m_committed);
            }

            template <class T>
            t_const_iterator<T> cend() const {
                return t_const_iterator<T>(m_data + m_committed, m_data + m_committed);
            }

            const_iterator cend() const {
                return const_iterator(m_data + m_committed, m_data + m_committed);
            }

            template <class T>
            t_const_iterator<T> begin() const {
                return cbegin<T>();
            }

            const_iterator begin() const {
                return cbegin();
            }

            template <class T>
            t_const_iterator<T> end() const {
                return cend<T>();
            }

            const_iterator end() const {
                return cend();
            }

            /**
             * In a bool context any initialized buffer is true.
             */
            explicit operator bool() const {
                return m_data != nullptr;
            }

            friend void swap(Buffer& lhs, Buffer& rhs) {
                using std::swap;

                swap(lhs.m_memory, rhs.m_memory);
                swap(lhs.m_data, rhs.m_data);
                swap(lhs.m_capacity, rhs.m_capacity);
                swap(lhs.m_written, rhs.m_written);
                swap(lhs.m_committed, rhs.m_committed);
            }

            /**
             * Purge removed items from the buffer. This is done by moving all
             * non-removed items forward in the buffer overwriting removed
             * items and then correcting the m_written and m_committed numbers.
             *
             * Note that calling this function invalidates all iterators on this
             * buffer and all offsets in this buffer.
             *
             * For every non-removed item that moves its position, the function
             * 'moving_in_buffer' is called on the given callback object with
             * the old and new offsets in the buffer where the object used to
             * be and is now, respectively. This call can be used to update any
             * indexes.
             */
            template <class TCallbackClass>
            void purge_removed(TCallbackClass* callback) {
                if (begin() == end()) {
                    return;
                }

                iterator it_write = begin();

                iterator next;
                for (iterator it_read = begin(); it_read != end(); it_read = next) {
                    next = std::next(it_read);
                    if (!it_read->removed()) {
                        if (it_read != it_write) {
                            assert(it_read.data() >= data());
                            assert(it_write.data() >= data());
                            size_t old_offset = static_cast<size_t>(it_read.data() - data());
                            size_t new_offset = static_cast<size_t>(it_write.data() - data());
                            callback->moving_in_buffer(old_offset, new_offset);
                            std::memmove(it_write.data(), it_read.data(), it_read->padded_size());
                        }
                        it_write.advance_once();
                    }
                }

                assert(it_write.data() >= data());
                m_written = static_cast<size_t>(it_write.data() - data());
                m_committed = m_written;
            }

        }; // class Buffer

        inline bool operator==(const Buffer& lhs, const Buffer& rhs) noexcept {
            return lhs.data() == rhs.data() && lhs.capacity() == rhs.capacity() && lhs.committed() == rhs.committed();
        }

        inline bool operator!=(const Buffer& lhs, const Buffer& rhs) noexcept {
            return ! (lhs == rhs);
        }

    } // namespace memory

} // namespace osmium

#endif // OSMIUM_MEMORY_BUFFER_HPP