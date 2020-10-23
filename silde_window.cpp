#include "sysinclude.h"

#include <stdlib.h>
#include <string.h>
#include<queue>
#include<vector>
#include<Winsock2.h>
using std::queue;

#define UINT8 int
#define MSG_TYPE_TIMEOUT 1
#define MSG_TYPE_SEND 2
#define MSG_TYPE_RECEIVE 3

#define WINDOW_SIZE_STOP_WAIT 1
#define WINDOW_SIZE_BACK_N_FRAME 4

extern void SendFRAMEPacket(unsigned char* pData, unsigned int len);

typedef enum{data, ack, nak} frame_kind;
typedef struct frame_head {
    frame_kind kind;//帧类型
    unsigned int seq;//序列号
    unsigned int ack; //确认号
    unsigned char data[100];//数据
};
typedef struct frame {
    frame_head head; //帧头
    unsigned int size; //数据的大小
};

typedef struct Frame_info {
    frame* pframe;
    unsigned int size;
};


int front = 0;
int head = 0;
int stu_slide_window_stop_and_wait(char* pBuffer, int bufferSize, UINT8 messageType) {
    static Frame_info frame_cache[WINDOW_SIZE_STOP_WAIT];
    static int front = 0;//窗口起始位置
    static int length = 0;//窗口中已发出的帧个数
    static queue<Frame_info> waiting_queue;
    if(messageType == MSG_TYPE_SEND){
        //分配区域，缓存帧或者发出这个帧
        char *ptmp_frame=(char*)malloc(bufferSize);
		memcpy(ptmp_frame, pBuffer);
        Frame_info fi;
        fi.pframe = (frame*)ptmp_frame;
        fi.size = bufferSize;
        
        if( length > WINDOW_SIZE_STOP_WAIT){//如果窗口大小过大
            //帧放入等待队列
            waiting_queue.push(fi);
            return;

        }else{//发送帧

            frame_cache[(front + length) % WINDOW_SIZE_STOP_WAIT] = fi;
            length++;
            SendFRAMEPacket((unsigned char*)pBuffer, bufferSize);
            return;
        }
    }
    else if(messageType == MSG_TYPE_TIMEOUT){
        printf("MSG_TYPE_TIMEOUT\n");
        //得到要发的帧的序列号
        unsigned int seq_num = *((unsigned int*)pBuffer);
        printf("the seq num is %d\n", seq_num);
        int rear = front + length;
        for(int i = seq_num; i < rear; ++i){
            SendFRAMEPacket((unsigned char*) frame_cache[i % WINDOW_SIZE_BACK_N_FRAME].pframe, frame_cache[i % WINDOW_SIZE_BACK_N_FRAME].size);

        }

        return;
    }
    else if(messageType == MSG_TYPE_RECEIVE){
        frame* received_frame = (frame*)pBuffer;
        unsigned int ack_num = (unsigned int)ntohl(received_frame->head.ack);
        printf("receive frame, the ack num is %d\n", ack_num);
        frame_kind fkind = (frame_kind)(ntohl((unsigned long)received_frame->head.kind));
        int rear = front + length;
        if(fkind == ack){//返回ack
            if(ack_num >= front && ack_num < rear){
                length -= ack_num - front + 1;//窗口缩小
                front = ack_num + 1;
                
                //指导书写的发一个帧，其实应该尽量都发完
                while(length <= WINDOW_SIZE_STOP_WAIT && !waiting_queue.empty()){
                    Frame_info fi = waiting_queue.front();
                    waiting_queue.pop();
                    frame_cache[(front + length) % WINDOW_SIZE_STOP_WAIT] = fi;
                    SendFRAMEPacket((unsigned char* )fi.pframe, fi.size);
                    length++;
                }
            }

        }else if(fkind == nak){//nak,重发
            printf("in received, fkind is nak\n");
            for(int i = ack_num; i < rear; ++i){
                SendFRAMEPacket((unsigned char *)frame_cache[i % WINDOW_SIZE_STOP_WAIT].pframe, frame_cache[i % WINDOW_SIZE_STOP_WAIT].size);
            }
        }

    }
}

