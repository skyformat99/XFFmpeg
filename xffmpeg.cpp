#include "xffmpeg.h"
#include<QDebug>
XFFmpeg::XFFmpeg()
{
    errorbuf[0]='\0';
    av_register_all();
}

XFFmpeg::~XFFmpeg()
{

}

bool XFFmpeg::Open(const char *path)
{
    Close();
    mutex.lock();
    int re=avformat_open_input(&ic,path,0,0);//返回值linux 风格：0：成功
    if(re!=0)
    {
        mutex.unlock();
        av_strerror(re,errorbuf, sizeof(errorbuf));
        return false;
     }

     totalMs=(ic->duration/AV_TIME_BASE)*1000;


           for(int i=0;i<ic->nb_streams;i++)
           {
               AVCodecContext *enc=ic->streams[i]->codec;
               if(enc->codec_type==AVMEDIA_TYPE_VIDEO)
               {
                   videostream=i;
                   AVCodec *codec=avcodec_find_decoder(enc->codec_id);
                   if(!codec)
                   {
                       mutex.unlock();
                       printf("video code not find\n");
                       return false;
                   }
                   int err=avcodec_open2(enc,codec, nullptr);
                   if(err!=0)
                   {
                       mutex.unlock();
                       av_strerror(err,errorbuf, sizeof(errorbuf));
                       return false;
                   }
                   printf("open codec success!\n");
               }
           }
     mutex.unlock();
     return true;
}

std::string XFFmpeg::Geterror()
{
    std::string err=errorbuf;
    return err;
}


AVPacket XFFmpeg::Read()
{
    AVPacket pkt;
    memset(&pkt,0,sizeof(AVPacket));
    mutex.lock();
    if(!ic)
    {
        mutex.unlock();
        return pkt;
    }
    int err=av_read_frame(ic,&pkt);
    if(err!=0)
    {
        av_strerror(err,errorbuf,sizeof(errorbuf));
    }

    mutex.unlock();
    return pkt;
}

AVFrame *XFFmpeg::Decode(const AVPacket *pkt)
{
    mutex.lock();
    if(!ic)
    {
        mutex.unlock();
        return NULL;
    }
    if(yuv==NULL)
    {
        yuv=av_frame_alloc();
    }
    int re=avcodec_send_packet(ic->streams[pkt->stream_index]->codec,pkt);
    if(re!=0)
    {
        mutex.unlock();
        return NULL;
    }

    re=avcodec_receive_frame(ic->streams[pkt->stream_index]->codec,yuv);
    if(re!=0)
    {
        mutex.unlock();
        return NULL;
    }
    mutex.unlock();
    return yuv;
}

bool XFFmpeg::ToRGB(const AVFrame,char *out,int outwidth,int outheight)
{
    mutex.lock();
    if(!ic)
    {
        mutex.unlock();
        return false;
    }

    AVCodecContext *videoctx=ic->streams[this->videostream]->codec;
    cCtx=sws_getCachedContext(cCtx,
                                          videoctx->width,
                                          videoctx->height,
                                          videoctx->pix_fmt,//视频格式
                                          outwidth,outheight,
                                          AV_PIX_FMT_BGRA,//qt
                                          SWS_BICUBIC,
                                          NULL,NULL,NULL);
                if(!cCtx)
                {
                    mutex.unlock();
                    printf("sws_getCachedContext failed!");
                    return false;
                }
    //转换
                uint8_t *data[AV_NUM_DATA_POINTERS]={0};
                data[0]=(uint8_t  *)out;
                int linesize[AV_NUM_DATA_POINTERS]={0};
                linesize[0]=outwidth*4;
                int h=sws_scale(cCtx,yuv->data,yuv->linesize,0,videoctx->height,data,linesize);
                if(h>0)
                {
                    printf("(%d)",h);
                }
    mutex.unlock();
    return true;
}

void XFFmpeg::Close()
{
    mutex.lock();
    if(ic)   avformat_close_input(&ic);
    if(yuv)  av_frame_free(&yuv);
    if(cCtx)
    {
        sws_freeContext(cCtx);
        cCtx=nullptr;
    }
    mutex.unlock();
}



