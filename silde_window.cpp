#include "sysinclude.h"

#include<queue>
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


//停等协议就是n比特重传的特殊情况
int stu_slide_window_stop_and_wait(char* pBuffer, int bufferSize, UINT8 messageType) {
    static Frame_info frame_cache[WINDOW_SIZE_STOP_WAIT];
    static int front = 1;//窗口维护的起始seq_num
    static int rear = 1;//窗口维护的结尾seq_num + 1
    static queue<Frame_info> waiting_queue;//等待队列
    if(messageType == MSG_TYPE_SEND){
        //分配区域，缓存帧或者发出这个帧
        char *ptmp_frame=(char*)malloc(bufferSize);
		memcpy(ptmp_frame, pBuffer, bufferSize);
        Frame_info fi;
        fi.pframe = (frame*)ptmp_frame;
        fi.size = bufferSize;
        waiting_queue.push(fi);//帧放入等待队列
        while( (rear - front) < WINDOW_SIZE_STOP_WAIT && !waiting_queue.empty()){//如果窗口小于最大窗口
            Frame_info qf = waiting_queue.front();
            waiting_queue.pop();
            SendFRAMEPacket((unsigned char*)qf.pframe, qf.size);
            frame_cache[rear % WINDOW_SIZE_STOP_WAIT] = qf;
            rear++;
        }
    }
    else if(messageType == MSG_TYPE_TIMEOUT){
        //得到要发的帧的序列号
        unsigned int seq_num = *((unsigned int*)pBuffer);
        //重发超时帧之后的帧.在停等协议中只发送唯一那个帧
        for(int i = front; i < rear; ++i){
		    unsigned int k =  (unsigned int)ntohl(frame_cache[i % WINDOW_SIZE_STOP_WAIT].pframe->head.seq);
            if(k == seq_num){
                for(int j = i; j < rear; ++j){
                     SendFRAMEPacket((unsigned char*) frame_cache[j % WINDOW_SIZE_STOP_WAIT].pframe, frame_cache[j % WINDOW_SIZE_STOP_WAIT].size);
                }
                break;
            }
        }
    }
    else if(messageType == MSG_TYPE_RECEIVE){     
        frame* received_frame = (frame*)pBuffer;
        unsigned int seq_num = (unsigned int)ntohl(received_frame->head.ack);
        frame_kind fkind = (frame_kind)(ntohl((unsigned long)received_frame->head.kind));
        if(fkind == ack){//返回ack
            if(seq_num >= front && seq_num < rear){
                front = seq_num + 1;
                
                //指导书写的发一个帧，其实应该尽量都发完
                while((rear - front) < WINDOW_SIZE_STOP_WAIT && !waiting_queue.empty()){
                    Frame_info fi = waiting_queue.front();
                    waiting_queue.pop();
                    frame_cache[rear % WINDOW_SIZE_STOP_WAIT] = fi;
                    SendFRAMEPacket((unsigned char* )fi.pframe, fi.size);
                    rear++;
                }
            }
        }else if(fkind == nak){//nak,重发
            for(int i = seq_num; i < rear; ++i){
                SendFRAMEPacket((unsigned char *)frame_cache[i % WINDOW_SIZE_STOP_WAIT].pframe, frame_cache[i % WINDOW_SIZE_STOP_WAIT].size);
            }
        }
    }
	return 0;


}

