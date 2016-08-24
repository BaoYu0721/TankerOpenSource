#ifndef __IWL_SOCKET_H__
#define __IWL_SOCKET_H__

struct Msg
{
	char* type;
	char* ssid;
	char* passwd;
	char* auth_mode;
	char* encryp_type;
	char* equipment_id;
};

extern int init_socket_send();
extern int init_socket_recv();
extern void send_heart_beat(int client_sockfd, char* equipment_id);
extern void send_warning_msg(int client_sockfd, char* equipment_id);
extern void send_error_msg(int client_sockfd, char* equipment_id, char* error_msg);
extern char* toArray(int number);
extern void receive_message_function();
extern void parse_json(char* str, struct Msg wr);
extern void edit_config_for_receiver(struct Msg m);
extern char* receive_msg(int client_sockfd);
extern void send_reply_for_alarm_got(int client_sockfd,char* equipment_id);
#endif
