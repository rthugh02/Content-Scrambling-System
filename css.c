#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define BUFFER_SIZE 1024*1024

void print_help(void)
{
	printf("CSS: Content Scrambling System.\n");
	printf("-h help menu\n-t target path(required)\n-k key(required)\n-d destination. overwrites target if none given\n");
	exit(0);
}

unsigned char lfsr_byte_gen(uint32_t* lfsr, size_t lfsr_length, size_t tag_length, char tags[])
{
	unsigned char ret = 0;
	for(size_t i = 0; i < sizeof(char); i++)
	{
		uint32_t bit = 0;
		for(size_t j = 0; j < tag_length; j++)
		{
			bit = (bit ^ (*lfsr >> tags[j]));
		}
		bit = bit & 1;
		ret = ret | (bit << i);
		*lfsr = (*lfsr >> 1) | (bit << (lfsr_length - 1));
	}
	return ret;
}

unsigned char* build_key(char* given_key)
{
	unsigned char* key = calloc(5, sizeof(unsigned char));
	size_t len = strlen(given_key);
	if(key == NULL)
		return NULL;
	for(size_t i = 0; i < len; i++)
	{
		key[0] &= given_key[i];
		key[1] += given_key[i];
		key[2] *= given_key[i];
		key[3] ^= given_key[i];
		key[4] -= given_key[i];
	}	
	return key;
}

int main(int argc, char** argv) 
{	
	char** arg_walker = argv;
	char* target_path = NULL;	
	char* destination_path = NULL;
	char* key = NULL;	

	while(*arg_walker != NULL)
	{	
		if(strcmp("-h", *arg_walker) == 0)
			print_help();

		else if(strcmp("-t", *arg_walker) == 0)
			target_path = *(++arg_walker);	

		else if(strcmp("-k", *arg_walker) == 0)
			key = *(++arg_walker);	
		
		else if(strcmp("-d", *arg_walker) == 0)
			destination_path = *(++arg_walker);

		arg_walker++;
	}
	
	if(target_path == NULL || key == NULL)
	{
		printf("Error: Missing Arguments. -h for help\n");
		return 1;
	}		
	if(strlen(key) < 5)
	{
		printf("Error: Invalid Key length (5 minimum)\n");
		return 1;
	}

	unsigned char* constructed_key = build_key(key);
	if(constructed_key == NULL)
	{
		printf("Error: not enough memory");
		return 1;	
	}
	uint32_t lfsr_17 = 0;
	uint32_t lfsr_25 = 0;

	lfsr_17 =((constructed_key[0] << 9) + constructed_key[1]) | (1 << 8);	
	lfsr_25 = ((constructed_key[2] << 17) + (constructed_key[3] << 9) + constructed_key[4]) | (1 << 8);	
	free(constructed_key);

	FILE* plain_text = NULL; 
	FILE* cipher_text = NULL;
	
	if(destination_path == NULL)
	{
		plain_text	= fopen(target_path, "r+b");
	}
	else 
	{
		plain_text = fopen(target_path, "rb");
		cipher_text = fopen(destination_path, "wb");
		if(cipher_text == NULL)
		{
			printf("Error: Could not create destination file\n");
			return 1;
		}
	}

	if(plain_text == NULL)
	{
		printf("Error opening file\n");
		return 1;
	}

	long total_bytes = 0;

	if(fseek(plain_text, 0, SEEK_END) != 0)
	{
		printf("Error reading file\n");
		return 1;
	}
	total_bytes = ftell(plain_text);
	rewind(plain_text);

	char* buffer = malloc(BUFFER_SIZE);
	if(buffer == NULL)
	{
		printf("Error: not enough memory\n");
		return 1;
	}
	
	long total_bytes_read = 0;
	long total_bytes_written = 0;
	while(1)
	{
		size_t bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, plain_text);
		total_bytes_read += bytes_read;	
		for(size_t i = 0; i < bytes_read; i++)
		{		
			unsigned char lfsr_17_byte = lfsr_byte_gen(&lfsr_17, 17, 2, (char[]) {0, 14});  
			unsigned char lfsr_25_byte = lfsr_byte_gen(&lfsr_25, 25, 4, (char[]) {0, 3, 4, 14});
			buffer[i] = buffer[i] ^ (lfsr_17_byte + lfsr_25_byte); 	
		}
		if(cipher_text != NULL)
		{
			total_bytes_written += fwrite(buffer, sizeof(char), bytes_read, cipher_text);
		}
		else
		{
			fseek(plain_text, -bytes_read, SEEK_CUR);
			total_bytes_written += fwrite(buffer, sizeof(char), bytes_read, plain_text);
		}
			
		if(bytes_read < BUFFER_SIZE)
			break;
			
	}

	free(buffer);
	fclose(plain_text);
	if(cipher_text != NULL)
		fclose(cipher_text);
	
	if((total_bytes_read != total_bytes) || (total_bytes_written != total_bytes))
	{
		printf("Error: Failed to completely encrypt file\n");
		return 1;
	}
	return 0;
}
