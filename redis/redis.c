#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/select.h>
#include <ctype.h>

/* 定义socket可排队个数 */
#define BACKLOG			32
/* 此宏用来计算数组的元素个数 */
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

int socket_server_init(char *servip, int port);

int main(int argc, char **argv)
{
	int		listen_fd = -1;
	int		client_fd = -1;
	int		rv = -1;
	int		port;
	int		max_fd = -1;
	fd_set	rdset;
	int		fds_array[1024];
	int		i;
	int		found;
	char	buf[1024];
	
	/* 用来确认程序执行的格式是否正确，不正确则退出并提醒用户 */
	// if (argc < 2)
	// {
	// 	printf("printf:Program Usage: %s [Port]\n", argv[0]);
    //     return 0;
	// }
	
	//将端口参数赋给参数变量
	//由于命令行传参进来是字符串类型，所以需要atoi转换为整型
	port = 9999;
		
	/* 创建listen_fd，这里封装了一个函数 */
	if ((listen_fd = socket_server_init(NULL, port)) < 0)
	{
		printf("printf:socket_server_init failure\n");
		return -1;
	}
	printf("printf:listen listen_fd[%d] on port[%d]\n", listen_fd, port);
	
	/* 将数组中所有元素设置为-1，表示为空 */
	for (i=0; i<ARRAY_SIZE(fds_array); i++)
	{
		fds_array[i] = -1;
	}
	fds_array[0] = listen_fd;

	while (1)
	{	
		/* 将rdset集合的内容清空 */
		FD_ZERO(&rdset);
		max_fd = 0;
		
		/* 把数组中的fd放入集合 */
		for (i=0; i<ARRAY_SIZE(fds_array); i++)
		{
			if (fds_array[i] < 0)
				continue;
			FD_SET(fds_array[i], &rdset);
			max_fd = fds_array[i]>max_fd ? fds_array[i] : max_fd;
		}
		
		/* 开始select */
		if ((rv = select(max_fd+1, &rdset, NULL, NULL, NULL)) < 0)
		{
			printf("printf:select() failure；%s\n", strerror(errno));
			goto cleanup; 
		}
		else if (rv == 0)
		{
			printf("printf:select() timeout\n");
			continue;
		}
		
		/* 有消息来了 */
		/* 判断是不是listen_fd的消息 */
		if (FD_ISSET(fds_array[0], &rdset))
		{	
			/*
			 * accept()
			 * 接受来自客户端的连接请求
			 * 返回一个client_fd与客户通信
			 */
			if ((client_fd = accept(listen_fd, (struct sockaddr *)NULL, NULL)) < 0)
			{
				printf("printf:accept new client failure: %s\n", strerror(errno));
				continue;
			}
			
			/*
			 * 在把client_fd放到数组中的空位中
			 * （元素的值为-1的地方）
			 */
			found = 0;
			for (i=0; i<ARRAY_SIZE(fds_array); i++)
			{
				if (fds_array[i] < 0)
				{
					fds_array[i] = client_fd;
					found = 1;
					break;
				}
			}
			
			/*
			 * 如果没找到空位，表示数组满了
			 * 不接收这个新客户端，关掉client_fd
			 */
			if (!found)
			{
				printf("printf:accept new client[%d], but full, so refuse\n", client_fd);
				close(client_fd);
			}
			printf("printf:accept new client[%d]\n", client_fd);


		}		/* end of server message */
		else	/* 来自已连接客户端的消息 */
		{
			for (i=0; i<ARRAY_SIZE(fds_array); i++)
			{	
				/* 判断fd是否有效，并且查看当前fd是否在rdset集合中 */
				if (fds_array[i] < 0 || !FD_ISSET(fds_array[i], &rdset))
					continue;
				
				/* 清空buf，以便存放读取的数据 */
				memset(buf, 0, sizeof(buf));
				if ((rv = read(fds_array[i], buf, sizeof(buf))) <= 0)
				{
					printf("printf:read data from client[%d] failure or get disconnected, so close it\n", fds_array[i]);
					close(fds_array[i]);
					fds_array[i] = -1;
					continue;
				}
				printf("printf:read %d Bytes data from client[%d]: %s\n", rv, fds_array[i], buf);
				
				/* 将小写字母转为大写 */
				for (int j=0; j<rv; j++)
				{
					if (buf[j] >= 'a' && buf[j] <= 'z')
						buf[j] = toupper(buf[j]);
				}
				
				/* 将数据发送到客户端 */
				if ((rv = write(fds_array[i], buf, rv)) < 0)
				{
					printf("printf:write data to client[%d] failure: %s\n", fds_array[i], strerror(errno));
					close(fds_array[i]);
					fds_array[i] = -1;
					continue;
				}
				printf("printf:write %d Bytes data to client[%d]: %s\n", rv, fds_array[i], buf);

			} /* end of for(i=0; i<ARRAY_SIZE(fds_array); i++) */

		} /* end of client message */

	} /* end of while(1) */

cleanup:
	close(listen_fd);

	return 0;

} /* end of main function */

/*
 * Socket Server Init Function
 * 创建 listen_fd，bind绑定ip和端口，并监听
 */
int socket_server_init(char *servip, int port)
{
	int					listen_fd = -1;
	int					rv = 0;
	int					on = 1;
	struct sockaddr_in	servaddr;
	
	/*
	 * socket()，创建一个新的sockfd
	 * 指定协议族为IPv4
	 * socket类型为SOCK_STREAM（TCP）
	 */
	if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("printf:create listen_fd failure: %s\n", strerror(errno));
		return -1;
	}
	
	//设置套接字端口可重用，修复了当Socket服务器重启时“地址已在使用(Address already in use)”的错误
	setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	
	/*
	 * bind()，将服务器的协议地址绑定到listen_fd
	 */
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	/* 若传的ip地址为空 */
	if (!servip)
	{
		/* 监听所有网卡的ip */
		servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	else
	{
		/* 将点分十进制的ip地址转为32位整型传入结构体 */
		if (inet_pton(AF_INET, servip, &servaddr.sin_addr) <= 0)
		{
			printf("printf:inet_pton() failure: %s\n", strerror(errno));
			rv = -2;
			goto cleanup;
		}

	}

	if (bind(listen_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
	{
		printf("printf:bind listenfd[%d] on port[%d] failure: %s\n", listen_fd, port, strerror(errno));
		rv = -3;
		goto cleanup;
	}
	
	/*
	 * listen()
	 * 监听listen_fd的端口，并设置最大排队连接个数
	 */
	if (listen(listen_fd, BACKLOG) < 0)
	{
		printf("printf:listen listen_fd[%d] on port[%d] failure: %s\n", listen_fd, port, strerror(errno));
		rv = -4;
		goto cleanup;
	}

cleanup:
	if (rv < 0)
		close(listen_fd);
	else
		rv = listen_fd;
	
	return rv;
}