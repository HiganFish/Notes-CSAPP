#include <netdb.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>
#include <sys/stat.h>
#include <map>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include "Buffer.h"
#include "fmt/format.h"

constexpr socklen_t MAXLINE = 1024;
constexpr socklen_t MAXBUFFER = 4096;

class RioT
{
public:
	explicit RioT(int fd):
		fd_(fd),
		buffer_()
	{

	}
	/**
	 * 从RioT读取一行到buffer
	 * 同时会读取\r\n 或者\n 到buffer中
	 * 读取结果末尾不增加\0
	 * @param buffer 存储字符串的缓冲区
	 * @param buffer_size 缓冲区长度
	 * @return 读取成功 返回读取长度, 数据不足返回 0 读满后依旧无换行或参数错误 返回-1
	 */
	ssize_t ReadLine(char* buffer, size_t buffer_size)
	{
		if (!buffer)
		{
			return -1;
		}

		char* line_begin = buffer_.ReadBegin();
		char* line_end = buffer_.Find('\n');
		if (!line_end)
		{
			/**
			 * 没有读取到换行符 移除已读取内容 并读取后再次查询
			 */
			buffer_.RemoveReadData();
			buffer_.ReadDataFromFd(fd_);
			line_end = buffer_.Find('\n');
			if (!line_end)
			{
				/**
				 * 如果缓冲区已经读满 但仍然没有换行则返回 -1 否则返回0
				 */
				return buffer_.Writeable() == 0 ? -1 : 0;
			}
		}

		int line_length = line_end - line_begin + 1;
		if (line_length + 1 > buffer_size)
		{
			return -1;
		}
		memcpy(buffer, line_begin, line_length);
		buffer_.AddReadIdx(line_length);
		buffer[line_length] = '\0';
		return line_length;
	}
private:
	int fd_;
	Buffer buffer_;
};

int OpenListenFd(const char* port)
{
	struct addrinfo hints{};
	hints.ai_socktype = SOCK_STREAM;

	// Socket address is intended for `bind'
	// Use configuration of this host to choose
	// 		returned address type..
	hints.ai_flags |= AI_PASSIVE | AI_ADDRCONFIG | AI_NUMERICSERV;

	struct addrinfo* result;
	getaddrinfo(nullptr, port, &hints, &result);

	int listenfd;
	int on = 1;
	struct addrinfo* p;
	for (p = result; p; p = p->ai_next)
	{
		if ((listenfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) < 0)
		{
			fprintf(stderr, "socket error: %s\n", strerror(errno));
			continue;
		}

		setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
				static_cast<const void*>(&on),
				static_cast<socklen_t>(sizeof on));
		if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0)
		{
			break;
		}
		close(listenfd);
	}
	freeaddrinfo(result);
	if (!p)
	{
		return -1;
	}
	if (listen(listenfd, 100) < 0)
	{
		return -1;
	}
	return listenfd;
}

void ClientError(int fd, const char* cause, const char* errnum, const char* shortmsg,
		const char* longmsg)
{
	std::string body;
	body.reserve(MAXBUFFER);

	body += "<html><title>Tiny Error</title>";
	body += "<body bgcolor=""ffffff"">\r\n";
	body += fmt::format("{}: {}\r\n", errnum, shortmsg);
	body += fmt::format("<p>{}: {}\r\n", longmsg, cause);
	body += "<hr><em>The Tiny Web server</em>\r\n";

	std::string header;
	header.reserve(MAXLINE);

	header += fmt::format("HTTP/1.0 {} {}\r\n", errnum, shortmsg);
	header += "Content-type: text/html\r\n";
	header += fmt::format("Content-length: {}\r\n\r\n", body.length());

	write(fd, header.c_str(), header.size());
	write(fd, body.c_str(), body.size());
}

void ReadRequestHeaders(RioT* rio_t)
{
	char buffer[MAXLINE];

	rio_t->ReadLine(buffer, sizeof buffer);
	while (strcmp(buffer, "\r\n") != 0)
	{
		rio_t->ReadLine(buffer, sizeof buffer);
	}
}

/**
 * 解析uri 填充参数 并返回是否是静态网页请求
 * @param uri 传入的uri参数
 * @param filename 保存文件名 静态网页默认文件名 index.html 动态文件名 .cginame
 * @param cgiargs 保存agiargs 不存在置为 \0
 * @return true 静态网页请求 false cgi请求
 */
bool ParseUri(char* uri, char* filename, char* cgiargs)
{
	if (!strstr(uri, "cgi-bin"))
	{
		strcpy(cgiargs, ""); // copy terminating null byte
		strcpy(filename, ".");
		strcat(filename, uri);
		if (uri[strlen(uri) - 1] == '/')
		{
			strcat(filename, "index.html");
		}
		return true;
	}
	else
	{
		char* ptr = strchr(uri, '?'); // change index to strchr
		if (ptr)
		{
			strcpy(cgiargs, ptr + 1);
			*ptr = '\0'; // remove '?' and cgiargs
		}
		else
		{
			strcpy(cgiargs, ""); // copy terminating null byte
		}
		strcpy(filename, ".");
		strcat(filename, uri);
		return false;
	}
}

