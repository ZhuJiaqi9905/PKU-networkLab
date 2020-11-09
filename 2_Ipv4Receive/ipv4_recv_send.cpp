/*
* THIS FILE IS FOR IP TEST
*/
// system support
#include "sysInclude.h"

extern void ip_DiscardPkt(char* pBuffer,int type);

extern void ip_SendtoLower(char*pBuffer,int length);

extern void ip_SendtoUp(char *pBuffer,int length);

extern unsigned int getIpv4Address();

// implemented by students


unsigned int getCheckSum(const char* pBuffer,unsigned int IHL){
    int len = IHL * 4;//头部的字节长度
    unsigned int sum = 0;
    for(int i = 0; i < len; i += 2){
        if(i == 10) continue;
        unsigned int num = *(unsigned short*)(pBuffer + i);
        sum += num;
    }
    while(sum > 0xFFFF){
        sum = (sum >> 16) + (sum & 0xFFFF);
    }

    return (~sum)&0xFFFF;
}
bool check_packet(char* pBuffer){
    unsigned int version = (pBuffer[0] >> 4) & 0xF;
    if(version != 4){
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_VERSION_ERROR);
        return false;
    }
    unsigned int IHL = pBuffer[0] & 0xF;//报头长度
    if(IHL < 5){
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_HEADLEN_ERROR);
        return false;
    }
    int TTL = pBuffer[8];//time to live
    if(TTL <= 0){
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_TTL_ERROR);
        return false;
    }
    unsigned int destAddr = ntohl(*(unsigned int*)(pBuffer + 16));
	
    if(destAddr != getIpv4Address()){
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_DESTINATION_ERROR);
        return false;
    }
    unsigned int headerCheckSum = (*(unsigned short*)(pBuffer + 10)) & 0xFFFF;
    if(headerCheckSum != getCheckSum(pBuffer, IHL)){
        ip_DiscardPkt(pBuffer, STUD_IP_TEST_CHECKSUM_ERROR);
        return false;
    }
    return true;
}

int stud_ip_recv(char *pBuffer,unsigned short length)
{
	if(check_packet(pBuffer)){
        ip_SendtoUp(pBuffer, length);
        return 0;
    }
    else{
        return 1;
    }
}

int stud_ip_Upsend(char *pBuffer,unsigned short len,unsigned int srcAddr,
				   unsigned int dstAddr,byte protocol,byte ttl)
{
    char* packet = (char*) malloc(len + 20);
    memset(packet, 0, len + 20);
    packet[0] = 0x45;//version 4, IHL 5
    unsigned short total_len = htons(len + 20);
    memcpy(packet + 2, &total_len, sizeof(unsigned short));
    srand (time(0)) ;
    unsigned short identification = rand();
    identification = htonl(identification);
    memcpy(packet + 4, &identification, sizeof(unsigned short));
    packet[8] = ttl;
    srcAddr = htonl(srcAddr);
    memcpy(packet + 12, &srcAddr, sizeof(unsigned int));
    dstAddr = htonl(dstAddr);
    memcpy(packet + 16, &dstAddr, sizeof(unsigned int));
    unsigned short checkSum = getCheckSum(packet, 5);
    memcpy(packet + 10, &checkSum, sizeof(unsigned short));
    memcpy(packet + 20, pBuffer, len);
    ip_SendtoLower(packet, len + 20);
    free(packet);
}
