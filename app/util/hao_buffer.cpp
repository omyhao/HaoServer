#include "hao_buffer.h"

#include <sys/uio.h>

#include <functional>
#include <cstring>
using namespace hao_util;

using std::boyer_moore_horspool_searcher;

const string_view kCRLF{"\r\n"};
boyer_moore_horspool_searcher my_searcher_(kCRLF.begin(), kCRLF.end());

const size_t Buffer::kInitialSize{ 1024 };
const size_t Buffer::kCheapPrepend{ 8 };

Buffer::Buffer(size_t initial_size)
    :buffer_(kCheapPrepend + initial_size),
    reader_index_{kCheapPrepend},
    writer_index_{kCheapPrepend}
{

}
void Buffer::swap(Buffer& rhs)
{
    buffer_.swap(rhs.buffer_);
    std::swap(reader_index_, rhs.reader_index_);
    std::swap(writer_index_, rhs.writer_index_);
}

// 可读数据大小
size_t Buffer::ReadableBytes() const
{
    return writer_index_ - reader_index_;
}

// 可写数据大小
size_t Buffer::WriteableBytes() const
{
    return buffer_.size() - writer_index_;
}

// 头部预留空间大小
size_t Buffer::PrependableBytes() const
{
    return reader_index_;
}

// 可读区域的起始地址
const char* Buffer::Peek() const
{
    return Begin() + reader_index_;
}

char* Buffer::BeginWrite()
{
    return Begin() + writer_index_;
}

const char* Buffer::BeginWrite() const
{
    return Begin() + writer_index_;
}

// 查找第一个CRLF
const char* Buffer::FindCRLF() const
{
    const char *crlf = std::search(Peek(), BeginWrite(), my_searcher_);
    return crlf  == BeginWrite() ? nullptr : crlf;
}

// 从start开始查找第一个CRLF
const char* Buffer::FindCRLF(const char *start) const
{
    const char *crlf = std::search(start, BeginWrite(), my_searcher_);
    return crlf == BeginWrite() ? nullptr : crlf;
}

// 查找第一个回车符
const char* Buffer::FindEOL() const
{
    const void* eol = std::memchr(Peek(), '\n', ReadableBytes());
    return static_cast<const char*>(eol);
}

// 从start查找第一个回车符
const char* Buffer::FindEOL(const char *start) const
{
    const void *eol = std::memchr(start, '\n', BeginWrite() - start);
    return static_cast<const char*>(eol);
}

// 缓冲区可读容量减少len
void Buffer::Retrieve(size_t len)
{
    if(len < ReadableBytes())
    {
        reader_index_ += len;
    }
    else
    {
        RetrieveAll();
    }
}

// 减少缓冲区容量，可取取余的起始地址移动到end指向的位置
void Buffer::RetrieveUtil(const char *end)
{
    Retrieve(end - Peek());
}

// 缓冲区可读容量减少sizeof(int8_t)
void Buffer::RetrieveInt8()
{
    Retrieve(sizeof(int8_t));
}

// 缓冲区可读容量减少sizeof(int16_t)
void Buffer::RetrieveInt16()
{
    Retrieve(sizeof(int16_t));
}

// 缓冲区可读容量减少sizeof(int32_t)
void Buffer::RetrieveInt32()
{
    Retrieve(sizeof(int32_t));
}

// 缓冲区可读容量减少sizeof(int 64)
void Buffer::RetrieveInt64()
{
    Retrieve(sizeof(int64_t));
}
// 重置缓冲区
void Buffer::RetrieveAll()
{
    reader_index_ = kCheapPrepend;
    writer_index_ = kCheapPrepend;
}

// 将可读区域容量减少到0，并读出该部分数据
string Buffer::RetrieveAllAsString()
{
    return RetrieveAsString(ReadableBytes());
}

// 缓冲区可读区域容量减少len，并读取该部分数据
string Buffer::RetrieveAsString(size_t len)
{
    string result(Peek(), len);
    Retrieve(len);
    return result;
}

// 读出可读区域的数据
string_view Buffer::ToStringView() const
{
    return string_view(Peek(), ReadableBytes());
}

// 数据添加接口
void Buffer::Append(string_view str)
{
    Append(str.data(), str.size());
}

void Buffer::Append(const char *data, size_t len)
{
    EnsureWritableBytes(len);
    std::copy(data, data + len, BeginWrite());
    HasWritten(len);
}

void Buffer::Append(const void *data, size_t len)
{
    Append(static_cast<const char*>(data), len);
}

void Buffer::AppendInt8(int8_t x)
{
    Append(&x, sizeof(x));
}

void Buffer::AppendInt16(int16_t x)
{
    int16_t big_end16 = htobe16(x);
    Append(&big_end16, sizeof(big_end16));
}

