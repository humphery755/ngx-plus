#ifndef _MQTT_PROTOCOL_H_
#define _MQTT_PROTOCOL_H_


#define MQTT_MSGTYPE_CONNECT 0x10
#define MQTT_MSGTYPE_CONNACK 0x20
#define MQTT_MSGTYPE_PUBLISH 0x30
#define MQTT_MSGTYPE_PUBACK 0x40
#define MQTT_MSGTYPE_PUBREC 0x50
#define MQTT_MSGTYPE_PUBREL 0x60
#define MQTT_MSGTYPE_PUBCOMP 0x70
#define MQTT_MSGTYPE_SUBSCRIBE 0x80
#define MQTT_MSGTYPE_SUBACK 0x90
#define MQTT_MSGTYPE_UNSUBSCRIBE 0xA0
#define MQTT_MSGTYPE_UNSUBACK 0xB0
#define MQTT_MSGTYPE_PINGREQ 0xC0
#define MQTT_MSGTYPE_PINGRESP 0xD0
#define MQTT_MSGTYPE_DISCONNECT 0xE0

#define CONNACK_ACCEPTED 0
#define CONNACK_REFUSED_PROTOCOL_VERSION 1
#define CONNACK_REFUSED_IDENTIFIER_REJECTED 2
#define CONNACK_REFUSED_SERVER_UNAVAILABLE 3
#define CONNACK_REFUSED_BAD_USERNAME_PASSWORD 4
#define CONNACK_REFUSED_NOT_AUTHORIZED 5


#define MOSQ_ERR_SUCCESS 0
#define MOSQ_ERR_NOMEM 1

#define MOSQ_MSB(A) (uint8_t)((A & 0xFF00) >> 8)
#define MOSQ_LSB(A) (uint8_t)(A & 0x00FF)

#ifdef _EXPORTING
#define API_DECLSPEC __declspec(dllexport)
#else
#define API_DECLSPEC __declspec(dllimport)
#endif

#define NGX_UINTX_ERROR 0

enum MQTT_QOS{
	QOS_MOST_ONE=0,
	QOS_LEAST_ONE,
	QOS_EXACTLY_ONCE ,
	QOS_RESERVED
};

typedef void* bit_vector_pool;
typedef void*(*bit_vector_alloc_pt) (bit_vector_pool *pool,size_t len, size_t size); 

typedef struct{
	unsigned char	*bits;
	size_t		size;
	int64_t		count;
}bit_vector_t;

typedef struct mqtt3_packet_s{
	uint8_t command;
	uint8_t *payload;
	uint32_t pos;
	uint32_t remaining_length;
	uint16_t mid;
	int 	qos;
	bool 	retain;
	bool 	dup;
	ngx_str_t id;
	ngx_str_t topic;
	ngx_str_t data;
}mqtt_packet_t;

typedef struct {
	int	depth;
	//enum COMMAND command;
	ngx_queue_t	uw_queue;
	ngx_queue_t	cw_queue;
	mqtt_packet_t packet;
}req_ctx_t;


ngx_atomic_t atomic_pub_count;
ngx_atomic_t atomic_sub_count;
ngx_atomic_t atomic_error_count;

int lua_utility_indexOf(const u_char *str1,const u_char *str2);
uint16_t lua_utility_redisCRC16(const char *buf, int len);


uint16_t utility_mqtt_read_uint16(u_char *p,size_t len);
uint32_t utility_mqtt_read_uint32(u_char *b,size_t len);
int64_t utility_mqtt_read_int64(u_char *p,size_t len);
uint64_t utility_mqtt_read_uint64(u_char *p,size_t len);
double utility_mqtt_read_double(u_char *p,size_t len);
int utility_mqtt_read_string(u_char *p,size_t plen,ngx_str_t *out);
int utility_mqtt_read_remaining_leng(u_char *p,size_t len,uint32_t *out);

int utility_debug_printf_binary(u_char *p,size_t len);



int utility_mqtt_write_uint16(u_char *p,size_t len,uint16_t n);
int utility_mqtt_write_uint32(u_char *p,size_t len,uint32_t n);
int utility_mqtt_write_int64(u_char *b,size_t len,int64_t n);
int utility_mqtt_write_uint64(u_char *b,size_t len,uint64_t n);
int utility_mqtt_write_string(u_char *p,size_t len,ngx_str_t *str);
int utility_mqtt_write_remaining_leng(u_char *p,size_t len,uint32_t remaining_length);

int utility_bit_vector_init(ngx_slab_pool_t *shpool,bit_vector_t *bv,uint64_t n);
void utility_bit_vector_destroy(ngx_slab_pool_t *shpool,bit_vector_t *bv);
void utility_bit_vector_set(bit_vector_t *bv,uint64_t bit);
bool utility_bit_vector_get(bit_vector_t *bv,uint64_t bit);
void utility_bit_vector_del(bit_vector_t *bv,uint64_t bit);
void utility_bit_vector_clear(bit_vector_t *bv);
uint64_t utility_bit_vector_count(bit_vector_t *bv);
/*
#define utility_mqtt_write_uint16(p,v)  do{(p[0]) = MOSQ_MSB(v);	(p[1])=MOSQ_LSB(v); return 2}while(0)

#define utility_mqtt_read_uint16(u_char *p,size_t len) \
do{ \
	uint16_t out; uint8_t msb, lsb;	if(len<2) return NGX_ERROR;	msb = p[0];	lsb = p[1];	out=(msb<<8) + lsb;	return out; \
}while(0)
*/
#endif

