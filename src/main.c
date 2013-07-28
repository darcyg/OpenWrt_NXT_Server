/* 
 * File: main.c
 * 
 * Copyright (C) 2013  Pavel Prokhorov (pavelvpster@gmail.com)
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <libusb-1.0/libusb.h>

#include <sys/socket.h>
#include <netinet/in.h>


/**
 * Наименования команд NXT.
 * 
 * В качестве индекса использовать OPCODE.
 * 
 */
const char* commands[] = {
	
	"Start Program",
	"Stop Program",
	"Play Sound File",
	"Play Tone",
	"Set Output State",
	"Set Input Mode",
	"Get Output State",
	"Get Input Values",
	"Reset Input Scaled Value",
	"Message Write",
	"Reset Motor Position",
	"Get Battery Level",
	"Stop Sound Playback",
	"Keep Alive",
	"LS Get Status",
	"LS Write",
	"LS Read",
	"Get Current Program Name",
	"",
	"Message Read"
};

#define MIN_OPCODE 0x00
#define MAX_OPCODE 0x13


/**
 * Главная функция программы.
 * 
 */
int main(int argc, char** argv) {

	// Hello, world!
	
	printf("\n OpenWrt_NXT_Server. Copyright 2013 Pavel Prokhorov \n\n");

	
	// Разбираем командную строку

	if (argc < 2) {
		
		printf("Usage: OpenWrt_NXT_Server <port> [-nodebug] \n");
		
		return -1;
	}
	
	// Порт
	
	int port = atoi(argv[1]);
	
	// Печать отладочных сообщений
	
	bool debug = true;
	
	if (argc > 2) {
		
		if (strcmp(argv[2], "-nodebug") == 0) debug = false;
	}
	
	
	// Инициализируем библиотеку USB
	
	int r = libusb_init(NULL);
	
	if (r < 0) {
		
		if (debug) printf("ERROR! USB library initialization error.\n");
		
		return -1;
	}
	
	// Обнаруживаем контроллер NXT
	
	libusb_device_handle* dev = libusb_open_device_with_vid_pid(NULL, 0x0694, 0x002);
	
	if (dev == NULL) {
		
		if (debug) printf("ERROR! NXT not found.\n");
		
		// Завершаем работу с библиотекой USB

		libusb_exit(NULL);
		
		return -2;
	}
	
	if (debug) printf("NXT found.\n");
	
	// Выбираем интерфейс 0
	
	r = libusb_claim_interface(dev, 0);
	
	if (r < 0) {
		
		if (debug) printf("ERROR! NXT is busy.\n");
		
		// Закрываем устройство

		libusb_close(dev);

		// Завершаем работу с библиотекой USB

		libusb_exit(NULL);
		
		return -3;
	}
	
	if (debug) printf("NXT is ready.\n");
	
	// Посигналим
	
	unsigned char toneCommand[6];
	
	toneCommand[0] = 0x80; // Без ожидания ответа
	toneCommand[1] = 0x03;
	toneCommand[2] = 0x20; // 800 Гц
	toneCommand[3] = 0x03;
	toneCommand[4] = 0xE8; // 1000 мс
	toneCommand[5] = 0x03;
	
	int t = 0;
	
	r = libusb_bulk_transfer(dev, 0x01, (unsigned char*)&toneCommand, sizeof(toneCommand), &t, 1000);
	
	if (r < 0) {
		
		if (debug) printf("ERROR! USB transfer error.\n");
	}
	
	
	// Открываем UDP сокет
	
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	
	if (sock < 0) {
		
		if (debug) printf("ERROR! Unable to create UDP socket.\n");

		// Освобождаем интерфейс

		libusb_release_interface(dev, 0);

		// Закрываем устройство

		libusb_close(dev);

		// Завершаем работу с библиотекой USB

		libusb_exit(NULL);
		
		return -3;
	}
	
	// Готовим свой адрес
	
	struct sockaddr_in address;
	
	bzero(&address, sizeof(address));
	
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(port);
	
	// Биндим адрес
	
	r = bind(sock, (struct sockaddr*)&address, sizeof(address));
	
	if (r < 0) {
		
		if (debug) printf("ERROR! Unable to bind to port.\n");

		// Закрываем сокет

		close(sock);

		// Освобождаем интерфейс

		libusb_release_interface(dev, 0);

		// Закрываем устройство

		libusb_close(dev);

		// Завершаем работу с библиотекой USB

		libusb_exit(NULL);
		
		return -4;
	}
	
	// Ожидаем сообщений

	if (debug) printf("Listen for commands on port %d ...\n", port);
	
	while (1) {
		
		// Принимаем датаграмму
		
		unsigned char message[64];

		struct sockaddr_in from;
		
		int len = sizeof(from);
		
		int n = recvfrom(sock, &message, sizeof(message), 0, (struct sockaddr*)&from, &len);

		// Проверяем, может это команда на выход
		
		if (n == 1 && message[0] == 0xFF) {
			
			if (debug) printf("  Received command: Exit.\n");

			break;
		}
		
		// А может echo
		
		if (message[0] == 0xF0) {
			
			if (debug) printf("  Received command: Echo.\n");
			
			// Отправляем echo

			sendto(sock, &message, n, 0, (struct sockaddr*)&from, len);
			
			continue ;
		}
		
		// Проверяем OPCODE
		
		if (message[1] < MIN_OPCODE || message[1] > MAX_OPCODE) {
			
			if (debug) printf("  Received bad command!\n");
			
			continue ;
		}
		
		if (debug) printf("  Received command: %s.\n", commands[ message[1] ] );
		
		// Отправляем команду на устройство
		
		r = libusb_bulk_transfer(dev, 0x01, (unsigned char*)&message, n, &t, 1000);
		
		if (r < 0) {

			if (debug) printf("ERROR! USB transfer error.\n");
		}
		
		// Нужен ли клиенту ответ?
		
		if (message[0] != 0x00 && message[0] != 0x01) continue ;

		// Читаем ответ
		
		r = libusb_bulk_transfer(dev, 0x82, (unsigned char*)&message, 64, &t, 1000);
		
		if (r < 0) {

			if (debug) printf("ERROR! USB transfer error.\n");
		}
		
		// Отправляем его клиенту
		
		sendto(sock, &message, t, 0, (struct sockaddr*)&from, len);
	}
	
	// Закрываем сокет

	close(sock);
	

	// Освобождаем интерфейс
	
	libusb_release_interface(dev, 0);
	
	// Закрываем устройство
	
	libusb_close(dev);
	
	// Завершаем работу с библиотекой USB
	
	libusb_exit(NULL);

	
	printf("\n Done.\n");
	
	return 0;
}