int stud_slide_window_back_n_frame(char* pBuffer, int bufferSize, UINT8 messageType){
    static Frame_info frame_cache[WINDOW_SIZE_BACK_N_FRAME];
    static int front = 1;//窗口维护的起始seq_num
    static int rear = 1;//窗口维护的结尾seq_num+1
    static queue<Frame_info> waiting_queue;
    if(messageType == MSG_TYPE_SEND){
        //分配区域，缓存帧或者发出这个帧
        char *ptmp_frame=(char*)malloc(bufferSize);
		memcpy(ptmp_frame, pBuffer, bufferSize);
        Frame_info fi;
        fi.pframe = (frame*)ptmp_frame;
        fi.size = bufferSize;
        waiting_queue.push(fi);//帧放入等待队列
        while( (rear - front) < WINDOW_SIZE_BACK_N_FRAME && !waiting_queue.empty()){//如果窗口小于最大窗口
            Frame_info qf = waiting_queue.front();//从等待队列中取出帧
            waiting_queue.pop();
            SendFRAMEPacket((unsigned char*)qf.pframe, qf.size);//发送帧
            frame_cache[rear % WINDOW_SIZE_BACK_N_FRAME] = qf;
            rear++;
        }
    }
    else if(messageType == MSG_TYPE_TIMEOUT){
        //指导书写着应该从超时的帧开始发。但实际上是发送所有的未确认帧
         for(int i = front; i < rear; ++i){
            SendFRAMEPacket((unsigned char*) frame_cache[i % WINDOW_SIZE_BACK_N_FRAME].pframe, frame_cache[i % WINDOW_SIZE_BACK_N_FRAME].size);
        }
    }
    else if(messageType == MSG_TYPE_RECEIVE){
        frame* received_frame = (frame*)pBuffer;
        unsigned int seq_num = (unsigned int)ntohl(received_frame->head.ack);
        frame_kind fkind = (frame_kind)(ntohl((unsigned long)received_frame->head.kind));
        if(fkind == ack){//返回ack
            if(seq_num >= front && seq_num < rear){
                front = seq_num + 1;
                //指导书写的发一个帧，其实应该尽量都发完
                while((rear - front) < WINDOW_SIZE_BACK_N_FRAME && !waiting_queue.empty()){
                    Frame_info fi = waiting_queue.front();
                    waiting_queue.pop();
                    frame_cache[rear % WINDOW_SIZE_BACK_N_FRAME] = fi;
                    SendFRAMEPacket((unsigned char* )fi.pframe, fi.size);
                    rear++;
                }
            }

        }
    }
	return 0;

}

int stu_slide_window_choice_frame_resend(char *pBuffer, int bufferSize, UINT8 messageType){
    static Frame_info frame_cache[WINDOW_SIZE_BACK_N_FRAME];
    static int front = 1;//窗口维护的起始seq_num
    static int rear = 1;//窗口维护的结尾seq_num + 1
    static queue<Frame_info> waiting_queue;//等待队列
    if(messageType == MSG_TYPE_SEND){   
        //分配区域，缓存帧或者发出这个帧
        char *ptmp_frame=(char*)malloc(bufferSize);
		memcpy(ptmp_frame, pBuffer, bufferSize);
        Frame_info fi;
        fi.pframe = (frame*)ptmp_frame;
        fi.size = bufferSize;
        waiting_queue.push(fi);//帧放入等待队列
        while( (rear - front) < WINDOW_SIZE_BACK_N_FRAME && !waiting_queue.empty()){//如果窗口小于最大窗口
            Frame_info qf = waiting_queue.front();//从等待队列中取出帧
            waiting_queue.pop();
            SendFRAMEPacket((unsigned char*)qf.pframe, qf.size);//发送帧
            frame_cache[rear % WINDOW_SIZE_BACK_N_FRAME] = qf;
            rear++;
        }
    }
    else if(messageType == MSG_TYPE_RECEIVE){
        frame* received_frame = (frame*)pBuffer;
        unsigned int seq_num = (unsigned int)ntohl(received_frame->head.ack);
        frame_kind fkind = (frame_kind)(ntohl((unsigned long)received_frame->head.kind));
        if(fkind == ack){//返回ack
            if(seq_num >= front && seq_num < rear){
                front = seq_num + 1;
                //指导书写的发一个帧，其实应该尽量都发完
                while((rear - front) < WINDOW_SIZE_BACK_N_FRAME && !waiting_queue.empty()){
                    Frame_info fi = waiting_queue.front();
                    waiting_queue.pop();
                    frame_cache[rear % WINDOW_SIZE_BACK_N_FRAME] = fi;
                    SendFRAMEPacket((unsigned char* )fi.pframe, fi.size);
                    rear++;
                }
            }

        }
        else if(fkind == nak){//nak,重发
            for(int i = front; i < rear; ++i){//找到要选择性重传的帧
                unsigned int k =  (unsigned int)ntohl(frame_cache[i % WINDOW_SIZE_BACK_N_FRAME].pframe->head.seq);
                if(k == seq_num){
                    SendFRAMEPacket((unsigned char *)frame_cache[i % WINDOW_SIZE_BACK_N_FRAME].pframe, frame_cache[i % WINDOW_SIZE_BACK_N_FRAME].size);
                    break;
                }
                
            }
        }
    }
	return 0;
}