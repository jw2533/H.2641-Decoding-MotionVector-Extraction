//H264һ֡���ݵĽṹ��
typedef struct Tag_NALU_t {
	unsigned char forbidden_bit;           //! Should always be FALSE
	unsigned char nal_reference_idc;       //! NALU_PRIORITY_xxxx
	unsigned char nal_unit_type;           //! NALU_TYPE_xxxx  
	unsigned int  startcodeprefix_len;      //! ǰ׺�ֽ���
	unsigned int  len;                     //! ����nal ͷ��nal ���ȣ��ӵ�һ��00000001����һ��000000001�ĳ���
	unsigned int  max_size;                //! ���һ��nal �ĳ���
	unsigned char * buf;                   //! ����nal ͷ��nal ����
	unsigned char Frametype;               //! ֡����
	unsigned int  lost_packets;            //! Ԥ��
} NALU_t;

//nal����
enum nal_unit_type_e {
	NAL_UNKNOWN = 0,
	NAL_SLICE = 1,
	NAL_SLICE_DPA = 2,
	NAL_SLICE_DPB = 3,
	NAL_SLICE_DPC = 4,
	NAL_SLICE_IDR = 5,    /* ref_idc != 0 */
	NAL_SEI = 6,    /* ref_idc == 0 */
	NAL_SPS = 7,
	NAL_PPS = 8
	/* ref_idc == 0 for 6,9,10,11,12 */
};

//֡����
enum Frametype_e {
	FRAME_I = 15,
	FRAME_P = 16,
	FRAME_B = 17
};