std::map<std::string, std::string> file_type_map{
		{"html", "text/html"},
		{"gif", "image/gif"},
		{"png", "image/png"},
		{"jpg", "image/ipeg"}};

void GetFileType(char* filename, std::string* filetype)
{
	std::string suffix(strrchr(filename, '.') + 1);

	auto iter = file_type_map.find(suffix);
	if (iter != file_type_map.end())
	{
		*filetype = iter->second;
	}
	else
	{
		filetype->append("text/plain");
	}
}

void ServerStatic(int fd, char* filename, size_t file_size)
{
	std::string file_type;
	GetFileType(filename, &file_type);

	std::string header;
	header.reserve(MAXBUFFER);
	header += "HTTP/1.0 200 OK\r\n";
	header += "Server: Tiny Web Server\r\n";
	header += "Connection: close\r\n";
	header += fmt::format("Content-length: {}\r\n", file_size);
	header += fmt::format("Content-type: {}\r\n\r\n", file_type);

	write(fd, header.c_str(), header.length());

	std::cout << "Response headers:\n";
	std::cout << header;

	int file_fd = open(filename, O_RDONLY, 0);
	char* srcp = static_cast<char*>(
			mmap(0, file_size, PROT_READ, MAP_PRIVATE,
					file_fd, 0));
	close(file_fd);

	write(fd, srcp, file_size);
	munmap(srcp, file_size);
}

void ServerDynamic(int fd, char* filename, char* cgiargs)
{
	std::string header;
	header.reserve(MAXBUFFER);
	header += "HTTP/1.0 200 OK\r\n";
	header += "Server: Tiny Web Server\r\n";
	write(fd, header.c_str(), header.length());

	if (fork() == 0)
	{
		setenv("QUERY_STRING", cgiargs, 1);
		dup2(fd, STDOUT_FILENO);
		execl(filename, nullptr, environ);
	}
	wait(nullptr);
}

void Doit(int connfd)
{
	RioT rio_t(connfd);
	char buffer[MAXLINE];
	rio_t.ReadLine(buffer, sizeof buffer);

	printf("Request headers:\n%s", buffer);

	char method[MAXLINE];
	char uri[MAXLINE];
	char version[MAXLINE];
	sscanf(buffer, "%s %s %s", method, uri, version);

	if (strcasecmp(method, "GET") != 0)
	{
		ClientError(connfd, method, "501", "Not Implemented",
				"Tiny does not implement this method");
		return;
	}

	ReadRequestHeaders(&rio_t);

	char filename[MAXLINE];
	char cgiargs[MAXLINE];
	bool is_static = ParseUri(uri, filename, cgiargs);

	struct stat status;
	if (stat(filename, &status) < 0)
	{
		ClientError(connfd, filename, "404", "Not found",
				"Tiny couldn't find this file");
		return;
	}

	if (is_static)
	{
		/**
		 * S_ISREG 是否是常规文件
		 * S_IRUSR read by owner
		 */
		if (!S_ISREG(status.st_mode) ||
				!(S_IRUSR & status.st_mode))
		{
			ClientError(connfd, filename, "403", "Forbidden",
					"Tiny couldn't read the file");
			return;
		}
		ServerStatic(connfd, filename, status.st_size);
	}
	else
	{
		if (!S_ISREG(status.st_mode) ||
				!(S_IRUSR & status.st_mode))
		{
			ClientError(connfd, filename, "403", "Forbidden",
					"Tiny couldn't run the CGI program");
			return;
		}
		ServerDynamic(connfd, filename, cgiargs);
	}
}

int main(int argc, char* argv[])
{
	int opt;
	char* port;
	while ((opt = getopt(argc, argv, "p:")) != -1)
	{
		switch (opt)
		{
		case 'p':
			port = optarg;
			break;
		default:
			fprintf(stderr, "Usage: %s [-p port]", argv[0]);
			exit(-1);
		}
	}

	int listenfd = OpenListenFd(port);
	struct sockaddr_storage clientaddr{};
	socklen_t clientaddr_len =
			static_cast<socklen_t>(sizeof clientaddr);

	char client_hostname[MAXLINE];
	char client_port[MAXLINE];

	while (true)
	{
		int connfd = accept(listenfd,
				reinterpret_cast<sockaddr*>(&clientaddr),
				&clientaddr_len);

		if (connfd < 0)
		{
			break;
		}
		getnameinfo(reinterpret_cast<sockaddr*>(&clientaddr), clientaddr_len,
				client_hostname, MAXLINE,
				client_port, MAXLINE, NI_NUMERICHOST | NI_NUMERICSERV);
		printf("Accept connection from (%s:%s)\n", client_hostname, client_port);

		Doit(connfd);
		close(connfd);
	}
}