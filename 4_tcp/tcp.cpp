#include "sysInclude.h"
#include <cstring>
/*
struct sockaddr_in {
    short   sin_family;
    u_short sin_port;
    struct  in_addr sin_addr;
    char    sin_zero[8];
};
*/
extern void tcp_DiscardPkt(char * pBuffer, int type);

extern void tcp_sendIpPkt(unsigned char* pData, uint16 len, unsigned int srcAddr, unsigned int dstAddr, uint8 ttl);

extern int waitIpPacket(char *pBuffer, int timeout);

extern void tcp_sendReport(int type);

extern UINT32 getIpv4Address( );

extern UINT32 getServerIpv4Address( );
int gSrcPort = 2005;
int gDstPort = 2006;
int gSeqNum = 1;
int gAckNum = 1;

enum STAT{
    ESTABLISHED,
    FIN_WAIT1,
    FIN_WAIT2,
    SYN_SENT,
    TIME_WAIT,
    CLOSED,
};

int lend = 0;
struct TCB{
    uint32_t dstAddr;
    uint32_t srcAddr;
    uint16_t dstPort;
    uint16_t srcPort;
    uint32_t window;
    uint32_t recSeq; //下一个要收到的序列号
    uint32_t sendSeq; // 下一个要发送的序列号
    STAT status;
}tcbList[100];
struct TCPHead{
	uint16_t srcPort;
	uint16_t dstPort;
	uint32_t seq;
	uint32_t ack;
	uint16_t flags;
	uint16_t window;
	uint16_t checksum;
	uint16_t uptr;
};
int getTcbByIp(uint32_t srcAddr, uint32_t dstAddr, uint16_t srcPort, uint16_t dstPort){
    for(int i = 0; i < lend; ++i){
        if(tcbList[i].srcAddr == srcAddr && tcbList[i].dstAddr == dstAddr && tcbList[i].srcPort == srcPort && tcbList[i].dstPort == dstPort){
            return i;
        }
    }
    return -1;
}


uint16_t compute(uint16_t x, uint16_t y){
    uint32_t res = x + y;
    res = (res >> 16) + (res&0xFFFF);
    return (uint16_t) res;
}
uint16_t calCheckSum(unsigned char* pBuffer, unsigned short len, uint32_t srcAddr, uint32_t dstAddr){
    uint16_t sum = 0;
    sum = compute(sum, (htonl(srcAddr)) &0xFFFF);
    sum = compute(sum, htonl(srcAddr) >> 16);
    sum = compute(sum, (htonl(dstAddr)) &0xFFFF);
    sum = compute(sum, htonl(dstAddr) >> 16);
    sum = compute(sum, len);
    sum = compute(sum, 0x6);
    uint16_t* ptr = (uint16_t*) pBuffer;
    for(int i = 0; i < len/2; ++i){
        if(i != 8) sum = compute(sum, htons(ptr[i]));
        if(len % 2){
            sum = compute(sum, htons((uint16_t) pBuffer[len - 1]<<8));
        }
    }
    sum = htons(sum);
    return ~sum;
}

