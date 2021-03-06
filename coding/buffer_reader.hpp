#include "coding/reader.hpp"

#include "std/shared_ptr.hpp"
#include "std/cstring.hpp"


/// Reader from buffer with ownership on it, but cheap copy constructor.
class BufferReader : public Reader
{
public:
  template <class ReaderT>
  explicit BufferReader(ReaderT const & reader, uint64_t offset = 0)
  {
    uint64_t const rSize = reader.Size();
    ASSERT_LESS_OR_EQUAL(offset, rSize, (offset, rSize));

    InitBuffer(static_cast<size_t>(rSize - offset));
    reader.Read(offset, m_data.get(), m_size);
  }

  explicit BufferReader(char const * p, size_t count)
  {
    InitBuffer(count);
    memcpy(m_data.get(), p, count);
  }

  inline uint64_t Size() const
  {
    return m_size;
  }

  inline void Read(uint64_t pos, void * p, size_t size) const
  {
    ASSERT_LESS_OR_EQUAL(pos + size, Size(), (pos, size));
    memcpy(p, m_data.get() + static_cast<size_t>(pos) + m_offset, size);
  }

  inline BufferReader SubReader(uint64_t pos, uint64_t size) const
  {
    return BufferReader(*this, pos, size);
  }

  inline BufferReader * CreateSubReader(uint64_t pos, uint64_t size) const
  {
    return new BufferReader(*this, pos, size);
  }

private:
  BufferReader(BufferReader const & src, uint64_t pos, uint64_t size)
    : m_data(src.m_data)
  {
    ASSERT_LESS_OR_EQUAL(pos + size, src.Size(), (pos, size));

    m_offset = static_cast<size_t>(src.m_offset + pos);
    m_size = static_cast<size_t>(size);
  }

  void InitBuffer(size_t count)
  {
    m_offset = 0;
    m_size = count;
    m_data.reset(new char[m_size], Deleter());
  }

  size_t m_offset, m_size;

  struct Deleter
  {
    void operator() (char * p) { delete [] p; }
  };
  shared_ptr<char> m_data;
};
