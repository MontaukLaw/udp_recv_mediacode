#include <jni.h>
#include <string>
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>

#define TAG "NATIVE_LIB"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__);

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
}

#define PORT     54322
#define UDP_PACKET_LEN  1000
#define DATA_BUFFER_LEN 800000
#define REMOTE_ALI_SERVER  "120.77.179.171" // 120.77.179.171
// #define REMOTE_ALI_SERVER  "192.168.31.18" // 120.77.179.171

typedef enum {
    S_FRAME = 1,
    P_FRAME = 2,
    E_FRAME = 3,
    D_START_PACKET = 4,
    D_REST_PACKET = 5
} frame_type_e;

extern "C"

JNIEXPORT jstring JNICALL
Java_com_wulala_myapplicationudprcv_MainActivity_stringFromJNI(JNIEnv *env, jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

struct Jni_Context {
    JNIEnv *jniEnv = nullptr;
    jobject instance = nullptr;
};

int dataFrameLen = 0;
JavaVM *jvm = nullptr;
bool ifSubThreadRunning = false;

jint JNI_OnLoad(JavaVM *vm, void *args) {
    ::jvm = vm;
    return JNI_VERSION_1_6;
}

bool ifFrameStarted(char *data) {

    if (data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 1) {
        return true;
    }
    return false;
}

frame_type_e getFrameType(int packetLen, char *data) {
    if (ifFrameStarted(data)) {
        if (packetLen == 20) {
            return S_FRAME;
        } else if (packetLen == 8) {
            return P_FRAME;
        } else if (packetLen == 9) {
            return E_FRAME;
        } else {
            return D_START_PACKET;
        }
    } else {
        return D_REST_PACKET;
    }
}
//
//frame_type_e getFrameTypeOld(int packetLen, char *data) {
//
//    if (ifFrameStarted(data)) {
//        if (packetLen == 19 || packetLen == 20) {
//            return S_FRAME;
//        } else if (packetLen == 8) {
//            return P_FRAME;
//        } else if (packetLen == 9) {
//            return E_FRAME;
//        } else {
//            return D_START_PACKET;
//        }
//    } else {
//        return D_REST_PACKET;
//    }
//}


void send_frame_to_java_list(jmethodID jmethodId, char *dataFrameBuf, jobject job, JNIEnv *subThreadEnv) {
    jbyteArray jbDataA = subThreadEnv->NewByteArray(dataFrameLen);
    subThreadEnv->SetByteArrayRegion(jbDataA, 0, dataFrameLen, (jbyte *) dataFrameBuf);
    subThreadEnv->CallVoidMethod(job, jmethodId, jbDataA);
    subThreadEnv->DeleteLocalRef(jbDataA);
}

// 20220919 改为tcp连接
void *sub_thread_process(void *args) {

    LOGD("Enter sub thread \n");
    int sockfd;
    int rtn;
    struct sockaddr_in servaddr, cliaddr;

    bool ifStart = false;

    char packetBuf[UDP_PACKET_LEN];   // udp包最大是1400

    char dataFrameBuf[DATA_BUFFER_LEN];

    // Creating socket file descriptor
    // 改为stream的tcp流
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        // if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        LOGD("socket creation failed");
        return nullptr;
    }

    printf("sockfd:%d\n", sockfd);

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    servaddr.sin_family = AF_INET; // IPv4
    // servaddr.sin_addr.s_addr = INADDR_ANY;
    inet_pton(AF_INET, REMOTE_ALI_SERVER, (struct in_addr *) &servaddr.sin_addr.s_addr);
    servaddr.sin_port = htons(PORT);

    rtn = connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    if (rtn < 0) {
        LOGD("connect failed");
        return nullptr;
    }
    // bind 部分多余的好像
    // if (bind(sockfd, (const struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
    // LOGD("bind failed");
    // return nullptr;
    // }

    // socklen_t clintAddrSize = sizeof(sockaddr_in);
    int recvLen;

    struct Jni_Context *jniContext = static_cast<Jni_Context *>(args);

    // 1. 通过JVM获取jnivEnv
    JNIEnv *subThreadEnv = nullptr;
    jint attachResult = ::jvm->AttachCurrentThread(&subThreadEnv, nullptr);

    if (attachResult != JNI_OK) {
        LOGD("Attach sub thread jni env failed\n");
        return nullptr;
    }
    LOGD("Got sub thread jni env\n");

    // 2. 获取jclass
    jclass mainActivityClass = subThreadEnv->GetObjectClass(jniContext->instance);  // 第一步, 获取MainActivity类

    // 3. 获取method
    // jmethodID jmethodId = subThreadEnv->GetMethodID(mainActivityClass, "printLog", "()V");
    jmethodID jmethodId = subThreadEnv->GetMethodID(mainActivityClass, "getUdpPacket", "([B)V");

    while (ifSubThreadRunning) {
        // recvLen = recvfrom(sockfd, (char *) packetBuf, UDP_PACKET_LEN, MSG_WAITALL, (struct sockaddr *) &cliaddr, &clintAddrSize);
        recvLen = read(sockfd, (char *) packetBuf, sizeof(packetBuf)); // MSG_WAITALL, (struct sockaddr *) &cliaddr, &clintAddrSize);
        // LOGD("Packet size :%d ", recvLen);
        // usleep(1);
        if (recvLen > 0) {
            // if (recvLen < 30) {
            // LOGD("Packet size :%d type: 0x%02x", recvLen, packetBuf[4]);
            // }

            frame_type_e frameType = getFrameType(recvLen, packetBuf);

            if (!ifStart) {
                if (frameType == S_FRAME) {
                    ifStart = true;
                }else{
                    continue;
                }
            }

            if (frameType == S_FRAME) {

                // S帧需要发两个包, 首先是已经保存好的数据帧
                if (dataFrameLen > 0) {
                    jbyteArray jbDataA = subThreadEnv->NewByteArray(dataFrameLen);
                    subThreadEnv->SetByteArrayRegion(jbDataA, 0, dataFrameLen, (jbyte *) dataFrameBuf);
                    subThreadEnv->CallVoidMethod(jniContext->instance, jmethodId, jbDataA);
                    subThreadEnv->DeleteLocalRef(jbDataA);

                    memset(dataFrameBuf, 0, DATA_BUFFER_LEN);
                    dataFrameLen = 0;
                }

                // 第2个包是S帧自己
                jbyteArray jbA = subThreadEnv->NewByteArray(recvLen);
                subThreadEnv->SetByteArrayRegion(jbA, 0, recvLen, (jbyte *) packetBuf);
                subThreadEnv->CallVoidMethod(jniContext->instance, jmethodId, jbA);
                subThreadEnv->DeleteLocalRef(jbA);

                // send_frame_to_java_list(jmethodId, packetBuf, jniContext->instance, subThreadEnv);

            } else if (frameType == P_FRAME) {
                jbyteArray jbA = subThreadEnv->NewByteArray(recvLen);
                subThreadEnv->SetByteArrayRegion(jbA, 0, recvLen, (jbyte *) packetBuf);
                subThreadEnv->CallVoidMethod(jniContext->instance, jmethodId, jbA);
                subThreadEnv->DeleteLocalRef(jbA);
            } else if (frameType == E_FRAME) {
                jbyteArray jbA = subThreadEnv->NewByteArray(recvLen);
                subThreadEnv->SetByteArrayRegion(jbA, 0, recvLen, (jbyte *) packetBuf);
                subThreadEnv->CallVoidMethod(jniContext->instance, jmethodId, jbA);
                subThreadEnv->DeleteLocalRef(jbA);
                dataFrameLen = 0;
            } else if (frameType == D_START_PACKET) {
                if (dataFrameLen > 0) {

                    jbyteArray jbDataA = subThreadEnv->NewByteArray(dataFrameLen);
                    subThreadEnv->SetByteArrayRegion(jbDataA, 0, dataFrameLen, (jbyte *) dataFrameBuf);
                    subThreadEnv->CallVoidMethod(jniContext->instance, jmethodId, jbDataA);
                    subThreadEnv->DeleteLocalRef(jbDataA);

                    memset(dataFrameBuf, 0, DATA_BUFFER_LEN);
                }

                // 复制数据
                memcpy(dataFrameBuf, packetBuf, recvLen);
                dataFrameLen = recvLen;

            } else {  // frameType == D_REST_PACKET
                memcpy(&dataFrameBuf[dataFrameLen], packetBuf, recvLen);
                dataFrameLen = dataFrameLen + recvLen;
            }

            memset(packetBuf, 0, UDP_PACKET_LEN);
            // jbA= nullptr;
        }
        // 4. 调用java的method
        // subThreadEnv->CallVoidMethod(jniContext->instance, jmethodId);

        usleep(1);

    }

    // 5. 结束线程
    ::jvm->DetachCurrentThread();

    LOGD("Sub thread finished.");

    return nullptr;
}

struct Jni_Context jniContext;
pthread_t pid;

extern "C"
JNIEXPORT void JNICALL
Java_com_wulala_myapplicationudprcv_MainActivity_threadTest(JNIEnv *env, jobject thiz) {

    jniContext.jniEnv = env;
    // jniContext.instance = thiz;  // 这样不成立, 因为jobject不可以跨函数调用

    jniContext.instance = env->NewGlobalRef(thiz);   // 提升全局引用

    ifSubThreadRunning = true;

    pthread_create(&pid, nullptr, sub_thread_process, (void *) &jniContext);   // 将jniContext当参数传进去

}