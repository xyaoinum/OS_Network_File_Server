#include "fs_client.h"
#include "fs_crypt.h"
#include "fs_server.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <fstream>
#include <vector>

using namespace std;

unordered_map<string, string> user_password_map;
unordered_map<unsigned int, string> session_user_map;
unordered_map<unsigned int, unsigned int> session_sequence_map;

unsigned int smallest_session = 0;
bool session_free = true;

vector<unsigned int> free_block;
fs_inode dir_inode;


class SocketExampleException {
  public:
    SocketExampleException(const string& err);
    string message;
};

SocketExampleException::SocketExampleException(const string& err)
: message(err) { }


class AddrinfoHolder {
	public:
		AddrinfoHolder(const string& host, const string& port, bool givenPort);
		~AddrinfoHolder();
		addrinfo* addr;
};

AddrinfoHolder::AddrinfoHolder(const string& host, const string& port, bool givenPort) {

	// Set up the address
	addrinfo hints;
	memset(&hints, 0, sizeof(addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	if(givenPort){
		hints.ai_socktype = 0;
	}
	hints.ai_flags = AI_PASSIVE;

	int rc = getaddrinfo(host.c_str(), port.c_str(), &hints, &addr);
	if (rc != 0) {
		throw SocketExampleException(gai_strerror(rc));
	}
}

AddrinfoHolder::~AddrinfoHolder() {
	freeaddrinfo(addr);
}

//parse the message into a vector of string
bool get_message_vector(string s, vector<string> &v){//s is ended with '\0'
	v.clear();
	if(s.size() < 2){
		return false;
	}
	if(s[s.size()-1] != '\0'){
		return false;
	}
	if(s.find("  ") != string::npos){
		return false;
	}                              
	if(s[0] == ' '){
		return false;
	}
	if(s[s.size()-2] == ' '){
		return false;
	}

	s = s.substr(0, s.size() - 1);

	bool has_space = true;
	while(1){
		if(has_space == false){
			v.push_back(s);
			break;
		}
		has_space = false;
		for(unsigned int i = 0; i < s.size(); i++){
			if(s[i] == ' '){
				v.push_back(s.substr(0, i));
				s = s.substr(i+1, s.size() - i - 1);
				has_space = true;
				break;
			}
		}
	}
	return true;
}


//decide if a string is an unsigned unmber
bool if_unsigned(string &s){
	if(s.size() == 0){
		return false;
	}
	if(s.size() > 10){
		return false;
	}
	if(s.size()>1 && s[0]=='0'){
		return false;
	}
	for(int i = 0; i<s.size(); i++){
		if(s[i]<'0' || s[i]>'9'){
			return false;
		}
	}
	if(s.size() == 10){
		return (s<"4294967296");
	}
	return true;
}

//convert a string to an unsigned number
unsigned int string_to_unsigned(string &s){
	unsigned int temp = 0;	
	for(unsigned int i=0;i<s.size();i++){
		temp = temp * 10 + (s[i] - '0');
	}
	return temp;
}

pthread_mutex_t error_check_lock;//lock of session and sequence check
pthread_mutex_t free_block_lock;//lock of free blocks
pthread_mutex_t the_mutex;//mutual exclusion of file rwlock
pthread_rwlock_t the_lock;//lock the entire request accessing the directory
pthread_mutex_t cout_lock;//lock the cout

unordered_map<string, pthread_rwlock_t*> filename_rwlock_map;//each file has its unique rwlock
unordered_map<string, unsigned int> filename_count;//the number of rwlocks each file holds


void filelock_start(string f, bool r){
	pthread_mutex_lock(&the_mutex);
	if(filename_rwlock_map.find(f) == filename_rwlock_map.end()){
		filename_count[f] = 0;
		filename_rwlock_map[f] = new pthread_rwlock_t;
    		pthread_rwlock_init(filename_rwlock_map[f], NULL);		
	}
	filename_count[f] ++;
	pthread_mutex_unlock(&the_mutex);
	if(r){
		pthread_rwlock_rdlock(filename_rwlock_map[f]);
	}else{
		pthread_rwlock_wrlock(filename_rwlock_map[f]);
	}
}

void filelock_end(string f){
	pthread_mutex_lock(&the_mutex);
	filename_count[f] --;
	pthread_rwlock_unlock(filename_rwlock_map[f]);
	if(filename_count[f] == 0){
		pthread_rwlock_destroy(filename_rwlock_map[f]);
		delete filename_rwlock_map[f];
		filename_rwlock_map.erase(f);
		filename_count.erase(f);
	}
	pthread_mutex_unlock(&the_mutex);
}

//check if a file name contains a '\0'
bool checkname(string &s){
	for(int i = 0; i<s.size(); i++){
		if(s[i]=='\0'){
			return false;
		}
	}
	return true;
}

//end of one request, close socket and delete the socket printor
void endRequest(int sock, void* sock_ptr){
	close(sock);
	if(sock_ptr != nullptr){
		delete (int*) sock_ptr;
	}
}

//run the request from the socket pointed by sock_ptr
void* run_request(void* sock_ptr){

	int sock = *((int*)(sock_ptr));

	char buf_header[1];
	string header;

	//get the header
	while(1){
		int res = recv(sock, &buf_header[0], sizeof(char), 0);
		if(buf_header[0] == '\0'){
			header.append(buf_header,1);
			break;
		}
		if(res == 1 && header.size() < 30){
			header.append(buf_header,1);
		}else{
			endRequest(sock,sock_ptr);
			return NULL;			
		}
	}

	vector<string> header_vector;

	if(!get_message_vector(header, header_vector)){
		endRequest(sock,sock_ptr);
		return NULL;
	}

	if(header_vector.size() != 2){
		endRequest(sock,sock_ptr);
		return NULL;
	}

	auto it = user_password_map.find(header_vector[0]);
	if(it == user_password_map.end()){
		endRequest(sock,sock_ptr);
		return NULL;
	}

	unsigned int encrypted_msg_size = string_to_unsigned(header_vector[1]);

	char *buf_encrypted = new char[encrypted_msg_size];


	unsigned int count = 0;
	while(count < encrypted_msg_size){
		int res= recv(sock, &buf_encrypted[count], encrypted_msg_size - count, 0);
		if(res <= 0){
			endRequest(sock,sock_ptr);
			return NULL;		
		}
		count += res;
	}

	string encrypted_msg = string(buf_encrypted, encrypted_msg_size);

	delete [] (char*)buf_encrypted;


	unsigned int decrypted_msg_size;

	string pswd = user_password_map[header_vector[0]];

	void * decrypted_msg_ptr = fs_decrypt((char*)(pswd.c_str()), (void*)encrypted_msg.c_str(), encrypted_msg_size, &decrypted_msg_size);

	if(decrypted_msg_ptr == nullptr){
		endRequest(sock,sock_ptr);
		return NULL;
	}

	string decrypted_msg = string((char*)decrypted_msg_ptr, decrypted_msg_size);
	delete [] (char*)decrypted_msg_ptr;

//cout << "###" << decrypted_msg << "###" << endl;

	if(decrypted_msg.size()!=decrypted_msg_size){
		endRequest(sock,sock_ptr);
		return NULL;		
	}

	string data="";
	unsigned int pos = decrypted_msg.find('\0');
	if(pos == decrypted_msg.size()){
		endRequest(sock,sock_ptr);
		return NULL;
	}

	if(pos + 1 < decrypted_msg.size()){
		data = decrypted_msg.substr(pos + 1, decrypted_msg.size() - pos - 1);
	}

	decrypted_msg = decrypted_msg.substr(0, pos + 1);



	vector<string> msg_vector;
	if(!get_message_vector(decrypted_msg, msg_vector)){
		endRequest(sock,sock_ptr);
		return NULL;
	}
	if(msg_vector.size() < 3){
		endRequest(sock,sock_ptr);
		return NULL;
	}

	unsigned int session_num = string_to_unsigned(msg_vector[1]);
	unsigned int seq_num = string_to_unsigned(msg_vector[2]);
	if(!if_unsigned(msg_vector[1]) || !if_unsigned(msg_vector[2])){
		endRequest(sock,sock_ptr);
		return NULL;
	}



	if(msg_vector[0] == "FS_SESSION"){

		if(msg_vector.size() != 3){
			endRequest(sock,sock_ptr);
			return NULL;
		}

		if(msg_vector[1] != "0"){
			endRequest(sock,sock_ptr);
			return NULL;
		}

		pthread_mutex_lock(&error_check_lock);
		if(!session_free){
			pthread_mutex_unlock(&error_check_lock);
			endRequest(sock,sock_ptr);
			return NULL;
		}
		session_user_map[smallest_session] = header_vector[0];
		session_sequence_map[smallest_session] = seq_num;
		smallest_session++;
		if(smallest_session == 0){
			session_free = false;
		}
		string response_cleartext_msg = to_string(smallest_session-1) + " " + msg_vector[2] + '\0';
		pthread_mutex_unlock(&error_check_lock);



		unsigned int cleartext_size = response_cleartext_msg.size();

		unsigned int encrypted_msg_size;
		void *encrypted_msg_ptr = fs_encrypt((char*)(user_password_map[header_vector[0]].c_str()), (void *)response_cleartext_msg.c_str(), cleartext_size, &encrypted_msg_size);
		string encrypted_msg = string((char*)encrypted_msg_ptr, encrypted_msg_size);

		string result_header = to_string((unsigned int)encrypted_msg_size) + '\0';
		send(sock, (char*)result_header.c_str(), result_header.size(), 0);
		send(sock, (char*)encrypted_msg.c_str(), encrypted_msg.size(), 0);
		endRequest(sock,sock_ptr);

		delete [](char*)encrypted_msg_ptr;

	}else if(msg_vector[0] == "FS_READ"){

		if(msg_vector.size() != 6){
			endRequest(sock,sock_ptr);
			return NULL;
		}



		pthread_mutex_lock(&error_check_lock);
		//check session number
		auto it = session_user_map.find(session_num);
		if (it == session_user_map.end()){
			pthread_mutex_unlock(&error_check_lock);
			endRequest(sock,sock_ptr);
			return NULL;
		}
		if(session_user_map[session_num] != header_vector[0]){
			pthread_mutex_unlock(&error_check_lock);
			endRequest(sock,sock_ptr);
			return NULL;
		}
		//check sequence number
		if(seq_num <= session_sequence_map[session_num]){
			pthread_mutex_unlock(&error_check_lock);
			endRequest(sock,sock_ptr);
			return NULL;
		}
		session_sequence_map[session_num] = seq_num;
		pthread_mutex_unlock(&error_check_lock);




		//check filename
		if(msg_vector[3].size() > FS_MAXFILENAME || !checkname(msg_vector[3])){
			endRequest(sock,sock_ptr);
			return NULL;
		}

		//check offset > 0
		if(!if_unsigned(msg_vector[4]) || string_to_unsigned(msg_vector[4]) >= FS_BLOCKSIZE*FS_MAXFILEBLOCKS){
			endRequest(sock,sock_ptr);
			return NULL;			
		}

		//check size > 0
		if(!if_unsigned(msg_vector[5]) || string_to_unsigned(msg_vector[5]) == 0 || string_to_unsigned(msg_vector[5]) > FS_BLOCKSIZE*FS_MAXFILEBLOCKS){
			endRequest(sock,sock_ptr);
			return NULL;			
		}

		if(string_to_unsigned(msg_vector[4]) + string_to_unsigned(msg_vector[5]) > FS_BLOCKSIZE*FS_MAXFILEBLOCKS){
			endRequest(sock,sock_ptr);
			return NULL;
		}

		fs_inode file_inode;
		bool find = false;
		string result;



		pthread_rwlock_rdlock(&the_lock);

		for(unsigned int i = 0; i < dir_inode.size/FS_BLOCKSIZE; i++){
			char buf[FS_BLOCKSIZE];


			disk_readblock(dir_inode.blocks[i], (void*)buf);
					
			for(unsigned int j = 0; j < FS_DIRENTRIES; j++){


				fs_direntry file_direntry = *((fs_direntry *)(&buf[16*j]));
				if(file_direntry.inode_block > 0 && strcmp(msg_vector[3].c_str(),file_direntry.name) == 0){
					find = true;

					filelock_start(msg_vector[3], true);

					disk_readblock(file_direntry.inode_block,(void*)(&file_inode));

					if(strcmp(header_vector[0].c_str(),file_inode.owner) != 0){

						filelock_end(msg_vector[3]);
						pthread_rwlock_unlock(&the_lock);
						endRequest(sock,sock_ptr);
						return NULL;
					}
					if(string_to_unsigned(msg_vector[4]) + string_to_unsigned(msg_vector[5]) > file_inode.size || string_to_unsigned(msg_vector[4]) >= file_inode.size || string_to_unsigned(msg_vector[5]) > file_inode.size){
						filelock_end(msg_vector[3]);
						pthread_rwlock_unlock(&the_lock);
						endRequest(sock,sock_ptr);
						return NULL;
					}

					unsigned int block_pos = string_to_unsigned(msg_vector[4])/FS_BLOCKSIZE;
					unsigned int offset = string_to_unsigned(msg_vector[4])%FS_BLOCKSIZE;
					unsigned int size = string_to_unsigned(msg_vector[5]);

					char databuf[FS_BLOCKSIZE];
					if(size <= FS_BLOCKSIZE - offset){
						disk_readblock(file_inode.blocks[block_pos],(void*)databuf);
						result.append(&databuf[offset], size);
					}else{
						disk_readblock(file_inode.blocks[block_pos],(void*)databuf);
						result.append(&databuf[offset], FS_BLOCKSIZE - offset);
						size -= FS_BLOCKSIZE - offset;
						while(size > FS_BLOCKSIZE) {
							block_pos++;
							disk_readblock(file_inode.blocks[block_pos],(void*)databuf);
							result.append(databuf, FS_BLOCKSIZE);
							size -= FS_BLOCKSIZE;
						}
						block_pos++;
						disk_readblock(file_inode.blocks[block_pos],(void*)databuf);
						result.append(databuf, size);
					}
					break;
				}
			}

			if(find){
				break;
			}

		}



		if(!find){
			pthread_rwlock_unlock(&the_lock);
			endRequest(sock,sock_ptr);
			return NULL;
		}


		filelock_end(msg_vector[3]);
		pthread_rwlock_unlock(&the_lock);


		string response_cleartext_msg = msg_vector[1] + " " + msg_vector[2] + '\0' + result;
		unsigned int cleartext_size = response_cleartext_msg.size();
		unsigned int encrypted_msg_size;
		void *encrypted_msg_ptr = fs_encrypt((char*)(user_password_map[header_vector[0]].c_str()), (void *)response_cleartext_msg.c_str(), cleartext_size, &encrypted_msg_size);
		string encrypted_msg = string((char*)encrypted_msg_ptr, encrypted_msg_size);


		string result_header = to_string((unsigned int)encrypted_msg_size) + '\0';
		send(sock, (char*)result_header.c_str(), result_header.size(), 0);
		send(sock, (char*)encrypted_msg.c_str(), encrypted_msg.size(), 0);
		endRequest(sock,sock_ptr);
		delete [](char*)encrypted_msg_ptr;

	}else if(msg_vector[0] == "FS_APPEND"){

		if(msg_vector.size() != 5){
			endRequest(sock,sock_ptr);
			return NULL;
		}

		pthread_mutex_lock(&error_check_lock);
		//check session number
		auto it = session_user_map.find(session_num);
		if (it == session_user_map.end()){
			pthread_mutex_unlock(&error_check_lock);
			endRequest(sock,sock_ptr);
			return NULL;			
		}
		if(session_user_map[session_num] != header_vector[0]){
			pthread_mutex_unlock(&error_check_lock);
			endRequest(sock,sock_ptr);
			return NULL;
		}
		//check sequence number
		if(seq_num <= session_sequence_map[session_num]){
			pthread_mutex_unlock(&error_check_lock);
			endRequest(sock,sock_ptr);
			return NULL;
		}
		session_sequence_map[session_num] = seq_num;
		pthread_mutex_unlock(&error_check_lock);

		//check filename
		if(msg_vector[3].size() > FS_MAXFILENAME || !checkname(msg_vector[3])){
			endRequest(sock,sock_ptr);
			return NULL;
		}


		//check size
		if(!if_unsigned(msg_vector[4]) || decrypted_msg_size - pos - 1 != string_to_unsigned(msg_vector[4]) || string_to_unsigned(msg_vector[4]) == 0){
			endRequest(sock,sock_ptr);
			return NULL;
		}

		pthread_mutex_lock(&free_block_lock);
		if(data.size() > free_block.size()*FS_BLOCKSIZE + FS_BLOCKSIZE - 1 || data.size() > FS_MAXFILEBLOCKS * FS_BLOCKSIZE){
			pthread_mutex_unlock(&free_block_lock);
			endRequest(sock,sock_ptr);
			return NULL;
		}
		pthread_mutex_unlock(&free_block_lock);

		bool find = false;
		fs_inode file_inode;

		pthread_rwlock_rdlock(&the_lock);

		vector<unsigned int> tmp_free_block;


		for(unsigned int i = 0; i < dir_inode.size/FS_BLOCKSIZE; i++){
			char buf[FS_BLOCKSIZE];

			disk_readblock(dir_inode.blocks[i], (void*)buf);

			for(unsigned int j = 0; j < FS_DIRENTRIES; j++){
				fs_direntry file_direntry = *((fs_direntry *)(&buf[16*j]));
				if(file_direntry.inode_block > 0 && strcmp(msg_vector[3].c_str(),file_direntry.name) == 0){

					find = true;

					filelock_start(msg_vector[3],false);


					disk_readblock(file_direntry.inode_block,(void*)(&file_inode));
					if(strcmp(header_vector[0].c_str(),file_inode.owner) != 0){
						filelock_end(msg_vector[3]);
						pthread_rwlock_unlock(&the_lock);
						endRequest(sock,sock_ptr);
						return NULL;
					}

					pthread_mutex_lock(&free_block_lock);
					if(data.size() > FS_MAXFILEBLOCKS * FS_BLOCKSIZE - file_inode.size || (data.size() > (free_block.size() + 1) * FS_BLOCKSIZE - (file_inode.size%FS_BLOCKSIZE) && file_inode.size%FS_BLOCKSIZE > 0) || (data.size() > free_block.size() * FS_BLOCKSIZE && file_inode.size%FS_BLOCKSIZE == 0)){
						pthread_mutex_unlock(&free_block_lock);
						filelock_end(msg_vector[3]);
						pthread_rwlock_unlock(&the_lock);
						endRequest(sock,sock_ptr);
						return NULL;
					}else{
						if(file_inode.size % FS_BLOCKSIZE == 0){

							for(int b = 0; b < (unsigned int)(ceil)((double)data.size()/FS_BLOCKSIZE);b++){
								tmp_free_block.push_back(free_block[free_block.size()-1]);
								free_block.pop_back();
							}
						}else if (data.size() + file_inode.size % FS_BLOCKSIZE <= FS_BLOCKSIZE){
							//
						}else{
							unsigned int remain = FS_BLOCKSIZE-(file_inode.size % FS_BLOCKSIZE);
							unsigned int appendsize = data.size() - remain;
							for(int b = 0; b < (unsigned int)(ceil)((double)appendsize/FS_BLOCKSIZE);b++){

								tmp_free_block.push_back(free_block[free_block.size()-1]);
								free_block.pop_back();
							}							
						}
					}
					pthread_mutex_unlock(&free_block_lock);

					unsigned int block_pos = file_inode.size/FS_BLOCKSIZE;
					unsigned int offset = file_inode.size%FS_BLOCKSIZE;
					char databuf[FS_BLOCKSIZE];

					if(file_inode.size % FS_BLOCKSIZE == 0){

						while(data.size()>FS_BLOCKSIZE){
							memcpy((void*)(databuf),data.c_str(),FS_BLOCKSIZE);
							file_inode.blocks[block_pos]=tmp_free_block[tmp_free_block.size()-1];
							tmp_free_block.pop_back();
							data=data.substr(FS_BLOCKSIZE,data.size()-FS_BLOCKSIZE);
							disk_writeblock(file_inode.blocks[block_pos],(void*)(databuf));
							block_pos++;
						}

						memcpy((void*)(databuf),data.c_str(),data.size());

						file_inode.blocks[block_pos]=tmp_free_block[tmp_free_block.size()-1];
						tmp_free_block.pop_back();

						disk_writeblock(file_inode.blocks[block_pos],(void*)(databuf));
						file_inode.size+=string_to_unsigned(msg_vector[4]);
						disk_writeblock(file_direntry.inode_block, (void*)(&file_inode));	
						break;
					}

					if (data.size()<=FS_BLOCKSIZE-offset){
						disk_readblock(file_inode.blocks[block_pos],(void*)(databuf));
						memcpy((void*)(databuf+offset),data.c_str(),data.size());
						disk_writeblock(file_inode.blocks[block_pos],(void*)(databuf));
					}else{

						disk_readblock(file_inode.blocks[block_pos],(void*)(databuf));
						memcpy((void*)(databuf+offset),data.c_str(),FS_BLOCKSIZE-offset);
						data=data.substr(FS_BLOCKSIZE-offset,data.size()-FS_BLOCKSIZE+offset);
						disk_writeblock(file_inode.blocks[block_pos],(void*)(databuf));	


						while(data.size()>FS_BLOCKSIZE){
							block_pos++;
							memcpy((void*)(databuf),data.c_str(),FS_BLOCKSIZE);
							file_inode.blocks[block_pos]=tmp_free_block[tmp_free_block.size()-1];
							tmp_free_block.pop_back();
							data=data.substr(FS_BLOCKSIZE,data.size()-FS_BLOCKSIZE);
							disk_writeblock(file_inode.blocks[block_pos],(void*)(databuf));

						}
						block_pos++;
						memcpy((void*)(databuf),data.c_str(),data.size());
						file_inode.blocks[block_pos]=tmp_free_block[tmp_free_block.size()-1];
						tmp_free_block.pop_back();
						disk_writeblock(file_inode.blocks[block_pos],(void*)(databuf));

					}
					file_inode.size+=string_to_unsigned(msg_vector[4]);
					disk_writeblock(file_direntry.inode_block, (void*)(&file_inode));	
					break;

				}
			}
			if (find){
				break;
			}


		}


		if (!find){
			pthread_rwlock_unlock(&the_lock);
			endRequest(sock,sock_ptr);
			return NULL;
		}

		filelock_end(msg_vector[3]);
		pthread_rwlock_unlock(&the_lock);

		string response_cleartext_msg = msg_vector[1] + " " + msg_vector[2] + '\0';
		unsigned int cleartext_size = response_cleartext_msg.size();
		unsigned int encrypted_msg_size;
		void *encrypted_msg_ptr = fs_encrypt((char*)(user_password_map[header_vector[0]].c_str()), (void *)response_cleartext_msg.c_str(), cleartext_size, &encrypted_msg_size);
		string encrypted_msg = string((char*)encrypted_msg_ptr, encrypted_msg_size);

		string result_header = to_string((unsigned int)encrypted_msg_size) + '\0';
		send(sock, (char*)result_header.c_str(), result_header.size(), 0);
		send(sock, (char*)encrypted_msg.c_str(), encrypted_msg.size(), 0);
		endRequest(sock,sock_ptr);
		delete [](char*)encrypted_msg_ptr;

	}else if(msg_vector[0] == "FS_CREATE"){

		if(msg_vector.size() != 4){
			endRequest(sock,sock_ptr);
			return NULL;
		}

		pthread_mutex_lock(&error_check_lock);
		//check session number
		auto it = session_user_map.find(session_num);
		if (it == session_user_map.end()){
			pthread_mutex_unlock(&error_check_lock);
			endRequest(sock,sock_ptr);
			return NULL;			
		}
		if(session_user_map[session_num] != header_vector[0]){
			pthread_mutex_unlock(&error_check_lock);
			endRequest(sock,sock_ptr);
			return NULL;
		}
		//check sequence number
		if(seq_num <= session_sequence_map[session_num]){
			pthread_mutex_unlock(&error_check_lock);
			endRequest(sock,sock_ptr);
			return NULL;
		}
		session_sequence_map[session_num] = seq_num;
		pthread_mutex_unlock(&error_check_lock);

		//check filename
		if(msg_vector[3].size() > FS_MAXFILENAME || !checkname(msg_vector[3])){
			endRequest(sock,sock_ptr);
			return NULL;
		}

		//no disk space for an file inode
		unsigned int free_b_1;
		unsigned int free_b_2;
		pthread_mutex_lock(&free_block_lock);
		if(free_block.empty()){
			pthread_mutex_unlock(&free_block_lock);
			endRequest(sock,sock_ptr);
			return NULL;
		}else{
			free_b_1 = free_block[free_block.size()-1];
			free_block.pop_back();
		}
		pthread_mutex_unlock(&free_block_lock);

		bool find_space = false;

		//check file name existence AND sufficient disk space
		bool exist_file = false;


		fs_direntry the_file_direntry;
		unsigned int position_i;
		unsigned int position_j;

		char the_buf[FS_BLOCKSIZE];

		pthread_rwlock_wrlock(&the_lock);

		for(unsigned int i = 0; i < dir_inode.size/FS_BLOCKSIZE; i++){

			char buf[FS_BLOCKSIZE];
			disk_readblock(dir_inode.blocks[i], (void*)buf);
			for(unsigned int j = 0; j < FS_DIRENTRIES; j++){
				fs_direntry file_direntry = *((fs_direntry *)(&buf[16*j]));

				if(file_direntry.inode_block > 0 && strcmp(msg_vector[3].c_str(),file_direntry.name) == 0){
					exist_file = true;
					break;
				}
				if(file_direntry.inode_block == 0 && find_space == false){
					find_space = true;
					the_file_direntry = file_direntry;
					position_i = i;
					position_j = j;
					memcpy(the_buf,buf,FS_BLOCKSIZE);
				}
			}
			if(exist_file){
				pthread_rwlock_unlock(&the_lock);
				endRequest(sock,sock_ptr);
				return NULL;
			}

		}


		if(find_space == true){
			memcpy(the_file_direntry.name, msg_vector[3].c_str(), FS_MAXFILENAME);
			the_file_direntry.name[msg_vector[3].size()] = '\0'; 
			the_file_direntry.inode_block = free_b_1;
			memcpy(&the_buf[16*position_j],&the_file_direntry,16);	
			fs_inode file_inode;
			file_inode.size = 0;
			memcpy(file_inode.owner, header_vector[0].c_str(), FS_MAXUSERNAME);
			file_inode.owner[header_vector[0].size()] = '\0';
			disk_writeblock(the_file_direntry.inode_block,(void*)(&file_inode));
			disk_writeblock(dir_inode.blocks[position_i],(void*)the_buf);
		}else{
			pthread_mutex_lock(&free_block_lock);
			if(free_block.size() > 0 && dir_inode.size/FS_BLOCKSIZE < FS_MAXFILEBLOCKS){
				free_b_2 = free_block[free_block.size()-1];
				free_block.pop_back();
			}else{
				free_block.push_back(free_b_1);
				pthread_mutex_unlock(&free_block_lock);
				endRequest(sock,sock_ptr);
				return NULL;
			}
			pthread_mutex_unlock(&free_block_lock);

			unsigned int entry_block = free_b_1;
			fs_direntry file_direntry;
			char buf[FS_BLOCKSIZE];
			file_direntry.inode_block = 0;
			for(int j = 0; j < FS_DIRENTRIES; j++){
				memcpy(&buf[16*j],&file_direntry,16);	
			}
			memcpy(file_direntry.name, msg_vector[3].c_str(), FS_MAXFILENAME);
			file_direntry.name[msg_vector[3].size()] = '\0';
			file_direntry.inode_block = free_b_2;
			memcpy(buf,&file_direntry,16);
			fs_inode file_inode;
			memcpy(file_inode.owner, header_vector[0].c_str(), FS_MAXUSERNAME);
			file_inode.owner[header_vector[0].size()] = '\0';
			file_inode.size = 0;
			disk_writeblock(file_direntry.inode_block,(void*)(&file_inode));
			disk_writeblock(entry_block,(void*)buf);
			dir_inode.size += FS_BLOCKSIZE;
			dir_inode.blocks[(dir_inode.size/FS_BLOCKSIZE) - 1] = entry_block;
			disk_writeblock(0,(void*)(&dir_inode));
		}


		pthread_rwlock_unlock(&the_lock);

		string response_cleartext_msg = msg_vector[1] + " " + msg_vector[2] + '\0';
		unsigned int cleartext_size = response_cleartext_msg.size();
		unsigned int encrypted_msg_size;
		void *encrypted_msg_ptr = fs_encrypt((char*)(user_password_map[header_vector[0]].c_str()), (void *)response_cleartext_msg.c_str(), cleartext_size, &encrypted_msg_size);
		string encrypted_msg = string((char*)encrypted_msg_ptr, encrypted_msg_size);
		string result_header = to_string((unsigned int)encrypted_msg_size) + '\0';
		send(sock, (char*)result_header.c_str(), result_header.size(), 0);
		send(sock, (char*)encrypted_msg.c_str(), encrypted_msg.size(), 0);
		endRequest(sock,sock_ptr);
		delete [](char*)encrypted_msg_ptr;

	}else if(msg_vector[0] == "FS_DELETE"){

		if(msg_vector.size() != 4){
			endRequest(sock,sock_ptr);
			return NULL;
		}

		pthread_mutex_lock(&error_check_lock);
		//check session number
		auto it = session_user_map.find(session_num);
		if (it == session_user_map.end()){
			pthread_mutex_unlock(&error_check_lock);
			endRequest(sock,sock_ptr);
			return NULL;			
		}
		if(session_user_map[session_num] != header_vector[0]){
			pthread_mutex_unlock(&error_check_lock);
			endRequest(sock,sock_ptr);
			return NULL;
		}
		//check sequence number
		if(seq_num <= session_sequence_map[session_num]){
			pthread_mutex_unlock(&error_check_lock);
			endRequest(sock,sock_ptr);
			return NULL;
		}
		session_sequence_map[session_num] = seq_num;
		pthread_mutex_unlock(&error_check_lock);
	

		//check filename
		if(msg_vector[3].size() > FS_MAXFILENAME || !checkname(msg_vector[3])){
			endRequest(sock,sock_ptr);
			return NULL;
		}

		bool finish = false;
		unsigned int file_inode_block;
		fs_inode file_inode;


		pthread_rwlock_wrlock(&the_lock);

		for(unsigned int i = 0; i < dir_inode.size/FS_BLOCKSIZE; i++){
			char buf[FS_BLOCKSIZE];
			disk_readblock(dir_inode.blocks[i], (void*)buf);

			for(unsigned int j = 0; j < FS_DIRENTRIES; j++){
				fs_direntry entry;
				memcpy(&entry,&buf[16*j], sizeof(fs_direntry));

				if(entry.inode_block > 0 && strcmp(msg_vector[3].c_str(), entry.name) == 0){

					file_inode_block = entry.inode_block;
					disk_readblock(file_inode_block,(void*)(&file_inode));

					if(strcmp(header_vector[0].c_str(),file_inode.owner) != 0){

						pthread_rwlock_unlock(&the_lock);
						endRequest(sock,sock_ptr);
						return NULL;
					}

					//free an entry block
					unsigned int use_count = 0;
					for(unsigned int k = 0; k < FS_DIRENTRIES; k++){
						unsigned int use;
						memcpy(&use,&buf[16*k+12], sizeof(unsigned int));
						if(use){
							use_count++;
						}
					}

					if(use_count == 1){
						pthread_mutex_lock(&free_block_lock);
						free_block.push_back(dir_inode.blocks[i]);
						pthread_mutex_unlock(&free_block_lock);
						for(unsigned int p = i; p < (unsigned int)ceil(dir_inode.size/FS_BLOCKSIZE) - 1; p++){
							dir_inode.blocks[p] = dir_inode.blocks[p+1];
						}

						dir_inode.size -= FS_BLOCKSIZE;
						disk_writeblock(0, (void*)(&dir_inode));
						finish = true;
						break;
					}else{
						unsigned int tmp = 0;
						memcpy(&buf[16*j+12],&tmp, sizeof(unsigned int));

						disk_writeblock(dir_inode.blocks[i],(void*)buf);
						finish = true;
						break;
					}
				}
			}

			if(finish){
				break;
			}

		}

		//file not found
		if(!finish){
			pthread_rwlock_unlock(&the_lock);
			endRequest(sock,sock_ptr);
			return NULL;
		}

		//free datablock
		pthread_mutex_lock(&free_block_lock);
		for(unsigned int i = 0; i < (unsigned int)ceil((double)file_inode.size/FS_BLOCKSIZE); i++){
			free_block.push_back(file_inode.blocks[i]);
		}
		free_block.push_back(file_inode_block);
		pthread_mutex_unlock(&free_block_lock);

		pthread_rwlock_unlock(&the_lock);

		string response_cleartext_msg = msg_vector[1] + " " + msg_vector[2] + '\0';
		unsigned int cleartext_size = response_cleartext_msg.size();
		unsigned int encrypted_msg_size;
		void *encrypted_msg_ptr = fs_encrypt((char*)(user_password_map[header_vector[0]].c_str()), (void *)response_cleartext_msg.c_str(), cleartext_size, &encrypted_msg_size);
		string encrypted_msg = string((char*)encrypted_msg_ptr, encrypted_msg_size);

		string result_header = to_string((unsigned int)encrypted_msg_size) + '\0';
		send(sock, (char*)result_header.c_str(), result_header.size(), 0);
		send(sock, (char*)encrypted_msg.c_str(), encrypted_msg.size(), 0);
		endRequest(sock,sock_ptr);
		delete [](char*)encrypted_msg_ptr;

	}else{
		endRequest(sock,sock_ptr); 
	}
	return NULL;
}






void server(const AddrinfoHolder& address) {
	// Allocate a socket
	int master_sock(socket(AF_INET, SOCK_STREAM, 0));

	// Enable reuse -- you want this.
	int yes = 1;
	if (setsockopt(master_sock, SOL_SOCKET,	SO_REUSEADDR, &yes, sizeof(int))) {
		throw SocketExampleException(strerror(errno));
	}

	// Bind to the interface.
	if (bind(master_sock, address.addr->ai_addr, address.addr->ai_addrlen)) {
		throw SocketExampleException(strerror(errno));
	}

	// Listen.
	if (listen(master_sock, 10)) {
		throw SocketExampleException(strerror(errno));
	}

	// Get socket port information
	sockaddr_in addr_name;
	socklen_t addr_len(sizeof(addr_name));
	if (getsockname(master_sock, (sockaddr*)&addr_name, &addr_len)) {
		throw SocketExampleException(strerror(errno));
	}

	pthread_mutex_lock(&cout_lock);
	cout << "\n@@@ port " << ntohs(addr_name.sin_port) << endl;
	pthread_mutex_unlock(&cout_lock);

	while (1) {
		int * sock_ptr = new int;
		*sock_ptr = accept(master_sock, nullptr, 0);
		pthread_t thread_id;
		pthread_create(&thread_id, NULL, &run_request, (void*)sock_ptr);
		pthread_detach(thread_id);
	}
}


int main(int argc, char** argv) {

	pthread_mutex_init(&error_check_lock,NULL);
	pthread_mutex_init(&free_block_lock,NULL);
	pthread_rwlock_init(&the_lock,NULL);
	pthread_mutex_init(&the_mutex,NULL);
	pthread_mutex_init(&cout_lock,NULL);


	string tmp_username;
	string tmp_password;

	while(cin >> tmp_username >> tmp_password){
		user_password_map[tmp_username] = tmp_password;
	}


	//initialize free block queue
	for(unsigned int i = 1; i < FS_DISKSIZE; i++){
		free_block.push_back(i);
	}

	disk_readblock(0, (void*)(&dir_inode));


	for(unsigned int i = 0; i < dir_inode.size/FS_BLOCKSIZE; i++){

		for(unsigned int p = 0; p < free_block.size(); p++){
			if(free_block[p] == dir_inode.blocks[i]){
				free_block.erase(free_block.begin()+p);
			}
		}

		char buf[FS_BLOCKSIZE];
		disk_readblock(dir_inode.blocks[i], (void*)buf);
		for(unsigned int j = 0; j < FS_DIRENTRIES; j++){
			fs_direntry entry = *((fs_direntry *)(&buf[16*j]));
			if(entry.inode_block > 0){
				fs_inode file_inode;
				disk_readblock(entry.inode_block, (void*)(&file_inode));
				for(unsigned int k = 0; k < (unsigned int)ceil((double)file_inode.size/FS_BLOCKSIZE); k++){
					for(unsigned int p = 0; p < free_block.size(); p++){
						if(free_block[p] == file_inode.blocks[k]){
							free_block.erase(free_block.begin()+p);
						}
					}
				}
			}
		}
	}


	try {
		string hostname("localhost");
		if (argc == 1) {
			server(AddrinfoHolder(hostname, "", true));
		}else if (argc == 2){
			string port(argv[1]);
			server(AddrinfoHolder(hostname, port, false));
		}else{
			cout << "Usage: " << argv[0] << " || " << argv[0] << " port" << endl;
		}
		return 0;
	} catch(const SocketExampleException& er) {
		cerr << er.message << endl;
		throw;
	}

	return 0;

}


