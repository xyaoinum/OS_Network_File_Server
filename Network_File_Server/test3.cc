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
#include <map>
#include <fstream>
#include <vector>

using namespace std;

main(int argc, char *argv[])

{

    char *server;
    int server_port;
    unsigned int session, seq=0;
    char buf[10000];

    if (argc != 3) {
        cout << "error: usage: " << argv[0] << " <server> <serverPort>\n";
        exit(1);
    }
    server = argv[1];
    server_port = atoi(argv[2]);

    fs_clientinit(server, server_port);


    fs_session("user1", "password1", &session, seq++);

    fs_create("user1", "password1", session, seq++, "tmp");

	string s;
	for(int i = 0; i < 1025; i++){
		s += to_string(((i%7)*(i%11))%10);
	}

	cout << s << endl;


    fs_append("user1", "password1", session, seq++, "tmp", s.c_str(), s.size());


    fs_read("user1", "password1", session, seq++, "tmp", 1, buf, 511);





    fs_read("user1", "password1", session, seq++, "tmp", 1, buf, 510);
    fs_read("user1", "password1", session, seq++, "tmp", 1, buf, 512);
    fs_read("user1", "password1", session, seq++, "tmp", 1, buf, 514);
    fs_read("user1", "password1", session, seq++, "tmp", 1, buf, 1024);

string t = string((char*)buf, 1024);

cout << t << endl;

    fs_create("user1", "password1", session, seq++, "tmp");
    fs_delete("user1", "password1", session, seq++, "tmp");
    fs_create("user1", "password1", session, seq++, "tmp");


    fs_session("user1", "password1", &session, seq++);
    fs_create("user1", "password1", session, seq++, "tmp");
    fs_append("user1", "password1", session, seq++, "tmp", "ab34c", 5);
    fs_append("user1", "password1", session, seq++, "tmp34", "ab34c", 5);
    fs_append("user1", "password1", session, seq++, "tmp56", "ab34c", 5);
    fs_read("user1", "password1", session, seq++, "tmp", 1, buf, 2);
    fs_delete("user1", "password1", session, seq++, "tmp");


}
