//
// Created by rjd67 on 2021/3/2.
//

#ifndef CSAPP_BUFFER_H
#define CSAPP_BUFFER_H

#include <vector>

class Buffer
{
public:
	Buffer();

	int Readable() const;
	int Writeable() const;
	char* ReadBegin();
	char* WriteBegin();
	void AddReadIdx(int idx);
	void AddWriteIdx(int idx);
	void RemoveReadData();
	char* Find(char c);
	ssize_t ReadDataFromFd(int fd);
private:
	int read_idx_;
	int write_idx_;
	std::vector<char> buffer_;
};
#endif //CSAPP_BUFFER_H
