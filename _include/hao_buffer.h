#ifndef _HAO_BUFFER_H_
#define _HAO_BUFFER_H_

#include <vector>
#include <string>
#include <string_view>

using std::string;
using std::vector;
using std::string_view;

namespace hao_util
{
    class Buffer
    {
        public:
            explicit Buffer(size_t initial_size = kInitialSize);
            void swap(Buffer& rhs);
             // 可读数据大小
            size_t ReadableBytes() const;
            // 可写数据大小
            size_t WriteableBytes() const;
            // 头部预留空间大小
            // 头部空闲空间大小
            size_t PrependableBytes() const;
            // 可读区域的起始地址
            const char* Peek() const;
            // 可写区域的起始地址
            char* BeginWrite();
            const char* BeginWrite() const;
            // 查找第一个CRLF
            const char* FindCRLF() const;
            // 从start开始查找第一个CRLF
            const char* FindCRLF(const char *start) const;
            // 查找第一个回车符
            const char* FindEOL() const;
            // 从start查找第一个回车符
            const char* FindEOL(const char *start) const;
            // 缓冲区可读容量减少len
            void Retrieve(size_t len);
            // 减少缓冲区容量，可取取余的起始地址移动到end指向的位置
            void RetrieveUtil(const char *end);
            // 缓冲区可读容量减少sizeof(int8_t)
            void RetrieveInt8();
            // 缓冲区可读容量减少sizeof(int16_t)
            void RetrieveInt16();
            // 缓冲区可读容量减少sizeof(int32_t)
            void RetrieveInt32();
            // 缓冲区可读容量减少sizeof(int 64)
            void RetrieveInt64();
            // 重置缓冲区
            void RetrieveAll();

            // 将可读区域容量减少到0，并读出该部分数据
            string RetrieveAllAsString();
            // 缓冲区可读区域容量减少len，并读取该部分数据
            string RetrieveAsString(size_t len);
            // 读出可读区域的数据
            string_view ToStringView() const;

            // 数据添加接口
            void Append(string_view str);
            void Append(const char *data, size_t len);
            void Append(const void *data, size_t len);
            void AppendInt8(int8_t);
            void AppendInt16(int16_t);
            void AppendInt32(int32_t);
            void AppendInt64(int64_t);

            // 确保可写区域能容得下len大小的数据，如果不够，则自动调整或扩容
            void EnsureWritableBytes(size_t len);
            // 写入数据，更新写指针
            void HasWritten(size_t len);
            void UnWrite(size_t len);

            // 读取整数，并调整对应缓冲区
            int8_t  ReadInt8();
            int16_t ReadInt16();
            int32_t ReadInt32();
            int64_t ReadInt64();

            // 只是读取，并不读出
            int8_t  PeekInt8();
            int16_t PeekInt16();
            int32_t PeekInt32();
            int64_t PeekInt64();

            // 添加大端字节数到 prependable 区域
            void PrependInt8(int8_t);
            void PrependInt16(int16_t);
            void PrependInt32(int32_t);
            void PrependInt64(int64_t);
            
            // 添加len长度数据到prependable区域
            void Prepend(const void* data, size_t len);

            void Shrink(size_t reserve);

            ssize_t ReadFd(int fd, int* saved_errno);

        private:
            vector<char> buffer_;
            size_t reader_index_;
            size_t writer_index_;

            static const size_t kInitialSize;
            static const size_t kCheapPrepend;

            // 获取缓冲区首地址
            char* Begin();
            const char* Begin() const;

            void MakeSpace(size_t len);
    };
}

#endif