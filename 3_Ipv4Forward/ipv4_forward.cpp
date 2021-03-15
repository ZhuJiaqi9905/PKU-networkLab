/*
* THIS FILE IS FOR IP FORWARD TEST
*/
#include "sysInclude.h"
#include<stack>
using std::stack;
// system support
extern void fwd_LocalRcv(char *pBuffer, int length);

extern void fwd_SendtoLower(char *pBuffer, int length, unsigned int nexthop);

extern void fwd_DiscardPkt(char *pBuffer, int type);

extern unsigned int getIpv4Address( );

// implemented by students

struct Node{
    Node* child[2];
    bool flag;
    unsigned int nexthop;
    Node(){
        for(int i = 0; i < 2; ++i){
            child[i] = NULL;
        }
        flag = false;
        nexthop = 0;
    }
};
struct TrieTree{
    Node* root;
    TrieTree(){
        root = new Node();
    }
    ~TrieTree(){
        deleteNode(root);
    }
    void deleteNode(Node* nd){
        for(int i = 0; i < 2; ++i){
            if(nd->child[i] != NULL){
                deleteNode(nd->child[i]);
            }
        }
        delete nd;
        nd = NULL;
    }
    void add(unsigned int prefix, unsigned int nexthop){
        stack<unsigned int> stk;
        while(prefix > 0){
            stk.push(prefix % 2);
            prefix = prefix / 2;
        }
        Node* nd = this->root;
        while(!stk.empty()){
            int idx = stk.top(); stk.pop();
            if(nd->child[idx] == NULL){
                nd->child[idx] = new Node();
            }
            nd = nd->child[idx];
        }
        nd->flag = true;
        nd->nexthop = nexthop;
    }
    bool getNexthop(unsigned int dest, unsigned int& nexthop){
        stack<unsigned int> stk;
        while(dest > 0){
            stk.push(dest % 2);
            dest = dest / 2;
        }
        Node* nd = this->root;
        bool find = false;
        while(!stk.empty()){
            int idx = stk.top(); stk.pop();
            if(nd->child[idx] == NULL){
                if(!find){
                    return false;
                }
            }
            nd = nd->child[idx];
            if(nd->flag){
                find = true;
                nexthop = nd->nexthop;
            }
        }
        return true;
    }
};
TrieTree routeTable;

unsigned short getCheckSum(const char* pBuffer,unsigned int IHL){
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
void stud_Route_Init()
{
	return;
}

void stud_route_add(stud_route_msg *proute)
{
    unsigned int dest = ntohl(proute->dest);
    unsigned int masklen = ntohl(proute->masklen);
    unsigned int nexthop = ntohl(proute->nexthop);
    unsigned int prefix = dest >> (32 - masklen);
    routeTable.add(prefix, nexthop);
}


int stud_fwd_deal(char *pBuffer, int length)
{
    unsigned int destAddr = ntohl(*(unsigned int*)(pBuffer + 16));
    unsigned int IHL = pBuffer[0] & 0xF;
    unsigned int TTL = pBuffer[8];//time to live
    if(destAddr == getIpv4Address()){//本机ip
        fwd_LocalRcv(pBuffer, length);
        return 0;
    }
    if(TTL == 0){
        fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_TTLERROR);
        return 1;
    }
    unsigned int nexthop = 0;
    bool find = routeTable.getNexthop(destAddr, nexthop);
    if(find){
        TTL -= 1;
        pBuffer[8] = TTL;
        unsigned short checkSum = getCheckSum(pBuffer, IHL);
        memcpy(pBuffer + 10, &checkSum, sizeof(unsigned short));
        fwd_SendtoLower(pBuffer, length, nexthop);
        return 0;
    }else{
        fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_NOROUTE);
        return 1;
    }
}

