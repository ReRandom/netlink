#include <netinet/in.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

struct route_info {
	struct in_addr dst_addr;
	struct in_addr src_addr;
	struct in_addr gateway;
	char if_name[80];
};

int main(int argc, char *argv[])
{
	int nl_sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
	int msg_seq = 0;

	//send
	{
		char buf[8192];
		struct nlmsghdr *nh = (struct nlmsghdr*) buf;
		struct rtmsg *rtmsg = (struct rtmsg*) NLMSG_DATA(nh);
		struct sockaddr_nl sa;
		struct iovec iov;
		struct msghdr msg;

		nh->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
		nh->nlmsg_type = RTM_GETROUTE;
		nh->nlmsg_flags = NLM_F_DUMP | NLM_F_REQUEST;
		nh->nlmsg_seq = msg_seq++;
		nh->nlmsg_pid = getpid();

		msg.msg_name = (void*)&sa;
		msg.msg_namelen = sizeof(sa);
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;

		memset(&sa, 0, sizeof(sa));
		sa.nl_family = AF_NETLINK;
		sa.nl_pid = 0;
		sa.nl_groups = 0;

		iov.iov_base = nh;
		iov.iov_len = nh->nlmsg_len;

		if(sendmsg(nl_sock, &msg, 0) < 0)
		{
			perror("sendmsg");
			return 1;
		}
	}

	//recv
	{
		char buf[8192];
		struct iovec iov;
		iov.iov_base = buf;
		iov.iov_len = sizeof(buf);
		struct sockaddr_nl sa;
		struct msghdr msg;
		struct nlmsghdr *nh;

		msg.msg_name = (void*)&sa;
		msg.msg_namelen = sizeof(sa);
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;

		int len = recvmsg(nl_sock, &msg, 0); 

		if(len < 0)
		{
			perror("sendmsg");
			return 1;
		}

		fprintf(stdout, "Destination\tGateway\tInterface\tSource\n");

		for(nh = (struct nlmsghdr*)buf; NLMSG_OK(nh, len); nh = NLMSG_NEXT(nh, len))
		{
			if (nh->nlmsg_type == NLMSG_DONE)
				break;

			if (nh->nlmsg_type == NLMSG_ERROR)
			{
				fprintf(stderr, "error\n");
			}

			struct rtmsg* payload = NLMSG_DATA(nh);
			if(payload->rtm_family != AF_INET || payload->rtm_table != RT_TABLE_MAIN)
			{
				//fprintf(stderr, "i don't now this address family or route's table\n");
				break;
			}
			struct rtattr *rt_attr = (struct rtattr *) RTM_RTA(payload);
			int rt_len = RTM_PAYLOAD(nh);
			struct route_info rt_info;
			for (; RTA_OK(rt_attr, rt_len); rt_attr = RTA_NEXT(rt_attr, rt_len)) 
			{
				switch (rt_attr->rta_type) {
				case RTA_OIF:
					if_indextoname(*(int *) RTA_DATA(rt_attr), rt_info.if_name);
					break;
				case RTA_GATEWAY:
					rt_info.gateway.s_addr= *(u_int *) RTA_DATA(rt_attr);
					break;
				case RTA_PREFSRC:
					rt_info.src_addr.s_addr= *(u_int *) RTA_DATA(rt_attr);
					break;
				case RTA_DST:
					rt_info.dst_addr .s_addr= *(u_int *) RTA_DATA(rt_attr);
					break;
				}
			}

			{
				if (rt_info.dst_addr.s_addr != 0)
					printf("%s\t", inet_ntoa(rt_info.dst_addr));
				else
					printf("*.*.*.*\t");

				if (rt_info.gateway.s_addr != 0)
					printf("%s\t", inet_ntoa(rt_info.gateway));
				else
					printf("*.*.*.*\t");

				printf("%s\t", rt_info.if_name);

				if (rt_info.src_addr.s_addr != 0)
					printf("%s\t", inet_ntoa(rt_info.src_addr));
				else
					printf("*.*.*.*\t");
				printf("\n");
			}
		}
	}
	return 0;
}