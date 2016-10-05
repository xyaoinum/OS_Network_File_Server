#include <iostream>
#include <cstdlib>
#include "fs_client.h"

using namespace std;

main(int argc, char *argv[])

{
    char *server;
    int server_port;
    unsigned int session, seq=0;          
    char buf[10];

    if (argc != 3) {
        cout << "error: usage: " << argv[0] << " <server> <serverPort>\n";
        exit(1);
    }
    server = argv[1];
    server_port = atoi(argv[2]);

    fs_clientinit(server, server_port);
    fs_session("user1", "password1", &session, seq++);


	string file512;
	for(int i = 0; i < 512; i++){
		file512.append(to_string(i%10));
	}

	
	string file124;
	for(int i = 0; i < 124; i++){
		file124+=file512;
	}

	for(int i = 0; i < 512/16+1; i++){
		string s = "tmp";
		s += to_string(i);
    		fs_create("user1", "password1", session, seq++, s.c_str());
	}

	for(int i = 0; i < 512/16+1; i++){
		string s = "tmp";
		s += to_string(i);
		fs_append("user1", "password1", session, seq++, s.c_str(), "", 0);
	}

    	fs_create("user1", "password1", session, seq++, "tmp");
	fs_append("user1", "password1", session, seq++, "tmp", "a", 1);
	fs_append("user1", "password1", session, seq++, "tmp", "a", 1);
    	fs_create("user1", "password1", session, seq++, "tmppp");
	fs_delete("user1", "password1", session, seq++, "tmp");



	for(int i = 0; i < 512/16+1; i++){
		string s = "tmp";
		s += to_string(i);
    		fs_create("user1", "password1", session, seq++, s.c_str());
	}

	for(int i = 0; i < 512/16+1; i++){
		string s = "tmp";
		s += to_string(i);
	    fs_delete("user1", "password1", session, seq++, s.c_str());
		s = "tmp";
		s += to_string(i+1);
		fs_append("user1", "password1", session, seq++, s.c_str(), "a", 1);
		fs_append("user1", "password1", session, seq++, s.c_str(), file512.c_str(), 512);
	}

    fs_append("user1", "password1", session, seq++, "tmp", "abc", 3);
    fs_append("user1", "password1", session, seq, "tmp", "abc", 3);
    fs_append("user1", "password1", session, seq, "tmp", "abc", 3);
    fs_append("user2", "password2", session, seq++, "tmp", "abcde", 4);
    fs_append("user3", "password3", session, seq++, "tmp", "abcfg", 4);
    fs_read("user1", "password1", session, seq++, "tmp", 1, buf, 2);
    fs_read("user2", "password2", session, seq++, "tmp", 1, buf, 2);
    fs_read("user2", "password1", session, seq++, "tmp", 1, buf, 2);
    fs_read("user3", "password1", session, seq++, "tmp", 1, buf, 2);
    fs_delete("user1", "password1", session, seq++, "tmp");

}
