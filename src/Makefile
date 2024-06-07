build:
	make object
	gcc -Wall -o webserver.out main.c *.o -pthread
	make clean-object

free_application_context: free_application_context.c 
	gcc -Wall -c free_application_context.o free_application_context.c  

parse_args: parse_args.c
	gcc -Wall -c parse_args.o parse_args.c

args:
	make free_application_context
	make parse_args

log_file_handler_vars: log_file_handler_vars.c 
	gcc -Wall -c log_file_handler_vars.o log_file_handler_vars.c -pthread

log_file_writer: log_file_writer.c
	gcc -Wall -c log_file_writer.o log_file_writer.c -pthread

log_message_producer: log_message_producer.c
	gcc -Wall -c log_message_producer.o log_message_producer.c -pthread

free_log_buffer: free_log_buffer.c
	gcc -Wall -c free_log_buffer.o free_log_buffer.c -pthread

terminate_log_file_writer: terminate_log_file_writer.c
	gcc -Wall -c terminate_log_file_writer.o terminate_log_file_writer.c -pthread

log_file_handler:
	make log_file_handler_vars
	make log_file_writer
	make log_message_producer
	make free_log_buffer
	make terminate_log_file_writer

stats_file_handler_vars: stats_file_handler_vars.c
	gcc -Wall -c stats_file_handler_vars.o stats_file_handler_vars.c -pthread

produce_stats_message: produce_stats_message.c
	gcc -Wall -c produce_stats_message.o produce_stats_message.c -pthread

show_stats: show_stats.c
	gcc -Wall -c show_stats.o show_stats.c -pthread

stats_file_handler:
	make stats_file_handler_vars
	make produce_stats_message
	make show_stats

clients_vars: clients_vars.c
	gcc -Wall -c clients_vars.o clients_vars.c -pthread

insert_client: insert_client.c
	gcc -Wall -c insert_client.o insert_client.c -pthread

remove_client: remove_client.c
	gcc -Wall -c remove_client.o remove_client.c -pthread

close_clients: close_clients.c
	gcc -Wall -c close_clients.o close_clients.c -pthread

clients:
	make clients_vars
	make insert_client
	make remove_client
	make close_clients

response_file_handler_vars: response_file_handler_vars.c
	gcc -Wall -c response_file_handler_vars.o response_file_handler_vars.c

get_file_content: get_file_content.c
	gcc -Wall -c get_file_content.o get_file_content.c 

get_file_path: get_file_path.c
	gcc -Wall -c get_file_path.o get_file_path.c

get_file_type: get_file_type.c
	gcc -Wall -c get_file_type.o get_file_type.c

response_file_handler:
	make response_file_handler_vars
	make get_file_content
	make get_file_path
	make get_file_type

server_vars: server_vars.c
	gcc -Wall -c server_vars.o server_vars.c

create_tcp_socket: create_tcp_socket.c
	gcc -Wall -c create_tcp_socket.o create_tcp_socket.c

client_thread: client_thread.c
	gcc -Wall -c client_thread.o client_thread.c -pthread

start_server: start_server.c
	gcc -Wall -c start_server.o start_server.c -pthread

server:
	make server_vars
	make create_tcp_socket
	make client_thread
	make start_server

terminate: terminate.c
	gcc -Wall -c terminate.o terminate.c

object:
	make args
	make log_file_handler
	make stats_file_handler
	make clients
	make response_file_handler
	make server
	make terminate

clean-object:
	- rm *.o

clean-out:
	- rm *.out

clean: 
	make clean-object
	make clean-out
