//Work
static bool bFirst = true;
    static AVCodecContext * pCodecCtx = NULL;
    static AVFrame * pavFrame = NULL;
    static AVCodec* pCodec = NULL;
    static size_t count = 0;
    if (bFirst)
    {
        bFirst = false;
 
        av_register_all();
        pCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
        if (pCodec)
        {
            pCodecCtx = avcodec_alloc_context3(pCodec);
            pCodecCtx->time_base.num = 1;
            pCodecCtx->time_base.den = 25;
            pCodecCtx->bit_rate = 0;
            pCodecCtx->frame_number = 1;
            pCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
            pCodecCtx->width = 704;
            pCodecCtx->height = 576;
 
            if (avcodec_open2(pCodecCtx, pCodec, NULL) >= 0)
            {
                pavFrame = avcodec_alloc_frame();
            }
        }
    }
        int nGot = 0;
        AVPacket avpacket;
        av_init_packet(&avpacket);
        uint8_t inputbuf[204800 + FF_INPUT_BUFFER_PADDING_SIZE];
        memcpy_s(inputbuf, sizeof(inputbuf), pFrame->pPacketBuffer, pFrame->dwPacketSize);
        avpacket.size = pFrame->dwPacketSize;
        avpacket.data = inputbuf;
 
        avcodec_decode_video2(pCodecCtx, pavFrame, &nGot, &avpacket);
        if (pFrame->nPacketType == 1)
        {
            printf("I frame!\n");
        }
        printf("%d\n", ++count);
        if (nGot)
        {
            printf("Got a picture!\n");
        }
     
    return 0;