int stud_slide_window_back_n_frame(char* pBuffer, int bufferSize, UINT8 messageType){
    static Frame_info frame_cache[WINDOW_SIZE_BACK_N_FRAME];
    static int front = 0;//窗口起始位置
    static int length = 0;//窗口中已发出的帧个数
    static queue<Frame_info> waiting_queue;
    if(messageType == MSG_TYPE_SEND){
        //分配区域，缓存帧或者发出这个帧
        char *ptmp_frame=(char*)malloc(bufferSize);
		memcpy(ptmp_frame, pBuffer);
        Frame_info fi;
        fi.pframe = (frame*)ptmp_frame;
        fi.size = bufferSize;
        
        if( length > WINDOW_SIZE_BACK_N_FRAME){//如果窗口大小过大
            //帧放入等待队列
            waiting_queue.push(fi);
            return;

        }else{//发送帧

            frame_cache[(front + length) % WINDOW_SIZE_BACK_N_FRAME] = fi;
            length++;
            SendFRAMEPacket((unsigned char*)pBuffer, bufferSize);
            return;
        }
    }
    else if(messageType == MSG_TYPE_TIMEOUT){
        //得到要发的帧的序列号
        unsigned int seq_num = *((unsigned int*)pBuffer);
        printf("the seq num is %d\n", seq_num);
        int rear = front + length;
        for(int i = seq_num; i < rear; ++i){
            SendFRAMEPacket((unsigned char*) frame_cache[i % WINDOW_SIZE_BACK_N_FRAME].pframe, frame_cache[i % WINDOW_SIZE_BACK_N_FRAME].size);

        }

        return;
    }
    else if(messageType == MSG_TYPE_RECEIVE){
        frame* received_frame = (frame*)pBuffer;
        unsigned int ack_num = (unsigned int)ntohl(received_frame->head.ack);
        printf("receive frame, the ack num is %d\n", ack_num);
        frame_kind fkind = (frame_kind)(ntohl((unsigned long)received_frame->head.kind));
        int rear = front + length;
        if(fkind == ack){//返回ack
            if(ack_num >= front && ack_num < rear){
                length -= ack_num - front + 1;//窗口缩小
                front = ack_num + 1;
                
                //指导书写的发一个帧，其实应该尽量都发完
                while(length <= WINDOW_SIZE_BACK_N_FRAME && !waiting_queue.empty()){
                    Frame_info fi = waiting_queue.front();
                    waiting_queue.pop();
                    frame_cache[(front + length) % WINDOW_SIZE_BACK_N_FRAME] = fi;
                    SendFRAMEPacket((unsigned char* )fi.pframe, fi.size);
                    length++;
                }
            }

        }else if(fkind == nak){//nak,重发
            printf("in received, fkind is nak\n");
            for(int i = ack_num; i < rear; ++i){
                SendFRAMEPacket((unsigned char *)frame_cache[i % WINDOW_SIZE_BACK_N_FRAME].pframe, frame_cache[i % WINDOW_SIZE_BACK_N_FRAME].size);
            }
        }

    }
}

int stu_slide_window_choice_frame_resend(char *pBuffer, int bufferSize, UINT8 messageType){
    static Frame_info frame_cache[WINDOW_SIZE_BACK_N_FRAME];
    static int front = 0;//窗口起始位置
    static int length = 0;//窗口中已发出的帧个数
    static queue<Frame_info> waiting_queue;
    if(messageType == MSG_TYPE_SEND){
        printf("MSG_TYPE_SEND\n");
        //分配区域，缓存帧或者发出这个帧
        char *ptmp_frame=(char*)malloc(bufferSize);
		memcpy(ptmp_frame, pBuffer);
        Frame_info fi;
        fi.pframe = (frame*)ptmp_frame;
        fi.size = bufferSize;
        
        if( length > WINDOW_SIZE_BACK_N_FRAME){//如果窗口大小过大
            //帧放入等待队列
            waiting_queue.push(fi);
            return;

        }else{//发送帧

            frame_cache[(front + length) % WINDOW_SIZE_BACK_N_FRAME] = fi;
            length++;
            SendFRAMEPacket((unsigned char*)pBuffer, bufferSize);
            return;
        }
    }
    else if(messageType == MSG_TYPE_TIMEOUT){
        printf("MSG_TYPE_TIMEOUT\n");
        //得到要发的帧的序列号
        unsigned int seq_num = *((unsigned int*)pBuffer);
        printf("the seq num is %d\n", seq_num);
        SendFRAMEPacket((unsigned char*) frame_cache[(seq_num) % WINDOW_SIZE_BACK_N_FRAME].pframe, frame_cache[seq_num % WINDOW_SIZE_BACK_N_FRAME].size);

        // if(seq_num == front){
        //     for(int i = 0; i < size; ++i){
        //         SendFRAMEPacket((unsigned char*) frame_cache[(front + i) % WINDOW_SIZE_BACK_N_FRAME].pframe, frame_cache[(front + i) % WINDOW_SIZE_BACK_N_FRAME].size);

        //     }
        // }
        // else{
        //     int rear = (front + length) % WINDOW_SIZE_BACK_N_FRAME;
        //     for(int i = seq_num; i != rear; i = (i + 1) % WINDOW_SIZE_BACK_N_FRAME){//重新发送
        //         SendFRAMEPacket((unsigned char*) frame_cache[i % WINDOW_SIZE_BACK_N_FRAME].pframe, frame_cache[i % WINDOW_SIZE_BACK_N_FRAME].size);
        //     }            
        // }

        return;
    }
    else if(messageType == MSG_TYPE_RECEIVE){
        printf("MSG_TYPE_RECEIVE\n");
        frame* received_frame = (frame*)pBuffer;
        unsigned int ack_num = (unsigned int)ntohl(received_frame->head.ack);
        printf("receive frame, the ack num is %d\n", ack_num);
        frame_kind fkind = (frame_kind)(ntohl((unsigned long)received_frame->head.kind));
        int rear = front + length;
        if(fkind == ack){//返回ack
            if(ack_num >= front && ack_num < rear){
                length -= ack_num - front + 1;//窗口缩小
                front = ack_num + 1;
                
                //指导书写的发一个帧，其实应该尽量都发完
                while(length <= WINDOW_SIZE_BACK_N_FRAME && !waiting_queue.empty()){
                    Frame_info fi = waiting_queue.front();
                    waiting_queue.pop();
                    frame_cache[(front + length) % WINDOW_SIZE_BACK_N_FRAME] = fi;
                    SendFRAMEPacket((unsigned char* )fi.pframe, fi.size);
                    length++;
                }
            }

        }else if(fkind == nak){//nak,重发
            printf("in received, fkind is nak\n");
            SendFRAMEPacket((unsigned char*) frame_cache[ack_num % WINDOW_SIZE_BACK_N_FRAME].pframe, frame_cache[ack_num % WINDOW_SIZE_BACK_N_FRAME].size);
            
        }

    } 
}