/*
ModuleCam读hdmi输入，然后处理，最后推流

Cam-->Rga callback-->opencv-->ModuleMemReader-->rga-->encoder-->rtsp

*/ 
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "module/vi/module_rtspClient.hpp"
#include "module/vp/module_mppdec.hpp"
#include "module/vp/module_rga.hpp"
#include "module/vi/module_cam.hpp"
#include "module/vi/module_memReader.hpp"
#include "module/vp/module_mppdec.hpp"
#include "module/vo/module_drmDisplay.hpp"
#include "module/vo/module_rtspServer.hpp"
#include "module/vp/module_mppenc.hpp"
#define ENABLE_OPENCV

//=============================================
#ifdef ENABLE_OPENCV
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include<opencv2/imgproc.hpp>
#endif
//=============================================

#include<iostream>
#include<yaml-cpp/yaml.h>


#define UNUSED(x) [&x] {}()
using namespace std;

struct External_ctx {
    shared_ptr<ModuleMedia> module;
    shared_ptr<ModuleMemReader> module_mmr;
    shared_ptr<ModuleRga> module_mmr_out;
    uint16_t test;
};

int callback_count=0;

void callback_external(void* _ctx, shared_ptr<MediaBuffer> buffer)
{
    External_ctx* ctx = static_cast<External_ctx*>(_ctx);
    shared_ptr<ModuleMedia> module = ctx->module;

    // 检查缓冲区类型，是否为视频缓冲区BUFFER_TYPE_VIDEO
    if (buffer == NULL || buffer->getMediaBufferType() != BUFFER_TYPE_VIDEO)
        return;

    // 获取视频缓冲区（将 buffer 转换为 shared_ptr<VideoBuffer> 类型，以便访问视频缓冲区的特定方法和属性）
    shared_ptr<VideoBuffer> buf = static_pointer_cast<VideoBuffer>(buffer);

    //获取视频帧数据和属性
    void* ptr = buf->getActiveData();               //ptr指向视频帧数据
    size_t size = buf->getActiveSize();             //size指视频帧数据大小
    uint32_t width = buf->getImagePara().hstride;
    uint32_t height = buf->getImagePara().vstride;

    //刷新DMA缓冲区
    buf->invalidateDrmBuf();

    UNUSED(size);

    //使用OpenCV进行处理
    #ifdef ENABLE_OPENCV
        cv::Mat img(cv::Size(width, height), CV_8UC3, ptr); //创建一个cv::Mat对象，使用视频帧数据

        //在视频帧上添加文本标注，包含模块名称和回调计数
        cv::putText(img,string(module->getName()) +" "+ to_string(callback_count),
                                cv::Point(50 , 50) , 
                                cv::FONT_HERSHEY_SIMPLEX , 
                                1 , 
                                cv::Scalar(0 , 255 , 0));

	
        //获取 ModuleMemReader 模块的共享指针 p_mmr
        // cv::imwrite("img.png",img);
    #undef img
        callback_count++;
    #endif

    //将数据传递给内存读取模块
    shared_ptr<ModuleMemReader> p_mmr = ctx->module_mmr;
	int ret = p_mmr->setInputBuffer(ptr, size, buf->getBufFd()); // 调用p_mmr->setInputBuffer 方法，将视频帧数据传递给内存读取模块进行处理
    ret = p_mmr->waitProcess(2000);             // 调用 p_mmr->waitProcess 方法，等待处理完成，超时时间为2000毫秒（2秒）
    // 处理超时机制
    if (ret != 0)
    {
        ff_warn("Wait timeout\n");
        if (p_mmr->waitProcess(2000))
        return;
    }
    
}

