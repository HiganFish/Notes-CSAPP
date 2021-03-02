//
// Created by rjd67 on 2021/3/2.
//
#include <cstring>
#include <unistd.h>
#include "Buffer.h"

constexpr int BUFFER_SIZE = 8192;

Buffer::Buffer():
		read_idx_(0),
		write_idx_(0),
		buffer_(BUFFER_SIZE)
{

}

int Buffer::Readable() const
{
	return write_idx_ - read_idx_;
}
int Buffer::Writeable() const
{
	return BUFFER_SIZE - write_idx_;
}
char* Buffer::ReadBegin()
{
	return &buffer_[read_idx_];
}
char* Buffer::WriteBegin()
{
	return &buffer_[write_idx_];
}
void Buffer::AddReadIdx(int idx)
{
	read_idx_ += idx;
}
void Buffer::AddWriteIdx(int idx)
{
	write_idx_ += idx;
}
void Buffer::RemoveReadData()
{
	int readable = Readable();
	memcpy(&buffer_[0], ReadBegin(),
			readable);
	read_idx_ = 0;
	write_idx_ = readable;
}
char* Buffer::Find(char c)
{
	return static_cast<char*>(memchr(ReadBegin(), c,
			Readable()));
}
ssize_t Buffer::ReadDataFromFd(int fd)
{
	ssize_t ret = read(fd, WriteBegin(), Writeable());
	if (ret > 0)
	{
		AddWriteIdx(ret);
	}
	return ret;
}