void Buffer::AppendInt32(int32_t x)
{
    int32_t big_end32 = htobe32(x);
    Append(&big_end32, sizeof(big_end32));
}

void Buffer::AppendInt64(int64_t x)
{
    int64_t big_end64 =  htobe64(x);
    Append(&big_end64, sizeof(int64_t));
}

// 确保可写区域能容得下len大小的数据，如果不够，则自动调整或扩容
void Buffer::EnsureWritableBytes(size_t len)
{
    if(WriteableBytes() < len)
    {
        MakeSpace(len);
    }
}

// 写入数据，更新写指针
void Buffer::HasWritten(size_t len)
{
    writer_index_ += len;
}

void Buffer::UnWrite(size_t len)
{
    writer_index_ -= len;
}

// 读取整数，并调整对应缓冲区
int8_t  Buffer::ReadInt8()
{
    int8_t result = PeekInt8();
    RetrieveInt8();
    return result;
}

int16_t Buffer::ReadInt16()
{
    int16_t result = PeekInt16();
    RetrieveInt16();
    return result;
}

int32_t Buffer::ReadInt32()
{
    int32_t result = PeekInt32();
    RetrieveInt32();
    return result;
}

int64_t Buffer::ReadInt64()
{
    int64_t result = PeekInt64();
    RetrieveInt64();
    return result;
}

// 只是读取，并不读出
int8_t  Buffer::PeekInt8()
{
    int8_t x = *Peek();
    return x;
}

int16_t Buffer::PeekInt16()
{
    int16_t big_end16{0};
    std::memcpy(&big_end16, Peek(), sizeof(big_end16));
    return be16toh(big_end16);
}

int32_t Buffer::PeekInt32()
{
    int32_t big_end32{0};
    std::memcpy(&big_end32, Peek(), sizeof(big_end32));
    return be32toh(big_end32);
}

int64_t Buffer::PeekInt64()
{
    int64_t big_end64{0};
    std::memcpy(&big_end64, Peek(), sizeof(big_end64));
    return be32toh(big_end64);
}

// 添加大端字节数到 prependable 区域
void Buffer::PrependInt8(int8_t x)
{
    Prepend(&x, sizeof(x));
}

void Buffer::PrependInt16(int16_t x)
{
    int16_t big_end16 = htobe16(x);
    Prepend(&big_end16, sizeof(big_end16));
}

void Buffer::PrependInt32(int32_t x)
{
    int32_t big_end32 = htobe32(x);
    Prepend(&big_end32, sizeof(big_end32));
}

void Buffer::PrependInt64(int64_t x)
{
    int64_t big_end64 = htobe64(x);
    Prepend(&big_end64, sizeof(big_end64));
}


// 添加len长度数据到prependable区域
void Buffer::Prepend(const void* data, size_t len)
{
    reader_index_ -= len;
    const char *d = static_cast<const char*>(data);
    std::copy(d, d+len, Begin()+reader_index_);
}

void Buffer::Shrink(size_t reserve)
{
    Buffer temp;
    temp.EnsureWritableBytes(ReadableBytes() + reserve);
    temp.Append(ToStringView());
    swap(temp);
}

char* Buffer::Begin()
{
    return &*buffer_.begin();
}
const char* Buffer::Begin() const
{
    return &*buffer_.begin();
}

void Buffer::MakeSpace(size_t len)
{
    // FIXME 这里其实可以先移动数据，然后再resize，
    // 这样其实可以避免下一次更大的resize
    if(WriteableBytes() + PrependableBytes() < len + kCheapPrepend)
    {
        buffer_.resize(writer_index_ + len);
    }
    else
    {
        size_t readable = ReadableBytes();
        std::copy(Begin() + reader_index_, Begin() + writer_index_, Begin() + kCheapPrepend);
        reader_index_ = kCheapPrepend;
        writer_index_ = reader_index_ + readable;
    }
}

ssize_t Buffer::ReadFd(int fd, int* saved_errno)
{
    char extra_buf[65536];
    struct iovec vec[2];
    const size_t writable = WriteableBytes();
    vec[0].iov_base = Begin() + writer_index_;
    vec[0].iov_len = writable;
    vec[1].iov_base = extra_buf;
    vec[1].iov_len = sizeof(extra_buf);

    const int iov_count = (writable < sizeof(extra_buf)) ? 2 : 1;
    const ssize_t n = readv(fd, vec,  iov_count);
    if(n < 0)
    {
        *saved_errno = errno;
    }
    else if(static_cast<size_t>(n) <= writable)
    {
        writer_index_ += n;
    }
    else
    {
        writer_index_ = buffer_.size();
        Append(extra_buf, n-writable);
    }
    // FIXME 如果没一次性读完，要怎么办
    // 其实这里没读完也没事，因为是LT模式，会继续triggered的
    return n;

}