int main(int argc, char** argv)
{
    if(argc!=2)
    {
        std::cout<<"run as: xxx path_to_config\n";
        return 0;
    }

    shared_ptr<ModuleMedia> last_media;

    /*解析配置文件
    *从命令行参数中读取配置文件的路径，并解析YAML文件获取输入设备，RTSP路径和端口等配置*/
    YAML::Node config = YAML::LoadFile(argv[1]);
    string dev = config["input_dev"].as<string>();
    string rtsp_path = config["rtsp_path"].as<string>();
    int rtsp_port = config["rtsp_port"].as<int>();

    /*初始化摄像头模块
    * 使用摄像头设备路径初始化 ModuleCam 模块*/
    ImagePara input_para;
    int ret;
    shared_ptr<ModuleCam> cam = make_shared<ModuleCam>(dev);
    //cam->setOutputImagePara(input_para);
    cam->setProductor(NULL);
    cam->setBufferCount(4);
    ret = cam->init();
    if (ret < 0) {
        ff_error("memory reader init failed\n");
        return -1;
    }

    input_para = cam->getOutputImagePara();

    /*根据输入格式选择是否解码
    */
    if((input_para.v4l2Fmt==V4L2_PIX_FMT_MJPEG)
        ||(input_para.v4l2Fmt==V4L2_PIX_FMT_H264)
        ||(input_para.v4l2Fmt==V4L2_PIX_FMT_HEVC)
        )
    {
        // 输入编码是V4L2_PIX_FMT_MJPEG V4L2_PIX_FMT_H264 V4L2_PIX_FMT_HEVC时，需要解码
        // 可以通过getOutputImagePara查看，在demo.cpp的466行
        shared_ptr<ModuleMppDec> mpp_d = make_shared<ModuleMppDec>();
        mpp_d->setProductor(cam);
        mpp_d->setBufferCount(10);
        ret = mpp_d->init();
        if(ret<0)
        {
            ff_error("ModuleMppDec init failed\n");
            return -1;
        }
        last_media = mpp_d;
    }


    // input_para = mem_r->getInputImagePara();
    // copy buffer
    ImagePara output_para = input_para;
    output_para.height=720;
    output_para.width=1280;
    output_para.hstride = output_para.width;
    output_para.vstride = output_para.height;
    // output_para.v4l2Fmt = V4L2_PIX_FMT_NV12;
    output_para.v4l2Fmt = V4L2_PIX_FMT_BGR24;

    /*初始化RGA模块进行图像处理 */
    auto rga = make_shared<ModuleRga>(output_para, RGA_ROTATE_NONE);
    if(last_media==nullptr)
    {
        std::cout<<"no encoder\n";
        rga->setProductor(cam);
    }
    else{
        rga->setProductor(last_media);

    }
    rga->setBufferCount(2);
    ret = rga->init();
    if (ret < 0) {
        ff_error("Failed to init rga\n");
        return -1;
    }

    // MemReader rga出来后，opencv处理后，再加到mem_r
    /*初始化MenReader模块
    */
    shared_ptr<ModuleMemReader> mem_r = NULL;
    mem_r = make_shared<ModuleMemReader>(output_para);
    ret = mem_r->init();
    if (ret < 0) {
        ff_error("memory reader init failed\n");
        return -1;
    }

    /*设置RGA模块的回调函数 */
    External_ctx* ctx1 = new External_ctx();
    ctx1->module = rga;
    ctx1->module_mmr = mem_r;
    rga->setOutputDataCallback(ctx1,callback_external);


    // memreader 输出给到rga
    // copy buffer 同时resize
    ImagePara output_para2 = output_para;
    output_para.height= ALIGN(720,8);
    output_para.width=ALIGN(1280,8);
    output_para.hstride = output_para.width;
    output_para.vstride = output_para.height;
    output_para.v4l2Fmt = V4L2_PIX_FMT_NV12;


    auto rga2 = make_shared<ModuleRga>(output_para2, RGA_ROTATE_NONE);
    // auto rga = make_shared<ModuleRga>(input_para, RGA_ROTATE_NONE);
    rga2->setProductor(mem_r);
    rga2->setBufferCount(2);
    ret = rga2->init();
    if (ret < 0) {
        ff_error("Failed to init rga\n");
        return -1;
    }

    // 编码
    input_para = cam->getOutputImagePara();

    shared_ptr<ModuleMppEnc> mpp_e = make_shared<ModuleMppEnc>(
        ENCODE_TYPE_H265
        // 30,
        // 30,
        // 2048,
        // ENCODE_RC_MODE_CBR,
        // ENCODE_QUALITY_MEDIUM,
        // ENCODE_PROFILE_BASELINE
                        );
    mpp_e->setProductor(rga2);
    mpp_e->setBufferCount(8);
    mpp_e->setDuration(0); // Use the input source timestamp
    ret = mpp_e->init();
    if (ret < 0) {
        ff_error("Enc init failed\n");
        return -1;
    }

    /*初始化RTSP服务器 */
    // shared_ptr<ModuleRtspServer> rtsp_s = make_shared<ModuleRtspServer>("/live/0", 19353);
    shared_ptr<ModuleRtspServer> rtsp_s = make_shared<ModuleRtspServer>(rtsp_path.c_str(),rtsp_port);
    rtsp_s->setProductor(mpp_e);
    // rtsp_s->setBufferCount(0);
    // rtsp_s->setSynchronize(inst->sync);
    ret = rtsp_s->init();
    if (ret) {
        ff_error("rtsp server init failed\n");
        return -1;
    }

    /*启动摄像头和MemReader模块*/
    cam->start();
    mem_r->start();
    cam->dumpPipe();
    mem_r->dumpPipe();
    getchar();
    cam->stop();
    mem_r->stop();
    return 0;
}
