free_application_context: free_application_context.c args.h
	gcc -Wall -c free_application_context.o free_application_context.c  

parse_args: parse_args.c args.h
	gcc -Wall -c parse_args.o parse_args.c

args:
	make free_application_context
	make parse_args

log_file_handler_vars:
	gcc -Wall -c log_file_handler_vars.o log_file_handler_vars.c -pthread

log_file_writer:
	gcc -Wall -c log_file_writer.o log_file_writer.c -pthread

log_message_producer:
	gcc -Wall -c log_message_producer.o log_message_producer.c -pthread

log_file_handler:
	make log_file_handler_vars
	make log_file_writer
	make log_message_producer

stats_file_handler_vars:
	gcc -Wall -c stats_file_handler_vars.o stats_file_handler_vars.c -pthread

produce_stats_message:
	gcc -Wall -c produce_stats_message.o produce_stats_message.c -pthread

show_stats:
	gcc -Wall -c show_stats.o show_stats.c -pthread

stats_file_handler:
	make stats_file_handler_vars
	make produce_stats_message
	make show_stats

clients_vars:
	gcc -Wall -c clients_vars.o clients_vars.c -pthread

insert_client:
	gcc -Wall -c insert_client.o insert_client.c -pthread

remove_client:
	gcc -Wall -c remove_client.o remove_client.c -pthread

close_clients:
	gcc -Wall -c close_clients.o close_clients.c -pthread

clients:
	make clients_vars
	make insert_client
	make remove_client
	make close_clients

object:
	make args
	make log_file_handler
	make stats_file_handler
	make clients

clean-object:
	- rm *.o

clean-out:
	- rm *.out

clean: 
	make clean-object
	make clean-out

build:
	make object
	gcc -Wall -g -o main.out main.c *.o -pthread
	make clean-object