int stud_tcp_input(char *pBuffer, unsigned short len, unsigned int srcAddr, unsigned int dstAddr)
{
    printf("stud_tcp_input \n");
    unsigned int temp = ntohl(srcAddr);
    srcAddr = ntohl(dstAddr);
    dstAddr = temp;
    TCPHead* tcphdr = (TCPHead*) pBuffer;
    uint32_t seq = ntohl(tcphdr->seq);
    uint16_t srcPort = ntohs(tcphdr->dstPort);
    uint16_t dstPort = ntohs(tcphdr->srcPort);
    int idx = getTcbByIp(srcAddr, dstAddr, srcPort, dstPort);
    printf("idx %d\n", idx);
    if(tcbList[idx].status != SYN_SENT && seq != tcbList[idx].recSeq){
        tcp_DiscardPkt(pBuffer, STUD_TCP_TEST_SEQNO_ERROR);
        return -1;
    }
    else if(ntohs(tcphdr->flags) & 0x13 != 0x10){//如果收到的是纯ACK，seq不用加。否则都要加。
        tcbList[idx].recSeq++;
    }
    if(tcbList[idx].status == SYN_SENT){
        //它应该收到一个SYN_ACK
        if((ntohs(tcphdr->flags) & 0x13) == 0x12){
            if(tcbList[idx].sendSeq + 1 != ntohl(tcphdr->ack)){
                tcp_DiscardPkt(pBuffer,STUD_TCP_TEST_SEQNO_ERROR);
				return -1;
            }
            tcbList[idx].sendSeq++;
            tcbList[idx].recSeq = ntohl(tcphdr->seq) + 1;
            //发送ack
            stud_tcp_output(NULL, 0, PACKET_TYPE_ACK,tcbList[idx].srcPort, tcbList[idx].dstPort, tcbList[idx].srcAddr, tcbList[idx].dstAddr);
            tcbList[idx].status = ESTABLISHED;
            return 0;
        }else{
            return -1;
        }
        
    }
    else if(tcbList[idx].status == ESTABLISHED){
        //收到数据
        if((ntohs(tcphdr->flags) & 0x13) == 0){
            //data
            return 0;
        }
        if((ntohs(tcphdr->flags) & 0x13) == 0x10){
            //收到ACK
            if(tcbList[idx].sendSeq + 1 != ntohl(tcphdr->ack)){
                tcp_DiscardPkt(pBuffer,STUD_TCP_TEST_SEQNO_ERROR);
				return -1;
            }
            else{
                tcbList[idx].sendSeq++; 
                return 0;
            }
        }
        else if((ntohs(tcphdr->flags) & 0x13) == 0x11){
            //收到FIN_ACK
            if(tcbList[idx].sendSeq + 1 != ntohl(tcphdr->ack)){
                tcp_DiscardPkt(pBuffer,STUD_TCP_TEST_SEQNO_ERROR);
				return -1;
            }
            else{
                tcbList[idx].sendSeq++;
                stud_tcp_output(NULL, 0, PACKET_TYPE_ACK,tcbList[idx].srcPort, tcbList[idx].dstPort, tcbList[idx].srcAddr, tcbList[idx].dstAddr);
				tcbList[idx].status = CLOSED;
                return 0;
            }
        }
    }
    else if(tcbList[idx].status == FIN_WAIT1){
        if((ntohs(tcphdr->flags) & 0x13) == 0x10){//ACK
            if(tcbList[idx].sendSeq + 1 != ntohl(tcphdr->ack)){
                tcp_DiscardPkt(pBuffer,STUD_TCP_TEST_SEQNO_ERROR);
				return -1;
            }else{
                tcbList[idx].sendSeq++;
                tcbList[idx].status = FIN_WAIT2;
                return 0;
            }
        }
        if((ntohs(tcphdr->flags) & 0x13) == 0x11){//FIN_ACK
            if(tcbList[idx].sendSeq + 1 != ntohl(tcphdr->ack)){
                tcp_DiscardPkt(pBuffer,STUD_TCP_TEST_SEQNO_ERROR);
				return -1;
            }else{
                tcbList[idx].sendSeq++;
                stud_tcp_output(NULL, 0, PACKET_TYPE_ACK,tcbList[idx].srcPort, tcbList[idx].dstPort, tcbList[idx].srcAddr, tcbList[idx].dstAddr);
                tcbList[idx].status = CLOSED;
                return 0;
            }
        }
    }
    else if(tcbList[idx].status == FIN_WAIT2){
        //应该收到FIN_ACK
        if((ntohs(tcphdr->flags) & 0x13) == 0x11 ) {
            stud_tcp_output(NULL, 0, PACKET_TYPE_ACK,tcbList[idx].srcPort, tcbList[idx].dstPort, tcbList[idx].srcAddr, tcbList[idx].dstAddr);
            tcbList[idx].status = CLOSED;
            return 0;
        }
    }
    return -1;
}

void stud_tcp_output(char *pData, unsigned short len, unsigned char flag, unsigned short srcPort, unsigned short dstPort, unsigned int srcAddr, unsigned int dstAddr){
    int idx = getTcbByIp(srcAddr, dstAddr, srcPort, dstPort);
    TCB* curr;
    if(idx == -1){
       //第一次
       curr = tcbList + lend;
       curr->srcAddr = srcAddr;
       curr->dstAddr = dstAddr;
       curr->srcPort = srcPort;
       curr->dstPort = dstPort;
       curr->sendSeq = gSeqNum;
       curr->recSeq = gAckNum;
       curr->status = CLOSED;
       lend++;
    }
    else{
        curr = tcbList + idx;
    }
    char pBuffer[len + 20];
    memset(pBuffer, 0, sizeof(pBuffer));
    TCPHead* tcphdr = (TCPHead*) pBuffer;
    tcphdr->srcPort = htons(srcPort);
    tcphdr->dstPort = htons(dstPort);
    tcphdr->window = htons(0x03E8);
    tcphdr->seq = htonl(curr->sendSeq);
    tcphdr->ack = htonl(curr->recSeq);
    switch(flag){
        case PACKET_TYPE_DATA:
            tcphdr->flags = htons(0x5000);
            break;
        case PACKET_TYPE_SYN:
            tcphdr->flags = htons(0x5002);
            curr->status = SYN_SENT;
            break;
        case PACKET_TYPE_SYN_ACK:
            tcphdr->flags = htons(0x5012);
            break;
        case PACKET_TYPE_ACK:
            tcphdr->flags = htons(0x5010);
            break;
        case PACKET_TYPE_FIN_ACK:
            tcphdr->flags = htons(0x5011);
            curr->status = FIN_WAIT1;
            break;
        default:
            printf("in stud_tcp_output_switch\n");
    }
    if(len != 0) memcpy(pBuffer + 20, pData, len);
    tcphdr->checksum = calCheckSum((unsigned char*)pBuffer, len + 20, ntohl(srcAddr), ntohl(dstAddr));
    tcp_sendIpPkt((unsigned char*)pBuffer,len+20,srcAddr,dstAddr, 50);

}
int stud_tcp_socket(int domain, int type, int protocol){
    printf("stud_tcp_socket\n");
    tcbList[lend].sendSeq = gSeqNum;
    tcbList[lend].recSeq = gAckNum;
    tcbList[lend].status = CLOSED;
    tcbList[lend].window = 0;
    tcbList[lend].srcAddr = getIpv4Address();
    tcbList[lend].srcPort = 1024 + lend;
    lend++;
    return lend - 1;
}
int stud_tcp_connect(int sockfd, struct sockaddr_in *addr, int addrlen){
    printf("stud_tcp_connect\n");
    tcbList[sockfd].dstAddr = ntohl(addr->sin_addr.s_addr);
    tcbList[sockfd].dstPort = ntohs(addr->sin_port);
    stud_tcp_output(NULL,0,PACKET_TYPE_SYN,tcbList[sockfd].srcPort,tcbList[sockfd].dstPort,tcbList[sockfd].srcAddr,tcbList[sockfd].dstAddr);
    tcbList[sockfd].status = SYN_SENT;
	char pBuffer[1024];
	int len = waitIpPacket(pBuffer,600);
	if (len == -1) return -1;
	if (stud_tcp_input(pBuffer, len, htonl(tcbList[sockfd].dstAddr), htonl(tcbList[sockfd].srcAddr)) == -1) return -1;

	return 0;
}

int stud_tcp_send(int sockfd, const unsigned char *pData, unsigned short datalen, int flags){
    printf("stud_tcp_send\n");
    if(tcbList[sockfd].status != ESTABLISHED) return -1;
    //发送报文
    stud_tcp_output((char*)pData,datalen,flags,tcbList[sockfd].srcPort,tcbList[sockfd].dstPort, tcbList[sockfd].srcAddr, tcbList[sockfd].dstAddr);
	//接收ACK
    char pBuffer[1024];
    int len;
    if((len = waitIpPacket(pBuffer, 600)) == -1) return -1;
    TCPHead* tcphdr = (TCPHead*) pBuffer;
    if((ntohs(tcphdr->flags) & 0x13) == 0x10){
        tcbList[sockfd].sendSeq = ntohl(tcphdr->ack);
        return 0;
    }
    else{
        return -1;
    }
}

int stud_tcp_recv(int sockfd, unsigned char *pData, unsigned short datalen, int flags){
    printf(" stud_tcp_recv\n");
    char pBuffer[1024];
    int len;
    if(tcbList[sockfd].status != ESTABLISHED) return -1;
    //接收报文
   if((len = waitIpPacket(pBuffer, 600)) == -1){
        return -1;
    }
    TCPHead* tcphdr = (TCPHead*) pBuffer;
    int headLen = ((ntohs(tcphdr->flags)) >> 10);
    tcbList[sockfd].recSeq = ntohl(tcphdr->seq) + 4;
    memcpy(pData, pBuffer + headLen, len - headLen);
    //发送ACK
    stud_tcp_output(NULL,0,PACKET_TYPE_ACK,tcbList[sockfd].srcPort,tcbList[sockfd].dstPort,tcbList[sockfd].srcAddr,tcbList[sockfd].dstAddr);
	return 0;
}

int stud_tcp_close(int sockfd){
    if(tcbList[sockfd].status != ESTABLISHED){
        tcbList[sockfd].status = CLOSED;
        return -1;
    }
    int len;
    char pBuffer[1024];
    //发送FIN_ACK
    stud_tcp_output(NULL, 0, PACKET_TYPE_FIN_ACK, tcbList[sockfd].srcPort, tcbList[sockfd].dstPort, tcbList[sockfd].srcAddr, tcbList[sockfd].dstAddr);
    tcbList[sockfd].status = FIN_WAIT1;
    //接收ACK
    if((len = waitIpPacket(pBuffer, 600) == -1)){
        return -1;
    }
    TCPHead* tcphdr = (TCPHead*) pBuffer;
    if((ntohs(tcphdr->flags) & 0x13) == 0x10){
        tcbList[sockfd].sendSeq = ntohl(tcphdr->ack);
    }else{
        return -1;
    }
    //接收FIN_ACK
    if((len = waitIpPacket(pBuffer, 600)) == -1){
        return -1;
    }
    tcphdr = (TCPHead*) pBuffer;
    if((htons(tcphdr->flags) & 0x13) == 0x11){
        tcbList[sockfd].recSeq = ntohl(tcphdr->seq) + 1;
    }else{
        return -1;
    }
    //发送ACK
    stud_tcp_output(NULL, 0, PACKET_TYPE_ACK, tcbList[sockfd].srcPort, tcbList[sockfd].dstPort, tcbList[sockfd].srcAddr, tcbList[sockfd].dstAddr);
    tcbList[sockfd].status = CLOSED;
    return 0;
}